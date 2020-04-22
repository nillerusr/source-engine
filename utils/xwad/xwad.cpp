//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <conio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <direct.h>
#include <stdarg.h>

#include "goldsrc_standin.h"

#include "wadlib.h"
#include "goldsrc_bspfile.h"


extern FILE *wadhandle;


#pragma pack( 1 )
	struct TGAHeader_t
	{
		unsigned char 	id_length;
		unsigned char	colormap_type;
		unsigned char	image_type;
		unsigned short	colormap_index;
		unsigned short	colormap_length;
		unsigned char	colormap_size;
		unsigned short	x_origin;
		unsigned short	y_origin;
		unsigned short	width;
		unsigned short	height;
		unsigned char	pixel_size;
		unsigned char	attributes;
	};
#pragma pack()


typedef enum {ST_SYNC=0, ST_RAND } synctype_t;
typedef enum { SPR_SINGLE=0, SPR_GROUP } spriteframetype_t;

typedef struct {
	int			ident;
	int			version;
	int			type;
	int			texFormat;
	float		boundingradius;
	int			width;
	int			height;
	int			numframes;
	float		beamlength;
	synctype_t	synctype;
} dsprite_t;

typedef struct {
	int			origin[2];
	int			width;
	int			height;
} dspriteframe_t;


class RGBAColor
{
public:
	unsigned char r,g,b,a;
};


const char *g_pDefaultShader = "LightmappedGeneric";
const char *g_pShader = g_pDefaultShader;
bool g_bBMPAllowTranslucent = false;
bool g_bDecal = false;
bool g_bQuiet = false;

#define MAX_VMT_PARAMS	16

struct VTexVMTParam_t
{
	const char *m_szParam;
	const char *m_szValue;
};

static VTexVMTParam_t g_VMTParams[MAX_VMT_PARAMS];

static int g_NumVMTParams = 0;

void PrintExitStuff()
{
	if ( !g_bQuiet )
	{
		printf( "Press a key to quit.\n" );
		getch();
	}
}


RGBAColor* ConvertToRGBUpsideDown( byte *pBits, int width, int height, byte *pPalette, bool *bTranslucent )
{
	RGBAColor *pRet = new RGBAColor[width*height];

	byte newPalette[256][3];
	for ( int i=0; i < 256; i++ )
	{
		newPalette[i][0] = pPalette[i*3+2];
		newPalette[i][1] = pPalette[i*3+1];
		newPalette[i][2] = pPalette[i*3+0];
	}

	// Write the lines upside-down.
	for ( int y=0; y < height; y++ )
	{
		byte *pLine = &pBits[(height-y-1)*width];
		for ( int x=0; x < width; x++ )
		{
			pRet[y*width+x].r = newPalette[pLine[x]][0];
			pRet[y*width+x].g = newPalette[pLine[x]][1];
			pRet[y*width+x].b = newPalette[pLine[x]][2];
			if ( pLine[x] == 255 )
			{
				*bTranslucent = true;
				pRet[y*width+x].a = 0;
			}
			else
			{
				pRet[y*width+x].a = 255;
			}
		}
	}

	return pRet;
}


void FloodSolidPixels( RGBAColor *pTexels, int width, int height )
{
	byte *pAlphaMap = new byte[width*height];
	byte *pNewAlphaMap = new byte[width*height];

	for ( int y=0; y < height; y++ )
		for ( int x=0; x < width; x++ )
			pAlphaMap[y*width+x] = pTexels[y*width+x].a;
	
	bool bHappy = false;
	while ( !bHappy )
	{
		bHappy = true;
	
		memcpy( pNewAlphaMap, pAlphaMap, width * height );

		for ( int y=0; y < height; y++ )
		{
			RGBAColor *pLine = &pTexels[y*width];

			for ( int x=0; x < width; x++ )
			{
				if ( pAlphaMap[y*width+x] == 0 )
				{
					int nNeighbors = 0;
					int neighborTotal[3] = {0,0,0};

					// Blend all the neighboring solid pixels.
					for ( int offsetX=-1; offsetX <= 1; offsetX++ )
					{
						for ( int offsetY=-1; offsetY <= 1; offsetY++ )
						{
							int testX = x + offsetX;
							int testY = y + offsetY;
							if ( testX >= 0 && testY >= 0 && testX < width && testY < height )
							{
								if ( pAlphaMap[testY*width+testX] )
								{
									RGBAColor *pNeighbor = &pTexels[testY*width+testX];
									++nNeighbors;
									neighborTotal[0] += pNeighbor->r;
									neighborTotal[1] += pNeighbor->g;
									neighborTotal[2] += pNeighbor->b;
								}
							}
						}
					}

					if ( nNeighbors )
					{
						pNewAlphaMap[y*width+x] = 255;
						bHappy = false;
						pLine[x].r = (byte)( neighborTotal[0] / nNeighbors );
						pLine[x].g = (byte)( neighborTotal[1] / nNeighbors );
						pLine[x].b = (byte)( neighborTotal[2] / nNeighbors );
					}
				}
			}
		}

		memcpy( pAlphaMap, pNewAlphaMap, width * height );
	}

	delete [] pAlphaMap;
	delete [] pNewAlphaMap;
}


