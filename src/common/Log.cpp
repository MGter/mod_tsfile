#include <iostream>
#include <ctime>
#include <common/Tool.h>
#include "common/Log.h"

LogLevel Log::m_logLevel = DEBUG;
std::string Log::m_logPath = "./log/";
std::ofstream Log::m_logFile;

void Log::initLogLevel() {
    Log::setLogLevel(DEBUG);
    Log::setLogPath("./log");
}

void Log::setLogLevel(LogLevel level) {
    m_logLevel = level;
}

void Log::setLogPath(std::string logPath){
    m_logPath = logPath;
}


void Log::debug(const char* file, const int line, const char* format, ...) {
    if (m_logLevel <= DEBUG) {
        std::string timeString;
        getCurrentTime(timeString);

        char message[256]; 
        va_list args;
        va_start(args, format);
        std::vsnprintf(message, 255, format, args);
        va_end(args);

        std::string logLine = "[" + timeString + "][DEBUG][" + file + ":" + std::to_string(line) + "] " + message + "\n"; 
        writeLog(logLine);
    }
}

void Log::info(const char* file, const int line, const char* format, ...) {
    if (m_logLevel <= INFO) {
        std::string timeString;
        getCurrentTime(timeString);

        char message[256]; 
        va_list args;
        va_start(args, format);
        std::vsnprintf(message, 255, format, args);
        va_end(args);

        std::string logLine = "[" + timeString + "][INFO][" + file + ":" + std::to_string(line) + "] " + message + "\n";
        writeLog(logLine);
    }
}

void Log::warning(const char* file, const int line, const char* format, ...) {
    if (m_logLevel <= WARNING) {
        std::string timeString;
        getCurrentTime(timeString);

        char message[256]; 
        va_list args;
        va_start(args, format);
        std::vsnprintf(message, 255, format, args);
        va_end(args);

        std::string logLine = "[" + timeString + "][WARNING][" + file + ":" + std::to_string(line) + "] " + message + "\n";
        writeLog(logLine);
    }
}

void Log::error(const char* file, const int line, const char* format, ...) {
    if (m_logLevel <= ERROR) {
        std::string timeString;
        getCurrentTime(timeString);
    
        char message[256]; 
        va_list args;
        va_start(args, format);
        std::vsnprintf(message, 255, format, args);
        va_end(args);

        std::string logLine = "[" + timeString + "][ERROR][" + file + ":" + std::to_string(line) + "] " + message + "\n";
        writeLog(logLine);
    }
}


bool Log::createLogFile() {
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm* timeInfo = std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(timeInfo, "%Y%m");
    std::string logDir = m_logPath + "/" + oss.str();
    std::string logFilename = logDir + "/" + oss.str() + ".log";

    if (!directoryExists(logDir)) {
        createDirectory(logDir);
    }

    // close the file
    if(m_logFile.is_open()){
        Log::closeLogFile();
    }

    // re open the file
    m_logFile.open(logFilename, std::ios_base::app);
    if(!m_logFile.is_open()){
        std::cout <<"[" << __FILE__ << "][" << __LINE__ << "]"  "Failed to open the logfile: " << logFilename << std::endl;
    }
}

void Log::closeLogFile() {
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
    else{
        std::cout << "[" << __FILE__ << "][" << __LINE__ << "]" << "No logfile open!" << std::endl;
    }
}

void Log::writeLog(const std::string& logLine) {
    if (m_logFile.is_open()) {
        m_logFile << logLine << std::endl;
    }
    else{
        std::cout << "[" << __FILE__ << "][" << __LINE__ << "]" << "No logfile open!" << std::endl;
        Log::createLogFile();
        Log::writeLog(logLine);
    }
}