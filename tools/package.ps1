# Builds Release and puts everything a stranger needs into one zip: the exe,
# the SFML runtime DLLs and the assets. Nothing else - the save file is written
# next to the exe on first run and must not ship with somebody else's progress
# in it.
#
#   powershell -ExecutionPolicy Bypass -File tools\package.ps1

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sfml = if ($env:SFML_DIR) { $env:SFML_DIR } else { "C:\SFML-3.0.2" }
$stage = Join-Path $root "dist\Nerith"
$zip = Join-Path $root "dist\Nerith.zip"

$msbuild = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $msbuild)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild `
            -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
    }
}
if (-not $msbuild -or -not (Test-Path $msbuild)) {
    Write-Host "MSBuild not found" -ForegroundColor Red
    exit 1
}

Write-Host "building Release"
& $msbuild "$root\Nerith.vcxproj" /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { Write-Host "build failed" -ForegroundColor Red; exit 1 }

$exe = Join-Path $root "x64\Release\Nerith.exe"
if (-not (Test-Path $exe)) { Write-Host "no exe at $exe" -ForegroundColor Red; exit 1 }

Write-Host "staging"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

Copy-Item $exe $stage
Copy-Item (Join-Path $root "assets") $stage -Recurse
Copy-Item (Join-Path $root "README.md") $stage -ErrorAction SilentlyContinue

# Only the four SFML modules the game actually links. SFML 3 links its audio
# backend in rather than loading OpenAL at runtime, so there is no fifth DLL.
# Shipping the debug builds would double the size and refuse to start on a
# machine without the debug runtime.
$dlls = @('sfml-graphics-3.dll', 'sfml-window-3.dll', 'sfml-audio-3.dll',
          'sfml-system-3.dll')

foreach ($dll in $dlls) {
    $src = Join-Path $sfml "bin\$dll"
    if (Test-Path $src) { Copy-Item $src $stage }
    else { Write-Host "  missing $dll" -ForegroundColor Yellow }
}

Write-Host "zipping"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $stage -DestinationPath $zip

$size = [math]::Round((Get-Item $zip).Length / 1MB, 1)
Write-Host ""
Write-Host "dist\Nerith.zip  ($size MB)" -ForegroundColor Green
Get-ChildItem $stage | ForEach-Object { Write-Host ("  " + $_.Name) }
