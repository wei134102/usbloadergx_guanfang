// PluginTitles.cpp
#include "PluginTitles.h"
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include "utils/Logger.h"

CPluginTitles customTitles;

// 转换字符串为小写
std::string ToLower(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
    return lowerStr;
}

CPluginTitles::CPluginTitles() {
    // 构造函数
}

CPluginTitles::~CPluginTitles() {
    // 析构函数
}

// 加载自定义标题文件
// 参数: filePath - 标题文件的路径
// 返回值: 成功加载返回true，否则返回false
bool CPluginTitles::LoadCustomTitles(const char* filePath) {
    // 打开自定义标题文件
    std::ifstream file(filePath);
    // 如果文件无法打开，则记录错误并返回false
    if (!file.is_open()) {
        WriteLog("ERROR", "无法打开自定义标题文件: %s", filePath);
        return false;
    }
    // 记录开始加载自定义标题文件的信息
    WriteLog("INFO", "开始加载自定义标题文件: %s", filePath);

    // 定义字符串变量以存储每一行的内容
    std::string line;
    // 定义变量以存储当前的魔法词（区块名称）
    std::string currentMagicWord;
    // 定义无序映射以存储当前区块的内容
    std::unordered_map<std::string, std::string> currentBlock;

    // 逐行读取文件内容
    while (std::getline(file, line)) {
        // 清理换行符和空格
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());

        // 修剪字符串前后空格
        auto trim = [](std::string& s) {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
            s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(), s.end());
        };
        trim(line);

        // 忽略空行或注释行
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;

        // 处理区块头
        if (line[0] == '[' && line.back() == ']') {
            // 如果当前魔法词和区块不为空，则将当前区块存入自定义标题映射，并清空当前区块
            if (!currentMagicWord.empty() && !currentBlock.empty()) {
                customTitlesMap[ToLower(currentMagicWord)] = currentBlock; // 转为小写
                currentBlock.clear();
            }
            // 更新当前魔法词为新的区块名称
            currentMagicWord = line.substr(1, line.size() - 2);
            continue;
        }

        // 解析键值对
        size_t eqPos = line.find('=');
        if (eqPos != std::string::npos) {
            std::string title = line.substr(0, eqPos);
            std::string customTitle = line.substr(eqPos + 1);
            trim(title);
            trim(customTitle);
            // 将解析出的键值对存入当前区块
            currentBlock[title] = customTitle; // 存入内层 map
        }
    }

    // 处理最后一个区块
    if (!currentMagicWord.empty() && !currentBlock.empty()) {
        customTitlesMap[ToLower(currentMagicWord)] = currentBlock; // 转为小写
    }

    // 记录自定义标题文件加载完成的信息
    WriteLog("INFO", "自定义标题文件加载完成: %s", filePath);
    return true;
}

const char* CPluginTitles::GetCustomTitle(const char* magicWord, const char* defaultTitle) const {
    if (!magicWord || !defaultTitle) {
        WriteLog("WARNING", "非法输入参数 magicWord=%p defaultTitle=%p", magicWord, defaultTitle);
        return defaultTitle;
    }

    // 转换 MagicWord 为小写
    std::string trimmedMagicWord = ToLower(magicWord);

    // 外层查找
    auto outerIt = customTitlesMap.find(trimmedMagicWord);
    if (outerIt == customTitlesMap.end()) {
        return defaultTitle;
    }

    // 内层查找
    const auto& innerMap = outerIt->second;
    auto innerIt = innerMap.find(defaultTitle);
    if (innerIt == innerMap.end()) {
        return defaultTitle;
    }

    // 命中结果，格式化为 新标题 (原标题)
    static std::string formattedTitle;
    formattedTitle = innerIt->second + " (" + std::string(defaultTitle) + ")";

    return formattedTitle.c_str();
}