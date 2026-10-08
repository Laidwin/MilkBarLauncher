using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Breath_of_the_Wild_Multiplayer.MVVM.Model;
using Breath_of_the_Wild_Multiplayer.MVVM.ViewModel;

namespace Breath_of_the_Wild_Multiplayer.MVVM.View
{
    public partial class ServerBrowserView : UserControl
    {
        public ServerBrowserView()
        {
            InitializeComponent();
        }

        // Fades the edges of the list that have more servers to scroll to (WPF: scrollState).
        public static void UpdateScrollFade(ScrollViewer scroll)
        {
            double offset = scroll.Offset.Y;
            double scrollable = scroll.ScrollBarMaximum.Y;
            bool atTop = offset <= 0;
            bool atBottom = offset >= scrollable;

            scroll.Classes.Set("top", atTop && !atBottom);
            scroll.Classes.Set("middle", !atTop && !atBottom);
            scroll.Classes.Set("bottom", !atTop && atBottom);
        }

        private void ScrollViewer_ScrollChanged(object sender, ScrollChangedEventArgs e) => UpdateScrollFade((ScrollViewer)sender);

        // Right-click selects the server too, before its context menu opens.
        private void Server_PointerPressed(object sender, PointerPressedEventArgs e)
        {
            if (e.GetCurrentPoint((Control)sender).Properties.IsRightButtonPressed && sender is Button { DataContext: serverDataModel server })
                ((ServerBrowserModel)this.DataContext).serverButtonClick.Execute(server.serverIndex);
        }

        private void EditClick(object sender, RoutedEventArgs e)
        {
            ((ServerBrowserModel)this.DataContext).EditServer();
        }

        private void RemoveClick(object sender, RoutedEventArgs e)
        {
            ((ServerBrowserModel)this.DataContext).RemoveServer();
        }
    }
}
