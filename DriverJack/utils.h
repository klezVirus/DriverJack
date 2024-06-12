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

BOOLEAN QueryHvciInfo(
    _Out_ PBOOLEAN pbHVCIEnabled,
    _Out_ PBOOLEAN pbHVCIStrictMode,
    _Out_ PBOOLEAN pbHVCIIUMEnabled
)
{
    BOOLEAN hvciEnabled;
    ULONG returnLength;
    NTSTATUS ntStatus;
    SYSTEM_CODEINTEGRITY_INFORMATION ci;

    if (pbHVCIEnabled) *pbHVCIEnabled = FALSE;
    if (pbHVCIStrictMode) *pbHVCIStrictMode = FALSE;
    if (pbHVCIIUMEnabled) *pbHVCIIUMEnabled = FALSE;

    ci.Length = sizeof(ci);

    ntStatus = NtQuerySystemInformation(
        SystemCodeIntegrityInformation,
        &ci,
        sizeof(ci),
        &returnLength);

    if (NT_SUCCESS(ntStatus)) {

        hvciEnabled = ((ci.CodeIntegrityOptions & CODEINTEGRITY_OPTION_ENABLED) &&
            (ci.CodeIntegrityOptions & CODEINTEGRITY_OPTION_HVCI_KMCI_ENABLED));

        if (pbHVCIEnabled)
            *pbHVCIEnabled = hvciEnabled;

        if (pbHVCIStrictMode)
            *pbHVCIStrictMode = (hvciEnabled == TRUE) &&
            (ci.CodeIntegrityOptions & CODEINTEGRITY_OPTION_HVCI_KMCI_STRICTMODE_ENABLED);

        if (pbHVCIIUMEnabled)
            *pbHVCIIUMEnabled = (ci.CodeIntegrityOptions & CODEINTEGRITY_OPTION_HVCI_IUM_ENABLED) > 0;

        return TRUE;
    }

    return FALSE;
}