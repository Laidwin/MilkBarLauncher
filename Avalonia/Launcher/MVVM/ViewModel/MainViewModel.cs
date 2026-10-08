using Avalonia.Controls;
using Avalonia.Media;
using Breath_of_the_Wild_Multiplayer.MVVM.Model;
using Breath_of_the_Wild_Multiplayer.Source_files;
using System;
using System.IO;

namespace Breath_of_the_Wild_Multiplayer.MVVM.ViewModel
{
    public class MainViewModel : ObservableObject
    {
        public MainMenuModel MainMenuVM { get; set; }
        public IngameMenuModel IngameMenuVM { get; set; }

        private bool _disableBackground;

        public bool disableBackground
        {
            get { return _disableBackground; }
            set {
                _disableBackground = value;
                OnPropertyChanged();
            }
        }

        private string _appTitle;

        public string appTitle
        {
            get { return _appTitle; }
            set {
                _appTitle = value;
                OnPropertyChanged();
            }
        }

        public string Version;

        private object _currentView;

        public object currentView
        {
            get { return _currentView; }
            set
            {
                _currentView = value;
                OnPropertyChanged();
            }
        }

        private bool _isTopView;

        public bool isTopView
        {
            get { return _isTopView; }
            set {
                _isTopView = value;
                OnPropertyChanged();
            }
        }

        private object _currentTopView;

        public object currentTopView
        {
            get { return _currentTopView; }
            set {
                _currentTopView = value;
                OnPropertyChanged();
            }
        }

        private IBrush _barColor;

        public IBrush barColor
        {
            get { return _barColor; }
            set {
                _barColor = value;
                OnPropertyChanged();
            }
        }

        public Window Window { get; set; }

        public RelayCommand changeTopView { get; set; }

        public MainViewModel()
        {
            SharedData.MainView = this;
            SharedData.LoadingMessage = new LoadingModel();
            this.MainMenuVM = new MainMenuModel();
            this.IngameMenuVM = new IngameMenuModel();
            this.disableBackground = false;

            currentView = this.MainMenuVM;

            // The WPF launcher also checks GitHub for updates here, through a Windows-only updater.
            string versionFile = Path.Combine(AppContext.BaseDirectory, "Version.txt");
            if (File.Exists(versionFile))
            {
                this.Version = File.ReadAllText(versionFile);
                this.appTitle = $"Breath of the Wild Multiplayer v. {this.Version}";
            }

            currentTopView = null;
            isTopView = false;

            barColor = new SolidColorBrush(Color.FromArgb(255, 63, 63, 63));

            changeTopView = new RelayCommand(o => this.updateTopView(o));

            if (Properties.Settings.Default.playerName == "Link")
                this.updateTopView(new ChangeNameModel());
        }

        public void updateTopView(object topViewData)
        {
            this.isTopView = true;
            this.currentTopView = topViewData;
        }

        public void closeTopView()
        {
            this.isTopView = false;
            this.currentTopView = null;
        }
    }
}
