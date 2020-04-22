#include "toollib.h"
#include "piclib.h"

byte_t*	g_tgabuffer;
byte_t*	g_tgabuffptr;

/*****************************************************************************
	TL_LoadPCX

*****************************************************************************/
void TL_LoadPCX(char* filename, byte_t** pic, byte_t** palette, int* width, int* height)
{
	byte_t*	raw;
	pcx_t*	pcx;
	int		x;
	int		y;
	int		len;
	int		databyte;
	int		runlength;
	byte_t*	out;
	byte_t*	pix;

	// load the file
	len = TL_LoadFile(filename,(void **)&raw);

	// parse the PCX file
	pcx = (pcx_t*)raw;
	raw = &pcx->data;

	pcx->xmin           = TL_LittleShort(pcx->xmin);
	pcx->ymin           = TL_LittleShort(pcx->ymin);
	pcx->xmax           = TL_LittleShort(pcx->xmax);
	pcx->ymax           = TL_LittleShort(pcx->ymax);
	pcx->hres           = TL_LittleShort(pcx->hres);
	pcx->vres           = TL_LittleShort(pcx->vres);
	pcx->bytes_per_line = TL_LittleShort(pcx->bytes_per_line);
	pcx->palette_type   = TL_LittleShort(pcx->palette_type);

	if (pcx->manufacturer != 0x0a || 
		pcx->version != 5 || 
		pcx->encoding != 1 || 
		pcx->bits_per_pixel != 8 || 
		pcx->xmax >= 640 || 
		pcx->ymax >= 480)
		TL_Error("Bad pcx file %s",filename);
	
	if (palette)
	{
		*palette = (byte_t*)TL_Malloc(768);
		memcpy(*palette,(byte_t*)pcx + len - 768,768);
	}

	if (width)
		*width = pcx->xmax+1;

	if (height)
		*height = pcx->ymax+1;

	if (!pic)
		return;

	out  = (byte_t*)TL_Malloc((pcx->ymax+1)*(pcx->xmax+1));
	*pic = out;
	pix  = out;

	for (y=0; y<=pcx->ymax; y++, pix += pcx->xmax+1)
	{
		for (x=0; x<=pcx->xmax; )
		{
			databyte = *raw++;

			if((databyte & 0xC0) == 0xC0)
			{
				runlength  = databyte & 0x3F;
				databyte = *raw++;
			}
			else
				runlength = 1;

			while (runlength-- > 0)
				pix[x++] = databyte;
		}
	}

	if (raw - (byte_t *)pcx > len)
		TL_Error("PCX file %s was malformed",filename);

	TL_Free(pcx);
}

/*****************************************************************************
	TL_SavePCX

*****************************************************************************/
void TL_SavePCX(char* filename, byte_t* data, int width, int height, byte_t* palette)
{
	int		i;
	int		j;
	int		length;
	pcx_t*	pcx;
	byte_t*	pack;
	  
	pcx = (pcx_t*)TL_Malloc(width*height*2+1000);

	pcx->manufacturer   = 0x0A;		// PCX id
	pcx->version        = 5;		// 256 color
 	pcx->encoding       = 1;		// uncompressed
	pcx->bits_per_pixel = 8;		// 256 color
	pcx->xmin           = 0;
	pcx->ymin           = 0;
	pcx->xmax           = TL_LittleShort((short)(width-1));
	pcx->ymax           = TL_LittleShort((short)(height-1));
	pcx->hres           = TL_LittleShort((short)width);
	pcx->vres           = TL_LittleShort((short)height);
	pcx->color_planes   = 1;		// chunky image
	pcx->bytes_per_line = TL_LittleShort((short)width);
	pcx->palette_type   = TL_LittleShort(2);	// not a grey scale

	// pack the image
	pack = &pcx->data;
	
	for (i=0; i<height; i++)
	{
		for (j=0; j<width; j++)
		{
			if ((*data & 0xc0) != 0xC0)
				*pack++ = *data++;
			else
			{
				*pack++ = 0xC1;
				*pack++ = *data++;
			}
		}
	}
			
	// write the palette
	*pack++ = 0x0C;	
	for (i=0; i<768; i++)
		*pack++ = *palette++;
		
	// write output file 
	length = pack - (byte_t*)pcx;
	TL_SaveFile(filename,pcx,length);

	TL_Free(pcx);
} 

