#include <string>
#include <windows.h>

#ifndef CDFSLIB
#define CDFSLIB

bool DetatchVirtualImage(HANDLE virtualImageHandle);
PWSTR GetPhysicalDrivePath(HANDLE virtualDiskHandle);
DWORD ExtractNumberFromPath(PWSTR path);
HANDLE MountISO(const std::string& isoPath);


LPSTR WaitForDrive();
BOOL ReplaceFileWith(std::string& drivePath, char* file, char* replaceWith);
BOOL XorDecryptFile(std::string& drivePath, char* file, char* key);
VOID JustTamper(char* pDrivePath, char* filePath, char* action, char* replacePath);
VOID MountAndTamper(char* rawIso, char* innerFile, char* action, char* replacePath, BOOL detachOnFinish);
VOID WaitForDriveAndTamper(char* innerFile, char* action, char* replacePath, BOOL detachOnFinish);
VOID GetDevicePathFromDriveLetter(wchar_t driveLetter, LPWSTR devicePath, DWORD maxLength);
BOOL OverwriteFileWithBuffer(std::string& fullDrivePath, void* replaceBuffer, unsigned int replaceBufferLen);
BOOL CompareFileToBuffer(std::string& fullDrivePath, void* replaceBuffer, unsigned int replaceBufferLen);
UINT64 xor_encode(LPVOID address, UINT64 data_len, const char* key, DWORD key_len);

#endif // CDFSLIB