RGBAColor* ResampleImage( RGBAColor *pRGB, int width, int height, int newWidth, int newHeight )
{
	RGBAColor *pResampled = new RGBAColor[newWidth * newHeight];
	for ( int y=0; y < newHeight; y++ )
	{
		float yPercent = (float)y / (newHeight - 1);
		float flSrcY = yPercent * (height - 1.00001f);
		int iSrcY = (int)flSrcY;
		float flYFrac = flSrcY - iSrcY;

		for ( int x=0; x < newWidth; x++ )
		{
			float xPercent = (float)x / (newWidth - 1);
			float flSrcX = xPercent * (width - 1.00001f);
			int iSrcX = (int)flSrcX;
			float flXFrac = flSrcX - iSrcX;

			byte *pSrc0 = ((byte*)&pRGB[iSrcY*width+iSrcX]);
			byte *pSrc1 = ((byte*)&pRGB[iSrcY*width+iSrcX+1]);
			byte *pSrc2 = ((byte*)&pRGB[(iSrcY+1)*width+iSrcX]);
			byte *pSrc3 = ((byte*)&pRGB[(iSrcY+1)*width+iSrcX+1]);
			byte *pDest = (byte*)&pResampled[y*newWidth+x];

			// Now blend the nearest 4 source pixels.
			for ( int i=0; i < 4; i++ )
			{
				//pDest[i] = pSrc0[i];
				float topColor    = ( pSrc0[i]*(1-flXFrac) + pSrc1[i]*flXFrac );
				float bottomColor = ( pSrc2[i]*(1-flXFrac) + pSrc3[i]*flXFrac );
				pDest[i] = (byte)( topColor*(1-flYFrac) + bottomColor*flYFrac );
			}
		}
	}
	return pResampled;
}


bool WriteTGAFile( 
	const char *pFilename, 
	bool bAllowTranslucent,
	byte *pBits, 
	int width, 
	int height, 
	byte *pPalette,
	bool bPowerOf2,
	bool *bTranslucent, 
	bool *bResized
	)
{
	*bResized = *bTranslucent = false;

	RGBAColor *pRGB = ConvertToRGBUpsideDown( pBits, width, height, pPalette, bTranslucent );

	// Unless the filename starts with '{', we don't allow translucency.
	if ( !bAllowTranslucent )
		*bTranslucent = false;

	// Flood the solid texel colors into the transparent texels.
	// Since we turn on point sampling for these textures, this only matters if we're resizing the texture.
	if ( *bTranslucent )
	{
		FloodSolidPixels( pRGB, width, height );
	}

	if ( bPowerOf2 )
	{
		// Is it not a power of 2?
		if ( (width & (width - 1)) || (height & (height - 1)) )
		{
			// Ok, resize it to the next highest power of 2.
			int newWidth = width;
			while ( (newWidth & (newWidth - 1)) )
				++newWidth;

			int newHeight = height;
			while ( (newHeight & (newHeight - 1)) )
				++newHeight;

			printf( "\t(%dx%d) -> (%dx%d)", width, height, newWidth, newHeight );

			RGBAColor *pResampled = ResampleImage( pRGB, width, height, newWidth, newHeight );
			delete [] pRGB;
			pRGB = pResampled;
			
			width = newWidth;
			height = newHeight;

			*bResized = true;
		}
	}

	// Write it..	
	TGAHeader_t hdr;
	memset( &hdr, 0, sizeof( hdr ) );

	hdr.width = width;
	hdr.height = height;
	hdr.colormap_type = 0;	// no, no colormap please
	hdr.image_type = 2;		// uncompressed, true-color
	hdr.pixel_size = 32;	// 32 bits per pixel
	
	FILE *fp = fopen( pFilename, "wb" );
	if ( !fp )
		return false;

	SafeWrite( fp, &hdr, sizeof( hdr ) );
	SafeWrite( fp, pRGB, sizeof( RGBAColor ) * width * height );	
	fclose( fp );

	delete [] pRGB;
	return true;
}


