/****************************************************************************
 * Copyright (C) 2012 FIX94
 * Copyright (C) 2016 Fledge68
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#ifndef _PLUGIN_HPP_
#define _PLUGIN_HPP_

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include "usbloader/disc.h"
#include "wstring.hpp"
#include "PluginTitles.h"

using namespace std;

#define PLUGIN_DEV		"{device}"
#define PLUGIN_PATH		"{path}"
#define PLUGIN_NAME		"{name}"
#define PLUGIN_NOEXT	"{name_no_ext}"
#define PLUGIN_LDR		"{loader}"

/** 多插件合并模式：enabledPlugin 设为该值时将显示所有插件的游戏列表，启动时仍按每条游戏的 magic 使用对应 DOL */
#define PLUGIN_MERGE_ALL_MAGIC	"ALL"

/** 插件分组：.ini 中可选 group=xxx，配合 Settings.pluginMergeGroup 只合并同组插件 */
struct PluginOptions
{
	u32 magic;
	int romPartition;
	string romDir;
	string fileTypes;	
	string DolName;
	string coverFolder;
	string Args;
	wString DisplayName;
	string group;  ///< 可选，如 "mame2003"，用于按组合并列表
};

// 扩展的插件游戏结构：借鉴 WiiFlow 的 dir_discHdr 设计
// 直接使用 discHdr.path 字段存储完整路径，无需独立的路径向量
struct PluginDiscHdr : public discHdr
{
	u32 pluginMagic;      // 该游戏所属插件的 magic
	PluginDiscHdr() : discHdr(), pluginMagic(0) {
		memset(this, 0, sizeof(discHdr));
	}
};

// Per-plugin game list cache entry - 简化版
struct PluginGameCache
{
	vector<PluginDiscHdr*> romList;  // 扩展结构，路径已集成在 path 字段中
	bool cacheValid;                 // 内存缓存是否有效
	bool cacheLoaded;                // 是否已从磁盘加载
};

class Plugin
{
public:
	char PluginMagicWord[9];
	const char *GetDolName(u32 magic);
	const char *GetCoverFolderName(u32 magic);
	const char *GetRomPath(const char *title);		
	const char *GetRomDir(u8 pos);
	int GetRomPartition(u8 pos);
	const string& GetFileTypes(u8 pos);
	wString GetPluginName(u8 pos);
	u32 GetPluginMagic(u8 pos);
	s8 GetPluginPosition(u32 magic);
	/** 返回插件 pos 的 group（.ini 中 group=），用于判断是否属于当前合并组 */
	const char* GetPluginGroup(u8 pos);
	int PluginsSize() { return Plugins.size();}
	
	void Init();
	bool AddPlugin(const char *filepath);
	void Cleanup();
	void EndAdd();
	void TrimLine(char *dest, char *src, int size);
	
	vector<string> CreateArgs(const char *device, const char *path, 
		const char *title, const char *loader, u32 title_len_no_ext, u32 magic);
	int BootPluginRom(struct discHdr *gameHdr);
	
	bool SNESCover(const char *magic);
	bool GBCover(const char *magic);
	vector<discHdr*> ParseScummvmINI(const char *filepath, const char *Device, u32 Magic);
	bool createPluginsList();
	bool createPluginsGameList();
	bool createPluginsGameList(bool forceRescan);  ///< 内部重载：forceRescan=true 强制扫描硬盘
	bool refreshPluginsGameList();   ///< 强制从硬盘重新扫描，返回是否发生实际重建
	void forceRefresh();            ///< 标记缓存失效，下次 createPluginsGameList 会重建
	bool isCacheValid();            ///< 缓存是否有效
	bool isPluginCacheValid(u32 magic); ///< 指定 magic 的 per-plugin 缓存是否有效
	bool IsRomExists(const char *path);       ///< 检查 ROM 文件是否仍存在于磁盘
	void invalidatePluginCache(u32 magic);    ///< 删除单个插件的缓存（ROM 已不存在时调用）
	vector<discHdr*>& GetPluginsGameList() { return pluginsGameList; } ///< 获取合并后的游戏列表
	void GetFiles(const char *Path, const vector<string>& FileTypes, u32 magic, u32 depth = 1, u32 *totalSubdirs = NULL);
	void addRomToList(const char *FullPath, u32 magic);

private:
	vector<PluginOptions> Plugins;
	s8 Plugin_Pos;
	bool adding;
	bool scanning;          ///< 当前是否有插件扫描在进行
	
	// 游戏列表缓存：主列表（合并后）
	vector<discHdr*> pluginsGameList;

	// Per-plugin 缓存：key = magic（十六进制字符串如 "4D414D57"）
	map<string, PluginGameCache> perPluginCache;

	// 辅助：将 magic（u32）转为 8 位十六进制字符串 key
	string magicToKey(u32 magic);
	// 辅助：获取（或创建）指定 magic 的 cache entry
	PluginGameCache* getOrCreateCache(u32 magic);
	// 辅助：扫描单个插件，结果写入 perPluginCache[romDir]
	void scanPluginAtIndex(u8 pos);
	// 辅助：从指定插件 pos 扫描 ROM，返回 discHdr 列表（不写缓存）
	void scanPluginDirect(u8 pos, vector<discHdr*>& outList, vector<string>& outPaths);
	// 辅助：构建指定 magic 的缓存文件路径
	string buildCacheFilePath(u32 magic);
	// 辅助：构建指定 key 的缓存文件路径
	string buildCacheFilePath(const char *key);
	// 辅助：保存指定 magic 的 per-plugin 缓存到磁盘
	bool savePluginCache(u32 magic, const vector<PluginDiscHdr*>& list);
	// 辅助：从磁盘加载指定 magic 的 per-plugin 缓存
	bool loadPluginCache(u32 magic, vector<PluginDiscHdr*>& outList);
	// 辅助：按 enabledPlugin 过滤 perPluginCache 合并到 pluginsGameList
	void buildMergedList();
	// 辅助：删除单个插件的缓存文件
	bool deletePluginCache(const char *key);

	vector<string> stringToVector(const string &text, char sep);

	// 以下变量保留向后兼容（供 GetRomPath 等使用）
	vector<string> romPathsList;
	vector<discHdr*> pluginGameList;  ///< 扫描单个插件时的临时列表
};

extern Plugin m_plugin;

#endif
