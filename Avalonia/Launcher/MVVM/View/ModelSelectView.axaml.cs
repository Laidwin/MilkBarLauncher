using Avalonia.Controls;

namespace Breath_of_the_Wild_Multiplayer.MVVM.View
{
    public partial class ModelSelectView : UserControl
    {
        public ModelSelectView()
        {
            InitializeComponent();
        }

        private void ScrollViewer_ScrollChanged(object sender, ScrollChangedEventArgs e) => ServerBrowserView.UpdateScrollFade((ScrollViewer)sender);
    }
}
