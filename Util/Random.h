#pragma once

#include <stdint.h>

// Per-thread xorshift32 state. Every thread starts from the same seed, so call
// SeedRandom() with a distinct value per thread if they must not share a sequence.
// The platform layer reseeds per render tile, so images don't depend on thread count.
inline thread_local uint32_t t_uRandomState = 0x9E3779B9u;

inline void SeedRandom(uint32_t _uSeed)
{
  t_uRandomState = _uSeed ? _uSeed : 0x9E3779B9u; // xorshift state must be non-zero
}

// Uniform float in [0, 1)
inline float Random()
{
  uint32_t x = t_uRandomState;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  t_uRandomState = x;
  return static_cast<float>(x >> 8) * (1.0f / 16777216.0f);
}
