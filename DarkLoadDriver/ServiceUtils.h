#include <windows.h>
#include <stdio.h>
#include <string>

BOOL __stdcall DoStopSvc(LPCWSTR szSvcName);
BOOL __stdcall StopDependentServices(SC_HANDLE schSCManager, SC_HANDLE schService);
BOOL __stdcall DoStartSvc(LPCWSTR szSvcName);
BOOL __stdcall DoReStartSvcAsync(std::string szSvcName);