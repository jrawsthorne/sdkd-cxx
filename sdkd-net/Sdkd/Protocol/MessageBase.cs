using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Json;

namespace Sdkd.Protocol
{
	public abstract class MessageBase
	{
		public MessageBase() { }
		
		public MessageBase(MessageBase message)
		{
			Id = message.Id;
			Command = message.Command;
			Handle = message.Handle;
		}

		public MessageBase(string request)
		{
			var jObj = JsonValue.Parse(request);

			Id = jObj["ReqID"].ReadAs<int>();
			Command = jObj["Command"].ReadAs<string>();			
		}

		public int Id { get; set; }

		public string Command { get; set; }

		public int Handle { get; set; }
	}
}
