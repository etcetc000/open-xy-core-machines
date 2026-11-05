// Fused derivative of the MIT-licensed Braids square hard-sync pair and the
// Dual Sync crossfade. Copyright 2012 Emilie Gillet. Full notice retained in
// vendor/LICENSE and the including generated analog oscillator source.
#pragma once

// Included within namespace braids by analog_oscillator.cpp, after
// <opxy/kernels/square_sync.hpp>. The master and slave keep their own
// shape-change initialization, pitch clamp and increment interpolation; the
// SDK kernel then runs both sample loops and the crossfade the macro
// oscillator applied to their two buffers, with the same per-sample order.
// The caller passes the crossfade step (its parameter interpolation's).
void AnalogOscillator::RenderSquareSyncPair(AnalogOscillator& slave, int16_t* __restrict__ buffer, size_t size,
                                           int32_t balance_start, int32_t balance_delta, int32_t xfade_step) {
  opxy::kernels::SquareSync pair;
  auto prepare = [size](AnalogOscillator& osc, opxy::kernels::SquareSyncOscillator& o) {
    if (osc.shape_ != osc.previous_shape_) {
      osc.Init(); osc.previous_shape_ = osc.shape_;
    }
    osc.phase_increment_ = osc.ComputePhaseIncrement(osc.pitch_);
    if (osc.pitch_ > kHighestNote) osc.pitch_ = kHighestNote;
    else if (osc.pitch_ < 0) osc.pitch_ = 0;
    o.phase = osc.phase_;
    o.increment = osc.previous_phase_increment_;
    o.step = osc.previous_phase_increment_ < osc.phase_increment_
        ? macroosc::chunk_math::phase_delta(osc.phase_increment_ - osc.previous_phase_increment_, size)
        : ~macroosc::chunk_math::phase_delta(osc.previous_phase_increment_ - osc.phase_increment_, size);
    o.next = osc.next_sample_;
    o.high = osc.high_;
  };
  prepare(*this, pair.master);
  prepare(slave, pair.slave);
  pair.balance_start = balance_start;
  pair.balance_delta = balance_delta;
  pair.xfade_step = xfade_step;
  opxy::kernels::square_sync(pair, buffer, size);
  auto finish = [](AnalogOscillator& osc, const opxy::kernels::SquareSyncOscillator& o) {
    osc.phase_ = o.phase;
    osc.previous_phase_increment_ = o.increment;
    osc.next_sample_ = o.next;
    osc.high_ = o.high != 0;
  };
  finish(*this, pair.master);
  finish(slave, pair.slave);
}
