using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Json;

namespace Sdkd.Protocol
{
	public abstract class MessageBase
	{
		protected JsonObject _json;
		
		public MessageBase() { }
		
		public MessageBase (MessageBase message)
		{
			Id = message.Id;
			Command = message.Command;
			Handle = message.Handle;
			_json = message._json;
		}

		public MessageBase (string request)
		{
			var jObj = JsonValue.Parse (request);

			Id = jObj ["ReqID"].ReadAs<int> ();
			Command = jObj ["Command"].ReadAs<string> ();	
			_json = jObj.ToJsonObject();
		}

		public int Id { get; set; }

		public string Command { get; set; }

		public int Handle { get; set; }
	}
}
