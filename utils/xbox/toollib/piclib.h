#ifndef _PICLIB_H_
#define _PICLIB_H_

typedef enum
{
	ms_none,
	ms_mask,
	ms_transcolor,
	ms_lasso
}
mask_t;

typedef enum
{
	cm_none,
	cm_rle1
}
compress_t;

typedef struct
{
    char			manufacturer;
    char			version;
    char			encoding;
    char			bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char			reserved;
    char			color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char			filler[58];
    unsigned char	data;
} pcx_t;

typedef struct
{
	unsigned char 	id_length;
	unsigned char	colormap_type;
	unsigned char	image_type;
	unsigned char	pad1;				// not in file
	unsigned short	colormap_index;
	unsigned short	colormap_length;
	unsigned char	colormap_size;
	unsigned char	pad2;				// not in file
	unsigned short	x_origin;
	unsigned short	y_origin;
	unsigned short	width;
	unsigned short	height;
	unsigned char	pixel_size;
	unsigned char	attributes;
} tga_t;

extern void		TL_LoadPCX(char* filename, byte_t** picture, byte_t** palette, int* width, int* height);
extern void		TL_SavePCX(char* filename, byte_t* data, int width, int height, byte_t* palette);
extern void		TL_LoadTGA(char* name, byte_t** pixels, int* width, int* height);
extern void		TL_SaveTGA(char* filename, byte_t* pixels, int width, int height, int sbpp, int tbpp);
extern void		TL_LoadImage(char* name, byte_t** pixels, byte_t** palette, int* width, int* height);

#endif