int PrintUsage( const char *pExtra, ... )
{
	va_list marker;
	va_start( marker, pExtra );
	vprintf( pExtra, marker );
	va_end( marker );

	printf( 
		"%s \n"
		"\t[-AutoDir]\n"
			"\t\tAutomatically detects -basedir and -wadfile or -bmpfile based\n"
			"\t\ton the last parameter (it must be a WAD file or a BMP file.\n"
		"\t[-decal]\n"
			"\t\tCreates VMTs for decals and creates VMTs for model decals.\n"
		"\t[-onlytex <tex name>]\n"
		"\t[-shader <shader name>]\n"
			"\t\tSpecify the shader to use in the VMT file (default\n"
			"\t\tis LightmappedGeneric.\n"
		"\t[-vtex]\n"
			"\t\tIf -vtex is specified, then it calls vtex on each newly-created\n"
			"\t\t.TGA file.\n"
		"\t[-vmtparam <ParamName> <ParamValue>]\n"
			"\t\tif -vtex was specified, passes the parameters to that process.\n"
			"\t\tUsed to add parameters to the generated .vmt file\n"
		"\t-BaseDir <basedir>\n"
		"\t-game <basedir>\n"
			"\t\tSpecifies where the root mod directory is.\n"
		"\t-WadFile <wildcard>\n"
			"\t\t-wadfile will make (power-of-2) TGAs, VTFs, and VMTs for each\n"
			"\t\ttexture in the WAD. It will also place them under a directory\n"
			"\t\twith the name of the WAD. In addition, it will create\n"
			"\t\t.resizeinfo files in the materials directory if it has to\n"
			"\t\tresize the texture. Then Hammer's File->WAD To Material\n"
			"\t\tcommand will use them to rescale texture coordinates.\n"
		"\t-bmpfile <wildcard>\n"
			"\t\t-bmpfile acts like -WadFile but for BMP files, and it'll place\n"
			"\t\tthem in the root materials directory.\n"
		"\t-sprfile <wildcard>\n"
			"\t\tActs like -bmpfile, but ports a sprite.\n"
		"\t-Transparent (BMP files only)\n"
			"\t\tIf this is set, then it will treat palette index 255 as a\n"
			"\t\ttransparent pixel."
		"\t-SubDir <subdirectory>\n"
			"\t\t-SubDir tells it what directory under materials to place the\n"
			"\t\tfinal art. If using a WAD file, then it will automatically\n"
			"\t\tuse the WAD filename if no -SubDir is specified.\n"
		"\t-Quiet\n"
			"\t\tDon't print out anything or wait for a keypress on exit.\n"
		"\n"
		, __argv[0] );
	printf( "ex: %s -vtex -BaseDir c:\\hl2\\dod -WadFile c:\\hl1\\dod\\*.wad\n", __argv[0] );
	printf( "ex: %s -vtex -BaseDir c:\\hl2\\dod -bmpfile test.bmp -SubDir models\\props\n", __argv[0] );
	printf( "ex: %s -vtex -vmtparam $ignorez 1 -BaseDir c:\\hl2\\dod -sprfile test.spr -SubDir sprites\\props\n", __argv[0] );
	
	PrintExitStuff();
	return 1;
}


// Take something like "c:/a/b/c/filename.ext" and return "filename".
void GetBaseFilename( const char *pWadFilename, char wadBaseName[512] )
{
	const char *pBase = strrchr( pWadFilename, '\\' );
	if ( strrchr( pWadFilename, '/' ) > pBase )
		pBase = strrchr( pWadFilename, '/' );

	if ( pBase )
		strcpy( wadBaseName, pBase+1 );
	else
		strcpy( wadBaseName, pWadFilename );

	char *pDot = strchr( wadBaseName, '.' );
	if ( pDot )
		*pDot = 0;
}


const char *LastSlash( const char *pSrc )
{
	const char *pRet = strrchr( pSrc, '/' );
	const char *pRet2 = strrchr( pSrc, '\\' );
	return (pRet > pRet2) ? pRet : pRet2;
}


void EnsureDirExists( const char *pDir )
{
	if ( _access( pDir, 0 ) != 0 )
	{
		// We use the shell's "md" command here instead of the _mkdir() function because 
		// md will create all the subdirectories leading up to the bottom one and _mkdir() won't.
		char cmd[1024];
		_snprintf( cmd, sizeof( cmd ), "md \"%s\"", pDir );
		system( cmd );

		if ( _access( pDir, 0 ) != 0 )
			Error( "\tCan't create directory: %s.\n", pDir );
	}
}


void WriteVMTFile( const char *pBaseDir, const char *pSubDir, const char *pName, bool bTranslucent )
{
	char vmtFilename[512];
	sprintf( vmtFilename, "%s\\materials\\%s\\%s.vmt", pBaseDir, pSubDir, pName );

	FILE *fp = fopen( vmtFilename, "wt" );
	if ( !fp )
	{
		Error( "\tWriteVMTFile failed to open %s for writing.\n", vmtFilename );
		return;
	}
	
	fprintf( fp, "\"%s\"\n{\n", g_pShader );
	fprintf( fp, "\t\"$basetexture\"\t\"%s\\%s\"\n", pSubDir, pName );
	
	if ( bTranslucent || g_bDecal )
	{
		fprintf( fp, "\t\"$alphatest\"\t\"1\"\n" );
		fprintf( fp, "\t\"$ALPHATESTREFERENCE\"\t\"0.5\"\n" );
	}

	if ( g_bDecal )
	{
		fprintf( fp, "\t\"$decal\"\t\t\"1\"\n" );
		
	}
	
	int i;
	for( i=0;i<g_NumVMTParams;i++ )
	{
		fprintf( fp, "\t\"%s\" \"%s\"\n", g_VMTParams[i].m_szParam, g_VMTParams[i].m_szValue );
	}
	
	fprintf( fp, "}" );

	fclose( fp );
}


