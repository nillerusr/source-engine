//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "jpegloader.h"
#include "tier0/dbg.h"
#include "tier1/utlvector.h"
#include "tier0/vprof.h"
#include "jpeglib/jpeglib.h"
#include <setjmp.h>
//#include "fileio.h"

class CFileWriter
{

};

class CJpegDestMgr : public jpeg_destination_mgr 
{
public:
	CJpegDestMgr( CFileWriter &refOutputFileWriter )
		: m_pOutputFileWriter( &refOutputFileWriter ),
		m_pOutputBuffer( NULL )
	{
		Init();
	}

	CJpegDestMgr( CUtlBuffer &refOutputBuffer )
		: m_pOutputFileWriter( NULL ),
		m_pOutputBuffer( &refOutputBuffer )
	{
		Init();
	}

	void Init()
	{
		this->init_destination = &CJpegDestMgr::imp_init_dest;
		this->empty_output_buffer = &CJpegDestMgr::imp_empty_output_buffer;
		this->term_destination = &CJpegDestMgr::imp_term_destination;

		this->next_output_byte = 0;
		this->free_in_buffer = 0;	
	}

	static void imp_init_dest( j_compress_ptr cinfo )
	{
		CJpegDestMgr *pInstance = (CJpegDestMgr*)cinfo->dest;

		if ( pInstance->m_pOutputBuffer )
			 pInstance->m_pOutputBuffer->EnsureCapacity( cinfo->image_width*cinfo->image_height*3 );

		const int k_cubMaxTempJpegBuffer = 1024*10;
		int cubBufferSize = MIN( cinfo->image_width*cinfo->image_height*3, k_cubMaxTempJpegBuffer );
		pInstance->m_Buffer.EnsureCapacity( cubBufferSize );
		pInstance->free_in_buffer = pInstance->m_Buffer.Size();
		pInstance->next_output_byte = (JOCTET*)pInstance->m_Buffer.Base();
	}

	static boolean imp_empty_output_buffer( j_compress_ptr cinfo )
	{
		/*
		CJpegDestMgr *pInstance = (CJpegDestMgr*)cinfo->dest;

		// Write the entire buffer, ignoring next_output_byte and free_in_buffer as they lie (see docs)
		if (  pInstance->m_pOutputFileWriter )
			pInstance->m_pOutputFileWriter->Write( pInstance->m_Buffer.Base(), pInstance->m_Buffer.Size() );
		else
			pInstance->m_pOutputBuffer->Put( pInstance->m_Buffer.Base(), pInstance->m_Buffer.Size() );

		pInstance->free_in_buffer = pInstance->m_Buffer.Size();
		pInstance->next_output_byte = (JOCTET*)pInstance->m_Buffer.Base();
		*/
		return true;
	}


	static void imp_term_destination(j_compress_ptr cinfo)
	{
		/*
		CJpegDestMgr *pInstance = (CJpegDestMgr*)cinfo->dest;

		// next_output_byte and free_in_buffer don't lie here like in empty_output_buffer
		int cubToWrite = (byte*)pInstance->next_output_byte - (byte*)pInstance->m_Buffer.Base();
		if ( cubToWrite > 0 )
		{
			if ( pInstance->m_pOutputFileWriter )
				pInstance->m_pOutputFileWriter->Write( pInstance->m_Buffer.Base(), cubToWrite );
			else
				pInstance->m_pOutputBuffer->Put( pInstance->m_Buffer.Base(), cubToWrite );
		}

		if ( pInstance->m_pOutputFileWriter )
			 pInstance->m_pOutputFileWriter->Flush();	
			 */
	}

	static void error_exit( j_common_ptr cptr )
	{
		longjmp( m_JmpBuf, 1 );
	}

	static jmp_buf m_JmpBuf;

private:
	CFileWriter *m_pOutputFileWriter;
	CUtlBuffer *m_pOutputBuffer;
	CUtlBuffer m_Buffer;
};

jmp_buf CJpegDestMgr::m_JmpBuf;


