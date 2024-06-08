#pragma once
#include <windows.h>
#include <stdio.h>

VOID ObtainBackupAndRestorePrivilege()
{
    HANDLE hToken;
    TOKEN_PRIVILEGES tp;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        fprintf(stderr, "Failed to open process token.\n");
        return;
    }

    // Set the privilege count
    tp.PrivilegeCount = 2;

    // Lookup the LUID for the backup privilege
    if (!LookupPrivilegeValue(NULL, SE_BACKUP_NAME, &tp.Privileges[0].Luid)) {
        fprintf(stderr, "Failed to lookup privilege value for SE_BACKUP_NAME.\n");
        CloseHandle(hToken);
        return;
    }

    // Enable the backup privilege
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Lookup the LUID for the restore privilege
    if (!LookupPrivilegeValue(NULL, SE_RESTORE_NAME, &tp.Privileges[1].Luid)) {
        fprintf(stderr, "Failed to lookup privilege value for SE_RESTORE_NAME.\n");
        CloseHandle(hToken);
        return;
    }

    // Enable the restore privilege
    tp.Privileges[1].Attributes = SE_PRIVILEGE_ENABLED;

    // Adjust the token privileges
    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
        fprintf(stderr, "Failed to adjust token privileges.\n");
    }

    // Check for errors from AdjustTokenPrivileges
    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        fprintf(stderr, "The token does not have one or more of the privileges.\n");
    }

    CloseHandle(hToken);
}




