// AMD_Compress_Test_Helpers.h : Defines the entry point for the console application.
//

#include "AMDCompress.h"

AMD_TC_FORMAT GetFormat(DWORD dwFourCC);
DWORD GetFourCC(AMD_TC_FORMAT nFormat);
bool IsDXT5SwizzledFormat(AMD_TC_FORMAT nFormat);
AMD_TC_FORMAT ParseFormat(TCHAR* pszFormat);
TCHAR* GetFormatDesc(AMD_TC_FORMAT nFormat);

bool LoadDDSFile(TCHAR* pszFile, AMD_TC_Texture& texture);
void SaveDDSFile(TCHAR* pszFile, AMD_TC_Texture& texture);
