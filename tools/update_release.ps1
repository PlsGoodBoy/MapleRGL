param(
    [Parameter(Mandatory = $true)][string]$Executable,
    [Parameter(Mandatory = $true)][string]$RuntimeDirectory,
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
if ($Configuration -ne 'Release') {
    Write-Output 'The release folder is only updated by a Release build.'
    exit 0
}

$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$releaseDirectory = Join-Path $projectRoot 'release'
$assetSource = Join-Path $projectRoot 'assets'
$assetDestination = [IO.Path]::GetFullPath((Join-Path $releaseDirectory 'assets'))
$runtimeNames = @('msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')
$runtimeFiles = @($runtimeNames | ForEach-Object { Join-Path $RuntimeDirectory $_ })

# Check inputs before replacing the playable files.
foreach ($inputFile in @($Executable, (Join-Path $assetSource 'manifest.json')) + $runtimeFiles) {
    Get-Item -LiteralPath $inputFile -ErrorAction Stop | Out-Null
}
New-Item -ItemType Directory -Path $releaseDirectory -Force | Out-Null
Copy-Item -LiteralPath $Executable -Destination (Join-Path $releaseDirectory 'MapleRGL.exe') -Force
Copy-Item -LiteralPath $runtimeFiles -Destination $releaseDirectory -Force

# Only this generated assets directory is mirrored, including removal of obsolete assets.
$releasePrefix = $releaseDirectory + [IO.Path]::DirectorySeparatorChar
if (-not $assetDestination.StartsWith($releasePrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The asset mirror destination must be inside the project release folder.'
}
& robocopy.exe $assetSource $assetDestination /MIR /R:1 /W:1 /XJ /NFL /NDL /NJH /NJS /NP
if ($LASTEXITCODE -ge 8) { throw "Asset synchronization failed (robocopy exit $LASTEXITCODE)." }

Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'release_README.txt') -Destination (Join-Path $releaseDirectory 'README.txt') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'release_NOTICES.txt') -Destination (Join-Path $releaseDirectory 'THIRD_PARTY_NOTICES.txt') -Force
$packagedExe = Join-Path $releaseDirectory 'MapleRGL.exe'
$hash = (Get-FileHash -LiteralPath $packagedExe -Algorithm SHA256).Hash
$assetCount = (Get-ChildItem -LiteralPath $assetDestination -File -Recurse | Measure-Object).Count
$packagedAt = [TimeZoneInfo]::ConvertTimeBySystemTimeZoneId([DateTimeOffset]::UtcNow, 'China Standard Time').ToString('yyyy-MM-dd HH:mm:ss zzz')
@"
MapleRGL Windows x64 Release - 0.2.0
Updated: $packagedAt
EXE SHA256: $hash
Asset files: $assetCount
"@ | Set-Content -LiteralPath (Join-Path $releaseDirectory 'VERSION.txt') -Encoding UTF8
Write-Output "Release updated: $releaseDirectory"
Write-Output "EXE SHA256: $hash"
exit 0
