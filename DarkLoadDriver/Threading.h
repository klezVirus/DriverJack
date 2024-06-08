#pragma once

// For Multithreading

typedef struct _IO_THREAD_PARAM
{
	LPCWSTR             lpwServiceName;
	LPCSTR				lpSymlinkFileName;
	LPCSTR				lpTargetFileName;
} IO_THREAD_PARAM, * PIO_THREAD_PARAM;


DWORD CheckProcessors(void);

DWORD WINAPI StartServiceThread(LPVOID lpParam);
DWORD WINAPI CreateSymlinkThread(LPVOID IoThreadParam);
void DriverRace(LPCWSTR lpwServiceName, LPCSTR lpSymlinkFileName, LPCSTR lpTargetFileName);