#pragma once

#include <stdint.h>

struct GameScreenBuffer
{
  void* pData;
  int iWidth;
  int iHeight;
};

struct GameInput
{
  float XOffset = 0.f;
  float YOffset = 0.f;
};

static constexpr size_t g_uBytesPerPixel = 4;

void InitGame();

void UpdateScreenBufferPartial(GameScreenBuffer* Buffer, int _iStartX, int _iStartY, int _iEndX, int _iEndY);

void UpdateGameBackBuffer(GameScreenBuffer* Buffer, const GameInput& GameInput);

void UpdateGameSoundBuffer(uint32_t& uCurrSampleIdx, void* pRegion1, size_t uRegion1Size, void* pRegion2, size_t uRegion2Size, size_t uBytesPerSample);
