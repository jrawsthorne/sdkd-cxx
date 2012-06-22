using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Json;

namespace Sdkd.Protocol
{
	public class Response : MessageBase
	{
		private Error _err = new Error(0);
		
		public Response() : base() { }
		
		public Response (Request request, int err = 0) :
			base(request)
		{
			_err = new Error (err);
		}
		
		public Response(Request request, Error err) :
			base(request)
		{
			_err = err;
		}
		
		public Response (Request request, JsonObject payload) :
			base(request)
		{
			_err = new Error (0);
			_json ["ResponseData"] = payload;
		}
		
		public string Encode ()
		{
			_json ["Status"] = _err.errnum;
			if (_err.errnum > 0) {
				_json ["ErrorString"] = _err.errstr;
			}
			if (!_json.ContainsKey ("ResponseData")) {
				_json ["ResponseData"] = new JsonObject ();
			}
			return _json.ToString ();
		}
	}
}