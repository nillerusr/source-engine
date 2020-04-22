//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Clean dxsupport.csv by sorting and merging existing records
//          Can also parse survey php and merge new data into dxsupport.csv
//
//===============================================================================

#include "tier0/platform.h"
#include "tier0/progressbar.h"
#include "tier2/tier2.h"
#include "tier0/memdbgon.h"
#include "filesystem.h"
#include "icommandline.h"
#include "tier1/utlstringmap.h"
#include "tier1/strtools.h"
#include "tier1/utlmap.h"
#include "tier2/fileutils.h"
#include "stdlib.h"

#include "tier0/dbg.h"


#define MAX_RECORDS	128

struct DeviceRecord_t {
	char szName[256];
	char szData[MAX_RECORDS][64]; // No more than MAX_RECORDS columns of data in dxsupport.cfg
};

int g_nVendorIDIndex;
int g_nDeviceMinIDIndex;
int g_nDeviceMaxIDIndex;
int g_nNumDXSupportColumns = 0;
bool g_bMergeConflict = false;

static void GrabSurveyData( char const *pInputFilename, char const *pOutputFilename )
{
	CRequiredInputTextFile f( pInputFilename );
	COutputTextFile o( pOutputFilename );
	char linebuffer[4096], szDummy[32], szVendorID[32], szDeviceID[32], szPartName[256], goodLine[] = "$vend_dev_to_name[ MakeLookup(";

	while ( f.ReadLine( linebuffer, sizeof( linebuffer ) ) )
	{
		if ( !strncmp( linebuffer, goodLine, strlen( goodLine ) ) ) // specific pattern on the line we want
		{
			V_strncpy( szDummy, strtok( linebuffer, ",()\"" ), sizeof( szDummy ) );
			V_strncpy( szVendorID, strtok( NULL, ",()\"" ), sizeof( szVendorID ) );
			V_strncpy( szDeviceID, strtok( NULL, ",()\"" ), sizeof( szDeviceID ) );
			V_strncpy( szDummy, strtok( NULL, ",()\"" ), sizeof( szDummy ) );
			V_strncpy( szPartName, strtok( NULL, ",()\"" ), sizeof( szPartName ) );

			// Convert to hex from decimal
			sprintf( szVendorID, "%04x", atoi( szVendorID ) );
			sprintf( szDeviceID, "%04x", atoi( szDeviceID ) );

			// Example string:
			//	ATI Radeon X600,,,,,,,0x1002,0x3150,0x3150,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
			o.Write( szPartName, strlen( szPartName ) );
			o.Write( ",,,,,,,", 7 );
			o.Write( "0x", 2 );
			o.Write( szVendorID, strlen( szVendorID ) );
			o.Write( ",", 1 );
			o.Write( "0x", 2 );
			o.Write( szDeviceID, strlen( szDeviceID ) );
			o.Write( ",", 1 );
			o.Write( "0x", 2 );
			o.Write( szDeviceID, strlen( szDeviceID ) );
			o.Write( "\n", 1 );
		}
	}

	o.Close();
}

char *CopyTokenUntilComma( char *pOut, char *pIn )
{
	if ( !pIn || !pOut )
	{
		Assert( 0 );
		return NULL;
	}

	while ( ( *pIn != ',' ) && ( *pIn != '\0' ) && ( *pIn != '\n' ) )
	{
		*pOut++ = *pIn++;
	}

	*pOut = '\0';

	return ++pIn; // gobble the comma
}


void ParseDeviceRecord( char *pStr, DeviceRecord_t *deviceRecord )
{
	pStr = CopyTokenUntilComma( deviceRecord->szName, pStr );

	for ( int i=0; i<g_nNumDXSupportColumns; i++ )					// Null out the strings
	{
		V_strncpy( deviceRecord->szData[i], "", 64 );
	}

	int i = 0;														// Populate the strings
	while ( *pStr != '\0' )
	{
		pStr = CopyTokenUntilComma( deviceRecord->szData[i], pStr );
		i++;
	}
}

