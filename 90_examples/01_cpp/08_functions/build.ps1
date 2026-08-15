param([ValidateSet('Debug','Release')][string]$Configuration='Debug',[switch]$Run)
$ErrorActionPreference='Stop';$r=$PSScriptRoot;$b=Join-Path $r 'build';$v=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if(-not(Test-Path $v)){throw 'vswhere.exe was not found.'};$vs=& $v -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $vs){throw 'Visual Studio with MSVC was not found.'};$c=Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $c -S $r -B $b -G 'Visual Studio 18 2026' -A x64;if($LASTEXITCODE-ne 0){throw 'Configure failed.'}
& $c --build $b --config $Configuration;if($LASTEXITCODE-ne 0){throw 'Build failed.'}
& $c --build $b --config $Configuration --target RUN_TESTS;if($LASTEXITCODE-ne 0){throw 'Tests failed.'}
if($Run){& (Join-Path $b "$Configuration\functions.exe");if($LASTEXITCODE-ne 0){throw 'Run failed.'}}
