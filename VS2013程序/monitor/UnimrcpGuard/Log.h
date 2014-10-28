/**
 * @file Log.h
 * @brief 日志文件处理
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

// 通过调用此处的宏，来进行写日志操作
#define LOG(log,level)      LogLine(__FILE__,__FUNCTION__,__LINE__,level, log).Stream()
#define LOGN(level)         LogLine(__FILE__,__FUNCTION__,__LINE__,level, NULL).Stream()
#define LOGS(name,level)    LogLine(__FILE__,__FUNCTION__,__LINE__,level, Log::GetInstance(name)).Stream()

// 日志等级
enum LogLevel {
    LOG_ERROR = 0,
    LOG_WARNING = 1,
    LOG_INFO = 2,
    LOG_DEBUG = 3,
};

// 处理日志的类
class Log {
public:
    friend class LogLine;
    // 初始化
    static bool Initialize();
    // 获得静态Log实例
    static Log *GetInstance(const char *name = NULL);
    // 设置日志文件目录，如果有多个Log对象，那么则需要设置在不同的日志文件路径
    bool SetDir(const char *file_dir, const char *package_dir = NULL);
    // 设置是否输出 代码文件名，代码函数名，代码行号
    void SetStyle(bool style_file = true, bool style_function = true, bool style_line = true);
    // 设置输出的等级，大于或等于次等级的日志才输出
    void SetLevel(int level = 1);
    // 设置输出的方式
    void SetStream(bool stream_file = true, bool stream_standard = false);
    // 设置日志时间间隔，单位是小时
    void SetTime(double time_file = 24, double time_package = 24 * 7, double time_buffer = 1.0 / 3600 * 10);

    // 1. 删除Log目录下的所有文件
    void DeleteAllLog();
    // 2. 删除Log目录下的所有过时的文件
    void DeleteOutdatedLog(double time_file);

    // 开启线程，记录日志
    void Start();
private:
    // 强制输出所有日志，然后关闭日志
    static void ReleaseLogs();
    // 私有的构造和析构函数
    Log();
    ~Log();
    // 写一行日志到buffer
    void WriteStringToBuffer(string &str);
    // 写一行日志到日志文件
    bool WriteStringToFile(string &str);
    // 线程调用的写日志函数
    static unsigned __stdcall WriteWithParams(void *params);
	// 将日志进行打包
	bool Package();
private:
    static map<string, Log *> logs_;      // 静态实例
    static SRWLOCK logs_lock_;            // 静态实例的管理
    static bool initialize_;              // 指示是否初始化

    char dir_[1024];                // 日志文件目录
    char package_[1024];            // 日志打包目录
    bool style_file_;               // 是否输出代码文件名
    bool style_function_;           // 是否输出代码函数名
    bool style_line_;               // 是否输出代码行号
    int level_;                     // 日志输出等级，大于或等于次等级的日志才输出
    bool stream_file_;              // 是否输出到文件
    bool stream_standard_;          // 是否输出到屏幕
    ULONGLONG time_file_;           // 每隔多长时间，更换一个日志文件
    ULONGLONG time_package_;        // 每隔多长时间，将日志文件打包
    int time_buffer_;               // 每隔多长之间，将lines_中的日志buffer输出到文件中去

    // 日志的一些操作变量
    SRWLOCK lock_;                  // 读写日志的锁
    queue<string> lines_;           // 日志队列
    unsigned thread_;               // 日志线程ID
    volatile bool thread_alive_;    // 日志线程是否存活
    string file_;                   // 当前日志文件
    SRWLOCK lock_file_;             // 读写日志的锁
};

// 处理每一个小日志片段的类.
class LogLine {
public:
    LogLine(const char *file, const char *function, int line, int level, Log *log);
    ~LogLine();
    // 宏定义的接口函数
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
