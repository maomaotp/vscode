#include "GeneralServiceInitialize.h"
#include "WorkDirInitialize.h"
#include "Log.h"
//#include "ProjectUpdateClient.h"
#include "tlhelp32.h"

using namespace std;
using namespace greatding::common;

DWORD GetProcessIdByName(const char *pName);

#ifdef USING_SERVICE
void stopService() {

    char command[256];
    sprintf(command,"sc stop \"%s\"",serviceName);
    LOG(NULL, LOG_WARNING) << "Executing " << command   <<endl;     

    if (system(command) == 0) {
        LOG(NULL, LOG_WARNING) <<"Success to stop service " << serviceName  <<endl;
    }else {
        LOG(NULL, LOG_WARNING) <<"Failed to stop service " << serviceName <<endl;
    }    
}

int startService() {
    int result = 0;
    char command[256];
    LOG(NULL, LOG_WARNING) <<"Restarting service "<<  serviceName << "..." << endl;
    sprintf(command,"sc start \"%s\"",serviceName);

    if (system(command) == 0) {
        LOG(NULL, LOG_WARNING) <<"Success to start service" <<serviceName <<endl;
        result = 0;
    }else {
        LOG(NULL, LOG_ERROR) <<"Failed to start service" <<serviceName <<endl;
        result = 1;
    }
    return result;
}
#else
void stopService() {

    char command[256];
    int pid = GetProcessIdByName(processName);
	if (pid > 0) {            
        sprintf(command,"taskkill /PID %d",pid);
        LOG(NULL, LOG_WARNING) << "Executing " << command   <<endl;     
        
        if (system(command) == 0) {
            LOG(NULL, LOG_WARNING) <<"Success to stop process " << processName  <<endl;
        }else {
            LOG(NULL, LOG_WARNING) <<"Failed to stop process " << processName <<endl;
        } 
    }    
   
}

int startService(bool stay) {
    int result = 0;
    char command[256];
    LOG(NULL, LOG_WARNING) <<"Starting process "<<  processName << "..." << endl;
    if(stay){
        sprintf(command,"start cmd /K %s",processName);
    }else{
        sprintf(command,"start cmd /C %s",processName);
    }

    

    if (system(command) == 0) {
        LOG(NULL, LOG_WARNING) <<"Success to start process" <<serviceName <<endl;
        Sleep(1500);
        result = 0;
    }else {
        LOG(NULL, LOG_ERROR) <<"Failed to start process" <<serviceName <<endl;
        result = 1;
    }
    return result;
}

#endif

int restartService(bool stay) {
	stopService();
	int result = startService(stay);
	return result;
}

DWORD GetProcessIdByName(const char *pName)
{
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (INVALID_HANDLE_VALUE == hSnapshot)
		return -1;
	PROCESSENTRY32 pe = { sizeof(PROCESSENTRY32) };
	for (BOOL fOk = Process32First(hSnapshot, &pe); fOk; fOk = Process32Next(hSnapshot, &pe))
	{
		if (!_stricmp(pe.szExeFile, pName))
		{
			DWORD pid = pe.th32ProcessID;
			CloseHandle(hSnapshot);
			return pid;
		}
	}
	CloseHandle(hSnapshot);
	return 0;
}

static SRWLOCK updateLock;


int service_main(int argc,char ** argv);
int main(int argc,char ** argv) {   
    char myServiceName[256];

    setlocale(LC_ALL,"Chinese-simplified");  // for compatible to Chinese characters in file names
	//liuq  ftp log****************
	HANDLE handle;
	DWORD  ExitNum;

	handle = (HANDLE)_beginthreadex(NULL, 0, &tarLog, NULL, 0, NULL);
	if (handle == NULL) {
		printf("start tarLog thread failed,errno:%d\n", GetLastError());
		return GetLastError();
	}
	GetExitCodeThread(handle, &ExitNum);
	if (ExitNum == 0){
		return -1;
	}
	//liuq_end

    sprintf(myServiceName,"%s",daemonName);    
	if (argc==1) {
		// run as service
		SERVICE_TABLE_ENTRY Table[]={{ myServiceName, (LPSERVICE_MAIN_FUNCTION)service_main},{NULL,NULL}};       
		StartServiceCtrlDispatcher(Table);
	}else {
		service_main(argc,argv);
	}
	return 0;
}

