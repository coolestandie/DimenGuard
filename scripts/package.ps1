[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')]
    [string]$EndstoneRevision,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[a-z0-9][a-z0-9-]{0,63}$')]
    [string]$CompatibilityLabel,

    [Parameter(Mandatory = $true)]
    [ValidateSet('UpstreamPinned', 'CustomFork')]
    [string]$SdkVariant,

    [Parameter(Mandatory = $true)]
    [string]$LicenseFile,

    [string]$BuildDirectory = 'build',

    [ValidatePattern('^rc[1-9][0-9]*$')]
    [string]$Candidate = 'rc1',

    [switch]$ValidateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$utf8 = New-Object System.Text.UTF8Encoding($false)

function Get-ProjectPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $projectRoot $Path))
}

function Assert-NoReparsePoint([string]$Path) {
    $current = [System.IO.Path]::GetFullPath($Path)
    while ($current) {
        if (Test-Path -LiteralPath $current) {
            $item = Get-Item -LiteralPath $current -Force
            if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Reparse points are not accepted for release inputs or output: $current"
            }
        }
        $current = Split-Path -Parent $current
    }
}

function Get-RequiredFile([string]$Path) {
    $absolute = Get-ProjectPath $Path
    Assert-NoReparsePoint $absolute
    if (-not (Test-Path -LiteralPath $absolute -PathType Leaf)) {
        throw "Required release input does not exist: $absolute"
    }
    if ((Get-Item -LiteralPath $absolute).Length -eq 0) {
        throw "Required release input is empty: $absolute"
    }
    return $absolute
}

function Invoke-ProjectGit([string[]]$GitArguments) {
    $result = & git -C $projectRoot @GitArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Git release validation failed: git $($GitArguments -join ' ')"
    }
    return $result
}

function Get-CacheValue([string]$Name) {
    $pattern = '(?m)^' + [regex]::Escape($Name) + ':[^=\r\n]+=([^\r\n]*)\r?$'
    $match = [regex]::Match($cacheText, $pattern)
    if (-not $match.Success) {
        throw "The CMake cache is missing $Name. Configure and build the release directory first."
    }
    return $match.Groups[1].Value
}

function Assert-X64Dll([string]$Path) {
    $stream = [System.IO.File]::OpenRead($Path)
    $reader = New-Object System.IO.BinaryReader($stream)
    try {
        if ($stream.Length -lt 64 -or $reader.ReadUInt16() -ne 0x5a4d) {
            throw 'The plugin input is not a Windows PE DLL.'
        }
        $stream.Position = 0x3c
        $peOffset = $reader.ReadUInt32()
        if ($peOffset -gt ($stream.Length - 26)) {
            throw 'The plugin input has an invalid PE header offset.'
        }
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550 -or $reader.ReadUInt16() -ne 0x8664) {
            throw 'The package requires a Windows x64 plugin DLL.'
        }
        $stream.Position = $peOffset + 22
        if (($reader.ReadUInt16() -band 0x2000) -eq 0 -or $reader.ReadUInt16() -ne 0x20b) {
            throw 'The plugin input is not a Windows x64 DLL image.'
        }
    }
    finally {
        $reader.Dispose()
    }
}

function Remove-OwnedStaging([string]$Path, [string]$Parent, [string]$ExpectedName) {
    $absolute = [System.IO.Path]::GetFullPath($Path)
    if ((Split-Path -Parent $absolute) -ne $Parent -or
        (Split-Path -Leaf $absolute) -ne $ExpectedName -or
        $ExpectedName -notmatch '^\.package-[0-9a-f]{32}$') {
        throw 'Refusing cleanup outside the uniquely owned packaging directory.'
    }
    Assert-NoReparsePoint $absolute
    if (Test-Path -LiteralPath $absolute) {
        Remove-Item -LiteralPath $absolute -Recurse -Force
    }
}

