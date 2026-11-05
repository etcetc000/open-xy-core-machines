"""Square Sync: both hard-synced squares and the crossfade as the SDK kernel.

opxy::kernels::square_sync runs the master's and the slave's original sample
loops (same increment interpolation, edge times, polynomial steps and sync
report) and then the Dual Sync crossfade, sample by sample, without the two
intermediate buffers. Each oscillator's shape-change initialization, pitch
clamp and interpolation setup stay in AnalogOscillator::RenderSquareSyncPair.
"""

MACRO_OLD = '''  if (base_shape == OSC_SHAPE_SQUARE) {
    analog_oscillator_[0].RenderSquareSyncMaster(nullptr, buffer, sync_buffer_, size);
    analog_oscillator_[1].RenderSquareSyncSlave(sync_buffer_, temp_buffer_, nullptr, size);
  } else {'''

MACRO_NEW = '''  if (base_shape == OSC_SHAPE_SQUARE) {
    // Both squares and the crossfade below, in one pass.
    analog_oscillator_[0].RenderSquareSyncPair(analog_oscillator_[1], buffer, size,
        previous_parameter_[1], parameter_[1] - previous_parameter_[1],
        macroosc::chunk_math::parameter1_increment(size));
    previous_parameter_[1] = parameter_[1];
    return;
  } else {'''

DECLARATION = '  void RenderFoldPair(AnalogOscillator&, int16_t*, size_t, int32_t, int32_t);\n'
INCLUDE = '#include "optimized/fold_pair.hpp"\n'


def transform_macro(source):
    if source.count(MACRO_OLD) != 1:
        raise ValueError('Square Sync dispatch changed')
    return source.replace(MACRO_OLD, MACRO_NEW)


def transform_analog(source):
    if source.count(INCLUDE) != 1:
        raise ValueError('Analog pair includes changed')
    source = source.replace(INCLUDE, INCLUDE + '#include "optimized/square_sync_pair.hpp"\n')
    return '#include <opxy/kernels/square_sync.hpp>\n' + source


def transform_header(header):
    if header.count(DECLARATION) != 1:
        raise ValueError('Analog pair declarations changed')
    return header.replace(DECLARATION, DECLARATION +
                          '  void RenderSquareSyncPair(AnalogOscillator&, int16_t*, size_t, int32_t, int32_t, int32_t);\n')
