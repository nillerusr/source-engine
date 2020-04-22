//========= Copyright Valve Corporation, All rights reserved. ============//


#include "crccheck_shared.h"
#include "tier1/checksum_crc.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#ifdef _WIN32
#include <process.h>
#else
#include <stdlib.h>
#define stricmp strcasecmp
#endif

#pragma warning( disable : 4996 )
#pragma warning( disable : 4127 )

#define MAX_INCLUDE_STACK_DEPTH 10
#ifdef POSIX
#define _vsnprintf vsnprintf
#endif

//-----------------------------------------------------------------------------
//	Sys_Error
//
//-----------------------------------------------------------------------------
void Sys_Error( const char* format, ... )
{
	va_list argptr;
		
	va_start( argptr,format );
	vfprintf( stderr, format, argptr );
	va_end( argptr );

	exit( 1 );
}


void SafeSnprintf( char *pOut, int nOutLen, const char *pFormat, ... )
{
	va_list marker;
	va_start( marker, pFormat );
	_vsnprintf( pOut, nOutLen, pFormat, marker );
	va_end( marker );

	pOut[nOutLen-1] = 0;
}


// for linked lists of strings
struct StringNode_t 
{
	StringNode_t *m_pNext;
	char m_Text[1];											// the string data
};


static StringNode_t *MakeStrNode( char const *pStr )
{
	size_t nLen = strlen( pStr );
	StringNode_t *nRet = ( StringNode_t * ) new unsigned char[sizeof( StringNode_t ) + nLen ];
	strcpy( nRet->m_Text, pStr );
	return nRet;
}

//-----------------------------------------------------------------------------
//	Sys_LoadFile
//-----------------------------------------------------------------------------
int Sys_LoadTextFileWithIncludes( const char* filename, char** bufferptr )
{
	FILE *pFileStack[MAX_INCLUDE_STACK_DEPTH];
	int nSP = MAX_INCLUDE_STACK_DEPTH;
	
	StringNode_t *pFileLines = NULL;		// tail ptr for fast adds
	
	size_t nTotalFileBytes = 0;
	FILE *handle = fopen( filename, "r" );
	if ( !handle )
		return -1;

	pFileStack[--nSP] = handle;								// push
	while ( nSP < MAX_INCLUDE_STACK_DEPTH )
	{
		// read lines
		for (;;)
		{
			char lineBuffer[2048];
			char *ln = fgets( lineBuffer, sizeof( lineBuffer ), pFileStack[nSP] );
			if ( !ln ) 
				break;										// out of text
			
			ln += strspn( ln, "\t " );						// skip white space
			if ( memcmp( ln, "#include", 8 ) == 0 )
			{
				// omg, an include
				ln += 8;
				ln += strspn( ln, " \t\"<" );				// skip whitespace, ", and <
				
				size_t nPathNameLength = strcspn( ln, " \t\">\n" );
				if ( !nPathNameLength )
				{
					Sys_Error( "bad include %s via %s\n", lineBuffer, filename );
				}
				ln[nPathNameLength] = 0;					// kill everything after end of filename
				
				FILE *inchandle = fopen( ln, "r" );
				if ( !inchandle )
				{
					Sys_Error( "can't open #include of %s\n", ln );
				}
				if ( !nSP )
				{
					Sys_Error( "include nesting too deep via %s", filename );
				}
				pFileStack[--nSP] = inchandle;
			}
			else
			{
				size_t nLen = strlen( ln );
				nTotalFileBytes += nLen;
				StringNode_t *pNewLine = MakeStrNode( ln );

				pNewLine->m_pNext = pFileLines;
				pFileLines = pNewLine;
			}
		}
		fclose( pFileStack[nSP] );
		nSP++;												// pop stack
	}
	
	
	// Reverse the pFileLines list so it goes the right way.
	StringNode_t *pPrev = NULL;
	StringNode_t *pCur;
	for( pCur = pFileLines; pCur; )
	{
		StringNode_t *pNext = pCur->m_pNext;
		pCur->m_pNext = pPrev;
		pPrev = pCur;
		pCur = pNext;
	}
	pFileLines = pPrev;


	// Now dump all the lines out into a single buffer.
	char *buffer = new char[nTotalFileBytes + 1];			// and null
	*bufferptr = buffer;									// tell caller

	// copy all strings and null terminate
	int nLine = 0;
	StringNode_t *pNext;
	for( pCur=pFileLines; pCur; pCur=pNext )
	{
		pNext = pCur->m_pNext;
		size_t nLen = strlen( pCur->m_Text );
		memcpy( buffer, pCur->m_Text, nLen );
		buffer += nLen;
		nLine++;
		
		// Cleanup the line..
		//delete [] (unsigned char*)pCur;
	}
	*( buffer++ ) = 0;										// null

	return (int)nTotalFileBytes;
}


