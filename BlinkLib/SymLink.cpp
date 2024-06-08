/** @file     symlink.cpp
 *  @brief    Symbolic link and junction point routines
 *  @author   amdf
 *  @version  0.1
 *  @date     July 2011
 */

#include <windows.h>
#include <ks.h> // because of GUID_NULL
#include <string> //wmemcpy
#include "ReparseLib.h"


static BOOL CreateLinkInternal(LPCWSTR sLinkName, LPCWSTR sPrintName, LPCWSTR sSubstituteName,
    BOOL bRelative, BOOL bJunction);
static BOOL FillREPARSE_DATA_BUFFER(PREPARSE_DATA_BUFFER pReparse, LPCWSTR sPrintName,
    USHORT uPrintNameLength, LPCWSTR sSubstituteName, USHORT uSubstituteNameLength);

REPARSELIB_API BOOL CreateJunction(IN LPCWSTR sLinkName, IN LPCWSTR sPrintName, IN LPCWSTR sSubstituteName)
{
    return CreateLinkInternal(sLinkName, sPrintName, sSubstituteName, FALSE, TRUE);
}

REPARSELIB_API BOOL CreateSymlink(
    IN LPCWSTR sLinkName, IN LPCWSTR sPrintName, IN LPCWSTR sSubstituteName, IN BOOL bRelative)
{
    return CreateLinkInternal(sLinkName, sPrintName, sSubstituteName, bRelative, FALSE);
}

/**
 *  @brief      Creates a symbolic link or a junction point.
 *              administrator access + SE_CREATE_SYMBOLIC_LINK_NAME privilege required
 *  @param[in]  sLinkName       Link file name
 *  @param[in]  sPrintName      Target print name
 *  @param[in]  sSubstituteName Target substitute name
 *  @param[in]  bRelative       Is symlink relative (ignored if junction)
 *  @param[in]  bJunction       Create a junction point (bRelative ignored)
 *  @return     TRUE if success, FALSE otherwise
 */
static BOOL CreateLinkInternal(IN LPCWSTR sLinkName, IN LPCWSTR sPrintName, IN LPCWSTR sSubstituteName,
    IN BOOL bRelative, IN BOOL bJunction)
{
    HANDLE hHandle;
    DWORD dwLen;

    hHandle = OpenFileForWrite(sLinkName,
        (GetFileAttributes(sLinkName) & FILE_ATTRIBUTE_DIRECTORY));

    if (INVALID_HANDLE_VALUE == hHandle)
    {
        return FALSE;
    }

    PREPARSE_DATA_BUFFER pReparse = (PREPARSE_DATA_BUFFER)GlobalAlloc(GPTR, MAX_REPARSE_BUFFER);

    pReparse->ReparseTag = bJunction ? IO_REPARSE_TAG_MOUNT_POINT : IO_REPARSE_TAG_SYMLINK;

    FillREPARSE_DATA_BUFFER(
        pReparse, sPrintName, (USHORT)wcslen(sPrintName),
        sSubstituteName, (USHORT)wcslen(sSubstituteName));

    if (!bJunction && bRelative)
    {
        pReparse->SymbolicLinkReparseBuffer.Flags = SYMLINK_FLAG_RELATIVE;
    }

    if (!DeviceIoControl(hHandle, FSCTL_SET_REPARSE_POINT,
        pReparse,
        pReparse->ReparseDataLength + REPARSE_DATA_BUFFER_HEADER_SIZE,
        NULL, 0, &dwLen, NULL))
    {
        CloseHandle(hHandle);
        GlobalFree(pReparse);
        return FALSE;
    }

    CloseHandle(hHandle);
    GlobalFree(pReparse);
    return TRUE;
}

