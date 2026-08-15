[CmdletBinding()]
param(
    [string]$QtRoot = 'C:\Qt\Qt5.9.7',
    [switch]$Package
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$qtKitRoot = Join-Path $QtRoot '5.9.7\mingw53_32'
$qtBin = Join-Path $qtKitRoot 'bin'
$mingwBin = Join-Path $QtRoot 'Tools\mingw530_32\bin'
$qmake = Join-Path $qtBin 'qmake.exe'
$make = Join-Path $mingwBin 'mingw32-make.exe'
$deployQt = Join-Path $qtBin 'windeployqt.exe'

foreach ($requiredTool in @($qmake, $make)) {
    if (-not (Test-Path -LiteralPath $requiredTool)) {
        throw "Required Qt tool was not found: $requiredTool"
    }
}

$env:PATH = "$qtBin;$mingwBin;$env:PATH"

function Invoke-NativeTool {
    param(
        [string]$Executable,
        [string[]]$Arguments
    )

    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Executable failed with exit code $LASTEXITCODE"
    }
}

$appBuild = Join-Path $projectRoot 'build\app'
$testBuild = Join-Path $projectRoot 'build\tests'
New-Item -ItemType Directory -Path $appBuild, $testBuild -Force | Out-Null

Push-Location $appBuild
try {
    Invoke-NativeTool $qmake @('..\..\src\protocol_sender.pro')
    # Always rebuild from source so stale object files cannot hide header/source drift.
    Invoke-NativeTool $make @('clean')
    Invoke-NativeTool $make @('-j2')
} finally {
    Pop-Location
}

Push-Location $testBuild
try {
    Invoke-NativeTool $qmake @('..\..\tests\protocol_sender_tests.pro')
    Invoke-NativeTool $make @('clean')
    Invoke-NativeTool $make @('-j2')
} finally {
    Pop-Location
}

$testExecutable = Join-Path $testBuild 'release\protocol_sender_tests.exe'
Invoke-NativeTool $testExecutable @('-txt')

if ($Package) {
    if (-not (Test-Path -LiteralPath $deployQt)) {
        throw "Required Qt deployment tool was not found: $deployQt"
    }
    $dist = Join-Path $projectRoot 'dist'
    New-Item -ItemType Directory -Path $dist -Force | Out-Null
    $appExecutable = Join-Path $appBuild 'release\protocol_sender.exe'
    $staging = Join-Path ([System.IO.Path]::GetTempPath()) ("citel-t007-deploy-$([Guid]::NewGuid().ToString('N'))")
    New-Item -ItemType Directory -Path $staging -Force | Out-Null
    $stagingExecutable = Join-Path $staging 'protocol_sender.exe'
    Copy-Item -LiteralPath $appExecutable -Destination $stagingExecutable -Force
    Invoke-NativeTool $deployQt @('--release', '--no-translations', '--dir', $staging, $stagingExecutable)

    $requiredRuntimeFiles = @(
        'protocol_sender.exe',
        'Qt5Core.dll',
        'Qt5Network.dll',
        'Qt5Sql.dll',
        'Qt5Widgets.dll',
        'Qt5Xml.dll',
        'platforms\qwindows.dll',
        'sqldrivers\qsqlite.dll',
        'libgcc_s_dw2-1.dll',
        'libstdc++-6.dll',
        'libwinpthread-1.dll'
    )
    foreach ($relativePath in $requiredRuntimeFiles) {
        $deployedPath = Join-Path $staging $relativePath
        if (-not (Test-Path -LiteralPath $deployedPath)) {
            throw "Deployment is incomplete; missing runtime file: $relativePath"
        }
    }

    $packageDestination = $dist
    $distExecutable = Join-Path $dist 'protocol_sender.exe'
    $runningFromDist = Get-Process -Name 'protocol_sender' -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -and ([System.IO.Path]::GetFullPath($_.Path) -eq [System.IO.Path]::GetFullPath($distExecutable)) }
    if ($runningFromDist) {
        $packageDestination = Join-Path $projectRoot 'dist-candidate'
        Write-Warning "dist is currently in use by protocol_sender.exe; writing the verified package to: $packageDestination"
    }

    New-Item -ItemType Directory -Path $packageDestination -Force | Out-Null
    Get-ChildItem -LiteralPath $staging -Force |
        Copy-Item -Destination $packageDestination -Recurse -Force
    $packagedExecutable = Join-Path $packageDestination 'protocol_sender.exe'
    Invoke-NativeTool $packagedExecutable @('--smoke-test')
    Write-Output "Package ready: $packageDestination"
}

Write-Output 'Build and tests completed successfully.'
