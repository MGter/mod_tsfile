#include "common/Tool.h"


void getCurrentTime(std::string& timeString) {
    time_t now = time(0);
    struct tm* timeinfo;
    char buffer[80];

    timeinfo = localtime(&now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    timeString = buffer;
}


bool directoryExists(const std::string& path) {
    struct stat info;
    return stat(path.c_str(), &info) == 0 && (info.st_mode & S_IFDIR);
}

void createDirectory(const std::string& path) {
    mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}