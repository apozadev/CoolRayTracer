#include "GameIncludes.h"

#include "Vec3.h"
#include "Vec2.h"
#include "Ray.h"
#include "MathUtils.h"

#include <math.h>
#include <cmath>
#include <memory>
#include <vector>
#include <cfloat>

// The data layout from "Ray Tracing in One Weekend": Hittable and Material base classes with
// virtual calls, every object and material in its own make_shared allocation, and a HitRecord
// that carries a shared_ptr. Scene and math match CoolRayTracer, so both render the same image.

using color = vec3;

const float g_fAirRefractionIndex = 1.0f;

// Only used to pick each sphere's material when building the scene
enum MaterialType
{
  MaterialType_Lambertian,
  MaterialType_Metal,
  MaterialType_Dielectric
};

class Material;

// Filled in for every hit closer than the closest so far, not just for the final one
struct HitRecord
{
  point3 vPoint;
  vec3 vNormal; // Outward. Unlike the tutorial, not flipped to face the ray, so bounces match CoolRayTracer
  std::shared_ptr<Material> pMaterial;
  float fT;
};

class Hittable
{
public:
  virtual ~Hittable() = default;

  virtual bool Hit(const ray& _oRay, float _fTMax, HitRecord& oRecord_) const = 0;
};

class Sphere : public Hittable
{
public:
  Sphere(const point3& _vCenter, float _fRadius, std::shared_ptr<Material> _pMaterial)
    : vCenter(_vCenter), fRadiusSqr(_fRadius * _fRadius), pMaterial(_pMaterial) {}

  // Ray directions are normalized (a == 1), so the quadratic uses the half-b form:
  // h = dot(oc, dir), disc = h^2 - c, t = -h -+ sqrt(disc).
  bool Hit(const ray& _oRay, float _fTMax, HitRecord& oRecord_) const override
  {
    const vec3 vSphereToRay = _oRay.vOrigin - vCenter;
    const float h = Dot(vSphereToRay, _oRay.vDir);
    const float c = Dot(vSphereToRay, vSphereToRay) - fRadiusSqr;

    const float discriminant = (h * h) - c;

    // Origin outside the sphere and pointing away from it: both roots are behind the ray.
    // Tested after the discriminant, which rejects most spheres with a well-predicted branch.
    if (discriminant > 0 && !(c > 0.f && h > 0.f))
    {
      const float sqrtDisc = sqrtf(discriminant);
      const float t0 = -h - sqrtDisc;
      const float t1 = -h + sqrtDisc;

      const float fT = (t0 > 0.f) ? t0 : ((t1 > 0.f) ? t1 : -1.f);

      if (fT > 0.001f && fT < _fTMax)
      {
        oRecord_.fT = fT;
        oRecord_.vPoint = _oRay.at(fT);
        oRecord_.vNormal = Normalize(oRecord_.vPoint - vCenter);
        oRecord_.pMaterial = pMaterial;
        return true;
      }
    }

    return false;
  }

private:
  point3 vCenter;
  float fRadiusSqr;
  std::shared_ptr<Material> pMaterial;
};

class Plane : public Hittable
{
public:
  Plane(const vec3& _vNormal, float _fPoint, std::shared_ptr<Material> _pMaterial)
    : vNormal(_vNormal), fPoint(_fPoint), pMaterial(_pMaterial) {}

  bool Hit(const ray& _oRay, float _fTMax, HitRecord& oRecord_) const override
  {
    const float fDenom = Dot(vNormal, _oRay.vDir);
    if (fabs(fDenom) > 0.0001f)
    {
      const float fT = (fPoint - Dot(vNormal, _oRay.vOrigin)) / fDenom;
      if (fT >= 0 && fT < _fTMax)
      {
        oRecord_.fT = fT;
        oRecord_.vPoint = _oRay.at(fT);
        oRecord_.vNormal = vNormal;
        oRecord_.pMaterial = pMaterial;
        return true;
      }
    }
    return false;
  }

private:
  vec3 vNormal;
  float fPoint;
  std::shared_ptr<Material> pMaterial;
};

