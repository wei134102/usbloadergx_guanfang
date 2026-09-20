
#ifndef _PLUGINTITLES_H_
#define _PLUGINTITLES_H_

#include <string>
#include <unordered_map>

class CPluginTitles {
public:
    CPluginTitles();
    ~CPluginTitles();

    // 加载自定义标题配置文件
    bool LoadCustomTitles(const char* filePath);
    
    // 获取自定义标题，如果没有找到则返回默认标题
    const char* GetCustomTitle(const char* magicWord, const char* defaultTitle) const;

// private:
//     std::unordered_map<std::string, std::string> customTitlesMap;
private:
    // 外层键: magicWord (如 "43505331")
    // 内层键: 原始标题 (如 "1941")
    std::unordered_map<
        std::string, 
        std::unordered_map<std::string, std::string>
    > customTitlesMap;   
};

// 全局实例声明
extern CPluginTitles customTitles;

#endif // _PLUGINTITLES_H_