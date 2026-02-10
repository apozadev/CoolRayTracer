#include "CoolRayTracer.h"

#include "Vec3.h"
#include "Ray.h"
#include "MathUtils.h"

#include <math.h>
#include <cmath>
#include <vector>

using color = vec3;

struct vec2
{
  float x, y;
};

enum HittableType
{
  Sphere,
  Plane
};

struct sphere
{
  vec3 vCenter;
  float fRadius;
};

struct plane
{
  vec3 vNormal;
  float fPoint;
};

struct Hittable
{
  HittableType eType;
  union
  {
    sphere oSphere;
    plane oPlane;
  };
  vec3 vColor;
};

struct HitInfo
{
  float fT;
  vec3 vNormal;
};

std::vector<Hittable> g_vHittables;

bool HitSphere(const ray& _oRay, const sphere& _oSphere, HitInfo& oHitInfo_)
{
  vec3 vSphereToRay = _oRay.vOrigin - _oSphere.vCenter;
  float a = Dot(_oRay.vDir, _oRay.vDir);
  float b = 2.0f * Dot(vSphereToRay, _oRay.vDir);
  float c = Dot(vSphereToRay, vSphereToRay) - (_oSphere.fRadius * _oSphere.fRadius);
  float discriminant = (b * b) - (4 * a * c);
  if (discriminant > 0)
  {
    float sqrtDisc = sqrtf(discriminant);
    float t0 = (-b - sqrtDisc) / (2.0f * a);
    float t1 = (-b + sqrtDisc) / (2.0f * a);

    float fT = (t0 < t1) ? t0 : t1;
    if (fT > 0.f)
    {
      oHitInfo_.fT = fT;
      oHitInfo_.vNormal = Normalize((_oRay.vOrigin + (oHitInfo_.fT * _oRay.vDir)) - _oSphere.vCenter);
      return true;
    }
  }

  return false;
}

bool HitPlane(const ray& _oRay, const plane& _oPlane, HitInfo& oHitInfo_)
{
  float fDenom = Dot(_oPlane.vNormal, _oRay.vDir);
  if (fabs(fDenom) > 0.0001f)
  {
    float fT = (_oPlane.fPoint - Dot(_oPlane.vNormal, _oRay.vOrigin)) / fDenom;
    if (fT >= 0)
    {
      oHitInfo_.fT = fT;
      oHitInfo_.vNormal = _oPlane.vNormal;
      return true;
    }
  }
  return false;
}

bool HitHittable(const ray& _oRay, const Hittable& _oHittable, HitInfo& oHitInfo)
{
  switch (_oHittable.eType)
  {
  case HittableType::Sphere:
  {
    return HitSphere(_oRay, _oHittable.oSphere, oHitInfo);
  } break;
  case HittableType::Plane:
  {
    return HitPlane(_oRay, _oHittable.oPlane, oHitInfo);
  }
  break;
  }
}

void InitGame()
{
  Hittable oSphereHittable = {};
  oSphereHittable.eType = HittableType::Sphere;
  oSphereHittable.oSphere.vCenter = vec3(0, 0, -5);
  oSphereHittable.oSphere.fRadius = 1.0f;
  oSphereHittable.vColor = vec3(0.5, 0, 0);
  g_vHittables.push_back(oSphereHittable);

  /*Hittable oSphereHittable2 = {};
  oSphereHittable2.eType = HittableType::Sphere;
  oSphereHittable2.oSphere.vCenter = vec3(1, 0, -5);
  oSphereHittable2.oSphere.fRadius = 0.8f;
  oSphereHittable.vColor = vec3(0, 0.5, 0);
  g_vHittables.push_back(oSphereHittable2);*/

  Hittable oPlaneHittable = {};
  oPlaneHittable.eType = HittableType::Plane;
  oPlaneHittable.oPlane.vNormal = vec3(0, 1, 0);
  oPlaneHittable.oPlane.fPoint = -1.0f;
  oPlaneHittable.vColor = vec3(0.5, 0.5, 0.5);
  g_vHittables.push_back(oPlaneHittable);
}

