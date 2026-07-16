param(
  [Parameter(Mandatory = $true)]
  [string]$RepositoryRoot,

  [Parameter(Mandatory = $true)]
  [string]$ComponentDll,

  [Parameter(Mandatory = $true)]
  [string]$OutputDirectory,

  [Parameter(Mandatory = $true)]
  [string]$Version
)

$ErrorActionPreference = 'Stop'

$repo = [IO.Path]::GetFullPath($RepositoryRoot).TrimEnd('\')
$dll = [IO.Path]::GetFullPath($ComponentDll)
$output = [IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\')
$buildRoot = [IO.Path]::GetFullPath((Join-Path $repo 'build')).TrimEnd('\')
$distRoot = [IO.Path]::GetFullPath((Join-Path $repo 'dist')).TrimEnd('\')

if ($Version -cnotmatch '^[0-9]+\.[0-9]+\.[0-9]+$') {
  throw "Version must use major.minor.patch format: $Version"
}

if (-not $dll.StartsWith($buildRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
  throw "Component DLL must be inside the repository build directory: $dll"
}

if ($dll -notmatch '[\\/]Release[\\/]foo_crate\.dll$') {
  throw "Only a Release foo_crate.dll can be packaged: $dll"
}

if (-not $output.Equals($distRoot, [StringComparison]::OrdinalIgnoreCase)) {
  throw "Package output must be the repository dist directory: $output"
}

if (-not (Test-Path -LiteralPath $dll -PathType Leaf)) {
  throw "Component DLL does not exist: $dll"
}

if ([IO.Path]::GetFileName($dll) -cne 'foo_crate.dll') {
  throw "Unexpected component filename: $dll"
}

$staging = Join-Path $buildRoot 'package-staging'
$package = Join-Path $output "FooCrate-$Version.fb2k-component"

if (Test-Path -LiteralPath $staging) {
  Remove-Item -LiteralPath $staging -Recurse -Force
}

New-Item -ItemType Directory -Path $staging -Force | Out-Null
New-Item -ItemType Directory -Path $output -Force | Out-Null
Copy-Item -LiteralPath $dll -Destination (Join-Path $staging 'foo_crate.dll')

if (Test-Path -LiteralPath $package) {
  Remove-Item -LiteralPath $package -Force
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($staging, $package, [IO.Compression.CompressionLevel]::Optimal, $false)

$archive = [IO.Compression.ZipFile]::OpenRead($package)
try {
  $entries = @($archive.Entries | ForEach-Object { $_.FullName.Replace('\', '/') })
} finally {
  $archive.Dispose()
}

if ($entries.Count -ne 1 -or $entries[0] -cne 'foo_crate.dll') {
  throw "Unexpected package contents: $($entries -join ', ')"
}

$hash = (Get-FileHash -LiteralPath $package -Algorithm SHA256).Hash
Write-Output "PACKAGE=$package"
Write-Output "SHA256=$hash"
