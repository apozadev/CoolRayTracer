#include "GameIncludes.h"

#include <immintrin.h>

#include "Vec3.h"
#include "Vec2.h"
#include "Ray.h"
#include "MathUtils.h"

#include <math.h>
#include <cmath>
#include <vector>

using color = vec3;

const float g_fAirRefractionIndex = 1.0f;

enum MaterialType
{
  MaterialType_Lambertian,
  MaterialType_Metal,
  MaterialType_Dielectric
};

struct Material
{
  MaterialType eType;
  color vAlbedo;
  union
  {
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

struct Scene
{
  std::vector<Material> vMaterials;

  // Sphere positions
  std::vector<float> vPosX;
  std::vector<float> vPosY;
  std::vector<float> vPosZ;

  // Sphere radii squared
  std::vector<float> vRadiusSqr;

};

Scene g_oScene = {};

// Ground plane at y = -1, same material as the plane in CoolRayTracer
const Material g_oPlaneMaterial = { MaterialType_Lambertian, color(0.5f, 0.5f, 0.5f) };

struct HitSphere8Result
{
  float t;
  int idx;
};

HitSphere8Result HitSphere8(
  const vec3& rayOrigin,
  const vec3& rayDir,
  const float* centerX,
  const float* centerY,
  const float* centerZ,
  const float* radiusSqr,
  int validCount)
{
  HitSphere8Result oResult = {};
  oResult.t = FLT_MAX;
  oResult.idx = -1;

  // Ray origin
  const __m256 rox = _mm256_set1_ps(rayOrigin.x());
  const __m256 roy = _mm256_set1_ps(rayOrigin.y());
  const __m256 roz = _mm256_set1_ps(rayOrigin.z());

  // Ray direction
  const __m256 rdx = _mm256_set1_ps(rayDir.x());
  const __m256 rdy = _mm256_set1_ps(rayDir.y());
  const __m256 rdz = _mm256_set1_ps(rayDir.z());

  // Sphere centers
  const __m256 cx = _mm256_loadu_ps(centerX);
  const __m256 cy = _mm256_loadu_ps(centerY);
  const __m256 cz = _mm256_loadu_ps(centerZ);

  // Sphere radii squared
  const __m256 r2 = _mm256_loadu_ps(radiusSqr);

  // oc = rayOrigin - sphereCenter
  const __m256 ocx = _mm256_sub_ps(rox, cx);
  const __m256 ocy = _mm256_sub_ps(roy, cy);
  const __m256 ocz = _mm256_sub_ps(roz, cz);

  // rayDir is normalized (a == 1), so use the half-b form:
  // h = dot(oc, rayDir), disc = h^2 - c, t = -h -+ sqrt(disc).
  // Dot products sum as (x + y) + z to match the scalar vec3 Dot.
  const __m256 h = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(ocx, rdx), _mm256_mul_ps(ocy, rdy)), _mm256_mul_ps(ocz, rdz));

  // c = dot(oc, oc) - radius^2
  const __m256 c = _mm256_sub_ps(_mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(ocx, ocx), _mm256_mul_ps(ocy, ocy)), _mm256_mul_ps(ocz, ocz)), r2);

  // discriminant = h^2 - c
  const __m256 discriminant = _mm256_sub_ps(_mm256_mul_ps(h, h), c);

  const __m256 zero = _mm256_setzero_ps();

  // Origin outside the sphere and pointing away from it: both roots are behind the ray
  const __m256 awayMask = _mm256_and_ps(_mm256_cmp_ps(c, zero, _CMP_GT_OQ), _mm256_cmp_ps(h, zero, _CMP_GT_OQ));

  // mask out padding lanes past the last real sphere
  const __m256 laneIdx = _mm256_setr_ps(0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f);
  const __m256 laneValidMask = _mm256_cmp_ps(laneIdx, _mm256_set1_ps(static_cast<float>(validCount)), _CMP_LT_OQ);

  // candidate lanes: discriminant > 0, not pointing away, real sphere
  const __m256 hitMask = _mm256_andnot_ps(awayMask, _mm256_and_ps(_mm256_cmp_ps(discriminant, zero, _CMP_GT_OQ), laneValidMask));

  // Whole group misses: skip the sqrt and the reduction
  if (_mm256_movemask_ps(hitMask) == 0)
  {
    return oResult;
  }

  // sqrtDiscriminant = sqrt(discriminant)
  const __m256 sqrtDiscriminant = _mm256_sqrt_ps(discriminant);

  // t0 = -h - sqrtDiscriminant, t1 = -h + sqrtDiscriminant
  const __m256 negH = _mm256_sub_ps(zero, h);
  const __m256 t0 = _mm256_sub_ps(negH, sqrtDiscriminant);
  const __m256 t1 = _mm256_add_ps(negH, sqrtDiscriminant);

  // t = (t0 > 0.f) ? t0 : ((t1 > 0.f) ? t1 : -1.f);
  const __m256 t0ValidMask = _mm256_cmp_ps(t0, zero, _CMP_GT_OQ);
  const __m256 t1ValidMask = _mm256_cmp_ps(t1, zero, _CMP_GT_OQ);
  const __m256 t = _mm256_blendv_ps(_mm256_blendv_ps(_mm256_set1_ps(-1.f), t1, t1ValidMask), t0, t0ValidMask);

  // mask for t > 0.001f (to avoid self-intersection)
  const __m256 tValidMask = _mm256_cmp_ps(t, _mm256_set1_ps(0.001f), _CMP_GT_OQ);

  // final hit mask combines candidate lanes and t > 0.001f
  const __m256 finalHitMask = _mm256_and_ps(hitMask, tValidMask);

  // Set t to infinity for invalid hits
  const __m256 inf = _mm256_set1_ps(FLT_MAX);
  __m256 validT = _mm256_blendv_ps(inf, t, finalHitMask);

  // Store T values to search for the smallest
  alignas(32) float tValues[8];
  _mm256_store_ps(tValues, validT);

  for (int i = 0; i < 8; i++)
  {
    if (tValues[i] < oResult.t)
    {
      oResult.t = tValues[i];
      oResult.idx = i;
    }
  }

  return oResult;
}

