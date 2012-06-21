using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Configuration;
using System.Net.Sockets;
using System.Net;
using Sdkd.Protocol;
using System.IO;

namespace SdkdConsole
{
	class Program
	{				
		static void Main(string[] args)
		{
			var maxConnections = int.Parse(ConfigurationManager.AppSettings["maxConnections"]);
			var portFile = ConfigurationManager.AppSettings["portFile"];

			var serverSocket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
			var endPoint = new IPEndPoint(IPAddress.Any, 0);
			
			serverSocket.Bind(endPoint);
			serverSocket.Listen(maxConnections);

			var port = (serverSocket.LocalEndPoint as IPEndPoint).Port;			
			Console.WriteLine("SDKD listening on port " + port);
			File.WriteAllText(portFile, port.ToString());

			while (true)
			{
				var result = serverSocket.BeginAccept(AcceptCallback, serverSocket);

				Console.WriteLine("Doing some stuff");

				result.AsyncWaitHandle.WaitOne();
			}
		}

		private static void AcceptCallback(IAsyncResult asyncResult)
		{
			var serverSocket = asyncResult.AsyncState as Socket;
			var clientSocket = serverSocket.EndAccept(asyncResult);			

			Console.WriteLine("AcceptCallback(): " + Thread.CurrentThread.GetHashCode());

			var clientState = new ClientState { ClientSocket = clientSocket };
			clientSocket.BeginReceive(clientState.ReceiveBuffer, 0, 
					clientState.ReceiveBuffer.Length, SocketFlags.None, ReceiveCallback, clientState);

		}

		private static void ReceiveCallback(IAsyncResult asyncResult)
		{
			var clientState = asyncResult.AsyncState as ClientState;

			var receiveMessageSize = clientState.ClientSocket.EndReceive(asyncResult);

			if (receiveMessageSize > 0)
			{
				Console.WriteLine("ReceiveCallback(): " + Thread.CurrentThread.GetHashCode());

				clientState.Message += Encoding.Default.GetString(clientState.ReceiveBuffer);				
				
				clientState.ClientSocket.BeginSend(clientState.ReceiveBuffer, 0,
						clientState.ReceiveBuffer.Length, SocketFlags.None, SendCallback, clientState);
			}
			else
			{
				var sb = new StringBuilder();
				for (int i = 0; i < clientState.ReceiveBuffer.Length; i++)
				{
					var str = Encoding.Default.GetString(clientState.ReceiveBuffer, i, 1);
					if (str == "\n") break;
					sb.Append(str);
				}

				var request = new Request(sb.ToString());
				Console.WriteLine("Received request with command: " + request.Command);
				clientState.ClientSocket.Close();
			}

		}

		private static void SendCallback(IAsyncResult asyncResult)
		{
			var clientState = asyncResult.AsyncState as ClientState;
			int bytesSent = clientState.ClientSocket.EndReceive(asyncResult);

			Console.WriteLine("SendCallback():" + Thread.CurrentThread.GetHashCode());
			clientState.ClientSocket.BeginReceive(clientState.ReceiveBuffer, 0,
						clientState.ReceiveBuffer.Length, SocketFlags.None, ReceiveCallback, clientState);

		}


		private class ClientState
		{
			private const int BUFFER_SIZE = 4096;

			public string Message { get; set; }

			public ClientState()
			{
				_receiveBuffer = new byte[BUFFER_SIZE];
			}

			public Socket ClientSocket { get; set; }

			private readonly byte[] _receiveBuffer;

			public byte[] ReceiveBuffer 
			{
				get { return _receiveBuffer; }
			}
		}


	}
}
