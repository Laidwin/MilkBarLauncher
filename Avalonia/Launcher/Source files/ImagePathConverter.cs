using Avalonia.Data.Converters;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;

namespace Breath_of_the_Wild_Multiplayer.Source_files
{
    // Turns the image paths the view models use into bitmaps: "/Images/..." are resources of the
    // app (WPF pack URIs in the original launcher), anything else is a file such as a background.
    public class ImagePathConverter : IValueConverter
    {
        public static readonly ImagePathConverter Instance = new ImagePathConverter();

        private static readonly Dictionary<string, Bitmap> Cache = new Dictionary<string, Bitmap>();

        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is not string path || string.IsNullOrEmpty(path))
                return null;

            if (Cache.TryGetValue(path, out Bitmap cached))
                return cached;

            Bitmap bitmap = null;
            try
            {
                string resourcePath = path.Replace('\\', '/');
                if (resourcePath.StartsWith("/Images/"))
                    bitmap = new Bitmap(AssetLoader.Open(new Uri($"avares://MilkBarLauncher{resourcePath}")));
                else if (File.Exists(path))
                    bitmap = new Bitmap(path);
            }
            catch (Exception)
            {
                // Missing picture (e.g. a model without one): show nothing, like WPF does.
            }

            Cache[path] = bitmap;
            return bitmap;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture) => throw new NotSupportedException();
    }
}
