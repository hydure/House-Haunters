# Builds the standalone HH.exe (if not built yet) and copies it to
# dist\HH.exe. Because the resources/ tree is now baked directly into
# HH.exe (HH_EMBED_RESOURCES=ON, see CMakeLists.txt) and audio is
# provided by the vendored miniaudio single-header library (compiled
# into HH.exe), HH.exe is the *only* file end users need -- no readme,
# no zip, no resources folder.
#
# Output:
#   dist/HH.exe
#
# Usage:
#   .\_package_standalone.ps1
#   .\_package_standalone.ps1 -NoBuild   # skip the rebuild step

param(
    [switch] $NoBuild
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $repoRoot

if (-not $NoBuild) {
    Write-Host "==> Building standalone HH.exe..." -ForegroundColor Cyan
    & "$env:ComSpec" /c "_build_standalone.bat"
    if ($LASTEXITCODE -ne 0) { throw "_build_standalone.bat failed (exit $LASTEXITCODE)" }
}

$exe = Join-Path $repoRoot 'build-standalone\HH.exe'
if (-not (Test-Path $exe)) { throw "Missing artifact: $exe" }

# Sanity check: the exe should be large (resources embedded). If it's
# under ~30 MB the standalone build was probably configured without
# HH_EMBED_RESOURCES, which would mean players need the resources/
# folder again -- exactly what this script is designed to prevent.
$exeSizeMB = [math]::Round((Get-Item $exe).Length / 1MB, 2)
if ($exeSizeMB -lt 30) {
    Write-Warning ("HH.exe is only {0} MB -- resources may NOT be embedded. " +
                   "Double-check that HH_EMBED_RESOURCES=ON in the standalone build." -f $exeSizeMB)
}

$dist = Join-Path $repoRoot 'dist'
if (-not (Test-Path $dist)) { New-Item -ItemType Directory -Force -Path $dist | Out-Null }
$out = Join-Path $dist 'HH.exe'
Copy-Item $exe $out -Force

$outMB = [math]::Round((Get-Item $out).Length / 1MB, 2)
Write-Host ""
Write-Host "DONE." -ForegroundColor Green
Write-Host ("  {0} ({1} MB)" -f $out, $outMB)
Write-Host ""
Write-Host "Send dist\HH.exe to your friends. That's it." -ForegroundColor Green
