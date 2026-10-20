param(
    [string]$BuildDir = "build",
    [string]$QtDir = "C:\Qt\6.9.3\mingw_64",
    [string]$MingwDir = "C:\Qt\Tools\mingw1310_64"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$env:PATH = "$MingwDir\bin;$QtDir\bin;$env:PATH"

cmake -S $root -B "$root\$BuildDir" -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_PREFIX_PATH="$QtDir" `
    -DCMAKE_C_COMPILER="$MingwDir\bin\gcc.exe" `
    -DCMAKE_CXX_COMPILER="$MingwDir\bin\g++.exe" `
    "$@"

cmake --build "$root\$BuildDir"
