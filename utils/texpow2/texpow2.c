/*
==================================
TEXPOW2 by Iikka Keranen 2001

Loads TGA files and scales them
up to the closest power of two.
Overwrites the originals, so be
careful and make backups.
==================================
*/

#define FLIST_LEN 2000

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>   
#include <math.h>

#include "texpow2.h"

/*
======= PROTOTYPES FOR FUNCS =======
*/

long countval(char *str);

/*
======= MAIN =======
*/

// Glob vars...
long width, height;

void main(int argc, char *argv[])
{
  unsigned char *animname=NULL;
  unsigned char odname[256];
	unsigned char ofname[256];
  unsigned char *fname[FLIST_LEN];
  long flags=0, contents=0, value=0;
  long x;
  unsigned char ignpal=0;
  short opn=0, numsrc=0;
	t_i_image *in;
	t_i_image *out;

  printf("TEXPOW2 by Iikka Ker„nen 2001\n\n");
  if (argc<2)
	{
    printf("Usage: TEXPOW2 <source> [source2] [source3] ... [options]\n");
    printf("Options:\n");
    printf("-o output dir -- output directory (by default, replaces original)\n\n");
    return;
  }

  for (x=1;x<argc;x++)
  {
    if (!stricmp(argv[x]+strlen(argv[x])-4,".tga"))
    {
      fname[numsrc]=argv[x];
      numsrc++;
    }
    if (x<argc-1)
    {
      if (!strcmp(argv[x],"-o"))
      {
        strcpy(odname, argv[x+1]);
        opn=1;
      }
    }
  }

  if (numsrc==1)
    printf("%d texture to be converted...\n", numsrc);
  else
    printf("%d textures to be converted...\n", numsrc);

  for (x=0;x<numsrc;x++)
  {
    printf("%s ... ",fname[x]);
    if (!opn)  strcpy(ofname, fname[x]);
		else sprintf(ofname, "%s/%s", odname, fname[x]);

		in = i_load_tga(fname[x]);
		if (in)
		{
			out = powerof2(in);
			if (out)
			{
				i_save_tga(out, ofname);
				printf("Saved %s\n", ofname);
			}
			else
			{
				printf("Error!\n");
			}
		}
  }
}

/*
========
SCALE
========
*/

t_i_image *powerof2(t_i_image *img1)
{
	int32 x,y,w,h;
	int32 dx, dy, tx, ty;
	uint32 c1, c2, c3, c4;

	t_i_image *img;

	w = 1; h = 1;
	while (w < img1->w)
		w = w * 2;
	while (h < img1->h)
		h = h * 2;

	if (w < 2 || h < 2)
		return NULL;

	img=new_image(w, h);
	if (!img)
		return NULL;

	dx = ((img1->w) << 16) / (w);
	dy = ((img1->h) << 16) / (h);
	ty = 0;
	for (y = 0; y < h; y++)
	{
		tx = 0;
		for (x = 0; x < w; x++)
		{
			c1 = i_getpixel(img1, (tx>>16), (ty>>16));
			c2 = i_getpixel(img1, (tx>>16)+1, (ty>>16));
			c3 = i_getpixel(img1, (tx>>16), (ty>>16)+1);
			c4 = i_getpixel(img1, (tx>>16)+1, (ty>>16)+1);

			c1 = i_pixel_alphamix(c1, c2, (tx & 0xffff)>>8);
			c2 = i_pixel_alphamix(c3, c4, (tx & 0xffff)>>8);
			c1 = i_pixel_alphamix(c1, c2, (ty & 0xffff)>>8);

			i_putpixel(img, x, y, c1);

			tx += dx;
		}
		ty += dy;
	}

	return img;
}

/*
========
IMAGE
========
*/

t_i_image *new_image(int32 w, int32 h)
{
	t_i_image *img;

	img=malloc(sizeof(t_i_image));
	if (!img)
	{
		return NULL;
	}

	img->w=w;
	img->h=h;

	img->data=calloc(w*h, sizeof(uint32));
	if (!img->data)
	{
		free(img);
		return NULL;
	}

	img->data32 = (int32 *) img->data;

	return img;
}

void del_image(t_i_image *img)
{
	if (!img)
		return;

	if (img->data)
		free(img->data);

	free(img);
}

/*
=============
TGA SAVE/LOAD
=============
*/

t_i_image *i_load_tga(char *fname)
{
	uint8 id_len, pal_type, img_type;
	uint16 f_color, pal_colors;
	uint8 pal_size;
	uint16 left, top, img_w, img_h;
	uint8 bpp, des_bits;

	t_i_image *image;

	uint8 *buffer;
	uint8 *line;

	int32 x,y,po;
	uint8 die=0;

	FILE *img;

	img=fopen(fname, "rb");
	if (!img)
		return NULL;

	// load header
	id_len=fgetc(img);
	pal_type=fgetc(img);
	img_type=fgetc(img);
	f_color=fgetc(img);
	f_color+=fgetc(img)<<8;
	pal_colors=fgetc(img);
	pal_colors+=fgetc(img)<<8;
	pal_size=fgetc(img);
	left=fgetc(img);
	left+=fgetc(img)<<8;
	top=fgetc(img);
	top+=fgetc(img)<<8;
	img_w=fgetc(img);
	img_w+=fgetc(img)<<8;
	img_h=fgetc(img);
	img_h+=fgetc(img)<<8;
	bpp=fgetc(img);
	des_bits=fgetc(img);

	// check for unsupported features
	if (id_len!=0 || pal_colors!=0 || (img_type!=2 && img_type!=3))
		die=1;

	if (img_type==3 && bpp!=8)
		die=1;

	if (img_type==2 && bpp!=24 && bpp!=32)
		die=1;

	if (die)
	{
		fclose(img);	
		return NULL;
	}


	// allocate buffer for the image
	image=new_image(img_w, img_h);
	if (!image)
		return NULL;
	buffer=image->data;

	// allocate temp buffer to store each line as they're read from the file
	line=malloc(img_w*(bpp>>3));
	if (!line)
	{
		del_image(image);
		fclose(img);	
		return NULL;
	}	

	image->data32=(uint32 *)image->data;

	// actually read the image data from file
	for (y=0;y<img_h;y++)
	{
		// read a line into memory
		fread(line, 1, img_w*(bpp>>3), img);

		// convert into 32bit truecolor
		if (des_bits & 0x20)
			po=y*img_w*4;
		else
			po=(img_h-y-1)*img_w*4;
		for (x=0;x<img_w;x++)
		{
			switch(bpp)
			{
				case 8:
					buffer[po++]=line[x];
					buffer[po++]=line[x];
					buffer[po++]=line[x];
					buffer[po++]=0;
					break;
				case 24:
					buffer[po++]=line[x*3];
					buffer[po++]=line[x*3+1];
					buffer[po++]=line[x*3+2];
					buffer[po++]=0;
					break;
				case 32:
					buffer[po++]=line[x*4];
					buffer[po++]=line[x*4+1];
					buffer[po++]=line[x*4+2];
					buffer[po++]=line[x*4+3];
					break;
			}
		}
	}

	free(line);

	return image;
}

