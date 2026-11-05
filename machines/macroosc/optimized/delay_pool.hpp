// Delay-line leases for the voices: opxy::LeasePool's layout, owners, stamps
// and choice of slot, but a newly chosen slot is cleared only where earlier
// models may have written, a word at a time, and not where the new model
// clears for itself before it reads. LeasePool clears all of every new slot a
// byte at a time, which cost the note-on block of the delay-line models about
// 8,200 cycles; the slot each model then reads is the same, all zero.
//
// A marked use (the plucked strings) also keeps, in the slot's last word, which
// no model's lines reach, a mark of what it wrote: nonzero only while every
// word of its lines outside the marked parts is zero. A newly chosen slot
// whose last owner left a valid mark is not cleared below the use's words; the
// mark goes to the caller, which clears the marked parts itself.
#pragma once
#include <opxy/types.hpp>

namespace macroosc {
// One model's use of a slot: the leading words it may write, and the leading
// words it clears itself on every strike, before it reads any of them.
struct DelayUse {
  opxy::u32 used;
  opxy::u32 self_cleared;
};
// A valid mark with nothing yet marked.
const opxy::u32 kMarkValid = 0x80000000u;

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

  static const opxy::u32 kMarkWord = sizeof(Lines) / 4 - 1;
  static opxy::u32 owner_of(opxy::u32 entry) { return entry & 0xffffu; }

  Lines* get(opxy::u32 owner, opxy::u32 lease) {
    return owner && lease && lease <= Count && owner_of(owners[lease - 1]) == owner
        ? &resources[lease - 1].lines : nullptr;
  }
  // Raises a valid mark on the owner's slot; an invalid one stays invalid.
  void raise_mark(opxy::u32 owner, opxy::u32 lease, opxy::u32 value) {
    if (Lines* lines = get(owner, lease)) {
      opxy::u32& word = reinterpret_cast<Slot*>(lines)->words[kMarkWord];
      if (word) word |= value;
    }
  }
  void release(opxy::u32 owner, opxy::u32 lease) {
    if (get(owner, lease)) owners[lease - 1] &= 0xffff0000u;
  }
  // Owners are the SDK's voice IDs, far below 65536. A marked use passes
  // `mark`: on a newly chosen slot it receives the last owner's valid mark, or
  // 0 when the pool cleared the use's words; on its own slot it is untouched.
  opxy::u32 acquire(opxy::u32 owner, opxy::u32 lease, DelayUse use, opxy::u32* mark = nullptr) {
    if (!owner) return 0;
    ++clock;
    if (get(owner, lease)) {
      stamps[lease - 1] = clock;
      if (!mark) resources[lease - 1].words[kMarkWord] = 0;
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
    const opxy::u32 previous = mark ? words[kMarkWord] : 0;
    for (opxy::u32 i = previous ? use.used : use.self_cleared; i < dirty; ++i) words[i] = 0;
    words[kMarkWord] = mark ? kMarkValid : 0;
    if (mark) *mark = previous;
    owners[chosen] = owner | (use.used << 16);
    stamps[chosen] = clock;
    return chosen + 1;
  }
};
}  // namespace macroosc