$repositoryRoot = [System.IO.Path]::GetFullPath((Invoke-ProjectGit @('rev-parse', '--show-toplevel')))
if ($repositoryRoot -ne $projectRoot) {
    throw 'Run this script from the independent DimenGuard Git repository, not a source export or parent repository.'
}
$dirty = @(Invoke-ProjectGit @('status', '--porcelain', '--untracked-files=no'))
if ($dirty.Count -ne 0) {
    throw 'Tracked source changes are present. Commit the intended release before packaging; local ignored notes are excluded.'
}
$sourceRevision = (Invoke-ProjectGit @('rev-parse', 'HEAD')).Trim()
$buildPath = Get-ProjectPath $BuildDirectory
$dllPath = Get-RequiredFile (Join-Path $buildPath 'endstone_dimenguard.dll')
$versionPath = Get-RequiredFile (Join-Path $buildPath 'generated/dimenguard/version.h')
$cachePath = Get-RequiredFile (Join-Path $buildPath 'CMakeCache.txt')
$licensePath = Get-RequiredFile $LicenseFile
$cmakePath = Get-RequiredFile 'CMakeLists.txt'
$cacheText = [System.IO.File]::ReadAllText($cachePath)
if ((Get-ProjectPath (Get-CacheValue 'CMAKE_HOME_DIRECTORY')) -ne $projectRoot) {
    throw 'The build directory belongs to a different source checkout.'
}
if ((Get-CacheValue 'DIMENGUARD_BUILD_PLUGIN') -ne 'ON') {
    throw 'The build directory was not configured to build the plugin.'
}
$configuration = Get-CacheValue 'CMAKE_BUILD_TYPE'
if ($configuration -notin @('Release', 'RelWithDebInfo', 'MinSizeRel')) {
    throw 'Use a Release, RelWithDebInfo or MinSizeRel build for a release candidate.'
}
$versionMatch = [regex]::Match([System.IO.File]::ReadAllText($versionPath),
    '(?m)^#define\s+DIMENGUARD_VERSION\s+"([0-9]+\.[0-9]+\.[0-9]+)"\s*$')
if (-not $versionMatch.Success) {
    throw 'The generated version header does not contain a supported DIMENGUARD_VERSION value.'
}
$version = $versionMatch.Groups[1].Value
$cmakeText = [System.IO.File]::ReadAllText($cmakePath)
$projectVersion = [regex]::Match($cmakeText, '(?s)project\(dimenguard\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)\s')
if (-not $projectVersion.Success -or $projectVersion.Groups[1].Value -ne $version) {
    throw 'The generated version does not match the source project version. Rebuild this checkout before packaging.'
}
$pinMatch = [regex]::Match($cmakeText, '(?s)FetchContent_Declare\(endstone\s+.*?GIT_TAG\s+([0-9a-fA-F]{40})\s*\)')
if (-not $pinMatch.Success) {
    throw 'The pinned Endstone revision could not be read from CMakeLists.txt.'
}
$EndstoneRevision = $EndstoneRevision.ToLowerInvariant()
$sdkOverride = Get-CacheValue 'FETCHCONTENT_SOURCE_DIR_ENDSTONE'
if ($SdkVariant -eq 'UpstreamPinned' -and $EndstoneRevision -ne $pinMatch.Groups[1].Value.ToLowerInvariant()) {
    throw 'An UpstreamPinned package must identify the exact Endstone revision pinned by this source checkout.'
}
if ($SdkVariant -eq 'UpstreamPinned' -and $sdkOverride) {
    throw 'An UpstreamPinned package requires a fresh default SDK build without an EndstoneSource override.'
}
if ($SdkVariant -eq 'CustomFork' -and -not $sdkOverride) {
    throw 'A CustomFork package requires a build configured with an explicit EndstoneSource override.'
}
Assert-X64Dll $dllPath

$documentPaths = @(Invoke-ProjectGit @('ls-files', '--', 'README.md', 'THIRD_PARTY_NOTICES.md', 'benchmarks/README.md', 'docs'))
$requiredDocuments = @('README.md', 'THIRD_PARTY_NOTICES.md', 'benchmarks/README.md',
    'docs/testing.md', 'docs/event-coverage.md', 'docs/releasing.md')