void i_save_tga(t_i_image *image, char *fname)
{
	uint16 img_w=image->w, img_h=image->h;
	uint8 *buffer;
	int32 y,x, po;
	FILE *img;

	img=fopen(fname, "wb");
	if (!img)
		return;

	// save header
	fputc(0, img);    // id_len
	fputc(0, img);    // pal_type
	fputc(2, img);    // img_type
  fputc(0, img); fputc(0, img);   // f_color
  fputc(0, img); fputc(0, img);   // pal_colors
  fputc(0, img); 									// pal_size
  fputc(0, img); fputc(0, img);   // left
  fputc(0, img); fputc(0, img);   // top
  fputc(img_w&255, img); fputc(img_w>>8, img);   // width
  fputc(img_h&255, img); fputc(img_h>>8, img);   // height
	fputc(24, img); 	// bpp
	fputc(0, img);    // des_bits
	
	buffer=image->data;

	// save the image data to file
	for (y=0;y<img_h;y++)
	{
		po=(img_h-y-1)*img_w*4;
		for (x = 0; x < img_w; x++)
		{
			fputc(buffer[po+x*4], img);
			fputc(buffer[po+x*4+1], img);
			fputc(buffer[po+x*4+2], img);
		}
		//fwrite(buffer+po, 1, img_w*4, img);
	}
}

/*
=========
PIXEL
=========
*/

uint32 i_rgb_to_32(uint32 r, uint32 g, uint32 b, uint32 a)
{
	uint32 co;

	co=(a<<24)+(r<<16)+(g<<8)+b;

	return co;
}

void i_putpixel(t_i_image *img, int32 x, int32 y, uint32 co)
{
	img->data32[(y%img->h)*img->w+(x%img->w)]=co;
}

void i_putpixel_rgba(t_i_image *img, int32 x, int32 y, int32 r, int32 g, int32 b, int32 a)
{
	uint32 co;

	co=i_rgb_to_32(r,g,b,a); //(a<<24)+(r<<16)+(g<<8)+b;

	img->data32[(y%img->h)*img->w+(x%img->w)]=co;
}

uint32 i_getpixel(t_i_image *img, int32 x, int32 y)
{
	uint32 co;

	co=img->data32[(y%img->h)*img->w+(x%img->w)];

	return co;
}

int32 i_getpixel_ch(t_i_image *img, int32 x, int32 y, int32 ch)
{
	uint32 co;

	co=img->data32[(y%img->h)*img->w+(x%img->w)];

	// ch:  0:red 1:green 2:blue 3:alpha

	switch (ch)
	{
		case 0: // red
			co=(co>>16)&255;
			break;
		case 1: // green
			co=(co>>8)&255;
			break;
		case 2: // blue
			co=co&255;
			break;
		case 3: // alpha
			co=(co>>24)&255;
			break;
		default: ;
	}

	return co;
}

uint32 i_pixel_alphamix(uint32 c1, uint32 c2, uint32 p)
{
	uint32 co; 

	co=i_pixel_add(i_pixel_multiply_n(c1,256-p), i_pixel_multiply_n(c2,p));

	return co;
}

uint32 i_pixel_multiply_n(uint32 c1, uint32 n)
{
	uint32 co,r,g,b,a;

	r=(((c1>>16)&255)*n)>>8;
	g=(((c1>>8)&255) *n)>>8;
	b=(((c1>>0)&255) *n)>>8;
	a=(((c1>>24)&255)*n)>>8;

	co=i_rgb_to_32(r,g,b,a);

	return co;
}

uint32 i_pixel_add(uint32 co1, uint32 co2)
{
	uint32 co,r,g,b,a;

	r=MIN(255, MAX(0, ((co1>>16)&255)+((co2>>16)&255)));
	g=MIN(255, MAX(0, ((co1>>8 )&255)+((co2>>8 )&255)));
	b=MIN(255, MAX(0, ((co1>>0 )&255)+((co2>>0 )&255)));
	a=MIN(255, MAX(0, ((co1>>24)&255)+((co2>>24)&255)));

	co=i_rgb_to_32(r,g,b,a);

	return co;
}

/*
========
MISC
========
*/

long countval(char *str)
{
  long val=0,n,l;

	l=strlen(str);
	for (n=0;n<l;n++)
		if (str[n]>47 && str[n]<58)
			val=val*10+str[n]-48;

	return val;
}