/*****************************************************************************
	TGA_GetByte

*****************************************************************************/
byte_t TGA_GetByte(void)
{
	return (*g_tgabuffptr++);
}

/*****************************************************************************
	TGA_GetShort

*****************************************************************************/
short TGA_GetShort(void)
{
	byte_t	msb;
	byte_t	lsb;

	lsb = g_tgabuffptr[0];
	msb = g_tgabuffptr[1];

	g_tgabuffptr += 2;

	return ((msb<<8)|lsb);
}

/*****************************************************************************
	TL_LoadTGA

*****************************************************************************/
void TL_LoadTGA(char* name, byte_t** pixels, int* width, int* height)
{
	int				columns;
	int				rows;
	int				numPixels;
	byte_t*			pixbuf;
	int				row;
	int				column;
	byte_t*			targa_rgba;
	tga_t			targa_header;
	byte_t			red;
	byte_t			green;
	byte_t			blue;
	byte_t			alphabyte;
	byte_t			packetHeader;
	byte_t			packetSize;
	byte_t			j;

	TL_LoadFile(name,(void**)&g_tgabuffer);
	g_tgabuffptr = g_tgabuffer;

	/* load unaligned tga data */
	targa_header.id_length       = TGA_GetByte();
	targa_header.colormap_type   = TGA_GetByte();
	targa_header.image_type      = TGA_GetByte();
	targa_header.colormap_index  = TGA_GetShort();
	targa_header.colormap_length = TGA_GetShort();
	targa_header.colormap_size   = TGA_GetByte();
	targa_header.x_origin        = TGA_GetShort();
	targa_header.y_origin        = TGA_GetShort();
	targa_header.width           = TGA_GetShort();
	targa_header.height          = TGA_GetShort();
	targa_header.pixel_size      = TGA_GetByte();
	targa_header.attributes      = TGA_GetByte();

	if (targa_header.image_type != 2 && targa_header.image_type != 10) 
		TL_Error("TL_LoadTGA: %s - Only type 2 and 10 targa RGB images supported",name);

	if ((targa_header.colormap_type != 0) || (targa_header.pixel_size != 32 && targa_header.pixel_size != 24))
		TL_Error("TL_LoadTGA: %s - Only 32 or 24 bit images supported (no colormaps)",name);

	columns   = targa_header.width;
	rows      = targa_header.height;
	numPixels = columns * rows;

	if (width)
		*width = columns;
	if (height)
		*height = rows;

	targa_rgba = (byte_t*)TL_Malloc(numPixels*4);
	*pixels    = targa_rgba;

	if (targa_header.id_length != 0)
	{
		// skip TARGA image comment
		g_tgabuffptr += targa_header.id_length;
	}

	if (targa_header.image_type==2) 
	{  
		// Uncompressed, RGB images
		for (row=rows-1; row>=0; row--) 
		{
			pixbuf = targa_rgba + row*columns*4;
			for(column=0; column<columns; column++) 
			{
				switch (targa_header.pixel_size) 
				{
					case 24:
							blue      = TGA_GetByte();
							green     = TGA_GetByte();
							red       = TGA_GetByte();
							alphabyte = 255; 
							break;

					case 32:
							blue      = TGA_GetByte();
							green     = TGA_GetByte();
							red       = TGA_GetByte();
							alphabyte = TGA_GetByte();
							break;
				}

				*pixbuf++ = red;
				*pixbuf++ = green;
				*pixbuf++ = blue;
				*pixbuf++ = alphabyte;

			}
		}
	}
	else if (targa_header.image_type==10) 
	{   
		// Runlength encoded RGB images
		for (row=rows-1; row>=0; row--)
		{
			pixbuf = targa_rgba + row*columns*4;
			for(column=0; column<columns; ) 
			{
				packetHeader = TGA_GetByte();
				packetSize   = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) 
				{        
					// run-length packet
					switch (targa_header.pixel_size)
					{
						case 24:
							blue      = TGA_GetByte();
							green     = TGA_GetByte();
							red       = TGA_GetByte();
							alphabyte = 255;
							break;

						case 32:
							blue      = TGA_GetByte();
							green     = TGA_GetByte();
							red       = TGA_GetByte();
							alphabyte = TGA_GetByte();
							break;
					}
	
					for(j=0; j<packetSize; j++) 
					{
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						column++;

						if (column==columns) 
						{ 
							// run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row*columns*4;
						}
					}
				}
				else 
				{
					// non run-length packet
					for(j=0; j<packetSize; j++)
					{
						switch (targa_header.pixel_size)
						{
							case 24:
								blue      = TGA_GetByte();
								green     = TGA_GetByte();
								red       = TGA_GetByte();
								alphabyte = 255;
								break;

							case 32:
								blue      = TGA_GetByte();
								green     = TGA_GetByte();
								red       = TGA_GetByte();
								alphabyte = TGA_GetByte();
								break;
						}

						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						column++;

						if (column == columns) 
						{
							// pixel packet run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row*columns*4;
						}						
					}
				}
			}
breakOut:;
		}
	}
	
	TL_Free(g_tgabuffer);
}

