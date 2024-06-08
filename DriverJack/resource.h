//
// Microsoft Visual C++ generated include file.
// Used by EmbedResources.rc
//
#include <windows.h>

#define IDR_ISO		                        101
#define IDR_KDU_EXE                         102
#define IDR_KDU_DLL                         103
#define IDR_DRVX    					    104

// Next default values for new objects
// 
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        103
#define _APS_NEXT_COMMAND_VALUE         40001
#define _APS_NEXT_CONTROL_VALUE         1001
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif


BOOL GetCurrentDirectoryW(LPWSTR buffer, DWORD size);
BOOL FileExists(LPCWSTR szPath);
LPWSTR UnpackFileFromResource(wchar_t* szFileName, int resourceId);
VOID* LoadResourceInMemory(int* payload_len, int resourceId);
