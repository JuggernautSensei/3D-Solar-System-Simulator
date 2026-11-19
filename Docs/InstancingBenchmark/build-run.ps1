param(
    [Parameter(Mandatory=$true)][string]$ProjectRoot,
    [string]$VcpkgRoot = 'C:\vcpkg',
    [string]$BuildDirectory = '',
    [int[]]$Counts = @(1000, 10000, 100000),
    [string]$Mesh = 'lowpoly'
)
$ErrorActionPreference = 'Stop'
$scriptRoot = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $scriptRoot 'Build' }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -property installationPath
Import-Module (Join-Path $installation 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $installation -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$deps = Join-Path $VcpkgRoot 'installed\x64-windows'
New-Item -ItemType Directory -Path $BuildDirectory -Force | Out-Null
$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
Copy-Item -LiteralPath (Join-Path $deps 'bin\glew32.dll') -Destination $BuildDirectory
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'Resources') -Destination $BuildDirectory -Recurse -Force
# Remove previous measurement records (replaced by the new runs).
$oldRecords = @('frames.jsonl', 'phases.jsonl', 'validation.jsonl', 'summary.json', 'measurement-report.md',
                'environment.json', 'run.log', 'overview-individual.png', 'overview-instanced.png',
                'belt-individual.png', 'belt-instanced.png')
foreach ($name in $oldRecords) {
    $path = Join-Path $scriptRoot $name
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
}
$results = Join-Path $scriptRoot 'Results'
if (Test-Path $results) { Remove-Item -LiteralPath $results -Recurse -Force }
New-Item -ItemType Directory -Path $results -Force | Out-Null
Push-Location $BuildDirectory
try {
    & cl.exe /nologo /std:c++20 /O2 /DNDEBUG /EHsc /MD /utf-8 /W4 /permissive- "/I$(Join-Path $deps 'include')" "/Fe:$BuildDirectory\benchmark.exe" (Join-Path $scriptRoot 'benchmark.cpp') /link "/LIBPATH:$(Join-Path $deps 'lib')" glew32.lib opengl32.lib user32.lib gdi32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Benchmark build failed.' }
    foreach ($count in $Counts) {
        $output = Join-Path $results "$Mesh-$count"
        New-Item -ItemType Directory -Path $output -Force | Out-Null
        & "$BuildDirectory\benchmark.exe" $output $count $Mesh 2>&1 | Tee-Object -FilePath (Join-Path $output 'run.log')
        if ($LASTEXITCODE -ne 0) { throw "Benchmark failed for $count asteroids: $LASTEXITCODE" }
    }
    $gpu = Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion
    $cpu = (Get-CimInstance Win32_Processor | Select-Object -First 1).Name
    [ordered]@{
        cpu = $cpu
        gpus = $gpu
        msvc_tools_version = $env:VCToolsVersion
        measured_at = (Get-Date).ToString('yyyy-MM-dd HH:mm K')
        benchmark_sha256 = (Get-FileHash -Algorithm SHA256 (Join-Path $scriptRoot 'benchmark.cpp')).Hash
        counts = $Counts
        mesh = $Mesh
    } | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 (Join-Path $results 'environment.json')
} finally {
    Pop-Location
}
Write-Host 'ALL_BENCHMARKS_COMPLETE'
