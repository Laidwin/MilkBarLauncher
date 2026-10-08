using Breath_of_the_Wild_Multiplayer.MVVM.Model;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;

namespace Breath_of_the_Wild_Multiplayer.Source_files
{
    // Starts Cemu with the mod and waits for the mod to connect to the launcher.
    public static class CemuLauncher
    {
        // libbotwm.so ships next to the launcher; BOTWM_MOD_LIBRARY overrides it (e.g. a dev build).
        public static string ModLibraryPath
        {
            get
            {
                string overridden = Environment.GetEnvironmentVariable("BOTWM_MOD_LIBRARY");
                return string.IsNullOrEmpty(overridden) ? Path.Combine(AppContext.BaseDirectory, "libbotwm.so") : overridden;
            }
        }

        public static async Task<Process> StartWithMod(string cemuExecutable, string gameDir)
        {
            // The game's executable sits in code/, next to the content/ folder mod managers point to.
            string rpxPath = Path.Combine(Path.GetDirectoryName(gameDir.TrimEnd('/', '\\'))!, "code", "U-King.rpx");

            return OperatingSystem.IsWindows() ? await StartAndInject(cemuExecutable, rpxPath) : await StartPreloaded(cemuExecutable, rpxPath);
        }

        // The WPF launcher's approach: inject InjectDLL.dll into the new Cemu process.
        private static async Task<Process> StartAndInject(string cemuExecutable, string rpxPath)
        {
            List<Process> ProcessesToFilter = Injector.GetProcesses("Cemu");

            SharedData.SetLoadingMessage("Starting Cemu...");

            await Task.Run(() => Process.Start(cemuExecutable, $"-g \"{rpxPath}\""));

            await Task.Delay(500);

            SharedData.SetLoadingMessage("Injecting Cemu...");

            Process CemuProcess = null;

            await Task.Run(() => {
                CemuProcess = Injector.Inject("Cemu", Path.Combine(AppContext.BaseDirectory, "Resources", "InjectDLL.dll"), ProcessesToFilter);
            });

            await StartPipe(CemuProcess);
            return CemuProcess;
        }

        // Linux: the mod is a shared library loaded into Cemu at startup with LD_PRELOAD, and it
        // connects to the socket given in BOTWM_LAUNCHER_SOCKET.
        private static async Task<Process> StartPreloaded(string cemuExecutable, string rpxPath)
        {
            string modLibrary = ModLibraryPath;
            if (!File.Exists(modLibrary))
                throw new ApplicationException($"Mod library not found: {modLibrary}");

            SharedData.SetLoadingMessage("Starting Cemu...");

            ProcessStartInfo start = new ProcessStartInfo(cemuExecutable) { UseShellExecute = false };
            start.ArgumentList.Add("-g");
            start.ArgumentList.Add(rpxPath);

            string preload = Environment.GetEnvironmentVariable("LD_PRELOAD");
            start.Environment["LD_PRELOAD"] = string.IsNullOrEmpty(preload) ? modLibrary : $"{modLibrary}:{preload}";
            start.Environment["BOTWM_LAUNCHER_SOCKET"] = NamedPipes.SocketPath;

            Process CemuProcess = await Task.Run(() => Process.Start(start));

            await StartPipe(CemuProcess);
            return CemuProcess;
        }

        private static async Task StartPipe(Process CemuProcess)
        {
            SharedData.SetLoadingMessage("Starting pipe...");

            try
            {
                await Task.Run(NamedPipes.StartServer);
            }
            catch (Exception)
            {
                CemuProcess.Kill();
                throw;
            }
        }
    }
}
