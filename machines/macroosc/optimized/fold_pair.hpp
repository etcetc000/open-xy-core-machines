// Fused derivative of the MIT-licensed Braids sine/triangle folding loops.
// Copyright 2012 Emilie Gillet. Full notice retained in vendor/LICENSE and
// the including generated analog oscillator source.
#pragma once

// Included within namespace braids by analog_oscillator.cpp. Preserve both
// oscillator states, the original shape-change initialization and two
// oversampling steps. Sharing the interpolation fraction removes duplicate
// work; explicitly unrolling the two steps avoids a target loop/spill penalty.
// Share gain/increment work only when both initial values and deltas match.
// High-pitch attenuation and shape changes can make the two states differ.
// Keep separate phases even on the shared path: earlier shapes may offset them.
// The mixer uses an unsigned residual to preserve overflow and floor rounding;
// high multiples of 65536 vanish in the final int16 conversion, as in Mix.
// Triangle: low16((phase>>16)<<1) equals low16((phase>>15)&~1u).
// Subtracting the top bit from zero supplies the original all-zero/all-one
// XOR mask. The final int16 conversion and32768 offset are unchanged.
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
  const int32_t step=macroosc::chunk_math::interpolation_increment(size);
  int32_t xfade=0;
  if (sine_start==tri_start && sine_delta==tri_delta && sine_inc==tri_inc && sine_step==tri_step) {
  // With unchanged controls the original interpolation is constant. Keep
  // the full moving-control path and both independently evolving phases.
  // floor(parameter*15/16) == parameter-ceil(parameter/16), exactly.
  if (sine_delta==0 && balance_delta==0) {
    const int16_t sine_gain=2048+sine_start-((sine_start+15)>>4);
    const uint16_t balance=balance_start<<1;
  while(size--) {
    sine_inc+=sine_step; tri_inc=sine_inc;
    const int16_t tri_gain=sine_gain;
    // Each halved signed16 term is in[-16384,16383]; two terms fit signed16.
    // Accumulate in32 bits to avoid redundant intermediate narrowing.
    int32_t sine_sum=0,tri_sum=0;
    {
      sine_phase+=sine_inc>>1;
      int16_t sine=macroosc::paired824(macroosc::sine_pairs,sine_phase);
      sine=(sine*sine_gain)>>15;
      sine_sum+=macroosc::paired88(macroosc::sine_fold_pairs,sine+32768)>>1;
      tri_phase+=tri_inc>>1;
      int16_t triangle=((tri_phase>>15)&~1u)^(0u-(tri_phase>>31));
      triangle+=32768;
      triangle=(triangle*tri_gain)>>15;
      tri_sum+=macroosc::paired88(macroosc::tri_fold_pairs,triangle+32768)>>1;
    }
    {
      sine_phase+=sine_inc>>1;
      int16_t sine=macroosc::paired824(macroosc::sine_pairs,sine_phase);
      sine=(sine*sine_gain)>>15;
      sine_sum+=macroosc::paired88(macroosc::sine_fold_pairs,sine+32768)>>1;
      tri_phase+=tri_inc>>1;
      int16_t triangle=((tri_phase>>15)&~1u)^(0u-(tri_phase>>31));
      triangle+=32768;
      triangle=(triangle*tri_gain)>>15;
      tri_sum+=macroosc::paired88(macroosc::tri_fold_pairs,triangle+32768)>>1;
    }
    const int16_t sine=sine_sum,triangle=tri_sum;
    *buffer++=macroosc::mix_exact(sine,triangle,balance);
  }
  } else {
  while(size--) {
    xfade+=step; sine_inc+=sine_step; tri_inc=sine_inc;
    const int32_t sine_parameter=sine_start+((sine_delta*xfade)>>15);
    const int16_t sine_gain=2048+sine_parameter-((sine_parameter+15)>>4);
    const int16_t tri_gain=sine_gain;
    // Each halved signed16 term is in[-16384,16383]; two terms fit signed16.
    // Accumulate in32 bits to avoid redundant intermediate narrowing.
    int32_t sine_sum=0,tri_sum=0;
    {
      sine_phase+=sine_inc>>1;
      int16_t sine=macroosc::paired824(macroosc::sine_pairs,sine_phase);
      sine=(sine*sine_gain)>>15;
      sine_sum+=macroosc::paired88(macroosc::sine_fold_pairs,sine+32768)>>1;
      tri_phase+=tri_inc>>1;
      int16_t triangle=((tri_phase>>15)&~1u)^(0u-(tri_phase>>31));
      triangle+=32768;
      triangle=(triangle*tri_gain)>>15;
      tri_sum+=macroosc::paired88(macroosc::tri_fold_pairs,triangle+32768)>>1;
    }
    {
      sine_phase+=sine_inc>>1;
      int16_t sine=macroosc::paired824(macroosc::sine_pairs,sine_phase);
      sine=(sine*sine_gain)>>15;
      sine_sum+=macroosc::paired88(macroosc::sine_fold_pairs,sine+32768)>>1;
      tri_phase+=tri_inc>>1;
      int16_t triangle=((tri_phase>>15)&~1u)^(0u-(tri_phase>>31));
      triangle+=32768;
      triangle=(triangle*tri_gain)>>15;
      tri_sum+=macroosc::paired88(macroosc::tri_fold_pairs,triangle+32768)>>1;
    }
    const int16_t sine=sine_sum,triangle=tri_sum;
    const uint16_t balance=(balance_start+((balance_delta*xfade)>>15))<<1;
    *buffer++=macroosc::mix_exact(sine,triangle,balance);
  }
  }
  } else {
  while(size--) {
    xfade+=step; sine_inc+=sine_step; tri_inc+=tri_step;
    const int32_t sine_parameter=sine_start+((sine_delta*xfade)>>15);
    const int32_t tri_parameter=tri_start+((tri_delta*xfade)>>15);
    const int16_t sine_gain=2048+sine_parameter-((sine_parameter+15)>>4);
    const int16_t tri_gain=2048+tri_parameter-((tri_parameter+15)>>4);
    // Each halved signed16 term is in[-16384,16383]; two terms fit signed16.
    // Accumulate in32 bits to avoid redundant intermediate narrowing.
    int32_t sine_sum=0,tri_sum=0;
    {
      sine_phase+=sine_inc>>1;
      int16_t sine=macroosc::paired824(macroosc::sine_pairs,sine_phase);
      sine=(sine*sine_gain)>>15;
      sine_sum+=macroosc::paired88(macroosc::sine_fold_pairs,sine+32768)>>1;
      tri_phase+=tri_inc>>1;
      int16_t triangle=((tri_phase>>15)&~1u)^(0u-(tri_phase>>31));
      triangle+=32768;
      triangle=(triangle*tri_gain)>>15;
      tri_sum+=macroosc::paired88(macroosc::tri_fold_pairs,triangle+32768)>>1;
    }
    {
      sine_phase+=sine_inc>>1;
      int16_t sine=macroosc::paired824(macroosc::sine_pairs,sine_phase);
      sine=(sine*sine_gain)>>15;
      sine_sum+=macroosc::paired88(macroosc::sine_fold_pairs,sine+32768)>>1;
      tri_phase+=tri_inc>>1;
      int16_t triangle=((tri_phase>>15)&~1u)^(0u-(tri_phase>>31));
      triangle+=32768;
      triangle=(triangle*tri_gain)>>15;
      tri_sum+=macroosc::paired88(macroosc::tri_fold_pairs,triangle+32768)>>1;
    }
    const int16_t sine=sine_sum,triangle=tri_sum;
    const uint16_t balance=(balance_start+((balance_delta*xfade)>>15))<<1;
    *buffer++=macroosc::mix_exact(sine,triangle,balance);
  }
  }
  phase_=sine_phase;tri.phase_=tri_phase;
  previous_phase_increment_=sine_inc;tri.previous_phase_increment_=tri_inc;
  previous_parameter_=parameter_;tri.previous_parameter_=tri.parameter_;
}

