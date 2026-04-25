#include "CoolRayTracer.h"

#include "Vec3.h"
#include "Vec2.h"
#include "Ray.h"
#include "MathUtils.h"

#include <math.h>
#include <cmath>
#include <vector>

using color = vec3;

float g_fAirRefractionIndex = 1.0f;

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
    } oDielectric;;
  };
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

    float fT = (t0 > 0.f) ? t0 : ((t1 > 0.f) ? t1 : -1.f);
    
    if (fT > 0.001f)
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
  } break;      
  }

  return false;
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

void InitGame()
{
  {
    Hittable oSphere = {};
    oSphere.eType = HittableType_Sphere;
    oSphere.oSphere.vCenter = vec3(0, 0, -5);
    oSphere.oSphere.fRadius = 1.0f;

    Material oMaterial = {};
    oMaterial.eType = MaterialType_Dielectric;
    oMaterial.oDielectric.fRefractionIndex = 1.5f;
    oMaterial.vAlbedo = vec3(1, 1, 1);

    g_oScene.vHittables.push_back(oSphere);
    g_oScene.vMaterials.push_back(oMaterial);
  }

  {
    Hittable oSphere = {};
    oSphere.eType = HittableType_Sphere;
    oSphere.oSphere.vCenter = vec3(2, 0, -5);
    oSphere.oSphere.fRadius = 1.0f;

    Material oMaterial = {};
    oMaterial.eType = MaterialType_Metal;
    oMaterial.oMetal.fRoughness = 0.2f;
    oMaterial.vAlbedo = vec3(1, 1, 1);

    g_oScene.vHittables.push_back(oSphere);
    g_oScene.vMaterials.push_back(oMaterial);
  }

  {
    Hittable oSphere = {};
    oSphere.eType = HittableType_Sphere;
    oSphere.oSphere.vCenter = vec3(-2.f, -0.75f, -4.f);
    oSphere.oSphere.fRadius = 0.25f;

    Material oMaterial = {};
    oMaterial.eType = MaterialType_Lambertian;    
    oMaterial.vAlbedo = vec3(0.35f, 0.2f, 0.5f);

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

  float fFocalLength = 1.0f;
  float fViewportHeight = 1.0f;
  float fViewportWidth = fViewportHeight * (float(Buffer->iWidth) / Buffer->iHeight);
  //float fAspectRatio = fViewportWidth / fViewportHeight;
  vec3 vCameraCenter = vec3(0, 0, 0);

  vec3 vViewportX = vec3(fViewportWidth, 0, 0);
  vec3 vViewportY = vec3(0, -fViewportHeight, 0);

  vec3 vPixelDeltaX = vViewportX / float(Buffer->iWidth);
  vec3 vPixelDeltaY = vViewportY / float(Buffer->iHeight);

  vec3 vViewportUpperLeft = vCameraCenter - (vViewportX / 2) - (vViewportY / 2) - (vec3(0, 0, fFocalLength));
  vec3 vStartPixel = vViewportUpperLeft + (vPixelDeltaX / 2) + (vPixelDeltaY / 2);

  constexpr int iMAX_BOUNCES = 4;

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
        vec3 vPixelCenter = vStartPixel + (x + vOffset.x()) * vPixelDeltaX + (y + vOffset.y()) * vPixelDeltaY;
        vec3 vRayDirection = Normalize(vPixelCenter - vCameraCenter);
        ray oRay(vCameraCenter, vRayDirection);
        
        color vRayColor = { 1.f, 1.f, 1.f };

        int iBounces = 0;

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
          
          vec3 vInRay = {};
          switch (oMaterial.eType)
          {
          case MaterialType_Lambertian:
            vInRay = TangentToWorld(Normalize(SampleHemisphereCosine(Random(), Random())), oHitInfo.vNormal);
            break;
          case MaterialType_Metal:
            vInRay = Reflect(oRay.vDir, oHitInfo.vNormal);
            break;
          case MaterialType_Dielectric:
          {
            bool bFromOutside = Dot(oRay.vDir, oHitInfo.vNormal) < 0.0f;
            float fRelativeRefractionIndex = bFromOutside
              ? (g_fAirRefractionIndex / oMaterial.oDielectric.fRefractionIndex)
              : (oMaterial.oDielectric.fRefractionIndex / g_fAirRefractionIndex);

            vInRay = Refract(oRay.vDir, oHitInfo.vNormal, fRelativeRefractionIndex);
            if(vInRay.LengthSqr() == 0.f) // Total internal reflection, fallback to reflection
            {
              vInRay = Reflect(oRay.vDir, oHitInfo.vNormal);
            }
            break;
          }
          default:
            break;
          }

          vec3 vBias = Dot(vInRay, oHitInfo.vNormal) > 0.0f
            ? oHitInfo.vNormal * 0.001f
            : -oHitInfo.vNormal * 0.001f;
          oRay = ray(oRay.vOrigin + (oHitInfo.fT * oRay.vDir) + vBias, vInRay);
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
