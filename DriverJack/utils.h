#pragma once
#include <Windows.h>
#include <vector>
#include <string>

std::vector<std::string> GetListOfDrives() {
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

LPCWSTR AnsiToWide(const char* ansiString) {
	int wideSize = MultiByteToWideChar(CP_ACP, 0, ansiString, -1, NULL, 0);
	LPWSTR wideString = new WCHAR[wideSize];
	MultiByteToWideChar(CP_ACP, 0, ansiString, -1, wideString, wideSize);
	return wideString;

}

DWORD ExtractNumberFromPath(PWSTR path) {
	DWORD number = 0;
	WCHAR* p = path;
	while (*p) {
		if (isdigit(*p)) {
			number = number * 10 + (*p - '0');
		}
		p++;
	}
	return number;
}