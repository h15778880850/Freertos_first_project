param(
    [ValidateSet("Debug", "Release")]
    [string]$Preset = "Debug",

    [switch]$ConfigureOnly,
    [switch]$BuildOnly,
    [switch]$PersistUserPath
)

$ErrorActionPreference = "Stop"

function Find-FirstFile {
    param(
        [string[]]$Roots,
        [string]$Filter
    )

    foreach ($root in $Roots) {
        if ([string]::IsNullOrWhiteSpace($root)) {
            continue
        }

        try {
            if (-not (Test-Path -LiteralPath $root -ErrorAction Stop)) {
                continue
            }

            $match = Get-ChildItem -LiteralPath $root -Recurse -Filter $Filter -File -ErrorAction SilentlyContinue |
                Select-Object -First 1
        }
        catch {
            continue
        }

        if ($null -ne $match) {
            return $match.FullName
        }
    }

    return $null
}

function Add-PathOnce {
    param([string]$Directory)

    if ([string]::IsNullOrWhiteSpace($Directory)) {
        return
    }

    $parts = $env:PATH -split ';' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    if ($parts -notcontains $Directory) {
        $env:PATH = "$Directory;$env:PATH"
    }
}

function Add-UserPathOnce {
    param([string[]]$Directories)

    $current = [Environment]::GetEnvironmentVariable("Path", "User")
    $parts = @()
    if (-not [string]::IsNullOrWhiteSpace($current)) {
        $parts = $current -split ';' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    }

    $changed = $false
    foreach ($directory in $Directories) {
        if ([string]::IsNullOrWhiteSpace($directory)) {
            continue
        }

        if ($parts -notcontains $directory) {
            $parts += $directory
            $changed = $true
        }
    }

    if ($changed) {
        [Environment]::SetEnvironmentVariable("Path", ($parts -join ';'), "User")
        Write-Host "Updated user PATH. Open a new PowerShell window to use it globally."
    }
}

$projectRoot = Split-Path -Parent $PSScriptRoot
Set-Location $projectRoot

$candidateRoots = @(
    $env:CUBE_BUNDLE_PATH,
    "$env:LOCALAPPDATA\stm32cube\bundles",
    "$env:LOCALAPPDATA\STMicroelectronics",
    "C:\ST",
    "C:\Program Files\STMicroelectronics",
    "C:\Program Files (x86)\STMicroelectronics"
)

$cmake = Get-Command cmake.exe -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source
if ([string]::IsNullOrWhiteSpace($cmake)) {
    $cmake = Get-Command cube-cmake -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source
}
$ninja = Get-Command ninja.exe -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source
$gcc = Get-Command arm-none-eabi-gcc.exe -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source

if ([string]::IsNullOrWhiteSpace($cmake)) {
    $cmake = Find-FirstFile -Roots $candidateRoots -Filter "cmake.exe"
}

if ([string]::IsNullOrWhiteSpace($ninja)) {
    $ninja = Find-FirstFile -Roots $candidateRoots -Filter "ninja.exe"
}

if ([string]::IsNullOrWhiteSpace($gcc)) {
    $gcc = Find-FirstFile -Roots $candidateRoots -Filter "arm-none-eabi-gcc.exe"
}

if ([string]::IsNullOrWhiteSpace($cmake) -or
    [string]::IsNullOrWhiteSpace($ninja) -or
    [string]::IsNullOrWhiteSpace($gcc)) {
    Write-Host "Tool discovery failed."
    Write-Host "cmake: $cmake"
    Write-Host "ninja: $ninja"
    Write-Host "arm-none-eabi-gcc: $gcc"
    Write-Host ""
    Write-Host "Install STM32CubeCLT or make sure CUBE_BUNDLE_PATH points to the STM32Cube bundles directory."
    exit 1
}

$cmakeDir = Split-Path -Parent $cmake
$ninjaDir = Split-Path -Parent $ninja
$gccDir = Split-Path -Parent $gcc

Add-PathOnce $cmakeDir
Add-PathOnce $ninjaDir
Add-PathOnce $gccDir

if ($PersistUserPath) {
    Add-UserPathOnce -Directories @($cmakeDir, $ninjaDir, $gccDir)
}

Write-Host "Using cmake: $cmake"
Write-Host "Using ninja: $ninja"
Write-Host "Using gcc:   $gcc"
Write-Host ""

if (-not $BuildOnly) {
    & $cmake --preset $Preset
}

if (-not $ConfigureOnly) {
    & $cmake --build --preset $Preset
}
