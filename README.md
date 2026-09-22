<p align="center"><a href="https://github.com/wiidev/usbloadergx/" title="USB Loader GX"><img src="data/web/logo.png"></a></p>
<p align="center">
<a href="https://github.com/wiidev/usbloadergx/releases" title="Releases"><img src="https://img.shields.io/github/v/release/wiidev/usbloadergx?logo=github"></a>
<a href="https://github.com/wiidev/usbloadergx/actions" title="Actions"><img src="https://img.shields.io/github/actions/workflow/status/wiidev/usbloadergx/main.yml?branch=enhanced&logo=github"></a>
</p>
# USB Loader GX 插件完全使用指南 (Plugin Complete Guide)

[中文说明 (Chinese Guide)](#中文说明) | [English Guide](#english-guide)

---

<a name="中文说明"></a>
# USB Loader GX 插件系统完全使用指南 (中文)

本项目（`plugins_mod` 分支）为 USB Loader GX 引入了强大的插件（Plugin）拓展系统，兼容 WiiFlow 插件标准。通过该系统，USB Loader GX 不仅可以引导 Wii 和 GameCube 游戏，还能够直接浏览、管理并一键启动各类模拟器（RetroArch、Snes9x GX、FCE Ultra GX、Wii64 等）的复古游戏 ROM。

---

## 目录
1. [系统目录结构](#1-系统目录结构)
2. [插件配置文件 (.ini) 详解](#2-插件配置文件-ini-详解)
3. [占位符与启动参数说明](#3-占位符与启动参数说明)
4. [界面操作与手柄快捷键](#4-界面操作与手柄快捷键)
5. [游戏封面 (Covers) 设置](#5-游戏封面-covers-设置)
6. [自定义游戏中文标题 (custom_titles.ini)](#6-自定义游戏中文标题-custom_titlesini)
7. [缓存机制与刷新 (lists/*.db)](#7-缓存机制与刷新-listsdb)
8. [多插件合并与分组模式](#8-多插件合并与分组模式)
9. [常用模拟器插件配置模板](#9-常用模拟器插件配置模板)
10. [常见问题排查 (FAQ)](#10-常见问题排查-faq)

---

### 1. 系统目录结构

插件系统主要依赖 SD 卡或 USB 设备上的以下目录及文件：

```
SD卡或USB根目录/
├── apps/
│   ├── usbloader_gx/             # USB Loader GX 主程序
│   │   ├── boot.dol
│   │   └── GXGlobal.cfg          # 全局设置文件
│   └── retroarch-wii/            # 各类模拟器程序（示例）
│       └── mame2003_plus_libretro_wii.dol
├── plugins/                      # 插件定义目录 (默认在 Settings.PluginsPath)
│   ├── lists/                    # 自动生成的 ROM 游戏列表二进制缓存
│   │   ├── 6D616D40.db
│   │   └── 534E4553.db
│   ├── mameA.ini                 # 插件配置文件 (可存放多个 .ini)
│   ├── snes.ini
│   └── scummvm.ini               # ScummVM 专用配置 (可选)
└── usbloader_gx/
    ├── custom_titles.ini         # 自定义游戏别名/中文名映射表 (与 GXGlobal.cfg 同级)
    └── images/
        ├── 2D/                   # 2D 封面目录 (由封面路径设定)
        │   ├── MAME2003/         # 与 ini 中的 coverfolder 对应
        │   │   └── kof97.png
        │   └── SNES/
        │       └── Super Mario World.png
        └── 3D/                   # 3D 封面目录
```

> **提示**：可以在 USB Loader GX 的「设置 (Settings)」->「自定义路径 (Custom Paths)」中自定义 `Plugins Path`（插件路径）。默认为 `<ConfigPath>/plugins/`。

---

### 2. 插件配置文件 (.ini) 详解

每个模拟器或游戏分类对应一个 `.ini` 文件，存放在 `plugins/` 目录下。

#### 完整参数字段说明：

| 字段名 | 类型 | 说明与示例 |
| :--- | :--- | :--- |
| `[PLUGIN]` | 标签 | 必须保留的节头标识 |
| `displayname` | 文本 | 在界面中显示的插件名称，例如：`街机-MAME (RetroArch)` |
| `magic` | 8位16进制 | 插件唯一标识码（**不可重复**，不能使用保留码），如 `6d616d40`、`534e4553` |
| `dolfile` | 路径 | 引导执行的模拟器 DOL 文件相对路径，如 `apps/snes9xgx/boot.dol` |
| `romdir` | 路径 | 该插件 ROM 文件存放的目录，如 `ROMS/SNES` 或 `ROMS/MAME2003/coreA` |
| `filetypes` | 文本 | 支持的文件后缀名，多个后缀用竖线 `\|` 分隔，如 `.zip\|.7z` 或 `.smc\|.sfc` |
| `rompartition` | 整数 | ROM 所在的存储设备分区：`0` 为 SD 卡，`1` 为 USB1 分区（不填则跟随全局设置） |
| `coverfolder` | 文本 | 封面图片存放的子文件夹名，如 `MAME2003`、`SNES` |
| `arguments` | 文本 | 启动 DOL 时传递给模拟器的参数模板，支持占位符与管道符 `\|` 分隔参数 |
| `group` | 文本 | *(可选)* 插件分组名，如 `mame2003plus`，用于同系列多核心合并展示 |
| `bannersound` | 路径 | *(可选)* 选定游戏时播放的音效文件路径，如 `ZZ--Sounds/Arcade.ogg` |

> ⚠️ **保留的 Magic 词（禁止使用）**：
> `4E47434D` (NGC), `4E574949` (WII), `4E414E44` (NAND), `454E414E` (EMUNAND), `4D555343` (MUSC)。

---

### 3. 占位符与启动参数说明

启动模拟器时，USB Loader GX 会解析 `arguments` 字符串，并将以下占位符替换为实际值：

- `{device}`：存储设备名称，根据 ROM 所在设备自动替换为 `sd` 或 `usb`。
- `{path}`：ROM 文件所在目录的相对路径（不含盘符与起始斜杠）。
- `{name}`：ROM 完整文件名（**包含扩展名**），如 `kof97.zip`。
- `{name_no_ext}`：ROM 文件名（**不包含扩展名**），如 `kof97`。
- `{loader}`：启动器路径（自动解析为 `<PluginsPath>/WiiFlowLoader.dol`）。
- 参数分隔符：`|` 用于隔开不同的命令行参数。

#### 常见参数配置示例：
- **RetroArch 核心参数**：
  ```ini
  arguments={device}:/{path}/|{name}
  ```
- **部分独立模拟器（传入完整文件路径）**：
  ```ini
  arguments={device}:/{path}/{name}
  ```

---

### 4. 界面操作与手柄快捷键

#### 切换进入插件模式：
1. 在主菜单顶部点击**游戏源切换按钮**（默认显示 Wii 标志）。
2. 点击依次轮转：`Wii 游戏` ➔ `NAND` ➔ `EmuNAND` ➔ **`插件游戏 (Plugin Mode)`** ➔ `自定义选择` ➔ `Wii 游戏`。
3. 进入插件模式后，顶部栏图标将变为插件专用图标，下方正中央会显示当前启用的插件名称（如 `街机-MAME`）。

#### 快速切换不同插件：
- **手柄快捷键**（在插件模式下生效，带 500ms 防抖）：
  - **Wii 遥控器**：按 `+` 键切换到下一个插件，按 `-` 键切换到上一个插件。
  - **经典手柄 (Classic Controller)**：按 `+` / `-` 键切换。
  - **GameCube (NGC) 手柄**：按 `R` 键切换到下一个插件，按 `L` 键切换到上一个插件。
- **弹窗选择**：
  - 点击顶部「游戏源选择按钮」，会直接弹出 **PluginPrompt（插件选择菜单）**。
  - 在列表中浏览并选择任意已加载的插件，点击即可瞬间加载该插件的游戏列表。

---

### 5. 游戏封面 (Covers) 设置

插件模式下支持加载 2D 和 3D 游戏封面。

1. **封面存放路径规范**：
   - 2D 封面：`{Settings.covers2d_path}/{coverfolder}/{ROM完整文件名}.png`
   - 3D 封面：`{Settings.covers_path}/{coverfolder}/{ROM完整文件名}.png`
   - *示例*：若 `coverfolder=SNES`，ROM 文件名为 `Super Mario World.sfc`，则封面路径为：
     `usb:/usbloader_gx/images/2D/SNES/Super Mario World.sfc.png`
     或 `usb:/usbloader_gx/images/2D/SNES/Super Mario World.png`
2. **2D / 3D 切换**：
   - 在设置中配置 `Plugin3DCvrs = 1` 使用 3D 封面，`0` 为使用 2D 封面。

---

### 6. 自定义游戏中文标题 (custom_titles.ini)

由于许多街机或复古 ROM 为简写文件名（如 `kof97.zip`、`mario.sfc`），系统提供了 `custom_titles.ini` 文件来显示友好标题或中文名称。

1. **文件位置**：存放在 USB Loader GX 的配置文件目录（通常为 `sd:/apps/usbloader_gx/custom_titles.ini` 或与其同级）。
2. **格式规范**：
   - 使用小节头声明插件的 8 位 `magic` 标识码。
   - 键为 **不带扩展名的 ROM 文件名**，值为自定义名称。

```ini
[6d616d40]
kof97=拳皇 97 (风云再起)
mslug3=合金弹头 3
sfa3=街头霸王 ZERO 3

[534e4553]
Super Mario World=超级马里奥世界
Chrono Trigger=超时空之轮
```

3. **显示效果**：
   - 匹配成功后，界面将展示为：`拳皇 97 (风云再起) (kof97)`，既便于识别中文游戏名，又保留了原文件名以便核对。
4. **特殊支持**：
   - 对于特定街机插件（如 PGM，magic 为 `50475648`），系统还支持直接读取 ROM 所在文件夹下的 `title.txt` 第一行作为标题。

---

### 7. 缓存机制与刷新 (lists/*.db)

为解决外置存储设备（FAT32/NTFS）扫描数千个复古 ROM 速度缓慢的问题，本修改版引入了 **独立分片二进制缓存机制**：

1. **缓存存储**：位于 `<PluginsPath>/lists/<MAGIC>.db`。
2. **按需懒加载 (Lazy Load)**：
   - 启动时不会无脑扫描所有插件目录；只有在用户切换到该插件时，才读取其对应 `.db` 缓存。
   - 缓存读取耗时通常小于 10 毫秒，瞬间呈现数百上千款游戏。
3. **ROM 文件变动时如何刷新**：
   - 如果添加或删除了 SD/USB 中的 ROM 文件，在主界面执行「刷新 (Refresh / Rescan)」操作。
   - 系统会自动销毁过期缓存并重新扫描目录生成最新的 `.db` 缓存文件。

---

### 8. 多插件合并与分组模式

本分支支持多插件列表合并展示功能：

1. **全局合并 (`enabledPlugin = ALL`)**：
   - 将所有已配置的插件游戏合并到同一个游戏列表中展现。
   - 启动各游戏时，系统会自动根据每条游戏的独立 `magic` 寻址并调用对应的模拟器 DOL。
2. **分组归类合并 (`group = xxx`)**：
   - 在 `.ini` 中添加 `group=mame2003plus` 等分组标识。
   - 配合设置 `pluginShowMerged = 1` 及 `pluginMergeGroup = mame2003plus`，可只将 MAME 核心的分拆插件（coreA、coreB 等）合并为一个列表，而与其他单机模拟器区分开。

---

### 9. 常用模拟器插件配置模板

#### 模板 1：RetroArch 街机 MAME2003 Plus
```ini
[PLUGIN]
displayname=街机-MAME2003 Plus
magic=6d616d40
dolfile=apps/retroarch-wii/mame2003_plus_libretro_wii.dol
romdir=ROMS/MAME2003
filetypes=.zip
rompartition=1
coverfolder=MAME2003
arguments={device}:/{path}/|{name}
group=mame2003plus
```

#### 模板 2：Snes9x GX (SFC/SNES 独立模拟器)
```ini
[PLUGIN]
displayname=SFC-Snes9x GX
magic=534e4553
dolfile=apps/snes9xgx/boot.dol
romdir=snes9xgx/roms
filetypes=.smc|.sfc|.zip
rompartition=0
coverfolder=SNES
arguments={device}:/{path}/{name}
```

#### 模板 3：FCE Ultra GX (FC/NES 模拟器)
```ini
[PLUGIN]
displayname=FC-FCE Ultra GX
magic=46434555
dolfile=apps/fceugx/boot.dol
romdir=fceugx/roms
filetypes=.nes|.zip
rompartition=0
coverfolder=NES
arguments={device}:/{path}/{name}
```

---

### 10. 常见问题排查 (FAQ)

1. **Q：点击游戏启动后黑屏或退回主菜单？**
   - **检查 DOL 路径**：确保 `dolfile` 指定的文件在 SD 卡或 USB 设备中真实存在（如 `apps/snes9xgx/boot.dol`）。
   - **检查 arguments 传参**：部分模拟器支持 `{device}:/{path}/|{name}`，部分模拟器需要不带管道符的 `{device}:/{path}/{name}`。
   - **检查分区设置**：确认 `rompartition` 是否正确（`0` 代表 SD 卡，`1` 代表 USB 第 1 分区）。

2. **Q：放入了新游戏，但列表中没有出现？**
   - 插件系统开启了 `.db` 缓存。请在界面上点击重新扫描或清空 `plugins/lists/` 目录下的 `.db` 缓存文件。

3. **Q：封面不显示？**
   - 确认 `coverfolder` 与图片存放的文件夹名完全一致（注意大小写区分）。
   - 确认图片格式为标准 `.png` 格式。

---
---

<a name="english-guide"></a>
# USB Loader GX Plugin System Complete User Guide (English)

The `plugins_mod` branch introduces a high-performance plugin system for USB Loader GX, built upon the WiiFlow plugin specification. This enables USB Loader GX to browse, manage, and launch retro console and arcade ROMs directly via Homebrew emulators (RetroArch cores, Snes9x GX, FCE Ultra GX, Wii64, etc.) alongside native Wii and GameCube titles.

---

## Table of Contents
1. [Directory Structure](#1-directory-structure)
2. [Plugin Configuration (.ini) Syntax](#2-plugin-configuration-ini-syntax)
3. [Arguments & Placeholders](#3-arguments--placeholders)
4. [User Interface & Controller Shortcuts](#4-user-interface--controller-shortcuts)
5. [Cover Art Setup](#5-cover-art-setup)
6. [Custom Game Titles (custom_titles.ini)](#6-custom-game-titles-custom_titlesini)
7. [Caching Mechanism (lists/*.db)](#7-caching-mechanism-listsdb)
8. [Multi-Plugin Merged Mode & Groups](#8-multi-plugin-merged-mode--groups)
9. [Sample Configurations](#9-sample-configurations)
10. [Troubleshooting & FAQ](#10-troubleshooting--faq)

---

### 1. Directory Structure

The plugin framework uses the following directory layout on your SD card or USB device:

```
Device Root (SD or USB)/
├── apps/
│   ├── usbloader_gx/             # USB Loader GX application folder
│   │   ├── boot.dol
│   │   └── GXGlobal.cfg          # Main configuration file
│   └── retroarch-wii/            # Emulator executables (example)
│       └── mame2003_plus_libretro_wii.dol
├── plugins/                      # Plugin definitions (Settings.PluginsPath)
│   ├── lists/                    # Binary ROM list cache files (auto-generated)
│   │   ├── 6D616D40.db
│   │   └── 534E4553.db
│   ├── mameA.ini                 # Plugin definition files (.ini)
│   ├── snes.ini
│   └── scummvm.ini               # ScummVM configuration (optional)
└── usbloader_gx/
    ├── custom_titles.ini         # Friendly title translation / alias map
    └── images/
        ├── 2D/                   # 2D covers path
        │   ├── MAME2003/         # Subfolder matching `coverfolder`
        │   │   └── kof97.png
        │   └── SNES/
        └── 3D/                   # 3D covers path
```

> **Note**: You can change the `Plugins Path` in USB Loader GX under **Settings** -> **Custom Paths** (defaults to `<ConfigPath>/plugins/`).

---

### 2. Plugin Configuration (.ini) Syntax

Each emulator or platform requires a dedicated `.ini` file placed inside the `plugins/` directory.

#### Configuration Directives:

| Key | Type | Description |
| :--- | :--- | :--- |
| `[PLUGIN]` | Header | Mandatory section header |
| `displayname` | String | Name displayed in GUI, e.g. `Arcade - MAME2003 Plus` |
| `magic` | 8-hex chars | Unique 8-digit hexadecimal identifier (e.g. `6d616d40`, `534e4553`) |
| `dolfile` | Path | Relative path to the emulator DOL binary (e.g. `apps/snes9xgx/boot.dol`) |
| `romdir` | Path | Relative path to the folder containing ROMs (e.g. `ROMS/SNES`) |
| `filetypes` | String | Supported file extensions separated by `\|` (e.g. `.zip\|.7z` or `.smc\|.sfc`) |
| `rompartition` | Integer | Storage partition: `0` for SD card, `1` for USB1 (inherits global setting if omitted) |
| `coverfolder` | String | Subfolder name inside the covers directory (e.g. `MAME2003`, `SNES`) |
| `arguments` | String | Execution argument template with placeholders and pipe `\|` separators |
| `group` | String | *(Optional)* Group identifier (e.g. `mame2003plus`) for grouped merging |
| `bannersound` | Path | *(Optional)* Sound played when browsing games (e.g. `ZZ--Sounds/Arcade.ogg`) |

> ⚠️ **Reserved Magic Words (Do Not Use)**:
> `4E47434D` (NGC), `4E574949` (WII), `4E414E44` (NAND), `454E414E` (EMUNAND), `4D555343` (MUSC).

---

### 3. Arguments & Placeholders

When launching an emulator, USB Loader GX replaces placeholders in `arguments` with runtime parameters:

- `{device}`: Target storage device (`sd` or `usb`).
- `{path}`: Directory of the selected ROM file relative to device root.
- `{name}`: Complete ROM filename with extension (e.g. `kof97.zip`).
- `{name_no_ext}`: ROM filename without extension (e.g. `kof97`).
- `{loader}`: Resolves to `<PluginsPath>/WiiFlowLoader.dol`.
- Pipe symbol `|`: Separates distinct command-line parameters passed to `BootHomebrew`.

#### Argument Examples:
- **RetroArch Wii Cores**:
  ```ini
  arguments={device}:/{path}/|{name}
  ```
- **Standard Standalone Emulators**:
  ```ini
  arguments={device}:/{path}/{name}
  ```

---

### 4. User Interface & Controller Shortcuts

#### Entering Plugin Mode:
1. Click the **Game Source** toggle button on the top menu bar.
2. Cycles through: `Wii` ➔ `NAND` ➔ `EmuNAND` ➔ **`Plugin Mode`** ➔ `Custom` ➔ `Wii`.
3. In Plugin Mode, the top source icon updates, and the active plugin name appears centered below the game counter.

#### Switching Plugins:
- **Controller Shortcuts** (with 500ms debounce protection):
  - **Wii Remote**: Press `+` for next plugin, `-` for previous plugin.
  - **Classic Controller**: Press `+` / `-`.
  - **GameCube Controller**: Press `R` trigger for next plugin, `L` trigger for previous plugin.
- **Plugin Selection Dialog**:
  - Clicking the top **Game Source** icon while in Plugin Mode pops up the **PluginPrompt** selection dialog.
  - Choose any detected plugin to switch instantly.

---

### 5. Cover Art Setup

Plugin mode supports both 2D and 3D cover art:

1. **File Locations**:
   - 2D: `{covers2d_path}/{coverfolder}/{ROM_FILENAME}.png`
   - 3D: `{covers_path}/{coverfolder}/{ROM_FILENAME}.png`
   - *Example*: For `coverfolder=SNES` and ROM `Super Mario World.sfc`:
     `usb:/usbloader_gx/images/2D/SNES/Super Mario World.sfc.png` (or `.png` without extra extension)
2. **Toggle 2D / 3D**:
   - Change `Plugin3DCvrs` in settings (`1` for 3D, `0` for 2D flat covers).

---

### 6. Custom Game Titles (custom_titles.ini)

To display localized, friendly titles for cryptic arcade and retro ROM filenames, use `custom_titles.ini`.

1. **File Location**: Put `custom_titles.ini` in the USB Loader GX configuration directory (same folder as `GXGlobal.cfg`).
2. **Format**:
   - Section headers define the 8-character plugin `magic`.
   - Keys are ROM filenames without extension; values are custom titles.

```ini
[6d616d40]
kof97=The King of Fighters '97
mslug3=Metal Slug 3

[534e4553]
Super Mario World=Super Mario World (SNES)
```

3. **Display Output**:
   - When matched, the game is displayed as: `Custom Title (OriginalName)`.
4. **Folder-based Titles (e.g. PGM)**:
   - For arcade games using magic `50475648`, the system also checks for a `title.txt` file inside the game's directory.

---

### 7. Caching Mechanism (lists/*.db)

Scanning thousands of ROMs on FAT32/NTFS drives at every startup is slow. This modification uses an **independent per-plugin binary cache**:

1. **Location**: `<PluginsPath>/lists/<MAGIC>.db`.
2. **Lazy Loading**:
   - Only the currently active plugin's database is loaded into memory on demand.
   - Cache retrieval takes less than 10 milliseconds.
3. **Rescanning / Rebuilding Cache**:
   - When adding or removing ROM files, trigger a rescan from the main menu or delete the `<MAGIC>.db` file inside `plugins/lists/`.

---

### 8. Multi-Plugin Merged Mode & Groups

1. **Merge All (`enabledPlugin = ALL`)**:
   - Displays all games across all configured plugins in a single unified list.
   - ROM launching automatically routes to the appropriate emulator DOL based on the entry's `magic`.
2. **Group Merging (`group = xxx`)**:
   - Setting `group=mame2003plus` across multiple `.ini` files allows merging specific cores (e.g. split arcade sets) while keeping other emulators separate.

---

### 9. Sample Configurations

#### Example 1: RetroArch MAME2003 Plus
```ini
[PLUGIN]
displayname=Arcade - MAME2003 Plus
magic=6d616d40
dolfile=apps/retroarch-wii/mame2003_plus_libretro_wii.dol
romdir=ROMS/MAME2003
filetypes=.zip
rompartition=1
coverfolder=MAME2003
arguments={device}:/{path}/|{name}
group=mame2003plus
```

#### Example 2: Snes9x GX
```ini
[PLUGIN]
displayname=SNES - Snes9x GX
magic=534e4553
dolfile=apps/snes9xgx/boot.dol
romdir=snes9xgx/roms
filetypes=.smc|.sfc|.zip
rompartition=0
coverfolder=SNES
arguments={device}:/{path}/{name}
```

---

### 10. Troubleshooting & FAQ

1. **Q: Black screen or crash back to Wii Menu when launching a game?**
   - Verify that the `dolfile` path exists on your SD card or USB drive.
   - Verify `arguments`: some emulators require a pipe separator (`{device}:/{path}/|{name}`), while others require a combined path (`{device}:/{path}/{name}`).
   - Verify that `rompartition` is set correctly (`0` for SD, `1` for USB Partition 1).

2. **Q: Newly copied ROMs do not show up in the list?**
   - USB Loader GX caches the ROM list in `plugins/lists/<MAGIC>.db`. Trigger a rescan or delete the `.db` file to refresh.

3. **Q: Covers are not showing up?**
   - Ensure the subfolder under your covers directory matches `coverfolder` exactly (case-sensitive on some file systems).
   - Ensure cover art is in standard `.png` format.

## Description
USB Loader GX allows you to play Wii and GameCube games from a USB storage device or an SD card, launch other homebrew apps, create backups, use cheats in games, and a whole lot more.

## Installation
1. Extract the apps folder to the root of your SD card and replace any existing files.
2. Install the [latest d2x cIOS](https://github.com/wiidev/d2x-cios/releases).
3. Optional: Update wiitdb.xml by selecting the update option within the loaders settings menu.
4. Optional: Install the loaders [forwarder channel](https://raw.githubusercontent.com/wiidev/usbloadergx/updates/USBLoaderGX_forwarder%5BUNEO%5D.wad), then go into `Loader Settings` and set `Return To` to `UNEO`.

## d2x cIOS
1. Use the correct cIOS package for your console — e.g., `d2x-v11-beta3` for the Wii and `d2x-v11-beta3-vWii` for the Wii U.
2. When using the d2x cIOS installer, set the cIOS to the version that you downloaded — e.g., `d2x-v11-beta3`.
3. Install the cIOS into each slot with the following settings.

````
Slot 248 base 38
Slot 249 base 56
Slot 250 base 57
Slot 251 base 58
````
