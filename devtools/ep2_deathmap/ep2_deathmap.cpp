//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include <stdio.h>
#include <process.h>
#include <string.h>
#include <windows.h>
#include <sys/stat.h>
#include <time.h>


#include "interface.h"
#include "imysqlwrapper.h"
#include "tier1/utlvector.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlstring.h"
#include "tier1/utldict.h"
#include "KeyValues.h"
#include "filesystem_helpers.h"
#include "tier2/tier2.h"
#include "filesystem.h"
#include "interface.h"
#include "vstdlib/random.h"
#include "jpeglib/jpeglib.h"


static char gamename[ 32 ] = "ep2";

extern IFileSystem *g_pFullFileSystem;

struct Image_t
{
	byte *data;
	int w, h;
};

bool ReadBitmapRGB( const byte *raw, size_t rawlen, Image_t *image )
{
	assert( image );

	CUtlBuffer buf;
	buf.Put( raw, rawlen );

	//	int i;
	BITMAPFILEHEADER bmfh;
	BITMAPINFOHEADER bmih;
	int cbBmpBits;
	byte *pb;

	// Read file header
	buf.Get( &bmfh, sizeof( bmfh ) );

	// Read info header
	buf.Get( &bmih, sizeof( bmih ) );

	// Bogus info header check
	if (!(bmih.biSize == sizeof( bmih ) && bmih.biPlanes == 1))
		return false;

	// Bogus bit depth?  Only 8-bit supported.
	if (bmih.biBitCount != 24)
		return false;

	// Bogus compression?  Only non-compressed supported.
	if (bmih.biCompression != BI_RGB )
		return false;

	image->w = bmih.biWidth;
	image->h = bmih.biHeight;

	// Read bitmap bits (remainder of file)
	cbBmpBits = bmfh.bfSize - buf.TellGet();
	assert( cbBmpBits == ( image->w * image->h * 3 ) );

	int cbTotalbytes = cbBmpBits;

	pb = new byte[ cbBmpBits ];
	if (pb == 0)
	{
		return false;
	}

	buf.Get( pb, cbBmpBits );

	byte *start = pb;
	image->data = new byte[ cbTotalbytes ];

	int bitrueWidth = (bmih.biWidth + 3) & ~3;

	byte *dst = image->data;
	int x, y;
	for ( y = 0; y < image->h; ++y )
	{
		int rowstart = 3 * ( image->h - 1 - y ) * bitrueWidth;
		byte *row = &start[ rowstart ];

		for ( x = 0; x < image->w; ++x )
		{
#define USE_GRAYSCALE
#if defined( USE_GRAYSCALE )
			int val = row[ 0 ] + row[ 1 ] + row[ 2 ];
			val /= 3;
			*dst++ = val;
			*dst++ = val;
			*dst++ = val;

			row += 3;
#else
			*dst++ = *row++;
			*dst++ = *row++;
			*dst++ = *row++;
#endif
		}
	}

	delete[] start;

	return true;
}

void ParseBugs( CUtlDict< CUtlVector< Vector > *, int >& list, char const *filename )
{
	FileHandle_t fh = g_pFullFileSystem->Open( filename, "rb" );
	if ( FILESYSTEM_INVALID_HANDLE == fh )
		return;
		
	size_t size = g_pFullFileSystem->Size( fh );

	char *buffer = new char[ size + 1 ];
	g_pFullFileSystem->Read( buffer, size, fh );
	buffer[ size ] = 0;

	g_pFullFileSystem->Close( fh );

	char *data = buffer;
	while ( 1 )
	{
		// Now parse out the data
		// First find level:  
		char *p = strstr( data, "level:  " );
		if ( !p )
			break;

		char mapname[ 512 ];
		char *i = p + 8;
		char *o = mapname;
		while ( *i && *i != ' ' && *i != '\r' && *i != '\n' )
			*o++ = *i++;
		*o = 0;

		data += 8;

		// Now find the position
		p = strstr( data, "setpos " );
		if ( !p )
			break;

		char position[ 512 ];
		i = p + 7;
		o = position;
		while ( *i && *i != ';' )
			*o++ = *i++;
		*o = 0;

		data += 7;

		Vector pos;
		if ( 3 == sscanf( position, "%f %f %f", &pos.x, &pos.y, &pos.z ) )
		{
			int idx = list.Find( mapname );
			if ( idx == list.InvalidIndex() )
			{
				idx = list.Insert( mapname, new CUtlVector< Vector > );
			}

			CUtlVector< Vector > *db = list[ idx ];
			db->AddToTail( pos );
		}
		// printf( "map: %s pos %s\n", mapname, position );
	}

	delete[] buffer;

	return;
}