void WriteTXTFile( const char *pBaseDir, const char *pSubDir, const char *pName )
{
	char filename[512];
	sprintf( filename, "%s\\materialsrc\\%s\\%s.txt", pBaseDir, pSubDir, pName );

	FILE *fp = fopen( filename, "wt" );
	if ( !fp )
		Error( "\tWriteTXTFile: can't open %s for writing.\n", filename );

	fprintf( fp, "\"pointsample\" \"1\"\n" );
	fclose( fp );
}


void WriteResizeInfoFile( const char *pBaseDir, const char *pSubDir, const char *pName, int width, int height )
{
	char filename[512];
	sprintf( filename, "%s\\materials\\%s\\%s.resizeinfo", pBaseDir, pSubDir, pName );

	FILE *fp = fopen( filename, "wt" );
	if ( !fp )
	{
		Error( "\tWriteResizeInfoFile failed to open %s for writing.\n", filename );
		return;
	}
	
	fprintf( fp, "%d %d", width, height );
	fclose( fp );
}


void RunVTexOnFile( const char *pBaseDir, const char *pFilename )
{
	char executableDir[MAX_PATH];
	GetModuleFileName( NULL, executableDir, sizeof( executableDir ) );
	
	char *pLastSlash = max( strrchr( executableDir, '/' ), strrchr( executableDir, '\\' ) );
	if ( !pLastSlash )
		Error( "Can't find filename in '%s'.\n", executableDir );
	
	*pLastSlash = 0;

	// Set the vproject environment variable (vtex doesn't allow game yet).
	char envStr[MAX_PATH];
	_snprintf( envStr, sizeof( envStr ), "vproject=%s", pBaseDir );
	putenv( envStr );

	// Call vtex on this texture now.
	char vtexCommand[1024];
	sprintf( vtexCommand, "%s\\vtex.exe -quiet -nopause \"%s\"", executableDir, pFilename );
	if ( system( vtexCommand ) != 0 )
	{
		Error( "\tCommand '%s' failed!\n", vtexCommand );
	}
}


void WriteOutputFiles(
	const char *pBaseDir,
	const char *pSubDir,
	const char *pName,
	bool bAllowTranslucent,
	byte *buffer,
	int width,
	int height,
	byte *pPalette,
	bool bVTex
)
{
	bool bTranslucent, bResized;
	bool bPowerOf2 = true;

	char tgaFilename[1024];
	sprintf( tgaFilename, "%s\\materialsrc\\%s\\%s.tga", pBaseDir, pSubDir, pName );
	if ( !WriteTGAFile( 
		tgaFilename, 
		bAllowTranslucent, 
		buffer, 
		width, 
		height, 
		pPalette, 
		bPowerOf2, 
		&bTranslucent, 
		&bResized ) )
	{
		Error( "\tError writing %s.\n", tgaFilename );
	}

	// Write its .VMT file.
	WriteVMTFile( pBaseDir, pSubDir, pName, bTranslucent );

	// Write a text file for it if it's translucent so we can enable pointsample for vtex.
//	if ( bTranslucent )
//		WriteTXTFile( pBaseDir, pSubDir, pName );

	// Write its .resizeinfo file.
	if ( bResized )
	{
		WriteResizeInfoFile( pBaseDir, pSubDir, pName, width, height );
	}

	if ( bVTex )
	{
		RunVTexOnFile( pBaseDir, tgaFilename );
	}
}


void EnsureDirectoriesExist( const char *pBaseDir, const char *pSubDir )
{
	char materialsrcDir[512], materialsDir[512];
	sprintf( materialsrcDir, "%s\\materialsrc\\%s", pBaseDir, pSubDir );
	sprintf( materialsDir, "%s\\materials\\%s", pBaseDir, pSubDir );
	EnsureDirExists( materialsrcDir );
	EnsureDirExists( materialsDir );
}


