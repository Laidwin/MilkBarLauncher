using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Breath_of_the_Wild_Multiplayer.MVVM.ViewModel;
using System;
using System.IO;
using System.Linq;
using System.Reflection;

namespace Breath_of_the_Wild_Multiplayer
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            CopyAppdataFiles();

            InitializeComponent();

            // The WPF window also looks for BCML's settings here; ModManagerSettings now finds
            // BCML or UKMM settings whenever they're needed.
            MainViewModel viewModel = new MainViewModel();
            viewModel.Window = this;
            DataContext = viewModel;
        }

        protected void CloseClick(object sender, RoutedEventArgs e)
        {
            Close();
        }

        protected void MinimizeClick(object sender, RoutedEventArgs e)
        {
            WindowState = WindowState.Minimized;
        }

        protected void TitleBarDrag(object sender, PointerPressedEventArgs e)
        {
            if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
                BeginMoveDrag(e);
        }

        // Data files the mod reads (weapon damages, quest flags, armor mapping), in the folder
        // where the mod looks for them: %APPDATA%\BOTWM on Windows, ~/.config/BOTWM on Linux.
        private static void CopyAppdataFiles()
        {
            string AppdataFolder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "BOTWM");
            Assembly assembly = Assembly.GetExecutingAssembly();

            Directory.CreateDirectory(AppdataFolder);

            foreach (string resource in assembly.GetManifestResourceNames().Where(resource => resource.Contains("AppdataFiles")))
            {
                using Stream source = assembly.GetManifestResourceStream(resource);
                using FileStream AppdataFile = new FileStream(Path.Combine(AppdataFolder, resource.Replace("Breath_of_the_Wild_Multiplayer.AppdataFiles.", "")), FileMode.Create);
                source.CopyTo(AppdataFile);
            }
        }
    }
}
