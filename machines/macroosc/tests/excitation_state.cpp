#include "braids/excitation.h"
#define Excitation OriginalExcitation
#include "vendor/braids/excitation.h"
#undef Excitation
#include <cassert>
#include <cstdio>

static_assert(sizeof(braids::Excitation) == sizeof(braids::OriginalExcitation), "Pulse layout grew");

static void check_timer_only() {
  const unsigned delays[] = {0, 1, 48, 192, 255, 65535};
  const unsigned periods[] = {1, 3, 49, 193, 257, 1024, 70000};
  const int levels[] = {0, 18000, -18000, 491520};
  unsigned comparisons = 0;
  for (unsigned delay : delays) for (unsigned period : periods) for (int level : levels) {
    braids::OriginalExcitation original;
    braids::Excitation candidate;
    original.Init();candidate.Init();
    original.set_delay(delay);candidate.set_delay(delay);
    for (unsigned i = 0; i < 70002; ++i) {
      if (!(i % period)) { original.Trigger(level);candidate.Trigger(level); }
      if (i == 500) {
        original.set_delay(delay ? 0 : 192);candidate.set_delay(delay ? 0 : 192);
      }
      original.Process();candidate.AdvanceTriggerOnly();
      assert(original.done() == candidate.done());
      ++comparisons;
    }
    // Shape changes reset the unused sample state before any other engine
    // can observe it. Verify normal sample generation after that reset.
    original.Init();candidate.Init();
    original.Trigger(1234);candidate.Trigger(1234);
    for (unsigned i = 0; i < 64; ++i) assert(original.Process() == candidate.Process());
  }
  std::printf("PASS %u timer-only steps and post-reset sample comparisons\n", comparisons);
}

int main() {
  check_timer_only();
  unsigned skipped = 0, comparisons = 0;
  const unsigned delays[] = {0, 1, 48, 255};
  const unsigned decays[] = {0, 1200, 1536, 3072, 4093, 4095, 65535};
  const int levels[] = {0, 13107, -32768, 491520};
  const unsigned periods[] = {1, 3, 49, 257, 1024};
  for (unsigned delay : delays) for (unsigned decay : decays)
  for (int level : levels) for (unsigned period : periods) {
    braids::OriginalExcitation original;
    braids::Excitation candidate;
    original.Init();candidate.Init();
    original.set_delay(delay);candidate.set_delay(delay);
    original.set_decay(decay);candidate.set_decay(decay);
    for (unsigned i = 0; i < 4096; ++i) {
      if (!(i % period)) {
        original.Trigger(level);candidate.Trigger(level);
        assert(!candidate.silent()); // Even a zero-level trigger is pending.
      }
      const int expected = original.Process();
      int actual;
      if (candidate.silent()) { actual = 0; ++skipped; }
      else actual = candidate.Process();
      assert(expected == actual);
      assert(original.done() == candidate.done());
      ++comparisons;
    }
  }
  assert(skipped > 0);
  std::printf("PASS %u pulse comparisons, %u zero-state steps skipped\n", comparisons, skipped);
}
