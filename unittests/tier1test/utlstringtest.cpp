#include "tier0/dbg.h"
#include "unitlib/unitlib.h"
#include "tier1/utlstring.h"

DEFINE_TESTSUITE( UtlStringTestSuite )

static void ConstructorTests()
{
	CUtlString string;
	Shipping_Assert( string.Length() == 0 );
	Shipping_Assert( string.IsEmpty() == true );
	Shipping_Assert( string.GetForModify() && string.GetForModify()[ 0 ] == 0 );

	CUtlString string2( "shiz" );
	Shipping_Assert( string2.Length() == 4 );
	Shipping_Assert( !V_stricmp( string2.Get(), "shiz" ) );

	CUtlString string3( "thisstringismuchlongerthantwentywholehugecharacters", 20 );
	Shipping_Assert( string3.Length() == 20 );
}

static void BasicFunctionalityTests()
{
	CUtlString empty;
	empty.SetLength( 10 );
	Assert( empty.Length() == 10 );
	V_memcpy( empty.GetForModify(), "blah", 4 );
	empty.GetForModify()[ 4 ] = 0;
	Assert( empty.Length() == 4 );
	Shipping_Assert( !V_stricmp( empty.Get(), "blah" ) );

	empty.Clear();
	Shipping_Assert( empty.IsEmpty() );
	empty = "blah";
	Shipping_Assert( !empty.IsEmpty() );
	empty.Purge();
	Shipping_Assert( empty.IsEmpty() );

	empty = "CaMeLcAsE";

	Shipping_Assert( empty.IsEqual_CaseSensitive( "CaMeLcAsE" ) );
	Shipping_Assert( empty.IsEqual_CaseInsensitive( "camelCASE" ) );

	CUtlString copy( empty );
	Shipping_Assert( empty == copy );
	empty.ToLower();
	Shipping_Assert( empty != copy );

	empty.Append( "271" );
	Shipping_Assert( !V_stricmp( empty.Get(), "camelcase271" ) );
	empty.Append( "35123", 3 );
	Shipping_Assert( !V_stricmp( empty.Get(), "camelcase271351" ) );
	empty.Append( 'A' );
	Shipping_Assert( !V_stricmp( empty.Get(), "camelcase271351A" ) );

	empty.Append( '/' );
	empty.Append( '\\' );
	Shipping_Assert( !V_stricmp( empty.Get(), "camelcase271351A/\\" ) );
	empty.StripTrailingSlash();
	empty.StripTrailingSlash();
	Shipping_Assert( !V_stricmp( empty.Get(), "camelcase271351A" ) );

	empty = "sometext";
	empty.SetLength( 4 );
	Shipping_Assert( empty.Get()[ 4 ] == '\0' ); // Check for null terminator
	Shipping_Assert( empty.Length() == 4 );
	Shipping_Assert( empty == "some" );
}

static void TrimAPITests()
{
	CUtlString orig( "  testy  " );
	CUtlString orig2( "\n \n\ttesty\t\r\n \n\t\r" );
	CUtlString s;
	
	s = orig;
	s.TrimLeft( ' ' );
	Shipping_Assert( !V_stricmp( s.Get(), "testy  " ) );

	s = orig;
	s.TrimRight( ' ' );
	Shipping_Assert( !V_stricmp( s.Get(), "  testy" ) );

	s = orig2;
	s.TrimLeft();
	s.TrimRight();
	Shipping_Assert( !V_stricmp( s.Get(), "testy" ) );

	s = orig;
	s.Trim( ' ' );
	Shipping_Assert( !V_stricmp( s.Get(), "testy" ) );
	s = orig2;
	s.Trim();
	Shipping_Assert( !V_stricmp( s.Get(), "testy" ) );
}