void ProcessWadFile( const char *pWadFilename, const char *pBaseDir, const char *pSubDir, const char *pOnlyTex, bool bVTex )
{
	if ( !g_bQuiet )
		printf( "\n\n[WADFILE %s]\n\n", pWadFilename );

	// If no -subdir was specified, then figure it out from the wad filename.
	char wadBaseName[512];
	if ( !pSubDir )
	{
		// Get the base wad filename.
		GetBaseFilename( pWadFilename, wadBaseName );
		pSubDir = wadBaseName;
	}

	EnsureDirectoriesExist( pBaseDir, pSubDir );
		

	// Now process all the images in the wad.
	W_OpenWad( pWadFilename );

	#define MAXLUMP (640*480*85/64)
	byte inbuffer[MAXLUMP];

	for (int i = 0; i < numlumps; i++) 
	{
		if ( pOnlyTex && stricmp( pOnlyTex, lumpinfo[i].name ) != 0 )
			continue;

		fseek( wadhandle, lumpinfo[i].filepos, SEEK_SET );
		SafeRead ( wadhandle, inbuffer, lumpinfo[i].size );

		miptex_t *qtex = (miptex_t *)inbuffer;
		int width = LittleLong(qtex->width);
		int height = LittleLong(qtex->height);

		if ( width <= 0 || height <= 0 || width > 5000 || height > 5000 )
		{
			if ( !g_bQuiet )
				printf("\tskipping %s @ %d  size %d (not an image?)\n", lumpinfo[i].name, lumpinfo[i].filepos, lumpinfo[i].size );
			continue;
		}
		else
		{
			if ( !g_bQuiet )
				printf( "\t%s", lumpinfo[i].name );
		}

		byte *pPalette = inbuffer + LittleLong( qtex->offsets[3] ) + width * height / 64 + 2;
		byte *psrc, *pdest;

		byte outbuffer[(640+320)*480];
	
		// The old xwad	put the mipmaps in there too, but we don't want that now (usually).
		// copy in 0 image
		psrc = inbuffer + LittleLong( qtex->offsets[0] );
		pdest = outbuffer;
		for (int t = 0; t < height; t++) 
		{
			memcpy( pdest + t * width, psrc + t * width, width );
		}

		
		WriteOutputFiles( 
			pBaseDir,				// base directory
			pSubDir,				// subdir under materials
			qtex->name,				// filename (w/o extension)
			qtex->name[0] == '{',	// allow transparency?
			outbuffer, 
			width, 
			height, 
			pPalette,
			bVTex
			);

		if ( !g_bQuiet )
			printf( "\n" );
	}
}


void ProcessBMPFile( const char *pBaseDir, const char *pSubDir, const char *pFilename, bool bVTex )
{
	if ( !g_bQuiet )
		printf( "[%s]\n", pFilename );

	if ( !pSubDir )
		pSubDir = ".";

	// First make directories under materialsrc and materials if they don't exist.
	EnsureDirectoriesExist( pBaseDir, pSubDir );

	// Read in the 8-bit BMP file.
	FILE *fp = fopen( pFilename, "rb" );
	if ( !fp )
		Error( "ProcessBMPFile( %s ) can't open the file for reading.\n", pFilename );

	BITMAPFILEHEADER bfh;
	BITMAPINFOHEADER bih;
	unsigned char pixelData[512*512];

	SafeRead( fp, &bfh, sizeof( bfh ) );
	SafeRead( fp, &bih, sizeof( bih ) );

	// Make sure it's an 8-bit one like we want.
	if ( bih.biSize != sizeof( bih ) || 
		bih.biPlanes != 1 || 
		bih.biBitCount != 8 || 
		bih.biCompression != BI_RGB ||
		bih.biHeight < 0 ||
		bih.biWidth * bih.biHeight > sizeof( pixelData ) )
	{
		Error( "ProcessBMPFile( %s ) - invalid format.\n", pFilename );
	}

	int nColorsUsed = 256;
	if ( bih.biClrUsed != 0 )
	{
		nColorsUsed = bih.biClrUsed;
		if ( nColorsUsed > 256 )
			Error( "ProcessBMPFile( %s ) - bih.biClrUsed = %d.\n", pFilename, bih.biClrUsed );
	}

	RGBQUAD quadPalette[256];
	SafeRead( fp, quadPalette, sizeof( quadPalette[0] ) * nColorsUsed );

	// Usually, bfOffBits is the same place we are at now, but sometimes it's a little different.
	fseek( fp, bfh.bfOffBits, SEEK_SET );

	// Now read the bitmap data.
	SafeRead( fp, pixelData, bih.biWidth * bih.biHeight );
	
	fclose( fp );


	// Convert the palette.
	byte palette[256][3];
	for ( int i=0; i < 256; i++ )
	{
		palette[i][0] = quadPalette[i].rgbRed;
		palette[i][1] = quadPalette[i].rgbGreen;
		palette[i][2] = quadPalette[i].rgbBlue;
	}

	// Unflip the pixel data.
	for ( int y=0; y < bih.biHeight / 2; y++ )
	{
		byte tempLine[1024];
		memcpy( tempLine, &pixelData[y*bih.biWidth], bih.biWidth );
		memcpy( &pixelData[y*bih.biWidth], &pixelData[(bih.biHeight-y-1)*bih.biWidth], bih.biWidth );
		memcpy( &pixelData[(bih.biHeight-y-1)*bih.biWidth], tempLine, bih.biWidth );
	}


	char baseFilename[512];
	GetBaseFilename( pFilename, baseFilename );

	// Save it out.
	WriteOutputFiles( 
		pBaseDir,		// base directory
		pSubDir,		// subdir under materials
		baseFilename,	// filename (w/o extension)
		g_bBMPAllowTranslucent,	// allow transparency
		pixelData, 
		bih.biWidth, 
		bih.biHeight, 
		(byte*)palette,
		bVTex
		);
}


