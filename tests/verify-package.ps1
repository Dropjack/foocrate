param(
  [Parameter(Mandatory = $true)]
  [string]$PackagePath
)

$ErrorActionPreference = 'Stop'
$package = [IO.Path]::GetFullPath($PackagePath)

if (-not (Test-Path -LiteralPath $package -PathType Leaf)) {
  throw "Package does not exist: $package"
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($package)
try {
  $entries = @($archive.Entries | ForEach-Object { $_.FullName.Replace('\', '/') })
} finally {
  $archive.Dispose()
}

if ($entries.Count -ne 1 -or $entries[0] -cne 'foo_refrain.dll') {
  throw "Package audit failed: $($entries -join ', ')"
}

$simulationRoot = Join-Path ([IO.Path]::GetTempPath()) ("refrain-package-" + [Guid]::NewGuid().ToString('N'))
$componentRoot = Join-Path $simulationRoot 'profile\user-components-x64\foo_refrain'

try {
  New-Item -ItemType Directory -Path $componentRoot -Force | Out-Null
  [IO.Compression.ZipFile]::ExtractToDirectory($package, $componentRoot)

  $installedDll = Join-Path $componentRoot 'foo_refrain.dll'
  $installedFiles = @(Get-ChildItem -LiteralPath $componentRoot -File -Recurse)
  $installedDirectories = @(Get-ChildItem -LiteralPath $componentRoot -Directory -Recurse)

  if (-not (Test-Path -LiteralPath $installedDll -PathType Leaf)) {
    throw "Package install simulation did not place foo_refrain.dll at the component root: $componentRoot"
  }

  if ($installedFiles.Count -ne 1 -or $installedDirectories.Count -ne 0) {
    throw "Package install simulation produced an unexpected nested layout: $componentRoot"
  }
} finally {
  if (Test-Path -LiteralPath $simulationRoot) {
    Remove-Item -LiteralPath $simulationRoot -Recurse -Force
  }
}

Write-Output 'PACKAGE_AUDIT_OK'
