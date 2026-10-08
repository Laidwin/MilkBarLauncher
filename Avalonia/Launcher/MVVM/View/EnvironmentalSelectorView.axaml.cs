using Avalonia.Controls;

namespace Breath_of_the_Wild_Multiplayer.MVVM.View
{
    public partial class EnvironmentalSelectorView : UserControl
    {
        public EnvironmentalSelectorView()
        {
            InitializeComponent();
        }

        private void ScrollViewer_ScrollChanged(object sender, ScrollChangedEventArgs e) => ServerBrowserView.UpdateScrollFade((ScrollViewer)sender);
    }
}
