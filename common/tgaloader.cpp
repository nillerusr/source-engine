//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "tgaloader.h"
#include "tier0/dbg.h"

#pragma pack(1)
typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;
#pragma pack()

#define TGA_ATTRIBUTE_HFLIP		16
#define TGA_ATTRIBUTE_VFLIP		32


int fgetLittleShort (unsigned char **p)
{
	byte	b1, b2;

	b1 = *((*p)++);
	b2 = *((*p)++);

	return (short)(b1 + b2*256);
}

int fgetLittleLong (unsigned char **p)
{
	byte	b1, b2, b3, b4;

	b1 = *((*p)++);
	b2 = *((*p)++);
	b3 = *((*p)++);
	b4 = *((*p)++);

	return b1 + (b2<<8) + (b3<<16) + (b4<<24);
}


bool GetTGADimensions( int32 iBytes, char *pData, int * width, int *height )
{
	TargaHeader header;
	unsigned char *p = (unsigned char *)pData;
	if (width) *width = 0;
	if (height) *height = 0;

	header.id_length = *(p++);
	header.colormap_type = *(p++);
	header.image_type = *(p++);

	header.colormap_index = fgetLittleShort(&p);
	header.colormap_length = fgetLittleShort(&p);
	header.colormap_size = *(p++);
	header.x_origin = fgetLittleShort(&p);
	header.y_origin = fgetLittleShort(&p);
	header.width = fgetLittleShort(&p);
	header.height = fgetLittleShort(&p);
	header.pixel_size = *(p++);
	header.attributes = *(p++);

	if ( header.image_type != 2 && header.image_type != 10 ) 
	{
		Msg( "LoadTGA: Only type 2 and 10 targa RGB images supported\n" );
		return false;
	}

	if ( header.colormap_type !=0 || ( header.pixel_size != 32 && header.pixel_size != 24 ) )
	{
		Msg("Texture_LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");
		return false;
	}

	if (width) *width = header.width;
	if (height) *height = header.height;
	return true;
}


bool LoadTGA( int32 iBytes, char *pData, byte **rawImage, int * rawImageBytes, int * width, int *height )
{
	TargaHeader header;
	unsigned char *p = (unsigned char *)pData;
	if (width) *width = 0;
	if (height) *height = 0;

	header.id_length = *(p++);
	header.colormap_type = *(p++);
	header.image_type = *(p++);

	header.colormap_index = fgetLittleShort(&p);
	header.colormap_length = fgetLittleShort(&p);
	header.colormap_size = *(p++);
	header.x_origin = fgetLittleShort(&p);
	header.y_origin = fgetLittleShort(&p);
	header.width = fgetLittleShort(&p);
	header.height = fgetLittleShort(&p);
	header.pixel_size = *(p++);
	header.attributes = *(p++);

	if ( header.image_type != 2 && header.image_type != 10 ) 
	{
		Msg( "LoadTGA: Only type 2 and 10 targa RGB images supported\n" );
		return false;
	}

	if ( header.colormap_type !=0 || ( header.pixel_size != 32 && header.pixel_size != 24 ) )
	{
		Msg("Texture_LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");
		return false;
	}

	int columns = header.width;
	int rows = header.height;
	int numPixels = columns * rows;

	if (width) *width = header.width;
	if (height) *height = header.height;
	if (rawImageBytes) *rawImageBytes = header.width * header.height * 4; 

	*rawImage = new byte[ numPixels * 4 ];
	byte *pixbuf = *rawImage;

	if ( header.id_length != 0 )
		p += header.id_length;  // skip TARGA image comment.

	if ( header.image_type == 2 ) {  // Uncompressed, RGB images
		for(int row = rows - 1; row >=0; row-- ) 
		{
			if ( header.attributes & TGA_ATTRIBUTE_VFLIP )
				pixbuf = *rawImage + (rows-row-1)*columns*4;
			else
				pixbuf = *rawImage + row*columns*4;				

			for(int column=0; column < columns; column++) 
			{
				unsigned char red,green,blue,alphabyte;
				switch ( header.pixel_size )
				{
					case 24:

						blue = *(p++);
						green = *(p++);
						red = *(p++);
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = 255;
						break;
					case 32:
						blue = *(p++);
						green = *(p++);
						red = *(p++);
						alphabyte = *(p++);
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						break;
				}
			}
		}
	}
	else if ( header.image_type == 10 ) 
	{   
		// Runlength encoded RGB images
		unsigned char red,green,blue,alphabyte,packetHeader,packetSize,j;
		for( int row = rows - 1; row >= 0; row--) 
		{
			if ( header.attributes & TGA_ATTRIBUTE_VFLIP )
				pixbuf = *rawImage + (rows-row-1)*columns*4;
			else
				pixbuf = *rawImage + row*columns*4;

			for( int column=0; column < columns; ) {
				packetHeader=*(p++);
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) {        // run-length packet
					switch ( header.pixel_size ) 
					{
					case 24:
						blue = *(p++);
						green = *(p++);
						red = *(p++);
						alphabyte = 255;
						break;
					case 32:
					default:
						blue = *(p++);
						green = *(p++);
						red = *(p++);
						alphabyte = *(p++);
						break;
					}

					for(j=0;j<packetSize;j++) 
					{
						*pixbuf++=red;
						*pixbuf++=green;
						*pixbuf++=blue;
						*pixbuf++=alphabyte;
						column++;
						if (column==columns) { // run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = *rawImage + row*columns*4;
						}
					}
				}
				else 
				{                            // non run-length packet
					for(j=0;j<packetSize;j++) {
						switch (header.pixel_size) {
							case 24:
								blue = *(p++);
								green = *(p++);
								red = *(p++);
								*pixbuf++ = red;
								*pixbuf++ = green;
								*pixbuf++ = blue;
								*pixbuf++ = 255;
								break;
							case 32:
								blue = *(p++);
								green = *(p++);
								red = *(p++);
								alphabyte = *(p++);
								*pixbuf++ = red;
								*pixbuf++ = green;
								*pixbuf++ = blue;
								*pixbuf++ = alphabyte;
								break;
						}
						column++;
						if (column==columns) 
						{ // pixel packet run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = *rawImage + row*columns*4;
						}						
					}
				}
			}
breakOut:;
		}
	}

	return true;
}

void WriteTGA( const char *pchFileName, void *rgba, int wide, int tall )
{
	_TargaHeader header;
	memset( &header, 0x0, sizeof(header) );
	header.width = wide;
	header.height = tall;
	header.image_type = 2;
	header.pixel_size = 32;
	
	FILE *fp = fopen( pchFileName, "w+" );
	fwrite( &header, 1, sizeof(header), fp );
	fwrite( rgba, 1, wide*tall*4, fp );
	fclose(fp);
	
}


