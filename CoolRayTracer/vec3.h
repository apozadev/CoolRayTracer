#pragma once

#include <cmath>
#include <iostream>

class vec3
{
public:

  union
  {
    struct { double x, y, z; };
    struct { double r, g, b; };
    double e[3];
  };

  vec3() : e{ 0,0,0 } {}
  vec3(double e0, double e1, double e2) : e{ e0, e1, e2 } {}

  vec3 operator-() const { return vec3(-e[0], -e[1], -e[2]); }
  double operator[](int i) const { return e[i]; }
  double& operator[](int i) { return e[i]; }

  vec3& operator+=(const vec3& v)
  {
    e[0] += v.e[0];
    e[1] += v.e[1];
    e[2] += v.e[2];
    return *this;
  }

  vec3& operator*=(double t)
  {
    e[0] *= t;
    e[1] *= t;
    e[2] *= t;
    return *this;
  }

  vec3& operator/=(double t)
  {
    return *this *= 1 / t;
  }

  double Length() const
  {
    return std::sqrt(LengthSqr());
  }

  double LengthSqr() const
  {
    return e[0] * e[0] + e[1] * e[1] + e[2] * e[2];
  }

  void Normalize()
  {
    double len = Length();
    if (len > 0)
    {
      double invLen = 1 / len;
      e[0] *= invLen;
      e[1] *= invLen;
      e[2] *= invLen;
    }
  }
};

// point3 is just an alias for vec3, but useful for geometric clarity in the code.
using point3 = vec3;


// Vector Utility Functions

inline std::ostream& operator<<(std::ostream& out, const vec3& v)
{
  return out << v.e[0] << ' ' << v.e[1] << ' ' << v.e[2];
}

inline vec3 operator+(const vec3& u, const vec3& v)
{
  return vec3(u.e[0] + v.e[0], u.e[1] + v.e[1], u.e[2] + v.e[2]);
}

inline vec3 operator-(const vec3& u, const vec3& v)
{
  return vec3(u.e[0] - v.e[0], u.e[1] - v.e[1], u.e[2] - v.e[2]);
}

inline vec3 operator*(const vec3& u, const vec3& v)
{
  return vec3(u.e[0] * v.e[0], u.e[1] * v.e[1], u.e[2] * v.e[2]);
}

inline vec3 operator*(double t, const vec3& v)
{
  return vec3(t * v.e[0], t * v.e[1], t * v.e[2]);
}

inline vec3 operator*(const vec3& v, double t)
{
  return t * v;
}

inline vec3 operator/(const vec3& v, double t)
{
  return (1 / t) * v;
}

inline double Dot(const vec3& u, const vec3& v)
{
  return u.e[0] * v.e[0]
    + u.e[1] * v.e[1]
    + u.e[2] * v.e[2];
}

inline vec3 Cross(const vec3& u, const vec3& v)
{
  return vec3(u.e[1] * v.e[2] - u.e[2] * v.e[1],
    u.e[2] * v.e[0] - u.e[0] * v.e[2],
    u.e[0] * v.e[1] - u.e[1] * v.e[0]);
}

inline vec3 Normalize(const vec3& v)
{
  return v / v.Length();
}