// Just like fgets() but it removes trailing newlines.
char* ChompLineFromFile( char *pOut, int nOutBytes, FILE *fp )
{
	char *pReturn = fgets( pOut, nOutBytes, fp );
	if ( pReturn )
	{
		int len = (int)strlen( pReturn );
		if ( len > 0 && pReturn[len-1] == '\n' )
		{
			pReturn[len-1] = 0;
			if ( len > 1 && pReturn[len-2] == '\r' )
				pReturn[len-2] = 0;
		}
	}

	return pReturn;
}


bool CheckSupplementalString( const char *pSupplementalString, const char *pReferenceSupplementalString )
{
	// The supplemental string is only checked while VPC is determining if a project file is stale or not. 
	// It's not used by the pre-build event's CRC check.
	// The supplemental string contains various options that tell how the project was built. It's generated in VPC_GenerateCRCOptionString.
	//
	// If there's no reference supplemental string (which is the case if we're running vpccrccheck.exe), then we ignore it and continue.
	if ( !pReferenceSupplementalString )
		return true;
	
	return ( pSupplementalString && pReferenceSupplementalString && stricmp( pSupplementalString, pReferenceSupplementalString ) == 0 );
}


bool VPC_CheckProjectDependencyCRCs( const char *pProjectFilename, const char *pReferenceSupplementalString, char *pErrorString, int nErrorStringLength )
{
	// Build the xxxxx.vcproj.vpc_crc filename
	char szFilename[512];
	SafeSnprintf( szFilename, sizeof( szFilename ), "%s.%s", pProjectFilename, VPCCRCCHECK_FILE_EXTENSION );
	
	// Open it up.
	FILE *fp = fopen( szFilename, "rt" );
	if ( !fp )
	{
		SafeSnprintf( pErrorString, nErrorStringLength, "Unable to load %s to check CRC strings", szFilename );
		return false;
	}

	bool bReturnValue = false;
	char lineBuffer[2048];

	// Check the version of the CRC file.
	const char *pVersionString = ChompLineFromFile( lineBuffer, sizeof( lineBuffer ), fp );
	if ( pVersionString && stricmp( pVersionString, VPCCRCCHECK_FILE_VERSION_STRING ) == 0 )
	{
		// Check the supplemental CRC string.
		const char *pSupplementalString = ChompLineFromFile( lineBuffer, sizeof( lineBuffer ), fp );
		if ( CheckSupplementalString( pSupplementalString, pReferenceSupplementalString ) )
		{
			// Now read each line. Each line has a CRC and a filename on it.
			while ( 1 )
			{
				char *pLine = ChompLineFromFile( lineBuffer, sizeof( lineBuffer ), fp );
				if ( !pLine )
				{
					// We got all the way through the file without a CRC error, so all's well.
					bReturnValue = true;
					break;
				}

				char *pSpace = strchr( pLine, ' ' );
				if ( !pSpace )
				{
					SafeSnprintf( pErrorString, nErrorStringLength, "Invalid line ('%s') in %s", pLine, szFilename );
					break;
				}

				// Null-terminate it so we have the CRC by itself and the filename follows the space.
				*pSpace = 0;
				const char *pVPCFilename = pSpace + 1;
			
				// Parse the CRC out.
				unsigned int nReferenceCRC;
				sscanf( pLine, "%x", &nReferenceCRC );

				
				// Calculate the CRC from the contents of the file.
				char *pBuffer;
				int nTotalFileBytes = Sys_LoadTextFileWithIncludes( pVPCFilename, &pBuffer );
				if ( nTotalFileBytes == -1 )
				{
					SafeSnprintf( pErrorString, nErrorStringLength, "Unable to load %s for CRC comparison.", pVPCFilename );
					break;
				}

				CRC32_t nCRCFromTextContents = CRC32_ProcessSingleBuffer( pBuffer, nTotalFileBytes );
				delete [] pBuffer;
				
				// Compare them.
				if ( nCRCFromTextContents != nReferenceCRC )
				{
					SafeSnprintf( pErrorString, nErrorStringLength, "This VCPROJ is out of sync with its VPC scripts.\n  %s mismatches (0x%x vs 0x%x).\n  Please use VPC to re-generate!\n  \n", pVPCFilename, nReferenceCRC, nCRCFromTextContents );
					break;
				}
			}
		}
		else
		{
			SafeSnprintf( pErrorString, nErrorStringLength, "Supplemental string mismatch." );
		}
	}
	else
	{
		SafeSnprintf( pErrorString, nErrorStringLength, "CRC file %s has an invalid version string ('%s')", szFilename, pVersionString ? pVersionString : "[null]" );
	}

	fclose( fp );
	return bReturnValue;
}