bool WriteBitmap( char const *filename, Image_t *image )
{
	FileHandle_t fh = g_pFullFileSystem->Open( filename, "wb" );
	if ( FILESYSTEM_INVALID_HANDLE == fh )
		return false;

	BITMAPFILEHEADER hdr = { 0 };
	BITMAPINFOHEADER bi = { 0 };

	int w = image->w;
	int h = image->h;

	int imageSize = image->w * image->h * 3;

	// file header
	hdr.bfType = 0x4d42;	// 'BM'
	hdr.bfSize = (DWORD) ( sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + imageSize );
	hdr.bfReserved1 = 0;
	hdr.bfReserved2 = 0;
	hdr.bfOffBits = (DWORD) ( sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) );

	g_pFullFileSystem->Write( (LPVOID)&hdr, sizeof(BITMAPFILEHEADER), fh );

	// bitmap header
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = image->w;
	bi.biHeight = image->h;
	bi.biPlanes = 1;
	bi.biBitCount = 24;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;	//vid.rowbytes * vid.height;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	g_pFullFileSystem->Write( (LPVOID)&bi, sizeof(BITMAPINFOHEADER), fh );
	// bitmap bits
	byte *hp = new byte[ imageSize ];
	Q_memcpy( hp, image->data, imageSize );

	byte b;
	int i;

	// Invert vertically for BMP format
	for ( i = 0; i < h / 2; i++ )
	{
		byte *top = hp + i * w * 3;
		byte *bottom = hp + (h - i - 1) * w * 3;
		for (int j = 0; j < w * 3; j++)
		{
			b = *top;
			*top = *bottom;
			*bottom = b;
			top++;
			bottom++;
		}
	}

	g_pFullFileSystem->Write( (LPVOID)hp, imageSize, fh );

	delete[] hp;

	// clean up
	g_pFullFileSystem->Close( fh );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Expanded data destination object for CUtlBuffer output
//-----------------------------------------------------------------------------
struct JPEGDestinationManager_t
{
	struct jpeg_destination_mgr pub; // public fields

	CUtlBuffer	*pBuffer;		// target/final buffer
	byte		*buffer;		// start of temp buffer
};

// choose an efficiently bufferaable size
#define OUTPUT_BUF_SIZE  4096	

