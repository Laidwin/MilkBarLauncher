using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.RegularExpressions;

namespace Breath_of_the_Wild_Multiplayer.Source_files
{
    // Where the game, Cemu and the merged mods are, read from the mod manager's settings. Replaces
    // the WPF launcher's BcmlSettings: BCML (settings.json) as before, plus UKMM (settings.yml),
    // BCML's successor and the usual choice on Linux.
    public static class ModManagerSettings
    {
        public enum ManagerKind { None, Bcml, Ukmm }

        // The settings file the user picked, else the first one found in the default locations.
        public static string SettingsPath
        {
            get
            {
                string chosen = Properties.Settings.Default.bcmlLocation;
                if (!string.IsNullOrEmpty(chosen) && File.Exists(chosen))
                    return chosen;

                foreach (string candidate in DefaultSettingsPaths())
                    if (File.Exists(candidate))
                        return candidate;

                return null;
            }
        }

        public static ManagerKind Kind
        {
            get
            {
                string path = SettingsPath;
                if (path == null)
                    return ManagerKind.None;
                return Path.GetExtension(path).Equals(".yml", StringComparison.OrdinalIgnoreCase) ? ManagerKind.Ukmm : ManagerKind.Bcml;
            }
        }

        public static string ManagerName => Kind == ManagerKind.Ukmm ? "UKMM" : "BCML";

        public static string GameDir => Kind == ManagerKind.Ukmm ? UkmmValue("content_dir") : BcmlValue("game_dir");
        public static string UpdateDir => Kind == ManagerKind.Ukmm ? UkmmValue("update_dir") : BcmlValue("update_dir");

        // Folder of the merged mods, which the launcher edits before each game.
        public static string MergedDir
        {
            get
            {
                if (Kind == ManagerKind.Bcml)
                {
                    string store = BcmlValue("store_dir");
                    return store == null ? null : Path.Combine(store, "merged", "content");
                }

                // UKMM deploys to <output>/content, or <output>/BreathOfTheWild_UKMM/content with
                // the "WithName" layout (uk-manager DeployConfig::final_output_paths).
                string output = UkmmValue("output");
                if (output == null)
                    return null;
                string withName = Path.Combine(output, "BreathOfTheWild_UKMM", "content");
                string withoutName = Path.Combine(output, "content");
                if (UkmmValue("layout") == "WithName" || (!Directory.Exists(withoutName) && Directory.Exists(withName)))
                    return withName;
                return withoutName;
            }
        }

        public static string CemuExecutable
        {
            get
            {
                if (Kind == ManagerKind.Ukmm)
                    return UkmmValue("executable");

                string cemuDir = BcmlValue("cemu_dir");
                if (cemuDir == null)
                    return null;
                if (OperatingSystem.IsWindows())
                    return Path.Combine(cemuDir, "cemu.exe");
                string linuxBinary = Path.Combine(cemuDir, "Cemu");
                return File.Exists(linuxBinary) ? linuxBinary : Path.Combine(cemuDir, "cemu");
            }
        }

        public static string CemuDir
        {
            get
            {
                string executable = CemuExecutable;
                return string.IsNullOrEmpty(executable) ? null : Path.GetDirectoryName(executable);
            }
        }

        public static bool IsSetup => !string.IsNullOrEmpty(GameDir) && !string.IsNullOrEmpty(CemuExecutable);

        private static IEnumerable<string> DefaultSettingsPaths()
        {
            string config = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData); // ~/.config on Linux
            string local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);

            yield return Path.Combine(config, "ukmm", "settings.yml");
            yield return Path.Combine(OperatingSystem.IsWindows() ? local : config, "bcml", "settings.json");
        }

        private static string BcmlValue(string setting)
        {
            string path = SettingsPath;
            if (path == null)
                return null;

            Dictionary<string, object> settings = JsonConvert.DeserializeObject<Dictionary<string, object>>(File.ReadAllText(path));
            return settings != null && settings.TryGetValue(setting, out object value) ? value?.ToString() : null;
        }

        // UKMM's settings.yml holds the Wii U values under `wiiu_config`, partly through serde's
        // polymorphic serialization (the dump source). Rather than depend on that exact shape, take
        // the first `key: value` (block or inline style) inside that section.
        private static string UkmmValue(string key)
        {
            string path = SettingsPath;
            if (path == null)
                return null;

            string section = WiiUSection(File.ReadAllLines(path));
            Match match = Regex.Match(section, $@"(?<![\w-]){Regex.Escape(key)}\s*:\s*(?:""((?:[^""\\]|\\.)*)""|'((?:[^']|'')*)'|([^,}}\r\n#]*))");
            if (!match.Success)
                return null;

            string value = match.Groups[1].Success ? Regex.Unescape(match.Groups[1].Value)
                         : match.Groups[2].Success ? match.Groups[2].Value.Replace("''", "'")
                         : match.Groups[3].Value.Trim();

            return value.Length == 0 || value == "~" || value == "null" ? null : value;
        }

        private static string WiiUSection(string[] lines)
        {
            List<string> section = new List<string>();
            bool inSection = false;

            foreach (string line in lines)
            {
                bool topLevel = line.Length > 0 && !char.IsWhiteSpace(line[0]) && !line.StartsWith("#");
                if (topLevel)
                {
                    inSection = line.StartsWith("wiiu_config:");
                    if (inSection)
                        section.Add(line.Substring("wiiu_config:".Length));
                    continue;
                }
                if (inSection)
                    section.Add(line);
            }

            return string.Join("\n", section);
        }
    }
}