// Attempt to merge record nRecord+1 into nRecord
static bool DidMerge( CUtlVector<DeviceRecord_t> &deviceRecords, int nRecord )
{
	// If we have the same name and vendor, we are a merge candidate
	if ( !V_strnicmp( deviceRecords[nRecord].szName, deviceRecords[nRecord+1].szName, 256 ) &&
		 !V_strnicmp( deviceRecords[nRecord].szData[g_nVendorIDIndex], deviceRecords[nRecord+1].szData[g_nVendorIDIndex], 64 ) )
	{
		// Convert hex device ID strings into integers for comparisons
		int nMinIDCur, nMaxIDCur, nMinIDNext, nMaxIDNext;
		sscanf( deviceRecords[nRecord].szData[g_nDeviceMinIDIndex], "%x", &nMinIDCur );
		sscanf( deviceRecords[nRecord].szData[g_nDeviceMaxIDIndex], "%x", &nMaxIDCur );
		sscanf( deviceRecords[nRecord+1].szData[g_nDeviceMinIDIndex], "%x", &nMinIDNext );
		sscanf( deviceRecords[nRecord+1].szData[g_nDeviceMaxIDIndex], "%x", &nMaxIDNext );

		// If the ranges overlap or are adjacent, merge the data
		if ( ( nMinIDNext >= nMinIDCur ) && ( nMinIDNext <= (nMaxIDCur+1) ) )
		{
			// Set current max to max of the two ranges, merge other elements and flag next record for deletion
			V_snprintf( deviceRecords[nRecord].szData[g_nDeviceMaxIDIndex], 64, "0x%04x", MAX( nMaxIDNext, nMaxIDCur ) );

			for( int i=g_nDeviceMaxIDIndex+1; i < g_nNumDXSupportColumns; i++ )										// Run through elements after maxID
			{
				if ( strlen( deviceRecords[nRecord+1].szData[i] ) != 0 )											// If the next record has any data in this element
				{
					if ( V_strnicmp( deviceRecords[nRecord].szData[i], deviceRecords[nRecord+1].szData[i], 64 ) )	// If it doesn't match the current record's element
					{
						if ( strlen( deviceRecords[nRecord].szData[i] ) == 0 )										// If the current record has no data for this entry
						{
							V_strncpy( deviceRecords[nRecord].szData[i], deviceRecords[nRecord+1].szData[i], 64 );	// Just copy the merged record's data into current record
						}
						else // This is the interesting case, where we have a bona fide conflict
						{
							V_strncat( deviceRecords[nRecord].szData[i], " FIXME ", 64 );							// Mark the element with ***
							V_strncat( deviceRecords[nRecord].szData[i], deviceRecords[nRecord+1].szData[i], 64 );	// Concat the merged element in

							g_bMergeConflict = true;
						}
					}
				}
			}

			return true;
		}
	}

	return false;
}


// Look for duplicate adjacent entries (same vendorID and min/max, ignoring device name string)
static bool FoundDuplicate( CUtlVector<DeviceRecord_t> &deviceRecords, int nRecord )
{
	// Spew cases with same vendor ID and device min/maxes...these are likely the same device
	if ( !V_strnicmp( deviceRecords[nRecord].szData[g_nVendorIDIndex], deviceRecords[nRecord + 1].szData[g_nVendorIDIndex], 64 ) )
	{
		// Convert hex device ID strings into integers for comparisons
		int nMinIDCur, nMaxIDCur, nMinIDNext, nMaxIDNext;
		sscanf( deviceRecords[nRecord].szData[g_nDeviceMinIDIndex], "%x", &nMinIDCur );
		sscanf( deviceRecords[nRecord].szData[g_nDeviceMaxIDIndex], "%x", &nMaxIDCur );
		sscanf( deviceRecords[nRecord + 1].szData[g_nDeviceMinIDIndex], "%x", &nMinIDNext );
		sscanf( deviceRecords[nRecord + 1].szData[g_nDeviceMaxIDIndex], "%x", &nMaxIDNext );

		if ( ( nMinIDCur == nMinIDNext ) && ( nMaxIDCur == nMaxIDNext ) )
		{
			printf( "Duplicate: %s  %s  0x%04x  0x%04x\n", deviceRecords[nRecord].szName, deviceRecords[nRecord].szData[g_nVendorIDIndex], nMinIDCur, nMaxIDCur );

			return true;
		}
	}

	return false;
}


void ParseColumnHeadings( char *pHeadings )
{
	char *pToken = strtok( pHeadings, "," );
	int i = 0;

	while ( pToken )
	{
		if ( !V_strnicmp( pToken, "VendorID", 64 ) )
		{
			g_nVendorIDIndex = i-1;
		}
		else if ( !V_strnicmp( pToken, "MinDeviceID", 64 ) )
		{
			g_nDeviceMinIDIndex = i-1;
		}
		else if ( !V_strnicmp( pToken, "MaxDeviceID", 64 ) )
		{
			g_nDeviceMaxIDIndex = i-1;
		}

		pToken = strtok( NULL, "," );
		i++;
	}

	g_nNumDXSupportColumns = i-1;

	Assert( g_nNumDXSupportColumns <= MAX_RECORDS );
}



