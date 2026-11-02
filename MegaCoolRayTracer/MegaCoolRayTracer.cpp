#include "GameIncludes.h"

#include "Vec3.h"
#include "Vec2.h"
#include "Ray.h"
#include "MathUtils.h"

#include <math.h>
#include <cmath>
#include <vector>
#include <cfloat>

// Same structure-of-arrays scene as SuperCoolRayTracer, but the sphere test is plain C++ with no
// intrinsics: the compiler generates the SIMD code itself (built with /arch:AVX2, see CMakeLists.txt).

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

// Spheres are tested in blocks: loops the compiler vectorizes compute the distance to every sphere
// in the block, and a scalar loop picks the nearest only when the block has something closer than
// the best hit so far. Big blocks pay the per-block setup less often (16 was 40% slower than 128).
static constexpr int g_iSphereBlockSize = 256;

// Writes the distance along the ray to each sphere in [_iFirst, _iFirst + _iCount) into aT_, or
// FLT_MAX where the ray misses it. Same operations in the same order as CoolRayTracer's HitSphere,
// so the results match bit for bit, but with no branches: every condition is a select, so the
// compiler can test 8 spheres per instruction.
//
// The work is split in two loops because MSVC won't vectorize a loop that has both sqrtf and a
// select (vectorizer reason 1100, "control flow"). Each loop on its own vectorizes.
void SphereDistances(const ray& _oRay, int _iFirst, int _iCount, float* aT_)
{
  const float* pPosX = g_oScene.vPosX.data() + _iFirst;
  const float* pPosY = g_oScene.vPosY.data() + _iFirst;
  const float* pPosZ = g_oScene.vPosZ.data() + _iFirst;
  const float* pRadiusSqr = g_oScene.vRadiusSqr.data() + _iFirst;

  const float fOriginX = _oRay.vOrigin.x();
  const float fOriginY = _oRay.vOrigin.y();
  const float fOriginZ = _oRay.vOrigin.z();
  const float fDirX = _oRay.vDir.x();
  const float fDirY = _oRay.vDir.y();
  const float fDirZ = _oRay.vDir.z();

  float aH[g_iSphereBlockSize];
  float aC[g_iSphereBlockSize];
  float aSqrtDisc[g_iSphereBlockSize];

  // The math, up to the square root
  for (int i = 0; i < _iCount; i++)
  {
    const float fOcX = fOriginX - pPosX[i];
    const float fOcY = fOriginY - pPosY[i];
    const float fOcZ = fOriginZ - pPosZ[i];

    const float h = ((fOcX * fDirX) + (fOcY * fDirY)) + (fOcZ * fDirZ);
    const float c = (((fOcX * fOcX) + (fOcY * fOcY)) + (fOcZ * fOcZ)) - pRadiusSqr[i];
    const float discriminant = (h * h) - c;

    aH[i] = h;
    aC[i] = c;
    // Clamped so a miss never takes the square root of a negative number
    aSqrtDisc[i] = sqrtf(discriminant > 0.f ? discriminant : 0.f);
  }

  // The decisions: which root, if any, is a hit
  for (int i = 0; i < _iCount; i++)
  {
    const float h = aH[i];
    const float c = aC[i];
    // Recomputed rather than stored: two operations are cheaper than another array round trip
    const float discriminant = (h * h) - c;
    const float t0 = -h - aSqrtDisc[i];
    const float t1 = -h + aSqrtDisc[i];

    // (t0 > 0) ? t0 : ((t1 > 0) ? t1 : -1) as two statements: MSVC won't vectorize a nested ?:
    const float fT1 = (t1 > 0.f) ? t1 : -1.f;
    const float fT = (t0 > 0.f) ? t0 : fT1;
    const float fFrontT = (fT > 0.001f) ? fT : FLT_MAX;

    // Real roots, and the origin isn't outside the sphere pointing away from it.
    // & instead of && so there's no branch.
    aT_[i] = ((discriminant > 0.f) & !((c > 0.f) & (h > 0.f))) ? fFrontT : FLT_MAX;
  }
}

// Index of the nearest sphere the ray hits, or -1, and its distance in fT_
int ClosestSphere(const ray& _oRay, float& fT_)
{
  int iClosest = -1;
  float fClosestT = FLT_MAX;

  const int iSphereCount = static_cast<int>(g_oScene.vMaterials.size());
  for (int iFirst = 0; iFirst < iSphereCount; iFirst += g_iSphereBlockSize)
  {
    const int iCount = min(g_iSphereBlockSize, iSphereCount - iFirst);

    float aT[g_iSphereBlockSize];
    SphereDistances(_oRay, iFirst, iCount, aT);

    // Counting the distances closer than the best hit so far vectorizes, unlike the search for the
    // nearest below, so blocks with none (most of them, in big scenes) skip the search.
    int iCloser = 0;
    for (int i = 0; i < iCount; i++)
    {
      iCloser += (aT[i] < fClosestT) ? 1 : 0;
    }

    if (iCloser == 0)
    {
      continue;
    }

    // Smallest distance, first index on ties: the same rule as the other tracers
    for (int i = 0; i < iCount; i++)
    {
      if (aT[i] < fClosestT)
      {
        fClosestT = aT[i];
        iClosest = iFirst + i;
      }
    }
  }

  fT_ = fClosestT;
  return iClosest;
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
          float fClosestT = FLT_MAX;
          const int iHittableIdx = ClosestSphere(oRay, fClosestT);

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
