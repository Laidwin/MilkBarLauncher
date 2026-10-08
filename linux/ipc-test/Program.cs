// Plays the launcher: the launcher's real NamedPipes.cs talks to the real libbotwm.so loaded
// into fake-cemu, checking the Linux launcher <-> mod connection end to end.
using System;
using System.Diagnostics;
using System.IO;
using Breath_of_the_Wild_Multiplayer.Source_files;

string cemuPath = args[0];
string modPath = args[1];
string socketPath = Path.Combine(Path.GetTempPath(), $"botwm-ipc-test-{Environment.ProcessId}.sock");
Environment.SetEnvironmentVariable("BOTWM_LAUNCHER_SOCKET", socketPath);

var start = new ProcessStartInfo(cemuPath);
start.Environment["LD_PRELOAD"] = modPath;
start.Environment["BOTWM_LAUNCHER_SOCKET"] = socketPath;
var cemu = Process.Start(start);

int failures = 0;
void Check(bool ok, string what)
{
    if (!ok)
        failures++;
    Console.WriteLine($"[{(ok ? "OK" : "FAIL")}] {what}");
}

try
{
    var watch = Stopwatch.StartNew();
    NamedPipes.StartServer();
    Check(NamedPipes.Online, $"mod connected after {watch.ElapsedMilliseconds} ms");
    Check(!NamedPipes.sendInstruction("!ping"), "unknown instruction answered Failed");
    Check(NamedPipes.sendInstruction("!startServerLoop"), "!startServerLoop answered Succeeded");
}
finally
{
    cemu.Kill();
    NamedPipes.Disconnect();
}
Check(!File.Exists(socketPath), "socket file removed on disconnect");

Console.WriteLine($"{failures} check(s) failed");
return failures == 0 ? 0 : 1;
