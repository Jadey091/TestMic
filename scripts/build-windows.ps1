param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',
    [string]$BuildDir = '',
    [ValidateRange(1, 64)]
    [int]$Jobs = 2,
    [switch]$RequireCubism,
    [switch]$Clean
)

$ErrorActionPreference = 'Continue'
[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
$canonicalPath = $env:Path
Remove-Item Env:PATH -ErrorAction SilentlyContinue
$env:Path = $canonicalPath
$env:VSLANG = '1033'
$env:MSBUILDDISABLENODEREUSE = '1'
$root = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
if (-not $BuildDir) { $BuildDir = Join-Path $root 'build-cubism' }
if (-not [IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $root $BuildDir
}
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
$esc = [char]27
$pink = '38;2;247;125;170'
$muted = '38;2;80;80;80'
$barWidth = 40

function Write-BuildProgress {
    param([int]$Percent, [string]$Message, [switch]$NewLine)
    $Percent = [Math]::Max(0, [Math]::Min(100, $Percent))
    $filled = [int][Math]::Floor($Percent * $script:barWidth / 100)
    $empty = $script:barWidth - $filled
    $fillText = '#' * $filled
    $emptyText = '.' * $empty
    $line = "`r${script:esc}[1m${script:esc}[$script:pink" +
        "m[$($Percent.ToString().PadLeft(3))%]${script:esc}[0m " +
        "${script:esc}[$script:pink" + "m$fillText" +
        "${script:esc}[$script:muted" + "m$emptyText${script:esc}[0m $Message"
    if ($NewLine) { Write-Host $line } else { Write-Host -NoNewline $line }
}

function Show-FailureLog {
    param([string[]]$Paths)
    Write-Host ''
    foreach ($path in $Paths) {
        if (-not (Test-Path -LiteralPath $path)) { continue }
        Get-Content -LiteralPath $path -Tail 30 | ForEach-Object { Write-Host $_ }
    }
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host 'Error: CMake was not found in PATH.'
    exit 1
}

if ($Clean -and (Test-Path -LiteralPath $BuildDir)) {
    $rootPrefix = $root.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $insideRoot = $BuildDir.StartsWith($rootPrefix,
        [StringComparison]::OrdinalIgnoreCase)
    if (-not $insideRoot -or (Split-Path $BuildDir -Leaf) -notlike 'build*') {
        Write-Host "Refusing to clean unexpected directory: $BuildDir"
        exit 1
    }
    Write-BuildProgress 2 'Cleaning previous build...'
    Remove-Item -LiteralPath $BuildDir -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$configureLog = Join-Path $BuildDir 'cmake_config.log'
$buildLog = Join-Path $BuildDir 'build.log'
$start = [DateTime]::UtcNow

Write-Host ''
Write-Host "BongoCat $Configuration build"
Write-BuildProgress 5 'Configuring project...'
$configureArgs = @(
    '-S', $root, '-B', $BuildDir,
    '-G', 'Visual Studio 17 2022', '-A', 'x64',
    '-DBONGO_CAT_WARNINGS_AS_ERRORS=ON'
)
if ($RequireCubism) { $configureArgs += '-DBONGO_CAT_REQUIRE_CUBISM=ON' }
$configureWriter = New-Object IO.StreamWriter(
    $configureLog, $false, (New-Object Text.UTF8Encoding($false)))
$configureActivity = 0
$configurePercent = 5
try {
    & cmake @configureArgs 2>&1 | ForEach-Object {
        $configureWriter.WriteLine($_.ToString())
        $configureActivity++
        $nextPercent = [Math]::Min(19,
            5 + [int][Math]::Floor([Math]::Sqrt($configureActivity)))
        if ($nextPercent -ne $configurePercent) {
            $configurePercent = $nextPercent
            Write-BuildProgress $configurePercent 'Configuring project...'
        }
    }
    $configureStatus = $LASTEXITCODE
} finally {
    $configureWriter.Dispose()
}
if ($configureStatus -ne 0) {
    Write-BuildProgress 5 'Configuration failed.' -NewLine
    Show-FailureLog @($configureLog)
    Write-Host "Full log: $configureLog"
    exit 1
}
Write-BuildProgress 20 'Configuration complete.' -NewLine

Remove-Item -LiteralPath $buildLog -Force -ErrorAction SilentlyContinue
$projects = @(Get-ChildItem -LiteralPath $BuildDir -Recurse -Filter '*.vcxproj' `
    -ErrorAction SilentlyContinue)
$compileItems = 0
foreach ($project in $projects) {
    $compileItems += @(Select-String -LiteralPath $project.FullName `
        -SimpleMatch '<ClCompile Include=' -ErrorAction SilentlyContinue).Count
}
$compileItems = [Math]::Max(1, $compileItems)
$buildArgs = @('--build', $BuildDir, '--config', $Configuration,
    '--target', 'bongo_cat', '--parallel', $Jobs)
$lastPercent = 20
$compiled = 0
$activity = 0
Write-BuildProgress $lastPercent 'Building BongoCat...'
$buildWriter = New-Object IO.StreamWriter(
    $buildLog, $false, (New-Object Text.UTF8Encoding($false)))
try {
    & cmake @buildArgs 2>&1 | ForEach-Object {
        $line = $_.ToString()
        $buildWriter.WriteLine($line)
        $activity++
        if ($line -match '\.(c|cc|cpp|cxx)(\s|$)') { $compiled++ }
        $compilePercent = 20 + [int][Math]::Floor(
            [Math]::Min(1.0, $compiled / [double]$compileItems) * 75)
        $activityPercent = 20 + [int][Math]::Min(72,
            [Math]::Floor([Math]::Sqrt($activity) * 4))
        $percent = [Math]::Min(95,
            [Math]::Max($lastPercent,
                [Math]::Max($compilePercent, $activityPercent)))
        if ($percent -ne $lastPercent) {
            $lastPercent = $percent
            $message = if ($compiled -gt 0) {
                "Compiling ($compiled files)..."
            } else { 'Building BongoCat...' }
            Write-BuildProgress $lastPercent $message
        }
    }
    $buildStatus = $LASTEXITCODE
} finally {
    $buildWriter.Dispose()
}

if ($buildStatus -ne 0) {
    Write-BuildProgress $lastPercent 'Build failed.' -NewLine
    Show-FailureLog @($buildLog)
    Write-Host "Full log: $buildLog"
    exit $buildStatus
}

$output = Join-Path (Join-Path $BuildDir $Configuration) 'BongoCat.exe'
if (-not (Test-Path -LiteralPath $output)) {
    Write-BuildProgress 95 'BongoCat.exe was not produced.' -NewLine
    exit 1
}
$elapsedTotal = [DateTime]::UtcNow - $start
Write-BuildProgress 100 'Build complete.' -NewLine
Write-Host ("Build time: {0:mm\:ss}" -f $elapsedTotal)
Write-Host "Output: $output"
Write-Host "Logs: $buildLog"
exit 0
