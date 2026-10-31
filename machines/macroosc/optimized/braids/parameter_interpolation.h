// Exact chunk quotients for analog phase interpolation, folded oscillators
// and digital FM parameter interpolation.
// Copyright 2012 Emilie Gillet. Derived from MIT-licensed Braids; the full
// notice is retained in vendor/LICENSE and the included original header.
// The remaining interpolation macros and all vendor files stay unchanged.
#pragma once
#include "../../vendor/braids/parameter_interpolation.h"
#include "../chunk_math.hpp"

#undef BEGIN_INTERPOLATE_PARAMETER_0
#define BEGIN_INTERPOLATE_PARAMETER_0 \
  int32_t parameter_0_start = previous_parameter_[0]; \
  int32_t parameter_0_delta = parameter_[0] - previous_parameter_[0]; \
  int32_t parameter_increment = macroosc::chunk_math::interpolation_increment(size); \
  int32_t parameter_xfade = 0;

#undef BEGIN_INTERPOLATE_PARAMETER_1
#define BEGIN_INTERPOLATE_PARAMETER_1 \
  int32_t parameter_1_start = previous_parameter_[1]; \
  int32_t parameter_1_delta = parameter_[1] - previous_parameter_[1]; \
  int32_t parameter_increment = macroosc::chunk_math::parameter1_increment(size); \
  int32_t parameter_xfade = 0;

#undef BEGIN_INTERPOLATE_PHASE_INCREMENT
#define BEGIN_INTERPOLATE_PHASE_INCREMENT \
  uint32_t phase_increment = previous_phase_increment_; \
  uint32_t phase_increment_increment = \
      previous_phase_increment_ < phase_increment_ \
      ? macroosc::chunk_math::phase_delta(phase_increment_ - previous_phase_increment_, size) \
      : ~macroosc::chunk_math::phase_delta(previous_phase_increment_ - phase_increment_, size);

#undef BEGIN_INTERPOLATE_PARAMETER
#define BEGIN_INTERPOLATE_PARAMETER \
  int32_t parameter_start = previous_parameter_; \
  int32_t parameter_delta = parameter_ - previous_parameter_; \
  int32_t parameter_increment = macroosc::chunk_math::interpolation_increment(size); \
  int32_t parameter_xfade = 0;

