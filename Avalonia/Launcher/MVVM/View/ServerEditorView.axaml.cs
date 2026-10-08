using Avalonia.Controls;
using System.Linq;

namespace Breath_of_the_Wild_Multiplayer.MVVM.View
{
    public partial class ServerEditorView : UserControl
    {
        public ServerEditorView()
        {
            InitializeComponent();
        }

        // Ports are digits only (WPF rejects other typed or pasted characters).
        private void Port_TextChanged(object sender, TextChangedEventArgs e)
        {
            TextBox textBox = (TextBox)sender;
            if (textBox.Text != null && textBox.Text.Any(c => !char.IsDigit(c)))
                textBox.Text = new string(textBox.Text.Where(char.IsDigit).ToArray());
        }
    }
}
