using System;
using System.Diagnostics;
using System.IO;
using System.IO.Pipes;
using System.Net.Sockets;
using System.Text;
using System.Threading;

namespace Breath_of_the_Wild_Multiplayer.Source_files
{
    // Connection with the mod inside Cemu. On Windows it's a message-mode named pipe. .NET doesn't
    // support message mode elsewhere, so on Linux it's a Unix domain socket where each message is
    // prefixed by its 4-byte little-endian length (see DLL/InjectDLL/NamedPipes.cpp).
    public static class NamedPipes
    {
        private static NamedPipeServerStream _server;
        private static Socket _listener;
        private static Socket _client;
        public static bool Online = false;
        public static Thread ListenThread;
        public static EventHandler<string> PipeReceived;

        // Linux only: Cemu must be started with BOTWM_LAUNCHER_SOCKET set to this path.
        public static string SocketPath
        {
            get
            {
                string path = Environment.GetEnvironmentVariable("BOTWM_LAUNCHER_SOCKET");
                if (!string.IsNullOrEmpty(path))
                    return path;

                string runtimeDir = Environment.GetEnvironmentVariable("XDG_RUNTIME_DIR");
                return string.IsNullOrEmpty(runtimeDir) ? "/tmp/botwm-launcher.sock" : Path.Combine(runtimeDir, "botwm-launcher.sock");
            }
        }

        public static void StartServer()
        {
            if (OperatingSystem.IsWindows())
            {
                _server = new NamedPipeServerStream(@"languageConnectionPipe", PipeDirection.InOut, 2, PipeTransmissionMode.Message);
                _server.WaitForConnectionWithTimeout(5);
            }
            else
            {
                File.Delete(SocketPath);
                _listener = new Socket(AddressFamily.Unix, SocketType.Stream, ProtocolType.Unspecified);
                _listener.Bind(new UnixDomainSocketEndPoint(SocketPath));
                _listener.Listen(1);

                // The mod connects once Cemu has initialized, which takes longer than an injection.
                var accept = _listener.AcceptAsync();
                if (!accept.Wait(TimeSpan.FromSeconds(30)))
                {
                    CloseSocket();
                    Online = false;
                    throw new System.Exception("Could not connect with Cemu. Try again.");
                }
                _client = accept.Result;
            }
            Online = true;
        }

        public static void StartListenThread()
        {
            ListenThread = new Thread(PipeListen);
            ListenThread.IsBackground = true;
            ListenThread.Start();
        }

        public static void PipeListen()
        {
            while(true)
            {
                string dllMessage = receiveResponse().Replace("\0", "");

                if(PipeReceived != null)
                {
                    PipeReceived.Invoke(null, dllMessage);
                }
            }
        }

        public static void Disconnect()
        {
            if (OperatingSystem.IsWindows())
                _server.Disconnect();
            else
                CloseSocket();
            Online = false;
        }

        public static bool sendInstruction(string instruction)
        {
            byte[] buff = Encoding.UTF8.GetBytes(instruction + ";[END]");

            return sendInstruction(buff);
        }

        public static bool sendInstruction(byte[] instruction)
        {
            try
            {
                if (OperatingSystem.IsWindows())
                    _server.Write(instruction, 0, instruction.Length);
                else
                    SendMessage(instruction);

                if (receiveResponse().Contains("Succeeded"))
                    return true;

                return false;
            }
            catch
            {
                if (Online)
                    Disconnect();
                Online = false;
                return false;
            }
        }

        public static string receiveResponse()
        {
            if (!OperatingSystem.IsWindows())
                return ReceiveMessage();

            byte[] buff = new byte[2048];

            try
            {
                _server.Read(buff, 0, buff.Length);
                return Encoding.UTF8.GetString(buff);
            }
            catch
            {
                return "";
            }
        }

        public static void WaitForConnectionWithTimeout(this NamedPipeServerStream namedPipe, int timeout)
        {
            namedPipe.WaitForConnectionAsync();

            var connectionWatch = Stopwatch.StartNew();

            while(connectionWatch.ElapsedMilliseconds < timeout * 1000)
            {
                if (namedPipe.IsConnected)
                    return;
            }

            namedPipe.Disconnect();
            namedPipe.Close();
            Online = false;
            throw new System.Exception("Could not connect with Cemu. Try again.");
        }

        private static void SendMessage(byte[] message)
        {
            _client.Send(BitConverter.GetBytes(message.Length));
            _client.Send(message);
        }

        private static string ReceiveMessage()
        {
            try
            {
                byte[] length = ReceiveExactly(4);
                return Encoding.UTF8.GetString(ReceiveExactly(BitConverter.ToInt32(length, 0)));
            }
            catch
            {
                Thread.Sleep(100); // Disconnected: keep PipeListen from spinning.
                return "";
            }
        }

        private static byte[] ReceiveExactly(int size)
        {
            byte[] buffer = new byte[size];
            int received = 0;
            while (received < size)
            {
                int count = _client.Receive(buffer, received, size - received, SocketFlags.None);
                if (count == 0)
                    throw new SocketException((int)SocketError.ConnectionReset);
                received += count;
            }
            return buffer;
        }

        private static void CloseSocket()
        {
            _client?.Close();
            _listener?.Close();
            _client = null;
            _listener = null;
            File.Delete(SocketPath);
        }

    }
}
