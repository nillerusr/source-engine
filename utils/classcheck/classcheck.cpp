//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "stdafx.h"
#include <stdio.h>
#include <windows.h>
#include "classcheck_util.h"
#include "icodeprocessor.h"
#include "tier1/strtools.h"
#include "tier0/dbg.h"


SpewRetval_t SpewFunc( SpewType_t type, char const *pMsg )
{	
	printf( "%s", pMsg );
	OutputDebugString( pMsg );
	
	if ( type == SPEW_ERROR )
	{
		printf( "\n" );
		OutputDebugString( "\n" );
	}

	return SPEW_CONTINUE;
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void printusage( void )
{
	vprint( 0, "usage:  classcheck -q -h -m -t <root source directory> <game:  hl2 | tf2>\n\
		\t-q = quiet\n\
		\t-h = print class hierarchy\n\
		\t-m = don't print member functions/variables of class\n\
		\t-t = don't print type description errors\n\
		\t-p = don't print prediction description errors\n\
		\t-c = create missing save descriptions\n\
		\t-x = create missing prediction descriptions\n\
		\t-i = specify specific input file to parse\n\
		\t-b = similar to -i, allows specifying files outside client\\server directories\n\
		\t-j = check for Crazy Jay Stelly's mismatched Hungarian notation errors\n\
		\t-l = log to file log.txt\n" );

	// Exit app
	exit( 1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *sourcetreebase - 
//			*subdir - 
//			*baseentityclass - 
//-----------------------------------------------------------------------------
void ProcessDirectory( const char *game, const char *sourcetreebase, const char *subdir, const char *baseentityclass )
{
	char rootdirectory[ 256 ];
	sprintf( rootdirectory, "%s\\%s", sourcetreebase, subdir );

	// check for existence
	if ( COM_DirectoryExists( rootdirectory ) )
	{
		processor->Process( baseentityclass, game, sourcetreebase, subdir );
	}
	else
	{
		vprint( 0, "Couldn't find directory %s, check path %s\n", rootdirectory, sourcetreebase );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *sourcetreebase - 
//			*subdir - 
//			*baseentityclass - 
//-----------------------------------------------------------------------------
void ProcessFile( const char *game, const char *sourcetreebase, const char *subdir, const char *baseentityclass, const char *pFileName )
{
	char rootdirectory[ 256 ];
	sprintf( rootdirectory, "%s\\%s", sourcetreebase, subdir );

	// check for existence
	if ( COM_DirectoryExists( rootdirectory ) )
	{
		processor->Process( baseentityclass, game, sourcetreebase, subdir, pFileName );
	}
	else
	{
		vprint( 0, "Couldn't find directory %s, check path %s\n", rootdirectory, sourcetreebase );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CheckLogFile( void )
{
	if ( processor->GetLogFile() )
	{
		_unlink( "log.txt" );
		vprint( 0, "    Outputting to log.txt\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : argc - 
//			argv[] - 
// Output : int
//-----------------------------------------------------------------------------
int main( int argc, char* argv[] )
{
	SpewOutputFunc( SpewFunc );

	vprint( 0, "Valve Software - classcheck.exe (%s)\n", __DATE__ );
	vprint( 0, "--- Game Code Static Analysis ---\n" );
	char *pSpecificFile = NULL;
	bool bOutsideGamedir = false;

	int i = 1;
	for ( i ; i<argc ; i++)
	{
		if ( argv[ i ][ 0 ] == '-' )
		{
			switch( argv[ i ][ 1 ] )
			{
			case 'q':
				processor->SetQuiet( true );
				break;
			case 'h':
				processor->SetPrintHierarchy( true );
				break;
			case 'm':
				processor->SetPrintMembers( false );
				break;
			case 't':
				processor->SetPrintTDs( false );
				break;
			case 'p':
				processor->SetPrintPredTDs( false );
				break;
			case 'c':
				processor->SetPrintCreateMissingTDs( true );
				break;
			case 'x':
				processor->SetPrintCreateMissingPredTDs( true );
				break;
			case 'i':
				if (i < argc-1)
				{
					pSpecificFile = argv[i+1];
					++i;
				}
				break;
			case 'b':
				if (i < argc-1)
				{
					pSpecificFile = argv[i+1];
					bOutsideGamedir = true;
					++i;
				}
				break;
			case 'l':
				processor->SetLogFile( true );
				break;
			case 'j':
				processor->SetCheckHungarian( true );
				break;
			default:
				printusage();
				break;
			}
		}
	}

	if ( argc < 3 || ( i != argc ) )
	{
		printusage();
	}

	CheckLogFile();

	vprint( 0, "    Looking for obvious screwups and boneheaded mistakes...\n" );

	char sourcetreebase[ 256 ];
	strcpy( sourcetreebase, argv[i-2] );

	Q_StripTrailingSlash( sourcetreebase );

	if ( !pSpecificFile )
	{
		ProcessDirectory( argv[ i-1 ], sourcetreebase, "server",  "CBaseEntity" );
		ProcessDirectory( argv[ i-1 ], sourcetreebase, "client",  "C_BaseEntity" );
	}
	else
	{
		if ( bOutsideGamedir )
		{
			ProcessFile( argv[ i-1 ], sourcetreebase, "", "", pSpecificFile );
		}
		else
		{
			ProcessFile( argv[ i-1 ], sourcetreebase, "server",  "CBaseEntity", pSpecificFile );
			ProcessFile( argv[ i-1 ], sourcetreebase, "client",  "C_BaseEntity", pSpecificFile );
		}
	}

	return 0;
}
