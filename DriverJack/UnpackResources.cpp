#include "common.h"
#include "resource.h"

// Function to get the current directory
BOOL GetCurrentDirectoryW(LPWSTR buffer, DWORD size) {
    if (GetCurrentDirectory(size, buffer)) {
        return TRUE;
    }
    else {
        wprintf(L"GetCurrentDirectory failed (%d).\n", GetLastError());
        return FALSE;
    }
}

// Function to check if a file exists
BOOL FileExists(LPCWSTR szPath) {
    DWORD dwAttrib = GetFileAttributes(szPath);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
        !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

LPWSTR UnpackFileFromResource(wchar_t* szFileName, int resourceId) {
    wchar_t szCurrentDirectory[MAX_PATH];
    wchar_t* szFullPath = (wchar_t*)malloc(MAX_PATH*sizeof(wchar_t));
    HANDLE hFile;
	VOID* payload;
	int payload_len;
	if (szFullPath == NULL) {
		wprintf(L"Failed to allocate memory for szFullPath.\n");
		return nullptr;
	}

    if (GetCurrentDirectoryW(szCurrentDirectory, MAX_PATH)) {
        // Concatenate the current directory with the provided file name
        swprintf(szFullPath, MAX_PATH, L"%s\\%s", szCurrentDirectory, (LPWSTR)szFileName);

        // Check if the file exists
        if (!FileExists(szFullPath)) {
            hFile = CreateFile(
                szFullPath,             // name of the write
                GENERIC_WRITE,          // open for writing
                0,                      // do not share
                NULL,                   // default security
                CREATE_NEW,             // create new file only
                FILE_ATTRIBUTE_NORMAL,  // normal file
                NULL);                  // no attr. template

            if (hFile == INVALID_HANDLE_VALUE) {
                wprintf(L"Failed to create file (%d).\n", GetLastError());
                return nullptr;
            }
            payload = LoadResourceInMemory(&payload_len, resourceId);

            if (!WriteFile(hFile, payload, payload_len, NULL, NULL)) {
                wprintf(L"Failed to write to file (%d).\n", GetLastError());
                return nullptr;
            }

            wprintf(L"File created successfully.\n");
			if (payload != NULL)
                free(payload);
			if (hFile != INVALID_HANDLE_VALUE)
                CloseHandle(hFile);
        }
        else {
            wprintf(L"File does exist already: %s\n", szFullPath);
        }
    } else {
        wprintf(L"Failed to get current directory.\n");
    }

    return (LPWSTR)szFullPath;
}


VOID* LoadResourceInMemory(int* length, int resourceId)
{
	HRSRC res;
	HGLOBAL resHandle = NULL;
	unsigned char* payload;

	// Extract payload from resources section
	res = FindResourceW(NULL, MAKEINTRESOURCE(resourceId), RT_RCDATA);
	if (res == NULL) {
		wprintf(L"FindResource failed (%d).\n", GetLastError());
		return nullptr;
	}
	resHandle = LoadResource(NULL, res);
	if (resHandle == NULL) {
		wprintf(L"LoadResource failed (%d).\n", GetLastError());
		return nullptr;
	}
	payload = (unsigned char*)LockResource(resHandle);
	*length = SizeofResource(NULL, res);
	int resSize = *length;
	unsigned char* array = (unsigned char*)malloc(sizeof(unsigned char) * resSize * 2);
	if (array == NULL) {
		wprintf(L"Failed to allocate memory for array.\n");
		return nullptr;
	}
	memcpy(array, payload, resSize);

	// We finished copy our payload, we can free the resource now
	FreeResource(resHandle);
	return array;
}
