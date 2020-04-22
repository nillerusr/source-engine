//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: searches through all bsp files in the current directory parsing out entity details
//
//=============================================================================//

#include <stdio.h>
#include <string.h>
#include <io.h>
#include <malloc.h>

#define max(x,y) ( ((x) > (y)) ? (x) : (y) )

void SetSearchWord( const char *searchWord );
char *FindSearchWord( char *buffer, char *bufend );
char *ParseToken( char *data, char *newToken );

void ClearTable( void );
void ClearUsageCountTable( void );
void AddToTable( const char *name );
void PrintOutTable( void );

void ParseFGD( char *buffer, char *bufend, const char *searchKey );

const char *g_UsageString = "usage:  entcount [-fgd <fgdfile>] [-nofgd] [-permap] [-onlyent <entname>] [-files <searchmask>]\n";


int main( int argc, char *argv[] )
{
	if ( argc < 2 )
	{
		printf( g_UsageString );
		return 0;
	}

	bool printPerMap = false;
	const char *filterEnt = NULL;
	const char *fgdFile = NULL;
	const char *fileMask = "*.bsp";

	// parse the arguments
	for ( int count = 1; count < argc; count++ )
	{
		if ( !stricmp( argv[count], "-permap" ) )
		{
			printPerMap = true;
		}
		else if ( !stricmp( argv[count], "-onlyent" ) )
		{
			count++;
			if ( count < argc )
			{
				filterEnt = argv[count];
			}
		}
		else if ( !stricmp( argv[count], "-fgd" ) )
		{
			count++;
			if ( count < argc )
			{
				fgdFile = argv[count];
			}
		}
		else if ( !stricmp( argv[count], "-files" ) )
		{
			count++;
			if ( count < argc )
			{
				fileMask = argv[count];
			}
		}
		else if ( !stricmp( argv[count], "-nofgd" ) )
		{
		}
		else
		{
			printf( "error: unknown parameter \"%s\"\n", argv[count] );
			printf( g_UsageString );
			return 1;
		}
	}

	// clear the entity accumulator table
	ClearTable();

	// open and parse the FGD, unless the -nofgd flag is specified
	if ( fgdFile && !filterEnt && !printPerMap )
	{
		FILE *f = fopen( fgdFile, "rb" );
		if ( !f )
		{
			printf( "error: could not open file %s\n", fgdFile );
			return 2;
		}

		int filelen;
		fseek( f, 0, SEEK_END );
		filelen = ftell( f );
		fseek( f, 0, SEEK_SET );

		// allocate and load into memory
		char *buffer = (char*)malloc( filelen );
		char *bufend = buffer + filelen;
		fread( buffer, filelen, 1, f );
		fclose( f );

		// search for all @pointclass, then @solidclass
		ParseFGD( buffer, bufend, "PointClass" );
		ParseFGD( buffer, bufend, "SolidClass" );

		// reset the usage counts to 0
		ClearUsageCountTable();	

		free( buffer );
	}

	// parse through all the bsp files
	_finddata_t fileinfo;
	int FHandle = _findfirst( fileMask, &fileinfo );
	
	if ( FHandle == -1 )
	{
		printf( "error: no files found in current directory\n" );
		return 1;
	}

	SetSearchWord( "\"classname\"" );

	do {
		// open the file
		FILE *f = fopen( fileinfo.name, "rb" );
		if ( !f )
		{
			printf( "error: couldn't open file %s\n", fileinfo.name );
			return 2;
		}

		// calculate file length
		int filelen;
		fseek( f, 0, SEEK_END );
		filelen = ftell( f );
		fseek( f, 0, SEEK_SET );

		// allocate and load into memory
		char *buffer = (char*)malloc( filelen );
		char *bufpos = buffer + strlen( "\"classname\"" ) - 1;
		char *bufend = buffer + filelen;
		fread( buffer, filelen, 1, f );
		fclose( f );

		bool entFound = false;

		while ( 1 )
		{
			bufpos = FindSearchWord( bufpos, bufend );
			if ( !bufpos )
				break;

			// find the next word
			static char Token[256];
			ParseToken( bufpos, Token );

			// add the word to the list, filtering if necessary
			if ( !filterEnt || !stricmp(filterEnt, Token) )
			{
				AddToTable( Token );
				entFound = true;
			}

			bufpos += strlen( Token );
		}

		free( buffer );

		// print the bsp name, if an ent is found, or we are not filtering for ents
		if ( entFound || !filterEnt )
			printf( "%s\n", fileinfo.name );

		if ( printPerMap )
		{
			PrintOutTable();
			ClearUsageCountTable();
		}

	} while ( _findnext(FHandle, &fileinfo) == 0 );

	PrintOutTable();

	return 0;
}