class HittableList : public Hittable
{
public:
  void Add(std::shared_ptr<Hittable> _pObject)
  {
    vObjects.push_back(_pObject);
  }

  bool Hit(const ray& _oRay, float _fTMax, HitRecord& oRecord_) const override
  {
    HitRecord oTempRecord;
    bool bHitAnything = false;
    float fClosestSoFar = _fTMax;

    for (const std::shared_ptr<Hittable>& pObject : vObjects)
    {
      if (pObject->Hit(_oRay, fClosestSoFar, oTempRecord))
      {
        bHitAnything = true;
        fClosestSoFar = oTempRecord.fT;
        oRecord_ = oTempRecord;
      }
    }

    return bHitAnything;
  }

private:
  std::vector<std::shared_ptr<Hittable>> vObjects;
};

vec3 Reflect(const vec3& _voutRay, const vec3& _vNormal)
{
  return _voutRay - (2 * Dot(_voutRay, _vNormal) * _vNormal);
}

vec3 Refract(const vec3& _vOutRay, vec3 _vNormal, float _fEta)
{
  float fCosI = Dot(_vOutRay, _vNormal);

  // If ray is inside the medium, flip the normal
  if (fCosI > 0.0f)
  {
    _vNormal = -_vNormal;
    fCosI = -fCosI;
  }
  // Now fCosI is guaranteed <= 0 (ray going against normal)
  fCosI = -fCosI; // make it positive for the formula

  float fK = 1.0f - _fEta * _fEta * (1.0f - fCosI * fCosI);

  if (fK < 0.0f)
  {
    return vec3(0.f, 0.f, 0.f); // TIR
  }

  return Normalize(_fEta * _vOutRay + (_fEta * fCosI - sqrtf(fK)) * _vNormal);
}

// Starts the bounced ray slightly off the surface, on the side it leaves through, so it doesn't hit the same surface again
ray ScatteredRay(const HitRecord& _oRecord, const vec3& _vDir)
{
  const vec3 vBias = Dot(_vDir, _oRecord.vNormal) > 0.0f
    ? _oRecord.vNormal * 0.001f
    : -_oRecord.vNormal * 0.001f;
  return ray(_oRecord.vPoint + vBias, _vDir);
}

class Material
{
public:
  virtual ~Material() = default;

  virtual bool Scatter(const ray& _oRayIn, const HitRecord& _oRecord, color& vAttenuation_, ray& oScattered_) const = 0;
};

class Lambertian : public Material
{
public:
  explicit Lambertian(const color& _vAlbedo) : vAlbedo(_vAlbedo) {}

  bool Scatter(const ray& /*_oRayIn*/, const HitRecord& _oRecord, color& vAttenuation_, ray& oScattered_) const override
  {
    const vec3 vDir = TangentToWorld(Normalize(SampleHemisphereCosine(Random(), Random())), _oRecord.vNormal);
    oScattered_ = ScatteredRay(_oRecord, vDir);
    vAttenuation_ = vAlbedo;
    return true;
  }

private:
  color vAlbedo;
};

class Metal : public Material
{
public:
  explicit Metal(const color& _vAlbedo) : vAlbedo(_vAlbedo) {}

  bool Scatter(const ray& _oRayIn, const HitRecord& _oRecord, color& vAttenuation_, ray& oScattered_) const override
  {
    oScattered_ = ScatteredRay(_oRecord, Reflect(_oRayIn.vDir, _oRecord.vNormal));
    vAttenuation_ = vAlbedo;
    return true;
  }

private:
  color vAlbedo;
};

class Dielectric : public Material
{
public:
  Dielectric(const color& _vAlbedo, float _fRefractionIndex) : vAlbedo(_vAlbedo), fRefractionIndex(_fRefractionIndex) {}

