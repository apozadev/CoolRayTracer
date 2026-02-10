#pragma once

#include "Vec3.h"
#include "Vec2.h"

constexpr double fPI = 3.14159265359;
constexpr double fPI_2 = fPI / 2.0;
constexpr double fPI_4 = fPI / 4.0;

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

inline double DegreesToRadians(double _fDegrees)
{
  return _fDegrees * (fPI / 180.0);
}

inline vec2 SampleDisk(double _fRandom1, double _fRandom2)
{
  double fOffsetX = 2.0 * _fRandom1 - 1.0;
  double fOffsetY = 2.0 * _fRandom2 - 1.0;

  if (fOffsetX == 0.0 && fOffsetY == 0.0)
    return vec2(0, 0);

  double fTheta, fRadius;

  if (abs(fOffsetX) > abs(fOffsetY))
  {
    fRadius = fOffsetX;
    fTheta = fPI_4 * (fOffsetY / fOffsetX);
  }
  else
  {
    fRadius = fOffsetY;
    fTheta = fPI_2 - fPI_4 * (fOffsetX / fOffsetY);
  }

  return vec2(cos(fTheta) * fRadius, sin(fTheta) * fRadius);
}

inline vec3 SampleHemisphereCosine(double _fRandom1, double _fRandom2)
{
  vec2 vDiskSample = SampleDisk(_fRandom1, _fRandom2);
  double fZ = sqrt(max(0.0, 1.0 - vDiskSample.x * vDiskSample.x - vDiskSample.y * vDiskSample.y));
  return vec3(vDiskSample.x, vDiskSample.y, fZ);
}

inline vec3 SampleHemisphere(double _fRandom1, double _fRandom2)
{
  double fPhi = 2.0 * fPI * _fRandom1;
  double fCosTheta = _fRandom2;
  double fSinTheta = sqrt(1.0 - fCosTheta * fCosTheta);
  return vec3(cos(fPhi) * fSinTheta, sin(fPhi) * fSinTheta, fCosTheta);
}

inline void TBN(vec3& vT_, vec3& vB_, vec3 _vN)
{
  if (_vN.z < -0.9999999)
  {
    vT_ = { 0, -1, 0 };
    vB_ = { -1, 0, 0 };
    return;
  }

  float a = 1.0f / (1.0f + _vN.z);
  float b = -_vN.x * _vN.y * a;

  vT_ = { 1.0 - _vN.x * _vN.x * a, b, -_vN.x };
  vB_ = { b, 1.0 - _vN.y * _vN.y * a, -_vN.y };
}

inline vec3 TangentToWorld(vec3 _vDir, vec3 _vNormal)
{
  vec3 vT, vB;
  TBN(vT, vB, _vNormal);
  vec3 vWorldDir = _vDir.x * vT + _vDir.y * vB + _vDir.z * _vNormal;
  return vWorldDir;
}

double Random()
{
  return static_cast<double>(rand()) / static_cast<double>(RAND_MAX);
}