static BOOL FillREPARSE_DATA_BUFFER(PREPARSE_DATA_BUFFER pReparse, LPCWSTR sPrintName, USHORT uPrintNameLength, LPCWSTR sSubstituteName, USHORT uSubstituteNameLength)
{
    BOOL Result = FALSE;

    pReparse->Reserved = 0;

    switch (pReparse->ReparseTag)
    {
    case IO_REPARSE_TAG_SYMLINK:
        pReparse->SymbolicLinkReparseBuffer.PrintNameOffset = 0;
        pReparse->SymbolicLinkReparseBuffer.PrintNameLength = (USHORT)(uPrintNameLength * sizeof(wchar_t));
        pReparse->SymbolicLinkReparseBuffer.SubstituteNameOffset = pReparse->SymbolicLinkReparseBuffer.PrintNameLength;
        pReparse->SymbolicLinkReparseBuffer.SubstituteNameLength = (USHORT)(uSubstituteNameLength * sizeof(wchar_t));

        pReparse->ReparseDataLength = FIELD_OFFSET(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer)
            + pReparse->SymbolicLinkReparseBuffer.SubstituteNameOffset
            + pReparse->SymbolicLinkReparseBuffer.SubstituteNameLength - REPARSE_DATA_BUFFER_HEADER_SIZE;

        if (pReparse->ReparseDataLength + REPARSE_DATA_BUFFER_HEADER_SIZE <= (USHORT)(MAXIMUM_REPARSE_DATA_BUFFER_SIZE / sizeof(wchar_t)))
        {
            wmemcpy(&pReparse->SymbolicLinkReparseBuffer.PathBuffer[pReparse->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(wchar_t)], sSubstituteName, uSubstituteNameLength);
            wmemcpy(&pReparse->SymbolicLinkReparseBuffer.PathBuffer[pReparse->SymbolicLinkReparseBuffer.PrintNameOffset / sizeof(wchar_t)], sPrintName, uPrintNameLength);
            Result = TRUE;
        }
        break;

    case IO_REPARSE_TAG_MOUNT_POINT:
        pReparse->MountPointReparseBuffer.SubstituteNameOffset = 0;
        pReparse->MountPointReparseBuffer.SubstituteNameLength = (USHORT)(uSubstituteNameLength * sizeof(wchar_t));
        pReparse->MountPointReparseBuffer.PrintNameOffset = pReparse->MountPointReparseBuffer.SubstituteNameLength + 2;
        pReparse->MountPointReparseBuffer.PrintNameLength = (USHORT)(uPrintNameLength * sizeof(wchar_t));

        pReparse->ReparseDataLength = FIELD_OFFSET(REPARSE_DATA_BUFFER, MountPointReparseBuffer.PathBuffer)
            + pReparse->MountPointReparseBuffer.PrintNameOffset
            + pReparse->MountPointReparseBuffer.PrintNameLength + 1 * sizeof(wchar_t) - REPARSE_DATA_BUFFER_HEADER_SIZE;

        if (pReparse->ReparseDataLength + REPARSE_DATA_BUFFER_HEADER_SIZE <= (USHORT)(MAXIMUM_REPARSE_DATA_BUFFER_SIZE / sizeof(wchar_t)))
        {
            wmemcpy(&pReparse->MountPointReparseBuffer.PathBuffer[pReparse->MountPointReparseBuffer.SubstituteNameOffset / sizeof(wchar_t)], sSubstituteName, uSubstituteNameLength + 1);
            wmemcpy(&pReparse->MountPointReparseBuffer.PathBuffer[pReparse->MountPointReparseBuffer.PrintNameOffset / sizeof(wchar_t)], sPrintName, uPrintNameLength + 1);
            Result = TRUE;
        }

        break;
    }

    return Result;
}

/**
 *  @brief      Get a print name of a mount point, junction point or symbolic link
 *  @param[in]  sFileName         File name
 *  @param[out] sPrintName        Print name from the reparse buffer
 *  @param[in]  uPrintNameLength  Length of the sPrintName buffer
 *  @return     TRUE if success, FALSE otherwise
 */
