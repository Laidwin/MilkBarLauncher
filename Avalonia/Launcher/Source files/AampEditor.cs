using Aamp.Security.Cryptography;
using Nintendo.Aamp;
using System;
using System.Linq;

namespace Breath_of_the_Wild_Multiplayer.Source_files
{
    // C# version of Resources/aapmLib.py, which the WPF launcher runs as a separate Windows
    // executable. Applies the same instructions to an AAMP file, e.g.
    //   l(param_root).l(ModelData).l(ModelData_0).o(Base).v(Folder,Link)
    // l(name) enters a list, o(name) an object, v(name,value) sets a parameter; several
    // instructions are separated by ';'.
    public static class AampEditor
    {
        public static byte[] Apply(byte[] data, string instructions)
        {
            AampFile file = AampFile.FromBinary(data);

            foreach (string instruction in instructions.Split(';'))
            {
                object current = file;

                foreach (string action in instruction.Split('.'))
                {
                    if (action.StartsWith("l"))
                        current = current is AampFile root ? Root(root, Parameter(action)) : ((ParamList)current).Lists(Parameter(action));
                    else if (action.StartsWith("o"))
                        current = ((ParamList)current).Objects(Parameter(action));
                    else if (action.StartsWith("v"))
                    {
                        string[] values = Parameter(action).Split(',');
                        SetString((ParamObject)current, values[0], values[1]);
                    }

                    if (current == null)
                        throw new Exception($"AAMP path not found: {instruction}");
                }
            }

            return file.ToBinary();
        }

        public static byte[] Apply(string hexData, string instructions) =>
            Apply(hexData.Split(' ', StringSplitOptions.RemoveEmptyEntries).Select(h => Convert.ToByte(h, 16)).ToArray(), instructions);

        // Text between the first '(' and the first ')', like getParam in aapmLib.py.
        private static string Parameter(string action)
        {
            string afterOpen = action.Split('(')[1];
            return afterOpen.Split(')')[0];
        }

        // In the Python library the file is the root and contains param_root; here the file's
        // RootNode is param_root itself.
        private static ParamList Root(AampFile file, string name) =>
            file.RootNode.Hash == Crc32.Compute(name) ? file.RootNode : null;

        // The Python aamp library writes a plain str as a StringRef parameter, adding it if needed.
        private static void SetString(ParamObject obj, string name, string value)
        {
            ParamEntry entry = obj.Params(name);
            if (entry == null)
            {
                entry = new ParamEntry { Hash = Crc32.Compute(name) };
                obj.ParamEntries = (obj.ParamEntries ?? Array.Empty<ParamEntry>()).Append(entry).ToArray();
            }

            entry.ParamType = ParamType.StringRef;
            entry.Value = new StringEntry(value);
        }
    }
}