class CJpegSourceMgr : public jpeg_source_mgr
{
public:
	CJpegSourceMgr()
	{
		this->init_source = &CJpegSourceMgr::imp_init_source;
		this->fill_input_buffer = &CJpegSourceMgr::imp_fill_input_buffer;
		this->skip_input_data = &CJpegSourceMgr::imp_skip_input_data;
		this->resync_to_restart = &CJpegSourceMgr::imp_resync_to_restart;
		this->term_source = &CJpegSourceMgr::imp_term_source;

		this->next_input_byte = 0;
		this->bytes_in_buffer = 0;
	}

	bool Init( const byte *pubData, int cubData )
	{
		m_Data.AddMultipleToTail( cubData, (char *)pubData );
		bytes_in_buffer = m_Data.Count();
		next_input_byte = (unsigned char*)m_Data.Base();
		return true;
	}

	static void imp_init_source(j_decompress_ptr cinfo)
	{
		// We handled everything needed in our own Init() call.
	}

	static boolean imp_fill_input_buffer(j_decompress_ptr cinfo)
	{
		// If this is hit it means we are reading a corrupt file.  We can't provide more data
		// as we provided all of it upfront already.
		return 0;
	}

	static void imp_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
	{
		// The library is asking us to skip a chunk of data, usually exif header data or such.  Need 
		// to actually do that.  The library tries to be robust and skip obviously bad data on its own
		// one byte at a time as well, but faster here, and safer as the library can't always do it 
		// correctly if we fail these calls.
		CJpegSourceMgr *pInstance = (CJpegSourceMgr*)cinfo->src;
		pInstance->bytes_in_buffer -= num_bytes;
		pInstance->next_input_byte += num_bytes;
	}

	static boolean imp_resync_to_restart(j_decompress_ptr cinfo, int desired)
	{
		// This happens if the library fails to find a resync marker where expected, we'll then
		// use it's default handler.  This handler will skip ahead trying to find the next point,
		// we could maybe do better going backwards as we have the full buffer, but that's lots more
		// work and this will only get hit on a partially corrupt image anyway.
		return jpeg_resync_to_restart( cinfo, desired );
	}

	static void imp_term_source(j_decompress_ptr cinfo)
	{
	}

	static void error_exit( j_common_ptr cptr )
	{
		longjmp( m_JmpBuf, 1 );
	}

	static jmp_buf m_JmpBuf;

public:
	CUtlVector<char> m_Data;
};

jmp_buf CJpegSourceMgr::m_JmpBuf;