//-----------------------------------------------------------------------------
// Purpose:  Initialize destination --- called by jpeg_start_compress
//  before any data is actually written.
//-----------------------------------------------------------------------------
METHODDEF(void) init_destination (j_compress_ptr cinfo)
{
	JPEGDestinationManager_t *dest = ( JPEGDestinationManager_t *) cinfo->dest;

	// Allocate the output buffer --- it will be released when done with image
	dest->buffer = (byte *)
		(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
		OUTPUT_BUF_SIZE * sizeof(byte));

	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

//-----------------------------------------------------------------------------
// Purpose: Empty the output buffer --- called whenever buffer fills up.
// Input  : boolean - 
//-----------------------------------------------------------------------------
METHODDEF(boolean) empty_output_buffer (j_compress_ptr cinfo)
{
	JPEGDestinationManager_t *dest = ( JPEGDestinationManager_t * ) cinfo->dest;

	CUtlBuffer *buf = dest->pBuffer;

	// Add some data
	buf->Put( dest->buffer, OUTPUT_BUF_SIZE );

	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: Terminate destination --- called by jpeg_finish_compress
// after all data has been written.  Usually needs to flush buffer.
//
// NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
// application must deal with any cleanup that should happen even
// for error exit.
//-----------------------------------------------------------------------------
METHODDEF(void) term_destination (j_compress_ptr cinfo)
{
	JPEGDestinationManager_t *dest = (JPEGDestinationManager_t *) cinfo->dest;
	size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

	CUtlBuffer *buf = dest->pBuffer;

	/* Write any data remaining in the buffer */
	if (datacount > 0) 
	{
		buf->Put( dest->buffer, datacount );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set up functions for writing data to a CUtlBuffer instead of FILE *
//-----------------------------------------------------------------------------
GLOBAL(void) jpeg_UtlBuffer_dest (j_compress_ptr cinfo, CUtlBuffer *pBuffer )
{
	JPEGDestinationManager_t *dest;

	/* The destination object is made permanent so that multiple JPEG images
	* can be written to the same file without re-executing jpeg_stdio_dest.
	* This makes it dangerous to use this manager and a different destination
	* manager serially with the same JPEG object, because their private object
	* sizes may be different.  Caveat programmer.
	*/
	if (cinfo->dest == NULL) {	/* first time for this JPEG object? */
		cinfo->dest = (struct jpeg_destination_mgr *)
			(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
			sizeof(JPEGDestinationManager_t));
	}

	dest = ( JPEGDestinationManager_t * ) cinfo->dest;

	dest->pub.init_destination		= init_destination;
	dest->pub.empty_output_buffer	= empty_output_buffer;
	dest->pub.term_destination		= term_destination;
	dest->pBuffer					= pBuffer;
}

bool ImageToJPEGBuffer( Image_t *image, CUtlBuffer& buf, int quality )
{
#if !defined( _X360 )

	// Validate quality level
	quality = clamp( quality, 1, 100 );

	int imagew = image->w;
	int imageh = image->h;

	// Allocate space for bits
	uint8 *pImage = new uint8[ imagew * 3 * imageh];
	if ( !pImage )
	{
		Msg( "Unable to allocate %i bytes for image\n", imagew * 3 * imageh );
		return false;
	}

	// Get Bits from the material system
	// ReadScreenPixels( 0, 0, GetModeWidth(), GetModeHeight(), pImage, IMAGE_FORMAT_RGB888 );
	// Copy data in
	for ( int y = 0; y < imageh; ++y )
	{
		byte *row = (byte *)&pImage[ y * imagew * 3 ];
		const byte *pSrc = (const byte *)&image->data[ y * imagew * 3 ];
		for ( int x = 0; x < imagew; ++x )
		{
			// BGR to RGB
			row[ 0 ] = pSrc[ 2 ];
			row[ 1 ] = pSrc[ 1 ];
			row[ 2 ] = pSrc[ 0 ];

			pSrc += 3;
			row += 3;
		}
	}

	JSAMPROW row_pointer[1];     // pointer to JSAMPLE row[s]
	int row_stride;              // physical row width in image buffer

	// stderr handler
	struct jpeg_error_mgr jerr;

	// compression data structure
	struct jpeg_compress_struct cinfo;

	row_stride = imagew * 3; // JSAMPLEs per row in image_buffer

	// point at stderr
	cinfo.err = jpeg_std_error(&jerr);

	// create compressor
	jpeg_create_compress(&cinfo);

	// Hook CUtlBuffer to compression
	jpeg_UtlBuffer_dest(&cinfo, &buf );

	// image width and height, in pixels
	cinfo.image_width = imagew;
	cinfo.image_height = imageh;
	// RGB is 3 componnent
	cinfo.input_components = 3;
	// # of color components per pixel
	cinfo.in_color_space = JCS_RGB;

	// Apply settings
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE );

	// Start compressor
	jpeg_start_compress(&cinfo, TRUE);

	// Write scanlines
	while ( cinfo.next_scanline < cinfo.image_height ) 
	{
		row_pointer[ 0 ] = &pImage[ cinfo.next_scanline * row_stride ];
		jpeg_write_scanlines( &cinfo, row_pointer, 1 );
	}

	// Finalize image
	jpeg_finish_compress(&cinfo);

	// Cleanup
	jpeg_destroy_compress(&cinfo);

	delete[] pImage;

#else
	// not supporting
	Assert( 0 );
#endif
	return true;
}

bool WriteJPeg( char const *filename, Image_t *image )
{
	FileHandle_t fh = g_pFullFileSystem->Open( filename, "wb" );
	if ( FILESYSTEM_INVALID_HANDLE == fh )
		return false;

	CUtlBuffer buf;
	ImageToJPEGBuffer( image, buf, 90 );

	g_pFullFileSystem->Write( buf.Base(), buf.TellPut(), fh );
	// clean up
	g_pFullFileSystem->Close( fh );

	return true;
}

#include <string>
//-------------------------------------------------
void v_escape_string (std::string& s)
{
	if ( !s.size() ) 
		return;
	for ( unsigned int i = 0;i<s.size();i++ )
	{
		switch (s[i]) 
		{
		case '\0':				/* Must be escaped for "mysql" */
			s[i] = '\\';
			s.insert(i+1,"0",1); i++;//lint !e534
			break;
		case '\n':				/* Must be escaped for logs */
			s[i] = '\\';
			s.insert(i+1,"n",1); i++;//lint !e534
			break;
		case '\r':
			s[i] = '\\';
			s.insert(i+1,"r",1); i++;//lint !e534
			break;
		case '\\':
			s[i] = '\\';
			s.insert(i+1,"\\",1); i++;//lint !e534
			break;
		case '\"':
			s[i] = '\\';
			s.insert(i+1,"\"",1); i++;//lint !e534
			break;
		case '\'':				/* Better safe than sorry */
			s[i] = '\\';
			s.insert(i+1,"\'",1); i++;//lint !e534
			break;
		case '\032':			/* This gives problems on Win32 */
			s[i] = '\\';
			s.insert(i+1,"Z",1); i++;//lint !e534
			break;
		default: 
			break;
		}
	}
}

void HSLToRGB( Color &out, float *hsl )
{
	float sat[3],ctmp[3];
	out[ 3 ] = 255;

	while (hsl[ 0 ] < 0)
		hsl[ 0 ] += 360;
	while (hsl[ 0 ] > 360)
		hsl[ 0 ] -= 360;

	if (hsl[ 0 ] < 120) 
	{
		sat[ 0 ] = (120 - hsl[ 0 ]) / 60.0;
		sat[ 1 ] = hsl[ 0 ] / 60.0;
		sat[ 2 ] = 0;
	} 
	else if (hsl[ 0 ] < 240) 
	{
		sat[ 0 ] = 0;
		sat[ 1 ] = (240 - hsl[ 0 ]) / 60.0;
		sat[ 2 ] = (hsl[ 0 ] - 120) / 60.0;
	} else 
	{
		sat[ 0 ] = (hsl[ 0 ] - 240) / 60.0;
		sat[ 1 ] = 0;
		sat[ 2 ] = (360 - hsl[ 0 ]) / 60.0;
	}
	sat[ 0 ] = min(sat[ 0 ],1.f);
	sat[ 1 ] = min(sat[ 1 ],1.f);
	sat[ 2 ] = min(sat[ 2 ],1.f);

	ctmp[ 0 ] = 2 * hsl[ 1 ] * sat[ 0 ] + (1 - hsl[ 1 ]);
	ctmp[ 1 ] = 2 * hsl[ 1 ] * sat[ 1 ] + (1 - hsl[ 1 ]);
	ctmp[ 2 ] = 2 * hsl[ 1 ] * sat[ 2 ] + (1 - hsl[ 1 ]);

	if (hsl[ 2 ] < 0.5) 
	{
		out[ 0 ] = 255.0f * hsl[ 2 ] * ctmp[ 0 ];
		out[ 1 ] = 255.0f * hsl[ 2 ] * ctmp[ 1 ];
		out[ 2 ] = 255.0f * hsl[ 2 ] * ctmp[ 2 ];
	}
	else 
	{
		out[ 0 ] = 255.0f * ( (1 - hsl[ 2 ]) * ctmp[ 0 ] + 2 * hsl[ 2 ] - 1 );
		out[ 1 ] = 255.0f * ( (1 - hsl[ 2 ]) * ctmp[ 1 ] + 2 * hsl[ 2 ] - 1 );
		out[ 2 ] = 255.0f * ( (1 - hsl[ 2 ]) * ctmp[ 2 ] + 2 * hsl[ 2 ] - 1 );
	}

}

inline void DrawColoredRect( Image_t *image, int x, int y, int w, int h, const Color &rgb, float flAlpha )
{
	flAlpha = clamp( flAlpha, 0.0f, 1.0f );
	float flOneMinusAlpha = 1.0f - flAlpha;

	for ( int xx = x; xx < x + w; ++xx )
	{
		if ( xx < 0 || xx >= image->w )
			continue;

		for ( int yy = y; yy < y + h; ++yy )
		{
			if ( yy < 0 || yy >= image->h )
				continue;

			int offset = yy * image->w * 3 + xx * 3;

			byte *dst = &image->data[ offset ];

			for ( int i = 0 ; i < 3; ++i )
			{
				dst[ i ] = (byte)( flOneMinusAlpha * dst[ i ] + flAlpha * rgb[ i ] );
			}
		}
	}
}

void BuildAggregateStats( IMySQL *mysql, char const *pszMapName, char const *whereClause )
{
	char q[ 2048 ];

	// Got it!!!
	std::string mapname;
	mapname = pszMapName;
	v_escape_string( mapname );

	// Now read all the locations from the sql server

	Q_snprintf( q, sizeof( q ), "select Sum(Count), Avg(Count), Avg(Seconds)/60.0, Avg(Deaths) from %s_maps where MapName = \'%s\' %s and Seconds < 15000;", gamename, mapname.c_str(), whereClause );

	int retcode = mysql->Execute( q );
	if ( retcode != 0 )
	{
		Msg( "Query %s failed\n", q );
		return;
	}

	if ( mysql->SeekToFirstRow() && mysql->NextRow() )
	{
		int SumCount = mysql->GetColumnValue_Int( 0 );
		if ( SumCount == 0 )
		{
			Msg( "No data for %s\n", mapname.c_str() );
			return;
		}
		float AverageCount = Q_atof( mysql->GetColumnValue_String( 1 ) );
		float AvgTime = Q_atof( mysql->GetColumnValue_String( 2 ) );
		float AvgDeaths = Q_atof( mysql->GetColumnValue_String( 3 ) );

		Msg( "Sessions %d, average sessions %.2f avg time %.2f (minutes) avgdeaths %.2f\n",
			SumCount,
			AverageCount,
			AvgTime,
			AvgDeaths );
	}
	else
	{
		Msg( "No data for %s\n", mapname.c_str() );
		return;
	}

	// Now show entity stats

	Q_snprintf( q, sizeof( q ), "select Entity, Sum(BodyCount), avg(BodyCount), Sum(KilledPlayer), avg(KilledPlayer) from %s_entities where mapname = \'%s\' %s group by Entity;", gamename, mapname.c_str(), whereClause );

	retcode = mysql->Execute( q );
	if ( retcode != 0 )
	{
		Msg( "Query %s failed\n", q );
		return;
	}

	Msg( "  Entities\n\n" );
	Msg( "  %32s %10s %10s %15s %10s\n",
		"Entity", "Killed", "Avg", "KilledPlayer", "Avg" );

	if ( mysql->SeekToFirstRow() )
	{
		while ( mysql->NextRow() )
		{
			Msg( "  %32s %10d %10.2f %15d %10.2f\n",
				mysql->GetColumnValue_String( 0 ),
				mysql->GetColumnValue_Int( 1 ),
				Q_atof( mysql->GetColumnValue_String( 2 ) ),
				mysql->GetColumnValue_Int( 3 ),
				Q_atof( mysql->GetColumnValue_String( 4 ) ) );
		}
	}

	// Now show weapon stats	
	Q_snprintf( q, sizeof( q ), "select Weapon, Sum(Shots), avg(Shots), sum(Hits), avg(Hits), Sum(Damage), Avg(Damage) from %s_weapons where Damage < 100000 and mapname = \'%s\' %s group by Weapon;", gamename, mapname.c_str(), whereClause );

	retcode = mysql->Execute( q );
	if ( retcode != 0 )
	{
		Msg( "Query %s failed\n", q );
		return;
	}

	Msg( "  Weapons\n" );
	Msg( "  %32s %10s %10s %15s %10s %15s %10s\n",
		"Weapon", "Shots", "Avg", "Hits", "Avg", "Damage", "Avg" );

	if ( mysql->SeekToFirstRow() )
	{
		while ( mysql->NextRow() )
		{
			Msg( "  %32s %10d %10.2f %15d %10.2f %15d %10.2f\n",
				mysql->GetColumnValue_String( 0 ),
				mysql->GetColumnValue_Int( 1 ),
				Q_atof( mysql->GetColumnValue_String( 2 ) ),
				mysql->GetColumnValue_Int( 3 ),
				Q_atof( mysql->GetColumnValue_String( 4 ) ),
				mysql->GetColumnValue_Int( 5 ),
				Q_atof( mysql->GetColumnValue_String( 6 ) ));
		}
	}

	// Now show weapon stats	
	Q_snprintf( q, sizeof( q ), "select count(*)/count(distinct(userid) ) from %s_saves where mapname = \'%s\' %s;", gamename, mapname.c_str(), whereClause );

	retcode = mysql->Execute( q );
	if ( retcode != 0 )
	{
		Msg( "Query %s failed\n", q );
		return;
	}

	Msg( "  Saves\n" );

	if ( mysql->SeekToFirstRow() )
	{
		while ( mysql->NextRow() )
		{
			const char *colVal = mysql->GetColumnValue_String( 0 );
			float flVal = 0.0f;
			if ( colVal )
				flVal = Q_atof( colVal );
			Msg( "  Average Saves per user: %.2f\n", flVal );
		}
	}

	Msg( "  Counters\n" );

	static char *counters[] =
	{
		"CRATESSMASHED",
		"OBJECTSPUNTED",
		"VEHICULARHOMICIDES",
		"DISTANCE_INVEHICLE",
		"DISTANCE_ONFOOT",
		"DISTANCE_ONFOOTSPRINTING",
		"FALLINGDEATHS",
		"VEHICLE_OVERTURNED",
		"LOADGAME_STILLALIVE",
		"LOADS",
		"SAVES",
		"GODMODES",
		"NOCLIPS",
		"DAMAGETAKEN",
		NULL
	};

	Q_snprintf( q, sizeof( q ), "select Sum(CRATESSMASHED), Avg(CRATESSMASHED),"\
								"Sum(OBJECTSPUNTED), Avg(OBJECTSPUNTED),"\
								"Sum(VEHICULARHOMICIDES), Avg(VEHICULARHOMICIDES),"\
								"Sum(DISTANCE_INVEHICLE), Avg(DISTANCE_INVEHICLE),"\
								"Sum(DISTANCE_ONFOOT), Avg(DISTANCE_ONFOOT),"\
								"Sum(DISTANCE_ONFOOTSPRINTING), Avg(DISTANCE_ONFOOTSPRINTING),"\
								"Sum(FALLINGDEATHS), Avg(FALLINGDEATHS),"\
								"Sum(VEHICLE_OVERTURNED), Avg(VEHICLE_OVERTURNED),"\
								"Sum(LOADGAME_STILLALIVE), Avg(LOADGAME_STILLALIVE),"\
								"Sum(LOADS), Avg(LOADS),"\
								"Sum(SAVES), Avg(SAVES),"\
								"Sum(GODMODES), Avg(GODMODES),"\
								"Sum(NOCLIPS), Avg(NOCLIPS),"\
								"Sum(DAMAGETAKEN), Avg(DAMAGETAKEN) "\
								"from %s_counters where mapname = \'%s\' %s;", gamename, mapname.c_str(), whereClause );

	retcode = mysql->Execute( q );
	if ( retcode != 0 )
	{
		Msg( "Query %s failed\n", q );
		return;
	}

	int i = 0;
	Msg( "  %32s %20s %20s\n", "Counter", "Total", "Average" );

#define INCHES_TO_MILES ( 1.0 / ( 5280.0 * 12.0 ) )

	if ( mysql->SeekToFirstRow() )
	{
		while ( mysql->NextRow() )
		{
			while ( counters[ i ] != NULL )
			{
				int idx = 2 * i;

				char const *raw1 = mysql->GetColumnValue_String( idx );
				char const *raw2 = mysql->GetColumnValue_String( idx + 1 );

	//			Msg( "%s raw %s\n%s\n", counters[ i ], raw1, raw2 );

				uint64 sum = _atoi64( raw1 );
				double avg = Q_atof( raw2 );

				if ( Q_stristr( counters[ i ], "DISTANCE_" ) )
				{
					Msg( "  %32s %20.2f %20.2f [miles]\n",
						counters[ i ],
						(double)sum * INCHES_TO_MILES,
						avg * INCHES_TO_MILES );
				}
				else
				{
					Msg( "  %32s %20I64u %20.2f\n",
						counters[ i ],
						sum,
						avg );
				}

				++i;
			}
		}
	}

	Msg( "-----------------------------------------------------------\n" );

	// Now show generic stats	
	Q_snprintf( q, sizeof( q ), "select StatName, Sum(Count), avg(Count), sum(Value), avg(Value) from %s_generic where mapname = \'%s\' %s group by StatName;", gamename, mapname.c_str(), whereClause );

	retcode = mysql->Execute( q );
	if ( retcode != 0 )
	{
		Msg( "Query %s failed\n", q );
		return;
	}

	Msg( "  Generic\n" );
	Msg( "  %32s %10s %10s %15s %10s\n",
		"Name", "Count", "Avg", "Value", "Avg" );

	if ( mysql->SeekToFirstRow() )
	{
		while ( mysql->NextRow() )
		{
			Msg( "  %32s %10d %10.2f %15d %10.2f\n",
				mysql->GetColumnValue_String( 0 ),
				mysql->GetColumnValue_Int( 1 ),
				Q_atof( mysql->GetColumnValue_String( 2 ) ),
				mysql->GetColumnValue_Int( 3 ),
				Q_atof( mysql->GetColumnValue_String( 4 ) ) );
		}
	}
}

#define EP2_DEATHS_VERSION "1.1"

void printusage( int argc, char * argv[] )
{
	Msg( "%s:\n"\
		" Version = " EP2_DEATHS_VERSION "\n"\
		" Date [ " __DATE__ " " __TIME__ " ]\n"\
		" Copyright Valve 2007.  All rights reserved.\n"\
		"  -game [ep2 | tf]		{ which game }\n"\
		"  -bugs bugtestfile     { special bug file culled from PVCSTracker report }\n"\
		"  -imagedir imagedir    { directory for mapinfo.res and image .bmps }\n"\
		"  -deaths               { build death images }\n"\
		"  -scale imagescalefactor (default 1.0)\n"\
		"  -h sqldbhost\n"\
		"  -u sqluser\n"\
		"  -p sqlpw\n"\
		"  -db sqldb\n"\
		"  -stress count         { run stress testing for image creation }\n"\
		"  -where 'additional where clause'\n"\
		"  -stats                { spew per level stats for entities, weapons, etc. }\n", argv[ 0 ] );
}

int main(int argc, char* argv[])
{
	int i;
	char bugfile[ 512 ] = { 0 };
	char pathname[ 512 ] = { 0 };
	char whereclause[ 1024 ] = { 0 };
	float flScale = 1.0f;

	char const *host = "";
	char const *user = "";
	char const *pw = "";
	char const *db = "";

	bool bBugSpots = false;
	bool bBuildImages = false;
	bool bBuildStats = false;
	bool bStressTest = false;
	int stresstestcount = 1000;

	for ( i = 1 ; i < argc ; ++i )
	{
		if (!stricmp(argv[i],"-bugs"))
		{
			bBugSpots = true;
			Q_strncpy( bugfile, argv[i+1], sizeof( bugfile ) );
			Msg( "  parsing bugs from '%s'\n", bugfile );
			++i;
		}
		else if ( !stricmp( argv[ i ], "-game" ))
		{
			Q_strncpy( gamename, argv[i+1], sizeof( gamename ) );
			Msg( "  death maps for game '%s'\n", gamename );
			++i;
		}
		else if (!stricmp(argv[i],"-deaths"))
		{
			bBuildImages = true;
			Msg( "  generating death images\n" );
		}
		else if (!stricmp(argv[i],"-imagedir"))
		{
			Q_strncpy( pathname, argv[i+1], sizeof( pathname ) );
			Msg( "  image dir '%s'\n", pathname );
			++i;
		}
		else if (!stricmp(argv[i],"-where"))
		{
			Q_snprintf( whereclause, sizeof( whereclause ), " and %s", argv[i+1] );
			Msg( "  using custom where clause '%s'\n", argv[i+1] );
			++i;
		}
		else if (!stricmp(argv[i],"-h"))
		{
			host = argv[ i + 1 ];
			Msg( "  mysql host '%s'\n", host );
			++i;
		}
		else if (!stricmp(argv[i],"-u"))
		{
			user = argv[ i + 1 ];
			Msg( "  mysql user '%s'\n", user );
			++i;
		}
		else if (!stricmp(argv[i],"-p"))
		{
			pw = argv[ i + 1 ];
			// Msg( "  mysql pw '%s'\n", pw );
			++i;
		}
		else if (!stricmp(argv[i],"-db"))
		{
			db = argv[ i + 1 ];
			Msg( "  mysql db '%s'\n", db );
			++i;
		}
		else if (!stricmp(argv[i],"-scale"))
		{
			flScale = max( 0.01f, Q_atof ( argv[ i + 1 ] ) );
			Msg( "  image scale '%g'\n", flScale );
			++i;
		}
		else if (!stricmp(argv[i],"-stats"))
		{
			bBuildStats = true;
			Msg( "  generating per level stats\n" );
		}
		else if (!stricmp(argv[i],"-stress"))
		{
			bStressTest = true;
			stresstestcount = Q_atoi( argv[ i + 1 ] );
			Msg( "  performing stress testing count = %d\n", stresstestcount );
			++i;
		}
		else
			break;
	}

	if ( i != argc )
	{
		printusage( argc, argv );
		return -1;
	}

	if ( !pathname[ 0 ] )
	{
		printusage( argc, argv );
		return -1;
	}
	InitDefaultFileSystem();

	CUtlDict< CUtlVector< Vector > *, int > raw;

	char fn[ 512 ];
	Q_snprintf( fn, sizeof( fn ), "%s/mapinfo.res", pathname );
	Q_FixSlashes( fn );
	Q_strlower( fn );

	KeyValues *kv = new KeyValues( "mapinfo.res" );
	if ( !kv->LoadFromFile( g_pFullFileSystem, fn, NULL ) )
	{
		Msg( "Unable to load mapinfo.res file from image directory [%s]\n", 
			pathname );
		exit( -1 );
	}

	CSysModule *sql = NULL;
	CreateInterfaceFn factory = NULL;
	IMySQL *mysql = NULL;

	bool bSqlOkay = false;

	if ( !bBugSpots )
	{
		sql = Sys_LoadModule( "mysql_wrapper" );
		if ( sql )
		{
			factory = Sys_GetFactory( sql );
			if ( factory )
			{
				mysql = ( IMySQL * )factory( MYSQL_WRAPPER_VERSION_NAME, NULL );
				if ( mysql )
				{
					if ( mysql->InitMySQL( db, host, user, pw ) )
					{
						bSqlOkay = true;
						Msg( "Successfully connected to database %s on host %s, user %s\n", db, host, user );
					}
					else
					{
						Msg( "mysql->InitMySQL( %s, %s, %s, [password]) failed\n", db, host, user );
					}
				}
				else
				{
					Msg( "Unable to get MYSQL_WRAPPER_VERSION_NAME(%s) from mysql_wrapper\n", MYSQL_WRAPPER_VERSION_NAME );
				}
			}
			else
			{
				Msg( "Sys_GetFactory on mysql_wrapper failed\n" );
			}
		}
		else
		{
			Msg( "Sys_LoadModule( mysql_wrapper ) failed\n" );
		}
	}
	else
	{
		ParseBugs( raw, bugfile );
	}
	
	// Build lookup
	int numgrades = 200;
	float flGradesMinusOne = (float)( numgrades - 1 );
	Color *colors = new Color[ numgrades ];
	float *fastSquareRoot = new float[ numgrades ];
	for ( int iGrades = 0; iGrades < numgrades; ++iGrades )
	{
		float flValue = (float)iGrades / flGradesMinusOne;
		float hsl[ 3 ];
		hsl[ 0 ] = 240.0f - ( 1.0f - flValue ) * 240.0f;
		hsl[ 1 ] = 1.0f;
		hsl[ 2 ] = 0.5f;
		HSLToRGB( colors[iGrades], hsl );

		fastSquareRoot[iGrades] = sqrt( flValue );
	}

	Color black( 0, 0, 0, 0 );

	for ( KeyValues *map = kv->GetFirstSubKey(); map; map = map->GetNextKey() )
	{
		char const *pszMapName = map->GetName();

		Msg( "Processing %s\n", pszMapName );

		if ( bBuildImages )
		{
			char imagefile[ 512 ];
			Q_snprintf( imagefile, sizeof( imagefile ), "%s/%s.bmp", pathname, pszMapName );
			Q_FixSlashes( imagefile );
			Q_strlower( imagefile );

			char outfile[ 512 ];
			Q_snprintf( outfile, sizeof( outfile ), "%s/%s_deaths.jpg", pathname, pszMapName );
			Q_FixSlashes( outfile );
			Q_strlower( outfile );


			// Do the processing
			FileHandle_t fh = g_pFullFileSystem->Open( imagefile, "rb" );
			if ( FILESYSTEM_INVALID_HANDLE == fh )
				continue;

			int pos_x = map->GetInt( "x", 0 );
			int pos_y = map->GetInt( "y", 0 );
			float scale = map->GetFloat( "scale", 1.0f );

			int size = g_pFullFileSystem->Size( fh );
			byte *buf = new byte[ size + 1 ];
			g_pFullFileSystem->Read( buf, size, fh );
			g_pFullFileSystem->Close( fh );
			buf[ size ] = 0;




			// Now parse into image
			Image_t image = { 0 };
			if ( ReadBitmapRGB( buf, size, &image ) )
			{
				float flMaxValue = 0.0f;
				float flRadius = flScale * 256.0f / scale;
				int nRadius = ( int )( flRadius );
				//float flRadius = 2.0f;
				//int nRadius = 2;
				float flRadiusSqr = flRadius * flRadius;

				CUtlVector< POINT > vecDeaths;
				if ( bBugSpots )
				{
					int idx = raw.Find( pszMapName );
					if ( idx != raw.InvalidIndex() )
					{
						CUtlVector< Vector > *pvDB = raw[ idx ];
						for ( int iVec= 0; iVec < pvDB->Count(); ++iVec )
						{
							int x = (int)pvDB->Element( iVec )[ 0 ];
							int y = (int)pvDB->Element( iVec )[ 1 ];
							// int z = mysql->GetColumnValue_Int( 2 );

							float pixx = (float)( x - pos_x );
							float pixy = (float)( y - pos_y );

							pixx /= scale;
							pixy /= -scale;

							POINT death;
							death.x = pixx;
							death.y = pixy;

							vecDeaths.AddToTail( death );
						}
					}
				}
				else
				{
					if ( bStressTest )
					{
						for ( int iTest = 0 ; iTest < stresstestcount; ++iTest )
						{
							POINT death;
							do 
							{
								death.x = RandomInt( 0, image.w - 1 );
								death.y = RandomInt( 0, image.h - 1 );

								byte *rgb = &image.data[ 3 * death.y * image.w + 3 * death.x ];
								if ( rgb[ 0 ] || rgb[ 1 ] || rgb[ 2 ] )
								{
									break;
								}
							} while ( true );

							vecDeaths.AddToTail( death );
						}
					}
					else
					{
						char q[ 512 ];

						// Got it!!!
						std::string mapname;
						mapname = pszMapName;
						v_escape_string( mapname );

						// Now read all the locations from the sql server

						Q_snprintf( q, sizeof( q ), "select x, y from %s_deaths where MapName = \'%s\' %s;", gamename, mapname.c_str(), whereclause );

						int retcode = mysql->Execute( q );
						if ( retcode != 0 )
						{
							printf( "Query %s failed\n", q );
							return -1;
						}

						bool bMoreData = mysql->SeekToFirstRow();
						while ( bMoreData && mysql->NextRow() )
						{
							int x = mysql->GetColumnValue_Int( 0 );
							int y = mysql->GetColumnValue_Int( 1 );
							// int z = mysql->GetColumnValue_Int( 2 );

							float pixx = (float)( x - pos_x );
							float pixy = (float)( y - pos_y );

							pixx /= scale;
							pixy /= -scale;

							POINT death;
							death.x = pixx;
							death.y = pixy;

							vecDeaths.AddToTail( death );
						}
					}
				}

				float *info = new float[ image.h * image.w ];
				Q_memset( info, 0, image.h * image.w * sizeof( float ) );

				float ooRadiusSqr = 1.0f / flRadiusSqr;

				int nSide = nRadius * 2 + 1;

				float *contribution = new float[ nSide * nSide ];
				Q_memset( contribution, 0, nSide * nSide * sizeof( float ) );

				for ( int s = 0; s < nSide; ++s )
				{
					for ( int t = 0; t < nSide; ++t )
					{
						int dx = s - nRadius;
						int dy = t - nRadius;
						float dSqr = dx * dx + dy * dy;
						if ( dSqr < flRadiusSqr )
						{
							contribution[ t * nSide + s ] = 1.0f - ( dSqr * ooRadiusSqr );
						}
					}
				}

				float st = Plat_FloatTime();

				int c = vecDeaths.Count();
				for ( int iDeath = 0; iDeath < c; ++iDeath )
				{
					POINT &v = vecDeaths[iDeath];
					int xpos = v.x;
					int ypos = v.y;
					for ( int y = max( 0, ypos - nRadius ); y <= min( ypos + nRadius, image.h - 1 ); ++y )
					{
						for ( int x = max( 0, xpos - nRadius ); x <= xpos + nRadius; ++x )
						{
							if ( x >= image.w )
								break;

							float *slot = &info[ y * image.w + x ];

							int dx = x - xpos + nRadius;
							int dy = y - ypos + nRadius;

							// Figure out contrubution
							flScale = contribution[ dy * nSide + dx ];
							if ( flScale <= 0.0f )
								continue;

							*slot += flScale;
							if ( *slot > flMaxValue )
								flMaxValue = *slot;
						}
					}
				}

				if ( flMaxValue >0.0f )
				{
					float flOneOverMax = 1.0f / flMaxValue;

					for ( int y = 0; y < image.h; ++y )
					{
						float *slot = &info[ y * image.w ];

						for ( int x = 0; x < image.w; ++x, ++slot )
						{
							float flValue = *slot * flOneOverMax;
							int colorIndex = clamp( (int)( flValue * flGradesMinusOne + 0.5f ), 0, numgrades - 1 );
							const Color &col = colors[ colorIndex ];

							DrawColoredRect( &image, x, y, 1, 1, black, flValue * flValue );
							float sqroot = fastSquareRoot[ colorIndex ];
							if ( sqroot != 0.0f )
								sqroot = max( sqroot, 0.33f );
							DrawColoredRect( &image, x, y, 1, 1, col, sqroot );
						}
					}
				}

				float et = Plat_FloatTime();
				double msec = 1000.0 * ( et - st );
				double msecperdeath = 0.0;
				if ( vecDeaths.Count() > 0 )
				{
					msecperdeath = 1000.0 * msec / (double)vecDeaths.Count();
				}

				if ( bStressTest )
				{
					Msg( "Processing took %f msec %f msec per thousand deaths [%d deaths]\n", msec, msecperdeath, vecDeaths.Count() );
				}

				delete[] contribution;
				delete[] info;

				// Now write the image back out 
				WriteJPeg( outfile, &image );
			}
			delete[] image.data;

			if ( bStressTest )
				break;
		}

		if ( bBuildStats )
		{
			BuildAggregateStats( mysql, pszMapName, whereclause );
		}
	}

	delete[] fastSquareRoot;
	delete[] colors;

	kv->deleteThis();

	if ( bSqlOkay )
	{
		if ( mysql )
		{
			mysql->Release();
			mysql = NULL;
		}

		if ( sql )
		{
			Sys_UnloadModule( sql );
			sql = NULL;
		}
	}


	return 0;
}
