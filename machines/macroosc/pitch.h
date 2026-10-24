// Copyright 2012 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Pitch lookup for the 44.1 kHz port. OP-XY: written for this port in Braids'
// pitch convention (1/128 semitone, MIDI 0 = 0); not an upstream file, and it
// carries Braids' MIT notice as derived work.

#pragma once

#include "stmlib/stmlib.h"

namespace braids {

// round(65536 * log2(1 + i / 256)), i = 0..256.
static const uint32_t kOpxyLog2Q16[257] = {
      0,   369,   736,  1102,  1466,  1829,  2190,  2551,
   2909,  3267,  3623,  3978,  4331,  4683,  5034,  5384,
   5732,  6079,  6425,  6769,  7112,  7454,  7795,  8134,
   8473,  8810,  9146,  9480,  9814, 10146, 10477, 10807,
  11136, 11464, 11791, 12116, 12440, 12764, 13086, 13407,
  13727, 14046, 14363, 14680, 14996, 15310, 15624, 15937,
  16248, 16559, 16868, 17177, 17484, 17791, 18096, 18401,
  18704, 19007, 19308, 19609, 19909, 20207, 20505, 20802,
  21098, 21393, 21687, 21980, 22272, 22564, 22854, 23144,
  23433, 23720, 24007, 24293, 24579, 24863, 25146, 25429,
  25711, 25992, 26272, 26551, 26830, 27108, 27384, 27660,
  27936, 28210, 28484, 28757, 29029, 29300, 29571, 29840,
  30109, 30378, 30645, 30912, 31178, 31443, 31707, 31971,
  32234, 32496, 32758, 33019, 33279, 33538, 33797, 34055,
  34312, 34569, 34825, 35080, 35334, 35588, 35841, 36094,
  36346, 36597, 36847, 37097, 37346, 37595, 37842, 38090,
  38336, 38582, 38827, 39072, 39316, 39559, 39802, 40044,
  40286, 40527, 40767, 41006, 41246, 41484, 41722, 41959,
  42196, 42432, 42667, 42902, 43137, 43370, 43603, 43836,
  44068, 44300, 44530, 44761, 44990, 45220, 45448, 45676,
  45904, 46131, 46357, 46583, 46809, 47034, 47258, 47482,
  47705, 47928, 48150, 48372, 48593, 48813, 49034, 49253,
  49472, 49691, 49909, 50127, 50344, 50560, 50776, 50992,
  51207, 51422, 51636, 51850, 52063, 52276, 52488, 52700,
  52911, 53122, 53332, 53542, 53751, 53960, 54169, 54377,
  54584, 54791, 54998, 55204, 55410, 55615, 55820, 56025,
  56229, 56432, 56635, 56838, 57040, 57242, 57443, 57644,
  57845, 58045, 58245, 58444, 58643, 58841, 59039, 59237,
  59434, 59631, 59827, 60023, 60219, 60414, 60609, 60803,
  60997, 61190, 61384, 61576, 61769, 61961, 62152, 62343,
  62534, 62725, 62915, 63104, 63294, 63483, 63671, 63859,
  64047, 64234, 64421, 64608, 64794, 64980, 65166, 65351,
  65536,
};

// round(128 * 69 - 1536 * log2(440 * 2^32 / 44100)).
static const int32_t kOpxyPitchOffset = -30110;

}  // namespace braids

