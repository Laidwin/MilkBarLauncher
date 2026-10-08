using Avalonia.Controls;

namespace Breath_of_the_Wild_Multiplayer.MVVM.View
{
    public partial class ChangeNameView : UserControl
    {
        public ChangeNameView()
        {
            InitializeComponent();
        }

        private void PlayerName_TextChanged(object sender, TextChangedEventArgs e) => SettingsPanelView.PlayerNameChanged((TextBox)sender);
    }
}
