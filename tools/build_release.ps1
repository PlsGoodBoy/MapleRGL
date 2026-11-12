$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildDirectory = Join-Path $projectRoot 'build-release'
$cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    $cmakePath = $cmakeCommand.Source
} else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $cmakePath = & $vswhere -latest -products '*' -find 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' | Select-Object -First 1
}
if (-not $cmakePath) { throw 'CMake was not found. Install the Visual Studio C++ and CMake build tools.' }
& $cmakePath -S $projectRoot -B $buildDirectory -A x64 "-DMAPLERGL_STAGE_DIR:PATH=$(Join-Path $projectRoot 'bin-release')"
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& $cmakePath --build $buildDirectory --config Release --target MapleRGL
if ($LASTEXITCODE -ne 0) { throw 'Release build failed.' }
$runtimeLine = Get-Content -LiteralPath (Join-Path $buildDirectory 'CMakeCache.txt') | Where-Object { $_.StartsWith('MAPLERGL_RUNTIME_DIR:INTERNAL=') }
$runtimeDirectory = $runtimeLine.Substring('MAPLERGL_RUNTIME_DIR:INTERNAL='.Length)
# Run the updater in this PowerShell session; MSBuild need not launch another shell.
& (Join-Path $PSScriptRoot 'update_release.ps1') -Executable (Join-Path $buildDirectory 'Release\MapleRGL.exe') -RuntimeDirectory $runtimeDirectory -Configuration Release
