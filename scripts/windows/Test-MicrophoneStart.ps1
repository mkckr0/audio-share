$ErrorActionPreference = 'Stop'
$fixtureName = 'Audio Share ' + [guid]::NewGuid()
$fixture = New-Item -ItemType Directory -Path (Join-Path ([IO.Path]::GetTempPath()) $fixtureName)
$launcher = Join-Path $fixture.FullName 'Microphone.ps1'

try {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Microphone.ps1') -Destination $launcher
    $settings = '{"Serial":"fixture","AdbPath":"audio-share-mic.exe"}'
    Set-Content -LiteralPath (Join-Path $fixture.FullName 'microphone-settings.json') -Encoding ASCII -Value $settings
    $stub = Join-Path $fixture.FullName 'audio-share-mic.exe'
    Add-Type -OutputType ConsoleApplication -OutputAssembly $stub -TypeDefinition @'
using System;
using System.Diagnostics;
using System.IO;
using System.Threading;

class Fixture {
    public static void Main(string[] args) {
        if (Array.IndexOf(args, "get-serialno") >= 0) Console.WriteLine("fixture");
        int stop = Array.IndexOf(args, "--stop-file");
        if (stop < 0) return;
        File.WriteAllText("receiver.pid.tmp", Process.GetCurrentProcess().Id.ToString());
        File.Move("receiver.pid.tmp", "receiver.pid");
        while (!File.Exists(args[stop + 1])) Thread.Sleep(50);
    }
}
'@

    & $launcher -Action Start
    $initial = & $launcher -Action Status
    $receiverPidPath = Join-Path $fixture.FullName 'receiver.pid'
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while (-not (Test-Path -LiteralPath $receiverPidPath) -and $timer.Elapsed.TotalSeconds -lt 5) {
        Start-Sleep -Milliseconds 100
    }
    $receiver = Get-Process -Id ([int](Get-Content -LiteralPath $receiverPidPath))

    $alternateLauncher = Join-Path $fixture.FullName.ToLowerInvariant() 'Microphone.ps1'
    & $alternateLauncher -Action Start
    $current = & $alternateLauncher -Action Status
    if (-not $current.SupervisorRunning -or $current.SupervisorId -ne $initial.SupervisorId -or
        $current.StopRequested -or $receiver.HasExited) {
        throw 'Starting again with different directory casing must preserve the supervisor and receiver.'
    }
} finally {
    if (Test-Path -LiteralPath $launcher) { & $launcher -Action Stop | Out-Null }
    foreach ($file in $fixture.GetFiles()) { Remove-Item -LiteralPath $file.FullName }
    Remove-Item -LiteralPath $fixture.FullName
}
