// Delay-line leases for the voices: opxy::LeasePool's layout, owners, stamps
// and choice of slot, but a newly chosen slot is cleared only where earlier
// models may have written, a word at a time, and not where the new model
// clears for itself before it reads. LeasePool clears all of every new slot a
// byte at a time, which cost the note-on block of the delay-line models about
// 8,200 cycles; the slot each model then reads is the same, all zero.
#pragma once
#include <opxy/types.hpp>

namespace macroosc {
// One model's use of a slot: the leading words it may write, and the leading
// words it clears itself on every strike, before it reads any of them.
struct DelayUse {
  opxy::u32 used;
  opxy::u32 self_cleared;
};

template <class Lines, opxy::u32 Count>
struct DelayPool {
  static_assert(Count > 0 && Count <= 256, "Bounded lease capacity required");
  static_assert(sizeof(Lines) % 4 == 0, "Slots clear by whole words");
  union Slot {
    Lines lines;
    opxy::u32 words[sizeof(Lines) / 4];
  };
  Slot resources[Count];
  // Low 16 bits: the owner, 0 when free. High 16 bits: the leading words of
  // the slot that may be nonzero; every later word of the slot is zero.
  opxy::u32 owners[Count];
  opxy::u32 stamps[Count];
  opxy::u32 clock;

  static opxy::u32 owner_of(opxy::u32 entry) { return entry & 0xffffu; }

  Lines* get(opxy::u32 owner, opxy::u32 lease) {
    return owner && lease && lease <= Count && owner_of(owners[lease - 1]) == owner
        ? &resources[lease - 1].lines : nullptr;
  }
  void release(opxy::u32 owner, opxy::u32 lease) {
    if (get(owner, lease)) owners[lease - 1] &= 0xffff0000u;
  }
  // Owners are the SDK's voice IDs, far below 65536.
  opxy::u32 acquire(opxy::u32 owner, opxy::u32 lease, DelayUse use) {
    if (!owner) return 0;
    ++clock;
    if (get(owner, lease)) {
      stamps[lease - 1] = clock;
      if ((owners[lease - 1] >> 16) < use.used) owners[lease - 1] = owner | (use.used << 16);
      return lease;
    }
    opxy::u32 chosen = 0;
    for (opxy::u32 i = 0; i < Count; ++i) {
      if (!owner_of(owners[i])) { chosen = i; break; }
      if (clock - stamps[i] > clock - stamps[chosen]) chosen = i;
    }
    // Words below self_cleared are the model's to clear; it writes no further than used.
    const opxy::u32 dirty = owners[chosen] >> 16;
    opxy::u32* words = resources[chosen].words;
    for (opxy::u32 i = use.self_cleared; i < dirty; ++i) words[i] = 0;
    owners[chosen] = owner | (use.used << 16);
    stamps[chosen] = clock;
    return chosen + 1;
  }
};
}  // namespace macroosc
