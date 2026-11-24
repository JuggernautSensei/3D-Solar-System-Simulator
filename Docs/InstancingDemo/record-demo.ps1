# 앱 본편을 Release로 빌드하고, 소행성 10만 개 데모(인스턴싱 OFF -> ON)를 녹화한 뒤
# 드로우 방식 비교 벤치마크(1천/1만/10만 개)를 다시 측정합니다.
param(
    [string]$VcpkgRoot = 'C:\vcpkg'
)
$ErrorActionPreference = 'Stop'
$scriptRoot  = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $scriptRoot '..\..')).Path

$vswhere      = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -property installationPath
Import-Module (Join-Path $installation 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $installation -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null

Write-Host '== 1/3 Build SolarSystem (Release x64) =='
& msbuild (Join-Path $projectRoot 'SolarSystem.sln') /nologo /m /v:minimal /p:Configuration=Release /p:Platform=x64
if ($LASTEXITCODE -ne 0) { throw 'SolarSystem build failed.' }

Write-Host '== 2/3 Record 100,000-asteroid demo (do not touch the window, ~30 s) =='
$capture = Join-Path $scriptRoot 'Capture'
$exeDir  = Join-Path $projectRoot 'Bin\x64\Release'
Push-Location $exeDir
try {
    & (Join-Path $exeDir 'SolarSystem.exe') --demo $capture 2>&1 | Tee-Object -FilePath (Join-Path $scriptRoot 'demo-run.log')
    if ($LASTEXITCODE -ne 0) { throw "Demo failed: $LASTEXITCODE" }
} finally {
    Pop-Location
}

Write-Host '== 3/3 Draw-call benchmark (1,000 / 10,000 / 100,000) =='
& (Join-Path $projectRoot 'Docs\InstancingBenchmark\build-run.ps1') -ProjectRoot $projectRoot -VcpkgRoot $VcpkgRoot -Counts 1000,10000,100000
if ($LASTEXITCODE -ne 0) { throw 'Benchmark failed.' }

Write-Host 'ALL_DONE'