void ProcessSPRFile( const char *pBaseDir, const char *pSubDir, const char *pFilename, bool bVTex )
{
	if ( !g_bQuiet )
		printf( "[%s]\n", pFilename );

	if ( !pSubDir )
		pSubDir = ".";

	char baseFilename[512];
	GetBaseFilename( pFilename, baseFilename );

	// First make directories under materialsrc and materials if they don't exist.
	EnsureDirectoriesExist( pBaseDir, pSubDir );

	// Read in the SPR file.
	FILE *fp = fopen( pFilename, "rb" );
	if ( !fp )
		Error( "ProcessSPRFile( %s ) can't open the file for reading.\n", pFilename );
	
	dsprite_t header;
	SafeRead( fp, &header, sizeof( header ) );

	// Make sure it's a sprite file.
	if ( ((header.ident>>0) & 0xFF) != 'I' || 
		((header.ident>>8) & 0xFF) != 'D' ||
		((header.ident>>16) & 0xFF) != 'S' ||
		((header.ident>>24) & 0xFF) != 'P' )
	{
		Warning( "WARNING: sprite %s is not a sprite file. Skipping.\n", pFilename );
		fclose( fp );
		return;
	}

	if ( header.version != 2 )
	{
		Warning( "WARNING: sprite %s is not a version 2 sprite file. Skipping.\n", pFilename );
		fclose( fp );
		return;
	}

	// Read the palette.
	short cnt;
	byte palette[768];
	SafeRead( fp, &cnt, sizeof( cnt ) );
	SafeRead( fp, palette, sizeof( palette ) );

	// Read the frames.
	int i;
	for ( i=0; i < header.numframes; i++ )
	{
		spriteframetype_t type;
		SafeRead( fp, &type, sizeof( type ) );
		if ( type == SPR_SINGLE )
		{
			dspriteframe_t frame;
			SafeRead( fp, &frame, sizeof( frame ) );
			if ( frame.width > 5000 || frame.height > 5000 || frame.width < 1 || frame.height < 1 )
			{
				Warning( "WARNING: sprite %s has an invalid frame size (%d x %d) for frame %d.\n", pFilename, frame.width, frame.height, i );
				fclose( fp );
				return;
			}
				
			byte *frameData = new byte[frame.width * frame.height];
			SafeRead( fp, frameData, frame.width * frame.height );

			Msg( "\tFrame %d   ", i );

			// Write the TGA file for this frame.
			bool bTranslucent, bResized;
			char frameFilename[512];
			_snprintf( frameFilename, sizeof( frameFilename ), "%s\\materialsrc\\%s\\%s%03d.tga", pBaseDir, pSubDir, baseFilename, i );
			if ( !WriteTGAFile( 
				frameFilename, 
				g_bBMPAllowTranslucent, 
				frameData, 
				frame.width, 
				frame.height, 
				palette, 
				true,			// allow power-of-2
				&bTranslucent, 
				&bResized ) )
			{
				Error( "\tError writing %s.\n", frameFilename );
			}

			if ( !g_bQuiet )
				printf( "\n" );

			delete [] frameData;
		}
		else if ( type == SPR_GROUP )
		{
			Error( "Sprite %s uses type SPR_GROUP. Get a programmer to add support for it.\n", pFilename );
		}
		else
		{
			Warning( "WARNING: sprite %s has an invalid frame type (%d) for frame %d.\n", pFilename, type, i );
			fclose( fp );
			return;
		}
	}
		
	fclose( fp );

	//
	// Generate a .txt file for the sprite.
	//
	char txtFilename[512];
	sprintf( txtFilename, "%s\\materialsrc\\%s\\%s.txt", pBaseDir, pSubDir, baseFilename );

	fp = fopen( txtFilename, "wt" );
	if ( !fp )
		Error( "\tProcessSPRFile: can't open %s for writing.\n", txtFilename );

	fprintf( fp, "\"startframe\" \"0\"\n" );
	fprintf( fp, "\"endframe\" \"%d\"\n", header.numframes-1 );
	fprintf( fp, "\"nomip\" \"1\"\n" );
	fprintf( fp, "\"nolod\" \"1\"\n" );
	fclose( fp );

	//
	// Run VTEX on the .txt file?
	//
	if ( bVTex )
	{
		RunVTexOnFile( pBaseDir, txtFilename );
	}

	//
	// Generate a .vmt file.
	//
	char vmtFilename[512];
	_snprintf( vmtFilename, sizeof( vmtFilename ), "%s\\materials\\%s\\%s.vmt", pBaseDir, pSubDir, baseFilename );
	fp = fopen( vmtFilename, "wt" );
	if ( !fp )
		Error( "\tProcessSPRFile: can't open %s for writing.\n", vmtFilename );

	if ( g_pShader == g_pDefaultShader )
		fprintf( fp, "\"UnlitGeneric\"\n" );
	else
		fprintf( fp, "\"%s\"\n", g_pShader );

	fprintf( fp, "{\n" );
	fprintf( fp, "\t\"$spriteorientation\" \"vp_parallel\"\n" );
	fprintf( fp, "\t\"$spriteorigin\" \"[ 0.50 0.50 ]\"\n" );
	fprintf( fp, "\t\"$basetexture\" \"%s/%s\"\n", pSubDir, baseFilename );

	for( i=0;i<g_NumVMTParams;i++ )
	{
		fprintf( fp, "\t\"%s\" \"%s\"\n", g_VMTParams[i].m_szParam, g_VMTParams[i].m_szValue );
	}

	fprintf( fp, "}" );

	fclose( fp );
}


