# Builds and runs tests/tests.cpp. No test framework and no second Visual
# Studio project: the tests only touch the table-driven parts of the game, so
# they need four translation units and sfml-system for its vector type.
#
#   powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1
#
# Exits non-zero if anything failed, so it can be wired to a hook or a CI step.

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sfml = if ($env:SFML_DIR) { $env:SFML_DIR } else { "C:\SFML-3.0.2" }
$outDir = Join-Path $root "x64\Tests"

if (-not (Test-Path "$sfml\include\SFML")) {
    Write-Host "SFML not found at $sfml. Set SFML_DIR." -ForegroundColor Red
    exit 1
}

# Find the compiler the same way Visual Studio does, rather than assuming a path.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Host "vswhere not found; is Visual Studio installed?" -ForegroundColor Red
    exit 1
}

$vsPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    Write-Host "no Visual Studio with the C++ tools found" -ForegroundColor Red
    exit 1
}

$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
    Write-Host "vcvars64.bat not found under $vsPath" -ForegroundColor Red
    exit 1
}

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$sources = @(
    "tests\tests.cpp",
    "src\upgrades\Stats.cpp",
    "src\upgrades\UpgradeTree.cpp",
    "src\world\LevelDef.cpp",
    "src\world\Objective.cpp",
    "src\ui\UpgradeIcons.cpp",
    "src\core\Random.cpp"
) | ForEach-Object { "`"$root\$_`"" }

$exe = Join-Path $outDir "tests.exe"

# One cmd invocation: vcvars only sets its environment for the process it runs in.
$compile = "cl /nologo /std:c++20 /EHsc /permissive- /W4 /MD /O2 " +
           "/I`"$sfml\include`" /I`"$root\src`" " +
           ($sources -join ' ') +
           " /Fo:`"$outDir\\`" /Fe:`"$exe`" " +
           "/link /LIBPATH:`"$sfml\lib`" sfml-graphics.lib sfml-window.lib sfml-system.lib"

Write-Host "building tests"
$build = & cmd /c "`"$vcvars`" >nul 2>&1 && $compile" 2>&1
$buildCode = $LASTEXITCODE

if ($buildCode -ne 0) {
    $build | ForEach-Object { Write-Host $_ }
    Write-Host "build failed" -ForegroundColor Red
    exit 1
}

# The DLLs have to sit next to the exe, the same way the game's do. The icons
# are built out of sf::VertexArray, which is a graphics type, so the tests need
# more than sfml-system even though they never open a window.
foreach ($dll in @('sfml-graphics-3.dll', 'sfml-window-3.dll', 'sfml-system-3.dll')) {
    Copy-Item "$sfml\bin\$dll" $outDir -Force -ErrorAction SilentlyContinue
}

Write-Host ""
& $exe
$code = $LASTEXITCODE

Write-Host ""
if ($code -eq 0) { Write-Host "tests passed" -ForegroundColor Green }
else { Write-Host "tests FAILED" -ForegroundColor Red }

exit $code