REPARSELIB_API BOOL GetPrintName(IN LPCWSTR sFileName, OUT LPWSTR sPrintName, IN USHORT uPrintNameLength)
{
    PREPARSE_DATA_BUFFER pReparse;
    DWORD dwTag;

    if ((NULL == sPrintName) || (0 == uPrintNameLength))
    {
        return FALSE;
    }

    if (!ReparsePointExists(sFileName))
    {
        return FALSE;
    }

    if (!GetReparseTag(sFileName, &dwTag))
    {
        return FALSE;
    }

    // If not mount point, reparse point or symbolic link
    if (!((dwTag == IO_REPARSE_TAG_MOUNT_POINT) || (dwTag == IO_REPARSE_TAG_SYMLINK)))
    {
        return FALSE;
    }

    pReparse = (PREPARSE_DATA_BUFFER)
        GlobalAlloc(GPTR, MAXIMUM_REPARSE_DATA_BUFFER_SIZE);

    if (!GetReparseBuffer(sFileName, (PREPARSE_GUID_DATA_BUFFER)pReparse))
    {
        GlobalFree(pReparse);
        return FALSE;
    }

    switch (dwTag)
    {
    case IO_REPARSE_TAG_MOUNT_POINT:
        if (uPrintNameLength >= pReparse->MountPointReparseBuffer.PrintNameLength)
        {
            memset(sPrintName, 0, uPrintNameLength);
            memcpy
            (
                sPrintName,
                &pReparse->MountPointReparseBuffer.PathBuffer
                [
                    pReparse->MountPointReparseBuffer.PrintNameOffset / sizeof(wchar_t)
                ],
                pReparse->MountPointReparseBuffer.PrintNameLength
            );
        }
        else
        {
            GlobalFree(pReparse);
            return FALSE;
        }
        break;
    case IO_REPARSE_TAG_SYMLINK:
        if (uPrintNameLength >= pReparse->SymbolicLinkReparseBuffer.PrintNameLength)
        {
            memset(sPrintName, 0, uPrintNameLength);
            memcpy
            (
                sPrintName,
                &pReparse->SymbolicLinkReparseBuffer.PathBuffer
                [
                    pReparse->SymbolicLinkReparseBuffer.PrintNameOffset / sizeof(wchar_t)
                ],
                pReparse->SymbolicLinkReparseBuffer.PrintNameLength
            );
        }
        else
        {
            GlobalFree(pReparse);
            return FALSE;
        }
        break;
    }

    GlobalFree(pReparse);

    return TRUE;
}

/**
 *  @brief      Get a substitute name of a mount point, junction point or symbolic link
 *  @param[in]  sFileName         File name
 *  @param[out] sSubstituteName   Substitute name from the reparse buffer
 *  @param[in]  uSubstituteNameLength  Length of the sSubstituteName buffer
 *  @return     TRUE if success, FALSE otherwise
 */
REPARSELIB_API BOOL GetSubstituteName(IN LPCWSTR sFileName, OUT LPWSTR sSubstituteName, IN USHORT uSubstituteNameLength)
{
    PREPARSE_DATA_BUFFER pReparse;
    DWORD dwTag;

    if ((NULL == sSubstituteName) || (0 == uSubstituteNameLength))
    {
        return FALSE;
    }

    if (!ReparsePointExists(sFileName))
    {
        return FALSE;
    }

    if (!GetReparseTag(sFileName, &dwTag))
    {
        return FALSE;
    }

    // If not mount point, reparse point or symbolic link
    if (!((dwTag == IO_REPARSE_TAG_MOUNT_POINT) || (dwTag == IO_REPARSE_TAG_SYMLINK)))
    {
        return FALSE;
    }

    pReparse = (PREPARSE_DATA_BUFFER)
        GlobalAlloc(GPTR, MAXIMUM_REPARSE_DATA_BUFFER_SIZE);

    if (!GetReparseBuffer(sFileName, (PREPARSE_GUID_DATA_BUFFER)pReparse))
    {
        GlobalFree(pReparse);
        return FALSE;
    }

    switch (dwTag)
    {
    case IO_REPARSE_TAG_MOUNT_POINT:
        if (uSubstituteNameLength >= pReparse->MountPointReparseBuffer.SubstituteNameLength)
        {
            memset(sSubstituteName, 0, uSubstituteNameLength);
            memcpy
            (
                sSubstituteName,
                &pReparse->MountPointReparseBuffer.PathBuffer
                [
                    pReparse->MountPointReparseBuffer.SubstituteNameOffset / sizeof(wchar_t)
                ],
                pReparse->MountPointReparseBuffer.SubstituteNameLength
            );
        }
        else
        {
            GlobalFree(pReparse);
            return FALSE;
        }
        break;
    case IO_REPARSE_TAG_SYMLINK:
        if (uSubstituteNameLength >= pReparse->SymbolicLinkReparseBuffer.SubstituteNameLength)
        {
            memset(sSubstituteName, 0, uSubstituteNameLength);
            memcpy
            (
                sSubstituteName,
                &pReparse->SymbolicLinkReparseBuffer.PathBuffer
                [
                    pReparse->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(wchar_t)
                ],
                pReparse->SymbolicLinkReparseBuffer.SubstituteNameLength
            );
        }
        else
        {
            GlobalFree(pReparse);
            return FALSE;
        }
        break;
    }

    GlobalFree(pReparse);

    return TRUE;
}

