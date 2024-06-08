/** @file     reparselib.cpp
 *  @brief    A library for working with NTFS Reparse Points
 *  @author   amdf
 *  @version  0.1
 *  @date     May 2011
 */

#include <windows.h>
#include <ks.h> // because of GUID_NULL
#include "ReparseLib.h"
#include <WinIoCtl.h>

 /**
  *@brief Get restore privilege in case we don't have it.
  *@param [in] bReadWrite Read and write? (if not, will use GENERIC_READ)
  */
VOID ObtainRestorePrivilege(IN BOOL bReadWrite)
{
    HANDLE hToken; TOKEN_PRIVILEGES tp;
    OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken);
    LookupPrivilegeValue(NULL,
        (bReadWrite ? SE_RESTORE_NAME : SE_BACKUP_NAME),
        &tp.Privileges[0].Luid);
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
    CloseHandle(hToken);
}

HANDLE OpenFileForWrite(IN LPCWSTR sFileName, IN BOOL bBackup)
{
    if (bBackup)
    {
        ObtainRestorePrivilege(TRUE);
    }
    return CreateFile(
        sFileName, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
        (bBackup)
        ? FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS
        : FILE_FLAG_OPEN_REPARSE_POINT, 0);
}

HANDLE OpenFileForRead(IN LPCWSTR sFileName, IN BOOL bBackup)
{
    if (bBackup)
    {
        ObtainRestorePrivilege(FALSE);
    }
    return CreateFile(
        sFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        (bBackup)
        ? FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS
        : FILE_FLAG_OPEN_REPARSE_POINT, 0);
}

/**
 *  @brief      Checks an existence of a Reparse Point of a specified file
 *  @param[in]  sFileName File name
 *  @return     TRUE if exists, FALSE otherwise
 */
REPARSELIB_API BOOL ReparsePointExists(IN LPCWSTR sFileName)
{
    return (GetFileAttributes(sFileName) & FILE_ATTRIBUTE_REPARSE_POINT);
}

/**
 *  @brief      Get a reparse point buffer
 *  @param[in]  sFileName File name
 *  @param[out] pBuf Caller allocated buffer (16 Kb minimum)
 *  @return     TRUE if success
 */
REPARSELIB_API BOOL GetReparseBuffer(IN LPCWSTR sFileName, OUT PREPARSE_GUID_DATA_BUFFER pBuf)
{
    DWORD dwRet;
    HANDLE hSrc;
    BOOL bResult = FALSE;

    if (NULL == pBuf)
    {
        return bResult;
    }

    if (!ReparsePointExists(sFileName))
    {
        return bResult;
    }

    hSrc = OpenFileForRead(sFileName,
        (GetFileAttributes(sFileName) & FILE_ATTRIBUTE_DIRECTORY));

    if (hSrc == INVALID_HANDLE_VALUE)
    {
        return bResult;
    }

    if (DeviceIoControl(hSrc, FSCTL_GET_REPARSE_POINT,
        NULL, 0, pBuf, MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &dwRet, NULL))
    {
        bResult = TRUE;
    }

    CloseHandle(hSrc);

    return bResult;
}

/**
 *  @brief      Get a GUID field of a reparse point
 *  @param[in]  sFileName File name
 *  @param[out] pGuid Pointer to GUID
 *  @return     TRUE if success
 */
REPARSELIB_API BOOL GetReparseGUID(IN LPCWSTR sFileName, OUT GUID* pGuid)
{
    BOOL bResult = FALSE;

    if (NULL == pGuid)
    {
        return FALSE;
    }

    if (!ReparsePointExists(sFileName))
    {
        return FALSE;
    }

    PREPARSE_GUID_DATA_BUFFER rd
        = (PREPARSE_GUID_DATA_BUFFER)GlobalAlloc(GPTR, MAXIMUM_REPARSE_DATA_BUFFER_SIZE);

    if (GetReparseBuffer(sFileName, rd))
    {
        *pGuid = rd->ReparseGuid;
        bResult = TRUE;
    }

    GlobalFree(rd);

    return bResult;
}

/**
 *  @brief      Get a reparse tag of a reparse point
 *  @param[in]  sFileName File name
 *  @param[out] pTag Pointer to reparse tag
 *  @return     Reparse tag or 0 if fails
 */
