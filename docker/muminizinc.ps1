<#
.SYNOPSIS
    Runs MuMiniZinc.
#>

$ErrorActionPreference = "Stop"

$ImageName = "muminizinc:latest"
$CurrentDir = (Get-Location).Path

# Only build the image if it does not exist or if instructed to do so
# with `--rebuild-docker-image`.
$ForceRebuild = $false
$PassthroughArgs = @()

foreach ($arg in $args) {
    if ($arg -eq "--rebuild-docker-image") {
        $ForceRebuild = $true
    } else {
        $PassthroughArgs += $arg
    }
}

$ImageId = (docker images -q "$ImageName" 2>$null)

if ($ForceRebuild -or -not $ImageId) {
    $ScriptDir = $PSScriptRoot
    $ProjectRoot = Split-Path -Parent $ScriptDir

    docker build -t "$ImageName" "$ProjectRoot"
}

# Check if the input is a terminal.
if ([Console]::IsInputRedirected) {
    $DockerFlags = "-i"
} else {
    $DockerFlags = "-it"
}

# Check if the output is a terminal.
# This value can be overriden by the user.
if ([Console]::IsOutputRedirected) {
    $ColorArgs = @("-c", "false")
} else {
    $ColorArgs = @("-c", "true")
}

docker run --rm $DockerFlags `
  -v "${CurrentDir}:/workspace" `
  -w /workspace `
  "$ImageName" `
  $ColorArgs $PassthroughArgs