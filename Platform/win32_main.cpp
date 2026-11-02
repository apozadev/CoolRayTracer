#include <windows.h>
#include <stdint.h>
#include <Xinput.h>
#include <dsound.h>
#include <math.h>
#include <cmath>
#include <stdio.h>
#include <string.h>

#include "GameIncludes.h"
#include "Random.h"

static bool g_bRunning = true;

struct Win32ScreenBuffer
{
  BITMAPINFO oBitmapInfo;
  void* pData;
  int iWidth;
  int iHeight;
};

static Win32ScreenBuffer g_oBackBuffer = {};

static constexpr int g_iWindowWidth = 1280;
static constexpr int g_iWindowHeight = 720;

static int g_iSamplesPerSecond = 48000;

static LPDIRECTSOUNDBUFFER g_pSecondaryBuffer;
static int g_iSoundBufferSeconds = 3;
static size_t g_uSoundBufferSize;
static int g_iSquareWaveCounter = 0;
static uint32_t g_uCurrSampleIdx = 0;
static constexpr DWORD g_ulBytesPerSample = (DWORD)(sizeof(int16_t) * 2);
static int g_iLatencySampleCount = 0;
static bool g_bSoundStarted = false;

GameInput g_oGameInput = {};

// XInput

typedef DWORD WINAPI XInputGetState_t(_In_ DWORD dwUserIndex, _Out_ XINPUT_STATE* pState);
typedef DWORD WINAPI XInputSetState_t(_In_ DWORD dwUserIndex, _In_ XINPUT_VIBRATION* pVibration);