void ExtractDirectory( const char *pFilename, char *prefix )
{
	const char *pSlash = strrchr( pFilename, '/' );
	if ( strrchr( pFilename, '\\' ) > pSlash )
		pSlash = strrchr( pFilename, '\\' );

	if ( pSlash )
	{
		memcpy( prefix, pFilename, pSlash - pFilename );
		prefix[ pSlash - pFilename ] = 0;
	}
	else
	{
		strcpy( prefix, ".\\" );
	}
}


// This allows them to have a WAD or BMP under their materialsrc directory and it'll try to figure out
// all the other parameters for them.
bool DragAndDropCheck( 
	const char **pBaseDir, 
	const char **pSubDir, 
	const char **pWadFilenames, 
	const char **pBMPFilenames,
	const char **pSPRFilenames,
	bool *bVTex )
{
	const char *pLastParam = __argv[__argc-1];

	// Get the first argument in upper case.
	char arg1[512];
	strncpy( arg1, pLastParam, sizeof( arg1 ) );
	strupr( arg1 );

	// Only handle it if there's a full path (with a colon).
	if ( !strchr( arg1, ':' ) )
		return false;

	if ( strstr( arg1, ".WAD" ) )
		*pWadFilenames = pLastParam;
	else if ( strstr( arg1, ".BMP" ) )
		*pBMPFilenames = pLastParam;
	else if ( strstr( arg1, ".SPR" ) )
		*pSPRFilenames = pLastParam;
	else
		return false;
	
	// Ok, we know that argv[1] has a valid filename. Is it under materialsrc?
	char *pMatSrc = strstr( arg1, "MATERIALSRC" );
	if ( !pMatSrc || pMatSrc == arg1 )
		return false;

	// The base directory is everything before materialsrc.
	static char baseDir[512];
	*pBaseDir = baseDir;
	memcpy( baseDir, arg1, pMatSrc-arg1 );
	baseDir[pMatSrc-arg1-1] = 0;

	// The subdirectory is everything after materialsrc, minus the filename.
	char *pSubDirSrc = pMatSrc + strlen( "MATERIALSRC" ) + 1;
	char *pEnd = strrchr( pSubDirSrc, '\\' );
	if ( strrchr( pSubDirSrc, '/' ) > pEnd )
		pEnd = strrchr( pSubDirSrc, '/' );

	if ( !pEnd )
		return false;

	static char subDir[512];
	*pSubDir = subDir;
	memcpy( subDir, pSubDirSrc, pEnd - pSubDirSrc );
	subDir[pEnd-pSubDirSrc] = 0;

	// Always use vtex in drag-and-drop mode.	
	*bVTex = true;
	return true;
}


