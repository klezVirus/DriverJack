#ifndef DAVELIB_H
#define DAVELIB_H

#include <windows.h>
#include <winternl.h>
#include <string>

#define DLLEXPORT extern "C" __declspec(dllexport)

#define SYMBOLIC_LINK_ALL_ACCESS 0xF0001
#define DIRECTORY_ALL_ACCESS 0xF000F
#define ProcessDeviceMap 23

typedef struct {
    HANDLE hProcess;
    HANDLE hThread;
} Process;

typedef NTSYSAPI NTSTATUS(*_NtSetInformationProcess)(HANDLE ProcessHandle, PROCESS_INFORMATION_CLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength);
typedef NTSYSAPI VOID(*_RtlInitUnicodeString)(PUNICODE_STRING DestinationString, PCWSTR SourceString);
typedef NTSYSAPI NTSTATUS(*_NtCreateSymbolicLinkObject)(PHANDLE pHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PUNICODE_STRING DestinationName);
typedef NTSYSAPI NTSTATUS(*_NtCreateDirectoryObject)(PHANDLE DirectoryHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes);
typedef NTSYSAPI NTSTATUS(*_NtOpenDirectoryObject)(PHANDLE DirectoryObjectHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes);
typedef NTSYSAPI NTSTATUS(*_NtQuerySymbolicLinkObject)(HANDLE LinkHandle, PUNICODE_STRING LinkTarget, PULONG ReturnedLength);
typedef NTSYSAPI NTSTATUS(*_NtOpenSymbolicLinkObject)(PHANDLE LinkHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes);
typedef NTSYSAPI NTSTATUS(*_NtQueryDirectoryObject)(HANDLE DirectoryHandle, PVOID Buffer, ULONG Length, BOOLEAN ReturnSingleEntry, BOOLEAN RestartScan, PULONG Context, PULONG ReturnLength);
typedef NTSYSAPI NTSTATUS(*_NtMakeTemporaryObject)(HANDLE ObjectHandle);

void LoadApis(void);

DLLEXPORT int CreateCRedirectedCallback(const wchar_t* redirectTo, const wchar_t* ensurePath, LPVOID callback);
DLLEXPORT int CreateGlobalCRedirectedCallback(const wchar_t* redirectTo, const wchar_t* ensurePath, LPVOID callback, LPVOID elevateCallback);
DLLEXPORT int CreateCRedirectedProcess(const wchar_t* executable, const wchar_t* args, const wchar_t* currDir, int suspended, const wchar_t* redirectTo, const wchar_t* ensurePath);


int InitializeProcess(Process* proc, const wchar_t* executable, const wchar_t* args, const wchar_t* currDir, int suspended, HANDLE token);
void ResumeProcess(Process* proc);
void CleanupProcess(Process* proc);

int OpenProcessByPid(DWORD pid, HANDLE* processHandle);
int OpenObjectDirectory(const wchar_t* directoryName, HANDLE* dirHandle);
int CreateObjectDirectory(const wchar_t* directoryName, HANDLE* dirHandle);
int SetProcessDevMap(HANDLE processHandle, HANDLE dirHandle);
int CreateNtSymLink(HANDLE dirHandle, const wchar_t* linkName, const wchar_t* targetName, PHANDLE symlinkHandle);
BOOL DirectoryExists(const wchar_t* szPath);

NTSTATUS ChangeSymlink(LPWSTR symLinkName, LPWSTR newDestination);
LPWSTR GetSymbolicLinkTarget(LPWSTR symLinkName);

#endif // DAVELIB_H
