using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Configuration;
using System.Net.Sockets;
using System.Net;
using System.IO;
using Sdkd.Protocol;

namespace SdkdConsole
{
	class Program
	{
		static void Main (string[] args)
		{
			var maxConnections = int.Parse (ConfigurationManager.AppSettings ["maxConnections"]);
			var portFile = "";
			foreach (string arg in args) {
				if (arg.StartsWith ("infofile")) {
					portFile = arg.Split ('=') [1];
				}
			}
			
			if (string.IsNullOrEmpty (portFile)) {
				throw new MissingFieldException ("Need portfile!");
			}

			var serverSocket = new Socket (AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
			var endPoint = new IPEndPoint (IPAddress.Any, 0);
			
			serverSocket.Bind (endPoint);
			serverSocket.Listen (maxConnections);
			
			var port = (serverSocket.LocalEndPoint as IPEndPoint).Port;			
			Console.WriteLine ("SDKD listening on port " + port);
			File.WriteAllText (portFile, port.ToString ());
			
			Socket connCtl = serverSocket.Accept ();
			ControlIOHandler oCtl = new ControlIOHandler (connCtl, serverSocket);
			oCtl.run ();
			
			return;
			
		}

//		private static void AcceptCallback(IAsyncResult asyncResult)
//		{
//			var serverSocket = asyncResult.AsyncState as Socket;
//			var clientSocket = serverSocket.EndAccept(asyncResult);			
//
//			Console.WriteLine("AcceptCallback(): " + Thread.CurrentThread.GetHashCode());
//
//			var clientState = new ClientState { ClientSocket = clientSocket };
//			clientSocket.BeginReceive(clientState.ReceiveBuffer, 0, 
//					clientState.ReceiveBuffer.Length, SocketFlags.None, ReceiveCallback, clientState);
//
//		}
//
//		private static void ReceiveCallback(IAsyncResult asyncResult)
//		{
//			var clientState = asyncResult.AsyncState as ClientState;
//
//			var receiveMessageSize = clientState.ClientSocket.EndReceive(asyncResult);
//
//			if (receiveMessageSize > 0)
//			{
//				Console.WriteLine("ReceiveCallback(): " + Thread.CurrentThread.GetHashCode());
//
//				clientState.Message += Encoding.Default.GetString(clientState.ReceiveBuffer);				
//				
//				clientState.ClientSocket.BeginSend(clientState.ReceiveBuffer, 0,
//						clientState.ReceiveBuffer.Length, SocketFlags.None, SendCallback, clientState);
//			}
//			else
//			{
//				var sb = new StringBuilder();
//				for (int i = 0; i < clientState.ReceiveBuffer.Length; i++)
//				{
//					var str = Encoding.Default.GetString(clientState.ReceiveBuffer, i, 1);
//					if (str == "\n") break;
//					sb.Append(str);
//				}
//
//				var request = new Request(sb.ToString());
//				Console.WriteLine("Received request with command: " + request.Command);
//				clientState.ClientSocket.Close();
//			}
//
//		}
//
//		private static void SendCallback(IAsyncResult asyncResult)
//		{
//			var clientState = asyncResult.AsyncState as ClientState;
//			int bytesSent = clientState.ClientSocket.EndReceive(asyncResult);
//
//			Console.WriteLine("SendCallback():" + Thread.CurrentThread.GetHashCode());
//			clientState.ClientSocket.BeginReceive(clientState.ReceiveBuffer, 0,
//						clientState.ReceiveBuffer.Length, SocketFlags.None, ReceiveCallback, clientState);
//
//		}
		
		private class SdkdTerminationException : Exception { }
		private class SdkdGoodbye : SdkdTerminationException { }
		private class SdkdUngracefulClose : SdkdTerminationException { }
		
		
		private class ProtocolIOHandler {
			
			public ProtocolIOHandler(Socket sock) {
				this.sock = sock;
			}
			
			private const int BUFFER_SIZE = 4096;
			private string _receiveBuffer = null;
			private List<Sdkd.Protocol.Request> reqbuf = new List<Request>();
			
			void run () { }
			
			
			private bool _get_single_request (out Sdkd.Protocol.Request req)
			{
				if (reqbuf.Count < 1) {
					req = null;
					return false;
				}
				
				req = reqbuf [0];
				reqbuf.RemoveAt (0);
				return true;
			}
				
			public
			Sdkd.Protocol.Request
			GetMessage (bool do_block)
			{
				
				Sdkd.Protocol.Request ret = null;
				
				if (_get_single_request (out ret) == true) {
					return ret;
				}
		
				sock.Blocking = do_block;
				
				int nr = 0;
				do {
					
					byte[] newbuf = new byte[BUFFER_SIZE];
					
					try {
						nr = sock.Receive (newbuf, BUFFER_SIZE, 0);
					} catch (SocketException exc) {
						switch ((int)exc.ErrorCode) {
						case (int)SocketError.Interrupted:
							continue;
						case (int)SocketError.WouldBlock:
							break;
						default:
							throw exc;
						}
					}
					
					string newchunk = 
						System.Text.Encoding.ASCII.GetString (newbuf);
					
					int pos = newchunk.IndexOf ("\n");
					if (pos < 0) {
						// We didn't find a message end in the current chunk, 
						// append to our current buffer
						// and continue.
						_receiveBuffer += newchunk;
						continue;
					}
					
					string reqjson = _receiveBuffer + newchunk.Substring (0, pos);					
					_receiveBuffer = newchunk.Substring (pos, newchunk.Length - pos);
					
					// Ensure that we construct the new request *after* 
					// the buffer. That way, if an exception occurs, our 
					// buffer handling remains consistent
					Sdkd.Protocol.Request req = new Sdkd.Protocol.Request (reqjson);
					
					if (do_block) {
						// If we are blocking, we want a message immediately 
						// to work on (because we have nothing else to do). 
						//Cleverer logic might be implemented, at the cost of
						// code complexity.
						return req;
					} else {
						reqbuf.Insert (reqbuf.Count, req);
					}
				} while (nr > 0);
				
				if (nr == 0) {
					throw new SdkdUngracefulClose();
				}
				
				return ret;
			}
			
			public void SendString (ref string buf, bool block)
			{
				sock.Blocking = block;
				int nw = 0;
				do {
					byte[] sndchunk = System.Text.Encoding.ASCII.GetBytes (buf);
					try {
						nw = sock.Send (sndchunk);
					} catch (SocketException exc) {
						if (exc.ErrorCode == 
						    (int)SocketError.WouldBlock && block == false) {
							break;
						} else if (exc.ErrorCode == 
						           (int)SocketError.Interrupted) {
							continue;
						} else {
							throw exc;
						}
					}
					buf = buf.Substring (nw);
				} while (nw > 0 && buf.Length > 0);
			}
			
			public Socket sock { get; set; }
		}
				
		private class ControlIOHandler : ProtocolIOHandler {
			// Socket on which we accept()
			private Socket _lsn;
			
			// Buffer for messages..
			private string _sendBuffer;
			
			// Dictionary containing handles and their appropriate IDs
			private Dictionary<int, HandleIOHandler> _handlesById 
				= new Dictionary<int, HandleIOHandler>();
			
			// List of *all* the handles. We iterate through this to delete stale
			// or dead threads.
			private List<HandleIOHandler> _allHandles 
				= new List<HandleIOHandler>();
			
			public ControlIOHandler(Socket sock, Socket listener) : base(sock)
			{
				this._lsn = listener;
			}
			
			
			public HandleIOHandler GetHandleByID (int id)
			{
				HandleIOHandler ret = null;
				lock (_handlesById) {
					_handlesById.TryGetValue (id, out ret);
					return ret;
				}
			}
			
			public void RegisterHandle (HandleIOHandler handle, int id)
			{
				lock (_handlesById) {
					if (_handlesById.ContainsKey (id)) {
						throw new ArgumentException 
							("Handle already exists with this ID");
					}
					_handlesById [id] = handle;
				}
			}
			
			private bool ProcessRequest (Sdkd.Protocol.Request req)
			{
				return false;
			}
			
			private void _cleanupChildThreads (bool force)
			{
				for (int ii = _allHandles.Count-1; ii >= 0; ii--) {
					HandleIOHandler handle = _allHandles [ii];
					
					if (force == true) {
						handle.thr.Abort ();
					}
					
					if (handle.thr.ThreadState == ThreadState.Stopped ||
					    	force == true) {
						
						handle.thr.Join ();
						// Handle's dtor will unregister itself from the dictionary
						_allHandles.RemoveAt (ii);
					}
				}
			}
			
			private void _sweep_once ()
			{
				List<Socket> sel_rd = new List<Socket> () { _lsn, sock };
				List<Socket> sel_wr = new List<Socket> ();
				
				// If we have something to write, then have Select check on write-ability too.
				if (!string.IsNullOrEmpty (_sendBuffer)) {
					sel_wr.Add (sock);
				}
				
				// Check for any new connections
				Socket.Select (sel_rd, sel_wr, null, -1);
				if (sel_rd.Contains (_lsn)) {
					Console.WriteLine ("Got a new connection.. " +
					                   "Creating child handle");
					
					Socket hsock = _lsn.Accept ();
					HandleIOHandler child = new HandleIOHandler (hsock, this);
					_allHandles.Add (child);
				}
				
				// Check for any new commands
				if (sel_rd.Contains (sock)) {
					Sdkd.Protocol.Request req = GetMessage (false);
					if (req != null && req.Command == "GOODBYE") {
						throw new SdkdGoodbye ();
					}
				}
				
				// Check if we need to write anything
				if (sel_wr.Contains (sock)) {
					SendString(ref _sendBuffer, false);
				}
				// Finally, collect any dead children:
				_cleanupChildThreads (false);
			}
			
			public void run ()
			{
				while (true) {
					try {
						_sweep_once ();
					} catch (Exception exc) {
						_cleanupChildThreads (true);
						throw exc;
					}
				}
			}
		}
		
		private class HandleIOHandler : ProtocolIOHandler {
			
			// Parent global object
			ControlIOHandler parent;
			
			// Thread object
			public Thread thr;
			
			public void SendMessage (Sdkd.Protocol.Response resp)
			{
				sock.Blocking = true;
				string msgbuf = resp.Encode () + "\n";
				while (msgbuf.Length > 0) {
					SendString (ref msgbuf, true);
				}
			}

			
			public int Id { get; set; }
			// Command handlers
			
			public HandleIOHandler(Socket sock, ControlIOHandler parent) : 
				base(sock)
			{
				this.parent = parent;
				Id = 0;
				thr = new Thread(new ThreadStart(this.run));
				thr.Start();
			}
			
			private void _handle_GOODBYE(Sdkd.Protocol.Request req)
			{
				throw new SdkdGoodbye();
			}
			private void _handle_MC_DS_GET(Sdkd.Protocol.Request req)
			{
			}
			
			
			private delegate void _CommandHandler (Sdkd.Protocol.Request req);
			private Dictionary<string, _CommandHandler> _commandHandlers 
				= new Dictionary<string, _CommandHandler>();
			
			public void log (string s)
			{
				string formatted = "[Handle Thread " + 
					Thread.CurrentThread.GetHashCode() + "] " + s;
				
				Console.WriteLine (formatted);
			}
			
			public void run ()
			{
				
				// Get initial NEWHANDLE request
				Sdkd.Protocol.Request req = GetMessage (true);
				log ("Got Initial message: " + req.Command);
				if (req.Command != "NEWHANDLE") {
					System.Console.WriteLine 
						("Expected initial NEWHANDLE command. " +
						"Got something else instead"
					);
					return;
				}
				
				parent.RegisterHandle (this, req.Handle);
				
				Sdkd.Protocol.Response resp 
					= new Sdkd.Protocol.Response (req,
					                              (int)ErrorCode.SUBSYSf_SDKD |
					(int)ErrorCode.SDKD_ENOIMPL
				);
				SendMessage (resp);
				
				while (true) {
					req = GetMessage (true);
					if (req == null) {
						log ("Socket closed without sending GOODBYE!");
						return;
					}
					
					log ("Got message " + req.Command);
					
					_CommandHandler handler;
					if (!_commandHandlers.TryGetValue (req.Command, out handler)) {
						//TODO: Send a response indicating SUBSYSf_SDKD | SDKD_ENOIMPL
					} else {
						handler (req);
					}
				}
			}
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
