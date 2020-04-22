using System;
using System.Text.RegularExpressions;
using System.Collections;
using System.IO;

namespace MapSorter
{
	public class Element : IComparable
	{
		/// <summary>
		/// Segment this element is in.
		/// </summary>
		public int Segment;

		/// <summary>
		/// Base or virtual file address.
		/// </summary>
		public int Address;

		/// <summary>
		/// Adjusted relative virtual address.
		/// </summary>
		public int RVA;

		/// <summary>
		/// Name of this element.
		/// </summary>
		public string Text;

		/// <summary>
		/// Object file this element is located in.
		/// </summary>
		public string Obj;

		/// <summary>
		/// Size of this element, or -1.
		/// </summary>
		public int Size = -1;

		/// <summary>
		/// True if this is the last element in an object file.
		/// </summary>
		public bool bCrossObj = false;	

		public Element( int Segment, int Address, string Text, int RVA, string Obj )
		{
			this.Segment = Segment;
			this.Address = Address;
			this.RVA = RVA;
			this.Text = Text;
			this.Obj = Obj;
		}

		// Comparable interface:
		int System.IComparable.CompareTo( object o )
		{
			// HACK HACK - sorts according to size if size field is non negative, otherwise sorts by segment then address
			Element other = (Element)o;

			// Sort by size:
			if( other.Size != -1 || Size != -1 )
			{
				if( Size < other.Size )
					return -1;

				if( Size > other.Size )
					return 1;

				return 0;
			}

			// Sizes aren't defined, sort by Segment then Address:
			if( Segment != other.Segment )
			{
				if( Segment < other.Segment )
					return -1;
				else
					return 0;
			}

			if( Address < other.Address )
				return -1;
			else if( Address > other.Address )
				return 1;

			return 0;
		}
	}

	public class Module : IComparable
	{
		public string Name;
		public int Size;

		public Module(string Name, int Size)
		{
			this.Name = Name;
			this.Size = Size;
		}

		int System.IComparable.CompareTo( object o )
		{
			Module m = (Module)o;
			if( Size < m.Size )
				return -1;

			if( Size > m.Size )
				return 1;
			return 0;
		}
	}

	/// <summary>
	/// An atomic class that loads and parses a given map file.	
	/// </summary>
	public class MapFileLoader
	{
		ArrayList Elements;
		ArrayList Modules;

		/// <summary>
		/// Regular expression to break mapfile elements up:
		/// </summary>
		protected static Regex ElementRegex = new Regex(@"([0-9a-fA-F]{4})\:([0-9a-fA-F]{8})\s+([^\s]*)\s+([0-9a-fA-F]{8})\s(f\s)?(i\s)?\s+(.*\.obj)",RegexOptions.Compiled | RegexOptions.Multiline | RegexOptions.IgnoreCase );

		
		public MapFileLoader( string filename )
		{
			// Load the element data from the mapfile:
			LoadElements( new StreamReader(filename).ReadToEnd() );

			// Compute modules and their sizes:
			ComputeModules();
		}

		public void DumpReport()
		{
			Console.WriteLine("***** Elements by size ascending. (*) = element straddles an .obj file boundary, so the size may not be correct.");
			Console.WriteLine();

			foreach (Element e in Elements )
			{
				if( e.Size < 1024 )
					continue;

				if( e.bCrossObj )
					Console.Write("(*)");

				Console.WriteLine( e.Size / 1024 + "k : " +e.Obj + " : " + e.Text  );
			}

			Console.WriteLine();
			Console.WriteLine();
			Console.WriteLine("***** Modules by size, ascending.  This is estimated based on elements that don't straddle .obj boundaries.");
			Console.WriteLine();

			foreach(Module m in Modules )
			{
				if( m.Size < 1024 )
					continue;

				Console.WriteLine( m.Size / 1024 + "k : " + m.Name  );
			}


		}

		protected void LoadElements( string mapfile )
		{
			// Match each appropriate line in the map file.  The summary entries at the top of each map file are NOT matched by this regex.
			MatchCollection matches = ElementRegex.Matches(mapfile);
				

			Elements = new ArrayList();
			
			// Convert each match to an Element type and add them to an array list.
			foreach( Match m in matches )
			{

				Element e = new Element(int.Parse(m.Groups[1].Value,System.Globalization.NumberStyles.AllowHexSpecifier),
					int.Parse(m.Groups[2].Value,System.Globalization.NumberStyles.AllowHexSpecifier),
					m.Groups[3].Value,
					int.Parse(m.Groups[4].Value,System.Globalization.NumberStyles.AllowHexSpecifier),
					m.Groups[7].Value);

				Elements.Add(e);

			}

			// Sort the list by address:
			Elements.Sort();
			
			Element previous = null;

			// Compute estimated sizes for each element in the list:
			foreach( Element e in Elements )
			{
				if( previous != null )
				{
					if( e.Segment == previous.Segment )
					{
						previous.Size = e.Address- previous.Address;
					
						// Take note of the symbols that cross object file boundaries:
						if( !previous.Obj.Equals(e.Obj) )
						{
							previous.bCrossObj = true;
						}
					}
				}

				previous = e;
			}

			// Sort the list by size
			Elements.Sort();


		}

		protected void ComputeModules()
		{
			// Estimate the size of each object file:
			Hashtable h = new Hashtable();

			foreach(Element e in Elements )
			{
				if( !h.ContainsKey(e.Obj) )
					h.Add(e.Obj,0);

				if( !e.bCrossObj )
				{
					h[e.Obj] = (int)h[e.Obj] + e.Size;
				}
			}

			Modules = new ArrayList();
			
			foreach( string key in h.Keys )
			{
				Modules.Add( new Module(key, (int)h[key] ));
			}

			Modules.Sort();
		}

	}
}
