param(
    [ValidateSet('Start', 'Run', 'Stop', 'Status')][string]$Action = 'Start'
)
$ErrorActionPreference = 'Stop'
$receiverPath = Join-Path $PSScriptRoot 'audio-share-mic.exe'
$settingsPath = Join-Path $PSScriptRoot 'microphone-settings.json'
$stopPath = Join-Path $PSScriptRoot 'microphone.stop'
$pidPath = Join-Path $PSScriptRoot 'supervisor.pid'
$statusPath = Join-Path $PSScriptRoot 'microphone-status.json'
$logPath = Join-Path $PSScriptRoot 'supervisor.log'
$scriptPath = $PSCommandPath
$pathHash = [BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes($PSScriptRoot.ToLowerInvariant()))).Replace('-', '').Substring(0, 20)

function Write-Log([string]$Message) {
    Add-Content -LiteralPath $logPath -Encoding UTF8 -Value ((Get-Date -Format o) + ' ' + $Message)
}

function Get-Supervisor {
    if (-not (Test-Path -LiteralPath $pidPath)) { return $null }
    $processIdText = (Get-Content -LiteralPath $pidPath -Raw).Trim()
    if ($processIdText -notmatch '^\d+$') { return $null }
    $processInfo = Get-CimInstance Win32_Process -Filter ('ProcessId=' + $processIdText) -ErrorAction SilentlyContinue
    if ($processInfo -and $processInfo.CommandLine -and $processInfo.CommandLine.Contains($scriptPath) -and $processInfo.CommandLine -match '-Action Run') {
        return $processInfo
    }
    return $null
}

if ($Action -ne 'Run') {
    $controlMutex = New-Object Threading.Mutex($false, ('Local\AudioShareMicrophone-Control-' + $pathHash))
    $ownsControl = $false
    try {
        try { $ownsControl = $controlMutex.WaitOne(35000) } catch [Threading.AbandonedMutexException] { $ownsControl = $true }
        if (-not $ownsControl) { throw 'Another microphone start/stop command is still in progress.' }
if ($Action -eq 'Status') {
    $supervisor = Get-Supervisor
    [PSCustomObject]@{ SupervisorRunning = [bool]$supervisor; SupervisorId = $supervisor.ProcessId; StopRequested = (Test-Path -LiteralPath $stopPath) }
    if (Test-Path -LiteralPath $statusPath) { Get-Content -LiteralPath $statusPath }
    exit 0
}

if ($Action -eq 'Stop') {
    Set-Content -LiteralPath $stopPath -Value 'stop' -Encoding ASCII
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ((Get-Supervisor) -and $timer.Elapsed.TotalSeconds -lt 30) { Start-Sleep -Milliseconds 250 }
    if (Get-Supervisor) { throw 'Receiver shutdown is still in progress. Check supervisor.log.' }
    Write-Output 'Microphone receiver stopped. The speaker server is unchanged.'
    exit 0
}

if ($Action -eq 'Start') {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ((Get-Supervisor) -and (Test-Path -LiteralPath $stopPath) -and $timer.Elapsed.TotalSeconds -lt 30) { Start-Sleep -Milliseconds 250 }
    if ((Get-Supervisor) -and (Test-Path -LiteralPath $stopPath)) { throw 'Receiver shutdown is still in progress; Start has not been applied.' }
    if (Get-Supervisor) { Write-Output 'Microphone receiver is already running.'; exit 0 }
    if (-not (Test-Path -LiteralPath $settingsPath)) { throw "Missing $settingsPath" }
    if (-not (Test-Path -LiteralPath $receiverPath)) { throw "Missing $receiverPath" }
    if (Test-Path -LiteralPath $stopPath) { Remove-Item -LiteralPath $stopPath -Force }
    $powershellPath = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + $scriptPath + '" -Action Run'
    Start-Process -FilePath $powershellPath -ArgumentList $arguments -WorkingDirectory $PSScriptRoot -WindowStyle Hidden | Out-Null
    $timer.Restart()
    while (-not (Get-Supervisor) -and $timer.Elapsed.TotalSeconds -lt 5) { Start-Sleep -Milliseconds 250 }
    if (-not (Get-Supervisor)) {
        Set-Content -LiteralPath $stopPath -Value 'stop' -Encoding ASCII
        throw 'Receiver supervisor did not start. Check supervisor.log.'
    }
    Write-Output 'Microphone receiver supervisor started.'
    exit 0
}
    } finally {
        if ($ownsControl) { $controlMutex.ReleaseMutex() }
        $controlMutex.Dispose()
    }
}

