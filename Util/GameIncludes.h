#pragma once

#include <stdint.h>

struct GameScreenBuffer {
    void* pData;
    int iWidth;
    int iHeight;
};

struct GameInput {
    float XOffset;
    float YOffset;    
};

static constexpr size_t g_uBytesPerPixel = 4;

// Spheres in the default scene. -spheres N builds a bigger or smaller world at the same density.
static constexpr int g_iDefaultSphereCount = 80;

void InitGame(int _iSphereCount);

void UpdateScreenBufferPartial(GameScreenBuffer* Buffer, int _iStartX, int _iStartY, int _iEndX, int _iEndY);

void UpdateGameBackBuffer(GameScreenBuffer* Buffer, const GameInput& GameInput);

void UpdateGameSoundBuffer(uint32_t& uCurrSampleIdx, void* pRegion1, size_t uRegion1Size, void* pRegion2, size_t uRegion2Size, size_t uBytesPerSample);
