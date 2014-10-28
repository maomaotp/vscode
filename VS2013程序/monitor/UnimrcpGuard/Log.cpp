/**
 * @file Log.cpp
 * @brief 日志文件处理
 *
 * @author jiangkaihua
 * @version 1.0
 * @date 2011-04-06
**/

#include "Log.h"
#include "zip.h"
#include <ctime>
#include <iomanip>
#include <iostream>
#include <vector>
#include <fstream>
#include <time.h>

#include <io.h>

#include <curl/curl.h>

using namespace std;
using namespace greatding::common;

const int MILLISECOND_PER_HOUR = 3600 * 1000;

const string LOG_NAMES[] = {
    "ERROR",
    "WARNING",
    "INFO",
    "DEBUG",
};

string GetTimeString(const char *format)
{
    time_t t = time(NULL);
    char buffer[50];
    strftime(buffer, 50, format, localtime(&t));
    return string(buffer);
}

// 输出头部
LogLine::LogLine(const char *file, const char *function, int line, int level, Log *log)
{
    level_ = level;
    if(log == NULL) log_ = Log::GetInstance();
    else            log_ = log;
    // 输出时间
    log_stream_ << "[" << GetTimeString("%Y-%m-%d %H:%M:%S") << "] ";
    // 输出日志等级
    log_stream_ << "(" << LOG_NAMES[level_] << ") ";
    // 输出代码文件名
    if(log_->style_file_)     log_stream_ << "(" << file << ") ";
    // 输出代码函数名
    if(log_->style_function_) log_stream_ << "(" << function << ") ";
    // 输出代码行号
    if(log_->style_line_)     log_stream_ << "(" << setw(4) << setfill('0') << line << ") ";
    log_stream_ << ":  ";
}

// 析构函数，确定最终的输出目的地
LogLine::~LogLine()
{
    if(level_ <= log_->level_)      log_->WriteStringToBuffer(log_stream_.str());
}

// 静态实例
map<string, Log *> Log::logs_;
// 静态实例的管理
SRWLOCK Log::logs_lock_;
// 指示是否初始化
bool Log::initialize_ = false;

// 初始化
bool Log::Initialize()
{
    if(!initialize_) {
        initialize_ = true;
        // 初始化锁
        InitializeSRWLock(&logs_lock_);
        // 添加到exit之前处理
        atexit(ReleaseLogs);
    }
    return true;
}

// 生成静态Log实例
Log *Log::GetInstance(const char *name)
{
    string str;
    if(name != NULL)    str = name;
    Log *log = NULL;
    AcquireSRWLockExclusive(&logs_lock_);
    log = logs_[str];
    if(log == NULL) {
        log = new Log();
        logs_[str] = log;
    }
    ReleaseSRWLockExclusive(&logs_lock_);
    return log;
}

// 强制输出所有日志，然后关闭日志
void Log::ReleaseLogs()
{
    AcquireSRWLockExclusive(&logs_lock_);
    map<string, Log *>::iterator iterator;
    for(iterator = logs_.begin(); iterator != logs_.end(); ++ iterator)
        delete iterator->second;
    logs_.clear();
    ReleaseSRWLockExclusive(&logs_lock_);
}

Log::Log()
{
    style_file_ = true;                                     // 是否输出代码文件名
    style_function_ = true;                                 // 是否输出代码函数名
    style_line_ = true;                                     // 是否输出代码行号
    level_ = 0;                                             // 日志输出等级，大于或等于次等级的日志才输出
    stream_file_ = false;                                   // 是否输出到文件
    stream_standard_ = true;                                // 是否输出到屏幕
    time_file_ = 24 * MILLISECOND_PER_HOUR;                 // 每隔多长时间，更换一个日志文件
    time_package_ = 24 * 7 * MILLISECOND_PER_HOUR;          // 每隔多长时间，将日志文件打包
    time_buffer_ = MILLISECOND_PER_HOUR / 3600 * 10;        // 每隔多长之间，将lines_中的日志buffer输出到文件中去
    InitializeSRWLock(&lock_);
    InitializeSRWLock(&lock_file_);
    thread_alive_ = false;
}

