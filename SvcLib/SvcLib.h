#include <windows.h>
#include <stdio.h>
#include <string>

#define DLL_EXPORT

DLL_EXPORT BOOL __stdcall DoStopSvc(LPCWSTR szSvcName);
DLL_EXPORT BOOL __stdcall StopDependentServices(SC_HANDLE schSCManager, SC_HANDLE schService);
DLL_EXPORT BOOL __stdcall DoStartSvc(LPCWSTR szSvcName);
DLL_EXPORT BOOL __stdcall DoReStartSvcAsync(std::string szSvcName);