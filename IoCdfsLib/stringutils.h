#pragma once
#include <windows.h>
#include <string>
#include <ctime>

#ifndef STRINGUTILS_H
#define STRINGUTILS_H

LPCWSTR AnsiToWide(const char* ansiString);
DWORD ExtractNumberFromPath(PWSTR path);
std::string gen_random(const int len);
bool isValidFileName(const std::string& fileName);

#endif // !STRINGUTILS_H