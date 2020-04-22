//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "pngloader.h"
#include <stdio.h>
#include <stdlib.h>
//#include "vstdlib/strtools.h"

#include <setjmp.h>

// clang3 on OSX folks the attribute into the prototype, causing a compile failure
// filed radar bug 10397783
#if ( __clang_major__ == 3 )
extern void longjmp( jmp_buf, int ) __attribute__((noreturn));
#endif

#include "libpng/png.h"
#include "libpng/pngstruct.h"
#include "tier0/dbg.h"

//
// struct to encapsulate png data in memory that we are walking through
//
struct PngDataStream_t
{
	PngDataStream_t( const byte *pchData, int cbData )
	{
		m_pPNGData = pchData;
		m_cbPNGData = cbData;
		m_iPNGData = 0;
	}

	const byte *m_pPNGData; // the png data
	uint32 m_cbPNGData; // length of the data
	uint32 m_iPNGData; // offset for next read
};


// helper function to nibble some more png data from our memory buffer
void ReadPNGData( png_structp png_ptr, png_bytep outBytes, png_size_t byteCountToRead )
{
	if(png_ptr->io_ptr == NULL)
		return;   // we didn't have our mem buffer set

	PngDataStream_t *pData = (PngDataStream_t *)png_ptr->io_ptr;

	if ( byteCountToRead + pData->m_iPNGData > pData->m_cbPNGData )
		return;

	Q_memcpy( outBytes, pData->m_pPNGData + pData->m_iPNGData, byteCountToRead );
	pData->m_iPNGData += byteCountToRead;
}


// called when we failed to load a png file, we just bail to the previously set exit
static void PNGErrorFunction(png_structp png_ptr, png_const_charp msg) 
{
	Warning( "PNG load error %s\n", msg );
	longjmp( png_jmpbuf(png_ptr), 1 );
}


// we had a warning lets log it
static void PNGWarningFunction(png_structp png_ptr, png_const_charp msg) 
{
	Warning( "PNG load error %s\n", msg );
}


// Get dimensions out of PNG header
bool GetPNGDimensions( const byte *pubPNGData, int cubPNGData, uint32 &width, uint32 &height )
{
	png_const_bytep pngData = (png_const_bytep)pubPNGData;
	if (png_sig_cmp( pngData, 0, 8))
        return false;   /* bad signature */

	PngDataStream_t pngDataStream( pubPNGData, cubPNGData ); // keeps a local copy of the data we a reading

	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;

    png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, &PNGErrorFunction, &PNGWarningFunction );
    if (!png_ptr)
        return false;   /* out of memory */

    info_ptr = png_create_info_struct( png_ptr );
    if ( !info_ptr ) 
	{
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return false;   /* out of memory */
    }

    /* setjmp() must be called in every function that calls a PNG-reading
     * libpng function */
    if ( setjmp( png_jmpbuf(png_ptr) ) ) 
	{
        png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
        return false;
    }

	png_set_read_fn( png_ptr, (void *)&pngDataStream, ReadPNGData );
    png_read_info( png_ptr, info_ptr );  /* read all PNG info up to image data */

	width = png_get_image_width( png_ptr, info_ptr );
	height = png_get_image_height( png_ptr, info_ptr );
	png_destroy_read_struct( &png_ptr, &info_ptr, NULL );

	return true;
}


// given a png formatted memory buffer return a raw rgba buffer
bool ConvertPNGToRGBA( const byte *pubPNGData, int cubPNGData, CUtlBuffer &bufOutput, int &width, int &height )
{
	png_const_bytep pngData = (png_const_bytep)pubPNGData;
	if (png_sig_cmp( pngData, 0, 8))
        return false;   /* bad signature */

	PngDataStream_t pngDataStream( pubPNGData, cubPNGData ); // keeps a local copy of the data we a reading

	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;

    /* could pass pointers to user-defined error handlers instead of NULLs: */

    png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, &PNGErrorFunction, &PNGWarningFunction );
    if (!png_ptr)
        return false;   /* out of memory */

    info_ptr = png_create_info_struct( png_ptr );
    if ( !info_ptr ) 
	{
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return false;   /* out of memory */
    }

    /* setjmp() must be called in every function that calls a PNG-reading
     * libpng function */

    if ( setjmp( png_jmpbuf(png_ptr) ) ) 
	{
        png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
        return false;
    }

	png_set_read_fn( png_ptr, (void *)&pngDataStream, ReadPNGData );
    png_read_info( png_ptr, info_ptr );  /* read all PNG info up to image data */


    /* alternatively, could make separate calls to png_get_image_width(),
     * etc., but want bit_depth and color_type for later [don't care about
     * compression_type and filter_type => NULLs] */

	int bit_depth;
	int color_type;
	uint32 png_width;
	uint32 png_height;

	png_get_IHDR( png_ptr, info_ptr, &png_width, &png_height, &bit_depth, &color_type, NULL, NULL, NULL );

	width = png_width;
	height = png_height;

    png_uint_32 rowbytes;

    /* expand palette images to RGB, low-bit-depth grayscale images to 8 bits,
     * transparency chunks to full alpha channel; strip 16-bit-per-sample
     * images to 8 bits per sample; and convert grayscale to RGB[A] */

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_expand( png_ptr );
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand( png_ptr );
    if (png_get_valid( png_ptr, info_ptr, PNG_INFO_tRNS ) )
        png_set_expand( png_ptr );
    if (bit_depth == 16)
        png_set_strip_16( png_ptr );
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb( png_ptr );

  /*
	double gamma;
  if (png_get_gAMA(png_ptr, info_ptr, &gamma))
        png_set_gamma(png_ptr, display_exponent, gamma);

*/
    /* all transformations have been registered; now update info_ptr data,
     * get rowbytes and channels, and allocate image memory */

    png_read_update_info( png_ptr, info_ptr );

    rowbytes = png_get_rowbytes( png_ptr, info_ptr );
    png_byte channels = (int)png_get_channels( png_ptr, info_ptr );
	Assert( channels == 4 );
	if ( channels != 4 )
		return false;

	png_bytepp  row_pointers = NULL;

	if ( ( row_pointers = (png_bytepp)malloc(height*sizeof(png_bytep) ) ) == NULL ) 
	{
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return false;
    }

	bufOutput.EnsureCapacity( rowbytes*height );
	bufOutput.SeekPut( CUtlBuffer::SEEK_HEAD, rowbytes*height );

    /* set the individual row_pointers to point at the correct offsets */

    for ( int i = 0;  i < height;  ++i)
        row_pointers[i] = (png_byte *)bufOutput.Base() + i*rowbytes;


    /* now we can go ahead and just read the whole image */

    png_read_image( png_ptr, row_pointers );

    free( row_pointers );
    row_pointers = NULL;

    png_read_end(png_ptr, NULL);

	if ( png_ptr && info_ptr ) 
	{
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
		png_ptr = NULL;
		info_ptr = NULL;
	}

	return true;
}
