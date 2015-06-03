// ATI_Compress_BenchMark.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <tchar.h>
#include <assert.h>
#include "ddraw.h"
#include "d3d9types.h"
#include "ATI_Compress.h"
#include "../ATI_Compress_Test/ATI_Compress_Test_Helpers.h"

#if defined(WIN32) || defined(_WIN64)
#define THREADED_COMPRESS 1
#define MAX_THREADS 64
static DWORD GetProcessorCount()
{
   SYSTEM_INFO systemInfo;
   GetSystemInfo(&systemInfo);
   return systemInfo.dwNumberOfProcessors;
}
#else // !WINDOWS
static DWORD GetProcessorCount()
{
   return 1;
}
#endif // !WINDOWS

int _tmain(int argc, _TCHAR* argv[])
{
   if(argc < 5)
   {
      _tprintf(_T("ATI_Compress_BenchMark File Format Iteration Speed [SingleThreaded]\n"));
      return 0;
   }

   TCHAR* pszSourceFile = argv[1];
   ATI_TC_FORMAT destFormat = ParseFormat(argv[2]);
   int nIterations = max(_tstoi(argv[3]), 1);
   int nCompressionSpeed = _tstoi(argv[4]);
   BOOL bSingleThreaded = ((argc >= 6) && (_tstoi(argv[5]) > 0));

   // Load the source texture
   ATI_TC_Texture srcTexture;
   if(!LoadDDSFile(pszSourceFile, srcTexture))
      return 0;

   // Init dest texture
   ATI_TC_Texture destTexture;
   destTexture.dwSize = sizeof(destTexture);
   destTexture.dwWidth = srcTexture.dwWidth;
   destTexture.dwHeight = srcTexture.dwHeight;
   destTexture.dwPitch = 0;
   destTexture.format = destFormat;
   destTexture.dwDataSize = ATI_TC_CalculateBufferSize(&destTexture);
   destTexture.pData = (ATI_TC_BYTE*) malloc(destTexture.dwDataSize);

   ATI_TC_CompressOptions options;
   memset(&options, 0, sizeof(options));
   options.dwSize = sizeof(options);
   options.nCompressionSpeed = (ATI_TC_Speed) nCompressionSpeed;
   options.bDisableMultiThreading = bSingleThreaded;

   LARGE_INTEGER frequency, loopStartTime, loopEndTime;
   QueryPerformanceFrequency(&frequency);
   QueryPerformanceCounter(&loopStartTime);

   for(int i = 0; i < nIterations; i++)
      ATI_TC_ConvertTexture(&srcTexture, &destTexture, &options, NULL, NULL, NULL);

   QueryPerformanceCounter(&loopEndTime);
   LONGLONG Duration = loopEndTime.QuadPart - loopStartTime.QuadPart;
   double fDuration = (((double) Duration) / ((double) frequency.QuadPart));
   double fNumPerSec = ((double) nIterations) / fDuration;
   double fThroughPut = (srcTexture.dwWidth * srcTexture.dwHeight * sizeof(DWORD) * fNumPerSec) / (1024 * 1024);
   double fMPixelsSec = (srcTexture.dwWidth * srcTexture.dwHeight * fNumPerSec) / (1024 * 1024);
   DWORD dwCoreCount = bSingleThreaded ? 1 : GetProcessorCount();

   TCHAR szResults[256];
   _stprintf(szResults, _T("Compressed to %s, %i iterations in %f seconds, %f per second.\nData Throughput = %f MB\\sec (%f MB\\core\\sec)\nPixel Throughput = %f MPixel\\sec (%f MPixel\\core\\sec)\n"),
      GetFormatDesc(destFormat), nIterations, fDuration, fNumPerSec, fThroughPut, fThroughPut/dwCoreCount, fMPixelsSec, fMPixelsSec/dwCoreCount);
   _tprintf(szResults);
   OutputDebugString(szResults);

   free(srcTexture.pData);
   free(destTexture.pData);

   return 0;
}
