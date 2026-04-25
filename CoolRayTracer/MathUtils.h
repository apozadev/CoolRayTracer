#pragma once

#include "Vec3.h"
#include "Vec2.h"

constexpr float fPI = 3.14159265359f;
constexpr float fPI_2 = fPI / 2.0f;
constexpr float fPI_4 = fPI / 4.0f;

template <typename T>
inline T clamp(T _value, T _min, T _max)
{
  if (_value < _min) return _min;
  if (_value > _max) return _max;
  return _value;
}

template <typename T>
inline T max(T _a, T _b)
{
  return (_a > _b) ? _a : _b;
}

template <typename T>
inline T min(T _a, T _b)
{
  return (_a < _b) ? _a : _b;
}

inline float DegreesToRadians(float _fDegrees)
{
  return _fDegrees * (fPI / 180.0f);
}

inline vec2 SampleDisk(float _fRandom1, float _fRandom2)
{
  float fOffsetX = 2.0f * _fRandom1 - 1.0f;
  float fOffsetY = 2.0f * _fRandom2 - 1.0f;

  if (fOffsetX == 0.0f && fOffsetY == 0.0f)
    return vec2(0, 0);

  float fTheta, fRadius;

  if (fabs(fOffsetX) > fabs(fOffsetY))
  {
    fRadius = fOffsetX;
    fTheta = fPI_4 * (fOffsetY / fOffsetX);
  }
  else
  {
    fRadius = fOffsetY;
    fTheta = fPI_2 - fPI_4 * (fOffsetX / fOffsetY);
  }

  return vec2(cosf(fTheta) * fRadius, sinf(fTheta) * fRadius);
}

inline vec3 SampleHemisphereCosine(float _fRandom1, float _fRandom2)
{
  vec2 vDiskSample = SampleDisk(_fRandom1, _fRandom2);
  float fZ = sqrt(max(0.0f, 1.0f - vDiskSample.x() * vDiskSample.x() - vDiskSample.y() * vDiskSample.y()));
  return vec3(vDiskSample.x(), vDiskSample.y(), fZ);
}

inline vec3 SampleHemisphere(float _fRandom1, float _fRandom2)
{
  float fPhi = 2.0f * fPI * _fRandom1;
  float fCosTheta = _fRandom2;
  float fSinTheta = sqrt(1.0f - fCosTheta * fCosTheta);
  return vec3(cosf(fPhi) * fSinTheta, sinf(fPhi) * fSinTheta, fCosTheta);
}

inline void TBN(vec3& vT_, vec3& vB_, vec3 _vN)
{
  if (_vN.z() < -0.9999999f)
  {
    vT_ = { 0, -1, 0 };
    vB_ = { -1, 0, 0 };
    return;
  }

  float a = 1.0f / (1.0f + _vN.z());
  float b = -_vN.x() * _vN.y() * a;

  vT_ = { 1.0f - _vN.x() * _vN.x() * a, b, -_vN.x() };
  vB_ = { b, 1.0f - _vN.y() * _vN.y() * a, -_vN.y() };
}

inline vec3 TangentToWorld(vec3 _vDir, vec3 _vNormal)
{
  vec3 vT, vB;
  TBN(vT, vB, _vNormal);
  vec3 vWorldDir = _vDir.x() * vT + _vDir.y() * vB + _vDir.z() * _vNormal;
  return vWorldDir;
}

float Random()
{
  return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}