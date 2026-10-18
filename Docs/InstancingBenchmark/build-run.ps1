param(
    [Parameter(Mandatory=$true)][string]$ProjectRoot,
    [string]$VcpkgRoot = 'C:\vcpkg',
    [string]$BuildDirectory = (Join-Path $PSScriptRoot 'Build')
)
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -property installationPath
Import-Module (Join-Path $installation 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $installation -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$deps = Join-Path $VcpkgRoot 'installed\x64-windows'
New-Item -ItemType Directory -Path $BuildDirectory -Force | Out-Null
$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
Copy-Item -LiteralPath (Join-Path $deps 'bin\glew32.dll') -Destination $BuildDirectory
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'Resources') -Destination $BuildDirectory -Recurse -Force
Push-Location $BuildDirectory
try {
    & cl.exe /nologo /std:c++20 /O2 /DNDEBUG /EHsc /MD /utf-8 /W4 /permissive- "/I$(Join-Path $deps 'include')" "/Fe:$BuildDirectory\benchmark.exe" (Join-Path $PSScriptRoot 'benchmark.cpp') /link "/LIBPATH:$(Join-Path $deps 'lib')" glew32.lib opengl32.lib user32.lib gdi32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Benchmark build failed.' }
    & "$BuildDirectory\benchmark.exe" $PSScriptRoot 2>&1 | Tee-Object -FilePath (Join-Path $PSScriptRoot 'run.log')
    if ($LASTEXITCODE -ne 0) { throw "Benchmark failed: $LASTEXITCODE" }
} finally {
    Pop-Location
}
