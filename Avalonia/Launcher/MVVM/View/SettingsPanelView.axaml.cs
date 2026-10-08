using Avalonia.Controls;

namespace Breath_of_the_Wild_Multiplayer.MVVM.View
{
    public partial class SettingsPanelView : UserControl
    {
        public SettingsPanelView()
        {
            InitializeComponent();
        }

        // Shared with ChangeNameView: an empty name restores the saved one, anything else is saved.
        public static void PlayerNameChanged(TextBox textBox)
        {
            if (textBox.Text == "")
            {
                textBox.Text = Properties.Settings.Default.playerName;
            }
            else if (textBox.Text != Properties.Settings.Default.playerName)
            {
                Properties.Settings.Default.playerName = textBox.Text;
                Properties.Settings.Default.Save();
            }
        }

        private void PlayerName_TextChanged(object sender, TextChangedEventArgs e) => PlayerNameChanged((TextBox)sender);
    }
}
