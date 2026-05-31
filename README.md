# FOrcaSlicer — Flexible OrcaSlicer

A fork of [Snapmaker OrcaSlicer](https://github.com/Snapmaker/OrcaSlicer) adding mixed nozzle size printing and color patch pipeline support for the Snapmaker U1 4-head printer.

## Download
👉 **[Latest Release](https://github.com/jiyang1018/FOrcaSlicer/releases/latest)**

- `FOrcaSlicer_Windows_Installer_V*.exe` — Windows installer
- `FOrcaSlicer_Windows_Portable_V*.zip` — Windows portable (no install needed)

## Development Roadmap
Track progress, design decisions, and implementation details:
👉 **[View the interactive roadmap](https://jiyang1018.github.io/FOrcaSlicer-roadmap/)**

---

## About

The Snapmaker U1 supports 4 independent print heads with different nozzle diameters (e.g. 0.2mm, 0.4mm, 0.6mm, 0.8mm). Stock Snapmaker OrcaSlicer syncs all nozzles to the same size. FOrcaSlicer extends the slicer with:

### Mixed Nozzle Size Support
- Independent nozzle diameter configuration per head
- Separate "Outer wall (OW)" and "Inner wall (IW)" from "Walls" in the Multimaterial tab
- OW, IW, infill, solid infill, and prime tower can each use different nozzle sizes independently
- Nozzle verification dialog before printing
- Print line widths in preview reflect actual nozzle diameter per extruder

### Color Patch Pipeline
- When a surface is painted with a color, only the outer N wall loops on that surface print in the painted color
- Top and bottom solid surfaces of painted faces print fully in the painted color
- Per-object, per-filament color patch loop count (CL)
- Unpainted volume sliced under original logic
- Color patch mode and original mode can be mixed per object on the same plate
- Original mode produces G-code identical to stock OrcaSlicer (verified by diff)
- Per-object CL settings save and load correctly with .3mf files
- CL slider, input box, and mode toggle correctly trigger re-slice when changed

---

## How to Install

### Windows
1. Download the installer or portable zip from the [releases page](https://github.com/jiyang1018/FOrcaSlicer/releases).
2. If you have trouble running the build, install these runtimes:
   - [MicrosoftEdgeWebView2RuntimeInstallerX64](https://go.microsoft.com/fwlink/p/?LinkId=2124703)
   - [vcredist2019_x64](https://aka.ms/vs/17/release/vc_redist.x64.exe)

### First Run
Log in with your Snapmaker account to access the printer library and set up your printer.

### Mac
> Not yet officially supported. Build from source using the instructions below.

### Linux
> Not yet officially supported. Build from source using the instructions below.

---

## How to Compile

### Windows 64-bit
Tools needed: Visual Studio 2019, CMake 3.14+, git, git-lfs, Strawberry Perl

```powershell
git clone https://github.com/jiyang1018/FOrcaSlicer
cd FOrcaSlicer
git lfs pull
cd build
cmake ..
.\build.ps1
```

Output: `build\FOrcaSlicer\FOrcaSlicer.exe`

### Building the Installer
Requires [NSIS](https://nsis.sourceforge.io/Download):
```powershell
makensis /DVERSION=2.3.2 installer.nsi
```

### Mac 64-bit
Tools needed: Xcode, CMake, git, gettext, libtool, automake, autoconf, texinfo
```bash
brew install cmake gettext libtool automake autoconf texinfo
./build_release_macos.sh
```

### Linux (Ubuntu)
```bash
sudo ./BuildLinux.sh -u
./BuildLinux.sh -dsir
```

---

## Klipper Note
If you're running Klipper, add this to your `printer.cfg`:
```
[exclude_object]
[gcode_arcs]
resolution: 0.1
```

---

## Name
**FOrcaSlicer** means two things:
1. **F**lexible **Orca**Slicer — extends OrcaSlicer with flexible per-head configuration
2. **Fork**-a-Slicer — a fork of the original OrcaSlicer

---

## 关于本项目 / About (中文)

[Snapmaker OrcaSlicer](https://github.com/Snapmaker/OrcaSlicer) 的分支版本，为 Snapmaker U1 四头打印机添加混合挤出口尺寸打印支持。

👉 **[查看交互式路线图](https://jiyang1018.github.io/FOrcaSlicer-roadmap/)**

### 混合挤出口尺寸支持
- 支持每个工作头独立配置挤出口直径
- 在多材料标签页中，将"外墙（OW）"和"内壁（IW）"从"总墙壁"中独立拆分
- 外墙、内壁、填充、实心填充及换料塔可分别指定不同尺寸挤出口
- 打印前通过挤出口验证对话框确认配置
- 预览中的打印线宽反映每个挤出机的实际挤出口直径

### 外壳着色模式（Color Patch）
- 上色时，仅该面最外层 N 圈壁会由涂色耗材打印
- 上色面的顶部和底部实体层也完全使用涂色耗材打印
- 支持逐对象、逐耗材着色区域路径数
- 未涂色区域按原始逻辑切片
- 外壳着色模式与原始模式可在同一实体上混合使用
- 含逐对象 CL 的项目设置可正确保存和读取 .3mf 文件

---

## Background
FOrcaSlicer is forked from Snapmaker OrcaSlicer.
Snapmaker OrcaSlicer is forked from OrcaSlicer by SoftFever.
OrcaSlicer is forked from Bambu Studio by BambuLab.
Bambu Studio is forked from [PrusaSlicer](https://github.com/prusa3d/PrusaSlicer) by Prusa Research.
PrusaSlicer is based on [Slic3r](https://github.com/Slic3r/Slic3r) by Alessandro Ranellucci and the RepRap community.
FOrcaSlicer incorporates features from SuperSlicer by @supermerill.

---

## License
FOrcaSlicer is licensed under the GNU Affero General Public License, version 3.

Snapmaker OrcaSlicer is licensed under the GNU Affero General Public License, version 3.

OrcaSlicer is licensed under the GNU Affero General Public License, version 3. OrcaSlicer is based on Bambu Studio by BambuLab.

Bambu Studio is licensed under the GNU Affero General Public License, version 3. Bambu Studio is based on PrusaSlicer by PrusaResearch.

PrusaSlicer is licensed under the GNU Affero General Public License, version 3. PrusaSlicer is owned by Prusa Research. PrusaSlicer is originally based on Slic3r by Alessandro Ranellucci.

Slic3r is licensed under the GNU Affero General Public License, version 3.

The GNU Affero General Public License, version 3 ensures that if you use any part of this software in any way (even behind a web server), your software must be released under the same license.

---

## Feedback & Contributions
File issues or feature requests at [GitHub Issues](https://github.com/jiyang1018/FOrcaSlicer/issues).
