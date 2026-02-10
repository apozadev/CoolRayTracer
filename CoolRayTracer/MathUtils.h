#pragma once

#include "Vec3.h"

constexpr double fPI = 3.14159265359f;

inline double DegreesToRadians(double _fDegrees)
{
  return _fDegrees * (fPI / 180.0f);
}

inline vec3 HemisphereSample(double _fRandom1, double _fRandom2)
{
  double fSinTheta = _fRandom2 * _fRandom2;
  double fCosTheta = sqrt(1.0f - _fRandom2 * _fRandom2);
  double fPhi = DegreesToRadians(2.0f * fPI * _fRandom2);
  vec3 vSampleDirection = vec3(cos(fPhi) * fSinTheta, cos(fPhi) * fSinTheta, fCosTheta);

  return vSampleDirection;
}

inline void TBN(vec3& vT_, vec3& vB_, vec3 _vN)
{
  if (_vN.z < -0.9999999f)
  {
    vT_ = { 0, -1, 0 };
    vB_ = { -1, 0, 0 };
    return;
  }

  float a = 1.0f / (1.0f + _vN.z);
  float b = -_vN.x * _vN.y * a;

  vT_ = { 1.0f - _vN.x * _vN.x * a, b, -_vN.x };
  vB_ = { b, 1.0f - _vN.y * _vN.y * a, -_vN.y };
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