  bool Scatter(const ray& _oRayIn, const HitRecord& _oRecord, color& vAttenuation_, ray& oScattered_) const override
  {
    const bool bFromOutside = Dot(_oRayIn.vDir, _oRecord.vNormal) < 0.0f;
    const float fRelativeRefractionIndex = bFromOutside
      ? (g_fAirRefractionIndex / fRefractionIndex)
      : (fRefractionIndex / g_fAirRefractionIndex);

    vec3 vDir = Refract(_oRayIn.vDir, _oRecord.vNormal, fRelativeRefractionIndex);
    if (vDir.LengthSqr() == 0.f) // Total internal reflection, fallback to reflection
    {
      vDir = Reflect(_oRayIn.vDir, _oRecord.vNormal);
    }

    oScattered_ = ScatteredRay(_oRecord, vDir);
    vAttenuation_ = vAlbedo;
    return true;
  }

private:
  color vAlbedo;
  float fRefractionIndex;
};

HittableList g_oWorld;

float LinearToGamma(float _fValue)
{
  return powf(_fValue, 1.0f / 2.2f);
}

void InitGame(int _iSphereCount)
{
  const float fRadiusMax = 3.0f;
  const float fRadiusMin = 0.2f;

  const float fPosXMin = -10.f;
  const float fPosXMax = 10.f;
  const float fPosZMin = -20.f;
  const float fPosZMax = 0.f;

  // Bigger scenes cover a bigger area at the same density. The area grows away from the
  // camera: its near edge stays at z = 0.
  const float fAreaScale = sqrtf(static_cast<float>(_iSphereCount) / g_iDefaultSphereCount);

  // Random() is called in the same order as in CoolRayTracer, so both build the same scene
  for (int i = 0; i < _iSphereCount; i++)
  {
    const float fRadius = Random() * (fRadiusMax - fRadiusMin) + fRadiusMin;

    const float fPosX = (Random() - 0.5f) * 2.0f * (fPosXMax - fPosXMin) * fAreaScale + fPosXMin;
    const float fPosZ = (Random() - 0.5f) * 2.0f * (fPosZMax - fPosZMin) * fAreaScale + fPosZMin * fAreaScale;

    const MaterialType eMatType = static_cast<MaterialType>(Random() * 2.99f);

    const color vAlbedo = vec3(Random(), Random(), Random());

    std::shared_ptr<Material> pMaterial;
    switch (eMatType)
    {
    case MaterialType_Lambertian:
      pMaterial = std::make_shared<Lambertian>(vAlbedo);
      break;
    case MaterialType_Metal:
      pMaterial = std::make_shared<Metal>(vAlbedo);
      break;
    case MaterialType_Dielectric:
      pMaterial = std::make_shared<Dielectric>(vAlbedo, 1.5f);
      break;
    }

    g_oWorld.Add(std::make_shared<Sphere>(point3(fPosX, -1 + fRadius, fPosZ), fRadius, pMaterial));
  }

  g_oWorld.Add(std::make_shared<Plane>(vec3(0, 1, 0), -1.0f, std::make_shared<Lambertian>(color(0.5f, 0.5f, 0.5f))));
}

