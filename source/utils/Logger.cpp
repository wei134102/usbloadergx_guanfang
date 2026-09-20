/*
 * 使用说明:
 * 1. 调用 WriteLog 函数记录日志。
 *    示例:
 *      WriteLog("INFO", "程序启动成功");
 *      WriteLog("ERROR", "无法加载文件: %s", "example.txt");
 *
 * 2. 日志文件路径为 usb1:/DEBUG.log。
 *    可在 LOG_FILE_PATH 常量中修改路径。
 *
 * 3. 日志级别包括:
 *    - "ERROR": 错误信息
 *    - "WARNING": 警告信息
 *    - "INFO": 一般信息
 *    - "DEBUG": 调试信息
 *
 * 4. 日志开关: 通过 Settings.pluginLogEnabled 控制是否输出日志
 */

#include <cstdio>
#include <cstdarg>
#include <ctime>
#include "settings/CSettings.h"

static const char* LOG_FILE_PATH = "usb1:/DEBUG.log"; // 日志文件路径

void WriteLog(const char* level, const char* format, ...) {
    // 运行时检查日志开关
    if (!Settings.pluginLogEnabled) {
        return;
    }

    FILE* logFile = fopen(LOG_FILE_PATH, "a");
    if (!logFile) return;

    // 获取当前时间
    time_t now = time(nullptr);
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // 写入日志级别和时间
    fprintf(logFile, "[%s][%s] ", timeStr, level);

    // 写入日志内容
    va_list args;
    va_start(args, format);
    vfprintf(logFile, format, args);
    va_end(args);

    fprintf(logFile, "\n");
    fclose(logFile);
}

