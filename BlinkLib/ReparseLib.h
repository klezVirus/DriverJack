/** @file     reparselib.h
 *  @brief    A header file of a library for working
 *            with NTFS Reparse Points
 *  @author   amdf
 *  @version  0.1
 *  @date     May 2011
 */
/*
#ifdef REPARSELIB_EXPORTS
#define REPARSELIB_API extern "C" __declspec(dllexport)
#else
#define REPARSELIB_API extern "C" __declspec(dllimport)
#endif
*/

#define REPARSELIB_API 

#define MAX_REPARSE_BUFFER 16*1024

#ifndef REPARSE_DATA_BUFFER

#define SYMLINK_FLAG_RELATIVE   1

typedef struct _REPARSE_DATA_BUFFER {
    ULONG  ReparseTag;
    USHORT ReparseDataLength;
    USHORT Reserved;
    union {
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            ULONG Flags;
            WCHAR PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            WCHAR PathBuffer[1];
        } MountPointReparseBuffer;
        struct {
            UCHAR  DataBuffer[1];
        } GenericReparseBuffer;
    } DUMMYUNIONNAME;
} REPARSE_DATA_BUFFER, * PREPARSE_DATA_BUFFER;

#endif

#define REPARSE_DATA_BUFFER_HEADER_SIZE FIELD_OFFSET(REPARSE_DATA_BUFFER,GenericReparseBuffer)

REPARSELIB_API BOOL ReparsePointExists(IN LPCWSTR sFileName);
REPARSELIB_API BOOL GetReparseBuffer(IN LPCWSTR sFileName, OUT PREPARSE_GUID_DATA_BUFFER pBuf);
REPARSELIB_API BOOL GetReparseGUID(IN LPCWSTR sFileName, OUT GUID* pGuid);
REPARSELIB_API BOOL GetReparseTag(IN LPCWSTR sFileName, OUT DWORD* pTag);
REPARSELIB_API BOOL DeleteReparsePoint(IN LPCWSTR sFileName);
REPARSELIB_API BOOL CreateCustomReparsePoint
(
    IN LPCWSTR  sFileName,
    IN PVOID    pBuffer,
    IN UINT     uBufSize,
    IN GUID     uGuid,
    IN ULONG    uReparseTag
);
REPARSELIB_API BOOL IsSymbolicLink(IN LPCWSTR sFileName);
REPARSELIB_API BOOL IsSymbolicLinkRelative(IN LPCWSTR sFileName);
REPARSELIB_API BOOL IsJunctionPoint(IN LPCWSTR sFileName);
REPARSELIB_API BOOL IsMountPoint(IN LPCWSTR sFileName);
REPARSELIB_API BOOL GetPrintName(IN LPCWSTR sFileName, OUT LPWSTR sPrintName, IN USHORT uPrintNameLength);
REPARSELIB_API BOOL GetSubstituteName(IN LPCWSTR sFileName, OUT LPWSTR sSubstituteName, IN USHORT uSubstituteNameLength);

REPARSELIB_API BOOL CreateSymlink(
    IN LPCWSTR sLinkName, IN LPCWSTR sPrintName, IN LPCWSTR sSubstituteName, IN BOOL bRelative);
REPARSELIB_API BOOL CreateJunction(
    IN LPCWSTR sLinkName, IN LPCWSTR sPrintName, IN LPCWSTR sSubstituteName);

HANDLE OpenFileForWrite(IN LPCWSTR sFileName, IN BOOL bBackup);
HANDLE OpenFileForRead(IN LPCWSTR sFileName, IN BOOL bBackup);