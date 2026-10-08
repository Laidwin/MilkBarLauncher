using Breath_of_the_Wild_Multiplayer.MVVM.Model;
using System;
using System.Windows.Input;

namespace Breath_of_the_Wild_Multiplayer.Source_files
{
    public class RelayCommand : ICommand
    {
        private Action<object> _execute;
        private Func<object, bool> _canExecute;

        // WPF re-queries CanExecute through CommandManager; Avalonia only listens to this event.
        public event EventHandler CanExecuteChanged;

        public RelayCommand(Action<object> execute, Func<object, bool> canExecute = null)
        {
            _execute = execute;
            _canExecute = canExecute;
        }

        public bool CanExecute(object parameter)
        {
            return _canExecute == null || _canExecute(parameter);
        }

        public void Execute(object parameter)
        {
            try
            {
                _execute(parameter);
            }
            catch(Exception ex)
            {
                SharedData.LoadErrorMessage(ex.Message);
            }
        }

        public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
    }
}
