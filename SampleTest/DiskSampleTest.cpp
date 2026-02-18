#include "../CoolRayTracer/CoolRayTracer.h"

#include "../CoolRayTracer/Vec3.h"
#include "../CoolRayTracer/Vec2.h"
#include "../CoolRayTracer/Ray.h"
#include "../CoolRayTracer/MathUtils.h"

#include <math.h>
#include <cmath>
#include <vector>

using color = vec3;

constexpr int SAMPLE_COUNT = 10000;

vec2 aSamplePoints[SAMPLE_COUNT];

void InitGame()
{
  for(int i = 0; i < SAMPLE_COUNT; i++)
  {
    aSamplePoints[i] = SampleDisk(Random(), Random());
  }
}

void UpdateScreenBufferPartial(GameScreenBuffer* Buffer, int _iStartX, int _iStartY, int _iEndX, int _iEndY)
{
  const int iPointPixelHalfSize = 1;

  for (int i = 0; i < SAMPLE_COUNT; i++)
  {
    vec2 vSamplePoint = aSamplePoints[i];

    float fAspectRatio = static_cast<float>(Buffer->iHeight) / Buffer->iWidth;

    // Remap to account for the point pixel radius, so that the points are not cut off at the edges of the screen.
    int iScreenX = static_cast<int>((vSamplePoint.x + 1.f) * 0.5f * (Buffer->iWidth * fAspectRatio - 2 * iPointPixelHalfSize) + iPointPixelHalfSize);
    int iScreenY = static_cast<int>((vSamplePoint.y + 1.f) * 0.5f * (Buffer->iHeight - 2 * iPointPixelHalfSize) + iPointPixelHalfSize);

    int iMinX = std::max(iScreenX - iPointPixelHalfSize, _iStartX);
    int iMaxX = std::min(iScreenX + iPointPixelHalfSize, _iEndX);
    int iMinY = std::max(iScreenY - iPointPixelHalfSize, _iStartY);
    int iMaxY = std::min(iScreenY + iPointPixelHalfSize, _iEndY);

    int uPitch = Buffer->iWidth * g_uBytesPerPixel;
    uint8_t* pRow = ((uint8_t*)Buffer->pData) + uPitch * iMinY;

    for(int y = iMinY; y < iMaxY; y++)
    {
      uint8_t* pPixel = ((uint8_t*)pRow) + iMinX * g_uBytesPerPixel;
      for (int x = iMinX; x < iMaxX; x++)
      {
        *pPixel++ = 255u; // Blue
        *pPixel++ = 255u; // Green
        *pPixel++ = 255u; // Red
        *pPixel++ = 0u;   // Alpha (unused)
      }
      pRow += uPitch;
    }
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
