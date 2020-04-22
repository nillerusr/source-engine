//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#if !defined( _X360 )
#include <windows.h>
#endif
#include <stdio.h>
#include "tier1/utlbuffer.h"
#include <vgui/VGUI.h>
#include <vgui_controls/Controls.h>
#include "filesystem.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#define	TYP_LUMPY		64				// 64 + grab command number

typedef struct
{
	char		identification[4];		// should be WAD2 or 2DAW
	int			numlumps;
	int			infotableofs;
} wadinfo_t;

typedef struct
{
	int			filepos;
	int			disksize;
	int			size;					// uncompressed
	char		type;
	char		compression;
	char		pad1, pad2;
	char		name[16];				// must be null terminated
} lumpinfo_t;

typedef struct
{
	char		name[16];
	unsigned	width, height;
	unsigned	offsets[4];		// four mip maps stored
} miptex_t;

unsigned char	pixdata[256];

float 	linearpalette[256][3];
float 	d_red, d_green, d_blue;
int		colors_used;
int		color_used[256];
float	maxdistortion;
unsigned char palLogo[768];

/*
=============
AveragePixels
=============
*/
unsigned char AveragePixels (int count)
{
	return pixdata[0];
}

/*
==============
GrabMip

filename MIP x y width height
must be multiples of sixteen
==============
*/
int GrabMip ( HANDLE hdib, unsigned char *lump_p, char *lumpname, COLORREF crf, int *width, int *height)
{
	int             i,x,y,xl,yl,xh,yh,w,h;
	unsigned char   *screen_p, *source;
	miptex_t		*qtex;
	int				miplevel, mipstep;
	int				xx, yy;
	int				count;
	int				byteimagewidth, byteimageheight;
	unsigned char   *byteimage;
	LPBITMAPINFO	lpbmi;      // pointer to BITMAPINFO structure (Win3.0)

	/* get pointer to BITMAPINFO (Win 3.0) */
	lpbmi = (LPBITMAPINFO)::GlobalLock((HGLOBAL)hdib);
	unsigned char *lump_start = lump_p;
	
	xl = yl = 0;
	w = lpbmi->bmiHeader.biWidth;
	h = lpbmi->bmiHeader.biHeight;

	*width = w;
	*height = h;

	byteimage = (unsigned char *)((LPSTR)lpbmi + sizeof( BITMAPINFOHEADER ) + 256 * sizeof( RGBQUAD ) );

	if ( (w & 15) || (h & 15) )
		return 0; //Error ("line %i: miptex sizes must be multiples of 16", scriptline);

	xh = xl+w;
	yh = yl+h;

	qtex = (miptex_t *)lump_p;
	qtex->width = (unsigned)(w);
	qtex->height = (unsigned)(h);
	Q_strncpy (qtex->name, lumpname, sizeof( qtex->name) ); 
	
	lump_p = (unsigned char *)&qtex->offsets[4];
	
	byteimagewidth = w;
	byteimageheight = h;

	source = (unsigned char *)lump_p;
	qtex->offsets[0] = (unsigned)((unsigned char *)lump_p - (unsigned char *)qtex);

	// We're reading from a dib, so go bottom up
	screen_p = byteimage + (h - 1) * w;
	for (y=yl ; y<yh ; y++)
	{
		for (x=xl ; x<xh ; x++)
			*lump_p++ = *screen_p++;

		screen_p -= 2 * w;
	}

	// calculate gamma corrected linear palette
	for (i = 0; i < 256; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			float f = (float)(palLogo[i*3+j] / 255.0);
			linearpalette[i][j] = f; //pow((double)f, 2); // assume textures are done at 2.2, we want to remap them at 1.0
		}
	}

	maxdistortion = 0;
	// assume palette full if it's a transparent texture
	colors_used = 256;
	for (i = 0; i < 256; i++)
		color_used[i] = 1;


	//
	// subsample for greater mip levels
	//

	for (miplevel = 1 ; miplevel<4 ; miplevel++)
	{
		d_red = d_green = d_blue = 0;	// no distortion yet
		qtex->offsets[miplevel] = (unsigned)(lump_p - (unsigned char *)qtex);
		
		mipstep = 1<<miplevel;

		for (y=0 ; y<h ; y+=mipstep)
		{
			for (x = 0 ; x<w ; x+= mipstep)
			{
				count = 0;
				for (yy=0 ; yy<mipstep ; yy++)
				{
					for (xx=0 ; xx<mipstep ; xx++)
						pixdata[count++] = source[(y+yy)*w + x + xx ];
				}

				*lump_p++ = AveragePixels (count);
			}	
		}
	}

	::GlobalUnlock(lpbmi);

	// Write out palette in 16bit mode
	*(unsigned short *) lump_p = 256;	// palette size
	lump_p += sizeof(short);

	memcpy(lump_p, &palLogo[0], 765);
	lump_p += 765;

	*lump_p++  = (unsigned char)(crf & 0xFF);
	
	*lump_p++  = (unsigned char)((crf >> 8) & 0xFF);
	
	*lump_p++  = (unsigned char)((crf >> 16) & 0xFF);

	return lump_p - lump_start;
}


void UpdateLogoWAD( void *phdib, int r, int g, int b )
{
	char logoname[ 32 ];
	char *pszName;
	Q_strncpy( logoname, "LOGO", sizeof( logoname ) );
	pszName = &logoname[ 0 ];

	HANDLE hdib = (HANDLE)phdib;
	COLORREF crf = RGB( r, g, b );

	if ((!pszName) || (pszName[0] == 0) || (hdib == NULL))
		return;
	// Generate lump

	unsigned char *buf = (unsigned char *)_alloca( 16384 );

	CUtlBuffer buffer( 0, 16384 );

	int width, height;
	
	int length = GrabMip (hdib, buf, pszName, crf, &width, &height);
	if ( length == 0 )
	{
		return;
	}

	bool sizevalid = false;

	if ( width == height )
	{
		if ( width == 16 ||
			 width == 32 ||
			 width == 64 )
		{
			sizevalid = true;
		}
	}

	if ( !sizevalid )
		return;

	while (length & 3)
		length++;

	// Write Header
	wadinfo_t	header;
	header.identification[0] = 'W';
	header.identification[1] = 'A';
	header.identification[2] = 'D';
	header.identification[3] = '3';
	header.numlumps = 1;     
	header.infotableofs = 0; 

	buffer.Put( &header, sizeof( wadinfo_t ) );

	// Fill Ino info table
	lumpinfo_t	info;
	Q_memset (&info, 0, sizeof(info));
	Q_strncpy(info.name, pszName, sizeof( info.name ) );
	info.filepos = (int)sizeof(wadinfo_t);
	info.size = info.disksize = length;
	info.type = TYP_LUMPY;
	info.compression = 0;
	
	// Write Lump
	buffer.Put( buf, length );

	// Write info table
	buffer.Put( &info, sizeof( lumpinfo_t ) );

	int savepos = buffer.TellPut();

	buffer.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );

	header.infotableofs = length + sizeof(wadinfo_t);

	buffer.Put( &header, sizeof( wadinfo_t ) );

	buffer.SeekPut( CUtlBuffer::SEEK_HEAD, savepos );

	// Output to file
	FileHandle_t file;
	file = g_pFullFileSystem->Open( "pldecal.wad", "wb" );
	if ( file != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFullFileSystem->Write( buffer.Base(), buffer.TellPut(), file );
		g_pFullFileSystem->Close( file );
	}

}