void Usage()
{
    printf("Usage: UnimrcpGuard [-stay] [-loglevel log_level] [-processname bin_file_name] [-exedir exe_file_path]\n");
    printf("  -stay        : make the host cmd window stays on the screen after the launched program termintes \n");
    printf("  level        : the criticality of the event to be logged  \n");
    printf("  bin_file_name: the executeable file name of the process (unimrcpserver.exe by default)  \n");
    printf("  bin_file_name: the path to find the executeable file (when not specified, using the path defined in registry key) \n");
    printf("                   HKEY_LOCAL_MACHINE\\SOFTWARE\\Unimrcp\\UnimrcpPath");

    exit(-1);
}
int service_main(int argc,char ** argv) {	
	
    InitializeService();

    

    char processname[1024];
    
    int log_level=LOG_INFO;
    bool stay=false;
    bool get_process_name = false;
    bool get_exe_dir = false;

    sprintf(processname,"%s","unimrcpserver.exe");


    for(int arg_idx=1;arg_idx<argc;arg_idx++){

        char *arg = argv[arg_idx];
        if(strcmp(arg,"-stay")==0){
            stay=true;
        }else if(strcmp(arg,"-loglevel")==0){
            arg_idx++;
            if(arg_idx>=argc){
               Usage();
            }else{
               log_level=atoi(argv[arg_idx]);
            }
        }else if(strcmp(arg,"-processname")==0){
            arg_idx++;
            if(arg_idx>=argc){
               Usage();
            }else{
               sprintf(processname,"%s",argv[arg_idx]);
               get_process_name = true;
            }
        }else if(strcmp(arg,"-exedir")==0){
            arg_idx++;
            if(arg_idx>=argc){
               Usage();
            }else{
               if(_chdir(argv[arg_idx])==0)
                   get_exe_dir = true;
               else{
                   printf("Unable to goto execute file path %s",argv[arg_idx]);                   
               }
            }
        }else{
            printf("Unknown argument %s",arg);
            Usage();

        }
    }

    if(get_exe_dir==false && (WorkDirInitialize()==-1)){
        printf("Failed setting execute file path, unable to launch the program");
        exit(-1);        
    }


    string logDir =  "..\\log\\guard\\";	
	// initialize log
	Log::Initialize();
	Log * log = Log::GetInstance();
    
    if(!log->SetDir(logDir.c_str(),logDir.c_str())) {
		cout << "Log SetDir failed"<<endl;
		log->SetStream(false, true);
	}

	// whether to ouput <code file name>,<function name>,<code line num>
	log->SetStyle(false, true, true);
	// set the output leve
	log->SetLevel(log_level);   //
	// set output type, file or std stream
	log->SetStream(true, true);
	// set intervals(hours),<change a log file>,<pack the log>, <output log>
	log->SetTime(1.0 / 3600 * 600, 24, 1.0 / 3600 * 10);
	_mkdir(logDir.c_str());

	LOG(NULL, LOG_INFO) <<"Starting logging thread..."<<endl;
	// start to serve
	log->Start();

	//const char * processName = "unimrcpserver.exe";
	const int interval = 500;    
    
    InitializeSRWLock(&updateLock);
    
	LOG(NULL, LOG_INFO) <<"Starting daemon routine..."<<endl;
    int log_cycle =0;
    while (true) {
        
        // 检测服务是否开启
        AcquireSRWLockExclusive(&updateLock);
		int pid = GetProcessIdByName(processName);
		if (pid > 0) {            
            if((log_cycle++)==120){
                LOG(NULL, LOG_INFO) <<"Process " << processName << " is running..."<<endl;
                log_cycle=0;
            }
		} else if (pid == 0) {
			LOG(NULL, LOG_ERROR) <<"Process " << processName << " NOT FOUND!!! Restarting..."<<endl;
			if (restartService(stay) == 0)
				LOG(NULL, LOG_WARNING) <<"Success to restart Service."<<endl;
			else
				LOG(NULL, LOG_ERROR) <<"Failed to restart Service!!!"<<endl;
		} else {
			LOG(NULL, LOG_ERROR) <<"Failed to call CreateToolhelp32Snapshot()!!!"<<endl;
		}
        ReleaseSRWLockExclusive(&updateLock);
		Sleep(interval);
	}
	return 0;
}