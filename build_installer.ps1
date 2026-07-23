# build_installer.ps1 - FOS release packaging
#
# Produces two artifacts in .\dist, both named for the same version:
#   FOrcaSlicer_Windows_Installer_V<Version>.exe   (NSIS, from installer.nsi)
#   FOrcaSlicer_Windows_Portable_V<Version>.zip    (unpacked tree, run in place)
#
# The zip mirrors the same exclusion list installer.nsi uses for its File /r payload,
# so the portable build contains exactly what the installer would lay down.
#
# Usage:
#   .\build_installer.ps1                          # version from git describe
#   .\build_installer.ps1 -Version 2.3.2-fos.8.2
#   .\build_installer.ps1 -Version 2.3.2-fos.8.2 -SkipZip
#   .\build_installer.ps1 -Version 2.3.2-fos.8.2 -SourceDir .\build\FOrcaSlicer

[CmdletBinding()]
param(
    [string] $Version,
    [string] $SourceDir = ".\build\FOrcaSlicer",
    [string] $OutDir    = ".\dist",
    [string] $MakeNsis  = "C:\Program Files (x86)\NSIS\makensis.exe",
    [switch] $SkipZip,
    [switch] $SkipInstaller
)

$ErrorActionPreference = "Stop"
Set-Location -Path $PSScriptRoot

function Fail($msg) { Write-Host "ERROR: $msg" -ForegroundColor Red; exit 1 }
function Step($msg) { Write-Host "==> $msg" -ForegroundColor Cyan }

# ---------------------------------------------------------------- version
if (-not $Version) {
    $tag = (git describe --tags --abbrev=0 2>$null)
    if ($LASTEXITCODE -ne 0 -or -not $tag) {
        Fail "No -Version given and 'git describe --tags' found nothing. Pass -Version explicitly, e.g. -Version 2.3.2-fos.8.2"
    }
    # v2.3.2-fos.8.2 -> 2.3.2-fos.8.2
    $Version = $tag -replace '^v', ''
    Write-Host "Version not given; using latest tag: $tag -> $Version" -ForegroundColor Yellow
}

$installerName = "FOrcaSlicer_Windows_Installer_V$Version.exe"
$portableName  = "FOrcaSlicer_Windows_Portable_V$Version.zip"

# ---------------------------------------------------------------- preflight
if (-not (Test-Path $SourceDir)) {
    Fail "Payload dir not found: $SourceDir  (build the slicer first: .\build_release_vs2022.bat slicer)"
}
$exe = Join-Path $SourceDir "FOrcaSlicer.exe"
if (-not (Test-Path $exe)) {
    Fail "FOrcaSlicer.exe not found in $SourceDir - that dir is not a finished build."
}
if (-not $SkipInstaller -and -not (Test-Path $MakeNsis)) {
    Fail "makensis not found at: $MakeNsis  (install NSIS, or pass -MakeNsis <path>)"
}

$built = (Get-Item $exe).LastWriteTime
Write-Host ""
Write-Host "  version   : $Version"
Write-Host "  payload   : $SourceDir  (built $built)"
Write-Host "  out dir   : $OutDir"
Write-Host ""

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

# ---------------------------------------------------------------- installer (NSIS)
if (-not $SkipInstaller) {
    Step "Building installer: $installerName"

    $nsisOut = Join-Path (Resolve-Path $OutDir) $installerName

    & $MakeNsis `
        /DVERSION=$Version `
        /DSOURCE_DIR=$SourceDir `
        /DOUTPUT_FILE=$nsisOut `
        installer.nsi

    if ($LASTEXITCODE -ne 0) { Fail "makensis failed with exit code $LASTEXITCODE" }
    if (-not (Test-Path $nsisOut)) { Fail "makensis reported success but $nsisOut is missing." }

    $mb = [math]::Round((Get-Item $nsisOut).Length / 1MB, 1)
    Write-Host "    OK  $installerName  ($mb MB)" -ForegroundColor Green
}

