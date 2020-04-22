//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "typedefs.h"

#define MIN(x,y)     (((x) < (y)) ? (x) : (y))
#define MAX(x,y)     (((x) > (y)) ? (x) : (y))

typedef struct {
	int32 w,h;
	uint8 *data;
	uint32 *data32;
} t_i_image;

// IMAGE

t_i_image *new_image(int32 w, int32 h);
void del_image(t_i_image *img);

// TGA

t_i_image *i_load_tga(char *fname);
void i_save_tga(t_i_image *image, char *fname);

// SCALE

t_i_image *powerof2(t_i_image *img1);

// PIXEL

uint32 i_rgb_to_32(uint32 r, uint32 g, uint32 b, uint32 a);
void i_putpixel(t_i_image *img, int32 x, int32 y, uint32 co);
void i_putpixel_rgba(t_i_image *img, int32 x, int32 y, int32 r, int32 g, int32 b, int32 a);
uint32 i_getpixel(t_i_image *img, int32 x, int32 y);
int32 i_getpixel_ch(t_i_image *img, int32 x, int32 y, int32 ch);
uint32 i_pixel_alphamix(uint32 c1, uint32 c2, uint32 p);
uint32 i_pixel_multiply_n(uint32 c1, uint32 n);
uint32 i_pixel_add(uint32 co1, uint32 co2);
