param(
    [string]$OutputDir = "dist/MMC",
    [string]$BuildDir = "build",
    [string]$QtDir = "C:\Qt\6.9.3\mingw_64",
    [string]$MingwBinDir = "C:\Qt\Tools\mingw1310_64\bin"
)

# Makes a folder that can be copied to a USB stick and started on any
# Windows computer: MMC.exe plus the Qt runtime it needs.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "$BuildDir/bin/MMC.exe"

if (-not (Test-Path $exe)) {
    throw "MMC.exe not found, build it first with tools\build.ps1"
}

$out = Join-Path $root $OutputDir
Remove-Item -Recurse -Force $out -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $out | Out-Null

Copy-Item $exe $out
& "$QtDir\bin\windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw $out/MMC.exe

# The MinGW runtime, without it the program does not start on another computer.
foreach ($dll in @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
    $source = Join-Path $MingwBinDir $dll
    if (Test-Path $source) {
        Copy-Item $source $out
    } else {
        Write-Warning "Missing $source, the portable build will not start on another computer."
    }
}

# A vault can sit next to the program on the stick.
$vaults = Join-Path $out "vaults"
New-Item -ItemType Directory -Force -Path $vaults | Out-Null

Write-Host "Portable build in $out"
Write-Host "Copy your vault (Documents\MMC\*.mmcv) next to it, or point MMC at it."
