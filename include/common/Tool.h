#ifndef __COMMON_TOOL_H_
#define __COMMON_TOOL_H_

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <sys/stat.h>
#include <sys/types.h>



// 获取当前时间，存入timeString
void getCurrentTime(std::string& timeString);

// 确认文件是否存在
bool directoryExists(const std::string& path);

// 创建相应的文件夹
void createDirectory(const std::string& path);









#endif  //__COMMON_TOOL_H_