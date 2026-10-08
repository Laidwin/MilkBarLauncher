using Breath_of_the_Wild_Multiplayer.Source_files;
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using System.Diagnostics;
using Breath_of_the_Wild_Multiplayer.MVVM.Model;
using System.Linq;
using Avalonia.Threading;

namespace Breath_of_the_Wild_Multiplayer.MVVM.ViewModel
{
    public class SettingsPanelModel : ObservableObject
    {
        private bool _enableCustomModels;

        public bool EnableCustomModels
        {
            get { return _enableCustomModels; }
            set { 
                _enableCustomModels = value;
                Properties.Settings.Default.playAsModel = value;
                Properties.Settings.Default.Save();
                OnPropertyChanged();
            }
        }

        public RelayCommand backgroundLeftButton { get; set; }
        public RelayCommand backgroundRightButton { get; set; }
        public RelayCommand restartSettings { get; set; }

        private bool _backgroundMovingLeft;

        public bool backgroundMovingLeft
        {
            get { return _backgroundMovingLeft; }
            set { 
                _backgroundMovingLeft = value;
                OnPropertyChanged();
            }
        }

        private bool _backgroundMovingRight;

        public bool backgroundMovingRight
        {
            get { return _backgroundMovingRight; }
            set { 
                _backgroundMovingRight = value;
                OnPropertyChanged();
            }
        }

        public RelayCommand discordButton { get; set; }
        public AsyncRelayCommand bcmlButton { get; set; }

        static private string BackgroundsPath = Path.Combine(AppContext.BaseDirectory, "Backgrounds");
        List<Background> Backgrounds = new List<Background>();
        int index = -1;

        public DispatcherTimer timer = new DispatcherTimer();

        public SettingsPanelModel()
        {
            SharedData.SettingsPanel = this;

            List<string> Extensions = new List<string>() { ".png", ".jpg" };

            Backgrounds.AddRange(this.FindBackgrounds(BackgroundsPath, Extensions));

            findCurrentIndex();

            backgroundMovingLeft = false;
            backgroundMovingRight = false;
            timer.Interval = TimeSpan.FromSeconds(20);
            timer.Tick += new EventHandler(timer_Tick);

            this.EnableCustomModels = Properties.Settings.Default.playAsModel;

            backgroundLeftButton = new RelayCommand(async o =>
            {
                if(index >= 0 && !backgroundMovingLeft)
                {
                    SharedData.MainMenu.changingBackground = true;
                    backgroundMovingLeft = true;
                    index--;
                    await Sleep(300);

                    if (index == -1)
                    {
                        Properties.Settings.Default.background = "Random";
                        Properties.Settings.Default.backgroundDir = "";
                        Properties.Settings.Default.backgroundExt = "";
                        setTimer(true);
                    }
                    else
                    {
                        Properties.Settings.Default.background = Backgrounds[index].Filename;
                        Properties.Settings.Default.backgroundDir = Backgrounds[index].Path;
                        Properties.Settings.Default.backgroundExt = Backgrounds[index].Extension;
                    }
                    updateBackground();
                    Properties.Settings.Default.Save();

                    await Sleep(300);
                    backgroundMovingLeft = false;
                    SharedData.MainMenu.changingBackground = false;
                }
            });

            backgroundRightButton = new RelayCommand(async o =>
            {
                if(index < Backgrounds.Count - 1 && !backgroundMovingRight)
                {
                    SharedData.MainMenu.changingBackground = true;
                    backgroundMovingRight = true;
                    index++;

                    if (index != -1)
                        setTimer(false);

                    await Sleep(300);
                    Properties.Settings.Default.background = Backgrounds[index].Filename;
                    Properties.Settings.Default.backgroundDir = Backgrounds[index].Path;
                    Properties.Settings.Default.backgroundExt = Backgrounds[index].Extension;
                    updateBackground();
                    Properties.Settings.Default.Save();

                    await Sleep(300);
                    backgroundMovingRight = false;
                    SharedData.MainMenu.changingBackground = false;
                }
            });

            restartSettings = new RelayCommand(o =>
            {
                Properties.Settings.Default.background = "Random";
                Properties.Settings.Default.playerName = "Link";
                index = 0;
                updateBackground();
                Properties.Settings.Default.Save();
            });

            discordButton = new RelayCommand(o => {
                Process.Start(new ProcessStartInfo("https://discord.gg/sqeKHhBJse") { UseShellExecute=true});
            });

            // Settings file of BCML (settings.json) or UKMM (settings.yml).
            bcmlButton = new AsyncRelayCommand(async o =>
            {
                string current = ModManagerSettings.SettingsPath;
                string fileName = await FileDialogs.OpenFile("Select the BCML or UKMM settings file", "BCML or UKMM settings", new[] { "settings.json", "settings.yml" },
                    current == null ? Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData) : Path.GetDirectoryName(current));

                if (fileName != null)
                {
                    Properties.Settings.Default.bcmlLocation = fileName;
                    Properties.Settings.Default.Save();
                }
            });
        }

