param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [switch]$Run
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$build = Join-Path $root 'build'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'vswhere.exe was not found.' }
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio with MSVC was not found.' }
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) { throw 'Visual Studio CMake was not found.' }

& $cmake -S $root -B $build -G 'Visual Studio 18 2026' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
& $cmake --build $build --config $Configuration
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
& $cmake --build $build --config $Configuration --target RUN_TESTS
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }

if ($Run) {
    & (Join-Path $build "$Configuration\types_variables_initialization.exe")
    if ($LASTEXITCODE -ne 0) { throw "Sample failed with exit code $LASTEXITCODE." }
}
