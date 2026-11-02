#include "GameIncludes.h"

#include "Vec3.h"
#include "Vec2.h"
#include "Ray.h"
#include "MathUtils.h"
#include "SimdVec4.h"

#include <math.h>
#include <cmath>
#include <vector>
#include <cfloat>

using color = vec3;

const float g_fAirRefractionIndex = 1.0f;

enum MaterialType
{
  MaterialType_Lambertian,
  MaterialType_Metal,
  MaterialType_Dielectric
};

enum HittableType
{
  HittableType_Sphere,
  HittableType_Plane
};

struct Material
{
  MaterialType eType;
  color vAlbedo;
  union{
    struct
    {
      float fRoughness;
    } oMetal;
    struct
    {
      float fRefractionIndex;
    } oDielectric;
  };
};

struct Sphere
{
  SimdVec4 vCenter;
  float fRadiusSqr;
};

struct Plane
{
  vec3 vNormal;
  float fPoint;
};

struct Hittable
{
  HittableType eType;
  union
  {
    Sphere oSphere;
    Plane oPlane;
  };
};

struct Scene
{
  std::vector<Hittable> vHittables;
  std::vector<Material> vMaterials;
};

Scene g_oScene = {};

// Ray directions are normalized (a == 1), so the quadratic uses the half-b form:
// h = dot(oc, dir), disc = h^2 - c, t = -h -+ sqrt(disc).
// Only computes t; the normal is computed once for the closest hit (see HitNormal).
bool HitSphere(const SimdRay& _oRay, const Sphere& _oSphere, float& fT_)
{
  const SimdVec4 vSphereToRay = _oRay.vOrigin - _oSphere.vCenter;
  const float h = Dot(vSphereToRay, _oRay.vDir);
  const float c = Dot(vSphereToRay, vSphereToRay) - _oSphere.fRadiusSqr;

  const float discriminant = (h * h) - c;

  // Origin outside the sphere and pointing away from it: both roots are behind the ray.
  // Tested after the discriminant, which rejects most spheres with a well-predicted branch.
  if (discriminant > 0 && !(c > 0.f && h > 0.f))
  {
    const float sqrtDisc = sqrtf(discriminant);
    const float t0 = -h - sqrtDisc;
    const float t1 = -h + sqrtDisc;

    const float fT = (t0 > 0.f) ? t0 : ((t1 > 0.f) ? t1 : -1.f);

    if (fT > 0.001f)
    {
      fT_ = fT;
      return true;
    }
  }

  return false;
}

bool HitPlane(const ray& _oRay, const Plane& _oPlane, float& fT_)
{
  const float fDenom = Dot(_oPlane.vNormal, _oRay.vDir);
  if (fabs(fDenom) > 0.0001f)
  {
    const float fT = (_oPlane.fPoint - Dot(_oPlane.vNormal, _oRay.vOrigin)) / fDenom;
    if (fT >= 0)
    {
      fT_ = fT;
      return true;
    }
  }
  return false;
}

bool HitHittable(const ray& _oRay, const SimdRay& _oSimdRay, const Hittable& _oHittable, float& fT_)
{
  switch (_oHittable.eType)
  {
  case HittableType_Sphere:
  {
    return HitSphere(_oSimdRay, _oHittable.oSphere, fT_);
  } break;
  case HittableType_Plane:
  {
    return HitPlane(_oRay, _oHittable.oPlane, fT_);
  } break;
  }

  return false;
}

vec3 HitNormal(const SimdRay& _oSimdRay, float _fT, const Hittable& _oHittable)
{
  switch (_oHittable.eType)
  {
  case HittableType_Sphere:
    return Normalize((_oSimdRay.vOrigin + (_fT * _oSimdRay.vDir)) - _oHittable.oSphere.vCenter).ToVec3();
  case HittableType_Plane:
    return _oHittable.oPlane.vNormal;
  }

  return vec3(0.f, 1.f, 0.f);
}

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

