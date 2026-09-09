param(
    [string]$EndstoneSource = '',
    [switch]$CoreOnly,
    [switch]$Benchmarks,
    [ValidateNotNullOrEmpty()]
    [string]$BuildDirectory = 'build'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))

function Get-ProjectPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $projectRoot $Path))
}

function Assert-BuildCache([string]$Text, [string]$RequestedSdk) {
    $origin = [regex]::Match($Text, '(?m)^CMAKE_HOME_DIRECTORY:[^=\r\n]+=([^\r\n]*)\r?$')
    if (-not $origin.Success -or
        (Get-ProjectPath $origin.Groups[1].Value).TrimEnd('\', '/') -ne $projectRoot.TrimEnd('\', '/')) {
        throw 'The CMake cache belongs to another or unknown source checkout. Choose a fresh BuildDirectory.'
    }
    $override = [regex]::Match($Text, '(?m)^FETCHCONTENT_SOURCE_DIR_ENDSTONE:[^=\r\n]+=([^\r\n]*)\r?$')
    if (-not $override.Success) {
        if ($Text -match '(?m)^(DIMENGUARD_BUILD_PLUGIN:[^=\r\n]+=ON|endstone_SOURCE_DIR:[^=\r\n]+=)') {
            throw 'The existing cache does not identify its Endstone SDK selection. Choose a fresh BuildDirectory.'
        }
        return
    }
    $cachedSdk = $override.Groups[1].Value
    if ($cachedSdk) { $cachedSdk = (Get-ProjectPath $cachedSdk).TrimEnd('\', '/') }
    if ($cachedSdk -ne $RequestedSdk.TrimEnd('\', '/')) {
        $cachedLabel = if ($cachedSdk) { $cachedSdk } else { 'pinned upstream (no source override)' }
        $requestedLabel = if ($RequestedSdk) { $RequestedSdk } else { 'pinned upstream (no source override)' }
        throw "SDK selection mismatch. Cached: $cachedLabel. Requested: $requestedLabel. " +
            'Use a separate fresh BuildDirectory, such as build/public or build/fork. No cache was changed.'
    }
}

$buildPath = Get-ProjectPath $BuildDirectory
if ($buildPath.TrimEnd('\', '/') -eq $projectRoot.TrimEnd('\', '/')) {
    throw 'In-source builds are not supported. Choose a separate BuildDirectory under build/.'
}
$sdkSource = ''
if ($EndstoneSource) {
    $sdkSource = Get-ProjectPath $EndstoneSource
    if (-not (Test-Path -LiteralPath (Join-Path $sdkSource 'CMakeLists.txt') -PathType Leaf)) {
        throw "EndstoneSource must identify an existing SDK checkout with CMakeLists.txt: $sdkSource"
    }
}
$cachePath = Join-Path $buildPath 'CMakeCache.txt'
if (Test-Path -LiteralPath $cachePath -PathType Leaf) {
    Assert-BuildCache ([System.IO.File]::ReadAllText($cachePath)) $sdkSource
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw 'Git is required to fetch the pinned build dependencies. Install Git and add it to PATH.'
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw 'Visual Studio Installer was not found. Install Visual Studio 2022 C++ Build Tools, Clang and CMake tools.'
}
$components = @('Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
    'Microsoft.VisualStudio.Component.VC.Llvm.Clang', 'Microsoft.VisualStudio.Component.VC.CMake.Project')
$installation = & $vswhere -latest -products * -requires @components -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $installation) {
    throw 'Visual Studio needs the x64 C++ tools, C++ Clang tools and CMake/Ninja components.'
}
$devShell = Join-Path $installation 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll'
$llvm = Join-Path $installation 'VC/Tools/Llvm/x64/bin'
$cmakeRoot = Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake'
$compiler = Join-Path $llvm 'clang-cl.exe'
$cmake = Join-Path $cmakeRoot 'CMake/bin/cmake.exe'
$ctest = Join-Path $cmakeRoot 'CMake/bin/ctest.exe'
$ninja = Join-Path $cmakeRoot 'Ninja/ninja.exe'
foreach ($tool in @($devShell, $compiler, (Join-Path $llvm 'lld-link.exe'), $cmake, $ctest, $ninja)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
        throw "A required Visual Studio tool is missing. Repair the C++/Clang/CMake installation: $tool"
    }
}
$cmakeDescription = & $cmake --version
if ($LASTEXITCODE -ne 0) { throw 'The bundled Visual Studio CMake could not be started.' }
$cmakeVersion = [regex]::Match(($cmakeDescription | Select-Object -First 1), '\d+\.\d+\.\d+')
if (-not $cmakeVersion.Success -or [version]$cmakeVersion.Value -lt [version]'3.29.0') {
    throw 'Update Visual Studio CMake tools: this project requires the bundled CMake to be at least 3.29.'
}
Write-Output "Build directory: $buildPath"
$sdkLabel = if ($sdkSource) { $sdkSource } else { 'pinned upstream (no source override)' }
Write-Output "SDK selection: $sdkLabel"
if ($CoreOnly) { Write-Output 'The Endstone plugin adapter is disabled for this core-only build.' }
Write-Output ($cmakeDescription | Select-Object -First 1)
& $compiler --version
if ($LASTEXITCODE -ne 0) { throw 'The bundled Visual Studio Clang compiler could not be started.' }

Import-Module $devShell
Enter-VsDevShell -VsInstallPath $installation -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$env:PATH = "$llvm;$(Join-Path $cmakeRoot 'CMake/bin');$(Join-Path $cmakeRoot 'Ninja');$env:PATH"
$plugin = if ($CoreOnly) { 'OFF' } else { 'ON' }
$benchmarkTargets = if ($Benchmarks) { 'ON' } else { 'OFF' }
$configureArgs = @('-S', $projectRoot, '-B', $buildPath, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=RelWithDebInfo', '-DCMAKE_C_COMPILER=clang-cl', '-DCMAKE_CXX_COMPILER=clang-cl',
    "-DDIMENGUARD_BUILD_PLUGIN=$plugin", "-DDIMENGUARD_BUILD_BENCHMARKS=$benchmarkTargets", '-DBUILD_TESTING=ON',
    "-DFETCHCONTENT_SOURCE_DIR_ENDSTONE:PATH=$sdkSource")
& $cmake @configureArgs
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& $cmake --build $buildPath --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed.' }
& $ctest --test-dir $buildPath --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
