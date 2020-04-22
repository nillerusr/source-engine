using System;
using System.Text.RegularExpressions;
using System.Collections;
using System.IO;

namespace MapSorter
{


	/// <summary>
	/// Summary description for Class1.
	/// </summary>
	class Class1
	{
		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main(string[] args)
		{
			if( args.Length != 1 )
			{
				Console.WriteLine("Usage: MapSorter <filename.map>");
				return;
			}

			new MapFileLoader(args[0]).DumpReport();
		}
	}
}
