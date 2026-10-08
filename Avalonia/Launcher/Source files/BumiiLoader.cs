using Breath_of_the_Wild_Multiplayer.MVVM.Model.DTO;
using Newtonsoft.Json;
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;

namespace Breath_of_the_Wild_Multiplayer.Source_files
{
    public static class BumiiLoader
    {
        public static async Task<Tuple<string, BumiiDTO?>> readBumii()
        {
            string fileName = await FileDialogs.OpenFile("Select a bumii file", "Bumii files", new[] { "*.bumii", "*.sbactorpack", "*.yml" }, Directory.GetCurrentDirectory());

            BumiiDTO? bumii = null;
            string path = "";

            if (fileName != null)
            {
                string cmdOutput = runCMD(fileName);

                try
                {
                    bumii = JsonConvert.DeserializeObject<BumiiDTO>(cmdOutput);
                    path = fileName;
                }
                catch
                {
                    throw new Exception("Error reading bumii file. Make sure the file is properly formatted.");
                }
            }

            return new Tuple<string, BumiiDTO?>(path, bumii);
        }

        // bumii_IO.exe is a Windows program shipped without sources; elsewhere it runs through Wine.
        private static string runCMD(string bumiiPath)
        {
            string tool = Path.Combine(AppContext.BaseDirectory, "Resources", "bumii_IO.exe");

            ProcessStartInfo start = new ProcessStartInfo()
            {
                FileName = OperatingSystem.IsWindows() ? tool : "wine",
                UseShellExecute = false,
                RedirectStandardOutput = true,
                CreateNoWindow = true
            };
            if (!OperatingSystem.IsWindows())
                start.ArgumentList.Add(tool);
            start.ArgumentList.Add(bumiiPath);

            Process cmd;
            try
            {
                cmd = Process.Start(start);
            }
            catch (Win32Exception) when (!OperatingSystem.IsWindows())
            {
                throw new Exception("Importing bumii files uses bumii_IO.exe, a Windows program. Install Wine to use it on this system.");
            }

            string output = cmd.StandardOutput.ReadToEnd();

            cmd.WaitForExit();

            return output;
        }
    }
}
