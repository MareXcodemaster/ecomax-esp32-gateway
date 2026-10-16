param([string]$Core = (Join-Path $env:USERPROFILE '.platformio'))
$ErrorActionPreference = 'Stop'
$project = [IO.Path]::GetFullPath($PSScriptRoot)
if (-not (Test-Path -LiteralPath $Core)) { New-Item -ItemType Directory -Path $Core | Out-Null }
$corePath = (Resolve-Path -LiteralPath $Core).Path
$pioCommand = Get-Command platformio -ErrorAction SilentlyContinue
$pio = if ($pioCommand) { $pioCommand.Source } else { Join-Path $corePath 'penv/Scripts/platformio.exe' }
if (-not (Test-Path -LiteralPath $pio)) { throw 'Install requirements-build.txt first, then activate its environment.' }
$letters = @('Z','Y','X','W','V','U') | Where-Object { -not (Test-Path "${_}:\") } | Select-Object -First 2
if ($letters.Count -ne 2) { throw 'Two unused drive letters are required for the Windows build.' }
$projectDrive = "$($letters[0]):"
$coreDrive = "$($letters[1]):"
$shortProject = "$projectDrive/$(Split-Path -Leaf $project)"
$oldCore = $env:PLATFORMIO_CORE_DIR
$oldBuild = $env:PLATFORMIO_BUILD_DIR
$projectMapped = $false
$coreMapped = $false
try {
    & subst $projectDrive (Split-Path -Parent $project)
    if ($LASTEXITCODE -ne 0) { throw 'Project path mapping failed' }
    $projectMapped = $true
    & subst $coreDrive $corePath
    if ($LASTEXITCODE -ne 0) { throw 'PlatformIO path mapping failed' }
    $coreMapped = $true
    $env:PLATFORMIO_CORE_DIR = "$coreDrive\"
    $env:PLATFORMIO_BUILD_DIR = "$shortProject/.pio/build"
    & $pio run -d $shortProject
    if ($LASTEXITCODE -ne 0) { throw 'Firmware build failed' }
} finally {
    $env:PLATFORMIO_CORE_DIR = $oldCore
    $env:PLATFORMIO_BUILD_DIR = $oldBuild
    if ($projectMapped) { & subst $projectDrive /D }
    if ($coreMapped) { & subst $coreDrive /D }
}

