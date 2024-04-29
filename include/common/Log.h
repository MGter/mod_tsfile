#ifndef __COMMON_LOG_H_
#define __COMMON_LOG_H_

#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <cstdarg>

extern std::map<std::string, std::string> g_configMap;

enum LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Log {
public:
    static void initLogLevel();
    static void setLogLevel(LogLevel level);
    static void setLogPath(std::string logPath);
    static void debug(const char* file, const int line, const char* message, ...);
    static void info(const char* file, const int line, const char* message, ...);
    static void warning(const char* file, const int line, const char* message, ...);
    static void error(const char* file, const int line, const char* message, ...);
private:
    static bool createLogFile();
    static void closeLogFile();
    static void writeLog(const std::string& logLine);
private:
    static LogLevel        m_logLevel;
    static std::string     m_logPath;
    static std::ofstream   m_logFile;
};


#endif //__LOG_H_