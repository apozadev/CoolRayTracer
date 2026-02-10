#pragma once

#include "vec3.h"

class ray
{
public:
  ray() {}

  ray(const point3& _vOrigin, const vec3& _vDirection) : vOrigin(_vOrigin), vDir(_vDirection) {}

  point3 at(double t) const
  {
    return vOrigin + t * vDir;
  }

  point3 vOrigin;
  vec3 vDir;
};