void ParseFGD( char *buffer, char *bufend, const char *searchKey )
{
	char *bufpos = buffer + strlen( searchKey ) - 1;
	SetSearchWord( searchKey );

	while ( 1 )
	{
		bufpos = FindSearchWord( bufpos, bufend );
		if ( !bufpos )
			break;

		// search for the corresponding '='
		while ( *bufpos != '=' )
			bufpos++;
		bufpos++;
		
		// find the classname
		static char Token[256];
		ParseToken( bufpos, Token );

		AddToTable( Token );

		bufpos += strlen( Token );
	}
}


/////////// entity table stuff //////////////
const int MAX_ENTS = 2000;
int NumEnts = 0;
char *EntNames[ MAX_ENTS ];
int EntUsage[ MAX_ENTS ];

void ClearTable( void )
{
	memset( EntNames, 0, sizeof(EntNames) );
	memset( EntUsage, 0, sizeof(EntUsage) );
}

void ClearUsageCountTable( void )
{
	memset( EntUsage, 0, sizeof(EntUsage) );
}

void AddToTable( const char *name )
{
	// search for it in the table
	for ( int i = 0; i < NumEnts;  i++ )
	{
		if ( EntNames[i] && !strcmp(EntNames[i], name) )
		{
			// it's already in the table
			// increment the usage count
			EntUsage[i] += 1;
			return;
		}
	}

	// append to the table
	EntNames[NumEnts] = (char*)malloc( strlen(name) + 1 );
	strcpy( EntNames[NumEnts], name );
	EntUsage[NumEnts] = 1;

	NumEnts++;
}

void PrintOutTable( void )
{
	while ( 1 )
	{
		// find the highest item
		int highestUsage = -1;
		int highestEnt = 0;

		for ( int i = 0; i < NumEnts; i++ )
		{
			if ( EntNames[i] && highestUsage < EntUsage[i] )
			{
				highestUsage = EntUsage[i];
				highestEnt = i;
			}
		}

		// check for no more ents
		if ( highestUsage == -1 )
			return;
		
		// display usage stats of item
		printf( " %5d  %s\n", highestUsage, EntNames[highestEnt] );

		// remove item from list
		free( EntNames[highestEnt] );
		EntNames[highestEnt] = NULL;
	}
}



////////// string search stuff ////////////

static unsigned char JumpTable[256];
static int SearchWordLen = 0;
static const char *SearchWord;

void SetSearchWord( const char *Word )
{
	SearchWord = Word;
	SearchWordLen = strlen( SearchWord );

	// build the jump table
	
	// initialize all values to jump the length of the string
	memset( JumpTable, SearchWordLen, sizeof(JumpTable) );

	// set the amount the searcher can jump forward, depending on the character
	for ( int i = 0; i < SearchWordLen; i++ )
	{
		JumpTable[ (unsigned char)SearchWord[i] ] = max( SearchWordLen - i - 1, 1 );
	}
}

char *FindSearchWord( char *buffer, char *bufend )
{

/*
	for ( int i = SearchWordLen-1; i >= 0; i-- )
	{
		if ( *buffer != SearchWord[i] )
		{
			buffer += ( JumpTable[ (unsigned char)(*(buffer + i - SearchWordLen + 1)) ] - 1 );

			// no more buffer to search
			if ( buffer >= bufend )
				return NULL;

			// reset search counter
			i = SearchWordLen;
		}
		else
		{
			// it's a match, move backwards to search
			buffer--;
		}
	}
*/

	while ( 1 )
	{
		if ( strnicmp(buffer - SearchWordLen, SearchWord, SearchWordLen) )
		{
			// strings not equal, jump ahead
			buffer += JumpTable[ (unsigned char)*buffer ];

			if ( buffer >= bufend )
				return NULL;
		}
		else
		{
			break;
		}
	}

	// we have a match!

	// return a pointer just past the found key
	return buffer;
}



/*
==============
ParseToken

Parse a token out of a string
outputs the parsed token into newToken
==============
*/
char *ParseToken( char *data, char *newToken )
{
	int             c;
	int             len;
	
	len = 0;
	newToken[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;                    // end of file;
		data++;
	}
	
// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while ( len < 256 )
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				newToken[len] = 0;
				return data;
			}
			newToken[len] = c;
			len++;
		}

		if ( len >= 256 )
		{
			len--;
			newToken[len] = 0;
		}
	}

// parse single characters
	if ( c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' )
	{
		newToken[len] = c;
		len++;
		newToken[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		newToken[len] = c;
		data++;
		len++;
		c = *data;
		if ( c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' )
			break;

		if ( len >= 256 )
		{
			len--;
			newToken[len] = 0;
			break;
		}

	} while (c>32);
	
	newToken[len] = 0;
	return data;
}
