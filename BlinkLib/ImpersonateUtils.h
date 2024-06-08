#pragma once
#include <windows.h>
#include <stdio.h>
#include <string>
#include <process.h>
#include <tlhelp32.h>

//https://github.com/Mr-Un1k0d3r/Elevate-System-Trusted-BOF/blob/main/elevate_x64.c

DWORD GetProcByName(LPWSTR name);
DWORD GetTrustedInstallerPID();
BOOL ImpersonateByPID(DWORD PID, HANDLE* hStorage);
BOOL ElevateSystem(HANDLE*);
BOOL ElevateTrustedInstaller(HANDLE*);