void HitPlane(const ray& _oRay, float& fT_)
{
  const float fDenom = Dot(vec3(0.f, 1.f, 0.f), _oRay.vDir);
  if (fabs(fDenom) > 0.0001f)
  {
    const float fT = (-1.f - Dot(vec3(0.f, 1.f, 0.f), _oRay.vOrigin)) / fDenom;
    if (fT >= 0)
    {
      fT_ = fT;      
    }
  }  
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
    const float fPosY = -1.0f + fRadius; 

    const MaterialType eMatType = static_cast<MaterialType>(Random() * 2.99f);

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

    g_oScene.vMaterials.push_back(oMaterial);
    g_oScene.vPosX.push_back(fPosX);
    g_oScene.vPosY.push_back(fPosY);
    g_oScene.vPosZ.push_back(fPosZ);
    g_oScene.vRadiusSqr.push_back(fRadius * fRadius);
  }

  // Pad sphere arrays to a multiple of 8 so HitSphere8 loads stay in bounds
  const size_t uPaddedCount = (g_oScene.vRadiusSqr.size() + 7) & ~size_t(7);
  g_oScene.vPosX.resize(uPaddedCount, 0.f);
  g_oScene.vPosY.resize(uPaddedCount, 0.f);
  g_oScene.vPosZ.resize(uPaddedCount, 0.f);
  g_oScene.vRadiusSqr.resize(uPaddedCount, 0.f);
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

        while (true)
        {
          int iHittableIdx = -1;
          float fClosestT = FLT_MAX;          

          const int iSphereCount = static_cast<int>(g_oScene.vMaterials.size());
          for (int i = 0; i < iSphereCount; i += 8)
          {
            HitSphere8Result oHitResult = HitSphere8(oRay.vOrigin, oRay.vDir, &g_oScene.vPosX[i], &g_oScene.vPosY[i], &g_oScene.vPosZ[i], &g_oScene.vRadiusSqr[i], iSphereCount - i);

            if (oHitResult.idx != -1 && oHitResult.t < fClosestT)
            {
              fClosestT = oHitResult.t;
              iHittableIdx = i + oHitResult.idx;
            }
          }

          // Check plane hit (after the spheres, so it only wins when strictly closer)
          bool bHitPlane = false;
          float fPlaneT = FLT_MAX;
          HitPlane(oRay, fPlaneT);
          if (fPlaneT < fClosestT)
          {
            fClosestT = fPlaneT;
            bHitPlane = true;
          }

          if (iHittableIdx < 0 && !bHitPlane)
          {
            // Blue-white gradient sky color
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

          vec3 vHitNormal;
          Material oMaterial;

          if (bHitPlane)
          {
            vHitNormal = vec3(0, 1, 0);
            oMaterial = g_oPlaneMaterial;
          }
          else
          {
            vHitNormal = Normalize(oRay.vOrigin + fClosestT * oRay.vDir - vec3(g_oScene.vPosX[iHittableIdx], g_oScene.vPosY[iHittableIdx], g_oScene.vPosZ[iHittableIdx]));
            oMaterial = g_oScene.vMaterials[iHittableIdx];
          }

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
            if (vInRay.LengthSqr() == 0.f) // Total internal reflection, fallback to reflection
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
