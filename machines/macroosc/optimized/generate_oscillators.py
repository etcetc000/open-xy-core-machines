"""Reproduce narrow oscillator overlays without editing vendor files."""
import argparse
import hashlib
import re
from pathlib import Path
import sys
sys.dont_write_bytecode = True
from generate_wave_tables import CHORD_RENDERERS as WAVE_RENDERERS, transform_digital
from generate_filter import transform_digital as transform_drum_filters
import triple_mix
import triple_square_render
import harmonic_render
import dispatch_render
import signed_sine_render
import wave_line_render
import analog_state
import analog_dispatch_render
import flute_render
import swarm_snare_render
import swarm_group_render
import cymbal_render
import kick_render
import struck_drum_render
import flute_bounds_render
import flute_jet_render
import morph_render
import sync_dispatch
import analog_division
import fof_render
import digital_filter_render
import byte_mix_render

HERE = Path(__file__).resolve().parent
SOURCES = {
    'macro': '715ee728aaae8dfaa3387c50bd6cc1f1327374ff0949c8104e48cf7c11d0d241',
    'digital': '6ca164788680d939ade7d1fe91e44a1d1fac9b00cee6a2b34a3e4a25c8bf9ada',
    'analog': 'dc7fbfe3b34314a2fdd73e81d5e4d65dd0015adf8cd4f45f678dd6ce0fda91c6',
}

def restrict_buffers(source):
    def select(match):
        signature = match.group(0)
        cls, method = match.group(1), match.group(2)
        # The adapter's audio block is separate from the oscillator state.
        # Keep this measured selection: annotating every digital renderer
        # makes VOSIM, Kick and Granular Cloud slower on the target compiler.
        if cls == 'DigitalOscillator' and method not in (
                'RenderFm', 'RenderFeedbackFm', 'RenderChaoticFeedbackFm',
                'RenderSawSwarm', 'RenderDigitalFilter', 'RenderDigitalFilterBandHigh', 'RenderVowel'):
            return signature
        if cls == 'MacroOscillator' and method == 'RenderSineTriangle':
            return signature
        if cls == 'AnalogOscillator' and ('Fold' in method):
            return signature
        return signature.replace('int16_t* buffer,', 'int16_t* __restrict__ buffer,')
    return re.sub(r'void (\w+)::(Render\w*)\([^{}]*\{', select, source)


def packed_lookups(source):
    source = '#include "optimized/exact_mix.hpp"\n'+source
    source = source.replace('Interpolate824(wav_sine,', 'macroosc::paired824(macroosc::sine_pairs,')
    source = source.replace('Interpolate88(ws_sine_fold,', 'macroosc::paired88(macroosc::sine_fold_pairs,')
    source = source.replace('Interpolate88(ws_tri_fold,', 'macroosc::paired88(macroosc::tri_fold_pairs,')
    source = source.replace('Interpolate88(ws_violent_overdrive,', 'macroosc::paired88(macroosc::violent_pairs,')
    # The sample-loop shapers benefit; Comb's once-per-call resonance lookup
    # is faster in its original form on the target compiler.
    selected = ('RenderTripleRingMod', 'RenderSawSwarm', 'RenderVowel',
                'RenderFilteredNoise', 'RenderTwinPeaksNoise')
    def packed_overdrive(match):
        return match[0].replace('Interpolate88(ws_moderate_overdrive,',
            'macroosc::paired88(macroosc::moderate_pairs,') if match[1] in selected else match[0]
    source = re.sub(r'void DigitalOscillator::(Render\w+)\([^{}]*\{.*?\n\}',
                    packed_overdrive, source, flags=re.S)

    for name in ('svf_cutoff','svf_damp','svf_scale','resonator_coefficient','resonator_scale','bell'):
        source = source.replace('Interpolate824(lut_'+name+',', 'macroosc::paired_u824(macroosc::'+name+'_pairs,')
    def fractional_coefficient(match):
        return match[0].replace('macroosc::paired_u824(', 'macroosc::paired_fractional_u824(') if match[1] in ('RenderStruckDrum','RenderParticleNoise') else match[0]
    source = re.sub(r'void DigitalOscillator::(Render\w+)\([^{}]*\{.*?\n\}',
                    fractional_coefficient, source, flags=re.S)
    return '#include "optimized/packed_tables.hpp"\n'+source


