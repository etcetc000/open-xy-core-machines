// Fused derivative of the MIT-licensed Braids sine/triangle folding loops.
// Copyright 2012 Emilie Gillet. Full notice retained in vendor/LICENSE and
// the including generated analog oscillator source.
#pragma once

// Included within namespace braids by analog_oscillator.cpp. Preserve both
// oscillator states, the original shape-change initialization and two
// oversampling steps; opxy::kernels::fold_pair (SDK) runs the sample loop.
// Share increment work only when both initial values and targets match.
// High-pitch attenuation and shape changes can make the two states differ.
// Keep separate phases even on the shared path: earlier shapes may offset them.
void AnalogOscillator::RenderFoldPair(AnalogOscillator& tri, int16_t* __restrict__ buffer, size_t size,
                                     int32_t balance_start, int32_t balance_delta) {
  auto prepare = [](AnalogOscillator& osc) {
    if (osc.shape_ != osc.previous_shape_) {
      osc.Init(); osc.previous_shape_ = osc.shape_;
    }
  };
  prepare(*this); prepare(tri);
  phase_increment_=ComputePhaseIncrement(pitch_);
  tri.phase_increment_=pitch_==tri.pitch_ ? phase_increment_ : tri.ComputePhaseIncrement(tri.pitch_);
  if (pitch_>kHighestNote) pitch_=kHighestNote;
  else if (pitch_<0) pitch_=0;
  if (tri.pitch_>kHighestNote) tri.pitch_=kHighestNote;
  else if (tri.pitch_<0) tri.pitch_=0;
  uint32_t sine_phase=phase_, tri_phase=tri.phase_;
  uint32_t sine_inc=previous_phase_increment_, tri_inc=tri.previous_phase_increment_;
  const uint32_t sine_step=sine_inc<phase_increment_ ? macroosc::chunk_math::phase_delta(phase_increment_-sine_inc,size)
      : ~macroosc::chunk_math::phase_delta(sine_inc-phase_increment_,size);
  // Identical initial/target increments imply the exact same quotient,
  // including the original complemented step on a falling or equal target.
  const uint32_t tri_step=(tri_inc==sine_inc && tri.phase_increment_==phase_increment_) ? sine_step : tri_inc<tri.phase_increment_ ? macroosc::chunk_math::phase_delta(tri.phase_increment_-tri_inc,size)
      : ~macroosc::chunk_math::phase_delta(tri_inc-tri.phase_increment_,size);
  const int32_t sine_start=previous_parameter_, sine_delta=parameter_-previous_parameter_;
  const int32_t tri_start=tri.previous_parameter_, tri_delta=tri.parameter_-tri.previous_parameter_;
  // The SDK's fold-pair kernel runs the general interpolating loop: with
  // unchanged or shared controls it reproduces the specialized loops exactly.
  opxy::kernels::FoldPair s;
  s.sine_phase=sine_phase; s.triangle_phase=tri_phase;
  s.sine_increment=sine_inc; s.triangle_increment=tri_inc;
  s.sine_step=sine_step; s.triangle_step=tri_step;
  s.xfade_step=macroosc::chunk_math::interpolation_increment(size);
  s.sine_start=sine_start; s.sine_delta=sine_delta;
  s.triangle_start=tri_start; s.triangle_delta=tri_delta;
  s.balance_start=balance_start; s.balance_delta=balance_delta;
  s.sine=macroosc::sine_pairs; s.sine_fold=macroosc::sine_fold_pairs; s.triangle_fold=macroosc::tri_fold_pairs;
  opxy::kernels::fold_pair(s, buffer, size);
  phase_=s.sine_phase;tri.phase_=s.triangle_phase;
  previous_phase_increment_=s.sine_increment;tri.previous_phase_increment_=s.triangle_increment;
  previous_parameter_=parameter_;tri.previous_parameter_=tri.parameter_;
}