void UpdateScreenBufferPartial(GameScreenBuffer* Buffer, int _iStartX, int _iStartY, int _iEndX, int _iEndY)
{
  //Camera

  float fFocalLength = 1.0;
  float fViewportHeight = 1.0;
  float fViewportWidth = fViewportHeight * (double(Buffer->iWidth) / Buffer->iHeight);
  float fAspectRatio = fViewportWidth / fViewportHeight;
  vec3 vCameraCenter = vec3(0, 0, 0);

  vec3 vViewportX = vec3(fViewportWidth, 0, 0);
  vec3 vViewportY = vec3(0, -fViewportHeight, 0);

  vec3 vPixelDeltaX = vViewportX / Buffer->iWidth;
  vec3 vPixelDeltaY = vViewportY / Buffer->iHeight;

  vec3 vViewportUpperLeft = vCameraCenter - (vViewportX / 2) - (vViewportY / 2) - (vec3(0, 0, fFocalLength));
  vec3 vStartPixel = vViewportUpperLeft + (vPixelDeltaX / 2) + (vPixelDeltaY / 2);

  constexpr int iSAMPLE_COUNT = 8;

  vec2 aOffsets[iSAMPLE_COUNT] = {
      { 1.f / 1,  1.f / -3 },
      { 1.f / -1,  1.f / 3 },
      { 1.f / 5,  1.f / 1 },
      { 1.f / -3,  1.f / -5 },
      { 1.f / -5,  1.f / 5 },
      { 1.f / -7,  1.f / -1 },
      { 1.f / 3,  1.f / 7 },
      { 1.f / 7,  1.f / -7 }
  };

  int uPitch = Buffer->iWidth * g_uBytesPerPixel;

  uint8_t* pRow = ((uint8_t*)Buffer->pData) + uPitch * _iStartY;

  for (int y = _iStartY; y < _iEndY; y++)
  {
    uint8_t* pPixel = ((uint8_t*)pRow) + _iStartX * g_uBytesPerPixel;
    for (int x = _iStartX; x < _iEndX; x++)
    {
      color vPixelColor = { 0.f, 0.f, 0.f };

      for (vec2 vOffset : aOffsets)
      {
        vec3 vPixelCenter = vStartPixel + (x + vOffset.x) * vPixelDeltaX + (y + vOffset.y) * vPixelDeltaY;
        vec3 vRayDirection = Normalize(vPixelCenter - vCameraCenter);
        ray oRay(vCameraCenter, vRayDirection);
        HitInfo oHitInfo = {};
        /*
         bool bHit = false;
        color vRayColor = { 1.f, 1.f, 1.f };

        do
        {
          bHit = false;
          color vHitColor = {};
          for (const Hittable& oHittable : g_vHittables)
          {
            HitInfo oCandidateHitInfo = {};

            if (HitHittable(oRay, oHittable, oCandidateHitInfo) && oHitInfo.fT == 0.f || oCandidateHitInfo.fT < oHitInfo.fT)
            {
              oHitInfo = oCandidateHitInfo;
              vHitColor = oHittable.vColor;
              bHit = true;
            }
          }

          if (bHit)
          {
            vec3 vHeimsphereSample = Normalize(HemisphereSample(Random(), Random()));
            vec3 vRefractedRay = TangentToWorld(vHeimsphereSample, oHitInfo.vNormal);
            oRay = ray(oRay.vOrigin + (oHitInfo.fT * oRay.vDir) + (oHitInfo.vNormal * 0.001f), vRefractedRay);
            vRayColor = vRayColor * vHitColor;
          }

        } while (bHit);

        vPixelColor += vRayColor;
        */
        bool bHit = false;
        for (const Hittable& oHittable : g_vHittables)
        {
          HitInfo oCandidateHitInfo = {};
          if (HitHittable(oRay, oHittable, oCandidateHitInfo) && (oHitInfo.fT == 0.f || oCandidateHitInfo.fT < oHitInfo.fT))
          {
            oHitInfo = oCandidateHitInfo;
            bHit = true;
          }
        }

        oHitInfo = {};
        oHitInfo.fT = 1.f;

        if (bHit)
        {
          vec3 vHeimsphereSample = Normalize(HemisphereSample(Random(), Random()));
          vec3 vRefractedRay = TangentToWorld(vHeimsphereSample, oHitInfo.vNormal);
          oRay = ray(oRay.vOrigin + (oHitInfo.fT * oRay.vDir) + (oHitInfo.vNormal * 0.001f), vRefractedRay);

          for (const Hittable& oHittable : g_vHittables)
          {
            HitInfo oCandidateHitInfo = {};
            if (HitHittable(oRay, oHittable, oCandidateHitInfo) && (oHitInfo.fT == 0.f || oCandidateHitInfo.fT < oHitInfo.fT))
            {
              oHitInfo = oCandidateHitInfo;
            }
          }
        }

        vPixelColor = vPixelColor + color{ 1.f, 1.f, 1.f } *oHitInfo.fT;
      }

      vPixelColor /= iSAMPLE_COUNT;

      *pPixel++ = static_cast<int8_t>(vPixelColor.b * 255.f);

      *pPixel++ = static_cast<int8_t>(vPixelColor.g * 255.f);

      *pPixel++ = static_cast<int8_t>(vPixelColor.r * 255.f);

      *pPixel++ = 0;
    }
    pRow += uPitch;
  }
}

void UpdateGameSoundBuffer(
  uint32_t& uCurrSampleIdx,
  void* pRegion1,
  size_t uRegion1Size,
  void* pRegion2,
  size_t uRegion2Size,
  size_t uBytesPerSample)
{
  /*constexpr int iHalfWavePeriod = (48000 / 512) / 2;

  int16_t* pSampleOut = (int16_t*)pRegion1;
  uint32_t ulRegion1SampleCount = uRegion1Size / uBytesPerSample;

  for (uint32_t ulSampleIdx = 0; ulSampleIdx < ulRegion1SampleCount; ulSampleIdx++)
  {
    float fT = 2.0f * 3.14159f * (float)uCurrSampleIdx / ((float)iHalfWavePeriod * 2.f);
    int16_t iValue = (int16_t)(sinf(fT) * 5000);
    *pSampleOut++ = iValue;
    *pSampleOut++ = iValue;

    uCurrSampleIdx++;
  }

  pSampleOut = (int16_t*)pRegion2;
  uint32_t ulRegion2SampleCount = uRegion2Size / (sizeof(int16_t) * 2);
  for (uint32_t ulSampleIdx = 0; ulSampleIdx < ulRegion2SampleCount; ulSampleIdx++)
  {
    float fT = 2.0f * 3.14159f * (float)uCurrSampleIdx / ((float)iHalfWavePeriod * 2.f);
    int16_t iValue = (int16_t)(sinf(fT) * 5000);
    *pSampleOut++ = iValue;
    *pSampleOut++ = iValue;

    uCurrSampleIdx++;
  }*/
}