/*****************************************************************************
	TL_SaveTGA

	Saves TGA.  Supports r/w 16/24/32 bpp.
*****************************************************************************/
void TL_SaveTGA(char* filename, byte_t* pixels, int width, int height, int sbpp, int tbpp)
{
	int				handle;
	tga_t			tga;
	unsigned short	rgba5551;
	unsigned long	rgba8888;
	int				r;
	int				g;
	int				b;
	int				x;
	int				y;
	int				a;
	byte_t*			tgabuffer;
	byte_t*			tgabufferptr;
	byte_t*			rawbufferptr;
	byte_t*			tempbuffer;
	byte_t*			tempbufferptr;
	int				bytesperpixel;

	// all source is upsampled into easy 32 bit rgba8888 
	// and downsampled into tga buffer
	tempbuffer = (byte_t*)TL_Malloc(width*height*4);

	if (sbpp == 16)
	{
		/* source is 16 bit rgba */
		rawbufferptr = pixels;
		for (y=0; y<height; y++)
		{
			tempbufferptr = tempbuffer + y*width*4;
			for (x=0; x<width; x++)
			{
				rgba5551 = *(unsigned short*)rawbufferptr;
				r        = (rgba5551 & 0xF800)>>11;
				g        = (rgba5551 & 0x07C0)>>6;
				b        = (rgba5551 & 0x003E)>>1;
				a        = (rgba5551 & 0x01);
				tempbufferptr[0] = (byte_t)(b * (255.0/31.0));
				tempbufferptr[1] = (byte_t)(g * (255.0/31.0));
				tempbufferptr[2] = (byte_t)(r * (255.0/31.0));
				tempbufferptr[3] = (byte_t)(a * 255.0);

				rawbufferptr  += sizeof(unsigned short);
				tempbufferptr += 4;
			}
		}
	}
	else if (sbpp == 24)
	{
		/* source is 24 bit rgba */
		rawbufferptr = pixels;
		for (y=0; y<height; y++)
		{
			tempbufferptr = tempbuffer + y*width*4;
			for (x=0; x<width; x++)
			{
				tempbufferptr[0] = rawbufferptr[0];
				tempbufferptr[1] = rawbufferptr[1];
				tempbufferptr[2] = rawbufferptr[2];
				tempbufferptr[3] = 255;

				rawbufferptr  += 3;
				tempbufferptr += 4;
			}
		}
	}
	else if (sbpp == 32)
	{
		/* source is 32 bit rgba */
		memcpy(tempbuffer,pixels,width*height*4);
	}
	else
		TL_Error("TL_SaveTGA: cannot handle source %d bits per pixel",sbpp);

	if (tbpp == 16)
		bytesperpixel = 2;
	else if (tbpp == 24)
		bytesperpixel = 3;
	else if (tbpp == 32)
		bytesperpixel = 4;
	else
		TL_Error("TL_SaveTGA: cannot handle target %d bits per pixel",tbpp);

	handle = TL_SafeOpenWrite(filename);

	/* write the targa header */
	tga.id_length       = 0;
	tga.colormap_type   = 0;
	tga.image_type      = 2;
	tga.colormap_index  = 0;
	tga.colormap_length = 0;
	tga.colormap_size   = 0;
	tga.x_origin        = 0;
	tga.y_origin        = 0;
	tga.width           = width;
	tga.height          = height;
	tga.pixel_size      = tbpp;
	tga.attributes      = 0;

	TL_SafeWrite(handle,&tga.id_length,sizeof(tga.id_length));
	TL_SafeWrite(handle,&tga.colormap_type,sizeof(tga.colormap_type));
	TL_SafeWrite(handle,&tga.image_type,sizeof(tga.image_type));
	TL_SafeWrite(handle,&tga.colormap_index,sizeof(tga.colormap_index));
	TL_SafeWrite(handle,&tga.colormap_length,sizeof(tga.colormap_length));
	TL_SafeWrite(handle,&tga.colormap_size,sizeof(tga.colormap_size));
	TL_SafeWrite(handle,&tga.x_origin,sizeof(tga.x_origin));
	TL_SafeWrite(handle,&tga.y_origin,sizeof(tga.y_origin));
	TL_SafeWrite(handle,&tga.width,sizeof(tga.width));
	TL_SafeWrite(handle,&tga.height,sizeof(tga.height));
	TL_SafeWrite(handle,&tga.pixel_size,sizeof(tga.pixel_size));
	TL_SafeWrite(handle,&tga.attributes,sizeof(tga.attributes));

	/* tga images are upside down left to right - !@#$% */
	tgabuffer = (byte_t*)TL_Malloc(width*height*bytesperpixel);

	/* source is 32 bit rgba */
	rawbufferptr = tempbuffer;
	for (y=height-1; y>=0; y--)
	{
		tgabufferptr = tgabuffer + y*width*bytesperpixel;
		for (x=0; x<width; x++)
		{
			switch (bytesperpixel)
			{
				case 2:
					break;

				case 3:
					rgba8888 = TL_BigLong(*(unsigned long*)rawbufferptr);
					r        = (rgba8888 & 0xFF000000)>>24;
					g        = (rgba8888 & 0x00FF0000)>>16;
					b        = (rgba8888 & 0x0000FF00)>>8;

					tgabufferptr[0] = b;
					tgabufferptr[1]	= g;
					tgabufferptr[2]	= r;
					break;

				case 4:
					rgba8888 = TL_BigLong(*(unsigned long*)rawbufferptr);
					r        = (rgba8888 & 0xFF000000)>>24;
					g        = (rgba8888 & 0x00FF0000)>>16;
					b        = (rgba8888 & 0x0000FF00)>>8;
					a        = rgba8888 & 0xFF;

					tgabufferptr[0] = b;
					tgabufferptr[1]	= g;
					tgabufferptr[2]	= r;
					tgabufferptr[3]	= a;
					break;
			}

			rawbufferptr += 4;
			tgabufferptr += bytesperpixel;
		}
	}

	TL_SafeWrite(handle,tgabuffer,width*height*bytesperpixel);
	close(handle);

	TL_Free(tempbuffer);
	TL_Free(tgabuffer);
}

/*****************************************************************************
	TL_LoadImage

	Loads an image based on extension.
*****************************************************************************/
void TL_LoadImage(char* name, byte_t** pixels, byte_t** palette, int* width, int* height)
{
	char	ext[16];

	TL_GetExtension(name,ext);
	if (!stricmp(ext,"pcx"))
		TL_LoadPCX(name,pixels,palette,width,height);
	else if (!stricmp(ext,"tga"))
	{
		TL_LoadTGA(name,pixels,width,height);
		*palette = NULL;
	}
	else
		TL_Error("TL_LoadImage: unknown image extension %s",ext);
}

