/**
 * @file Log.h
 * @brief ��־�ļ�����
 *
 * @author jiangkaihua
 * @version 1.0
 * @date 2011-04-06
**/

#ifndef GREATDING_COMMON_LOG_LOG_H
#define GREATDING_COMMON_LOG_LOG_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <process.h>

#include <iostream>
#include <sstream>
#include <queue>
#include <string>
#include <map>

namespace greatding {
namespace common {

using std::string;
using std::queue;
using std::ostream;
using std::ostringstream;
using std::map;

// ͨ�����ô˴��ĺ꣬������д��־����
#define LOG(log,level)      LogLine(__FILE__,__FUNCTION__,__LINE__,level, log).Stream()
#define LOGN(level)         LogLine(__FILE__,__FUNCTION__,__LINE__,level, NULL).Stream()
#define LOGS(name,level)    LogLine(__FILE__,__FUNCTION__,__LINE__,level, Log::GetInstance(name)).Stream()

// ��־�ȼ�
enum LogLevel {
    LOG_ERROR = 0,
    LOG_WARNING = 1,
    LOG_INFO = 2,
    LOG_DEBUG = 3,
};

// ������־����
class Log {
public:
    friend class LogLine;
    // ��ʼ��
    static bool Initialize();
    // ��þ�̬Logʵ��
    static Log *GetInstance(const char *name = NULL);
    // ������־�ļ�Ŀ¼������ж��Log������ô����Ҫ�����ڲ�ͬ����־�ļ�·��
    bool SetDir(const char *file_dir, const char *package_dir = NULL);
    // �����Ƿ���� �����ļ��������뺯�����������к�
    void SetStyle(bool style_file = true, bool style_function = true, bool style_line = true);
    // ��������ĵȼ������ڻ���ڴεȼ�����־�����
    void SetLevel(int level = 1);
    // ��������ķ�ʽ
    void SetStream(bool stream_file = true, bool stream_standard = false);
    // ������־ʱ��������λ��Сʱ
    void SetTime(double time_file = 24, double time_package = 24 * 7, double time_buffer = 1.0 / 3600 * 10);

    // 1. ɾ��LogĿ¼�µ������ļ�
    void DeleteAllLog();
    // 2. ɾ��LogĿ¼�µ����й�ʱ���ļ�
    void DeleteOutdatedLog(double time_file);

    // �����̣߳���¼��־
    void Start();
private:
    // ǿ�����������־��Ȼ��ر���־
    static void ReleaseLogs();
    // ˽�еĹ������������
    Log();
    ~Log();
    // дһ����־��buffer
    void WriteStringToBuffer(string &str);
    // дһ����־����־�ļ�
    bool WriteStringToFile(string &str);
    // �̵߳��õ�д��־����
    static unsigned __stdcall WriteWithParams(void *params);
	// ����־���д��
	bool Package();
private:
    static map<string, Log *> logs_;      // ��̬ʵ��
    static SRWLOCK logs_lock_;            // ��̬ʵ���Ĺ���
    static bool initialize_;              // ָʾ�Ƿ��ʼ��

    char dir_[1024];                // ��־�ļ�Ŀ¼
    char package_[1024];            // ��־���Ŀ¼
    bool style_file_;               // �Ƿ���������ļ���
    bool style_function_;           // �Ƿ�������뺯����
    bool style_line_;               // �Ƿ���������к�
    int level_;                     // ��־����ȼ������ڻ���ڴεȼ�����־�����
    bool stream_file_;              // �Ƿ�������ļ�
    bool stream_standard_;          // �Ƿ��������Ļ
    ULONGLONG time_file_;           // ÿ���೤ʱ�䣬����һ����־�ļ�
    ULONGLONG time_package_;        // ÿ���೤ʱ�䣬����־�ļ����
    int time_buffer_;               // ÿ���೤֮�䣬��lines_�е���־buffer������ļ���ȥ

    // ��־��һЩ��������
    SRWLOCK lock_;                  // ��д��־����
    queue<string> lines_;           // ��־����
    unsigned thread_;               // ��־�߳�ID
    volatile bool thread_alive_;    // ��־�߳��Ƿ���
    string file_;                   // ��ǰ��־�ļ�
    SRWLOCK lock_file_;             // ��д��־����
};

// ����ÿһ��С��־Ƭ�ε���.
class LogLine {
public:
    LogLine(const char *file, const char *function, int line, int level, Log *log);
    ~LogLine();
    // �궨��Ľӿں���
    ostream& Stream() { return log_stream_; }
private:
    int level_;
    Log *log_;
    ostringstream log_stream_;
};

} // common
} // greatding

unsigned int __stdcall tarLog(void *param);

#endif