Log::~Log()
{
    if(thread_alive_) {
        thread_alive_ = false;
        WaitForSingleObject((HANDLE) thread_, INFINITE);
    }
    if(stream_file_) {
        // 输出未完成的日志
        while(lines_.size() > 0) {
            string str = lines_.front();
            lines_.pop();
            WriteStringToFile(str);
        }
        // 完成打包功能
        Package();
    }
}

// 设置日志文件目录，如果有多个Log对象，那么则需要设置在不同的日志文件路径
bool Log::SetDir(const char *file_dir, const char *package_dir)
{
    strcpy(dir_, file_dir);
    if(package_dir == NULL)         strcpy(package_, file_dir);
    else                            strcpy(package_, package_dir);
    if(access(dir_, 2) != 0)        return false;
    if(access(package_, 2) != 0)    return false;
    return true;
}

// 设置是否输出 代码文件名，代码函数名，代码行号
void Log::SetStyle(bool style_file, bool style_function, bool style_line)
{
    style_file_ = style_file;
    style_function_ = style_function;
    style_line_ = style_line;
}

// 设置输出的等级，大于或等于次等级的日志才输出
void Log::SetLevel(int level)
{
    level_ = level;
}

// 设置输出的方式
void Log::SetStream(bool stream_file, bool stream_standard)
{
    stream_file_ = stream_file;
    stream_standard_ = stream_standard;
}

// 设置日志时间间隔，单位是小时
void Log::SetTime(double time_file, double time_package, double time_buffer)
{
    time_file_ = ULONGLONG(time_file * MILLISECOND_PER_HOUR);
    time_package_ = ULONGLONG(time_package * MILLISECOND_PER_HOUR);
    time_buffer_ = int(time_buffer * MILLISECOND_PER_HOUR);
}

// 开启线程，记录日志
void Log::Start()
{
    if(thread_alive_)   return ;
    thread_ = _beginthreadex(NULL, 0, WriteWithParams, this, 0, NULL);
    thread_alive_ = true;
}

// 写一行日志到buffer
void Log::WriteStringToBuffer(string &str)
{
    if(stream_standard_)    cout << str;
    if(stream_file_) {
        // 添加到日志队列中去
        AcquireSRWLockExclusive(&lock_);
        lines_.push(str);
        ReleaseSRWLockExclusive(&lock_);
    }
}

// 写一行日志到日志文件
bool Log::WriteStringToFile(string &str)
{
    if(file_.size() == 0) {
        file_ = dir_ + GetTimeString("\\%Y%m%d-%H%M%S.log");
    }
    ofstream ofs(file_.c_str(), ios::app);
    if(!ofs.is_open())      return false;
    ofs << str;
    return true;
}

// 将日志进行打包
bool Log::Package()
{
    vector<string> files;
    // 查找所有的日志文件
    string root = dir_;
    HANDLE hsearch;
    WIN32_FIND_DATA fd;
    if((root != "") && (root[root.length()-1] != '\\'))	root += "\\";
    string match = root + "*.log";
    hsearch = FindFirstFile(match.c_str(), &fd);
    if(hsearch != INVALID_HANDLE_VALUE) {
        do{
            string file = fd.cFileName;
            if((file=="."||file==".."))
                continue;
            file = root + file;
            if(!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                files.push_back(file);
            }
        }while(FindNextFile(hsearch, &fd));
    }
    FindClose(hsearch);
    // 对这些日志文件进行打包
    int size = files.size();
    if(size > 0) {
        string current = GetTimeString("%Y%m%d-%H%M%S");
        string zip = package_;
        zip += "\\" + current + ".zip";
        HZIP hzip = CreateZip(zip.c_str(), 0);
        if(hzip == NULL)    return false;
        for(int index = 0; index < size; ++ index) {
            string file = files[index];
            string name = current + file.substr(file.find_last_of("\\"));
            ZipAdd(hzip, name.c_str(), file.c_str());
            remove(file.c_str());
        }
        CloseZip(hzip);
    }
    return true;
}