REPARSELIB_API BOOL GetReparseTag(IN LPCWSTR sFileName, OUT DWORD* pTag)
{
    BOOL bResult = FALSE;

    if (NULL == pTag)
    {
        return FALSE;
    }

    if (!ReparsePointExists(sFileName))
    {
        return FALSE;
    }

    PREPARSE_GUID_DATA_BUFFER rd
        = (PREPARSE_GUID_DATA_BUFFER)GlobalAlloc(GPTR, MAXIMUM_REPARSE_DATA_BUFFER_SIZE);

    if (GetReparseBuffer(sFileName, rd))
    {
        *pTag = rd->ReparseTag;
        bResult = TRUE;
    }

    GlobalFree(rd);

    return bResult;
}

/**
 *  @brief      Delete a reparse point
 *  @param[in]  sFileName File name
 *  @return     TRUE if success, FALSE otherwise
 */
REPARSELIB_API BOOL DeleteReparsePoint(IN LPCWSTR sFileName)
{
    PREPARSE_GUID_DATA_BUFFER rd;
    BOOL bResult;
    GUID gu;
    DWORD dwRet, dwReparseTag;

    if (!ReparsePointExists(sFileName) || !GetReparseGUID(sFileName, &gu))
    {
        return FALSE;
    }

    rd = (PREPARSE_GUID_DATA_BUFFER)
        GlobalAlloc(GPTR, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE);

    if (GetReparseTag(sFileName, &dwReparseTag))
    {
        rd->ReparseTag = dwReparseTag;
    }
    else
    {
        //! The routine cannot delete a reparse point without determining it's reparse tag
        GlobalFree(rd);
        return FALSE;
    }

    HANDLE hDel = OpenFileForWrite(sFileName,
        (GetFileAttributes(sFileName) & FILE_ATTRIBUTE_DIRECTORY));

    if (hDel == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }

    // Try to delete a system type of the reparse point (without GUID)
    bResult = DeviceIoControl(hDel, FSCTL_DELETE_REPARSE_POINT,
        rd, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE, NULL, 0,
        &dwRet, NULL);

    if (!bResult)
    {
        // Set up the GUID
        rd->ReparseGuid = gu;

        // Try to delete with GUID
        bResult = DeviceIoControl(hDel, FSCTL_DELETE_REPARSE_POINT,
            rd, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE, NULL, 0,
            &dwRet, NULL);
    }

    GlobalFree(rd);
    CloseHandle(hDel);
    return bResult;
}

/**
 *  @brief      Creates a custom reparse point
 *  @param[in]  sFileName   File name
 *  @param[in]  pBuffer     Reparse point content
 *  @param[in]  uBufSize    Size of the content
 *  @param[in]  uGuid       Reparse point GUID
 *  @param[in]  uReparseTag Reparse point tag
 *  @return     TRUE if success, FALSE otherwise
 */
REPARSELIB_API BOOL CreateCustomReparsePoint
(
    IN LPCWSTR  sFileName,
    IN PVOID    pBuffer,
    IN UINT     uBufSize,
    IN GUID     uGuid,
    IN ULONG    uReparseTag
)
{
    DWORD dwLen = 0;
    BOOL bResult = FALSE;

    if (NULL == pBuffer || 0 == uBufSize || uBufSize > MAXIMUM_REPARSE_DATA_BUFFER_SIZE)
    {
        return bResult;
    }

    HANDLE hHandle = OpenFileForWrite(sFileName,
        (GetFileAttributes(sFileName) & FILE_ATTRIBUTE_DIRECTORY));

    if (INVALID_HANDLE_VALUE == hHandle)
    {
        return bResult;
    }

    PREPARSE_GUID_DATA_BUFFER rd
        = (PREPARSE_GUID_DATA_BUFFER)GlobalAlloc(GPTR, MAXIMUM_REPARSE_DATA_BUFFER_SIZE);

    rd->ReparseTag = uReparseTag;
    rd->ReparseDataLength = uBufSize;
    rd->Reserved = 0;
    rd->ReparseGuid = uGuid;

    memcpy(rd->GenericReparseBuffer.DataBuffer, pBuffer, uBufSize);

    if (DeviceIoControl(hHandle, FSCTL_SET_REPARSE_POINT, rd,
        rd->ReparseDataLength + REPARSE_GUID_DATA_BUFFER_HEADER_SIZE,
        NULL, 0, &dwLen, NULL))
    {
        bResult = TRUE;
    }

    CloseHandle(hHandle);
    GlobalFree(rd);

    return bResult;
}