[English](README.md) | **简体中文**

# FOrcaSlicer — Flexible OrcaSlicer

[Snapmaker OrcaSlicer](https://github.com/Snapmaker/OrcaSlicer) 的分支版本，专为 Snapmaker U1 打造，核心理念只有一个：U1 有四个独立的工作头，不必让它们做同样的事。给每个工作头分配各自的挤出口直径、线宽和速度——用较细挤出口打印可见的外墙，用较粗的挤出口打印其它——把精度用在看得见的地方，把速度用在看不见的地方。它还加入了仅外壳着色（Color Patch）流程：把涂色面打印成**外壳包裹着不同材料内核**——比如在硬质骨架外的包括软质握把、或者通过控制半透明着色外壳厚度来控制透色的程度。

> **当前状态：** 开发预览版，仍有其它功能在积极开发中。面向 **Snapmaker U1** 四头打印机。**Windows** 与 **macOS** 提供打包版本；Linux 仅支持自行编译。本版本尚未经过独立的实机打印测试——欢迎反馈实际打印结果。

## 发布演示

一段简短的演示，介绍 FOrcaSlicer 相较于原版 Snapmaker OrcaSlicer 的三大不同之处：**混直径打印**、**将外墙与内壁拆分到不同工作头**，以及**仅外壳着色（Color Patch）模式**。

[![FOrcaSlicer 发布演示](docs/images/launch-demo-zh.jpg)](https://youtu.be/5z-unU9aujo)

### 混直径打印：外部精细，内部更快

一组共七个公制模数 1 齿轮——尺寸与齿形各异，模拟变速箱或动力传动项目会用到的组合，同一盘上摆放了 3 套。截图展示的是整盘以不同方式切片的结果：原版（stock）在整个零件上强制使用同一挤出口直径（左1、4两列），而 FOrcaSlicer 用小挤出口打印外墙、用更大的挤出口打印其它（左2、3两列）——在大约一半的时间内保留了全 0.2 mm 打印的齿形细节。

| 每圈 0.2 mm（原版） | 外墙 0.2 mm，其余 0.4 mm | 外墙 0.2 mm，内壁 0.4 mm，其余 0.6 mm | 每圈 0.4 mm（原版） |
|---|---|---|---|
| ![齿轮，全 0.2 mm](docs/images/gear-0.2-stock.jpg) | ![齿轮，FOS 2-4-4](docs/images/gear-fos-2-4-4.jpg) | ![齿轮，FOS 2-4-6](docs/images/gear-fos-2-4-6.jpg) | ![齿轮，全 0.4 mm](docs/images/gear-0.4-stock.jpg) |
| 精细细节，打印慢 — **8h 39m** | 精细细节，打印更快 — **5h 15m** | 精细细节，打印更快 — **5h 5m** | 细节较少，打印最快 — **4h 29m** |

## 下载

👉 **[最新版本](https://github.com/jiyang1018/FOrcaSlicer/releases/latest)**

- `FOrcaSlicer_Windows_Installer_V*.exe` — Windows 安装程序
- `FOrcaSlicer_Windows_Portable_V*.zip` — Windows 便携版（免安装）

## 开发路线图

追踪开发进度、设计决策与实现细节：
👉 **[查看交互式路线图](https://jiyang1018.github.io/FOrcaSlicer-roadmap/)**

---

## 新增功能

Snapmaker U1 有 4 个独立工作头，可搭载不同的挤出口直径（例如 0.2 / 0.4 / 0.6 / 0.8 mm）。原版 Snapmaker OrcaSlicer 会把所有工作头同步为相同直径。FOrcaSlicer 解除了这一限制。

### 混直径打印

- 每个工作头可独立设置挤出口直径。
- 在多材料标签页中，**外墙（OW）** 与 **内壁（IW）** 从"总墙壁"中拆分出来，因而可分配到不同的工作头。参见 [OW / IW 拆分指南（wiki）](https://github.com/jiyang1018/FOrcaSlicer/wiki/OW-IW-Split)。
- 外墙、内壁、稀疏填充、实心填充、支撑和换料塔（prime tower）都可各自分配到不同的挤出口。
- **每挤出口独立的线宽与速度** —— 每个特征的线宽和速度都取自实际打印它的那个挤出口的打印流程预设（PRP），而不再全部继承挤出口 1。
- **每挤出口独立的换料塔线宽** —— 每个工作头以各自的线宽擦拭和退料。
- **单一层高，受最小在用挤出口封顶** —— 层高和首层高度是全局单一值，会自动限制在当前使用的最小口径挤出口能打印的范围内，因此不会让 0.2 mm 的挤出口去打印 0.6 mm 的层。
- 打印前的挤出口验证对话框。
- 预览中的线宽反映每个挤出机的实际出口直径。
- **在单一挤出口直径的机器上，输出与原版完全一致** —— 当所有挤出口直径相同时，所有改动都不产生任何变化。

![逐挤出口的质量与速度标签页](docs/images/per_nozzle_settings_zh_CN.jpg)

*打开 **混直径（Mixed）**，为每个挤出口指定各自的打印流程预设，然后在质量和速度标签页中逐挤出口设置线宽与速度。层高保持全局，位于挤出口标签页下方。*

### 多材料供料系统（multiACE）

耗材数多于打印头时：可以告诉 FOrcaSlicer 有一套多材料供料系统在为打印机供料，并按此切片。首先支持 **multiACE** —— decay71 为 U1 开发的开源多材料单元，参见 **[decay71/multiACE](https://github.com/decay71/multiACE)**。

- **在 打印机设置 > 机器 > 附件 中声明** —— 供料系统、打印机局域网 IP、模式（`normal` / `multi` / `head`，沿用 multiACE 自身的术语），以及供料单元数或配备单元的工具头。模式决定每个挤出口能取到多少种耗材，这是切片的输入，因此需在切片前设定，而不是发送时。
- **从打印机读取** 会向机器询问当前接线与已装配的挤出口，完整展示后，只有在你确认时才填入字段。它绝不会覆盖挤出口直径。
- **发送到 multiACE** 出现在打印下拉菜单中，可将切片文件直接上传到机器。
- **硬件无法装载的组合会被拒绝切片** —— 若某一挤出口直径的耗材数超过已声明接线一次能容纳的数量，打印会被拦下并给出具体数量，而不是到打印机上才失败。
- 导出的 G-code 在文件头中携带机器的物理挤出口直径，供料系统据此可将文件的需求与实际装配情况进行比对。

### 仅外壳着色（Color Patch）流程

当你为一个面涂色时，只有它的外壳用涂色耗材打印，内部仍为基础材料。重点不在颜色——而在于**外壳与内核可以是不同的材料**，靠几何结构结合在一起。这解锁了原版涂色模式做不到的两件事。

**混合材料与质感。** 在原版（Original）下，每种涂色材料都是彼此**相邻**的实心区域——如果在硬质 PETG 主体旁涂上柔软的 TPU 握把，这个打印多半会失败，因为 TPU 不会与 PETG 粘合。仅外壳着色（Color Patch）则让涂色材料**包裹**内核：TPU 握把以机械结构的方式固定在 PETG 核心上的从而实现柔软表层，夜光的蓝色半透明刃口也可以透出一些硬质刃口的材质。使用原始模式（Original）的区域仍是实心体——因此红色装饰可以用来变现宝石镶嵌，而非表面涂装——而填充图案也可以透过半透明区域显现，成为设计的一部分。这一切都不需要不同的挤出口直径。

| 涂色面，原始模式（Original） | 涂色面，仅外壳着色模式（Color Patch） |
|---|---|
| ![斧头，原始模式](docs/images/axe_sliced_original.jpg) | ![斧头，仅外壳着色模式](docs/images/axe_sliced_color_patch.jpg) |

*模型：[Star Orbit Judgment Axe](https://makerworld.com/en/models/2332850-star-orbit-judgment-axe)，作者 Ted_k，来自 MakerWorld。*

**透视外壳。** 在原始模式下把裤袜涂成黑色，整条腿会从里到外打印成实心黑色——那是一条黑腿，而不是衣物。仅外壳着色把涂色区域打印成**半透明外壳**（可选用 0.2 mm 挤出口以获得更薄的外壳），罩在肤色内核之外，让底色像真实织物那样透出来。这一效果依赖**半透明耗材**——要达到最佳观感，需要对耗材的颜色与透明度做一些尝试。

| 已涂色的模型 | 原始模式：整条腿从里到外实心黑色 | 仅外壳着色：肤色内核外的半透明外壳 |
|---|---|---|
| ![已涂色的角色](docs/images/character_painted.jpg) | ![角色，原始模式](docs/images/character_painted_original.jpg) | ![角色，仅外壳着色](docs/images/character_painted_color_patch.jpg) |

*模型：[Anime girl figure](https://makerworld.com/en/models/2598006-anime-girl-figure)，作者 "I know U"，来自 MakerWorld。*

**CL —— 着色区域路径数（Color Loops）** 是唯一需要调整的参数：涂色从表面向内占据多少圈壁（CL = 1 是最薄的表层；数值越大越厚、越不透明）。可逐对象、逐涂色分别设置。由于圈宽跟随**涂色挤出口自身的流量**，CL 是圈数而非固定厚度——0.8 mm 挤出口上的 CL = 1，是 0.2 mm 挤出口上 CL = 1 的四倍厚。

![15 mm 圆柱上的 CL = 4 到 CL = 1](docs/images/4x_to_1x_CL.jpg)

*15 mm 圆柱上的 CL 4 → 1：半透明灰色 PLA 作为着色区域（0.2 mm 挤出口），罩在肤色 PLA 基材（0.4 mm 挤出口）之外。CL 越低越薄、越透。*

原始模式的几何**与原版 OrcaSlicer 逐字节一致**（经 diff 验证），设置随 `.3mf` 保存/读取。

> 完整教程、材料搭配与 CL 调校：**[仅外壳着色指南（wiki）](https://github.com/jiyang1018/FOrcaSlicer/wiki/Color-Patch)**

---

## 安装（Windows）

1. 从[发布页面](https://github.com/jiyang1018/FOrcaSlicer/releases)下载安装程序或便携版 zip。
2. 若无法启动，请安装以下运行库：
   - [Microsoft Edge WebView2 运行时](https://go.microsoft.com/fwlink/p/?LinkId=2124703)
   - [Visual C++ 2019 可再发行组件 (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe)
3. 详细步骤请参见 [下载与安装指南（wiki）](https://github.com/jiyang1018/FOrcaSlicer/wiki/Download-and-Install)。

**首次运行：** 参见 [首次启动指南（wiki）](https://github.com/jiyang1018/FOrcaSlicer/wiki/First-Launch)，用你的 Snapmaker 账号登录并设置打印机。

**首次打印前：** 请阅读 [打印前须知（wiki）](https://github.com/jiyang1018/FOrcaSlicer/wiki/Before-You-Print)——如何选择挤出口类型与直径、更换热端，以及为什么不要同步挤出口信息。FOrcaSlicer 在一些关键之处的行为与原版不同，值得花几分钟了解。

**macOS：** 每个发布版本均附带通用版本（Intel + Apple 芯片，macOS 12 及以上）。
该版本**未签名、未公证**，因此 macOS 首次启动时会提示"Apple 无法检查其是否包含恶意软件"
并拒绝打开。将 FOrcaSlicer 拖入"应用程序"后，执行一次以下命令清除隔离标记：

```bash
xattr -dr com.apple.quarantine "/Applications/FOrcaSlicer.app"
```

**Linux：** 尚未打包——请从源码编译（见下）。

---

## 从源码编译

### Windows（64 位）

需要：**Visual Studio 2022**、CMake 3.14+、Git、Git-LFS、Strawberry Perl。

```powershell
git clone https://github.com/jiyang1018/FOrcaSlicer
cd FOrcaSlicer
git lfs pull

# build dependencies once, then the slicer:
build_release_vs2022.bat deps
build_release_vs2022.bat slicer
# (or just `build_release_vs2022.bat` to do both)
```

输出：`build\src\Release\FOrcaSlicer.exe`

**编译 Windows 安装程序**（需要 [NSIS](https://nsis.sourceforge.io/Download)）：

```powershell
.\build_installer.ps1 -Version 2.3.2-fos.8.5
```

### macOS（64 位）

需要 Git，以及完整版 Xcode 或仅安装命令行工具（`xcode-select --install`）。然后：

```bash
brew install gettext libtool automake autoconf texinfo ninja
```

**请勿执行 `brew install cmake`。** Homebrew 现提供 CMake 4.x，它已移除对
`cmake_minimum_required(<3.5)` 的支持；依赖树中声明了更低的最低版本，配置会立即失败。
请改用 3.31.x：

```bash
curl -fsSL -o /tmp/cmake.tar.gz https://cmake.org/files/v3.31/cmake-3.31.6-macos-universal.tar.gz
tar -xzf /tmp/cmake.tar.gz -C /tmp
export PATH="/tmp/cmake-3.31.6-macos-universal/CMake.app/Contents/bin:$PATH"
cmake --version
```

继续之前，`cmake --version` 必须显示 3.31.6。

Homebrew 的 gettext 属于 keg-only，而切片器编译的最后一步会执行 `scripts/run_gettext.sh`
（在 `set -e` 下运行）。若缺少 `msgfmt`，已经完成的编译会在最后一步前功尽弃，因此请先将其
加入 PATH：

```bash
which msgfmt || export PATH="$(brew --prefix gettext)/bin:$PATH"
```

**务必加上 `-x`。** 该选项选择 Ninja Multi-Config 生成器。若不加，脚本默认使用 Xcode
生成器，它需要调用 `xcodebuild`，因而必须安装完整版 `Xcode.app`；仅安装命令行工具时，
配置阶段会以 `No CMAKE_C_COMPILER could be found` 失败，即使工具链本身完全正常。Ninja
在两种情况下都可用。重新编译时可加 `-b` 跳过重新配置；修改任何 `CMakeLists.txt` 后请
去掉 `-b`。

部署目标默认为 12.0。每种架构都有各自独立的依赖树，因此对某一架构执行 `-d` 对另一架构
没有帮助。

以下三种方式均已在 Apple 芯片（Mac mini M4，仅命令行工具）上完整验证。请按需求选择其中
一种展开。

| 你的需求 | 展开 |
|---|---|
| 在自己的 Apple 芯片 Mac 上运行 | 仅 Apple 芯片 |
| 测试或分发 Intel 版本 | 仅 Intel |
| 一个 DMG 覆盖所有 Mac（发布用） | 通用版 |

#### 编译耗时参考

以下为冷编译实测（已删除 `build/`、`deps/build/` 与下载缓存）：

| 机器 | 单架构 | 通用版 |
|---|---|---|
| Mac mini M4（10 核） | 约 17-20 分钟 | 约 38 分钟 |
| MacBook Pro 2019，i9-9880H（8 核） | 约 46 分钟（x86_64） | 未测试 |
| GitHub Actions `macos-14` | - | 约 1 小时 50 分至 2 小时 15 分 |

其中绝大部分时间用于编译依赖树，且只需编译一次。之后修改代码重新编译通常只需几分钟——
加上 `-b` 可跳过重新配置。

如需查看各阶段耗时，可在任意编译命令后加上 `--time`：

```bash
./build_release_macos.sh -s -a arm64 -x --time
```

不加时输出与以往完全一致。`-T` 是它的短选项，请注意是大写；小写的 `-t` 用于设置部署目标
并会吞掉后面一个参数，因此脚本会在该处拒绝非版本号的取值，而不是稍后抛出令人费解的
CMake 错误。

<details>
<summary><b>仅 Apple 芯片（arm64）</b> —— 在 M 系列 Mac 上原生运行，无法在 Intel 上运行</summary>

仅当你只需要 Apple 芯片版本时使用。这是最短的路径：一棵依赖树，一次切片器编译。

```bash
./build_release_macos.sh -d -a arm64
./build_release_macos.sh -s -a arm64 -x
```

输出：`build/arm64/FOrcaSlicer/FOrcaSlicer.app`

打包为 DMG：

```bash
mkdir -p dist
FOS_VER=$(sed -n 's/.*FOS_VERSION "\(.*\)".*/\1/p' src/common_func/common_func.hpp)
STAGE="$(mktemp -d)/FOrcaSlicer"
mkdir -p "$STAGE"
cp -R build/arm64/FOrcaSlicer/FOrcaSlicer.app "$STAGE/"
ln -s /Applications "$STAGE/Applications"
xattr -cr "$STAGE/FOrcaSlicer.app" || true
hdiutil create -volname "FOrcaSlicer $FOS_VER" -srcfolder "$STAGE" -ov -format UDZO -imagekey zlib-level=9 dist/FOrcaSlicer-$FOS_VER-macos-arm64.dmg
```

</details>

<details>
<summary><b>仅 Intel（x86_64）</b> —— 可在 Intel Mac 上运行，也可在 Apple 芯片上通过 Rosetta 运行</summary>

仅当你需要 Intel 版本时使用。它可以在 Apple 芯片 Mac 上交叉编译，因此无需 Intel 硬件即可
产出，但需要一棵独立的依赖树，与已有的 arm64 依赖树互不相通。

若要在 Apple 芯片上*运行*编译结果，需要安装 Rosetta
（`softwareupdate --install-rosetta`），并且 macOS 会提示 Intel 应用支持即将结束。

```bash
./build_release_macos.sh -d -a x86_64
./build_release_macos.sh -s -a x86_64 -x
```

输出：`build/x86_64/FOrcaSlicer/FOrcaSlicer.app`

打包为 DMG：

```bash
mkdir -p dist
FOS_VER=$(sed -n 's/.*FOS_VERSION "\(.*\)".*/\1/p' src/common_func/common_func.hpp)
STAGE="$(mktemp -d)/FOrcaSlicer"
mkdir -p "$STAGE"
cp -R build/x86_64/FOrcaSlicer/FOrcaSlicer.app "$STAGE/"
ln -s /Applications "$STAGE/Applications"
xattr -cr "$STAGE/FOrcaSlicer.app" || true
hdiutil create -volname "FOrcaSlicer $FOS_VER" -srcfolder "$STAGE" -ov -format UDZO -imagekey zlib-level=9 dist/FOrcaSlicer-$FOS_VER-macos-x86_64.dmg
```

</details>

<details>
<summary><b>通用版（两种架构）</b> —— 一个 DMG 覆盖所有 Mac，即发布版本的形态</summary>

仅当你需要一个到处都能运行的版本时使用。它要求**先完成**上面两个部分，因为这一步只是把两个
已编译好的 app 用 lipo 合并起来——本身不编译任何代码，只需几分钟。

请为前置条件预留时间：两棵完整的依赖树，从零开始需要数小时。

```bash
./build_release_macos.sh -s -a universal -x -b
```

输出：`build/universal/FOrcaSlicer/FOrcaSlicer.app`

打包为 DMG：

```bash
mkdir -p dist
FOS_VER=$(sed -n 's/.*FOS_VERSION "\(.*\)".*/\1/p' src/common_func/common_func.hpp)
STAGE="$(mktemp -d)/FOrcaSlicer"
mkdir -p "$STAGE"
cp -R build/universal/FOrcaSlicer/FOrcaSlicer.app "$STAGE/"
ln -s /Applications "$STAGE/Applications"
xattr -cr "$STAGE/FOrcaSlicer.app" || true
hdiutil create -volname "FOrcaSlicer $FOS_VER" -srcfolder "$STAGE" -ov -format UDZO -imagekey zlib-level=9 dist/FOrcaSlicer-$FOS_VER-macos-universal.dmg
```

</details>

#### 关于 DMG

`ln -s /Applications` 会生成常见的拖拽安装窗口；`xattr -cr` 在打包前清除隔离属性，这样
测试者通过命令行传输后无需再执行 `xattr -dr`。**切勿**用普通的 `zip` 打包 `.app`——它会
破坏 bundle 中的符号链接与资源分支，导致 app 在其它机器上无法启动。请使用上面的
`hdiutil`，或用 `ditto -c -k --sequesterRsrc --keepParent` 生成 zip。

分发前请确认编译结果：

```bash
lipo -archs build/universal/FOrcaSlicer/FOrcaSlicer.app/Contents/MacOS/FOrcaSlicer
```

预期输出为 `arm64`、`x86_64`，通用版则是 `x86_64 arm64`。

调试符号会输出到 `build/<arch>/FOrcaSlicer/dSYM/`。它们刻意不包含在 DMG 中，但每一份都通过
UUID 与某一次编译一一对应——分发任何版本时请一并归档对应的 dSYM，否则该版本的崩溃报告将
无法符号化。

**Build macOS release** GitHub Actions 工作流同样会产出通用版 DMG。

### Linux（Ubuntu）

```bash
./build_linux.sh -u      # first time: install system dependencies
./build_linux.sh -dsi    # build dependencies, slicer, and AppImage
```

---

## Klipper 说明

若你使用 Klipper，请在 `printer.cfg` 中加入：

```
[exclude_object]
[gcode_arcs]
resolution: 0.1
```

---

## 参与贡献

欢迎在 [GitHub Issues](https://github.com/jiyang1018/FOrcaSlicer/issues) 提交缺陷报告与功能建议。这是一个专注于 U1 的分支——报告切片问题时，请附上你的挤出口配置、必要时的 `.3mf` 文件，以及 FOrcaSlicer 版本号（帮助 → 关于）。

---

## 名称由来

**FOrcaSlicer** 有双重含义：

1. **F**lexible **Orca**Slicer——为 OrcaSlicer 增加灵活的逐工作头配置。
2. **Fork**-a-Slicer——原版 OrcaSlicer 的一个分支（fork）。

---

## 传承

FOrcaSlicer 分支自 Snapmaker OrcaSlicer；后者分支自 SoftFever 的 [OrcaSlicer](https://github.com/SoftFever/OrcaSlicer)；OrcaSlicer 分支自 BambuLab 的 Bambu Studio；Bambu Studio 分支自 Prusa Research 的 [PrusaSlicer](https://github.com/prusa3d/PrusaSlicer)；PrusaSlicer 基于 Alessandro Ranellucci 与 RepRap 社区的 [Slic3r](https://github.com/Slic3r/Slic3r)。FOrcaSlicer 还吸收了 @supermerill 的 SuperSlicer 中的特性。

## 许可证

FOrcaSlicer 采用 **GNU Affero 通用公共许可证第 3 版（AGPL-3.0）** 授权——与其之前的 OrcaSlicer、Bambu Studio、PrusaSlicer 和 Slic3r 相同。AGPL-3.0 要求：只要你以任何方式使用本软件的任何部分（即便是在 Web 服务器后端运行），你的软件也必须以相同许可证发布。参见 [LICENSE](LICENSE)。