// 线程调用的写日志函数
unsigned __stdcall Log::WriteWithParams(void *params)
{
    Log *log = (Log *) params;
    int size = 0;
    string str;
    ULONGLONG begin_file = GetTickCount64();
    ULONGLONG begin_package = begin_file;
    ULONGLONG end;
    while(log->thread_alive_) {
        end = GetTickCount64();
        // 判断是否需要更换日志文件
        if(end - begin_file > log->time_file_) {
            begin_file = end;
            log->file_ = "";
        }
        // 判断是否需要进行日志打包
        if(end - begin_package > log->time_package_) {
            begin_package = end;
            log->file_ = "";
            AcquireSRWLockExclusive(&(log->lock_file_));
            log->Package();
            ReleaseSRWLockExclusive(&(log->lock_file_));
        }
        // 将队列中的数据，输出到日志文件中去
        int line_count = 0;
        while(true) {
            AcquireSRWLockExclusive(&(log->lock_));
            size = log->lines_.size();
            if(size > 0) {
                str = log->lines_.front();
                log->lines_.pop();
            }
            ReleaseSRWLockExclusive(&(log->lock_));
            if(size == 0)   break;
            AcquireSRWLockExclusive(&(log->lock_file_));
            log->WriteStringToFile(str);
            ReleaseSRWLockExclusive(&(log->lock_file_));
            ++ line_count;
            if(line_count > 1000)    break;
        }
        Sleep(log->time_buffer_);
    }
    return 0;
}

void FindFile(const string &root_dir, const string &regular,
    vector<string> *file_name = NULL, vector<string> *file_dir = NULL) {
        HANDLE hsearch;
        WIN32_FIND_DATA fd;
        if(file_name != NULL)	file_name->clear();
        if(file_dir != NULL)	file_dir->clear();
        string root = root_dir;
        if((root != "") && (root[root.length()-1] != '\\'))	root += "\\";
        string match = root + regular;
        hsearch = FindFirstFile(match.c_str(), &fd);
        if (hsearch == INVALID_HANDLE_VALUE)	return ;
        do {
            string file = fd.cFileName;
            if((file=="."||file==".."))
                continue;
            file = root + file;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if(file_dir != NULL)		file_dir->push_back(file);
            } else {
                if(file_name != NULL)	file_name->push_back(file);
            }
        } while (FindNextFile(hsearch, &fd));
        FindClose(hsearch);
        if(file_name != NULL)   sort(file_name->begin(), file_name->end());
        if(file_dir != NULL)    sort(file_dir->begin(), file_dir->end());
}

// 删除Log目录下的所有文件
void Log::DeleteAllLog()
{
    AcquireSRWLockExclusive(&lock_file_);
    vector<string> files;
    FindFile(string(package_), "*.zip", &files, NULL);
    for(int index = 0; index < files.size(); ++ index) {
        remove(files[index].c_str());
    }
    FindFile(string(dir_), "*.txt", &files, NULL);
    for(int index = 0; index < files.size(); ++ index) {
        remove(files[index].c_str());
    }
    ReleaseSRWLockExclusive(&lock_file_);
}

// 删除Log目录下的所有过时的文件
void Log::DeleteOutdatedLog(double time_file)
{
    time_t file_time = time(NULL) - time_t(time_file * 3600);
    char buffer_time[50];
    strftime(buffer_time, 50, "%Y%m%d-%H%M%S", localtime(&file_time));
    string string_time = string(buffer_time);

    AcquireSRWLockExclusive(&lock_file_);
    vector<string> files;
    FindFile(string(package_), "*.zip", &files, NULL);
    for(int index = 0; index < files.size(); ++ index) {
        string file = files[index].substr(files[index].find_last_of("\\") + 1);
        file = file.substr(0, file.size() - 4);
        if(file < string_time) {
            remove(files[index].c_str());
        }
    }
    FindFile(string(dir_), "*.log", &files, NULL);
    for(int index = 0; index < files.size(); ++ index) {
        string file = files[index].substr(files[index].find_last_of("\\") + 1);
        file = file.substr(0, file.size() - 4);
        if(file < string_time) {
            remove(files[index].c_str());
        }
    }
    ReleaseSRWLockExclusive(&lock_file_);
}


/*liuqiang   FTP--LOG************************/

