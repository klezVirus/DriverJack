#include <string>
#include "ServiceUtils.h"

bool DetatchVirtualImage(HANDLE virtualImageHandle);
PWSTR GetPhysicalDrivePath(HANDLE virtualDiskHandle);
DWORD ExtractNumberFromPath(PWSTR path);
HANDLE MountISO(const std::wstring& isoPath);

UINT64 xor_encode(LPVOID address, UINT64 data_len)
{
    const char* key = "Microsoft";
    int key_len = 9;

    for (int i = 0; i < data_len; i++) {
        *(char*)((UINT64)address + i) = (*(char*)((UINT64)address + i) ^ (unsigned char)key[i % key_len]);
    }
    return data_len;
}