$mutex = New-Object Threading.Mutex($false, ('Local\AudioShareMicrophone-' + $pathHash))
$ownsMutex = $false
$receiver = $null
try {
    try { $ownsMutex = $mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $ownsMutex = $true }
    if (-not $ownsMutex) { exit 0 }
    if (Test-Path -LiteralPath $stopPath) { exit 0 }
    if ((Test-Path -LiteralPath $logPath) -and (Get-Item -LiteralPath $logPath).Length -gt 1048576) {
        Copy-Item -LiteralPath $logPath -Destination ($logPath + '.old') -Force
        Clear-Content -LiteralPath $logPath
    }
    $settings = Get-Content -LiteralPath $settingsPath -Raw | ConvertFrom-Json
    $serial = [string]$settings.Serial
    if ($serial -notmatch '^[a-zA-Z0-9._:-]+$') { throw 'Set Serial to the USB phone identifier from adb devices.' }
    $adbPath = [string]$settings.AdbPath
    if (-not [IO.Path]::IsPathRooted($adbPath)) { $adbPath = Join-Path $PSScriptRoot $adbPath }
    $adbPath = [IO.Path]::GetFullPath($adbPath)
    if (-not (Test-Path -LiteralPath $adbPath)) { throw "ADB not found: $adbPath" }
    $deviceName = [string]$settings.Device
    if (-not $deviceName) { $deviceName = 'CABLE Input' }
    if ($deviceName.Contains('"') -or $deviceName.Contains("`r") -or $deviceName.Contains("`n")) { throw 'Invalid device name.' }
    if (-not (Test-Path -LiteralPath $receiverPath)) { throw "Receiver not found: $receiverPath" }
    Set-Content -LiteralPath $pidPath -Value $PID -Encoding ASCII

    function Invoke-Adb([string[]]$Arguments, [switch]$UsbOnly) {
        foreach ($argument in $Arguments) { if ($argument -match '[\s"`;&|<>]') { throw 'Invalid ADB argument' } }
        $client = New-Object Diagnostics.Process
        $client.StartInfo.FileName = $adbPath
        $selector = if ($UsbOnly) { '-d ' } else { '-s ' + $serial + ' ' }
        $client.StartInfo.Arguments = $selector + ($Arguments -join ' ')
        $client.StartInfo.UseShellExecute = $false
        $client.StartInfo.CreateNoWindow = $true
        $client.StartInfo.RedirectStandardOutput = $true
        $client.StartInfo.RedirectStandardError = $true
        try {
            [void]$client.Start()
            $outTask = $client.StandardOutput.ReadToEndAsync()
            $errTask = $client.StandardError.ReadToEndAsync()
            if (-not $client.WaitForExit(4000)) { $client.Kill(); throw 'ADB client timed out.' }
            return [PSCustomObject]@{ Code = $client.ExitCode; Out = $outTask.GetAwaiter().GetResult(); Err = $errTask.GetAwaiter().GetResult() }
        } finally { $client.Dispose() }
    }
    Write-Log 'START'
    $lastLinkState = ''
    while (-not (Test-Path -LiteralPath $stopPath)) {
        if (-not $receiver -or $receiver.HasExited) {
            if ($receiver) { Write-Log ('Receiver exited: ' + $receiver.ExitCode); $receiver.Dispose() }
            $receiverArguments = '--listen 127.0.0.1:65529 --device "' + $deviceName + '" --status-file "' + $statusPath + '" --stop-file "' + $stopPath + '"'
            $receiver = Start-Process -FilePath $receiverPath -ArgumentList $receiverArguments -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -RedirectStandardOutput (Join-Path $PSScriptRoot 'receiver.stdout.log') -RedirectStandardError (Join-Path $PSScriptRoot 'receiver.stderr.log') -PassThru
            Write-Log ('Receiver PID=' + $receiver.Id)
        }
        try {
            $state = Invoke-Adb @('get-serialno') -UsbOnly
            if ($state.Code -ne 0 -or $state.Out.Trim() -ne $serial) {
                $linkState = 'USB phone unavailable; waiting'
            } else {
                $reverse = Invoke-Adb @('reverse', '--list')
                if ($reverse.Code -ne 0) { throw $reverse.Err }
                $existing = @($reverse.Out -split "`n" | Where-Object { $_ -match '\s+tcp:65529\s+' })
                if ($existing.Count -gt 0 -and ($existing -join "`n") -notmatch '\s+tcp:65529\s+tcp:65529\s*$') { throw 'USB port 65529 already maps to another destination.' }
                if ($existing.Count -eq 0) {
                    $mapped = Invoke-Adb @('reverse', '--no-rebind', 'tcp:65529', 'tcp:65529')
                    if ($mapped.Code -ne 0) { throw $mapped.Err }
                }
                $linkState = 'USB microphone forwarding ready'
            }
        } catch { $linkState = 'USB reconnect: ' + $_.Exception.Message }
        if ($linkState -ne $lastLinkState) { Write-Log $linkState; $lastLinkState = $linkState }
        Start-Sleep -Seconds 2
    }
} catch {
    Write-Log ('ERROR ' + $_.Exception.Message)
    exit 1
} finally {
    if ($ownsMutex) {
        if ($receiver) {
            Set-Content -LiteralPath $stopPath -Value 'stop' -Encoding ASCII
            if (-not $receiver.HasExited -and -not $receiver.WaitForExit(3000)) { $receiver.Kill() }
            $receiver.Dispose()
        }
        # ADB offers no ownership token for a reverse mapping. Leave it in place
        # rather than removing a mapping another process may have recreated.
        # No audio can flow after the local receiver closes; USB detach clears it.
        if (Test-Path -LiteralPath $pidPath) { Remove-Item -LiteralPath $pidPath -Force }
        Write-Log 'STOP'
        $mutex.ReleaseMutex()
    }
    $mutex.Dispose()
}
