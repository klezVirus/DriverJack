#include <Windows.h>
#include <stdio.h>
#include "DriveSwap.h"


#pragma warning (disable : 4800)


void ManageDriveLetter(const std::wstring& driveLetter, const std::wstring& newDriveLetter, const std::wstring& ntDevice, bool removeDriveLetter)
{
    if (driveLetter.empty() || newDriveLetter.empty())
    {
        wprintf(L"Drive letter cannot be empty\n");
        return;
    }

    WCHAR szUniqueVolumeName[MAX_PATH];
    WCHAR drive = driveLetter[0];
    std::wstring driveLetterAndSlash = std::wstring(1, drive) + L":\\";
    std::wstring driveLetterColon = std::wstring(1, drive) + L":";

    WCHAR newDrive = newDriveLetter[0];

    std::wstring newDriveLetterColon = std::wstring(1, newDrive) + L":";
    std::wstring newDriveLetterAndSlash = std::wstring(1, newDrive) + L":\\";

    printf("  Drive letter: %wc\n", drive);
    printf("  New Drive letter: %wc\n", newDrive);
    printf("  NT Device: %ws\n", ntDevice.c_str());

    BOOL fResult;

    if (removeDriveLetter)
    {
        fResult = DeleteVolumeMountPoint(driveLetterAndSlash.c_str());

        if (!fResult)
            wprintf(L"(-) error %lu: couldn't remove %s\n", GetLastError(), driveLetterAndSlash.c_str());
    }

    fResult = DefineDosDevice(DDD_RAW_TARGET_PATH, newDriveLetterColon.c_str(), ntDevice.c_str());

    if (fResult)
    {
        if (!GetVolumeNameForVolumeMountPoint(newDriveLetterAndSlash.c_str(), szUniqueVolumeName, MAX_PATH))
        {
            DEBUG_PRINT(L"GetVolumeNameForVolumeMountPoint failed", GetLastError());
            szUniqueVolumeName[0] = L'\0';
        }

        fResult = DefineDosDevice(DDD_RAW_TARGET_PATH | DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE, newDriveLetterColon.c_str(), ntDevice.c_str());

        if (!fResult)
            DEBUG_PRINT(L"DefineDosDevice failed", GetLastError());

        fResult = SetVolumeMountPoint(newDriveLetterAndSlash.c_str(), szUniqueVolumeName);

        if (!fResult)
            wprintf(L"error %lu: could not add %s\n", GetLastError(), newDriveLetterAndSlash.c_str());
    }
}

#if defined (DEBUG)
void DebugPrint(LPCWSTR pszMsg, DWORD dwErr)
{
    if (dwErr)
        wprintf(L"%s: %lu\n", pszMsg, dwErr);
    else
        wprintf(L"%s\n", pszMsg);
}
#endif
