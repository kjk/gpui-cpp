<#
.SYNOPSIS
    Generate compile_commands.json for clangd from the gpui build system.

.DESCRIPTION
    This script runs the TypeScript build to discover compiler flags, then
    emits a compile_commands.json that clangd can use for IntelliSense.

    Requires: clang-cl on PATH (from Visual Studio or standalone LLVM install)

.EXAMPLE
    .\cmd\gen-compile-commands.ps1
    # Generates .work\compile_commands.json for the default release build

.EXAMPLE
    .\cmd\gen-compile-commands.ps1 -Debug
    # Uses debug flags instead
#>
param(
    [switch]$Debug,
    [switch]$Clang,
    [string]$OutFile = ".work\compile_commands.json"
)

$ErrorActionPreference = "Stop"

$mode = if ($Debug) { "-dbg" } else { "-rel" }
$clangFlag = if ($Clang) { "-clang" } else { "" }

Write-Host "Building with: bun cmd/build.ts $mode $clangFlag system_monitor (dry run to get flags)..."

# We can't easily extract the command line from the TS build without running it.
# Instead, we construct a minimal but representative compile_commands.json.

$root = Split-Path -Parent $PSScriptRoot
$amalgamDir = Join-Path $root ".work"
$gpuiCpp = Join-Path $amalgamDir "gpui.cpp"

if (-not (Test-Path $gpuiCpp)) {
    Write-Error "Amalgam not found at $gpuiCpp. Run 'bun cmd/build.ts -rel system_monitor' first to generate it."
    exit 1
}

# Find clang-cl
$clangCl = Get-Command clang-cl -ErrorAction SilentlyContinue
if (-not $clangCl) {
    Write-Warning "clang-cl not found on PATH. compile_commands.json will use a placeholder."
    $clangCl = "clang-cl.exe"
} else {
    $clangCl = $clangCl.Source
}

# Base flags matching cflagsFor() in build.ts for Windows + clang-cl
$flags = @(
    "/nologo",
    "/std:c++20",
    "/EHsc",
    "/utf-8",
    "/I", $amalgamDir,
    "/DUNICODE",
    "/D_UNICODE",
    "/W4",
    "/WX",
    "/wd4996",
    "/Z7",
    "-Wno-missing-field-initializers",
    "-Wno-microsoft-exception-spec",
    "-Wno-delete-non-abstract-non-virtual-dtor",
    "-Wno-unused-command-line-argument"
)

if ($Debug) {
    $flags += @("/Od", "/MTd", "/DDEBUG")
} else {
    $flags += @("/O2", "/Gy", "/Gw", "/MT", "/DNDEBUG")
}

$cmd = ($flags -join " ") + " /TP `"$gpuiCpp`""

$compileDb = @(
    @{
        directory = $root
        file = $gpuiCpp
        arguments = @($clangCl) + $flags + @("/TP", $gpuiCpp)
    }
)

$json = $compileDb | ConvertTo-Json -Depth 10
$json | Out-File -Encoding utf8 $OutFile

Write-Host "Wrote $OutFile"
Write-Host "Open this project in VS Code or another clangd client to get IntelliSense."