void AddHittable(Hittable&& _oHittable, Material&& _oMaterial, Scene& oScene_)
{
  oScene_.vHittables.emplace_back(_oHittable);
  oScene_.vMaterials.emplace_back(_oMaterial);
}

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

  for (int i = 0; i < _iSphereCount; i++)
  {    
    const float fRadius = Random() * (fRadiusMax - fRadiusMin) + fRadiusMin;

    const float fPosX = (Random() - 0.5f) * 2.0f * (fPosXMax - fPosXMin) * fAreaScale + fPosXMin;
    const float fPosZ = (Random() - 0.5f) * 2.0f * (fPosZMax - fPosZMin) * fAreaScale + fPosZMin * fAreaScale;    

    const MaterialType eMatType = static_cast<MaterialType>(Random() * 2.99f);

    Hittable oSphere = {};
    oSphere.eType = HittableType_Sphere;
    oSphere.oSphere.vCenter = SimdVec4(fPosX, -1 + fRadius, fPosZ);
    oSphere.oSphere.fRadiusSqr = fRadius * fRadius;

    Material oMaterial = {};
    oMaterial.eType = eMatType;
    switch (oMaterial.eType)
    {
    case MaterialType_Lambertian:      
      break;
    case MaterialType_Metal:
      oMaterial.oMetal.fRoughness = 0.f;
      break;
    case MaterialType_Dielectric:
      oMaterial.oDielectric.fRefractionIndex = 1.5f;
      break;
    } 

    oMaterial.vAlbedo = vec3(Random(), Random(), Random());    

    g_oScene.vHittables.push_back(oSphere);
    g_oScene.vMaterials.push_back(oMaterial);
  }

  {
    Hittable oPlane = {};
    oPlane.eType = HittableType_Plane;
    oPlane.oPlane.vNormal = vec3(0, 1, 0);
    oPlane.oPlane.fPoint = -1.0f;

    Material oMaterial = {};
    oMaterial.eType = MaterialType_Lambertian;
    oMaterial.vAlbedo = vec3(0.5, 0.5, 0.5);

    g_oScene.vHittables.push_back(oPlane);
    g_oScene.vMaterials.push_back(oMaterial);
  }
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
          int iHittableIdx = -1;
          float fClosestT = FLT_MAX;

          // Convert once per bounce, not once per sphere test
          const SimdRay oSimdRay = { SimdVec4(oRay.vOrigin), SimdVec4(oRay.vDir) };

          for (int i = 0; i < g_oScene.vHittables.size(); i++)
          {
            float fCandidateT;
            if (HitHittable(oRay, oSimdRay, g_oScene.vHittables[i], fCandidateT) && fCandidateT < fClosestT)
            {
              fClosestT = fCandidateT;
              iHittableIdx = i;
            }
          }

          if (iHittableIdx < 0)
          {
            // Classic blue-white gradient
            float t = 0.5f * (oRay.vDir.y() + 1.0f);
            vRayColor = vRayColor * ((1.0f - t) * vec3(1, 1, 1) + t * vec3(0.5f, 0.7f, 1.0f));
            //vec3 vSkyGradient = oRay.vDir * 0.5 + 0.5;
            //vRayColor = vRayColor * vSkyGradient;
            break;
          }
          else if (iBounces > iMAX_BOUNCES)
          {
            // No light source found, does not contribute
            vRayColor = vec3(0, 0, 0);
            break;
          }

          const Material& oMaterial = g_oScene.vMaterials[iHittableIdx];
          const vec3 vHitNormal = HitNormal(oSimdRay, fClosestT, g_oScene.vHittables[iHittableIdx]);

          vec3 vInRay = {};
          switch (oMaterial.eType)
          {
          case MaterialType_Lambertian:
            vInRay = TangentToWorld(Normalize(SampleHemisphereCosine(Random(), Random())), vHitNormal);
            break;
          case MaterialType_Metal:
            vInRay = Reflect(oRay.vDir, vHitNormal);
            break;
          case MaterialType_Dielectric:
          {
            const bool bFromOutside = Dot(oRay.vDir, vHitNormal) < 0.0f;
            const float fRelativeRefractionIndex = bFromOutside
              ? (g_fAirRefractionIndex / oMaterial.oDielectric.fRefractionIndex)
              : (oMaterial.oDielectric.fRefractionIndex / g_fAirRefractionIndex);

            vInRay = Refract(oRay.vDir, vHitNormal, fRelativeRefractionIndex);
            if(vInRay.LengthSqr() == 0.f) // Total internal reflection, fallback to reflection
            {
              vInRay = Reflect(oRay.vDir, vHitNormal);
            }
            break;
          }
          default:
            break;
          }

          const vec3 vBias = Dot(vInRay, vHitNormal) > 0.0f
            ? vHitNormal * 0.001f
            : -vHitNormal * 0.001f;
          oRay = ray(oRay.vOrigin + (fClosestT * oRay.vDir) + vBias, vInRay);
          vRayColor = vRayColor * oMaterial.vAlbedo;                    

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
