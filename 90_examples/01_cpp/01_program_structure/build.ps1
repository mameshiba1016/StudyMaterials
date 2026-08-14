param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [switch]$Run
)

# Keep this script ASCII-compatible so Windows PowerShell 5 can parse it
# even when the repository stores text files as UTF-8 without a BOM.
$ErrorActionPreference = 'Stop'
$sampleRoot = $PSScriptRoot
$buildDirectory = Join-Path $sampleRoot 'build'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'

if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'vswhere.exe was not found. Install Visual Studio with Desktop development with C++.'
}

$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) {
    throw 'Visual Studio with the MSVC C++ toolchain was not found.'
}

$cmake = Join-Path $visualStudio 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) {
    throw "Visual Studio CMake was not found: $cmake"
}

# -S selects the source tree and -B keeps generated files outside that tree.
& $cmake -S $sampleRoot -B $buildDirectory -G 'Visual Studio 18 2026' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }

& $cmake --build $buildDirectory --config $Configuration
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }

& $cmake --build $buildDirectory --config $Configuration --target RUN_TESTS
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }

if ($Run) {
    $executable = Join-Path $buildDirectory "$Configuration\program_structure.exe"
    & $executable
    if ($LASTEXITCODE -ne 0) { throw "The sample returned exit code $LASTEXITCODE." }
}
