using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace SdkdTests
{
	public abstract class ProtocolTestsBase
	{
		public string ReadJsonFile(string fileName)
		{
			var path = Path.Combine(Environment.CurrentDirectory, "Data", fileName);
			return File.ReadAllText(path);
		}
	}
}