/**
 *  @brief      Determine whether the symbolic link is relative or not
 *  @param[in]  sFileName File name
 *  @return     TRUE if the link is relative, FALSE otherwise
 *              FALSE if not a symbolic link
 */
REPARSELIB_API BOOL IsSymbolicLinkRelative(IN LPCWSTR sFileName)
{
    DWORD dwTag;
    PREPARSE_DATA_BUFFER pReparse;
    BOOL bResult = FALSE;

    if (ReparsePointExists(sFileName))
    {
        if (GetReparseTag(sFileName, &dwTag))
        {
            if (IO_REPARSE_TAG_SYMLINK == dwTag)
            {
                pReparse = (PREPARSE_DATA_BUFFER)
                    GlobalAlloc(GPTR, MAXIMUM_REPARSE_DATA_BUFFER_SIZE);

                GetReparseBuffer(sFileName, (PREPARSE_GUID_DATA_BUFFER)pReparse);

                bResult = (pReparse->SymbolicLinkReparseBuffer.Flags & SYMLINK_FLAG_RELATIVE);

                GlobalFree(pReparse);
            }
        }
    }
    return bResult;
}

/**
 *  @brief      Determine whether the file is a junction point
 *  @param[in]  sFileName File name
 *  @return     TRUE if the file is a junction point, FALSE otherwise
 */
REPARSELIB_API BOOL IsJunctionPoint(IN LPCWSTR sFileName)
{
    DWORD dwTag;
    BOOL bResult = FALSE;
    PWCHAR pChar;

    if (ReparsePointExists(sFileName))
    {
        if (GetReparseTag(sFileName, &dwTag))
        {
            // IO_REPARSE_TAG_MOUNT_POINT is a common type for both
            // mount points and junction points
            if (IO_REPARSE_TAG_MOUNT_POINT == dwTag)
            {
                pChar = (PWCHAR)GlobalAlloc(GPTR, MAX_REPARSE_BUFFER);
                if (GetPrintName(sFileName, pChar, MAX_REPARSE_BUFFER))
                {
                    if (0 != wcsncmp(L"\\??\\Volume", pChar, 10))
                    {
                        bResult = TRUE;
                    }
                }
                GlobalFree(pChar);
            }
        }
    }
    return bResult;
}

/**
 *  @brief      Determine whether the file is a volume mount point
 *  @param[in]  sFileName File name
 *  @return     TRUE if the file is a volume mount point, FALSE otherwise
 */
REPARSELIB_API BOOL IsMountPoint(IN LPCWSTR sFileName)
{
    DWORD dwTag;
    BOOL bResult = FALSE;
    PWCHAR pChar;

    if (ReparsePointExists(sFileName))
    {
        if (GetReparseTag(sFileName, &dwTag))
        {
            // IO_REPARSE_TAG_MOUNT_POINT is a common type for both
            // mount points and junction points
            if (IO_REPARSE_TAG_MOUNT_POINT == dwTag)
            {
                pChar = (PWCHAR)GlobalAlloc(GPTR, MAX_REPARSE_BUFFER);
                if (GetSubstituteName(sFileName, pChar, MAX_REPARSE_BUFFER))
                {
                    if (0 == wcsncmp(L"\\??\\Volume", pChar, 10))
                    {
                        bResult = TRUE;
                    }
                }
                GlobalFree(pChar);
            }
        }
    }
    return bResult;
}

/**
 *  @brief      Determine whether the file is a symbolic link
 *  @param[in]  sFileName File name
 *  @return     TRUE if the file is a symbolic link, FALSE otherwise
 */
REPARSELIB_API BOOL IsSymbolicLink(IN LPCWSTR sFileName)
{
    DWORD dwTag;
    if (!ReparsePointExists(sFileName))
    {
        return FALSE;
    }
    else
    {
        if (!GetReparseTag(sFileName, &dwTag))
        {
            return FALSE;
        }
        else
        {
            return (IO_REPARSE_TAG_SYMLINK == dwTag);
        }
    }
}