using Avalonia.Data.Converters;
using System;
using System.Globalization;

namespace Breath_of_the_Wild_Multiplayer.Source_files
{
    // True when the bound value equals the converter parameter, e.g. to set a style class from an
    // int (WPF DataTrigger Binding="{Binding viewPosition}" Value="1").
    public class EqualsConverter : IValueConverter
    {
        public static readonly EqualsConverter Instance = new EqualsConverter();

        public object Convert(object value, Type targetType, object parameter, CultureInfo culture) =>
            value != null && parameter != null && value.ToString() == parameter.ToString();

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture) => throw new NotSupportedException();
    }
}
