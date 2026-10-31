#pragma once

#include <cmath>
#include <iostream>

class vec2
{
public:

  float e[2];  

  vec2() : e{ 0,0 } {}
  vec2(float e0, float e1) : e{ e0, e1 } {}

  vec2 operator-() const { return vec2(-e[0], -e[1]); }
  float operator[](int i) const { return e[i]; }
  float& operator[](int i) { return e[i]; }

  vec2& operator+=(const vec2& v)
  {
    e[0] += v.e[0];
    e[1] += v.e[1];
    return *this;
  }

  vec2& operator*=(float t)
  {
    e[0] *= t;
    e[1] *= t;
    return *this;
  }

  vec2& operator/=(float t)
  {
    return *this *= 1 / t;
  }

  float Length() const
  {
    return std::sqrt(LengthSqr());
  }

  float LengthSqr() const
  {
    return e[0] * e[0] + e[1] * e[1];
  }

  void Normalize()
  {
    float len = Length();
    if (len > 0)
    {
      float invLen = 1 / len;
      e[0] *= invLen;
      e[1] *= invLen;
    }
  }

  float x() const { return e[0]; }
  float y() const { return e[1]; }  

  float& x() { return e[0]; }
  float& y() { return e[1]; }  

  float r() const { return e[0]; }
  float g() const { return e[1]; }  

  float& r() { return e[0]; }
  float& g() { return e[1]; }  
};

// point3 is just an alias for vec2, but useful for geometric clarity in the code.
using point2 = vec2;


// Vector Utility Functions

inline std::ostream& operator<<(std::ostream& out, const vec2& v)
{
  return out << v.e[0] << ' ' << v.e[1] << ' ' << v.e[2];
}

inline vec2 operator+(const vec2& u, const vec2& v)
{
  return vec2(u.e[0] + v.e[0], u.e[1] + v.e[1]);
}

inline vec2 operator-(const vec2& u, const vec2& v)
{
  return vec2(u.e[0] - v.e[0], u.e[1] - v.e[1]);
}

inline vec2 operator*(const vec2& u, const vec2& v)
{
  return vec2(u.e[0] * v.e[0], u.e[1] * v.e[1]);
}

inline vec2 operator*(float t, const vec2& v)
{
  return vec2(t * v.e[0], t * v.e[1]);
}

inline vec2 operator*(const vec2& v, float t)
{
  return t * v;
}

inline vec2 operator/(const vec2& v, float t)
{
  return (1 / t) * v;
}

inline float Dot(const vec2& u, const vec2& v)
{
  return u.e[0] * v.e[0]
    + u.e[1] * v.e[1];
}

inline vec2 Cross(const vec2& u, const vec2& v)
{
  return vec2(u.e[0] * v.e[1] - u.e[1] * v.e[0], 0.0);
}

inline vec2 Normalize(const vec2& v)
{
  return v / v.Length();
}