int main (int argc, char **argv)
{
	if (argc < 2) 
	{
		return PrintUsage( "" );
	}

	bool bWriteTGA = true;
	bool bWriteBMP = false;
	bool bPowerOf2 = true;
	bool bAutoDir = false;

	bool bVTex = false;
	const char *pBaseDir = NULL;
	const char *pWadFilenames = NULL;
	const char *pBMPFilenames = NULL;
	const char *pSPRFilenames = NULL;
	const char *pSubDir = NULL;
	const char *pOnlyTex = NULL;

	// Scan for options.
	for ( int i=1; i < argc; i++ )
	{
		if ( (i+2) < argc )
		{
			if ( stricmp( argv[i], "-vmtparam" ) == 0 && g_NumVMTParams < MAX_VMT_PARAMS )
			{
				g_VMTParams[g_NumVMTParams].m_szParam = argv[i+1];
				g_VMTParams[g_NumVMTParams].m_szValue = argv[i+2];

				if( !g_bQuiet )
				{
					fprintf( stderr, "Adding .vmt parameter: \"%s\"\t\"%s\"\n", 
								g_VMTParams[g_NumVMTParams].m_szParam,
								g_VMTParams[g_NumVMTParams].m_szValue );
				}

				g_NumVMTParams++;

				i += 2;
			}
		}

		if ( (i+1) < argc )
		{
			if ( stricmp( argv[i], "-basedir" ) == 0 )
			{
				pBaseDir = argv[i+1];
				++i;
			}
			else if ( stricmp( argv[i], "-wadfile" ) == 0 )
			{
				pWadFilenames = argv[i+1];
				++i;
			}
			else if ( stricmp( argv[i], "-bmpfile" ) == 0 )
			{
				pBMPFilenames = argv[i+1];
				++i;
			}
			else if ( stricmp( argv[i], "-sprfile" ) == 0 )
			{
				pSPRFilenames = argv[i+1];
				++i;
			}
			else if ( stricmp( argv[i], "-OnlyTex" ) == 0 )
			{
				pOnlyTex = argv[i+1];
				++i;
			}
			else if ( stricmp( argv[i], "-SubDir" ) == 0 )
			{
				pSubDir = argv[i+1];
				++i;
			}
			else if ( stricmp( argv[i], "-shader" ) == 0 )
			{
				g_pShader = argv[i+1];
				++i;
			}
		}
		
		if ( stricmp( argv[i], "-AutoDir" ) == 0 )
		{
			bAutoDir = true;
		}
		else if ( stricmp( argv[i], "-Transparent" ) == 0 )
		{
			g_bBMPAllowTranslucent = true;
		}
		else if ( stricmp( argv[i], "-Decal" ) == 0 )
		{	
			g_bDecal = true;
			if ( g_pShader == g_pDefaultShader )
				g_pShader = "DecalModulate";
		}
		else if ( stricmp( argv[i], "-quiet" ) == 0 )
		{
			g_bQuiet = true;
		}
		else if ( stricmp( argv[i], "-vtex" ) == 0 )
		{
			bVTex = true;
		}
	}
	
	if ( bAutoDir )
	{
		if ( !DragAndDropCheck( &pBaseDir, &pSubDir, &pWadFilenames, &pBMPFilenames, &pSPRFilenames, &bVTex ) )
			return PrintUsage( "-AutoDir failed to setup directories." );
	}

	if ( !pBaseDir || (!pWadFilenames && !pBMPFilenames && !pSPRFilenames) )
		return PrintUsage( "Missing a parameter.\n" );

	char prefix[512];

	// Scan through each wadfile.
	if ( pWadFilenames )
	{
		ExtractDirectory( pWadFilenames, prefix );

		_finddata_t findData;
		long handle = _findfirst( pWadFilenames, &findData );
		if ( handle != -1 )
		{
			do
			{
				if ( !(findData.attrib & _A_SUBDIR) )
				{
					char fullFilename[512];
					sprintf( fullFilename, "%s\\%s", prefix, findData.name );
					ProcessWadFile( fullFilename, pBaseDir, pSubDir, pOnlyTex, bVTex );
				}
			} while( _findnext( handle, &findData ) == 0 );

			_findclose( handle );
		}
	}

	// Process BMP files.
	if ( pBMPFilenames )
	{
		ExtractDirectory( pBMPFilenames, prefix );

		_finddata_t findData;
		long handle = _findfirst( pBMPFilenames, &findData );
		if ( handle != -1 )
		{
			do
			{
				if ( !(findData.attrib & _A_SUBDIR) )
				{
					char fullFilename[512];
					sprintf( fullFilename, "%s\\%s", prefix, findData.name );
					ProcessBMPFile( pBaseDir, pSubDir, fullFilename, bVTex );
				}
			} while( _findnext( handle, &findData ) == 0 );

			_findclose( handle );
		}
	}

	if ( pSPRFilenames )
	{
		ExtractDirectory( pSPRFilenames, prefix );

		_finddata_t findData;
		long handle = _findfirst( pSPRFilenames, &findData );
		if ( handle != -1 )
		{
			do
			{
				if ( !(findData.attrib & _A_SUBDIR) )
				{
					char fullFilename[512];
					sprintf( fullFilename, "%s\\%s", prefix, findData.name );
					ProcessSPRFile( pBaseDir, pSubDir, fullFilename, bVTex );
				}
			} while( _findnext( handle, &findData ) == 0 );

			_findclose( handle );
		}
	}

	PrintExitStuff();
	return 0;
}	

