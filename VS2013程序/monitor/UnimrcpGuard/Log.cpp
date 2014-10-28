/**
 * @file Log.cpp
 * @brief ��־�ļ�����
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

// ���ͷ��
LogLine::LogLine(const char *file, const char *function, int line, int level, Log *log)
{
    level_ = level;
    if(log == NULL) log_ = Log::GetInstance();
    else            log_ = log;
    // ���ʱ��
    log_stream_ << "[" << GetTimeString("%Y-%m-%d %H:%M:%S") << "] ";
    // �����־�ȼ�
    log_stream_ << "(" << LOG_NAMES[level_] << ") ";
    // ��������ļ���
    if(log_->style_file_)     log_stream_ << "(" << file << ") ";
    // ������뺯����
    if(log_->style_function_) log_stream_ << "(" << function << ") ";
    // ��������к�
    if(log_->style_line_)     log_stream_ << "(" << setw(4) << setfill('0') << line << ") ";
    log_stream_ << ":  ";
}

// ����������ȷ�����յ����Ŀ�ĵ�
LogLine::~LogLine()
{
    if(level_ <= log_->level_)      log_->WriteStringToBuffer(log_stream_.str());
}

// ��̬ʵ��
map<string, Log *> Log::logs_;
// ��̬ʵ���Ĺ���
SRWLOCK Log::logs_lock_;
// ָʾ�Ƿ��ʼ��
bool Log::initialize_ = false;

// ��ʼ��
bool Log::Initialize()
{
    if(!initialize_) {
        initialize_ = true;
        // ��ʼ����
        InitializeSRWLock(&logs_lock_);
        // ��ӵ�exit֮ǰ����
        atexit(ReleaseLogs);
    }
    return true;
}

// ���ɾ�̬Logʵ��
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

// ǿ�����������־��Ȼ��ر���־
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
    style_file_ = true;                                     // �Ƿ���������ļ���
    style_function_ = true;                                 // �Ƿ�������뺯����
    style_line_ = true;                                     // �Ƿ���������к�
    level_ = 0;                                             // ��־����ȼ������ڻ���ڴεȼ�����־�����
    stream_file_ = false;                                   // �Ƿ�������ļ�
    stream_standard_ = true;                                // �Ƿ��������Ļ
    time_file_ = 24 * MILLISECOND_PER_HOUR;                 // ÿ���೤ʱ�䣬����һ����־�ļ�
    time_package_ = 24 * 7 * MILLISECOND_PER_HOUR;          // ÿ���೤ʱ�䣬����־�ļ����
    time_buffer_ = MILLISECOND_PER_HOUR / 3600 * 10;        // ÿ���೤֮�䣬��lines_�е���־buffer������ļ���ȥ
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
        // ���δ��ɵ���־
        while(lines_.size() > 0) {
            string str = lines_.front();
            lines_.pop();
            WriteStringToFile(str);
        }
        // ��ɴ������
        Package();
    }
}

// ������־�ļ�Ŀ¼������ж��Log������ô����Ҫ�����ڲ�ͬ����־�ļ�·��
bool Log::SetDir(const char *file_dir, const char *package_dir)
{
    strcpy(dir_, file_dir);
    if(package_dir == NULL)         strcpy(package_, file_dir);
    else                            strcpy(package_, package_dir);
    if(access(dir_, 2) != 0)        return false;
    if(access(package_, 2) != 0)    return false;
    return true;
}

// �����Ƿ���� �����ļ��������뺯�����������к�
void Log::SetStyle(bool style_file, bool style_function, bool style_line)
{
    style_file_ = style_file;
    style_function_ = style_function;
    style_line_ = style_line;
}

// ��������ĵȼ������ڻ���ڴεȼ�����־�����
void Log::SetLevel(int level)
{
    level_ = level;
}

// ��������ķ�ʽ
void Log::SetStream(bool stream_file, bool stream_standard)
{
    stream_file_ = stream_file;
    stream_standard_ = stream_standard;
}

// ������־ʱ��������λ��Сʱ
void Log::SetTime(double time_file, double time_package, double time_buffer)
{
    time_file_ = ULONGLONG(time_file * MILLISECOND_PER_HOUR);
    time_package_ = ULONGLONG(time_package * MILLISECOND_PER_HOUR);
    time_buffer_ = int(time_buffer * MILLISECOND_PER_HOUR);
}

// �����̣߳���¼��־
void Log::Start()
{
    if(thread_alive_)   return ;
    thread_ = _beginthreadex(NULL, 0, WriteWithParams, this, 0, NULL);
    thread_alive_ = true;
}

// дһ����־��buffer
void Log::WriteStringToBuffer(string &str)
{
    if(stream_standard_)    cout << str;
    if(stream_file_) {
        // ��ӵ���־������ȥ
        AcquireSRWLockExclusive(&lock_);
        lines_.push(str);
        ReleaseSRWLockExclusive(&lock_);
    }
}

// дһ����־����־�ļ�
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

// ����־���д��
bool Log::Package()
{
    vector<string> files;
    // �������е���־�ļ�
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
    // ����Щ��־�ļ����д��
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

// �̵߳��õ�д��־����
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
        // �ж��Ƿ���Ҫ������־�ļ�
        if(end - begin_file > log->time_file_) {
            begin_file = end;
            log->file_ = "";
        }
        // �ж��Ƿ���Ҫ������־���
        if(end - begin_package > log->time_package_) {
            begin_package = end;
            log->file_ = "";
            AcquireSRWLockExclusive(&(log->lock_file_));
            log->Package();
            ReleaseSRWLockExclusive(&(log->lock_file_));
        }
        // �������е����ݣ��������־�ļ���ȥ
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

// ɾ��LogĿ¼�µ������ļ�
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

// ɾ��LogĿ¼�µ����й�ʱ���ļ�
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

	//���ϴ�Դ�ļ�
	printf("zip.c_str()==%s\n", zip.c_str());
	if (NULL == (sendFile = fopen(zip.c_str(), "rb"))){
		return -1;
	}
	//��ȡ��Ҫ���͵��ļ���С
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
	// ���ҵ������־�ļ�
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
			//�ж���־ʱ�� 
			//if ( (ULONGLONG&)ft > ((ULONGLONG&)fd.ftLastWriteTime + 60*10000000) )
			//	continue;
			file = root + file;

			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				files.push_back(file);
			}
		} while (FindNextFile(hsearch, &fd));
	}
	FindClose(hsearch);
	// ����Щ��־�ļ����д��
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
		//ftp�ϴ���������־�ļ����ϴ��ɹ���ɾ�������ļ�
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
		//ÿ��30�����ж�һ��ϵͳʱ���Ƿ�Ϊ23��,��һ���ϴ���־��sleep24h���ٴ�ִ��
		Sleep(1800*1000);
	}
	while (1) {
		Sleep(86400000);
		packageLog();
	}
	return 0;
}
/*end-----------------*/