foreach ($required in $requiredDocuments) {
    if ($required -notin $documentPaths) {
        throw "Commit the required release document before packaging: $required"
    }
}
$documents = foreach ($relative in $documentPaths) {
    if ($relative -notmatch '^(README\.md|THIRD_PARTY_NOTICES\.md|benchmarks/README\.md|docs/[a-zA-Z0-9_/-]+\.md)$' -or
        $relative.Split('/') -contains '..') {
        throw "Only tracked Markdown release documentation is allowed in the package: $relative"
    }
    [pscustomobject]@{ Relative = $relative; Source = (Get-RequiredFile $relative) }
}
$variantLabel = if ($SdkVariant -eq 'UpstreamPinned') { 'upstream' } else { 'custom-fork' }
$packageName = "DimenGuard-$version-$Candidate-windows-x64-$variantLabel-$CompatibilityLabel-$($sourceRevision.Substring(0, 7))"
$distPath = Join-Path $projectRoot 'dist'
Assert-NoReparsePoint $distPath
$archivePath = Join-Path $distPath "$packageName.zip"
$archiveHashPath = "$archivePath.sha256"
if ((Test-Path -LiteralPath $archivePath) -or (Test-Path -LiteralPath $archiveHashPath)) {
    throw "A release artifact already exists. Choose a new candidate number; nothing will be overwritten: $archivePath"
}
if ($ValidateOnly) {
    Write-Output "Release preflight passed: $packageName"
    Write-Output "Inputs: one x64 DLL, $($documents.Count) tracked Markdown documents and the explicitly supplied license."
    Write-Output 'No files were copied or created. Build provenance and gameplay acceptance still require the release checklist.'
    return
}

$stagingName = '.package-' + [guid]::NewGuid().ToString('N')
$stagingPath = Join-Path $distPath $stagingName
$stagingOwned = $false
try {
    [System.IO.Directory]::CreateDirectory($distPath) | Out-Null
    New-Item -Path $stagingPath -ItemType Directory -ErrorAction Stop | Out-Null
    $stagingOwned = $true
    $payloadPath = Join-Path $stagingPath 'payload'
    [System.IO.Directory]::CreateDirectory($payloadPath) | Out-Null
    Copy-Item -LiteralPath $dllPath -Destination (Join-Path $payloadPath 'endstone_dimenguard.dll')
    Copy-Item -LiteralPath $licensePath -Destination (Join-Path $payloadPath 'LICENSE')
    foreach ($document in $documents) {
        $destination = Join-Path $payloadPath $document.Relative
        [System.IO.Directory]::CreateDirectory((Split-Path -Parent $destination)) | Out-Null
        Copy-Item -LiteralPath $document.Source -Destination $destination
    }
    $metadata = [ordered]@{
        package = 'DimenGuard'
        version = $version
        candidate = $Candidate
        status = 'release-candidate'
        gameplay_acceptance = 'pending'
        created_utc = [DateTime]::UtcNow.ToString('o')
        platform = 'windows-x64'
        configuration = $configuration
        plugin_revision = $sourceRevision
        endstone_revision = $EndstoneRevision
        sdk_variant = $SdkVariant
        compatibility_label = $CompatibilityLabel
        sdk_provenance = 'Caller-attested build revision; see docs/releasing.md.'
        compatibility_note = 'API version alone does not establish binary or gameplay compatibility.'
    }
    [System.IO.File]::WriteAllText((Join-Path $payloadPath 'release.json'), ($metadata | ConvertTo-Json) + "`n", $utf8)
    $checksums = foreach ($file in (Get-ChildItem -LiteralPath $payloadPath -Recurse -File | Sort-Object FullName)) {
        $relative = $file.FullName.Substring($payloadPath.Length + 1).Replace('\', '/')
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $relative"
    }
    [System.IO.File]::WriteAllText((Join-Path $payloadPath 'SHA256SUMS'), ($checksums -join "`n") + "`n", $utf8)
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $stagedArchive = Join-Path $stagingPath "$packageName.zip"
    [System.IO.Compression.ZipFile]::CreateFromDirectory($payloadPath, $stagedArchive)
    $archiveHash = (Get-FileHash -LiteralPath $stagedArchive -Algorithm SHA256).Hash.ToLowerInvariant()
    $stagedHash = Join-Path $stagingPath "$packageName.zip.sha256"
    [System.IO.File]::WriteAllText($stagedHash, "$archiveHash  $packageName.zip`n", $utf8)
    [System.IO.File]::Move($stagedArchive, $archivePath)
    [System.IO.File]::Move($stagedHash, $archiveHashPath)
    Write-Output "Created release candidate: $archivePath"
    Write-Output "Archive SHA256: $archiveHash"
    Write-Output 'No publication, deployment or server changes were performed. Gameplay acceptance remains pending.'
}
finally {
    if ($stagingOwned) {
        Remove-OwnedStaging $stagingPath $distPath $stagingName
    }
}
