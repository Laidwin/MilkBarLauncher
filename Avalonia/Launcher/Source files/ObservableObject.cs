using Avalonia.Threading;
using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace Breath_of_the_Wild_Multiplayer.Source_files
{
    public class ObservableObject : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler PropertyChanged;

        // Models update from background tasks (server pings, loading). WPF marshals those
        // notifications to the UI thread itself; Avalonia doesn't, so do it here.
        public void OnPropertyChanged([CallerMemberName] string propertyName = null)
        {
            if (Dispatcher.UIThread.CheckAccess())
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
            else
                Dispatcher.UIThread.Post(() => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName)));
        }
    }
}
