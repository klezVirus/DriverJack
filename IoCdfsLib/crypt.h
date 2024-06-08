#pragma once
#include <windows.h>

UINT64 xor_encode(LPVOID address, UINT64 data_len, const char* key, DWORD key_len);
