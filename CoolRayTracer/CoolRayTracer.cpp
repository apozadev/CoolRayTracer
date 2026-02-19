#include "CoolRayTracer.h"

#include "Vec3.h"
#include "Vec2.h"
#include "Ray.h"
#include "MathUtils.h"

#include <math.h>
#include <cmath>
#include <vector>

using color = vec3;

enum MaterialType
{
  MaterialType_Lambertian,
  MaterialType_Metal
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
};

struct Sphere
{
  vec3 vCenter;
  float fRadius;
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

struct HitInfo
{
  float fT;
  vec3 vNormal;
};

struct Scene
{
  std::vector<Hittable> vHittables;
  std::vector<Material> vMaterials;
};

Scene g_oScene = {};

bool HitSphere(const ray& _oRay, const Sphere& _oSphere, HitInfo& oHitInfo_)
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

bool HitPlane(const ray& _oRay, const Plane& _oPlane, HitInfo& oHitInfo_)
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
  case HittableType_Sphere:
  {
    return HitSphere(_oRay, _oHittable.oSphere, oHitInfo);
  } break;
  case HittableType_Plane:
  {
    return HitPlane(_oRay, _oHittable.oPlane, oHitInfo);
  }
  break;
  }
}

vec3 Reflect(const vec3& _vInRay, const vec3& _vNormal)
{
  return _vInRay - (2 * Dot(_vInRay, _vNormal) * _vNormal);
}

vec3 GetIncidentRay(const vec3& _vOutRay, const vec3& _vNormal, MaterialType _eMaterialType)
{
  vec3 vInRay = {};

  switch (_eMaterialType)
  {
  case MaterialType_Lambertian:
    vInRay = TangentToWorld(Normalize(SampleHemisphereCosine(Random(), Random())), _vNormal);
    break;
  case MaterialType_Metal:
    vInRay = Reflect(_vOutRay, _vNormal);
    break;
  default:
    break;
  }

  return vInRay;
}

void AddHittable(Hittable&& _oHittable, Material&& _oMaterial, Scene& oScene_)
{
  oScene_.vHittables.emplace_back(_oHittable);
  oScene_.vMaterials.emplace_back(_oMaterial);
}

double LinearToGamma(double _fValue)
{
  return pow(_fValue, 1.0 / 2.2);
}

void InitGame()
{
  Hittable oSphereHittable = {};
  oSphereHittable.eType = HittableType_Sphere;
  oSphereHittable.oSphere.vCenter = vec3(0, 0, -5);
  oSphereHittable.oSphere.fRadius = 1.0f;  

  Material oMaterial1 = {};
  oMaterial1.eType = MaterialType_Metal;
  oMaterial1.vAlbedo = vec3(0.5, 0, 0);

  g_oScene.vHittables.push_back(oSphereHittable);
  g_oScene.vMaterials.push_back(oMaterial1);

  Hittable oPlaneHittable = {};
  oPlaneHittable.eType = HittableType_Plane;
  oPlaneHittable.oPlane.vNormal = vec3(0, 1, 0);
  oPlaneHittable.oPlane.fPoint = -1.0f;  

  Material oMaterial2 = {};
  oMaterial2.eType = MaterialType_Lambertian;
  oMaterial2.vAlbedo = vec3(0.5, 0.5, 0.5);

  g_oScene.vHittables.push_back(oPlaneHittable);
  g_oScene.vMaterials.push_back(oMaterial2);
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
        
        color vRayColor = { 1.f, 1.f, 1.f };

        while(true)
        {
          int iHittableIdx = -1;
          HitInfo oHitInfo = {};          

          for (int i = 0; i < g_oScene.vHittables.size(); i++)
          {
            const Hittable& oHittable = g_oScene.vHittables[i];
            HitInfo oCandidateHitInfo = {};
            if (HitHittable(oRay, oHittable, oCandidateHitInfo) && (oHitInfo.fT == 0.f || oCandidateHitInfo.fT < oHitInfo.fT))
            {
              oHitInfo = oCandidateHitInfo;              
              iHittableIdx = i;
            }
          }

          if (iHittableIdx < 0)
          {
            break;
          }

          const Material& oMaterial = g_oScene.vMaterials[iHittableIdx];

          vec3 vInRay = GetIncidentRay(oRay.vDir, oHitInfo.vNormal, oMaterial.eType);
          oRay = ray(oRay.vOrigin + (oHitInfo.fT * oRay.vDir) + (oHitInfo.vNormal * 0.001f), vInRay);
          vRayColor = vRayColor * oMaterial.vAlbedo;
        }

        vPixelColor += vRayColor;        
      }

      vPixelColor /= iSAMPLE_COUNT;

      *pPixel++ = static_cast<uint8_t>(LinearToGamma(vPixelColor.b) * 255.f);

      *pPixel++ = static_cast<uint8_t>(LinearToGamma(vPixelColor.g) * 255.f);

      *pPixel++ = static_cast<uint8_t>(LinearToGamma(vPixelColor.r) * 255.f);

      *pPixel++ = 0u;
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
}
