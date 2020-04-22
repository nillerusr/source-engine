//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// tagbuild.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <process.h>
#include <string.h>
#include "interface.h"
#include "imysqlwrapper.h"

const char *search = "VLV_INTERNAL                    ";

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void printusage( void )
{
	printf( "usage:  tagbuild engine.dll buildid <hostname database username password>\n" );

	// Exit app
	exit( 1 );
}

int main(int argc, char* argv[])
{
	if ( argc != 3 && argc != 7 )
	{
		printusage();
	}

	int idlen = strlen( argv[ 2 ] );

	if ( idlen > 32 )
	{
		printf( "id string '%s' is %i long, 32 is max\n", argv[ 2 ], idlen );
		printusage();
	}

	// Open for reading and writing
	FILE *fp = fopen( argv[ 1 ], "r+" );
	if ( !fp )
	{
		printf( "unable to open %s\n", argv[ 1 ] );
		exit( 1 );
	}

	fseek( fp, 0, SEEK_END );
	int size = ftell( fp );
	fseek( fp, 0, SEEK_SET );

	int searchlen = strlen( search );

	char alreadytagged[ 32 ];
	Q_memset( alreadytagged, ' ', sizeof( alreadytagged ) );
	alreadytagged[ 31 ] = 0;
	// Copies in the id, but doesn't copy the terminating '\0'
	Q_memcpy( alreadytagged, argv[ 2 ], Q_strlen( argv[ 2 ] ) );

	bool found = false;
	char *buf = new char[ size ];
	if ( buf )
	{
		fread( buf, size, 1, fp );

		// Now search
		char *start = buf;
		while ( start - buf < ( size - searchlen ) )
		{
			if ( !memcmp( start, search, searchlen - 1 ) )
			{
				int pos = start - buf;

				printf( "Found placeholder at %i, writing '%s' into file\n",
					pos, argv[ 2 ] );

				fseek( fp, pos, SEEK_SET );
				fwrite( argv[ 2 ], idlen, 1, fp );
				while ( idlen < searchlen )
				{
					fputc( ' ', fp );
					++idlen;
				}

				found = true;
				break;
			}
			else if ( !memcmp( start, alreadytagged, searchlen - 1 ) )
			{
				int pos = start - buf;

				printf( "Found tag at %i (%s)\n", pos, argv[ 2 ] );
				found = true;
				break;
			}

			++start;
		}

		if ( !found )
		{
			printf( "Couldn't find search string '%s' in binary data\n", search );
		}

	}
	else
	{
		printf( "unable to allocate %i bytes for %s\n", size, argv[ 1 ] );
	}


	fclose( fp );

	if ( argc <= 3 )
	{
		printf( "Skipping database insertion\n" );
	}
	else
	{
		// Now connect to steamweb and update the engineaccess table
		CSysModule *sql = Sys_LoadModule( "mysql_wrapper" );
		if ( sql )
		{
			CreateInterfaceFn factory = Sys_GetFactory( sql );
			if ( factory )
			{
				IMySQL *mysql = ( IMySQL * )factory( MYSQL_WRAPPER_VERSION_NAME, NULL );
				if ( mysql )
				{
					if ( mysql->InitMySQL( argv[ 4 ], argv[ 3 ], argv[ 5 ], argv[ 6 ] ) )
					{
						char q[ 512 ];

						Q_snprintf( q, sizeof( q ), "insert into engineaccess (BuildIdentifier,AllowAccess) values (\"%s\",1);",
							argv[2] );

						int retcode = mysql->Execute( q );
						if ( retcode != 0 )
						{
							printf( "Query %s failed\n", q );
						}
						else
						{
							printf( "Successfully added buildidentifier '%s' to %s:%s\n",
								argv[ 2 ], argv[ 3 ], argv[ 4 ] );
						}
					}
					else
					{
						printf( "InitMySQL failed\n" );
					}

					mysql->Release();
				}
				else
				{
					printf( "Unable to connect via mysql_wrapper\n");
				}
			}
			else
			{
				printf( "Unable to get factory from mysql_wrapper.dll, not updating access mysql table!!!" );
			}

			Sys_UnloadModule( sql );
		}
		else
		{
			printf( "Unable to load mysql_wrapper.dll, not updating access mysql table!!!" );
		}
	}

	return 0;
}
