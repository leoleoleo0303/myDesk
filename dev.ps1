<#
.SYNOPSIS
    myDesk dev script: auto configure + build + run.

.DESCRIPTION
    Auto-detects the CMake bundled with Visual Studio and the vcpkg toolchain,
    configures on first run, then does incremental builds afterwards.
    `build` action now auto-launches the GUI after successful build.

.PARAMETER Action
    configure  Generate / refresh the CMake project only
    build      (default) configure-if-needed + build + auto-launch GUI
    run        build then run the given app
    rebuild    wipe the build dir, then configure + build + auto-launch
    headless   build then run mydesk in headless mode

.PARAMETER App
    App to run: capture_demo / codec_demo / host / viewer / signal_server / mydesk

.PARAMETER AppArgs
    Arguments passed to the app (just append them after the app name)

.PARAMETER P2P
    Enable WebRTC P2P (needs extra deps; off by default)

.PARAMETER NoLaunch
    Skip auto-launch after build (for CI or scripted usage)

.EXAMPLE
    .\dev.ps1                              # build + auto-launch mydesk GUI
    .\dev.ps1 -NoLaunch                    # build only, don't launch
    .\dev.ps1 run capture_demo shot.ppm
    .\dev.ps1 run host 9000
    .\dev.ps1 run viewer 127.0.0.1 9000
    .\dev.ps1 headless                     # build + run headless service
    .\dev.ps1 rebuild

.NOTES
    To pass app args that start with '-' (e.g. host's own --p2p), use the
    PowerShell stop-parsing token --% :
        .\dev.ps1 run host --% --p2p 1.2.3.4 9000 myid
#>
[CmdletBinding()]
param(
    [ValidateSet('configure', 'build', 'run', 'rebuild', 'headless')]
    [string]$Action = 'build',

    [string]$App,

    [switch]$P2P,

    [switch]$NoLaunch,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AppArgs
)

$ErrorActionPreference = 'Stop'

# Script dir == project root
$Root      = $PSScriptRoot
$BuildDir  = Join-Path $Root 'build'
$Config    = 'Release'
$BinDir    = Join-Path $BuildDir "bin\$Config"

# ---- 1. Locate vcpkg toolchain ----
$VcpkgRoot = $env:VCPKG_ROOT
if (-not $VcpkgRoot) {
    foreach ($cand in @('D:\vcpkg', 'C:\vcpkg', "$env:USERPROFILE\vcpkg")) {
        if (Test-Path (Join-Path $cand 'vcpkg.exe')) { $VcpkgRoot = $cand; break }
    }
}
if (-not $VcpkgRoot -or -not (Test-Path (Join-Path $VcpkgRoot 'vcpkg.exe'))) {
    throw "vcpkg not found. Set the VCPKG_ROOT env var to your vcpkg dir."
}
$Toolchain = Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'

# ---- 2. Locate CMake (prefer the one bundled with VS) ----
$CMakeExe = $null
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmake) {
    $CMakeExe = $cmake.Source
} else {
    $pf86 = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
    $vswhere = Join-Path $pf86 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -property installationPath
        $candidate = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
        if (Test-Path $candidate) { $CMakeExe = $candidate }
    }
}
if (-not $CMakeExe) {
    throw "CMake not found. Install Visual Studio (with CMake) or CMake itself."
}

function Set-BuildEnvironment {
    $normalizedPath = [System.Environment]::GetEnvironmentVariable('Path', 'Process')
    if (-not $normalizedPath) {
        $normalizedPath = [System.Environment]::GetEnvironmentVariable('PATH', 'Process')
    }

    [System.Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
    [System.Environment]::SetEnvironmentVariable('Path', $null, 'Process')
    if ($normalizedPath) {
        [System.Environment]::SetEnvironmentVariable('Path', $normalizedPath, 'Process')
    }

    [System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', $VcpkgRoot, 'Process')
    [System.Environment]::SetEnvironmentVariable('VCPkgLocalAppDataDisabled', '1', 'Process')
}

function Get-ConfiguredP2PState {
    $cache = Join-Path $BuildDir 'CMakeCache.txt'
    if (-not (Test-Path $cache)) { return $null }

    $entry = Select-String -Path $cache -Pattern '^RD_ENABLE_P2P(:\w+)?=(.*)$' -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $entry) { return $null }

    return $entry.Matches[0].Groups[2].Value.Trim()
}

function Invoke-Configure {
    Set-BuildEnvironment
    $p2pFlag = if ($P2P) { 'ON' } else { 'OFF' }
    Write-Host ">> Configuring CMake (P2P=$p2pFlag)..." -ForegroundColor Cyan
    & $CMakeExe -S $Root -B $BuildDir -G 'Visual Studio 17 2022' -A x64 `
        "-DCMAKE_TOOLCHAIN_FILE=$Toolchain" `
        '-DVCPKG_TARGET_TRIPLET=x64-windows' `
        "-DRD_ENABLE_P2P=$p2pFlag"
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
}

function Invoke-Build {
    Set-BuildEnvironment

    $expectedP2P = if ($P2P) { 'ON' } else { 'OFF' }
    $cache = Join-Path $BuildDir 'CMakeCache.txt'
    if (-not (Test-Path $cache) -or (Get-ConfiguredP2PState) -ne $expectedP2P) {
        Invoke-Configure
    }

    Write-Host ">> Building ($Config)..." -ForegroundColor Cyan
    & $CMakeExe --build $BuildDir --config $Config -j
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
    Write-Host ">> Build done. Output: $BinDir" -ForegroundColor Green
}

function Invoke-LaunchGUI {
    $exe = Join-Path $BinDir "mydesk.exe"
    if (-not (Test-Path $exe)) {
        Write-Host ">> mydesk.exe not found, skipping auto-launch" -ForegroundColor Yellow
        return
    }
    Write-Host ">> Launching myDesk GUI..." -ForegroundColor Cyan
    Start-Process -FilePath $exe
}

switch ($Action) {
    'configure' { Invoke-Configure }
    'build' {
        Invoke-Build
        if (-not $NoLaunch) { Invoke-LaunchGUI }
    }
    'rebuild' {
        if (Test-Path $BuildDir) { Remove-Item $BuildDir -Recurse -Force }
        Invoke-Build
        if (-not $NoLaunch) { Invoke-LaunchGUI }
    }
    'headless' {
        Invoke-Build
        $exe = Join-Path $BinDir "mydesk.exe"
        if (-not (Test-Path $exe)) { throw "mydesk.exe not found" }
        Write-Host ">> Running myDesk in headless mode..." -ForegroundColor Cyan
        & $exe --headless @AppArgs
    }
    'run' {
        if (-not $App) { throw "run needs an app name, e.g.: .\dev.ps1 run capture_demo" }
        Invoke-Build
        $exe = Join-Path $BinDir "$App.exe"
        if (-not (Test-Path $exe)) { throw "Executable not found: $exe" }
        Write-Host ">> Running $App $AppArgs" -ForegroundColor Cyan
        & $exe @AppArgs
    }
}
