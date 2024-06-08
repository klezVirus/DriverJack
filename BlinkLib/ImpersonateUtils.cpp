#include "ImpersonateUtils.h"

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
