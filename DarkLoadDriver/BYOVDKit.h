#pragma once
#include <windows.h>
#include <vector>

int ByovdKitExecuteWithArgs(const std::vector<wchar_t*>& args);
int processPIDByName(const WCHAR* name);
void printHelp();