static void OperatorAPITests()
{
	CUtlString orig( "base" );
	CUtlString orig2( "different" );

	// operator = on CUtlString
	orig = orig2;
	Shipping_Assert( !V_stricmp( orig.Get(), "different" ) );
	// perator = on const char *
	orig = "different2";
	Shipping_Assert( !V_stricmp( orig.Get(), "different2" ) );
	
	orig = orig2;
	// op ==
	Shipping_Assert( orig == orig2 );
	orig = "base";
	// op !=
	Shipping_Assert( orig != orig2 );

	orig += "1";
	Shipping_Assert( !V_stricmp( orig.Get(), "base1" ) );
	orig2 = "2";
	orig += orig2;
	Shipping_Assert( !V_stricmp( orig.Get(), "base12" ) );
	orig += '3';
	Shipping_Assert( !V_stricmp( orig.Get(), "base123" ) );
	// integer
	orig += 123;
	Shipping_Assert( !V_stricmp( orig.Get(), "base123123" ) );
	orig += 1.5f;
	Shipping_Assert( !V_stricmp( orig.Get(), "base1231231.5" ) );

	orig = "1";
	orig2 = "2";
	CUtlString newString = orig + orig2;
	Shipping_Assert( !V_stricmp( newString.Get(), "12" ) );
	newString = orig + "3";
	Shipping_Assert( !V_stricmp( newString.Get(), "13" ) );
	newString = orig + 42;
	Shipping_Assert( !V_stricmp( newString.Get(), "142" ) );

	orig = "this is a long string";
	newString = orig.Slice( 4 );
	Shipping_Assert( !V_stricmp( newString.Get(), " is a long string" ) );
	newString = orig.Slice( 5, 10 );
	Shipping_Assert( !V_stricmp( newString.Get(), "is a " ) );

	newString = orig.Left( 4 );
	Shipping_Assert( !V_stricmp( newString.Get(), "this" ) );
	newString = orig.Right( 6 );
	Shipping_Assert( !V_stricmp( newString.Get(), "string" ) );
	newString = orig.Replace( 's', 'q' );
	Shipping_Assert( !V_stricmp( newString.Get(), "thiq iq a long qtring" ) );
}

static void PatternTests()
{
	CUtlString str( "this is a very long pattern of very long things" );
	CUtlString pattern( "this is*" );

	Shipping_Assert( str.MatchesPattern( pattern ) );

	pattern = "notpresent";
	Shipping_Assert( !str.MatchesPattern( pattern ) );
}

static void FmtStr( CUtlString &str, const char *pFmt, ... )
{
	va_list argptr;
	va_start( argptr, pFmt );
	str.FormatV( pFmt, argptr );
	va_end( argptr );
}

static void FormatTests()
{
	CUtlString str;
	str.Format( "%s %s %i", "shiz", "baz", 1 );
	Shipping_Assert( !V_stricmp( str.Get(), "shiz baz 1" ) );

	FmtStr( str, "blah%i", 3 );
	Shipping_Assert( !V_stricmp( str.Get(), "blah3" ) );
}

static void FileNameAPITests()
{
	CUtlString path( "c:\\source2\\game\\source2\\somefile.ext" );

	CUtlString absPath = path.AbsPath();
	Shipping_Assert( absPath == path );
	CUtlString file = path.UnqualifiedFilename();
	Shipping_Assert( !V_stricmp( file.Get(), "somefile.ext" ) );
	CUtlString dir = path.DirName();
	Shipping_Assert( !V_stricmp( dir.Get(), "c:\\source2\\game\\source2" ) );
	dir = dir.DirName();
	Shipping_Assert( !V_stricmp( dir.Get(), "c:\\source2\\game" ) );
	CUtlString baseName = path.StripExtension();
	Shipping_Assert( !V_stricmp( baseName.Get(), "c:\\source2\\game\\source2\\somefile" ) );
	dir = path.StripFilename();
	Shipping_Assert( !V_stricmp( dir.Get(), "c:\\source2\\game\\source2" ) );

	file = path.GetBaseFilename();
	Shipping_Assert( !V_stricmp( file.Get(), "somefile" ) );
	CUtlString ext = path.GetExtension();
	Shipping_Assert( !V_stricmp( ext.Get(), "ext" ) );

	absPath = path.PathJoin( dir.Get(), file.Get() );
	Shipping_Assert( !V_stricmp( absPath.Get(), "c:\\source2\\game\\source2\\somefile" ) );
}

DEFINE_TESTCASE( UtlStringTest, UtlStringTestSuite )
{
	Msg( "Running CUtlString tests\n" );

	ConstructorTests();

	BasicFunctionalityTests();

	TrimAPITests();

	OperatorAPITests();

	PatternTests();

	FormatTests();

	FileNameAPITests();
}