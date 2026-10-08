using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Xml;

namespace Breath_of_the_Wild_Multiplayer.Source_files
{
    // Reads which graphic packs are enabled in Cemu, to check the ones the mod needs before
    // starting a game.
    public static class CemuSettings
    {
        // Required packs, identified by the `path` in their rules.txt, plus a file path fallback
        // for entries whose rules.txt can't be found (the WPF launcher's check, BCML layout).
        private static readonly (string Label, string RulesPath, string FileSuffix)[] RequiredPacks =
        {
            ("  · Utilities", "The Legend of Zelda: Breath of the Wild/Multiplayer/Utilities", "bcmlPatches/MilkBarLauncher/rules.txt"),
            ("  · Extended Memory", "The Legend of Zelda: Breath of the Wild/Mods/Extended Memory", "BreathOfTheWild/Mods/ExtendedMemory/rules.txt"),
        };

        // Labels of the required graphic packs that aren't enabled.
        public static List<string> MissingGraphicPacks(string cemuDir)
        {
            string settingsPath = FindSettingsFile(cemuDir);
            if (settingsPath == null)
                throw new ApplicationException("Cemu settings file doesn't exist. Make sure to have open cemu and setup the graphic packs");

            List<string> entries;
            try
            {
                XmlDocument doc = new XmlDocument();
                doc.Load(settingsPath);
                entries = doc.DocumentElement.SelectNodes("/content/GraphicPack/Entry").Cast<XmlNode>()
                             .Select(node => node.Attributes?["filename"]?.Value)
                             .Where(file => !string.IsNullOrEmpty(file))
                             .ToList();
            }
            catch (Exception)
            {
                throw new ApplicationException("Failed to read cemu graphic packs settings. Please make sure that your cemu is setup correctly.");
            }

            List<string> searchDirs = new List<string> { Path.GetDirectoryName(settingsPath), cemuDir, DataDirectory() }
                                        .Where(dir => !string.IsNullOrEmpty(dir)).Distinct().ToList();

            List<string> rulesPaths = new List<string>();
            List<string> files = new List<string>();
            foreach (string entry in entries)
            {
                string file = entry.Replace('\\', '/');
                files.Add(file);

                string rules = searchDirs.Select(dir => Path.IsPathRooted(file) ? file : Path.Combine(dir, file))
                                         .FirstOrDefault(File.Exists);
                if (rules != null)
                    rulesPaths.Add(ReadRulesPath(rules));
            }

            return RequiredPacks.Where(pack => !rulesPaths.Contains(pack.RulesPath) && !files.Any(file => file.EndsWith(pack.FileSuffix)))
                                .Select(pack => pack.Label)
                                .ToList();
        }

        // Cemu keeps settings.xml next to the executable in portable mode (and on Cemu 1.x),
        // otherwise in the user config folder.
        private static string FindSettingsFile(string cemuDir)
        {
            List<string> candidates = new List<string>();
            if (!string.IsNullOrEmpty(cemuDir))
            {
                candidates.Add(Path.Combine(cemuDir, "portable", "settings.xml"));
                candidates.Add(Path.Combine(cemuDir, "settings.xml"));
            }
            candidates.Add(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Cemu", "settings.xml"));

            return candidates.FirstOrDefault(File.Exists);
        }

        // Where Cemu keeps graphicPacks outside portable mode: ~/.local/share/Cemu on Linux.
        private static string DataDirectory()
        {
            if (OperatingSystem.IsWindows())
                return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Cemu");

            string dataHome = Environment.GetEnvironmentVariable("XDG_DATA_HOME");
            if (string.IsNullOrEmpty(dataHome))
                dataHome = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".local", "share");
            return Path.Combine(dataHome, "Cemu");
        }

        private static string ReadRulesPath(string rulesFile)
        {
            foreach (string line in File.ReadLines(rulesFile))
            {
                Match match = Regex.Match(line, @"^\s*path\s*=\s*""?([^""]*)""?\s*$");
                if (match.Success)
                    return match.Groups[1].Value.Trim();
            }
            return null;
        }
    }
}
