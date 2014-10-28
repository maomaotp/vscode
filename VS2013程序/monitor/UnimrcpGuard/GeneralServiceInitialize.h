#ifndef GREATDING_CTRIP_SERVICEINITIALIZE_H
#define GREATDING_CTRIP_SERVICEINITIALIZE_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
SERVICE_STATUS ServiceStatus; 
SERVICE_STATUS_HANDLE hStatus; 

const char *serviceName = "unimrcp";
const char *daemonName = "uniMrcpGuard";
const char * processName = "unimrcpserver.exe";


void ControlHandler(DWORD request) 
{ 
	switch (request) { 
	case SERVICE_CONTROL_STOP: 
		ServiceStatus.dwWin32ExitCode = 0; 
		ServiceStatus.dwCurrentState = SERVICE_STOPPED; 
		SetServiceStatus (hStatus, &ServiceStatus);
		return; 

	case SERVICE_CONTROL_SHUTDOWN: 
		ServiceStatus.dwWin32ExitCode = 0; 
		ServiceStatus.dwCurrentState = SERVICE_STOPPED; 
		SetServiceStatus (hStatus, &ServiceStatus);
		return; 

	default:
		break;
	} 

	// Report current status
	SetServiceStatus (hStatus, &ServiceStatus);
	return; 
}
int InitializeService() {
	ServiceStatus.dwServiceType = 
		SERVICE_WIN32; 
	ServiceStatus.dwCurrentState = 
		SERVICE_START_PENDING; 
	//ServiceStatus.dwCurrentState = 
	//	SERVICE_RUNNING;		//Open the service
	ServiceStatus.dwControlsAccepted   =  
		SERVICE_ACCEPT_STOP | 
		SERVICE_ACCEPT_SHUTDOWN;
	ServiceStatus.dwWin32ExitCode = 0; 
	ServiceStatus.dwServiceSpecificExitCode = 0; 
	ServiceStatus.dwCheckPoint = 0; 
	ServiceStatus.dwWaitHint = 0; 

	hStatus = RegisterServiceCtrlHandler(
		"uniMrcpGuard", 
		(LPHANDLER_FUNCTION)ControlHandler); 
	if (hStatus == (SERVICE_STATUS_HANDLE)0) 
	{ 
		// Registering Control Handler failed
		return -1; 
	}  
	// We report the running status to SCM. 
	ServiceStatus.dwCurrentState = 
		SERVICE_RUNNING; 
	SetServiceStatus (hStatus, &ServiceStatus);
	return 0;
}
#endif