void UpdateScreenBufferPartial(GameScreenBuffer* Buffer, int _iStartX, int _iStartY, int _iEndX, int _iEndY)
{
  //Camera

  const float fFocalLength = 1.0f;
  const float fViewportHeight = 1.0f;
  const float fViewportWidth = fViewportHeight * (float(Buffer->iWidth) / Buffer->iHeight);
  //float fAspectRatio = fViewportWidth / fViewportHeight;
  const vec3 vCameraCenter = vec3(0, 3, 8);

  const vec3 vCameraForward = Normalize(vec3(0, -0.25, -1));
  const vec3 vCameraRight = Normalize(Cross(vec3(0, 1, 0), vCameraForward));
  const vec3 vCameraUp = Cross(vCameraForward, vCameraRight);

  const vec3 vViewportX = vCameraRight * fViewportWidth;
  const vec3 vViewportY = -vCameraUp * fViewportHeight;

  const vec3 vPixelDeltaX = vViewportX / float(Buffer->iWidth);
  const vec3 vPixelDeltaY = vViewportY / float(Buffer->iHeight);

  const vec3 vViewportUpperLeft = vCameraCenter - (vViewportX / 2) - (vViewportY / 2) + vCameraForward * fFocalLength;
  const vec3 vStartPixel = vViewportUpperLeft + (vPixelDeltaX / 2) + (vPixelDeltaY / 2);

  const int iMAX_BOUNCES = 4;

  const int iSAMPLE_COUNT = 8;

  const vec2 aOffsets[iSAMPLE_COUNT] = {
      { 1.f / 1,  1.f / -3 },
      { 1.f / -1,  1.f / 3 },
      { 1.f / 5,  1.f / 1 },
      { 1.f / -3,  1.f / -5 },
      { 1.f / -5,  1.f / 5 },
      { 1.f / -7,  1.f / -1 },
      { 1.f / 3,  1.f / 7 },
      { 1.f / 7,  1.f / -7 }
  };

  const int uPitch = Buffer->iWidth * g_uBytesPerPixel;

  const uint8_t* pRow = ((uint8_t*)Buffer->pData) + uPitch * _iStartY;

  for (int y = _iStartY; y < _iEndY; y++)
  {
    uint8_t* pPixel = ((uint8_t*)pRow) + _iStartX * g_uBytesPerPixel;
    for (int x = _iStartX; x < _iEndX; x++)
    {
      color vPixelColor = { 0.f, 0.f, 0.f };

      for (const vec2& vOffset : aOffsets)
      {
        const vec3 vPixelCenter = vStartPixel + (x + vOffset.x()) * vPixelDeltaX + (y + vOffset.y()) * vPixelDeltaY;
        const vec3 vRayDirection = Normalize(vPixelCenter - vCameraCenter);

        ray oRay(vCameraCenter, vRayDirection);

        color vRayColor = { 1.f, 1.f, 1.f };

        int iBounces = 0;

        while(true)
        {
          HitRecord oRecord;

          if (!g_oWorld.Hit(oRay, FLT_MAX, oRecord))
          {
            // Classic blue-white gradient
            float t = 0.5f * (oRay.vDir.y() + 1.0f);
            vRayColor = vRayColor * ((1.0f - t) * vec3(1, 1, 1) + t * vec3(0.5f, 0.7f, 1.0f));
            break;
          }
          else if (iBounces > iMAX_BOUNCES)
          {
            // No light source found, does not contribute
            vRayColor = vec3(0, 0, 0);
            break;
          }

          color vAttenuation;
          ray oScattered;
          if (!oRecord.pMaterial->Scatter(oRay, oRecord, vAttenuation, oScattered))
          {
            // Absorbed
            vRayColor = vec3(0, 0, 0);
            break;
          }

          oRay = oScattered;
          vRayColor = vRayColor * vAttenuation;

          iBounces++;
        }

        vPixelColor += vRayColor;
      }

      vPixelColor /= iSAMPLE_COUNT;

      *pPixel++ = static_cast<uint8_t>(LinearToGamma(vPixelColor.b()) * 255.f);

      *pPixel++ = static_cast<uint8_t>(LinearToGamma(vPixelColor.g()) * 255.f);

      *pPixel++ = static_cast<uint8_t>(LinearToGamma(vPixelColor.r()) * 255.f);

      *pPixel++ = 0u;
    }
    pRow += uPitch;
  }
}

void UpdateGameSoundBuffer(
  uint32_t& /*uCurrSampleIdx*/,
  void* /*pRegion1*/,
  size_t /*uRegion1Size*/,
  void* /*pRegion2*/,
  size_t /*uRegion2Size*/,
  size_t /*uBytesPerSample*/)
{
}