        private async Task Sleep(int i)
        {
            await Task.Delay(i);
        }

        private void setTimer(bool on)
        {
            if (on)
                timer.Start();
            else
                timer.Stop();
        }

        void findCurrentIndex()
        {
            List<int> matches = Backgrounds.Select((bg, i) => new { Index = i, Value = bg })
                                                  .Where(bg => Properties.Settings.Default.background == bg.Value.Filename && Properties.Settings.Default.backgroundDir == bg.Value.Path && Properties.Settings.Default.backgroundExt == bg.Value.Extension)
                                                  .Select(bg => bg.Index).ToList();

            if(matches.Count == 0)
            {
                index = -1;
                Properties.Settings.Default.background = "Random";
                Properties.Settings.Default.backgroundDir = "";
                Properties.Settings.Default.backgroundExt = "";
                updateBackground();
                Properties.Settings.Default.Save();
                return;
            }

            index = matches.First();
        }

        void updateBackground()
        {
            if (Properties.Settings.Default.background == "Random" && string.IsNullOrEmpty(Properties.Settings.Default.backgroundDir) && string.IsNullOrEmpty(Properties.Settings.Default.backgroundExt))
            {
                string currentBackground = Properties.Settings.Default.actualBackground;
                string newBackground = currentBackground;
                int retries = 0;
                Random randomGenerator = new Random();
                
                while(currentBackground == newBackground && retries < 10)
                {
                    Background randomBackground = Backgrounds[randomGenerator.Next(Backgrounds.Count)];
                    newBackground = Path.Combine(randomBackground.Path, randomBackground.Filename + randomBackground.Extension);
                    retries++;
                }

                Properties.Settings.Default.actualBackground = newBackground;
                return;
            }

            if (!File.Exists(Path.Combine(Properties.Settings.Default.backgroundDir, Properties.Settings.Default.background + Properties.Settings.Default.backgroundExt)))
            {
                Properties.Settings.Default.background = "Random";
                Properties.Settings.Default.backgroundDir = "";
                Properties.Settings.Default.backgroundExt = "";
                Properties.Settings.Default.actualBackground = Properties.Settings.DefaultBackground;
                Properties.Settings.Default.Save();
                index = -1;
                return;
            }

            Properties.Settings.Default.actualBackground = Path.Combine(Properties.Settings.Default.backgroundDir, Properties.Settings.Default.background + Properties.Settings.Default.backgroundExt);
        }

        public IEnumerable<Background> FindBackgrounds(string path, List<string> extensions) => Directory.EnumerateFiles(path, "*.*", SearchOption.AllDirectories)
                                                                                                  .Where(file => extensions.IndexOf(Path.GetExtension(file)) >= 0)
                                                                                                  .Select(file => new Background() { Path = Path.GetDirectoryName(file), Filename = Path.GetFileNameWithoutExtension(file), Extension = Path.GetExtension(file) });

        private async void timer_Tick(object sender, EventArgs e) 
        {
            SharedData.MainMenu.changingBackground = true;
            await Sleep(300);
            updateBackground();
            SharedData.MainMenu.changingBackground = false;
        }
    }
}
