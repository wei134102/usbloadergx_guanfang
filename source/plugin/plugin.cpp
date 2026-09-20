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
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <dirent.h>
#include <unistd.h>
#include <malloc.h>
#include "plugin.hpp"
#define ALIGN32(x) (((x) + 31) & ~31)
#include "utils/StringTools.h"
#include "settings/CSettings.h"
#include "Controls/DeviceHandler.hpp"
#include "homebrewboot/BootHomebrew.h"
#include "FileOperations/fileops.h"
#include "utils/Logger.h"
#include "gecko.h"
#include "utils/wifi_gecko.h"

Plugin m_plugin;

// ─────────────────────────────────────────────────────────────────────────────
// Helper: magic (u32) → 8-char hex key (uppercase), e.g. 0x4D414D57 → "4D414D57"
// ─────────────────────────────────────────────────────────────────────────────
string Plugin::magicToKey(u32 magic)
{
	char buf[9];
	snprintf(buf, sizeof(buf), "%08X", (unsigned int)magic);
	buf[8] = '\0';
	return string(buf);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: build per-plugin cache file path  <PluginsPath>lists/<MAGIC>.db
// ─────────────────────────────────────────────────────────────────────────────
string Plugin::buildCacheFilePath(u32 magic)
{
	string key = magicToKey(magic);
	char path[512];
	// PluginsPath already ends with '/', so just append "lists/<MAGIC>.db"
	snprintf(path, sizeof(path), "%slists/%s.db", Settings.PluginsPath, key.c_str());
	return string(path);
}

string Plugin::buildCacheFilePath(const char *key)
{
	char path[512];
	snprintf(path, sizeof(path), "%slists/%s.db", Settings.PluginsPath, key);
	return string(path);
}

// 缓存版本号，格式变更时递增此值
#define PLUGIN_CACHE_VERSION 2

// ─────────────────────────────────────────────────────────────────────────────
// Helper: get or create a PluginGameCache entry for |magic|
// ─────────────────────────────────────────────────────────────────────────────
PluginGameCache* Plugin::getOrCreateCache(u32 magic)
{
	string key = magicToKey(magic);
	map<string, PluginGameCache>::iterator it = perPluginCache.find(key);
	if (it != perPluginCache.end())
		return &it->second;

	// Create fresh entry
	PluginGameCache cache;
	cache.cacheValid  = false;
	cache.cacheLoaded = false;
	map<string, PluginGameCache>::value_type v(key, cache);
	std::pair<map<string, PluginGameCache>::iterator, bool> result = perPluginCache.insert(v);
	return &result.first->second;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: save one plugin's cache to disk - 简化版
// 直接序列化 PluginDiscHdr 数组，路径已集成在 path 字段中
// ─────────────────────────────────────────────────────────────────────────────
bool Plugin::savePluginCache(u32 magic, const vector<PluginDiscHdr*>& list)
{
	string cachePath = buildCacheFilePath(magic);

	// Ensure lists/ directory exists
	char listDir[512];
	snprintf(listDir, sizeof(listDir), "%slists", Settings.PluginsPath);
	mkdir(listDir, 0755);

	FILE* fp = fopen(cachePath.c_str(), "wb");
	if (!fp) {
		WriteLog("WARNING", "[CACHE] cannot open cache for save: %s", cachePath.c_str());
		return false;
	}

	// 写入版本号
	u32 version = PLUGIN_CACHE_VERSION;
	if (fwrite(&version, sizeof(u32), 1, fp) != 1) {
		fclose(fp);
		return false;
	}

	u32 count = (u32)list.size();
	if (fwrite(&count, sizeof(u32), 1, fp) != 1) {
		fclose(fp);
		return false;
	}
	// 直接写入整个 PluginDiscHdr 数组，类似 CCache::SaveAll
	for (u32 i = 0; i < count; ++i) {
		if (fwrite(list[i], sizeof(PluginDiscHdr), 1, fp) != 1) {
			WriteLog("WARNING", "[CACHE] write failed at entry %d", i);
			fclose(fp);
			return false;
		}
		// 移除 fflush，避免 USB 写入阻塞 UI 线程
		if ((i + 1) % 50 == 0 || i + 1 == count) {
			WriteLog("INFO", "[CACHE] save progress: %d/%d", i + 1, count);
		}
	}
	fclose(fp);
	WriteLog("INFO", "[CACHE] saved plugin cache: %d ROMs → %s", count, cachePath.c_str());
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: load one plugin's cache from disk - 简化版
// 直接读取 PluginDiscHdr 数组，类似 CCache::LoadAll
// ─────────────────────────────────────────────────────────────────────────────
bool Plugin::loadPluginCache(u32 magic, vector<PluginDiscHdr*>& outList)
{
	string cachePath = buildCacheFilePath(magic);
	FILE* fp = fopen(cachePath.c_str(), "rb");
	if (!fp) {
		WriteLog("INFO", "[CACHE] no disk cache found: %s", cachePath.c_str());
		return false;
	}

	// 读取版本号
	u32 version;
	if (fread(&version, sizeof(u32), 1, fp) != 1) {
		fclose(fp);
		WriteLog("WARNING", "[CACHE] failed to read version from: %s", cachePath.c_str());
		return false;
	}

	// 版本不匹配，删除旧缓存
	if (version != PLUGIN_CACHE_VERSION) {
		fclose(fp);
		WriteLog("WARNING", "[CACHE] cache version mismatch (got %d, expected %d), deleting: %s",
		         version, PLUGIN_CACHE_VERSION, cachePath.c_str());
		deletePluginCache(cachePath.c_str());
		return false;
	}

	u32 count;
	if (fread(&count, sizeof(u32), 1, fp) != 1) {
		fclose(fp);
		return false;
	}
	WriteLog("INFO", "[CACHE] loading plugin cache: %s (%d ROMs)", cachePath.c_str(), count);

	outList.clear();
	outList.reserve(count);

	for (u32 i = 0; i < count; ++i) {
		PluginDiscHdr* hdr = (PluginDiscHdr*)memalign(32, ALIGN32(sizeof(PluginDiscHdr)));
		if (!hdr) {
			WriteLog("WARNING", "[CACHE] memalign failed at entry %d/%d", i, count);
			break;
		}
		if (fread(hdr, sizeof(PluginDiscHdr), 1, fp) != 1) {
			WriteLog("WARNING", "[CACHE] read failed at entry %d", i);
			free(hdr);
			break;
		}
		outList.push_back(hdr);
		if ((i + 1) % 100 == 0 || i + 1 == count)
			WriteLog("INFO", "[CACHE] progress: %d/%d loaded", i + 1, count);
	}
	fclose(fp);

	bool ok = !outList.empty();
	if (ok)
		WriteLog("INFO", "[CACHE] plugin cache loaded: %d ROMs", outList.size());
	return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: delete a single plugin's cache file
// ─────────────────────────────────────────────────────────────────────────────
bool Plugin::deletePluginCache(const char *key)
{
	string cachePath = buildCacheFilePath(key);
	int rc = remove(cachePath.c_str());
	if (rc == 0)
		WriteLog("INFO", "[CACHE] deleted plugin cache: %s", cachePath.c_str());
	return (rc == 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// IsRomExists: check if the ROM file still exists on disk
// ─────────────────────────────────────────────────────────────────────────────
bool Plugin::IsRomExists(const char *path)
{
	if (!path || path[0] == '\0') {
		WriteLog("WARNING", "[CACHE] IsRomExists: path is NULL or empty");
		return false;
	}
	bool exists = CheckFile(path);
	if (!exists) {
		WriteLog("WARNING", "[CACHE] IsRomExists: file does not exist: %s", path);
	}
	return exists;
}

// ─────────────────────────────────────────────────────────────────────────────
// invalidatePluginCache: remove in-memory cache and disk file for one plugin
// ─────────────────────────────────────────────────────────────────────────────
void Plugin::invalidatePluginCache(u32 magic)
{
	string key = magicToKey(magic);
	map<string, PluginGameCache>::iterator it = perPluginCache.find(key);
	if (it != perPluginCache.end()) {
		// Free in-memory data
		for (size_t i = 0; i < it->second.romList.size(); ++i)
			free(it->second.romList[i]);
		it->second.romList.clear();
		it->second.cacheValid = false;
		it->second.cacheLoaded = false;
	}
	// Delete disk cache file
	deletePluginCache(magicToKey(magic).c_str());
	WriteLog("INFO", "[CACHE] invalidated plugin cache for magic=%08X", magic);
}

// ─────────────────────────────────────────────────────────────────────────────
// Initialize the plugin system
// ─────────────────────────────────────────────────────────────────────────────
void Plugin::Init()
{
	Plugins.clear();
	PluginMagicWord[8] = '\0';
	adding = true;
	scanning = false;

	char titlespath[300];
	snprintf(titlespath, sizeof(titlespath), "%scustom_titles.ini", Settings.ConfigPath);
	WriteLog("INFO", "成功找到自定义标题文件 %s\n", titlespath);
	customTitles.LoadCustomTitles(titlespath);
}

// ─────────────────────────────────────────────────────────────────────────────
// wchar string compare (case-insensitive)
// ─────────────────────────────────────────────────────────────────────────────
static bool wchar_cmp(const wchar_t *first, const wchar_t *second, u32 first_len, u32 second_len)
{
	u32 i = 0;
	while((i < first_len) && (i < second_len))
	{
		if(tolower(first[i]) < tolower(second[i])) return true;
		else if(tolower(first[i]) > tolower(second[i])) return false;
		++i;
	}
	return first_len < second_len;
}

static bool PluginOptions_cmp(PluginOptions lhs, PluginOptions rhs)
{
	const wchar_t *first  = lhs.DisplayName.c_str();
	const wchar_t *second = rhs.DisplayName.c_str();
	return wchar_cmp(first, second, wcslen(first), wcslen(second));
}

void Plugin::EndAdd()
{
	std::sort(Plugins.begin(), Plugins.end(), PluginOptions_cmp);
	adding = false;
}

void Plugin::Cleanup()
{
	Plugins.clear();
	adding = true;
	scanning = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// AddPlugin: parse one .ini file
// ─────────────────────────────────────────────────────────────────────────────
bool Plugin::AddPlugin(const char *filepath)
{
	if(!adding) return false;

	PluginOptions NewPlugin;
	FILE *file = fopen(filepath, "r");
	if (!file) return false;

	string PluginName;
	NewPlugin.romPartition = Settings.PluginRomPart;
	char line[256], temp[256], name[256], value[256];

	while (fgets(line, sizeof(line), file))
	{
		if (line[0] == '#') continue;
		if (line[0] == '[') continue;

		strncpy(temp, line, sizeof(temp));
		char *eq = strchr(temp, '=');
		if (!eq) continue;

		*eq = 0;
		this->TrimLine(name, temp, sizeof(name));
		this->TrimLine(value, eq + 1, sizeof(value));

		if      (strcmp(name, "dolfile")      == 0) NewPlugin.DolName       = value;
		else if (strcmp(name, "coverfolder")  == 0) NewPlugin.coverFolder   = value;
		else if (strcmp(name, "magic")        == 0) {
			strncpy(PluginMagicWord, value, 8);
			PluginMagicWord[8] = '\0';
			NewPlugin.magic = strtoul(value, NULL, 16);
		}
		else if (strcmp(name, "romdir")       == 0) NewPlugin.romDir        = value;
		else if (strcmp(name, "filetypes")    == 0) NewPlugin.fileTypes     = value;
		else if (strcmp(name, "rompartition") == 0) NewPlugin.romPartition  = strtol(value, NULL, 10);
		else if (strcmp(name, "displayname")  == 0) PluginName              = value;
		else if (strcmp(name, "arguments")    == 0) NewPlugin.Args          = value;
		else if (strcmp(name, "group")        == 0) NewPlugin.group         = value;
	}
	fclose(file);

	if(PluginName.size() < 2)
	{
		PluginName = NewPlugin.DolName;
		PluginName.erase(PluginName.end() - 4, PluginName.end());
	}
	NewPlugin.DisplayName.fromUTF8(PluginName.c_str());

	// Ignore reserved magic words
	if(strncasecmp(PluginMagicWord, "4E47434D", 8) == 0) return false;
	if(strncasecmp(PluginMagicWord, "4E574949", 8) == 0) return false;
	if(strncasecmp(PluginMagicWord, "4E414E44", 8) == 0) return false;
	if(strncasecmp(PluginMagicWord, "454E414E", 8) == 0) return false;
	if(strncasecmp(PluginMagicWord, "4D555343", 8) == 0) return false;

	Plugins.push_back(NewPlugin);
	return true;
}

void Plugin::TrimLine(char *dest, char *src, int size)
{
	while (*src == ' ') src++;
	int len = strlen(src);
	while (len > 0 && strchr(" \r\n", src[len - 1])) len--;
	if (len >= size) len = size - 1;
	strncpy(dest, src, len);
	dest[len] = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Getters
// ─────────────────────────────────────────────────────────────────────────────
s8 Plugin::GetPluginPosition(u32 magic)
{
	for(u8 pos = 0; pos < Plugins.size(); pos++)
		if(magic == Plugins[pos].magic) return (s8)pos;
	return -1;
}

u32 Plugin::GetPluginMagic(u8 pos) { return Plugins[pos].magic; }

const char *Plugin::GetDolName(u32 magic)
{
	if((Plugin_Pos = GetPluginPosition(magic)) >= 0)
		return Plugins[Plugin_Pos].DolName.c_str();
	return NULL;
}

const char *Plugin::GetCoverFolderName(u32 magic)
{
	if((Plugin_Pos = GetPluginPosition(magic)) >= 0)
		return Plugins[Plugin_Pos].coverFolder.c_str();
	return NULL;
}

const char *Plugin::GetRomPath(const char *title)
{
	if(!title || title[0] == '\0') return "";
	if(pluginsGameList.empty()) {
		WriteLog("WARNING", "[CACHE] GetRomPath: pluginsGameList is empty!");
		return "";
	}
	for(u32 i = 0; i < pluginsGameList.size(); i++)
		if(strcasecmp((const char*)pluginsGameList[i]->title, title) == 0) {
			if(pluginsGameList[i]->path[0] != '\0')
				return pluginsGameList[i]->path;
			WriteLog("WARNING", "[CACHE] GetRomPath: path field is empty for title='%s'", title);
			return "";
		}
	WriteLog("WARNING", "[CACHE] GetRomPath NOT FOUND: title='%s'", title);
	return "";
}

wString Plugin::GetPluginName(u8 pos) { return Plugins[pos].DisplayName; }

int Plugin::GetRomPartition(u8 pos) { return Plugins[pos].romPartition; }

const char *Plugin::GetRomDir(u8 pos) { return Plugins[pos].romDir.c_str(); }

const string& Plugin::GetFileTypes(u8 pos) { return Plugins[pos].fileTypes; }

const char* Plugin::GetPluginGroup(u8 pos)
{
	if (pos >= Plugins.size()) return "";
	return Plugins[pos].group.c_str();
}

// ─────────────────────────────────────────────────────────────────────────────
// SNES / GB cover magic detection
// ─────────────────────────────────────────────────────────────────────────────
static const char SNESCovers[4][9] = {
"534e5854", //Snes9X-Next
"534e4553", //SNES9xGX
"4e4f3634", //Not64
"57493634"  //Wii64
};

static const char GBCovers[12][9] = {
"474d4254", "474d4264", "474d4274", "56425854", "56424158", "56424168",
"56424178", "56424188", "4d45445e", "4d45446e", "4d45447e", "57495358"
};

bool Plugin::SNESCover(const char *magic)
{
	if(magic == NULL) return false;
	for(int i = 0; i < 4; i++)
		if(strncasecmp(magic, SNESCovers[i], 8) == 0) return true;
	return false;
}

bool Plugin::GBCover(const char *magic)
{
	if(magic == NULL) return false;
	for(int i = 0; i < 12; i++)
		if(strncasecmp(magic, GBCovers[i], 8) == 0) return true;
	return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateArgs: substitute {device}/{path}/{name}/{name_no_ext}/{loader}
// ─────────────────────────────────────────────────────────────────────────────
vector<string> Plugin::CreateArgs(const char *device, const char *path,
			const char *title, const char *loader, u32 title_len_no_ext, u32 magic)
{
	vector<string> args;
	Plugin_Pos = GetPluginPosition(magic);
	if(Plugin_Pos < 0) return args;

	vector<string> PluginArgs = stringToVector(Plugins[Plugin_Pos].Args, '|');
	for(vector<string>::const_iterator arg = PluginArgs.begin(); arg != PluginArgs.end(); ++arg)
	{
		string Argument(*arg);
		if(Argument.find(PLUGIN_DEV)    != string::npos) Argument.replace(Argument.find(PLUGIN_DEV),    strlen(PLUGIN_DEV),    device);
		if(Argument.find(PLUGIN_PATH)   != string::npos) Argument.replace(Argument.find(PLUGIN_PATH),   strlen(PLUGIN_PATH),   path);
		if(Argument.find(PLUGIN_NAME)   != string::npos) Argument.replace(Argument.find(PLUGIN_NAME),   strlen(PLUGIN_NAME),   title);
		if(Argument.find(PLUGIN_LDR)    != string::npos) Argument.replace(Argument.find(PLUGIN_LDR),    strlen(PLUGIN_LDR),    loader);
		if(Argument.find(PLUGIN_NOEXT)  != string::npos) Argument.replace(Argument.find(PLUGIN_NOEXT), strlen(PLUGIN_NOEXT), title, title_len_no_ext);
		args.push_back(Argument);
	}
	return args;
}

// ─────────────────────────────────────────────────────────────────────────────
// ParseScummvmINI – unchanged from original
// ─────────────────────────────────────────────────────────────────────────────
vector<discHdr*> Plugin::ParseScummvmINI(const char *filepath, const char *Device, u32 Magic)
{
	vector<discHdr*> gameHeader;
	FILE * file = fopen(filepath, "r");
	if (!file) return gameHeader;

	discHdr* ListElement = NULL;
	char line[256], temp[256], name[256], value[256];
	while(fgets(line, sizeof(line), file)) {
		if(line[0] == '[') break;
	}

	while(1)
	{
		const char *GameDomain = NULL;
		const char *GameName = NULL;
		const char *GameDevice = NULL;
		char *i = strchr(line, '[');
		if(i == NULL) break;
		strncpy(temp, i + 1, sizeof(temp));
		char *closeBracket = strchr(temp, ']');
		if (!closeBracket) break;
		*closeBracket = '\0';
		GameDomain = temp;

		while(fgets(line, sizeof(line), file))
		{
			if(line[0] == '#') continue;
			if(line[0] == '[') break;
			strncpy(temp, line, sizeof(temp));
			char *eq = strchr(temp, '=');
			if (!eq) continue;
			*eq = 0;
			this->TrimLine(name, temp, sizeof(name));
			this->TrimLine(value, eq + 1, sizeof(value));
			if(strcmp(name, "description") == 0) GameName = value;
			else if(strcmp(name, "path") == 0) {
				GameDevice = value;
				*strchr((char*)GameDevice, '/') = '\0';
			}
		}
		if(strlen(GameName) < 2 || strncasecmp(Device, GameDevice, 2) != 0) continue;

		ListElement = (discHdr*)memalign(32, ALIGN32(sizeof(discHdr)));
		if (!ListElement) continue;
		memset(ListElement, 0, sizeof(discHdr));
		strncpy(ListElement->title, GameName, 63);
		ListElement->magic = Magic;
		romPathsList.push_back(fmt("%s/%s", GameDevice, GameDomain));
		gameHeader.push_back(ListElement);
	}
	fclose(file);
	return gameHeader;
}

// ─────────────────────────────────────────────────────────────────────────────
// createPluginsList – unchanged from original
// ─────────────────────────────────────────────────────────────────────────────
bool Plugin::createPluginsList()
{
	if(!adding) return false;
	char pluginFilePath[256];
	DIR *pdir = opendir(Settings.PluginsPath);
	if(pdir == NULL) return false;

	dirent *pent = NULL;
	const char *fileExt = NULL;
	while((pent = readdir(pdir)) != NULL)
	{
		if(pent->d_name[0] == '.' || strcmp(pent->d_name, "scummvm.ini") == 0) continue;
		snprintf(pluginFilePath, sizeof(pluginFilePath), "%s%s", Settings.PluginsPath, pent->d_name);
		if(pent->d_type == DT_REG)
		{
			fileExt = strrchr(pent->d_name, '.');
			if(fileExt != NULL && strcmp(fileExt, ".ini") == 0)
				AddPlugin(pluginFilePath);
		}
	}
	closedir(pdir);
	EndAdd();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// IsFileSupported – check if a filename matches any supported extension
static inline bool IsFileSupported(const char *File, const vector<string>& FileTypes)
{
	for(vector<string>::const_iterator cmp = FileTypes.begin(); cmp != FileTypes.end(); ++cmp)
		if(strcasecmp(File, cmp->c_str()) == 0) return true;
	return false;
}

void Plugin::scanPluginDirect(u8 pos, vector<discHdr*>& outList, vector<string>& outPaths)
{
	string romDir  = GetRomDir(pos);
	u32      magic = GetPluginMagic(pos);
	u32      part  = (u32)GetRomPartition(pos);

	if (romDir.find("scummvm.ini") == string::npos) {
		const char *fullRomDirPath = fmt("%s:/%s", DeviceName[part], romDir.c_str());
		const vector<string> fileTypes = stringToVector(GetFileTypes(pos), '|');
		char FullRomPath[256];
		DIR *pdir = opendir(fullRomDirPath);
		if (!pdir) return;
		dirent *pent = NULL;
		const char *fileExt = NULL;
		while((pent = readdir(pdir)) != NULL)
		{
			if(pent->d_name[0] == '.') continue;
			snprintf(FullRomPath, sizeof(FullRomPath), "%s/%s", fullRomDirPath, pent->d_name);
			if(pent->d_type == DT_REG)
			{
				fileExt = strrchr(pent->d_name, '.');
				if(fileExt == NULL) fileExt = pent->d_name;
				if(IsFileSupported(fileExt, fileTypes))
				{
					discHdr* ListElement = (discHdr*)memalign(32, ALIGN32(sizeof(discHdr)));
					if (!ListElement) continue;
					memset(ListElement, 0, sizeof(discHdr));
					const char *RomTitle = strrchr(FullRomPath, '/') + 1;
					char titleCopy[128];
					strncpy(titleCopy, RomTitle, sizeof(titleCopy)-1);
					titleCopy[sizeof(titleCopy)-1] = '\0';
					char *dot = strrchr(titleCopy, '.');
					if (dot) *dot = '\0';
					char magicWord[9];
					snprintf(magicWord, sizeof(magicWord), "%08X", (unsigned int)magic);
					const char *customTitle = customTitles.GetCustomTitle(magicWord, titleCopy);
					if (customTitle && customTitle[0] != '\0')
						strncpy(ListElement->title, customTitle, 63);
					else
						strncpy(ListElement->title, titleCopy, 63);
					ListElement->title[63] = '\0';
					ListElement->magic = magic;
					strncpy(ListElement->path, FullRomPath, sizeof(ListElement->path)-1);
					outList.push_back(ListElement);
					outPaths.push_back(string(FullRomPath));
				}
			}
		}
		closedir(pdir);
	} else {
		vector<discHdr*> scummList = ParseScummvmINI(
			fmt("%s%s", Settings.PluginsPath, "scummvm.ini"),
			DeviceName[part], magic);
		for (size_t hIdx = 0; hIdx < scummList.size(); ++hIdx) {
			outList.push_back(scummList[hIdx]);
			outPaths.push_back(string(scummList[hIdx]->title));
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// scanPluginAtIndex – full recursive scan mirroring original GetFiles + addRomToList
// Writes results directly into the PluginGameCache entry.
// ─────────────────────────────────────────────────────────────────────────────
void Plugin::scanPluginAtIndex(u8 pos)
{
	string romDir  = GetRomDir(pos);
	u32      magic = GetPluginMagic(pos);
	u32      part  = (u32)GetRomPartition(pos);

	PluginGameCache* cache = getOrCreateCache(magic);

	if (romDir.find("scummvm.ini") == string::npos) {
		// ── Recursive ROM dir scan ───────────────────────────────────────────
		const char *fullRomDirPath = fmt("%s:/%s", DeviceName[part], romDir.c_str());
		const vector<string> fileTypes = stringToVector(GetFileTypes(pos), '|');

		// Save current state, scan into members, then copy to cache
		vector<discHdr*> savedPluginList = pluginGameList;
		vector<string>   savedRomPaths   = romPathsList;
		pluginGameList.clear();
		romPathsList.clear();
		u32 totalSubdirs = 0;  // Reset subdir counter for each plugin scan
		GetFiles(fullRomDirPath, fileTypes, magic, 0, &totalSubdirs);

		// Copy results into cache - 转换为 PluginDiscHdr
		WriteLog("INFO", "[SCANN] copying %u ROMs to cache...", (u32)pluginGameList.size());
		
		// 内存保护：限制每个插件最多缓存 500 个 ROM
		const u32 MAX_ROMS_PER_PLUGIN = 500;
		u32 copyCount = 0;
		
		for (size_t i = 0; i < pluginGameList.size(); ++i) {
			if (copyCount >= MAX_ROMS_PER_PLUGIN) {
				WriteLog("WARNING", "[SCANN] reached cache limit (%u), skipping remaining ROMs", MAX_ROMS_PER_PLUGIN);
				break;
			}
			
			PluginDiscHdr* pluginHdr = (PluginDiscHdr*)memalign(32, ALIGN32(sizeof(PluginDiscHdr)));
			if (!pluginHdr) {
				WriteLog("WARNING", "[SCANN] memalign failed at ROM %u/%u, stopping", (u32)(i + 1), (u32)pluginGameList.size());
				break;
			}
			memcpy((discHdr*)pluginHdr, pluginGameList[i], sizeof(discHdr));
			pluginHdr->pluginMagic = magic;
			cache->romList.push_back(pluginHdr);
			copyCount++;
			
			if (copyCount % 50 == 0 || copyCount == pluginGameList.size()) {
				WriteLog("INFO", "[SCANN] copy progress: %u/%u", copyCount, (u32)pluginGameList.size());
				fflush(stdout);
			}
		}
		WriteLog("INFO", "[SCANN] copy completed: %u ROMs copied", (u32)cache->romList.size());

		// Restore original lists
		pluginGameList = savedPluginList;
		romPathsList   = savedRomPaths;

	} else {
		// ── ScummVM ──────────────────────────────────────────────────────────
		vector<discHdr*> scummList = ParseScummvmINI(
			fmt("%s%s", Settings.PluginsPath, "scummvm.ini"),
			DeviceName[part], magic);
		for (size_t i = 0; i < scummList.size(); ++i) {
			PluginDiscHdr* pluginHdr = (PluginDiscHdr*)memalign(32, ALIGN32(sizeof(PluginDiscHdr)));
			if (pluginHdr) {
				memcpy((discHdr*)pluginHdr, scummList[i], sizeof(discHdr));
				pluginHdr->pluginMagic = magic;
				strncpy(pluginHdr->path, romPathsList[romPathsList.size() - scummList.size() + i].c_str(), 
				        sizeof(pluginHdr->path) - 1);
				pluginHdr->path[sizeof(pluginHdr->path) - 1] = '\0';
				cache->romList.push_back(pluginHdr);
			}
		}
	}

	cache->cacheValid = true;
	string key = magicToKey(magic);
	WriteLog("INFO", "[SCANN] plugin %d magic=%s done: %d ROMs",
	         pos, key.c_str(), (int)cache->romList.size());
}

// ─────────────────────────────────────────────────────────────────────────────
// buildMergedList – filter per-plugin caches into pluginsGameList based on
//                  enabledPlugin / pluginMergeGroup / ALL mode.
// ─────────────────────────────────────────────────────────────────────────────
void Plugin::buildMergedList()
{
	pluginsGameList.clear();
	// romPathsList 不再需要，路径已集成在 discHdr->path 字段中

	bool mergeByGroup = Settings.pluginShowMerged && (Settings.pluginMergeGroup[0] != '\0');
	bool mergeAll     = Settings.pluginShowMerged &&
	                    (strncasecmp(Settings.enabledPlugin, PLUGIN_MERGE_ALL_MAGIC, 3) == 0);

	// Fallback: if enabledPlugin is empty, include all caches
	if (Settings.enabledPlugin[0] == '\0' && !mergeByGroup && !mergeAll)
		mergeAll = true;

	for (map<string, PluginGameCache>::iterator kvIt = perPluginCache.begin(); kvIt != perPluginCache.end(); ++kvIt) {
		const string& key   = kvIt->first;
		PluginGameCache& cache = kvIt->second;
		if (!cache.cacheValid || cache.romList.empty()) continue;

		bool include = false;
		if (mergeAll) {
			include = true;
		} else if (mergeByGroup) {
			// Find plugin pos matching this magic
			u32 magic = 0;
			unsigned int magicTmp = 0;
			sscanf(key.c_str(), "%08X", &magicTmp);
			magic = (u32)magicTmp;
			for (u8 i = 0; i < (u8)Plugins.size(); ++i) {
				if (Plugins[i].magic == magic &&
				    !Plugins[i].group.empty() &&
				    strcasecmp(Plugins[i].group.c_str(), Settings.pluginMergeGroup) == 0) {
					include = true;
					break;
				}
			}
		} else {
			// Single-plugin mode
			u32 enabledMagic = 0;
			sscanf(Settings.enabledPlugin, "%08X", (unsigned int*)&enabledMagic);
			if (enabledMagic == 0) {
				// enabledPlugin is stored as hex string – try parsing
				enabledMagic = (u32)strtoul(Settings.enabledPlugin, NULL, 16);
			}
			u32 cacheMagic = 0;
			unsigned int cacheMagicTmp = 0;
			sscanf(key.c_str(), "%08X", &cacheMagicTmp);
			cacheMagic = (u32)cacheMagicTmp;
			include = (cacheMagic == enabledMagic);
		}

		if (include) {
			for (size_t i = 0; i < cache.romList.size(); ++i) {
				// 复制 PluginDiscHdr 到新的 discHdr，避免指针转换问题
				discHdr* newHdr = (discHdr*)memalign(32, ALIGN32(sizeof(discHdr)));
				if (newHdr) {
					memset(newHdr, 0, sizeof(discHdr));
					memcpy(newHdr, cache.romList[i], sizeof(discHdr));
					pluginsGameList.push_back(newHdr);
				}
			}
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// createPluginsGameList – NEW: per-plugin lazy-loading cache
//
// Flow:
//   1. Determine which plugins are "target" based on enabledPlugin / merge mode.
//   2. For each target plugin:
//        a. Check per-plugin disk cache → if valid, load it.
//        b. If no cache or forceRescan, scan the ROM dir, save cache, then load.
//   3. buildMergedList() → populate pluginsGameList & romPathsList.
//
// This avoids scanning ALL plugins at startup; only the active one is scanned.
// ─────────────────────────────────────────────────────────────────────────────
bool Plugin::createPluginsGameList()
{
	return createPluginsGameList(false);
}

// ─────────────────────────────────────────────────────────────────────────────
// createPluginsGameList – per-plugin lazy-loading cache
//
// Flow:
//   1. Determine target plugins based on enabledPlugin / merge mode.
//   2. For each target plugin:
//        a. Check per-plugin disk cache – if valid, load it.
//        b. If no cache or forceRescan, scan the ROM dir, save cache.
//   3. buildMergedList() – populate pluginsGameList & romPathsList.
//
// This avoids scanning ALL plugins at startup; only the active one is scanned.
// ─────────────────────────────────────────────────────────────────────────────
bool Plugin::createPluginsGameList(bool forceRescan)
{
	WriteLog("INFO", "[CACHE] createPluginsGameList(forceRescan=%d)", forceRescan);

	// 扫描保护：如果有插件正在扫描中，等待其完成（最多等10秒）
	if (scanning) {
		WriteLog("INFO", "[CACHE] scan in progress, waiting up to 10s ...");
		for (int wait = 0; wait < 100; ++wait) {
			usleep(100000);
			if (!scanning) break;
		}
		if (scanning) {
			WriteLog("WARNING", "[CACHE] scan wait timeout, proceeding anyway");
		}
	}

	if (forceRescan) {
		// Delete all per-plugin caches on disk
		map<string, PluginGameCache>::iterator kvIt2;
		for (kvIt2 = perPluginCache.begin(); kvIt2 != perPluginCache.end(); ++kvIt2) {
			deletePluginCache(kvIt2->first.c_str());
		}
		perPluginCache.clear();
	}

	// Step 1: determine target plugin indices
	// 若 enabledPlugin 为空（首次运行或未保存过设置），自动选择第一个插件
	if (Plugins.size() > 0 && (Settings.enabledPlugin[0] == '\0')) {
		u32 firstMagic = GetPluginMagic(0);
		snprintf(Settings.enabledPlugin, sizeof(Settings.enabledPlugin), "%08X", (unsigned int)firstMagic);
		Settings.enabledPlugin[sizeof(Settings.enabledPlugin)-1] = '\0';
		WriteLog("INFO", "[CACHE] enabledPlugin was empty, auto-selected first plugin magic=%s", Settings.enabledPlugin);
	}
	vector<u8> targetPluginIndices;
	bool mergeByGroup = Settings.pluginShowMerged && (Settings.pluginMergeGroup[0] != '\0');
	bool mergeAll     = Settings.pluginShowMerged &&
	                    (strncasecmp(Settings.enabledPlugin, PLUGIN_MERGE_ALL_MAGIC, 3) == 0);

	for (u8 i = 0; i < (u8)Plugins.size(); ++i) {
		if (mergeByGroup) {
			if (!Plugins[i].group.empty() &&
			    strcasecmp(Plugins[i].group.c_str(), Settings.pluginMergeGroup) == 0)
				targetPluginIndices.push_back(i);
		} else if (mergeAll) {
			targetPluginIndices.push_back(i);
		} else {
			u32 magic = GetPluginMagic(i);
			char magicStr[9];
			snprintf(magicStr, sizeof(magicStr), "%08X", (unsigned int)magic);
			if (strncasecmp(Settings.enabledPlugin, magicStr, 8) == 0)
				targetPluginIndices.push_back(i);
		}
	}

	WriteLog("INFO", "[CACHE] target plugins: %d", (int)targetPluginIndices.size());

	// Step 2: for each target plugin, lazy-load or scan+save cache
	for (size_t ti = 0; ti < targetPluginIndices.size(); ++ti) {
		u8 pos = targetPluginIndices[ti];
		u32 magic = GetPluginMagic(pos);
		string key = magicToKey(magic);

		PluginGameCache* cache = getOrCreateCache(magic);

		if (!forceRescan && cache->cacheValid && !cache->romList.empty()) {
			WriteLog("INFO", "[CACHE] plugin %d magic=%s in-memory cache hit", pos, key.c_str());
			// 不要 continue，继续执行下面的逻辑确保 buildMergedList 被调用
		}
		// Try disk cache first
		else if (!forceRescan) {
			bool loaded = loadPluginCache(magic, cache->romList);
			if (loaded) {
				cache->cacheValid  = true;
				cache->cacheLoaded = true;
				WriteLog("INFO", "[CACHE] plugin %d magic=%s disk cache loaded (%d ROMs)",
				         pos, key.c_str(), (int)cache->romList.size());
			} else {
				WriteLog("WARNING", "[CACHE] plugin %d magic=%s disk cache load FAILED", pos, key.c_str());
			}
		}

		// No cache – scan the ROM dir
		if (cache->romList.empty()) {
			scanning = true;
			WriteLog("INFO", "[SCANN] scanning plugin %d magic=%s romDir=%s",
			         pos, key.c_str(), GetRomDir(pos));
			scanPluginAtIndex(pos);

			// Scan done – log before save (save can hang on slow USB)
			WriteLog("INFO", "[SCANN] plugin %d magic=%s scan done: %d ROMs",
			         pos, key.c_str(), (int)cache->romList.size());

			// Save to disk for next time
			if (!cache->romList.empty())
				savePluginCache(magic, cache->romList);

			WriteLog("INFO", "[SCANN] plugin %d magic=%s saved", pos, key.c_str());
		}
	}

	// Step 3: build merged list from per-plugin caches
	// Clear existing list first
	for (size_t i = 0; i < pluginsGameList.size(); ++i) {
		if (pluginsGameList[i]) free(pluginsGameList[i]);
	}
	pluginsGameList.clear();
	romPathsList.clear();

	WriteLog("INFO", "[CACHE] building merged list from %d target caches", (int)targetPluginIndices.size());
	
	// Only merge target plugins
	for (size_t ti = 0; ti < targetPluginIndices.size(); ++ti) {
		u8 pos = targetPluginIndices[ti];
		u32 magic = GetPluginMagic(pos);
		string key = magicToKey(magic);
		map<string, PluginGameCache>::iterator it = perPluginCache.find(key);
		if (it != perPluginCache.end() && !it->second.romList.empty()) {
			for (size_t ri = 0; ri < it->second.romList.size(); ++ri) {
				PluginDiscHdr* src = it->second.romList[ri];
				discHdr* dest = (discHdr*)memalign(32, ALIGN32(sizeof(discHdr)));
				if (dest) {
					memcpy(dest, src, sizeof(discHdr));
					pluginsGameList.push_back(dest);
					romPathsList.push_back(string(src->path));
				}
			}
		}
	}
	
	WriteLog("INFO", "[CACHE] merged list built: %d games total", (int)pluginsGameList.size());
	scanning = false;
	return true;
}

bool Plugin::refreshPluginsGameList()
{
	return createPluginsGameList(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// forceRefresh – invalidate ALL caches (legacy: deletes old global cache too)
// ─────────────────────────────────────────────────────────────────────────────
void Plugin::forceRefresh()
{
	perPluginCache.clear();
	// Also delete old global cache for compatibility
	char oldCacheFile[512];
	snprintf(oldCacheFile, sizeof(oldCacheFile), "%slists/cache.db", Settings.PluginsPath);
	remove(oldCacheFile);
	WriteLog("INFO", "[CACHE] all caches invalidated");
}

// ─────────────────────────────────────────────────────────────────────────────
// Legacy cache API (kept for backward compatibility with callers that check
// isCacheValid / cleanupGameListCache etc.)
// ─────────────────────────────────────────────────────────────────────────────
bool Plugin::isCacheValid()
{
	return !pluginsGameList.empty();
}

bool Plugin::isPluginCacheValid(u32 magic)
{
	string key = magicToKey(magic);
	map<string, PluginGameCache>::iterator it = perPluginCache.find(key);
	if (it == perPluginCache.end()) return false;
	return it->second.cacheValid && !it->second.romList.empty();
}

// ─────────────────────────────────────────────────────────────────────────────
// GetFiles – unchanged from original (recursive dir scanner)
// ─────────────────────────────────────────────────────────────────────────────
const char *fileExt = NULL;
dirent *pent = NULL;
DIR  *pdir  = NULL;

void Plugin::GetFiles(const char *Path, const vector<string>& FileTypes, u32 magic, u32 depth, u32 *totalSubdirs)
{
	u32 localSubdirs = 0;
	if (totalSubdirs == NULL)
		totalSubdirs = &localSubdirs;

	char FullRomPath[256];
	vector<string> SubPaths;
	u32 fileCount = 0;
	const u32 MAX_SUBDIRS = 100;  // Limit subdirs to prevent UI freeze

	pdir = opendir(Path);
	if(pdir == NULL) return;

	while((pent = readdir(pdir)) != NULL)
	{
		if(pent->d_name[0] == '.') continue;
		snprintf(FullRomPath, sizeof(FullRomPath), "%s/%s", Path, pent->d_name);

		if(pent->d_type == DT_DIR && depth < 10 && *totalSubdirs < MAX_SUBDIRS)
		{
			SubPaths.push_back(FullRomPath);
			(*totalSubdirs)++;
		}
		else if(pent->d_type == DT_REG)
		{
			fileExt = strrchr(pent->d_name, '.');
			if(fileExt == NULL) fileExt = pent->d_name;
			if(IsFileSupported(fileExt, FileTypes))
			{
				addRomToList(FullRomPath, magic);
				++fileCount;
				if (fileCount % 50 == 0)
					WriteLog("INFO", "[SCANN] %08X scanned %u files in %s",
					         magic, fileCount, Path);
			}
		}
	}
	closedir(pdir);
	WriteLog("INFO", "[SCANN] %08X done %s: %u files, %u subdirs",
	         magic, Path, fileCount, (u32)SubPaths.size());

	if (*totalSubdirs >= MAX_SUBDIRS)
	{
		WriteLog("WARNING", "[SCANN] %08X reached subdir limit (%u), skipping remaining subdirs",
		         magic, MAX_SUBDIRS);
	}

	for(vector<string>::const_iterator p = SubPaths.begin(); p != SubPaths.end(); ++p)
	{
		WriteLog("INFO", "[SCANN] %08X entering subdir: %s (depth=%u)",
		         magic, p->c_str(), depth + 1);
		GetFiles(p->c_str(), FileTypes, magic, depth + 1, totalSubdirs);
	}
	SubPaths.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// addRomToList – unchanged from original
// ─────────────────────────────────────────────────────────────────────────────
void Plugin::addRomToList(const char *FullPath, u32 magic)
{
	if(!FullPath) { gprintf("【错误】空路径参数\n"); return; }

	const char *fileName = strrchr(FullPath, '/');
	fileName = fileName ? fileName + 1 : FullPath;

	static const char* excludedFiles[] = { "pgm.zip", "neogeo.zip", NULL };
	for(int i = 0; excludedFiles[i]; ++i)
		if(strcasecmp(fileName, excludedFiles[i]) == 0) {
			gprintf("【排除】跳过 %s\n", fileName);
			return;
		}

	static const u32 titleCheckMagics[] = { 0x50475648, 0 };
	bool needCheckTitle = false;
	for(int i = 0; titleCheckMagics[i]; ++i)
		if(magic == titleCheckMagics[i]) { needCheckTitle = true; break; }

	discHdr* ListElement = (discHdr*)memalign(32, ALIGN32(sizeof(discHdr)));
	if (!ListElement) return;
	memset(ListElement, 0, sizeof(discHdr));

	// 先保存完整路径（包含扩展名）
	strncpy(ListElement->path, FullPath, 259);
	ListElement->path[259] = '\0';
	romPathsList.push_back(string(FullPath));

	const char *RomTitle = strrchr(FullPath, '/') + 1;
	char *dot = strrchr((char*)RomTitle, '.');
	if (dot) *dot = '\0';

	if(needCheckTitle) {
		char titlePath[512];
		strncpy(titlePath, FullPath, sizeof(titlePath)-1);
		titlePath[sizeof(titlePath)-1] = '\0';
		char* lastSlash = strrchr(titlePath, '/');
		if(lastSlash) {
			strncpy(lastSlash+1, "title.txt", sizeof(titlePath)-(size_t)(lastSlash+1-titlePath)-1);
			FILE* titleFile = fopen(titlePath, "r");
			if(titleFile) {
				char fileTitle[64] = {0};
				if(fgets(fileTitle, sizeof(fileTitle), titleFile)) {
					char *newline = strchr(fileTitle, '\n');
					if(newline) *newline = '\0';
					if(fileTitle[0] != '\0') {
						strncpy(ListElement->title, fileTitle, 63);
						ListElement->title[63] = '\0';
						fclose(titleFile);
						ListElement->magic = magic;
						strncpy(ListElement->path, FullPath, 259);
						ListElement->path[259] = '\0';
						pluginGameList.push_back(ListElement);
						return;
					}
				}
				fclose(titleFile);
			}
		}
	}

	char magicWord[9];
	snprintf(magicWord, sizeof(magicWord), "%08X", (unsigned int)magic);
	const char *CustomTitle = customTitles.GetCustomTitle(magicWord, RomTitle);
	if(CustomTitle && CustomTitle[0] != '\0')
		strncpy(ListElement->title, CustomTitle, 63);
	else
		strncpy(ListElement->title, RomTitle, 63);
	ListElement->title[63] = '\0';
	ListElement->magic = magic;
	pluginGameList.push_back(ListElement);
}

// ─────────────────────────────────────────────────────────────────────────────
// stringToVector – unchanged from original
// ─────────────────────────────────────────────────────────────────────────────
vector<string> Plugin::stringToVector(const string &text, char sep)
{
	vector<string> v;
	if (text.empty()) return v;
	u32 count = 1;
	for (u32 i = 0; i < text.size(); ++i)
		if (text[i] == sep) ++count;
	v.reserve(count);
	string::size_type off = 0, i = 0;
	do {
		i = text.find_first_of(sep, off);
		if (i != string::npos) {
			v.push_back(text.substr(off, i - off));
			off = i + 1;
		} else {
			v.push_back(text.substr(off));
		}
	} while (i != string::npos);
	return v;
}

// ─────────────────────────────────────────────────────────────────────────────
// BootPluginRom – with ROM existence check and cache invalidation
// ─────────────────────────────────────────────────────────────────────────────
int Plugin::BootPluginRom(struct discHdr *gameHdr)
{
	struct discHdr gameHeader;
	memcpy(&gameHeader, gameHdr, sizeof(struct discHdr));

	char title[101];
	memset(&title, 0, sizeof(title));
	u32 title_len_no_ext = 0;
	const char *rom_path = GetRomPath(gameHeader.title);
	const char *path = NULL;
	const char *device = NULL;

	if(strchr(rom_path, '.') != NULL)
	{
		strncpy(title, strrchr(rom_path, '/') + 1, 100);
		if(strchr(rom_path, '.') != NULL)
			title_len_no_ext = strlen(title) - strlen(strrchr(title, '.'));
		*strrchr(rom_path, '/') = '\0';
		path = strchr(rom_path, '/') + 1;
	}
	else
	{
		*strrchr(rom_path, '/') = '\0';
		strncpy(title, gameHeader.title, 63);
	}

	device = strncasecmp(rom_path, "sd", 2) == 0 ? "sd" : "usb";

	const char *loader = fmt("%sWiiFlowLoader.dol", Settings.PluginsPath);
	vector<string> arguments = CreateArgs(device, path, title, loader, title_len_no_ext, gameHeader.magic);

	const char *plugin_dol_name = GetDolName(gameHeader.magic);
	const char *plugin_file = plugin_dol_name;
	if(strchr(plugin_file, ':') == NULL || !CheckFile(plugin_file))
	{
		plugin_file = fmt("%s/%s", Settings.PluginsPath, plugin_dol_name);
		if(!CheckFile(plugin_file))
		{
			for(u8 i = SD; i < MAXDEVICES; ++i)
			{
				plugin_file = fmt("%s:/%s", DeviceName[i], plugin_dol_name);
				if(CheckFile(plugin_file)) break;
			}
			if(!CheckFile(plugin_file)) return -1;
		}
	}

	u8 *buffer = NULL;
	u32 filesize = 0;
	LoadFileToMem(plugin_file, &buffer, &filesize);
	if(!buffer) return -1;
	FreeHomebrewBuffer();
	CopyHomebrewMemory(buffer, 0, filesize);

	AddBootArgument(plugin_file);
	for(u32 i = 0; i < arguments.size(); ++i)
		AddBootArgument(arguments[i].c_str());
	return !(BootHomebrewFromMem() < 0);
}
