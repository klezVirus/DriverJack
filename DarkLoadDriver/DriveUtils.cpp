#include <windows.h>
#include <string>

bool BackupDriver(const char* driverName) {
    std::string sourceFilePath = "C:\\Windows\\system32\\drivers\\";
    sourceFilePath.append(driverName);
    sourceFilePath.append(".sys");
    std::string backupFilePath = "C:\\Windows\\TEMP\\";
    backupFilePath.append(driverName);
    backupFilePath.append(".sys");

    printf("(*) Backing up driver: %s -> %s\n", sourceFilePath.c_str(), backupFilePath.c_str());

    return CopyFileA(sourceFilePath.c_str(), backupFilePath.c_str(), FALSE) != 0;
}

bool RestoreDriver(const char* driverName) {
    std::string destinationFilePath = "C:\\Windows\\system32\\drivers\\";
    destinationFilePath.append(driverName);
    destinationFilePath.append(".sys");
    std::string backupFilePath = "C:\\Windows\\TEMP\\";
    backupFilePath.append(driverName);
    backupFilePath.append(".sys");

    printf("(*) Restoring driver: %s -> %s\n", backupFilePath.c_str(), destinationFilePath.c_str());

    return CopyFileA(backupFilePath.c_str(), destinationFilePath.c_str(), FALSE) != 0;
}
