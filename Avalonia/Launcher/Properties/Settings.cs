using Breath_of_the_Wild_Multiplayer.Source_files;
using Newtonsoft.Json;
using System;
using System.IO;

namespace Breath_of_the_Wild_Multiplayer.Properties
{
    // Stands in for the WPF launcher's Properties.Settings (same names and defaults), which only
    // exists on .NET for Windows. Stored as JSON next to the mod's files: %APPDATA%\BOTWM on
    // Windows, ~/.config/BOTWM on Linux.
    public class Settings : ObservableObject
    {
        public const string DefaultBackground = "/Images/mainWindowBackground.png";

        private static readonly string FilePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "BOTWM", "LauncherSettings.json");

        public static Settings Default { get; } = Load();

        private string _playerName = "Link";
        public string playerName { get => _playerName; set { _playerName = value; OnPropertyChanged(); } }

        private string _background = "Random";
        public string background { get => _background; set { _background = value; OnPropertyChanged(); } }

        private string _actualBackground = DefaultBackground;
        public string actualBackground { get => _actualBackground; set { _actualBackground = value; OnPropertyChanged(); } }

        private string _serversAdded = "[]";
        public string serversAdded { get => _serversAdded; set { _serversAdded = value; OnPropertyChanged(); } }

        private string _backgroundDir = "";
        public string backgroundDir { get => _backgroundDir; set { _backgroundDir = value; OnPropertyChanged(); } }

        private string _backgroundExt = "";
        public string backgroundExt { get => _backgroundExt; set { _backgroundExt = value; OnPropertyChanged(); } }

        // Settings file of the mod manager: BCML's settings.json or UKMM's settings.yml.
        private string _bcmlLocation = "";
        public string bcmlLocation { get => _bcmlLocation; set { _bcmlLocation = value; OnPropertyChanged(); } }

        private string _playerModel = "{\"Name\":\"Link\",\"Description\":\"This model does allow for armor sync.\",\"IsArmorSync\":true,\"Model\":\"Jugador1ModelNameLongForASpecificReason\",\"BumiiData\":null,\"AlternativeModel\":\"\",\"BustPic\":\"/Images/Bust/Link.png\",\"BodyPic\":\"/Images/Body/Link.png\",\"ModelIndex\":0,\"HasCustomAction\":false,\"Selected\":true,\"CustomAction\":null}";
        public string playerModel { get => _playerModel; set { _playerModel = value; OnPropertyChanged(); } }

        private bool _playAsModel = false;
        public bool playAsModel { get => _playAsModel; set { _playAsModel = value; OnPropertyChanged(); } }

        public void Save()
        {
            Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);
            File.WriteAllText(FilePath, JsonConvert.SerializeObject(this, Formatting.Indented));
        }

        private static Settings Load()
        {
            try
            {
                if (File.Exists(FilePath))
                    return JsonConvert.DeserializeObject<Settings>(File.ReadAllText(FilePath)) ?? new Settings();
            }
            catch (JsonException)
            {
                // A corrupt file falls back to the defaults, like a reset of the WPF settings.
            }

            return new Settings();
        }
    }
}
