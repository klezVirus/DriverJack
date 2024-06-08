#include "crypt.h"

UINT64 xor_encode(LPVOID address, UINT64 data_len, const char* key, DWORD key_len)
{
	for (int i = 0; i < data_len; i++) {
		*(char*)((UINT64)address + i) = (*(char*)((UINT64)address + i) ^ (unsigned char)key[i % key_len]);
	}
	return data_len;
}
