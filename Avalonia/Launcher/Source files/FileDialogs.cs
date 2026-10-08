using Avalonia.Platform.Storage;
using Breath_of_the_Wild_Multiplayer.MVVM.Model;
using System.IO;
using System.Threading.Tasks;

namespace Breath_of_the_Wild_Multiplayer.Source_files
{
    // Avalonia's file picker, in place of WPF's Microsoft.Win32.OpenFileDialog.
    public static class FileDialogs
    {
        // Local path of the chosen file, or null if the dialog was cancelled.
        public static async Task<string> OpenFile(string title, string filterName, string[] patterns, string startDirectory = null)
        {
            IStorageProvider storage = SharedData.MainView.Window.StorageProvider;

            FilePickerOpenOptions options = new FilePickerOpenOptions
            {
                Title = title,
                AllowMultiple = false,
                FileTypeFilter = new[] { new FilePickerFileType(filterName) { Patterns = patterns } },
            };
            if (!string.IsNullOrEmpty(startDirectory) && Directory.Exists(startDirectory))
                options.SuggestedStartLocation = await storage.TryGetFolderFromPathAsync(startDirectory);

            var files = await storage.OpenFilePickerAsync(options);
            return files.Count > 0 ? files[0].TryGetLocalPath() : null;
        }
    }
}
