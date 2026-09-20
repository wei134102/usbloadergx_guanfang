#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <stdio.h>

// 简单日志写入函数 - 运行时通过 Settings.pluginLogEnabled 控制
void WriteLog(const char* level, const char* format, ...);

// 日志宏 - 不再使用编译时开关，改为运行时检查
#define LOG_ERROR(format, ...) WriteLog("ERROR", format, ##__VA_ARGS__)
#define LOG_WARNING(format, ...) WriteLog("WARNING", format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) WriteLog("INFO", format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) WriteLog("DEBUG", format, ##__VA_ARGS__)

#endif // _LOGGER_H_
