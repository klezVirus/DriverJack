#include "DaveLib.h"
#include <string>
#include <stdio.h>

int main(int argc, char** argv) {
    Process pProcess;
    HANDLE dirHandle;
    HANDLE hSymlink;
    DWORD pid;
    const wchar_t* dirName = L"\\??\\glbl";
    const wchar_t* linkName = L"C:";
    const wchar_t* targetName = L"\\Device\\CdRom0";

    LoadApis();

	InitializeProcess(&pProcess, L"C:\\Windows\\System32\\cmd.exe", L"", L"C:\\", TRUE, NULL);

    if (CreateObjectDirectory(dirName, &dirHandle) != 0) {
        if (OpenObjectDirectory(dirName, &dirHandle) != 0) {
            return 1;
        }
    }

    if (SetProcessDevMap(pProcess.hProcess, dirHandle) != 0) {
        return 1;
    }

	ResumeProcess(&pProcess);

    if (!DirectoryExists(L"E:\\")) {
        printf("[!] Error: Directory E:\\ does not exist for us to target\n");
        return 5;
    }

    if (CreateNtSymLink(dirHandle, linkName, targetName, &hSymlink) != 0) {
        return 1;
    }

    DirList(L"C:\\");

    printf("[*] All Done, Hit Enter To Remove Symlink\n");
    getchar();

    CloseHandle(dirHandle);

    printf("[*] Returning ProcessDeviceMap to \\??\n");

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