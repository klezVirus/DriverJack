#pragma once
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif

#ifdef NTDDI_VERSION
#undef NTDDI_VERSION
#define NTDDI_VERSION 0x05000000
#endif

#if defined (DEBUG)
static void DebugPrint(LPCWSTR pszMsg, DWORD dwErr);
#define DEBUG_PRINT(pszMsg, dwErr) DebugPrint(pszMsg, dwErr)
#else
#define DEBUG_PRINT(pszMsg, dwErr) NULL
#endif

#include <string>

void ManageDriveLetter(const std::wstring& driveLetter, const std::wstring& newDriveLetter, const std::wstring& ntDevice, bool removeDriveLetter);
