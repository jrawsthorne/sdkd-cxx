using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using NUnit.Framework;
using Sdkd.Protocol;

namespace SdkdTests
{
	[TestFixture]
	public class ProtocolTests : ProtocolTestsBase
	{
		private const int REQUEST_ID = 8675309;
		private const string COMMAND = "NEWHANDLE";

		[Test]
		public void When_Constructing_Request_From_Json_Properties_Are_Set()
		{
			var json = ReadJsonFile("Request.json");
			var request = new Request(json);

			Assert.That(request.Id, Is.EqualTo(REQUEST_ID));
			Assert.That(request.Command, Is.StringMatching(COMMAND));

		}

		[Test]
		public void When_Constructing_Response_From_Request_Base_Properties_Are_Copied()
		{
			var json = ReadJsonFile("Request.json");
			var request = new Request(json);

			//Sanity check
			Assert.That(request.Id, Is.EqualTo(REQUEST_ID));
			Assert.That(request.Command, Is.StringMatching(COMMAND));

			var response = new Response(request);
			Assert.That(response.Id, Is.EqualTo(request.Id));
			Assert.That(response.Command, Is.StringMatching(request.Command));
			Assert.That(response.Handle, Is.EqualTo(request.Handle));
		}
	}
}
