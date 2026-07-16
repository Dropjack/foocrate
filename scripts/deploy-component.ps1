param(
  [Parameter(Mandatory = $true)]
  [ValidateSet('dev', 'test')]
  [string]$Instance,

  [Parameter(Mandatory = $true)]
  [string]$ComponentDll
)

$ErrorActionPreference = 'Stop'

$repo = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\')
$dll = [IO.Path]::GetFullPath($ComponentDll)
$buildRoot = [IO.Path]::GetFullPath((Join-Path $repo 'build')).TrimEnd('\')
$instanceName = if ($Instance -eq 'dev') { 'foobar-dev' } else { 'foobar-test' }
$instanceRoot = [IO.Path]::GetFullPath((Join-Path $repo ".local/$instanceName")).TrimEnd('\')
$allowedRoot = [IO.Path]::GetFullPath((Join-Path $repo '.local')).TrimEnd('\')

if (-not $instanceRoot.StartsWith($allowedRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
  throw "Resolved instance escaped the allowed .local directory: $instanceRoot"
}

if ($instanceRoot -notin @(
    [IO.Path]::GetFullPath((Join-Path $repo '.local/foobar-dev')).TrimEnd('\'),
    [IO.Path]::GetFullPath((Join-Path $repo '.local/foobar-test')).TrimEnd('\')
  )) {
  throw "Unsupported deployment target: $instanceRoot"
}

if (-not $dll.StartsWith($buildRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
  throw "Component DLL must be inside the repository build directory: $dll"
}

if (-not (Test-Path -LiteralPath $dll -PathType Leaf)) {
  throw "Component DLL does not exist: $dll"
}

if ([IO.Path]::GetFileName($dll) -cne 'foo_crate.dll') {
  throw "Unexpected component filename: $dll"
}

$foobarExe = Join-Path $instanceRoot 'foobar2000.exe'
if (-not (Test-Path -LiteralPath $foobarExe -PathType Leaf)) {
  throw "Allowed foobar2000 instance is missing: $foobarExe"
}

$running = Get-CimInstance Win32_Process -Filter "Name = 'foobar2000.exe'" |
  Where-Object { $_.ExecutablePath -and ([IO.Path]::GetFullPath($_.ExecutablePath) -ieq $foobarExe) }
if ($running) {
  throw "Close $instanceName before deploying FooCrate."
}

$destination = Join-Path $instanceRoot 'profile/user-components-x64/foo_crate'
New-Item -ItemType Directory -Path $destination -Force | Out-Null
Copy-Item -LiteralPath $dll -Destination (Join-Path $destination 'foo_crate.dll') -Force

Write-Output "DEPLOYED_TO=$destination"