DWORD WINAPI XInputGetStateStub(_In_ DWORD /*dwUserIndex*/, _Out_ XINPUT_STATE* /*pState*/)
{
  return ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI XInputSetStateStub(_In_ DWORD /*dwUserIndex*/, _In_ XINPUT_VIBRATION* /*pVibration*/)
{
  return ERROR_DEVICE_NOT_CONNECTED;
}

static XInputGetState_t* g_pfncXInputGetState = XInputGetStateStub;
static XInputSetState_t* g_pfncXInputSetState = XInputSetStateStub;

#define XInputGetState g_pfncXInputGetState
#define XInputSetState g_pfncXInputSetState

void Win32LoadXInput()
{
  HMODULE hModule = LoadLibrary(XINPUT_DLL_A);

  if (hModule)
  {
    XInputGetState = (XInputGetState_t*)GetProcAddress(hModule, "XInputGetState");
    XInputSetState = (XInputSetState_t*)GetProcAddress(hModule, "XInputSetState");
  }
}

// DirectSound 

typedef HRESULT WINAPI DirectSoundCreate_t(_In_opt_ LPCGUID pcGuidDevice, _Outptr_ LPDIRECTSOUND* ppDS, _Pre_null_ LPUNKNOWN pUnkOuter);

HRESULT DirectSoundCreateStub(_In_opt_ LPCGUID /*pcGuidDevice*/, _Outptr_ LPDIRECTSOUND* /*ppDS*/, _Pre_null_ LPUNKNOWN /*pUnkOuter*/)
{
  return 0;
}

void Win32InitDirectSound(HWND _hWnd)
{
  HMODULE hModule = LoadLibrary("dsound.dll");

  DirectSoundCreate_t* pfnDirectSoundCreate = nullptr;

  if (hModule)
  {
    pfnDirectSoundCreate = (DirectSoundCreate_t*)GetProcAddress(hModule, "DirectSoundCreate");
  }

  LPDIRECTSOUND pDSound;

  if (pfnDirectSoundCreate &&
    SUCCEEDED(pfnDirectSoundCreate(0, &pDSound, 0)) &&
    SUCCEEDED(pDSound->SetCooperativeLevel(_hWnd, DSSCL_PRIORITY)))
  {
    WAVEFORMATEX oWaveFormat = {};
    oWaveFormat.wFormatTag = WAVE_FORMAT_PCM;
    oWaveFormat.nChannels = 2;
    oWaveFormat.nSamplesPerSec = (DWORD)g_iSamplesPerSecond;
    oWaveFormat.wBitsPerSample = 16;
    oWaveFormat.nBlockAlign = (oWaveFormat.nChannels * oWaveFormat.wBitsPerSample) / 8;
    oWaveFormat.nAvgBytesPerSec = oWaveFormat.nSamplesPerSec * oWaveFormat.nBlockAlign;
    oWaveFormat.cbSize = 0;

    LPDIRECTSOUNDBUFFER pPrimaryBuffer;

    {
      DSBUFFERDESC oBufferDesc = {};
      oBufferDesc.dwSize = sizeof(oBufferDesc);
      oBufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;

      if (SUCCEEDED(pDSound->CreateSoundBuffer(&oBufferDesc, &pPrimaryBuffer, 0)))
      {
        HRESULT hRes = pPrimaryBuffer->SetFormat(&oWaveFormat);
        if (SUCCEEDED(hRes))
        {
          OutputDebugStringA("Primary Buffer Created\n");
        }
      }
    }

    g_uSoundBufferSize = oWaveFormat.nSamplesPerSec * (oWaveFormat.wBitsPerSample / 8) * g_iSoundBufferSeconds;
    g_iLatencySampleCount = oWaveFormat.nSamplesPerSec / 20;

    {
      DSBUFFERDESC oBufferDesc = {};
      oBufferDesc.dwSize = sizeof(oBufferDesc);
      oBufferDesc.dwFlags = DSBCAPS_GLOBALFOCUS;
      oBufferDesc.dwBufferBytes = static_cast<DWORD>(g_uSoundBufferSize);
      oBufferDesc.lpwfxFormat = &oWaveFormat;

      if (SUCCEEDED(pDSound->CreateSoundBuffer(&oBufferDesc, &g_pSecondaryBuffer, 0)))
      {
        OutputDebugStringA("Secondary Buffer Created\n");
      }
    }
  }
}

void UpdateSoundBuffer()
{
  //int iHalfWavePeriod = (48000 / 512) / 2;

  DWORD ulWriteCursor;
  DWORD ulPlayCursor;

  static DWORD s_ulPrevPlayCursor = 0;

  if (SUCCEEDED(g_pSecondaryBuffer->GetCurrentPosition(&ulPlayCursor, &ulWriteCursor)))
  {
    DWORD ulByteToLock = (g_uCurrSampleIdx * g_ulBytesPerSample) % g_uSoundBufferSize;
    DWORD ulBytesToWrite;

    if (!g_bSoundStarted)
    {
      ulBytesToWrite = g_iLatencySampleCount * g_ulBytesPerSample;
      g_bSoundStarted = true;
    }
    else
    {
      DWORD ulTargetCursor = (ulPlayCursor + (DWORD)(g_iLatencySampleCount)*g_ulBytesPerSample);

      ulTargetCursor = ulTargetCursor % g_uSoundBufferSize;
      if (ulByteToLock > ulTargetCursor)
      {
        ulBytesToWrite = ((DWORD)g_uSoundBufferSize - ulByteToLock) + ulTargetCursor;
      }
      else
      {
        ulBytesToWrite = ulTargetCursor - ulByteToLock;
      }
    }

    VOID* pRegion1;
    DWORD ulRegion1Size;
    VOID* pRegion2;
    DWORD ulRegion2Size;

    HRESULT hr = g_pSecondaryBuffer->Lock(
      ulByteToLock, ulBytesToWrite,
      &pRegion1, &ulRegion1Size,
      &pRegion2, &ulRegion2Size,
      0);

    if (SUCCEEDED(hr))
    {
      UpdateGameSoundBuffer(g_uCurrSampleIdx, pRegion1, ulRegion1Size, pRegion2, ulRegion2Size, g_ulBytesPerSample);

      g_pSecondaryBuffer->Unlock(pRegion1, ulRegion1Size, pRegion2, ulRegion2Size);
    }

    // DEBUG VISUALIZATION
    /*{
      uint8_t* pRow = (uint8_t*)g_oBackBuffer.pData;
      int uPitch = g_oBackBuffer.iWidth * g_uBytesPerPixel;

      bool bLoop = ulByteToLock + ulBytesToWrite > g_uSoundBufferSize;

      int iRegion1Byte = ulByteToLock;
      int iRegion2Byte = bLoop ? 0 : -1;

      int iRegion1Size = bLoop ? (g_uSoundBufferSize - ulByteToLock) : ulBytesToWrite;
      int iRegion2Size = bLoop ? (ulBytesToWrite - (g_uSoundBufferSize - ulByteToLock)) : -1;

      iRegion1Byte = ((float)iRegion1Byte / (float)g_uSoundBufferSize) * g_oBackBuffer.iWidth;
      iRegion2Byte = ((float)iRegion2Byte / (float)g_uSoundBufferSize) * g_oBackBuffer.iWidth;
      iRegion1Size = ((float)iRegion1Size / (float)g_uSoundBufferSize) * g_oBackBuffer.iWidth;
      iRegion2Size = ((float)iRegion2Size / (float)g_uSoundBufferSize) * g_oBackBuffer.iWidth;

      int iTargetByte = ((float)ulPlayCursor / (float)g_uSoundBufferSize) * g_oBackBuffer.iWidth;

      const int iDangerZoneSize = 30;

      const int iMinDangerByte = iTargetByte - iDangerZoneSize / 2;
      const int iMaxDangerByte = iTargetByte + iDangerZoneSize / 2;

      for (int y = 0; y < g_oBackBuffer.iHeight; y++)
      {
        uint8_t* pPixel = (uint8_t*)pRow;
        for (int x = 0; x < g_oBackBuffer.iWidth; x++)
        {

          float fR = 0.f, fG = 0.f, fB = 0.f;

          if (y < (g_oBackBuffer.iHeight / 2))
          {
            if (x > iRegion1Byte && x < (iRegion1Byte + iRegion1Size)
              || bLoop && x > iRegion2Byte && x < iRegion2Byte + iRegion2Size)
            {
              fG = 1.f;
            }
          }
          else if(x <= iTargetByte - iDangerZoneSize / 2)
          {
            fB = 1.f;
          }
          else if (x > iMinDangerByte && x < iMaxDangerByte)
          {
            fB = 1.f - (float)(x - iMinDangerByte) / (float)(iMaxDangerByte - iMinDangerByte);
          }

          *pPixel = (uint8_t)(fB * 255.f);
          pPixel++;

          *pPixel = (uint8_t)(fG * 255.f);
          pPixel++;

          *pPixel = (uint8_t)(fR * 255.f);
          pPixel++;

          *pPixel = 0;
          pPixel++;
        }
        pRow += uPitch;
      }
    }*/
  }
}

void GetWindowDimensions(HWND _hWnd, int& iWidth_, int& iHeight_)
{
  RECT oWindowRect;
  GetClientRect(_hWnd, &oWindowRect);
  iHeight_ = oWindowRect.bottom - oWindowRect.top;
  iWidth_ = oWindowRect.right - oWindowRect.left;
}

void Win32ResizeDIBSection(int _iWidth, int _iHeight)
{

  if (g_oBackBuffer.pData)
  {
    VirtualFree(g_oBackBuffer.pData, 0, MEM_RELEASE);
  }

  g_oBackBuffer.iWidth = _iWidth;
  g_oBackBuffer.iHeight = _iHeight;

  g_oBackBuffer.oBitmapInfo = {};
  g_oBackBuffer.oBitmapInfo.bmiHeader.biSize = sizeof(g_oBackBuffer.oBitmapInfo.bmiHeader);
  g_oBackBuffer.oBitmapInfo.bmiHeader.biWidth = g_oBackBuffer.iWidth;
  g_oBackBuffer.oBitmapInfo.bmiHeader.biHeight = -g_oBackBuffer.iHeight;
  g_oBackBuffer.oBitmapInfo.bmiHeader.biPlanes = 1;
  g_oBackBuffer.oBitmapInfo.bmiHeader.biBitCount = 32;
  g_oBackBuffer.oBitmapInfo.bmiHeader.biCompression = BI_RGB;

  size_t uBitmapByteSize = g_oBackBuffer.iWidth * g_oBackBuffer.iHeight * g_uBytesPerPixel;
  g_oBackBuffer.pData = VirtualAlloc(0, uBitmapByteSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

  GameScreenBuffer oGameBuffer = {};
  oGameBuffer.pData = g_oBackBuffer.pData;
  oGameBuffer.iWidth = g_oBackBuffer.iWidth;
  oGameBuffer.iHeight = g_oBackBuffer.iHeight;

  //UpdateGameBackBuffer(&oGameBuffer, g_oGameInput);
}

void Win32DisplayBackBuffer(HDC _hDeviceCtx, int _iWidth, int _iHeight)
{
  StretchDIBits(_hDeviceCtx,
    0, 0, _iWidth, _iHeight,
    0, 0, g_oBackBuffer.iWidth, g_oBackBuffer.iHeight,
    g_oBackBuffer.pData,
    &g_oBackBuffer.oBitmapInfo,
    DIB_RGB_COLORS, SRCCOPY);
}

void HandleGamepadInput()
{
  static DWORD s_ulPrevPacketNum = 0xFFFFFFFFul;

  DWORD dwResult;

  XINPUT_STATE state = {};

  // Simply get the state of the controller from XInput.
  dwResult = XInputGetState(0, &state);

  if (dwResult == ERROR_SUCCESS)
  {
    if (state.dwPacketNumber != s_ulPrevPacketNum)
    {
      g_oGameInput.XOffset = (float)state.Gamepad.sThumbLX / 32768;
      g_oGameInput.YOffset = (float)state.Gamepad.sThumbLY / 32768;

      XINPUT_VIBRATION oVibration = {};

      if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0)
      {
        oVibration.wLeftMotorSpeed = 65535 / 5;
        oVibration.wRightMotorSpeed = 65535 / 5;
      }

      XInputSetState(0, &oVibration);

      s_ulPrevPacketNum = state.dwPacketNumber;
    }
  }
  else
  {
    // Controller is not connected
  }
}

void SaveBitmap(const char* _aFileName, void* _pData, int _iWidth, int _iHeight)
{
  BITMAPFILEHEADER oFileHeader = {};
  oFileHeader.bfType = 0x4D42; // 'BM'
  oFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  oFileHeader.bfSize = oFileHeader.bfOffBits + (_iWidth * _iHeight * g_uBytesPerPixel);
  BITMAPINFOHEADER oInfoHeader = {};
  oInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
  oInfoHeader.biWidth = _iWidth;
  oInfoHeader.biHeight = -_iHeight; // Negative height for top-down bitmap
  oInfoHeader.biPlanes = 1;
  oInfoHeader.biBitCount = 32;
  oInfoHeader.biCompression = BI_RGB;
  HANDLE hFile = CreateFileA(_aFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile != INVALID_HANDLE_VALUE)
  {
    DWORD dwBytesWritten;
    WriteFile(hFile, &oFileHeader, sizeof(oFileHeader), &dwBytesWritten, NULL);
    WriteFile(hFile, &oInfoHeader, sizeof(oInfoHeader), &dwBytesWritten, NULL);
    WriteFile(hFile, _pData, _iWidth * _iHeight * g_uBytesPerPixel, &dwBytesWritten, NULL);
    CloseHandle(hFile);
  }
}

LRESULT WndProc(
  HWND _hWnd,
  UINT _uMsg,
  WPARAM _wParam,
  LPARAM _lParam)
{
  LRESULT result = 0;

  switch (_uMsg)
  {
  case WM_SIZE:
  {
    OutputDebugStringA("WM_SIZE\n");
  }
  break;
  case WM_DESTROY:
    OutputDebugStringA("WM_DESTROY\n");
    g_bRunning = false;
    break;
  case WM_CLOSE:
    OutputDebugStringA("WM_CLOSE\n");
    g_bRunning = false;
    break;
  case WM_ACTIVATEAPP:
    OutputDebugStringA("WM_ACTIVATEAPP\n");
    break;
  case WM_PAINT:
  {
    OutputDebugStringA("WM_PAINT\n");

    PAINTSTRUCT oPaintStruct = {};

    HDC hDeviceCtx = BeginPaint(_hWnd, &oPaintStruct);

    int iHeight = oPaintStruct.rcPaint.bottom - oPaintStruct.rcPaint.top;
    int iWidth = oPaintStruct.rcPaint.right - oPaintStruct.rcPaint.left;

    Win32DisplayBackBuffer(hDeviceCtx, iWidth, iHeight);

    EndPaint(_hWnd, &oPaintStruct);
  }break;
  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP:
  case WM_KEYDOWN:
  case WM_KEYUP:
  {
    uint32_t uVKCode = (uint32_t)_wParam;

    bool bWasDown = (_lParam & (1 << 30)) != 0;
    bool bIsDown = (_lParam & (1 << 31)) != 0;

    //bool bPress = bIsDown && !bWasDown;
    bool bRelease = !bIsDown && bWasDown;

    if (uVKCode == 'W')
    {
    }
    if (uVKCode == 'A')
    {
    }
    if (uVKCode == 'S')
    {
    }
    if (uVKCode == 'D')
    {
    }
    if (uVKCode == 'Q')
    {
    }
    if (uVKCode == 'E')
    {
    }
    if (uVKCode == VK_UP)
    {
    }
    if (uVKCode == VK_DOWN)
    {
    }
    if (uVKCode == VK_RIGHT)
    {
    }
    if (uVKCode == VK_UP)
    {
    }
    if (uVKCode == VK_ESCAPE)
    {
      if (bRelease)
      {
        g_bRunning = false;
      }
    }
    if (uVKCode == VK_SPACE)
    {
    }
  }
  break;
  default:
    result = DefWindowProc(_hWnd, _uMsg, _wParam, _lParam);
    break;
  }

  return result;
}

struct vec2
{
  float x;
  float y;
};

// The render is split into square tiles that worker threads pull from a shared counter.
// Keep the tile size a multiple of 16 px: with 4-byte pixels a tile row is then a whole
// number of 64-byte cache lines, so two tiles never write to the same line (no false sharing).
// -tile N overrides it for tiny benchmark images, where each pixel costs far more than any
// false sharing could.
static int g_iTileSize = 32;

// WaitForMultipleObjects can wait on at most this many handles
static constexpr int g_iMaxRenderThreads = MAXIMUM_WAIT_OBJECTS;

// Next tile to render. Every worker increments it, so it gets a cache line to itself
// instead of invalidating whatever globals the linker would otherwise place next to it.
struct alignas(64) TileQueue
{
  volatile LONG lNextTile;
  char aPadding[64 - sizeof(LONG)];
};

static TileQueue g_oTileQueue = {};

// Integer hash (lowbias32) to turn a tile index into a well-mixed RNG seed
static uint32_t HashTileIndex(uint32_t _uIdx)
{
  _uIdx ^= _uIdx >> 16;
  _uIdx *= 0x7feb352du;
  _uIdx ^= _uIdx >> 15;
  _uIdx *= 0x846ca68bu;
  _uIdx ^= _uIdx >> 16;
  return _uIdx;
}

DWORD WINAPI RenderWorkerThread(LPVOID /*lpParam*/)
{
  GameScreenBuffer oGameBuffer = {};
  oGameBuffer.pData = g_oBackBuffer.pData;
  oGameBuffer.iWidth = g_oBackBuffer.iWidth;
  oGameBuffer.iHeight = g_oBackBuffer.iHeight;

  const int iTilesX = (oGameBuffer.iWidth + g_iTileSize - 1) / g_iTileSize;
  const int iTilesY = (oGameBuffer.iHeight + g_iTileSize - 1) / g_iTileSize;
  const int iTileCount = iTilesX * iTilesY;

  while (true)
  {
    const int iTile = InterlockedIncrement(&g_oTileQueue.lNextTile) - 1;
    if (iTile >= iTileCount)
    {
      break;
    }

    const int iStartX = (iTile % iTilesX) * g_iTileSize;
    const int iStartY = (iTile / iTilesX) * g_iTileSize;
    const int iEndX = min(iStartX + g_iTileSize, oGameBuffer.iWidth);
    const int iEndY = min(iStartY + g_iTileSize, oGameBuffer.iHeight);

    // Seed per tile, not per thread, so the image is the same for any thread count or scheduling
    SeedRandom(HashTileIndex(static_cast<uint32_t>(iTile)));

    UpdateScreenBufferPartial(&oGameBuffer, iStartX, iStartY, iEndX, iEndY);
  }

  return 0;
}

// "-name N" on the command line, or _iDefault when it isn't there
static int ParseIntArg(const char* _pCmdLine, const char* _pName, int _iDefault)
{
  const char* pArg = _pCmdLine ? strstr(_pCmdLine, _pName) : nullptr;
  return pArg ? atoi(pArg + strlen(_pName)) : _iDefault;
}

// -threads N: number of render threads (default 1, 0 = all logical processors)
static int ParseThreadCount(const char* _pCmdLine)
{
  int iThreadCount = ParseIntArg(_pCmdLine, "-threads", 1);
  if (iThreadCount <= 0)
  {
    iThreadCount = static_cast<int>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
  }

  return max(1, min(iThreadCount, g_iMaxRenderThreads));
}

int WINAPI WinMain(
  HINSTANCE hInstance,
  HINSTANCE,
  LPSTR lpCmdLine,
  int
)
{
  // -bench: write draw time to drawtime.txt and quit as soon as the render finishes
  const bool bBenchMode = lpCmdLine && strstr(lpCmdLine, "-bench") != nullptr;

  // -nodisplay: don't refresh the window while rendering, only show the finished image
  const bool bLiveDisplay = !(lpCmdLine && strstr(lpCmdLine, "-nodisplay") != nullptr);

  const int iThreadCount = ParseThreadCount(lpCmdLine);

  // -spheres N: scene size. -width W -height H: render resolution, stretched to the window.
  // -tile N: tile size. Together they let bench.ps1 render scenes that don't fit in cache.
  const int iSphereCount = max(0, ParseIntArg(lpCmdLine, "-spheres", g_iDefaultSphereCount));
  const int iRenderWidth = max(1, ParseIntArg(lpCmdLine, "-width", g_iWindowWidth));
  const int iRenderHeight = max(1, ParseIntArg(lpCmdLine, "-height", g_iWindowHeight));
  g_iTileSize = max(1, ParseIntArg(lpCmdLine, "-tile", g_iTileSize));

  Win32LoadXInput();

  WNDCLASS oWndClass = {};
  oWndClass.style = CS_HREDRAW | CS_VREDRAW;
  oWndClass.lpfnWndProc = WndProc;
  oWndClass.cbClsExtra = 0;
  oWndClass.cbWndExtra = 0;
  oWndClass.hInstance = hInstance;
  oWndClass.hIcon = 0;
  oWndClass.hCursor = 0;
  oWndClass.hbrBackground = 0;
  oWndClass.lpszMenuName = NULL;
  oWndClass.lpszClassName = "HandmadeHeroWndClass";


  if (!RegisterClass(&oWndClass))
  {
    // ERROR: Window Class register failed
  }

  HWND hWnd = CreateWindowEx(0,
    oWndClass.lpszClassName,
    "CoolRayTracer",
    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    g_iWindowWidth,
    g_iWindowHeight,
    NULL,
    NULL,
    hInstance,
    NULL);

  if (!hWnd)
  {
    // ERROR: Window creation failed
  }

  Win32InitDirectSound(hWnd);

  UpdateSoundBuffer();

  g_pSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

  Win32ResizeDIBSection(iRenderWidth, iRenderHeight);

  InitGame(iSphereCount);

  LARGE_INTEGER ilPerfFrequency;
  QueryPerformanceFrequency(&ilPerfFrequency);

  LARGE_INTEGER ilBeginTime;
  QueryPerformanceCounter(&ilBeginTime);

  //DWORD64 ulBeginCycleCount = __rdtsc();

  GameScreenBuffer oGameBuffer = {};
  oGameBuffer.pData = g_oBackBuffer.pData;
  oGameBuffer.iWidth = g_oBackBuffer.iWidth;
  oGameBuffer.iHeight = g_oBackBuffer.iHeight;

  HANDLE hThreadArray[g_iMaxRenderThreads] = {};

  LARGE_INTEGER ilDrawStartTime;
  QueryPerformanceCounter(&ilDrawStartTime);

  for (int i = 0; i < iThreadCount; i++)
  {
    hThreadArray[i] = CreateThread(0, 0,
      RenderWorkerThread,
      nullptr,
      0, 0);
  }

  //UpdateGameBackBuffer(&oGameBuffer, g_oGameInput);

  bool bProcessRunning = true;  
  
  while (g_bRunning)
  {
    MSG msg;
    while (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
    {
      if (msg.message == WM_QUIT)
      {
        g_bRunning = false;
      }

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    // Sleep until the workers finish or ~16 ms pass, then repaint. Polling with a zero
    // timeout here would keep a whole core busy and slow the render threads down.
    DWORD ulWaitResult = WaitForMultipleObjects(iThreadCount, hThreadArray, TRUE, bProcessRunning ? 16 : 0);

    if (ulWaitResult == WAIT_TIMEOUT || ulWaitResult == WAIT_FAILED)
    {
      HandleGamepadInput();

      /*GameScreenBuffer oGameBuffer = {};
      oGameBuffer.pData = g_oBackBuffer.pData;
      oGameBuffer.iWidth = g_oBackBuffer.iWidth;
      oGameBuffer.iHeight = g_oBackBuffer.iHeight;

      UpdateGameBackBuffer(&oGameBuffer, g_oGameInput);*/

      UpdateSoundBuffer();      

      if (bLiveDisplay)
      {
        HDC hDeviceCtx = GetDC(hWnd);

        int iWidth, iHeight;
        GetWindowDimensions(hWnd, iWidth, iHeight);
        Win32DisplayBackBuffer(hDeviceCtx, iWidth, iHeight);

        ReleaseDC(hWnd, hDeviceCtx);
      }

     /* DWORD64 ulEndCycleCount = __rdtsc();
      DWORD64 ulElapsedCycleCount = ulEndCycleCount - ulBeginCycleCount;
      ulBeginCycleCount = ulEndCycleCount;*/

     /* LARGE_INTEGER ilEndTime;
      QueryPerformanceCounter(&ilEndTime);

      LONGLONG ilElapsedTime = ilEndTime.QuadPart - ilBeginTime.QuadPart;
      if (ilElapsedTime > 0)
      {
        ilElapsedTime = 1000 * ilElapsedTime / ilPerfFrequency.QuadPart;
      }

      LONGLONG ilFPS = 999999;

      if (ilElapsedTime > 0)
      {
        ilFPS = 1000 / ilElapsedTime;
      }*/

      //char aBuffer[256];
      //wsprintf(aBuffer, "FPS: %ld \t CPF: %ld\n", static_cast<long long>(ilFPS), static_cast<long long>(ulElapsedCycleCount / (1000 * 1000)));
      //OutputDebugStringA(aBuffer);

      //ilBeginTime = ilEndTime;    
    }
    else if(bProcessRunning)
    {
      bProcessRunning = false;

      LARGE_INTEGER ilDrawEndTime;
      QueryPerformanceCounter(&ilDrawEndTime);

      LONGLONG ilDrawElapsedTime = ilDrawEndTime.QuadPart - ilDrawStartTime.QuadPart;
      const double dDrawElapsedMs = 1000.0 * static_cast<double>(ilDrawElapsedTime) / static_cast<double>(ilPerfFrequency.QuadPart);

      char aBuffer[256];
      sprintf_s(aBuffer, "Draw Time: %.2f ms\n", dDrawElapsedMs);
      OutputDebugStringA(aBuffer);

      if (bBenchMode)
      {
        FILE* pFile = nullptr;
        if (fopen_s(&pFile, "drawtime.txt", "w") == 0 && pFile)
        {
          fprintf(pFile, "%.2f\n", dDrawElapsedMs);
          fclose(pFile);
        }
      }

      // Draw one last time to make sure we see the final result
      HDC hDeviceCtx = GetDC(hWnd);
      int iWidth, iHeight;
      GetWindowDimensions(hWnd, iWidth, iHeight);
      Win32DisplayBackBuffer(hDeviceCtx, iWidth, iHeight);

      SaveBitmap("output.bmp", g_oBackBuffer.pData, g_oBackBuffer.iWidth, g_oBackBuffer.iHeight);

      if (bBenchMode)
      {
        g_bRunning = false;
      }
    }
    else if (g_bRunning)
    {
      // Render finished: block until the next window message instead of spinning
      WaitMessage();
    }
  }

  for (int i = 0; i < iThreadCount; i++)
  {
    CloseHandle(hThreadArray[i]);
  }

  return 0;
}