//-----------------------------------------------------------------------------
// Purpose: returns the dimensions of a jpg file from it's contents
//-----------------------------------------------------------------------------
bool GetJpegDimensions( const byte *pubData, int cubData, uint32 &width, uint32 &height )
{
	CJpegSourceMgr sourceMgr;
	bool bRet = sourceMgr.Init( pubData, cubData );
	if ( !bRet )
		return false;

	// Load the jpeg.
	struct jpeg_decompress_struct jpegInfo;
	struct jpeg_error_mgr jerr;

	memset( &jpegInfo, 0, sizeof( jpegInfo ) );
	jpegInfo.err = jpeg_std_error(&jerr);
	jerr.error_exit = &CJpegSourceMgr::error_exit;
	jpeg_create_decompress(&jpegInfo);
	jpegInfo.src = &sourceMgr;

	if ( setjmp( sourceMgr.m_JmpBuf ) == 1 )
	{
		jpeg_destroy_decompress(&jpegInfo);
		return false;
	}

	if (jpeg_read_header(&jpegInfo, TRUE) != JPEG_HEADER_OK)
	{
		jpeg_destroy_decompress(&jpegInfo);
		return false;
	}	

	width = jpegInfo.image_width;
	height = jpegInfo.image_height;

	jpeg_destroy_decompress(&jpegInfo);
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: takes the contents of a .jpg file and turns it into RGB data, returning the width and height and optionally the number of bytes used
//-----------------------------------------------------------------------------
bool ConvertJpegToRGB( const byte *pubData, int cubData, CUtlVector<byte> &buf, int &width, int &height, int *pcubUsed )
{
	CJpegSourceMgr sourceMgr;
	bool bRet = sourceMgr.Init( pubData, cubData );
	if ( !bRet )
		return false;

	if ( pcubUsed )
		*pcubUsed = 0;

	sourceMgr.bytes_in_buffer = sourceMgr.m_Data.Count();
	sourceMgr.next_input_byte = (unsigned char*)sourceMgr.m_Data.Base();

	// Load the jpeg.
	struct jpeg_decompress_struct jpegInfo;
	struct jpeg_error_mgr jerr;

	memset( &jpegInfo, 0, sizeof( jpegInfo ) );
	jpegInfo.err = jpeg_std_error(&jerr);
	jerr.error_exit = &CJpegSourceMgr::error_exit;
	jpeg_create_decompress(&jpegInfo);
	jpegInfo.src = &sourceMgr;

	if ( setjmp( sourceMgr.m_JmpBuf ) == 1 )
	{
		jpeg_destroy_decompress(&jpegInfo);
		return false;
	}

	if (jpeg_read_header(&jpegInfo, TRUE) != JPEG_HEADER_OK)
	{
		jpeg_destroy_decompress(&jpegInfo);
		return false;
	}	

	// start the decompress with the jpeg engine.
	if ( !jpeg_start_decompress(&jpegInfo) || jpegInfo.output_components != 3)
	{
		jpeg_destroy_decompress(&jpegInfo);
		return false;
	}

	// now that we've started the decompress with the jpeg lib, we have the attributes of the
	// cdnfile ready to be read out of the decompress struct.
	int row_stride = jpegInfo.output_width * jpegInfo.output_components;
	int mem_required = jpegInfo.image_height * jpegInfo.image_width * jpegInfo.output_components;
	JSAMPROW row_pointer[1];
	int cur_row = 0;

	width = jpegInfo.output_width;
	height = jpegInfo.output_height;

	// allocate the memory to read the cdnfile data into.
	buf.SetSize( mem_required );

	// read in all the scan lines of the cdnfile into our cdnfile data buffer.
	bool working = true;
	while (working && (jpegInfo.output_scanline < jpegInfo.output_height))
	{
		row_pointer[0] = &(buf[cur_row * row_stride]);
		if ( !jpeg_read_scanlines(&jpegInfo, row_pointer, 1) )
		{
			working = false;
		}
		++cur_row;
	}

	if (!working)
	{
		jpeg_destroy_decompress(&jpegInfo);
		return false;
	}

	if ( pcubUsed )
		*pcubUsed = (int)((byte*)sourceMgr.next_input_byte - (byte*)sourceMgr.m_Data.Base());
	jpeg_finish_decompress(&jpegInfo);
	jpeg_destroy_decompress(&jpegInfo);
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Converts an RGB image to RGBA opacity will be 100%
//-----------------------------------------------------------------------------
void ConvertRGBToRGBAImage( CUtlVector<unsigned char> &srcData,
						   int srcWidth,
						   int srcHeight,
						   byte *destData,
						   int destWidth,
						   int destHeight )
{
	int destPixelSize = 4;
	int cub = destWidth * destHeight * destPixelSize;
	byte *pSrc = srcData.Base();
	for ( int i = 0; i < cub; i += 4 )
	{
		destData[i] = *pSrc++;
		destData[i+1] = *pSrc++;
		destData[i+2] = *pSrc++;
		destData[i+3] = 255;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Converts raw Jpeg file data and converts to RGBA image buffer
//-----------------------------------------------------------------------------
bool ConvertJpegToRawInternal( const byte *pubJpegData, int cubJpegData, CUtlBuffer &bufOutput, int &width, int &height, int *pcubUsed, bool bMakeRGBA )
{
	// Temporary buffer to decompress Jpeg into as RGB, since libjpeg doesn't do RGBA
	CUtlVector<byte> vecRGB;

	bool bConverted = false;
	{
		VPROF_BUDGET( "ConvertJpegToRGB", VPROF_BUDGETGROUP_OTHER_VGUI );
		bConverted = ConvertJpegToRGB( pubJpegData, cubJpegData, vecRGB, width, height, pcubUsed );
	}

	if ( bConverted )
	{
		bufOutput.Clear();

		int bytesPerPixel = 3;
		if ( bMakeRGBA )
			bytesPerPixel = 4;

		// make sure the buffer is big enough to hold the cdnfile data
		bufOutput.EnsureCapacity( width * height * bytesPerPixel );

		// If the buffer was externally allocated the EnsureCapacity can fail
		if ( bufOutput.Size() < width * height * bytesPerPixel )
			return false;

		bufOutput.SeekPut( CUtlBuffer::SEEK_HEAD, width*height*bytesPerPixel );

		// convert RGB->RGBA, into the final buffer
		if ( bMakeRGBA )
		{
			VPROF_BUDGET( "ConvertRGBToRGBAImage", VPROF_BUDGETGROUP_OTHER_VGUI );
			ConvertRGBToRGBAImage( vecRGB, width, height, (byte *)bufOutput.Base(), width, height );
		}
		else
		{
			Q_memcpy( bufOutput.Base(), vecRGB.Base(), width*height*bytesPerPixel );
		}

		// done
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Converts raw Jpeg file data and converts to RGBA image buffer
//-----------------------------------------------------------------------------
bool ConvertJpegToRGBA( const byte *pubJpegData, int cubJpegData, CUtlBuffer &bufOutput, int &width, int &height, int *pcubUsed )
{
	return ConvertJpegToRawInternal( pubJpegData, cubJpegData, bufOutput, width, height, pcubUsed, true );
}


//-----------------------------------------------------------------------------
// Purpose: Converts raw Jpeg file data and converts to RGB image buffer
//-----------------------------------------------------------------------------
bool ConvertJpegToRGB( const byte *pubJpegData, int cubJpegData, CUtlBuffer &bufOutput, int &width, int &height, int *pcubUsed )
{
	return ConvertJpegToRawInternal( pubJpegData, cubJpegData, bufOutput, width, height, pcubUsed, false );
}


//-----------------------------------------------------------------------------
// Purpose: Internal method for converting RGB to Jpeg and writing either
// to disk or to an output buffer
//-----------------------------------------------------------------------------
bool ConvertRGBToJpegInternal( CJpegDestMgr &destMgr, int quality, int width, int height, CUtlBuffer &bufRGB )
{
	struct jpeg_compress_struct cinfo = {0};
	JSAMPROW row_ptr[1];
	struct jpeg_error_mgr jerr;

	cinfo.err = jpeg_std_error(&jerr);
	jerr.error_exit = &CJpegDestMgr::error_exit;
	jpeg_create_compress(&cinfo);
	cinfo.dest = &destMgr;

	if ( setjmp( destMgr.m_JmpBuf ) == 1 )
	{
		jpeg_destroy_compress(&cinfo);
		return false;
	}

	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;

	jpeg_set_defaults( &cinfo );
	cinfo.dct_method = JDCT_FLOAT;
	jpeg_set_quality(&cinfo, quality, TRUE);

	jpeg_start_compress( &cinfo, TRUE );

	byte *rawBytes = (byte*)bufRGB.Base();
	while( cinfo.next_scanline < cinfo.image_height )
	{
		row_ptr[0] = (unsigned char *)rawBytes+(width*3*cinfo.next_scanline);
		jpeg_write_scanlines( &cinfo, row_ptr, 1 );
	}

	jpeg_finish_compress( &cinfo );
	jpeg_destroy_compress( &cinfo );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Takes a RGB buffer and writes it to disk as a Jpeg
// Params: qualtity should be 0-100, where 100 is best quality.  80 is generally
// a good value.
//-----------------------------------------------------------------------------
bool ConvertRGBToJpeg( const char *pchFileOut, int quality, int width, int height, CUtlBuffer &bufRGB )
{
	/*
	CFileWriter fileWriter;
	if ( !fileWriter.BSetFile( pchFileOut ) )
		return false;

	CJpegDestMgr destMgr( fileWriter );
	return ConvertRGBToJpegInternal( destMgr, quality, width, height, bufRGB );
	*/
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Takes a RGB buffer and writes it to a buffer as a jpeg
// Params: qualtity should be 0-100, where 100 is best quality.  80 is generally
// a good value.
//-----------------------------------------------------------------------------
bool ConvertRGBToJpeg( CUtlBuffer &bufOutput, int quality, int width, int height, CUtlBuffer &bufRGB )
{
	CJpegDestMgr destMgr( bufOutput );
	return ConvertRGBToJpegInternal( destMgr, quality, width, height, bufRGB );
}