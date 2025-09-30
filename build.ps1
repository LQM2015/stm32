param(
    [string]$IdeRoot = "D:\\Devtools\\ST\\STM32CubeIDE_1.19.0\\STM32CubeIDE",
    [string]$Workspace = (Join-Path -Path ([System.IO.Path]::GetTempPath()) -ChildPath "stm32cubeide_ws"),
    [string]$ProjectPath = $PSScriptRoot,
    [string]$ProjectName = "ext_burn",
    [string]$Configuration = "FlashLoader_Debug",
    [switch]$Clean,
    [switch]$VerboseLog
)

$ErrorActionPreference = 'Stop'

function Resolve-HeadlessExecutable {
    param([string]$Root)
    $candidates = @(
        (Join-Path -Path $Root -ChildPath "stm32cubeidec.exe"),
        (Join-Path -Path $Root -ChildPath "stm32cubeide.exe"),
        (Join-Path -Path $Root -ChildPath "stm32cubeidec")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }
    throw "Headless executable not found in $Root. Please verify IdeRoot."
}

if (-not (Test-Path $ProjectPath)) {
    throw "Project path $ProjectPath does not exist."
}

$projectFullPath = (Resolve-Path -Path $ProjectPath).ProviderPath

if (-not (Test-Path $Workspace)) {
    New-Item -ItemType Directory -Path $Workspace | Out-Null
}

$workspaceRoot = (Resolve-Path -Path $Workspace).ProviderPath

$headlessExe = Resolve-HeadlessExecutable -Root $IdeRoot

$workspaceName = "ws_" + (Get-Date -Format "yyyyMMddHHmmss")
$effectiveWorkspace = Join-Path -Path $workspaceRoot -ChildPath $workspaceName
New-Item -ItemType Directory -Path $effectiveWorkspace -Force | Out-Null

$tempDir = Join-Path -Path $effectiveWorkspace -ChildPath "tmp"
New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

$buildCommand = if ($Clean) { "-cleanBuild" } else { "-build" }

$args = @(
    "-nosplash",
    "--launcher.suppressErrors",
    "-application", "org.eclipse.cdt.managedbuilder.core.headlessbuild",
    "-data", $effectiveWorkspace,
    "-import", $projectFullPath,
    $buildCommand, "$ProjectName/$Configuration",
    "-consoleLog",
    "-vmargs", "-Djava.io.tmpdir=$tempDir"
)

if ($VerboseLog) {
    $args += "-debug"
}

$previousTemp = $env:TEMP
$previousTmp = $env:TMP
$previousTmpDir = $env:TMPDIR
$previousTempUser = [System.Environment]::GetEnvironmentVariable("TEMP", "User")
$previousTmpUser = [System.Environment]::GetEnvironmentVariable("TMP", "User")

Write-Host "Running STM32CubeIDE headless build"
Write-Host "  Executable : $headlessExe"
Write-Host "  Workspace  : $effectiveWorkspace"
Write-Host "  Project    : $ProjectName"
Write-Host "  Config     : $Configuration"
Write-Host "  Build flag : $buildCommand"
Write-Host "  Temp dir    : $tempDir"
Write-Host "  Original TEMP : $previousTemp"

[System.Environment]::SetEnvironmentVariable("TEMP", $tempDir, "Process")
[System.Environment]::SetEnvironmentVariable("TMP", $tempDir, "Process")
[System.Environment]::SetEnvironmentVariable("TMPDIR", $tempDir, "Process")
[System.Environment]::SetEnvironmentVariable("TEMP", $tempDir, "User")
[System.Environment]::SetEnvironmentVariable("TMP", $tempDir, "User")
$env:TEMP = $tempDir
$env:TMP = $tempDir
$env:TMPDIR = $tempDir

function ConvertTo-CommandLine {
    param([string[]]$Arguments)
    return ($Arguments | ForEach-Object {
        if ($_ -match '[\s"\\]') {
            '"{0}"' -f ($_ -replace '\\', '\\\\' -replace '"', '\"')
        } else {
            $_
        }
    }) -join ' '
}

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $headlessExe
$psi.Arguments = ConvertTo-CommandLine -Arguments $args
$psi.WorkingDirectory = $effectiveWorkspace
$psi.UseShellExecute = $false

# Copy all current environment variables first
foreach ($key in [System.Environment]::GetEnvironmentVariables().Keys) {
    $value = [System.Environment]::GetEnvironmentVariable($key)
    if ($value -ne $null) {
        $psi.EnvironmentVariables[$key] = $value
    }
}

# Override temp directory variables for GCC
$psi.EnvironmentVariables["TEMP"] = $tempDir
$psi.EnvironmentVariables["TMP"] = $tempDir
$psi.EnvironmentVariables["TMPDIR"] = $tempDir
$psi.EnvironmentVariables["TEMPDIR"] = $tempDir

try {
    $process = [System.Diagnostics.Process]::Start($psi)
    if ($process -eq $null) {
        throw "Failed to start headless build process."
    }
    $process.WaitForExit()
    $exitCode = $process.ExitCode
} finally {
    if ($process) { $process.Close() }
    if ($null -ne $previousTemp) { $env:TEMP = $previousTemp } else { Remove-Item Env:TEMP -ErrorAction SilentlyContinue }
    if ($null -ne $previousTmp) { $env:TMP = $previousTmp } else { Remove-Item Env:TMP -ErrorAction SilentlyContinue }
    if ($null -ne $previousTmpDir) { $env:TMPDIR = $previousTmpDir } else { Remove-Item Env:TMPDIR -ErrorAction SilentlyContinue }
    [System.Environment]::SetEnvironmentVariable("TEMP", $previousTempUser, "User")
    [System.Environment]::SetEnvironmentVariable("TMP", $previousTmpUser, "User")
}

if ($exitCode -ne 0) {
    throw "Build failed with exit code $exitCode"
}

Write-Host "Build completed successfully."