# ---------------------------------------------------------------- portable (zip)
if (-not $SkipZip) {
    Step "Building portable zip: $portableName"

    # Stage a clean copy so the zip contains the same set of files the installer
    # would write. Keep this list in sync with the File /r excludes in installer.nsi.
    $stageRoot = Join-Path $env:TEMP ("fos_portable_" + [guid]::NewGuid().ToString("N"))
    $stage     = Join-Path $stageRoot "FOrcaSlicer"
    New-Item -ItemType Directory -Force -Path $stage | Out-Null

    $excludeFiles = @(
        "*.pdb","*.ilk","*.exp","*.lib","*.obj","*.idb","*.tlog",
        "*.h","*.hpp","*.c","*.cpp","*.cxx","*.cc",
        "*.vcxproj","*.vcxproj.filters","*.vcxproj.user","*.sln",
        "*.cmake","*.py","*.md"
    )
    $excludeDirs = @(
        "CMakeFiles","RelWithDebInfo","Debug","MinSizeRel",".vs",
        "vcpkg_installed","include","lib"
    )

    # robocopy: 8 = first bit that means "real failure"; 0-7 are success//copied/extra
    robocopy $SourceDir $stage /E /NFL /NDL /NJH /NJS /NP `
        /XF $excludeFiles `
        /XD $excludeDirs | Out-Null
    if ($LASTEXITCODE -ge 8) {
        Remove-Item -Recurse -Force $stageRoot -ErrorAction SilentlyContinue
        Fail "robocopy failed with exit code $LASTEXITCODE"
    }

    if (-not (Test-Path (Join-Path $stage "FOrcaSlicer.exe"))) {
        Remove-Item -Recurse -Force $stageRoot -ErrorAction SilentlyContinue
        Fail "Staged tree is missing FOrcaSlicer.exe - exclusion list is too aggressive."
    }

    $zipOut = Join-Path (Resolve-Path $OutDir) $portableName
    if (Test-Path $zipOut) { Remove-Item -Force $zipOut }

    # Zip the FOrcaSlicer\ folder itself, so the archive extracts to a single top-level dir.
    # FOS: prefer 7-Zip (multithreaded) for the portable .zip - much faster than Compress-Archive
    # on this many-file tree; falls back to Compress-Archive if 7-Zip is not installed. Format
    # stays .zip (universal extract); the large core DLL is the single-thread floor.
    $sevenZip = @(
        "$env:ProgramFiles\7-Zip\7z.exe",
        "${env:ProgramFiles(x86)}\7-Zip\7z.exe",
        (Get-Command 7z.exe -ErrorAction SilentlyContinue).Source
    ) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1

    if ($sevenZip) {
        Push-Location $stageRoot
        & $sevenZip a -tzip -mx=9 -mmt=on -bso0 -bsp0 $zipOut "FOrcaSlicer" | Out-Null
        $ec = $LASTEXITCODE; Pop-Location
        if ($ec -ne 0) {
            Remove-Item -Recurse -Force $stageRoot -ErrorAction SilentlyContinue
            Fail "7-Zip failed with exit code $ec"
        }
    } else {
        Compress-Archive -Path $stage -DestinationPath $zipOut -CompressionLevel Optimal
    }

    Remove-Item -Recurse -Force $stageRoot -ErrorAction SilentlyContinue

    if (-not (Test-Path $zipOut)) { Fail "Portable zip step produced no output." }
    $mb = [math]::Round((Get-Item $zipOut).Length / 1MB, 1)
    Write-Host "    OK  $portableName  ($mb MB)" -ForegroundColor Green
}

# ---------------------------------------------------------------- summary
Write-Host ""
Step "Done. Artifacts in $OutDir :"
Get-ChildItem $OutDir -Filter "*V$Version.*" |
    Select-Object Name, @{n="Size(MB)";e={[math]::Round($_.Length/1MB,1)}}, LastWriteTime |
    Format-Table -AutoSize
