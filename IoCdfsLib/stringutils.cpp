#include "stringutils.h"

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

std::string gen_random(const int len) {
	static const char alphanum[] =
		"abcdefghijklmnopqrstuvwxyz";
	std::string tmp_s;
	tmp_s.reserve(len);

	for (int i = 0; i < len; ++i) {
		tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
	}

	return tmp_s;
}

bool isValidFileName(const std::string& fileName) {
	return !fileName.empty() && fileName.find("/") == std::string::npos && fileName.find("\\") == std::string::npos;
}