#define LOG_FTP_PATH			"ftp://192.168.88.130//home/"
#define FILE_NAME_FORMAT		"%Y%m%d-%H%M%S.zip"
#define LOG_LOCAL_PATH			"E:/lingban/src/monitor/Release/"

size_t read_data(void *ptr, size_t size, size_t nmemb, void *data)
{
	size_t return_size = fread(ptr, size, nmemb, (FILE *)data);
	printf("write %d\n", (int)return_size);
	return return_size;
}

int uploadLog(string zip)
{
	CURL* curl;
	FILE* sendFile;
	CURLcode res;

	//打开上传源文件
	printf("zip.c_str()==%s\n", zip.c_str());
	if (NULL == (sendFile = fopen(zip.c_str(), "rb"))){
		return -1;
	}
	//获取需要发送的文件大小
	fseek(sendFile, 0, SEEK_END);
	int sendSize = ftell(sendFile);
	if (sendSize < 0){
		fclose(sendFile);
		return -1;
	}
	fseek(sendFile, 0L, SEEK_SET);
	if ((res = curl_global_init(CURL_GLOBAL_ALL)) != 0){
		fclose(sendFile);
		return -1;
	}
	if ((curl = curl_easy_init()) == NULL){
		fclose(sendFile);
		curl_global_cleanup();
		return -1;
	}

	string ftp_path = LOG_FTP_PATH + GetTimeString(FILE_NAME_FORMAT);

	curl_easy_setopt(curl, CURLOPT_URL, ftp_path.c_str());
	curl_easy_setopt(curl, CURLOPT_USERPWD, "root:maomaotp");
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl, CURLOPT_READDATA, sendFile);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_data);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(curl, CURLOPT_INFILESIZE, sendSize);
	curl_easy_setopt(curl, CURLOPT_FTP_CREATE_MISSING_DIRS, 1);
	
	res = curl_easy_perform(curl);
	if (res != 0){
		fclose(sendFile);
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        return -1;		
	}
	curl_easy_cleanup(curl);
	fclose(sendFile);
	curl_global_cleanup();
	return 0;
}

void packageLog()
{
	SYSTEMTIME st;
	FILETIME ft;
	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);

	vector<string> files;
	// 查找当天的日志文件
	string root = LOG_LOCAL_PATH;
	HANDLE hsearch;
	WIN32_FIND_DATA fd;
	if ((root != "") && (root[root.length() - 1] != '\\'))	root += "\\";
	string match = root + "*.log";

	hsearch = FindFirstFile(match.c_str(), &fd);
	if (hsearch != INVALID_HANDLE_VALUE) {
		do{
			string file = fd.cFileName;
			if ( (file == "." || file == "..") )
				continue;
			//判断日志时间 
			//if ( (ULONGLONG&)ft > ((ULONGLONG&)fd.ftLastWriteTime + 60*10000000) )
			//	continue;
			file = root + file;

			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				files.push_back(file);
			}
		} while (FindNextFile(hsearch, &fd));
	}
	FindClose(hsearch);
	// 对这些日志文件进行打包
	int size = files.size();
	if (size > 0) {
		string zip = LOG_LOCAL_PATH + GetTimeString(FILE_NAME_FORMAT);
		HZIP hzip = CreateZip(zip.c_str(), 0);
		if (hzip == NULL)    return;
		for (int index = 0; index < size; ++index) {
			string file = files[index];
			string name = GetTimeString(FILE_NAME_FORMAT) + file.substr(file.find_last_of("\\"));
			ZipAdd(hzip, name.c_str(), file.c_str());
			remove(file.c_str());
		}
		CloseZip(hzip);
		//ftp上传打包后的日志文件，上传成功后删除本地文件
		if (uploadLog(zip) == 0){
			printf("delete .zip file\n");
			remove(zip.c_str());
		}
	}
	return;
}

unsigned int __stdcall tarLog(void *param)
{
	time_t rawtime;
	struct tm * timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	while (1) {
		if (timeinfo->tm_hour == 17){
			packageLog();
			break;
		}
		//每隔30分钟判断一次系统时间是否为23点,第一次上传日志后，sleep24h后再次执行
		Sleep(1800*1000);
	}
	while (1) {
		Sleep(86400000);
		packageLog();
	}
	return 0;
}
/*end-----------------*/