int VPC_OldeStyleCRCChecks( int argc, char **argv )
{
	for ( int i=1; (i+2) < argc; )
	{
		const char *pTestArg = argv[i];
		if ( stricmp( pTestArg, "-crc" ) != 0 )
		{
			++i;
			continue;
		}

		const char *pVPCFilename = argv[i+1];
		
		// Get the CRC value on the command line.
		const char *pTestCRC = argv[i+2];
		unsigned int nCRCFromCommandLine;
		sscanf( pTestCRC, "%x", &nCRCFromCommandLine );

		// Calculate the CRC from the contents of the file.
		char *pBuffer;
		int nTotalFileBytes = Sys_LoadTextFileWithIncludes( pVPCFilename, &pBuffer );
		if ( nTotalFileBytes == -1 )
		{
			Sys_Error( "Unable to load %s for CRC comparison.", pVPCFilename );
		}

		CRC32_t nCRCFromTextContents = CRC32_ProcessSingleBuffer( pBuffer, nTotalFileBytes );
		delete [] pBuffer;
		
		// Compare them.
		if ( nCRCFromTextContents != nCRCFromCommandLine )
		{
			Sys_Error( "  \n  This VCPROJ is out of sync with its VPC scripts.\n  %s mismatches (0x%x vs 0x%x).\n  Please use VPC to re-generate!\n  \n", pVPCFilename, nCRCFromCommandLine, nCRCFromTextContents );
		}

		i += 2;
	}

	return 0;
}


int VPC_CommandLineCRCChecks( int argc, char **argv )
{
	if ( argc < 2 )
	{
		fprintf( stderr, "Invalid arguments to " VPCCRCCHECK_EXE_FILENAME ". Format: " VPCCRCCHECK_EXE_FILENAME " [project filename]\n" );
		return 1;
	}

	const char *pFirstCRC = argv[1];

	// If the first argument starts with -crc but is not -crc2, then this is an old CRC check command line with all the CRCs and filenames
	// directly on the command line. The new format puts all that in a separate file.
	if ( pFirstCRC[0] == '-' && pFirstCRC[1] == 'c' && pFirstCRC[2] == 'r' && pFirstCRC[3] == 'c' && pFirstCRC[4] != '2' )
	{
		return VPC_OldeStyleCRCChecks( argc, argv );
	}

	if ( stricmp( pFirstCRC, "-crc2" ) != 0 )
	{
		fprintf( stderr, "Missing -crc2 parameter on vpc CRC check command line." );
		return 1;
	}

	const char *pProjectFilename = argv[2];

	char errorString[1024];
	bool bCRCsValid = VPC_CheckProjectDependencyCRCs( pProjectFilename, NULL, errorString, sizeof( errorString ) );

	if ( bCRCsValid )
	{
		return 0;
	}
	else
	{
		fprintf( stderr, "%s", errorString );
		return 1;
	}
}

