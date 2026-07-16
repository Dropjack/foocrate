param(
  [Parameter(Mandatory = $true)]
  [string]$RepositoryRoot,

  [Parameter(Mandatory = $true)]
  [string]$Version
)

$ErrorActionPreference = 'Stop'

if ($Version -cnotmatch '^[0-9]+\.[0-9]+\.[0-9]+$') {
  throw "Version must use major.minor.patch format: $Version"
}

$repo = [IO.Path]::GetFullPath($RepositoryRoot).TrimEnd('\')
$dist = [IO.Path]::GetFullPath((Join-Path $repo 'dist')).TrimEnd('\')
$package = Join-Path $dist "FooCrate-$Version.fb2k-component"
$installationSource = Join-Path $repo 'docs\INSTALLATION_AND_UPGRADE.md'
$releaseNotesSource = Join-Path $repo "docs\RELEASE_NOTES_$Version.md"
$installationTarget = Join-Path $dist "FooCrate-$Version-INSTALLATION.md"
$releaseNotesTarget = Join-Path $dist "FooCrate-$Version-RELEASE-NOTES.md"
$checksumTarget = Join-Path $dist "FooCrate-$Version-SHA256SUMS.txt"

if (-not (Test-Path -LiteralPath $package -PathType Leaf)) {
  throw "Release package does not exist: $package"
}

foreach ($source in @($installationSource, $releaseNotesSource)) {
  if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
    throw "Release document does not exist: $source"
  }
}

New-Item -ItemType Directory -Path $dist -Force | Out-Null

foreach ($staleName in @('FooCrate-0.1.0.fb2k-component', 'foocrate-0.1.0.fcl')) {
  $stalePath = [IO.Path]::GetFullPath((Join-Path $dist $staleName))
  if (-not $stalePath.StartsWith($dist + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to remove a stale artifact outside dist: $stalePath"
  }
  if (Test-Path -LiteralPath $stalePath -PathType Leaf) {
    Remove-Item -LiteralPath $stalePath -Force
  }
}

Copy-Item -LiteralPath $installationSource -Destination $installationTarget -Force
Copy-Item -LiteralPath $releaseNotesSource -Destination $releaseNotesTarget -Force

$hash = (Get-FileHash -LiteralPath $package -Algorithm SHA256).Hash
$checksumLine = "$hash  $([IO.Path]::GetFileName($package))`n"
[IO.File]::WriteAllText($checksumTarget, $checksumLine, [Text.UTF8Encoding]::new($false))

Write-Output "PACKAGE=$package"
Write-Output "INSTALLATION=$installationTarget"
Write-Output "RELEASE_NOTES=$releaseNotesTarget"
Write-Output "CHECKSUMS=$checksumTarget"
Write-Output "SHA256=$hash"