static void CleanCSVFile( char const *pInputFilename, char const *pOutputFilename )
{
	CRequiredInputTextFile f( pInputFilename );
	COutputTextFile o( pOutputFilename );
	char lineBuffer[4096];

	CUtlVector<DeviceRecord_t> deviceRecords;

	f.ReadLine( lineBuffer, sizeof( lineBuffer ) );	// Pass header line through
	o.Write( lineBuffer, strlen( lineBuffer ) );

	ParseColumnHeadings( lineBuffer );

	while ( f.ReadLine( lineBuffer, sizeof( lineBuffer ) ) )
	{
		if ( V_strlen( lineBuffer ) > 12 )	// Just make sure we have some data
		{
			DeviceRecord_t deviceRecord;
			ParseDeviceRecord( lineBuffer, &deviceRecord );

			// If we have a line with a real device, add it to the vector
			if ( ( strlen( deviceRecord.szData[g_nVendorIDIndex]    ) != 0 ) &&
				 ( strlen( deviceRecord.szData[g_nDeviceMinIDIndex] ) != 0 ) &&
				 ( strlen( deviceRecord.szData[g_nDeviceMaxIDIndex] ) != 0 ) )
			{
				deviceRecords.AddToTail( deviceRecord );
			}
			else
			{
				o.Write( lineBuffer, strlen( lineBuffer ) );
			}
		}
		else
		{
			o.Write( lineBuffer, strlen( lineBuffer ) ); // pass short lines through
		}
	}

	// At this point, the output file has the header block from the file and we have records for all of the real device lines

	///////////////////////////////////////////////////////////////////////////////////////////////
	//
	// Merge "adjacent" instances of the same device
	//
	int i = 0;
	while( i < deviceRecords.Count() - 1 )			// March through devices, merging
	{
		if ( DidMerge( deviceRecords, i ) )			// Did i+1 get merged into i?
		{
			deviceRecords.Remove( i+1 );			// Remove i+1 and check with the same i again
			continue; // don't increment
		}

		i++;										// Move to next record
	}

	///////////////////////////////////////////////////////////////////////////////////////////////
	//
	// Remove duplicate devices (based on deviceID, not name)
	//
	i = 0;
	while ( i < deviceRecords.Count() - 1 )			// March through devices, looking for duplicates
	{
		if ( FoundDuplicate( deviceRecords, i ) )	// Does i+1 look like a duplicate of i?
		{
			deviceRecords.Remove( i + 1 );			// Remove i+1 and check with the same i again
			continue; // don't increment
		}

		i++;										// Move to next record
	}

	// Clean up the hex device ID formatting (pad to 0x1234 with leading zeroes)
	for ( int i = 0; i< deviceRecords.Count(); i++ )
	{
		int nDummy;
		sscanf( deviceRecords[i].szData[g_nDeviceMinIDIndex], "%x", &nDummy );
		V_snprintf( deviceRecords[i].szData[g_nDeviceMinIDIndex], 64, "0x%04x", nDummy );

		sscanf( deviceRecords[i].szData[g_nDeviceMaxIDIndex], "%x", &nDummy );
		V_snprintf( deviceRecords[i].szData[g_nDeviceMaxIDIndex], 64, "0x%04x", nDummy );
	}

	// Just spray the devices out
	for ( int i = 0; i< deviceRecords.Count(); i++ )
	{
		o.Write( deviceRecords[i].szName, strlen( deviceRecords[i].szName ) );
		for ( int j=0; j<g_nNumDXSupportColumns; j++ )
		{
			o.Write( ",", 1 );
			o.Write( deviceRecords[i].szData[j], strlen( deviceRecords[i].szData[j] ) );
		}

		o.Write( "\n", 1 );
	}
}


void main(int argc,char **argv)
{
	InitCommandLineProgram( argc, argv );
	if ( argc != 5 )
	{
		printf( "\n" );
		printf( "  dxsupportclean output depends on the input file type (.csv or not).\n" );
		printf( "  The output is always a CSV file.\n\n" );
		printf( "   usage: dxsupportclean -i <inputfile> -o <outputfile.csv>\n" );
		printf( "          If the input file is not csv, dxsupportclean expects hardware\n" );
		printf( "          survey php syntax.  If the input file is csv, dxsupportclean expects\n" );
		printf( "          the first line to contain specific header columns.\n" );
		printf( "          If you want to provide some other type of input or if you modify the\n" );
		printf( "          header columns in dxsupport, you'll have to modify dxsupportclean.\n" );
		return;
	}

	const char *pInputFile = CommandLine()->ParmValue( "-i" );
	const char *pOutputFile = CommandLine()->ParmValue( "-o" );

	// Can't input and output the same file
	if ( !V_strnicmp( pInputFile, pOutputFile, MAX_PATH ) )
	{
		printf( "Input and out files must not be the same file!  No output produced.\n" );
		return;
	}

	if ( !stricmp( "csv", Q_GetFileExtension( pInputFile ) ) )
	{
		CleanCSVFile( pInputFile, pOutputFile );

		if ( g_bMergeConflict )
		{
			printf( "\n*************************************************************************\n" );
			printf( " Merge Conflict detected.  Search output for FIXME and resolve manually!\n" );
			printf( "*************************************************************************\n\n" );
		}
	}
	else
	{
		GrabSurveyData( pInputFile, pOutputFile );
	}
}
