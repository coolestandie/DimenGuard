param(
    [string]$EndstoneSource = '',
    [switch]$CoreOnly,
    [switch]$Benchmarks,
    [string]$BuildDirectory = 'build'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Install Visual Studio C++ Build Tools and the Clang tools component.' }
$devShell = Join-Path $installation 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll'
Import-Module $devShell
Enter-VsDevShell -VsInstallPath $installation -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$llvm = Join-Path $installation 'VC/Tools/Llvm/x64/bin'
$cmakeRoot = Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake'
$env:PATH = "$llvm;$(Join-Path $cmakeRoot 'CMake/bin');$(Join-Path $cmakeRoot 'Ninja');$env:PATH"
$buildPath = Join-Path $projectRoot $BuildDirectory
$plugin = if ($CoreOnly) { 'OFF' } else { 'ON' }
$benchmarkTargets = if ($Benchmarks) { 'ON' } else { 'OFF' }
$configureArgs = @('-S', $projectRoot, '-B', $buildPath, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=RelWithDebInfo', '-DCMAKE_C_COMPILER=clang-cl', '-DCMAKE_CXX_COMPILER=clang-cl',
    "-DDIMENGUARD_BUILD_PLUGIN=$plugin", "-DDIMENGUARD_BUILD_BENCHMARKS=$benchmarkTargets", '-DBUILD_TESTING=ON')
if ($EndstoneSource) { $configureArgs += "-DFETCHCONTENT_SOURCE_DIR_ENDSTONE=$((Resolve-Path -LiteralPath $EndstoneSource).Path)" }
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& cmake --build $buildPath --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed.' }
& ctest --test-dir $buildPath --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
