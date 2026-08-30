**English** | [简体中文](README.zh_CN.md)

# FOrcaSlicer — Flexible OrcaSlicer

A fork of [Snapmaker OrcaSlicer](https://github.com/Snapmaker/OrcaSlicer) for the Snapmaker U1, built on one idea: the U1 has four independent heads, so stop making them all do the same job. Give each head its own nozzle size, line width, and speed — a fine nozzle on the visible outer wall, coarser nozzles on everything behind it — and you spend precision where it shows and speed where it doesn't. It also adds a Color Patch pipeline that prints a painted surface as a **shell over a different-material core** — a soft grip on a rigid body, or a translucent see-through layer over a solid interior.

> **Status:** Research preview, actively developed. Targets the **Snapmaker U1** 4-head printer. **Windows** and **macOS** have packaged builds; Linux is build-from-source only. Not yet independently bed-tested at this release — please report real-print results.

## Launch demo

A short walkthrough of what FOrcaSlicer does differently from stock Snapmaker OrcaSlicer, across three areas: **mixed nozzle sizes**, **splitting the outer and inner wall onto different heads**, and **Color Patch mode**.

[![FOrcaSlicer launch demo](docs/images/launch-demo-en.jpg)](https://youtu.be/5z-unU9aujo)

### Mixed nozzle sizes: fine outer detail, faster interior

Example is a set of seven metric module-1 gears — different sizes and tooth styles, the kind of spread you'd print for a gearbox or power-transmission project, with 3 copies on the same plate. Screenshot shows the whole plate sliced differently: stock forces one nozzle size across the whole part (outer columns), while FOrcaSlicer prints the outer wall with a fine tip and everything behind it with larger nozzles (middle columns) — keeping the tooth detail of an all-0.2 mm print in roughly half the time.

| Every loop 0.2 mm (stock) | OW 0.2 mm, everything else 0.4 mm | OW 0.2 mm, IW 0.4 mm, everything else 0.6 mm | Every loop 0.4 mm (stock) |
|---|---|---|---|
| ![gear, all 0.2 mm](docs/images/gear-0.2-stock.jpg) | ![gear, FOS 2-4-4](docs/images/gear-fos-2-4-4.jpg) | ![gear, FOS 2-4-6](docs/images/gear-fos-2-4-6.jpg) | ![gear, all 0.4 mm](docs/images/gear-0.4-stock.jpg) |
| Fine detail, slow print — **8h 39m** | Fine detail, faster print — **5h 15m** | Fine detail, faster print — **5h 5m** | Less detail, fastest print — **4h 29m** |

## Download

👉 **[Latest Release](https://github.com/jiyang1018/FOrcaSlicer/releases/latest)**

- `FOrcaSlicer_Windows_Installer_V*.exe` — Windows installer
- `FOrcaSlicer_Windows_Portable_V*.zip` — Windows portable (no install needed)

## Development Roadmap

Track progress, design decisions, and implementation details:
👉 **[View the interactive roadmap](https://jiyang1018.github.io/FOrcaSlicer-roadmap/)**

---

## What it adds

The Snapmaker U1 has 4 independent heads that can carry different nozzle diameters (e.g. 0.2 / 0.4 / 0.6 / 0.8 mm). Stock Snapmaker OrcaSlicer syncs every head to the same size. FOrcaSlicer unlocks them.

### Mixed nozzle sizes

- Independent nozzle diameter per head.
- **Outer wall (OW)** and **inner wall (IW)** are split out from "Walls" in the Multimaterial tab, so they can go to different heads. See the [OW / IW Split guide (wiki)](https://github.com/jiyang1018/FOrcaSlicer/wiki/OW-IW-Split).
- Outer wall, inner wall, infill, solid infill, support, and the prime tower can each be assigned to a different nozzle.
- **Per-nozzle line width and speed** — every feature is sized and sped from the process preset of the nozzle that actually prints it, instead of everything inheriting Nozzle 1.
- **Per-nozzle prime tower line width** — each tool wipes and rams at its own width.
- **One layer height, guarded by the smallest nozzle you use** — layer and first-layer height are a single global value, automatically limited to the range the smallest active nozzle can print, so you can't ask a 0.2 mm tip to lay a 0.6 mm layer.
- Nozzle verification dialog before printing.
- Preview line widths reflect each extruder's actual nozzle diameter.
- **On a single-nozzle-size machine, output is identical to stock** — every change is a no-op when all nozzles match.

![The per-nozzle Quality and Speed tabs](docs/images/per_nozzle_settings.jpg)

*Turn on **Mixed**, give each nozzle its own process preset, then set line width and speed per nozzle in the Quality and Speed tabs. Layer height stays global, below the nozzle tabs.*

### Multi-material supply (multiACE)

More filaments than heads: FOrcaSlicer can be told that a multi-material supply system feeds the printer, and slices for it. Support starts with **multiACE**, the open-source multi-material unit for the U1 by decay71 — see **[decay71/multiACE](https://github.com/decay71/multiACE)**.

- **Declared under Printer settings > Machine > Accessory** — supply system, printer LAN IP, mode (`normal` / `multi` / `head`, using multiACE's own vocabulary), and unit count or unit-driven toolheads. Mode decides how many filaments each nozzle can reach, which is a slicing input, so it is set before slicing rather than at send time.
- **Read from printer** asks the machine for its current wiring and fitted nozzles, shows the full picture, and fills the fields in only after you confirm. It never overwrites nozzle diameters.
- **Send to multiACE** appears under the print dropdown and uploads the sliced file straight to the machine.
- **Slicing refuses combinations the hardware cannot load** — if more filaments demand a nozzle diameter than the declared wiring can hold at once, the print is stopped with the counts spelled out, instead of failing at the printer.
- Exported G-code carries the machine's physical nozzle diameters in the header, so the supply system can match what the file demands against what is actually fitted.

### Color Patch pipeline

When you paint a surface, only its outer shell prints in the painted filament; the interior stays base material. The point isn't color — it's that **the shell and the core can be different materials**, held together by geometry. That unlocks two things stock's paint mode can't do.

**Mix materials and textures.** In stock (Original) mode, each painted material is a solid region sitting *beside* the next — so a soft TPU grip painted next to a rigid PETG body simply delaminates, because TPU won't bond to PETG. Color Patch makes the painted material **wrap** the core instead: a TPU grip becomes a soft skin held mechanically onto a PETG core, and a translucent edge glows over a metallic core. Regions you leave in Original mode stay solid volumes — so red accents read as inset gems rather than surface paint — and the infill pattern shows through the translucent regions as part of the design. None of this needs different nozzle sizes.

| Painted surface, Original mode | Painted surface, Color Patch mode |
|---|---|
| ![axe, original mode](docs/images/axe_sliced_original.jpg) | ![axe, color patch mode](docs/images/axe_sliced_color_patch.jpg) |

*Model: [Star Orbit Judgment Axe](https://makerworld.com/en/models/2332850-star-orbit-judgment-axe) by Ted_k on MakerWorld.*

**See through the shell.** Paint a legging black in Original mode and the leg prints solid black all the way through — a black leg, not clothing. Color Patch prints the painted region as a **translucent shell** (optionally on a 0.2 mm nozzle for a thinner one) over a skin-colored core, so the base reads through it the way fabric does. The effect depends on a **translucent filament** — the best look takes some experimentation with the filament's color and translucency.

| A painted model | Original: legs solid black throughout | Color Patch: translucent shell over skin-colored core |
|---|---|---|
| ![painted character](docs/images/character_painted.jpg) | ![character, original](docs/images/character_painted_original.jpg) | ![character, color patch](docs/images/character_painted_color_patch.jpg) |

*Model: [Anime girl figure](https://makerworld.com/en/models/2598006-anime-girl-figure) by "I know U" on MakerWorld.*

**CL — Color Loops** is the one knob: how many perimeter loops the painted color occupies inward from the surface (CL = 1 is the thinnest skin; higher is thicker and more opaque). Set per object and per painted color. Because loop width follows the *painted nozzle's* own flow, CL is a count of loops, not a fixed thickness — CL = 1 on a 0.8 mm nozzle is four times thicker than CL = 1 on a 0.2 mm nozzle.

![CL = 4 through CL = 1 on a 15 mm cylinder](docs/images/4x_to_1x_CL.jpg)

*CL 4 → 1 on a 15 mm cylinder: translucent gray PLA Color Patch (0.2 mm nozzle) over a skin-color PLA base (0.4 mm nozzle). Lower CL = thinner, more see-through.*

Original-mode geometry stays **byte-identical to stock OrcaSlicer** (verified by diff), and settings save/load with `.3mf`.

> Full walkthrough, material combinations, and CL tuning: **[Color Patch guide (wiki)](https://github.com/jiyang1018/FOrcaSlicer/wiki/Color-Patch)**

---

## Install (Windows)

1. Download the installer or portable zip from the [releases page](https://github.com/jiyang1018/FOrcaSlicer/releases).
2. If it won't start, install these runtimes:
   - [Microsoft Edge WebView2 Runtime](https://go.microsoft.com/fwlink/p/?LinkId=2124703)
   - [Visual C++ 2019 Redistributable (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe)
3. For step-by-step instructions, follow the [Download and Install guide (wiki)](https://github.com/jiyang1018/FOrcaSlicer/wiki/Download-and-Install).

**First run:** follow the [First Launch guide (wiki)](https://github.com/jiyang1018/FOrcaSlicer/wiki/First-Launch) to log in with your Snapmaker account and set up your printer.

**Before your first print:** read [Before You Print (wiki)](https://github.com/jiyang1018/FOrcaSlicer/wiki/Before-You-Print) — picking nozzle types and diameters, replacing hot ends, and why not to sync nozzle info. FOrcaSlicer behaves differently from stock in ways that matter here, so it's worth the few minutes.

**macOS:** a universal build (Intel + Apple Silicon, macOS 12+) is attached to each release.
It is **not signed or notarized**, so macOS refuses the first launch with "Apple cannot check
it for malicious software". After dragging FOrcaSlicer to Applications, clear the quarantine
flag once:

```bash
xattr -dr com.apple.quarantine "/Applications/FOrcaSlicer.app"
```

**Linux:** not yet packaged — build from source (below).

---

## Build from source

### Windows (64-bit)

Requires: **Visual Studio 2022**, CMake 3.14+, Git, Git-LFS, Strawberry Perl.

```powershell
git clone https://github.com/jiyang1018/FOrcaSlicer
cd FOrcaSlicer
git lfs pull

# build dependencies once, then the slicer:
build_release_vs2022.bat deps
build_release_vs2022.bat slicer
# (or just `build_release_vs2022.bat` to do both)
```

Output: `build\src\Release\FOrcaSlicer.exe`

**Build the Windows installer** (requires [NSIS](https://nsis.sourceforge.io/Download)):

```powershell
.\build_installer.ps1 -Version 2.3.2-fos.8.5
```

### macOS (64-bit)

Requires Git, and either full Xcode or just the Command Line Tools
(`xcode-select --install`). Then:

```bash
brew install gettext libtool automake autoconf texinfo ninja
```

**Do not `brew install cmake`.** Homebrew ships CMake 4.x, which removed support for
`cmake_minimum_required(<3.5)`; the dependency tree declares older minimums and configure
fails immediately. Use 3.31.x instead:

```bash
curl -fsSL -o /tmp/cmake.tar.gz https://cmake.org/files/v3.31/cmake-3.31.6-macos-universal.tar.gz
tar -xzf /tmp/cmake.tar.gz -C /tmp
export PATH="/tmp/cmake-3.31.6-macos-universal/CMake.app/Contents/bin:$PATH"
cmake --version
```

`cmake --version` must report 3.31.6 before you continue.

Homebrew's gettext is keg-only, and the slicer build runs `scripts/run_gettext.sh` as its
final step under `set -e`. If `msgfmt` is missing you lose a finished compile at the very
end, so put it on PATH first:

```bash
which msgfmt || export PATH="$(brew --prefix gettext)/bin:$PATH"
```

**Always pass `-x`.** It selects the Ninja Multi-Config generator. Without it the script
defaults to the Xcode generator, which shells out to `xcodebuild` and therefore needs a full
`Xcode.app`; with only the Command Line Tools installed, configure fails with
`No CMAKE_C_COMPILER could be found` even though the toolchain is fine. Ninja works either
way. Add `-b` to skip reconfiguring on a rebuild, and drop `-b` again after editing any
`CMakeLists.txt`.

Deployment target defaults to 12.0. Each architecture has its own dependency tree, so
`-d` for one arch does not help the other.

These steps are verified on both Apple silicon and Intel Macs. The three paths below were
run end to end on Apple silicon (Mac mini M4, Command Line Tools only); the Intel host path
uses the same script and was verified on an Intel MacBook Pro.

Pick the one that matches the result you want, and open just that section.

| What you want | Open |
|---|---|
| The app running on your own Apple silicon Mac | Apple silicon only |
| To test or ship an Intel build | Intel only |
| One DMG that runs on every Mac, for a release | Universal |

#### How long it takes

Cold builds, with `build/`, `deps/build/` and the download cache removed:

| Machine | Single arch | Universal |
|---|---|---|
| Mac mini M4 (10-core) | ~17-20 min | ~38 min |
| MacBook Pro 2019, i9-9880H (8-core) | ~46 min (x86_64) | not measured |
| GitHub Actions `macos-14` | - | ~1h 50m to 2h 15m |

Most of that is the dependency tree, and it is built once. Rebuilding after a code change
takes minutes, not tens of minutes -- add `-b` to skip reconfiguring.

Add `--time` to any build command to print elapsed time per stage plus a total:

```bash
./build_release_macos.sh -s -a arm64 -x --time
```

Without it the output is unchanged. `-T` is the short form -- note the capital. Lowercase
`-t` sets the deployment target and consumes the next argument, so the script rejects a
non-version value there rather than failing later with a confusing CMake error.

<details>
<summary><b>Apple silicon only (arm64)</b> -- native on M-series Macs, will not run on Intel</summary>

Use this only if you want a build for Apple silicon. It is the shortest route: one
dependency tree, one slicer build.

```bash
./build_release_macos.sh -d -a arm64
./build_release_macos.sh -s -a arm64 -x
```

Output: `build/arm64/FOrcaSlicer/FOrcaSlicer.app`

Package it as a DMG:

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
<summary><b>Intel only (x86_64)</b> -- runs on Intel Macs, and on Apple silicon under Rosetta</summary>

Use this only if you want an Intel build. It cross-compiles correctly from an Apple silicon
Mac, so no Intel hardware is needed to produce one. It does need its own dependency tree,
independent of any arm64 build you already have.

To *launch* the result on Apple silicon you need Rosetta
(`softwareupdate --install-rosetta`), and macOS will warn that Intel app support is ending.

```bash
./build_release_macos.sh -d -a x86_64
./build_release_macos.sh -s -a x86_64 -x
```

Output: `build/x86_64/FOrcaSlicer/FOrcaSlicer.app`

Package it as a DMG:

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
<summary><b>Universal (both architectures)</b> -- one DMG for every Mac, what a release ships</summary>

Use this only if you want a single build that runs everywhere. It requires **both** sections
above to have been completed first, since it just lipos the two finished bundles together --
it compiles nothing itself and takes a few minutes.

Budget for that prerequisite: two full dependency trees, several hours from a cold start.

```bash
./build_release_macos.sh -s -a universal -x -b
```

Output: `build/universal/FOrcaSlicer/FOrcaSlicer.app`

Package it as a DMG:

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

#### Notes on the DMG

`ln -s /Applications` gives the usual drag-to-install window, and `xattr -cr` clears
quarantine before imaging so testers do not need `xattr -dr` after a command-line transfer.
Never package a `.app` with plain `zip` -- it mangles bundle symlinks and resource forks and
the app will not launch on another machine. Use `hdiutil` as above, or
`ditto -c -k --sequesterRsrc --keepParent` if you want a zip instead.

Check what you built before shipping it:

```bash
lipo -archs build/universal/FOrcaSlicer/FOrcaSlicer.app/Contents/MacOS/FOrcaSlicer
```

Expect `arm64`, `x86_64`, or `x86_64 arm64` for the universal build.

Debug symbols are written to `build/<arch>/FOrcaSlicer/dSYM/`. They are deliberately not
part of the DMG, but each one matches exactly one build by UUID -- archive the dSYM
alongside any build you distribute, or crash reports from that release cannot be
symbolicated.

A universal DMG is also produced by the **Build macOS release** GitHub Actions workflow.

### Linux (Ubuntu)

> **Not verified.** Unlike the Windows and macOS builds, the Linux path is inherited from
> upstream and has not been tested by this fork. Treat the commands below as a starting
> point rather than a known-good recipe, and please report what you hit.

```bash
./build_linux.sh -u      # first time: install system dependencies
./build_linux.sh -dsi    # build dependencies, slicer, and AppImage
```

---

## Klipper note

If you run Klipper, add this to your `printer.cfg`:

```
[exclude_object]
[gcode_arcs]
resolution: 0.1
```

---

## Contributing

Bug reports and feature requests are welcome at [GitHub Issues](https://github.com/jiyang1018/FOrcaSlicer/issues). This is a U1-focused fork — please include your nozzle setup, a `.3mf` if relevant, and the FOrcaSlicer version (Help → About) when reporting slicing issues.

---

## The name

**FOrcaSlicer** means two things:

1. **F**lexible **Orca**Slicer — extends OrcaSlicer with flexible per-head configuration.
2. **Fork**-a-Slicer — a fork of the original OrcaSlicer.

---

## Lineage

FOrcaSlicer is forked from Snapmaker OrcaSlicer, which is forked from [OrcaSlicer](https://github.com/SoftFever/OrcaSlicer) by SoftFever, which is forked from Bambu Studio by BambuLab, which is forked from [PrusaSlicer](https://github.com/prusa3d/PrusaSlicer) by Prusa Research, which is based on [Slic3r](https://github.com/Slic3r/Slic3r) by Alessandro Ranellucci and the RepRap community. FOrcaSlicer also incorporates features from SuperSlicer by @supermerill.

## License

FOrcaSlicer is licensed under the **GNU Affero General Public License, version 3** — the same license as OrcaSlicer, Bambu Studio, PrusaSlicer, and Slic3r before it. AGPL-3.0 requires that if you use any part of this software in any way (even behind a web server), your software must be released under the same license. See [LICENSE](LICENSE).
