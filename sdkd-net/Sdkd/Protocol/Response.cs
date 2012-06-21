using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Sdkd.Protocol
{
	public class Response : MessageBase
	{
		public Response() : base() { }
		
		public Response(Request request) : base(request) { }
	}
}