def generated(kind):
    path = HERE.parent / ('vendor/braids/' + kind + '_oscillator.cc')
    raw = path.read_bytes().replace(b'\r\n', b'\n')
    if hashlib.sha256(raw).hexdigest() != SOURCES[kind]:
        raise ValueError('Vendor source changed; review the overlay before updating its provenance')
    source = raw.decode().replace('\r\n', '\n')
    if kind == 'macro':
        # This machine never accepts external oscillator sync. Internal hard-sync
        # master/slave buffers retain their original fractional timing.
        source, count = re.subn(r'(analog_oscillator_\[[^\]]+\]\.Render\()sync(, [^\n]+, NULL, size\);)',
                                r'\1nullptr\2', source)
        if count != 13:
            raise ValueError('Unexpected number of no-sync analog calls: ' + str(count))
        source = source.replace('.Render(nullptr,', '.RenderNoSync(nullptr,')
        # Undo simple no-sync specialization for the folding pair. Its caller
        # is replaced below by the separately measured fused renderer.
        start = source.index('void MacroOscillator::RenderSineTriangle(')
        end = source.index('void MacroOscillator::RenderBuzz(', start)
        source = source[:start] + source[start:end].replace('.RenderNoSync(nullptr,', '.Render(sync,') + source[end:]
        source = source.replace('analog_oscillator_[0].Render(sync, buffer, sync_buffer_, size);',
                                'analog_oscillator_[0].RenderSyncMaster(nullptr, buffer, sync_buffer_, size);')
        source = source.replace('analog_oscillator_[1].Render(sync_buffer_, temp_buffer_, NULL, size);',
                                'analog_oscillator_[1].RenderSyncSlave(sync_buffer_, temp_buffer_, nullptr, size);')
        start = source.index('void MacroOscillator::RenderSineTriangle(')
        end = source.index('void MacroOscillator::RenderBuzz(', start)
        renderer = source[start:end]
        cut = renderer.index('  analog_oscillator_[0].Render(sync,')
        renderer = renderer[:cut] + '''  analog_oscillator_[0].RenderFoldPair(analog_oscillator_[1], buffer, size,
      previous_parameter_[1], parameter_[1] - previous_parameter_[1]);
  previous_parameter_[1] = parameter_[1];
}

'''
        source = source[:start] + renderer + source[end:]
        source = triple_mix.transform_macro(source)
        source = sync_dispatch.transform_macro(source)
        source = restrict_buffers(source)
        source = dispatch_render.transform(source, kind)
        source = morph_render.transform(packed_lookups(source))
        source = analog_dispatch_render.transform_macro(source)
        return ('// Generated by optimized/generate_oscillators.py; edit the generator.\n'
                '// Null analog sync selects the exact no-external-sync specialization.\n' + source).encode()
    original_phase = None
    if kind == 'digital':
        start = source.index('uint32_t DigitalOscillator::ComputePhaseIncrement(')
        end = source.index('uint32_t DigitalOscillator::ComputeDelay(', start)
        original_phase = source[start:end].replace(
            'uint32_t DigitalOscillator::ComputePhaseIncrement(',
            'static inline __attribute__((always_inline)) uint32_t OriginalPhaseIncrementForChords(')
    old = '  delay_ = ComputeDelay(pitch_);'
    new = '''  // Only these three renderers read delay_. Use the actual dispatch entry:
  // the vendor shape enum and function table differ in this region.
  // Recompute before every use, including the first render after a switch.
  if (fn == &DigitalOscillator::RenderBowed ||
      fn == &DigitalOscillator::RenderBlown ||
      fn == &DigitalOscillator::RenderFluted) {
    delay_ = ComputeDelay(pitch_);
  }'''
    if kind == 'digital':
        if source.count(old) != 1:
            raise ValueError('Expected exactly one delay-setup site')
        source = source.replace(old, new)
    old_loop = '''  size_t num_shifts = 0;
  while (ref_pitch < 0) {
    ref_pitch += kOctave;
    ++num_shifts;
  }'''
    new_loop = '''  const size_t num_shifts = macroosc::octave_shifts(ref_pitch);
  ref_pitch += static_cast<int32_t>(num_shifts) * kOctave;'''
    if source.count(old_loop) != (2 if kind == 'digital' else 1):
        raise ValueError('Unexpected number of pitch-folding loops')
    source = source.replace(old_loop, new_loop)
    if original_phase is not None:
        # With the expanded exact pitch table, this no longer introduces the
        # register-pressure regression observed in the earlier chord renderer.
        original_phase = original_phase.replace(old_loop, new_loop)
    methods = ['ComputePhaseIncrement'] + (['ComputeDelay'] if kind == 'digital' else [])
    for method in methods:
        signature = 'uint32_t ' + kind.title() + 'Oscillator::' + method + '('
        if source.count(signature) != 1:
            raise ValueError('Expected exactly one pitch helper definition')
        # Preserve inlining after replacing the loops: otherwise the compiler
        # introduces calls and extra spills into the Wave Chords renderer.
        source = source.replace(signature, 'inline __attribute__((always_inline)) ' + signature)
    if kind == 'digital':
        signature = 'inline __attribute__((always_inline)) uint32_t DigitalOscillator::ComputePhaseIncrement('
        source = source.replace(signature, original_phase + signature, 1)
        start = source.index('void DigitalOscillator::RenderWaveParaphonic(')
        end = source.index('void DigitalOscillator::RenderFilteredNoise(', start)
        renderer = source[start:end]
        call = 'ComputePhaseIncrement(pitch_ + detune)'
        if renderer.count(call) != 1:
            raise ValueError('Expected exactly one chord pitch-setup site')
        renderer = renderer.replace(call, 'OriginalPhaseIncrementForChords(pitch_ + detune)')
        source = source[:start] + renderer + source[end:]
        start = source.index('void DigitalOscillator::RenderWaveMap(')
        end = source.index('static const uint8_t wave_line[]', start)
        source = source[:start] + source[start:end].replace('Crossfade(', 'MacroCrossfadeByte(') + source[end:]
        start = source.index('void DigitalOscillator::RenderVowelFof(')
        end = source.index('void DigitalOscillator::RenderFm(', start)
        renderer = source[start:end]
        setup_start = renderer.index('  int16_t amplitudes[kNumFormants];')
        setup_end = renderer.index('    if (init_) {', setup_start)
        renderer = renderer[:setup_start] + '''  int32_t svf_lp[kNumFormants];
  int32_t svf_bp[kNumFormants];
  int16_t svf_f[kNumFormants];
  const uint32_t key = 0x80008000u |
      (static_cast<uint32_t>(parameter_[0]) << 16) | parameter_[1];
  if (state_.fof.control_key != key) {
    state_.fof.control_key = key;
    for (size_t i = 0; i < kNumFormants; ++i) {
      int32_t frequency = InterpolateFormantParameter(
          formant_f_data, parameter_[1], parameter_[0], i) + (12 << 7);
      state_.fof.coefficients[i] = Interpolate824(lut_svf_cutoff, frequency << 17);
    }
    // Preserve the original renderer's use of formant zero's amplitude for
    // all five filters; the other four amplitude calculations were unused.
    state_.fof.amplitude = InterpolateFormantParameter(
        formant_a_data, parameter_[1], parameter_[0], 0);
  }
  const int16_t amplitude = state_.fof.amplitude;
  for (size_t i = 0; i < kNumFormants; ++i) {
    svf_f[i] = state_.fof.coefficients[i];
''' + renderer[setup_end:]
        renderer = renderer.replace('amplitudes[0]', 'amplitude')
        source = source[:start] + renderer + source[end:]
        # The machine's digital renderers always receive kNoSync. Internal
        # hard-sync is implemented entirely by the analog oscillator pair.
        # Keep the original code in renderers where constant propagation
        # worsened native scheduling/register allocation.
        if source.count('*sync++') != 17:
            raise ValueError('Unexpected digital sync sites')
        keep_sync = ('RenderVosim', 'RenderHarmonics', 'RenderWavetables', 'RenderWaveMap')
        def no_sync(match):
            return match.group(0) if match.group(1) in keep_sync else match.group(0).replace('*sync++', '0')
        source = re.sub(r'void DigitalOscillator::(Render\w+)\([^{}]*\{.*?\n\}', no_sync, source, flags=re.S)
        start = source.index('void DigitalOscillator::RenderHarmonics(')
        end = source.index('void DigitalOscillator::RenderStruckDrum(', start)
        renderer = source[start:end]
        old = '''    out = 0;
    for (size_t i = 0; i < kNumAdditiveHarmonics; ++i) {
      out += Interpolate824(wav_sine, phase * (i + 1)) * amplitude[i] >> 15;'''
        new = '''    out = 0;
    uint32_t harmonic_phase = phase;
    for (size_t i = 0; i < kNumAdditiveHarmonics; ++i) {
      // Spectrum normalization bounds amplitudes to 0..32767; the one-pole
      // preserves that range. The fractional multiply is therefore exact.
      out += opxy::q31::multiply(static_cast<int32_t>(Interpolate824(wav_sine, harmonic_phase)) * 65536, amplitude[i]);
      harmonic_phase += phase;'''
        if renderer.count(old) != 1:
            raise ValueError('Harmonics inner loop changed')
        source = source[:start] + renderer.replace(old, new) + source[end:]
        source = source.replace('(target_increment - modulator_phase_increment) / size', 'macroosc::chunk_math::phase_delta(target_increment - modulator_phase_increment, size)')
        source = source.replace('(modulator_phase_increment - target_increment) / size', 'macroosc::chunk_math::phase_delta(modulator_phase_increment - target_increment, size)')
        source = source.replace('65536 / size', 'macroosc::chunk_math::phase_delta(65536, size)')
        source = source.replace('32768 / size', 'macroosc::chunk_math::phase_delta(32768, size)')
        source = transform_digital(source)
    if kind == 'analog':
        fast_functions = []
        def specialize(match):
            function = match.group(0)
            opening = function.index('{')
            body = function[opening+1:-1]
            fast = body.replace('*sync_in++', '0').replace('*sync_in', '0')
            fast = fast.replace('sync_in++;', '').replace('if (sync_out)', 'if (false)')
            signature = function[:opening+1].replace('(', 'NoSync(', 1)
            fast_functions.append(signature + fast + '}')
            if '::RenderSquare(' in function or '::RenderSaw(' in function:
                master = body.replace('*sync_in++', '0').replace('*sync_in', '0')
                master = master.replace('sync_in++;', '').replace('if (sync_out)', 'if (true)')
                fast_functions.append(function[:opening+1].replace('(', 'Master(', 1) + master + '}')
                slave = body.replace('if (sync_out)', 'if (false)')
                fast_functions.append(function[:opening+1].replace('(', 'Slave(', 1) + slave + '}')
            return function
        source, count = re.subn(r'void AnalogOscillator::Render\w+\([^{}]*\{.*?\n\}', specialize, source, flags=re.S)
        if count != 9:
            raise ValueError('Unexpected analog renderer count')
        selector = '  RenderFn fn = fn_table_[shape_];'
        if source.count(selector) != 1:
            raise ValueError('Unexpected analog dispatcher')
        start = source.index('void AnalogOscillator::Render(')
        end = source.index('\n}', start) + 2
        dispatcher = source[start:end].replace('AnalogOscillator::Render(', 'AnalogOscillator::RenderNoSync(')
        dispatcher = dispatcher.replace(selector, '  RenderFn fn = no_sync_table_[shape_];')
        dispatchers = [dispatcher]
        for mode in ('Master', 'Slave'):
            dispatcher = source[start:end].replace('AnalogOscillator::Render(', 'AnalogOscillator::RenderSync'+mode+'(')
            dispatcher = dispatcher.replace(selector, '  RenderFn fn = shape_ == OSC_SHAPE_SQUARE ? &AnalogOscillator::RenderSquare'+mode+' : &AnalogOscillator::RenderSaw'+mode+';')
            dispatchers.append(dispatcher)
        source = source[:end] + '\n\n' + '\n\n'.join(dispatchers) + source[end:]
        start = source.index('AnalogOscillator::RenderFn AnalogOscillator::fn_table_[]')
        end = source.index('\n};', start) + 3
        table = source[start:end].replace('::fn_table_', '::no_sync_table_')
        table = re.sub(r'(&AnalogOscillator::Render\w+)', r'\1NoSync', table)
        source = source.replace('}  // namespace braids', '\n'.join(fast_functions) + '\n' + table + '\n\n#include "optimized/fold_pair.hpp"\n\n}  // namespace braids')
    if kind == 'analog':
        source = triple_mix.transform_analog(source)
        source = analog_state.transform(source)
        source = sync_dispatch.transform_analog(source)
        source = analog_division.transform(source)
    pitch_lookup = '''  uint32_t a = lut_oscillator_increments[ref_pitch >> 4];
  uint32_t b = lut_oscillator_increments[(ref_pitch >> 4) + 1];
  uint32_t phase_increment = a + \\
      (static_cast<int32_t>(b - a) * (ref_pitch & 0xf) >> 4);'''
    if source.count(pitch_lookup) != (2 if kind == 'digital' else 1):
        raise ValueError('Unexpected pitch interpolation sites')
    source = source.replace(pitch_lookup,
                            '  uint32_t phase_increment = macroosc::pitch_increments[ref_pitch];')
    source = '#include "optimized/pitch_table.hpp"\n' + source
    if kind == 'digital':
        source = transform_drum_filters(source)
        source = harmonic_render.transform(source)
        source = wave_line_render.transform(source)
        source = flute_render.transform(source)
        source = swarm_snare_render.transform(source)
        source = fof_render.transform(source)
        source = digital_filter_render.transform(source)
        source = byte_mix_render.transform(source)
    source = restrict_buffers(source)
    source = '\n'.join(line.rstrip() for line in source.splitlines()) + '\n'
    source = dispatch_render.transform(source, kind)
    source = signed_sine_render.transform(packed_lookups(source), kind)
    if kind == 'analog':
        source = analog_dispatch_render.transform_analog(source)
        source = triple_square_render.transform(source)
    if kind == 'digital':
        source = swarm_group_render.transform(source)
        source = cymbal_render.transform(source)
        source = kick_render.transform(source)
        source = struck_drum_render.transform(source)
        source = flute_bounds_render.transform(source)
        source = flute_jet_render.transform(source)
        import harmonic_sample_render
        source = harmonic_sample_render.transform(source)
    return ('// Generated by optimized/generate_oscillators.py; edit the generator.\n'
            '// Original vendor file and its full licence remain unchanged.\n'
            '#include "optimized/pitch_math.hpp"\n' + source).encode()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    outputs = {HERE/(kind + '_oscillator.cpp'): generated(kind) for kind in SOURCES}
    header = (HERE.parent/'vendor/braids/analog_oscillator.h').read_bytes().replace(b'\r\n', b'\n')
    if hashlib.sha256(header).hexdigest() != '6870f34dd3d6c67e12227299fca4d49ccb15a651b89293bb0f688f313b964991':
        raise ValueError('Analog header changed')
    header = header.decode()
    header = header.replace('#ifndef BRAIDS_ANALOG_OSCILLATOR_H_\n#define BRAIDS_ANALOG_OSCILLATOR_H_', '#pragma once').replace('#endif // BRAIDS_ANALOG_OSCILLATOR_H_', '')
    declarations = re.findall(r'  void Render\w+\(const uint8_t\*, int16_t\*, uint8_t\*, size_t\);', header)
    if len(declarations) != 9:
        raise ValueError('Analog declarations changed')
    extra = '\n'.join(d.replace('(', 'NoSync(', 1) for d in declarations)
    extra += '\n' + '\n'.join(d.replace('(', mode+'(', 1) for d in declarations for mode in ('Master','Slave') if 'RenderSquare(' in d or 'RenderSaw(' in d)
    header = header.replace(' private:', '  void RenderNoSync(const uint8_t*, int16_t*, uint8_t*, size_t);\n\n private:', 1)
    header = header.replace(' private:', '  void RenderSyncMaster(const uint8_t*, int16_t*, uint8_t*, size_t);\n  void RenderSyncSlave(const uint8_t*, int16_t*, uint8_t*, size_t);\n\n private:', 1)
    header = header.replace(' private:', '  void RenderFoldPair(AnalogOscillator&, int16_t*, size_t, int32_t, int32_t);\n\n private:', 1)
    header = header.replace('  static RenderFn fn_table_[];', '  static RenderFn fn_table_[];\n  static RenderFn no_sync_table_[];\n' + extra)
    header = triple_mix.transform_header(header)
    header = sync_dispatch.transform_header(header)
    header = analog_dispatch_render.transform_header(header)
    outputs[HERE/'braids/analog_oscillator.h'] = ('// Generated by optimized/generate_oscillators.py. Original MIT notice follows.\n'+header).encode()
    header = (HERE.parent/'vendor/braids/digital_oscillator.h').read_bytes().replace(b'\r\n', b'\n')
    if hashlib.sha256(header).hexdigest() != 'e348500313b5d5bdd534aca68f021a4f283dfe6cc5d2dc9a4daf300ab1c17492':
        raise ValueError('Digital header changed')
    header = header.decode()
    header = header.replace('#ifndef BRAIDS_DIGITAL_OSCILLATOR_H_\n#define BRAIDS_DIGITAL_OSCILLATOR_H_', '#pragma once').replace('#endif // BRAIDS_DIGITAL_OSCILLATOR_H_', '')
    old = '  int32_t svf_bp[kNumFormants];\n};'
    if header.count(old) != 1:
        raise ValueError('FOF state shape changed')
    header = header.replace(old, '  int32_t svf_bp[kNumFormants];\n  uint32_t control_key;\n  int16_t coefficients[kNumFormants];\n  int16_t amplitude;\n};')
    declarations = '\n'.join('  template<bool Pure> void '+name+'Paired(const uint8_t*, int16_t*, size_t);' for name in WAVE_RENDERERS)
    header = header.replace(' private:', ' private:\n'+declarations, 1)
    header = wave_line_render.transform_header(header)
    header = digital_filter_render.transform_header(header)
    outputs[HERE/'braids/digital_oscillator.h'] = ('// Generated by optimized/generate_oscillators.py. Original MIT notice follows.\n'+header).encode()
    header = (HERE.parent/'vendor/braids/excitation.h').read_bytes().replace(b'\r\n', b'\n')
    if hashlib.sha256(header).hexdigest() != 'e9555383effc411f26afaaf475a92ceabad59a4891569dc3d4c3cd14f3ee3197':
        raise ValueError('Excitation header changed')
    header = header.decode().replace('#ifndef BRAIDS_EXCITATION_H_\n#define BRAIDS_EXCITATION_H_', '#pragma once').replace('#endif  // BRAIDS_EXCITATION_H_', '')
    header = header.replace('  inline int32_t Process() {',
        '  // A zero state without a pending trigger stays zero until Trigger().\n'
        '  inline bool silent() const { return state_ == 0 && counter_ == 0; }\n\n'
        '  // Timer-only use: the caller must not observe the sample or state\n'
        '  // before Init() resets it. Kick pulse2 observes only done().\n'
        '  inline void AdvanceTriggerOnly() { if (counter_ > 0) --counter_; }\n\n'
        '  inline int32_t Process() {')
    outputs[HERE/'braids/excitation.h'] = ('// Generated by optimized/generate_oscillators.py. Original MIT notice follows.\n'+header).encode()
    for target, expected in outputs.items():
        if args.check:
            if not target.is_file() or target.read_bytes().replace(b'\r\n', b'\n') != expected:
                raise ValueError(str(target) + ' overlay does not match the reviewed transformation')
            print(target.name + ' overlay matches its vendor source and reviewed transformation')
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(expected)


if __name__ == '__main__':
    main()
