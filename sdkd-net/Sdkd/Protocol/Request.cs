using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Json;

namespace Sdkd.Protocol
{
	public class Request : MessageBase
	{
		public Request() : base() { }

		public Request(string request) : base(request) { }

		public object Payload { get; set; }

		public bool IsValid 
		{
			get { return Id > 0; }
		}
	}
}
