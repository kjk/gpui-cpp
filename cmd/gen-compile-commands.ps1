<#
.SYNOPSIS
    Generate compile_commands.json for clangd from the gpui build system.

.DESCRIPTION
    Creates a compile_commands.json that tells clangd to treat all src/** files
    as part of the amalgam (.work/gpui.cpp), not as standalone TUs. This gives
    correct diagnostics and navigation even when a src/** file is open in the editor.

    The amalgam is the only file that actually compiles; src/** are concatenated
    into it with #line directives for mapping diagnostics back.

.EXAMPLE
    .\cmd\gen-compile-commands.ps1
    # Generates .work\compile_commands.json

.NOTES
    Requires: clang-cl on PATH (VS Installer → C++ Clang Compiler for Windows)
#>
param(
    [switch]$Debug,
    [switch]$Clang,
    [string]$OutFile = ".work\compile_commands.json"
)

$ErrorActionPreference = "Stop"
# Hardcode for this known layout; Split-Path above is behaving unexpectedly
# in this shell environment. TODO: make this robust.
$root = "C:\Users\kjk\src\gpui-cpp"
if (-not (Test-Path (Join-Path $root "cmd"))) {
    # Fallback: try to compute from PSScriptRoot
    $scriptDir = Split-Path -Parent $PSScriptRoot
    $root = Split-Path -Parent $scriptDir
}
Write-Host "Debug: root=$root"

$amalgamDir = Join-Path $root ".work"
$gpuiCpp = Join-Path $amalgamDir "gpui.cpp"
$gpuiH = Join-Path $amalgamDir "gpui.h"

if (-not (Test-Path $gpuiCpp) -or -not (Test-Path $gpuiH)) {
    Write-Error @"
Amalgam not found. Run a build first:
    bun cmd/build.ts -rel system_monitor

This generates .work/gpui.h and .work/gpui.cpp which compile_commands.json references.
"@
    exit 1
}

# Find clang-cl for the compile commands (clangd uses it to understand flags)
$clangCl = Get-Command clang-cl -ErrorAction SilentlyContinue
if (-not $clangCl) {
    Write-Warning "clang-cl not found on PATH. Using placeholder 'clang-cl.exe'."
    Write-Warning "For full IntelliSense, install 'C++ Clang Compiler for Windows' via VS Installer."
    $clangClPath = "clang-cl.exe"
} else {
    $clangClPath = $clangCl.Source
}

# Base flags matching cflagsFor() in build.ts for Windows + clang-cl (release)
$baseFlags = @(
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
    "-Wno-unused-command-line-argument",
    "-Wno-unused-function"
)

if ($Debug) {
    $baseFlags += @("/Od", "/MTd", "/DDEBUG")
} else {
    $baseFlags += @("/O2", "/Gy", "/Gw", "/MT", "/DNDEBUG")
}

# The amalgam itself is the main TU
$commands = @()

# Entry for the amalgam - this is what actually compiles
$commands += @{
    directory = $root
    file = $gpuiCpp
    arguments = @($clangClPath) + $baseFlags + @("/TP", $gpuiCpp)
}

# Also register gpui.h as a "header" the amalgam depends on
# (clangd uses this for include resolution)
$commands += @{
    directory = $root
    file = $gpuiH
    arguments = @($clangClPath) + $baseFlags + @("/TP", $gpuiH)
}

# For every src/** file, create an entry that points clangd at the amalgam.
# When clangd sees a src/** file, it will use the amalgam's compile command,
# which gives it the full context (#defines, includes, amalgam header).
# The #line directives in the amalgam then map diagnostics back to src/**.
Get-ChildItem -Path (Join-Path $root "src") -Recurse -File -Include "*.cpp","*.h","*.hpp" | ForEach-Object {
    $srcFile = $_.FullName
    $commands += @{
        directory = $root
        file = $srcFile
        # Point at the amalgam, not the src file. clangd will see the #line markers
        # and display diagnostics in the correct src location.
        arguments = @($clangClPath) + $baseFlags + @("/TP", $gpuiCpp)
    }
}

$json = $commands | ConvertTo-Json -Depth 10
$json | Out-File -Encoding utf8 $OutFile

Write-Host "Wrote $OutFile with $($commands.Count) entries"
Write-Host ""
Write-Host "clangd will now:"
Write-Host "  - Index the amalgam (.work/gpui.cpp) as the real TU"
Write-Host "  - Map diagnostics back to src/** via #line directives"
Write-Host "  - Resolve includes correctly for both amalgam and src/** files"
Write-Host ""
Write-Host "Restart your language server (Zed: 'editor: restart language server')"
Write-Host "or reload the window for changes to take effect."
