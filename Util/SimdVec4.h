#pragma once

#include <xmmintrin.h>
#include <emmintrin.h>

#include "vec3.h"

// 4-wide SSE vector used as a drop-in replacement for vec3 in hot paths.
// Holds (x, y, z, w) in a single __m128. All operations assume w == 0 so that
// horizontal reductions (Dot, Length) only see the xyz components.
class SimdVec4
{
public:

  __m128 m;

  SimdVec4() : m(_mm_setzero_ps()) {}
  explicit SimdVec4(__m128 _m) : m(_m) {}
  SimdVec4(float _fX, float _fY, float _fZ, float _fW = 0.f) : m(_mm_setr_ps(_fX, _fY, _fZ, _fW)) {}
  explicit SimdVec4(const vec3& _v) : m(_mm_setr_ps(_v.x(), _v.y(), _v.z(), 0.f)) {}

  static SimdVec4 Splat(float _f) { return SimdVec4(_mm_set1_ps(_f)); }

  float x() const { return _mm_cvtss_f32(m); }
  float y() const { return _mm_cvtss_f32(_mm_shuffle_ps(m, m, _MM_SHUFFLE(1, 1, 1, 1))); }
  float z() const { return _mm_cvtss_f32(_mm_shuffle_ps(m, m, _MM_SHUFFLE(2, 2, 2, 2))); }
  float w() const { return _mm_cvtss_f32(_mm_shuffle_ps(m, m, _MM_SHUFFLE(3, 3, 3, 3))); }

  vec3 ToVec3() const
  {
    alignas(16) float a[4];
    _mm_store_ps(a, m);
    return vec3(a[0], a[1], a[2]);
  }

  SimdVec4 operator-() const { return SimdVec4(_mm_sub_ps(_mm_setzero_ps(), m)); }

  SimdVec4& operator+=(const SimdVec4& _v) { m = _mm_add_ps(m, _v.m); return *this; }
  SimdVec4& operator-=(const SimdVec4& _v) { m = _mm_sub_ps(m, _v.m); return *this; }
  SimdVec4& operator*=(float _t) { m = _mm_mul_ps(m, _mm_set1_ps(_t)); return *this; }

  float LengthSqr() const;
  float Length() const;
};

// Sums lanes x + y + z (+ w) into lane 0. Same summation order as the scalar
// vec3 Dot, (x + y) + z, so results stay bit-identical when w == 0.
inline __m128 HorizontalSum(__m128 _v)
{
  __m128 vShuf = _mm_shuffle_ps(_v, _v, _MM_SHUFFLE(2, 3, 0, 1)); // (y, x, w, z)
  __m128 vSums = _mm_add_ps(_v, vShuf);                           // (x+y, x+y, z+w, z+w)
  vShuf = _mm_movehl_ps(vShuf, vSums);                            // (z+w, z+w, ...)
  return _mm_add_ss(vSums, vShuf);                                // (x+y) + (z+w)
}

inline SimdVec4 operator+(const SimdVec4& _u, const SimdVec4& _v) { return SimdVec4(_mm_add_ps(_u.m, _v.m)); }
inline SimdVec4 operator-(const SimdVec4& _u, const SimdVec4& _v) { return SimdVec4(_mm_sub_ps(_u.m, _v.m)); }
inline SimdVec4 operator*(const SimdVec4& _u, const SimdVec4& _v) { return SimdVec4(_mm_mul_ps(_u.m, _v.m)); }
inline SimdVec4 operator*(float _t, const SimdVec4& _v) { return SimdVec4(_mm_mul_ps(_mm_set1_ps(_t), _v.m)); }
inline SimdVec4 operator*(const SimdVec4& _v, float _t) { return _t * _v; }

// Multiply by the reciprocal, matching vec3's operator/ exactly.
inline SimdVec4 operator/(const SimdVec4& _v, float _t) { return (1 / _t) * _v; }

inline float Dot(const SimdVec4& _u, const SimdVec4& _v)
{
  return _mm_cvtss_f32(HorizontalSum(_mm_mul_ps(_u.m, _v.m)));
}

inline float SimdVec4::LengthSqr() const { return Dot(*this, *this); }

inline float SimdVec4::Length() const { return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(LengthSqr()))); }

inline SimdVec4 Normalize(const SimdVec4& _v)
{
  return _v / _v.Length();
}

struct SimdRay
{
  SimdVec4 vOrigin;
  SimdVec4 vDir;
};
