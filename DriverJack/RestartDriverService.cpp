#include "common.h"

BOOL RestartDriverService()
{
	BOOL success = FALSE;

#pragma region Opening SCM
	RAII::ScHandle svcManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
	if (!svcManager.GetHandle())
	{
		Error(GetLastError());
		return 1;
	}
	else std::cout << "(+) Opened handle to the SCM!\n";
#pragma endregion Opening SCM

#pragma region Starting Driver Service

	RAII::ScHandle drvSvc = OpenServiceW(svcManager.GetHandle(), DRIVER_SERVICE_NAME, SERVICE_ALL_ACCESS);
	if (!drvSvc.GetHandle())
	{
		Error(GetLastError());
		std::cout << "(-) Couldn't get a handle to the " << DRIVER_SERVICE_NAME << " service...\n";
		return 1;
	}
	else std::cout << "(+) Opened handle to " << DRIVER_SERVICE_NAME << " service!\n";
	
	SERVICE_STATUS_PROCESS ssp;
	success = ControlService(drvSvc.GetHandle(), SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&ssp);
	// We don't care if the service is already stopped

	success = StartServiceW(drvSvc.GetHandle(), 0, nullptr);
	if (!success)
	{
		Error(GetLastError());
		std::cout << "(-) Couldn't restart " << DRIVER_SERVICE_NAME << "  service...\n";
		return 1;
	}
	else std::cout << "(+) Successfully restarted the " << DRIVER_SERVICE_NAME << "  service!\n";

#pragma endregion Driver Service Restarted
	return success;
}