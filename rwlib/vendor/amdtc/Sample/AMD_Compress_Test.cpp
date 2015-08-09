// AMD_Compress_Test.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <tchar.h>
#include <assert.h>
#include "ddraw.h"
#include "d3d9types.h"
#include "AMDCompress.h"
#include "AMD_Compress_Test_Helpers.h"

// Example linking for using AMDCompress_Mx DLL's
#ifdef  BUILD_MT_DLL
#pragma comment(lib,"AMDCompress_MT_DLL.lib")
#endif

#ifdef  BUILD_MD_DLL
#pragma comment(lib,"AMDCompress_MD_DLL.lib")
#endif

// Example static linking for AMDCompress_Mx libs
#ifdef  BUILD_MT
#pragma comment(lib,"AMDCompress_MT.lib")
#endif

#ifdef  BUILD MD
#pragma comment(lib,"AMDCompress_MD.lib")
#endif


int _tmain(int argc, _TCHAR* argv[])
{
   if(argc < 5)
   {
      _tprintf(_T("AMD_Compress_Test SourceFile DestFile Format Speed\n"));
      return 0;
   }

   // Note: No error checking is done on user arguments, so all parameters must be correct in this example
   // (files must exist, values correct format, etc..)
   TCHAR* pszSourceFile			= argv[1];
   TCHAR* pszDestFile			= argv[2];
   AMD_TC_FORMAT destFormat		= ParseFormat(argv[3]);
   int nCompressionSpeed		= _tstoi(argv[4]);

   if(destFormat == AMD_TC_FORMAT_Unknown)
   {
      _tprintf(_T("Unsupported dest format\n"));
      return 0;
   }

   // Load the source texture
   AMD_TC_Texture srcTexture;
   if(!LoadDDSFile(pszSourceFile, srcTexture))
   {
      _tprintf(_T("Unsupported dest format\n"));
      return 0;
   }

   // Init dest texture
   AMD_TC_Texture destTexture;
   destTexture.dwSize		= sizeof(destTexture);
   destTexture.dwWidth		= srcTexture.dwWidth;
   destTexture.dwHeight		= srcTexture.dwHeight;
   destTexture.dwPitch		= 0;
   destTexture.format		= destFormat;
   destTexture.dwDataSize	= AMD_TC_CalculateBufferSize(&destTexture);
   destTexture.pData		= (AMD_TC_BYTE*) malloc(destTexture.dwDataSize);

   AMD_TC_CompressOptions options;
   memset(&options, 0, sizeof(options));
   options.dwSize				= sizeof(options);
   options.nCompressionSpeed	= (AMD_TC_Speed) nCompressionSpeed;

   AMD_TC_ConvertTexture(&srcTexture, &destTexture, &options, NULL, NULL, NULL);

   SaveDDSFile(pszDestFile, destTexture);

   free(srcTexture.pData);
   free(destTexture.pData);
      
   return 0;
}
