# Builds and runs the balance table. See tools/balance.cpp.
$ErrorActionPreference = 'Continue'
$root = Split-Path -Parent $PSScriptRoot
$sfml = "C:\SFML-3.0.2"
$out  = "$PSScriptRoot\balancebuild"
New-Item -ItemType Directory -Force -Path $out | Out-Null

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"

$sources = @("$PSScriptRoot\balance.cpp",
             "$root\src\world\LevelDef.cpp",
             "$root\src\upgrades\UpgradeTree.cpp",
             "$root\src\upgrades\Stats.cpp") | ForEach-Object { "`"$_`"" }

$compile = "cl /nologo /std:c++20 /EHsc /permissive- /O2 /MD " +
           "/I`"$sfml\include`" /I`"$root\src`" " + ($sources -join ' ') +
           " /Fo:`"$out/`" /Fe:`"$out\balance.exe`" " +
           "/link /LIBPATH:`"$sfml\lib`" sfml-system.lib"

# Out of the way first: without this a failed compile leaves the last good
# build sitting there and the table reports numbers from before the edit.
Remove-Item "$out\balance.exe" -ErrorAction SilentlyContinue

cmd /c "`"$vcvars`" >nul 2>nul && $compile"
if (-not (Test-Path "$out\balance.exe")) { Write-Host "build failed"; exit 1 }

# The tree lays its nodes out with polar vectors, so it needs the one library.
$env:PATH = "$sfml\bin;$env:PATH"
& "$out\balance.exe"
