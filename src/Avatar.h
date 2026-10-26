#pragma once

// Avatar app: a little animated character that does something different every
// so often (waves, listens to music, dances, rests, types, sleeps at night).
//
// The artwork is private and NOT part of this repository. Put the generated
// src/AvatarArt.h next to this file (it is listed in .gitignore). Without it
// the app still works and shows a simple placeholder face.
//
// AvatarArt.h holds, per activity, a 256-colour palette and kFrames frames of
// palette indices (kW x kH), each compressed with raw deflate. A frame is
// inflated with the inflater in the ESP32-S3 ROM (zlib on the PC preview).

#include <stdint.h>
#include <string.h>

#if __has_include("AvatarArt.h")
#include "AvatarArt.h"
#endif

#if defined(AVATAR_ART)
#if defined(ARDUINO)
#include "rom/miniz.h"
#else
#include <zlib.h>
#endif
#endif

namespace avatar {

enum class Activity : uint8_t { Wave, Listen, Dance, Rest, Type, Sleep };
constexpr int kActivityCount = 6;

#if defined(AVATAR_ART)
constexpr bool kHasArt = true;
constexpr int kX = avatar_art::kX, kY = avatar_art::kY;
constexpr int kW = avatar_art::kW, kH = avatar_art::kH;
constexpr int kFrames = avatar_art::kFrames;
#else
constexpr bool kHasArt = false;
constexpr int kX = 58, kY = 110, kW = 272, kH = 286;
constexpr int kFrames = 1;
#endif

// Inflates frame `frame` of activity `a` into RGB565 pixels (kW * kH) using
// `scratch` (kW * kH bytes). False when there is no art or the data is bad.
inline bool decodeFrame(Activity a, int frame, uint8_t *scratch, uint16_t *out) {
#if defined(AVATAR_ART)
  const int i = static_cast<int>(a) * kFrames + frame % kFrames;
  const uint8_t *src = avatar_art::kData + avatar_art::kOffset[i];
  const size_t srcLen = avatar_art::kSize[i];
  const size_t want = static_cast<size_t>(kW) * kH;
#if defined(ARDUINO)
  const size_t got = tinfl_decompress_mem_to_mem(scratch, want, src, srcLen, 0);
  if (got != want) return false;
#else
  z_stream z = {};
  if (inflateInit2(&z, -15) != Z_OK) return false;
  z.next_in = const_cast<uint8_t *>(src);
  z.avail_in = srcLen;
  z.next_out = scratch;
  z.avail_out = want;
  const int r = inflate(&z, Z_FINISH);
  inflateEnd(&z);
  if (r != Z_STREAM_END) return false;
#endif
  const uint16_t *pal = avatar_art::kPalette[static_cast<int>(a)];
  for (size_t p = 0; p < want; p++) out[p] = pal[scratch[p]];
  return true;
#else
  (void)a; (void)frame; (void)scratch; (void)out;
  return false;
#endif
}

} // namespace avatar
