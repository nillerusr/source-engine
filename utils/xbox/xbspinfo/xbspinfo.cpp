//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	Spews BSP Info
//
//=====================================================================================//

#include "xbspinfo.h"

BEGIN_BYTESWAP_DATADESC( dheader_t )
	DEFINE_FIELD( ident, FIELD_INTEGER ),
	DEFINE_FIELD( version, FIELD_INTEGER ),
	DEFINE_EMBEDDED_ARRAY( lumps, HEADER_LUMPS ),
	DEFINE_FIELD( mapRevision, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( lump_t )
	DEFINE_FIELD( fileofs, FIELD_INTEGER ),
	DEFINE_FIELD( filelen, FIELD_INTEGER ),
	DEFINE_FIELD( version, FIELD_INTEGER ),
	DEFINE_ARRAY( fourCC, FIELD_CHARACTER, 4 ),
END_BYTESWAP_DATADESC()

typedef struct
{
	const char	*pFriendlyName;
	const char	*pName;
	int			lumpNum;
} lumpName_t;

lumpName_t g_lumpNames[] =
{
	{"Entities",						"LUMP_ENTITIES",					LUMP_ENTITIES},
	{"Planes",							"LUMP_PLANES",						LUMP_PLANES},
	{"TexData",							"LUMP_TEXDATA",						LUMP_TEXDATA},
	{"Vertexes",						"LUMP_VERTEXES",					LUMP_VERTEXES},
	{"Visibility",						"LUMP_VISIBILITY",					LUMP_VISIBILITY},
	{"Nodes",							"LUMP_NODES",						LUMP_NODES},
	{"TexInfo",							"LUMP_TEXINFO",						LUMP_TEXINFO},
	{"Faces",							"LUMP_FACES",						LUMP_FACES},
	{"Face IDs",						"LUMP_FACEIDS",						LUMP_FACEIDS},
	{"Lighting",						"LUMP_LIGHTING",					LUMP_LIGHTING},
	{"Occlusion",						"LUMP_OCCLUSION",					LUMP_OCCLUSION},
	{"Leafs",							"LUMP_LEAFS",						LUMP_LEAFS},
	{"Edges",							"LUMP_EDGES",						LUMP_EDGES},
	{"Surf Edges",						"LUMP_SURFEDGES",					LUMP_SURFEDGES},
	{"Models",							"LUMP_MODELS",						LUMP_MODELS},
	{"World Lights",					"LUMP_WORLDLIGHTS",					LUMP_WORLDLIGHTS},
	{"Leaf Faces",						"LUMP_LEAFFACES",					LUMP_LEAFFACES},
	{"Leaf Brushes",					"LUMP_LEAFBRUSHES",					LUMP_LEAFBRUSHES},
	{"Brushes",							"LUMP_BRUSHES",						LUMP_BRUSHES},
	{"Brush Sides",						"LUMP_BRUSHSIDES",					LUMP_BRUSHSIDES},
	{"Areas",							"LUMP_AREAS",						LUMP_AREAS},
	{"Area Portals",					"LUMP_AREAPORTALS",					LUMP_AREAPORTALS},
	{"Disp Info",						"LUMP_DISPINFO",					LUMP_DISPINFO},
	{"Original Faces",					"LUMP_ORIGINALFACES",				LUMP_ORIGINALFACES},
	{"Phys Disp",						"LUMP_PHYSDISP",					LUMP_PHYSDISP},
	{"Phys Collide",					"LUMP_PHYSCOLLIDE",					LUMP_PHYSCOLLIDE},
	{"Vert Normals",					"LUMP_VERTNORMALS",					LUMP_VERTNORMALS},
	{"Vert Normal Indices",				"LUMP_VERTNORMALINDICES",			LUMP_VERTNORMALINDICES},
	{"Disp Lightmap Alphas",			"LUMP_DISP_LIGHTMAP_ALPHAS",		LUMP_DISP_LIGHTMAP_ALPHAS},
	{"Disp Verts",						"LUMP_DISP_VERTS",					LUMP_DISP_VERTS},
	{"Disp Lightmap Sample Positions",	"LUMP_DISP_LIGHTMAP_SAMPLE_POSITIONS", LUMP_DISP_LIGHTMAP_SAMPLE_POSITIONS},
	{"Game Lump",						"LUMP_GAME_LUMP",					LUMP_GAME_LUMP},
	{"Leaf Water Data",					"LUMP_LEAFWATERDATA",				LUMP_LEAFWATERDATA},
	{"Primitives",						"LUMP_PRIMITIVES",					LUMP_PRIMITIVES},
	{"Prim Verts",						"LUMP_PRIMVERTS",					LUMP_PRIMVERTS},
	{"Prim Indices",					"LUMP_PRIMINDICES",					LUMP_PRIMINDICES},
	{"Pak File",						"LUMP_PAKFILE",						LUMP_PAKFILE},
	{"Clip Portal Verts",				"LUMP_CLIPPORTALVERTS",				LUMP_CLIPPORTALVERTS},
	{"Cube Maps",						"LUMP_CUBEMAPS",					LUMP_CUBEMAPS},
	{"Tex Data String Data",			"LUMP_TEXDATA_STRING_DATA",			LUMP_TEXDATA_STRING_DATA},
	{"Tex Data String Table",			"LUMP_TEXDATA_STRING_TABLE",		LUMP_TEXDATA_STRING_TABLE},
	{"Overlays",						"LUMP_OVERLAYS",					LUMP_OVERLAYS},
	{"Leaf Min Dist To Water",			"LUMP_LEAFMINDISTTOWATER",			LUMP_LEAFMINDISTTOWATER},
	{"Face Macro Texture Info",			"LUMP_FACE_MACRO_TEXTURE_INFO",		LUMP_FACE_MACRO_TEXTURE_INFO},
	{"Disp Tris",						"LUMP_DISP_TRIS",					LUMP_DISP_TRIS},
	{"Phys Collide Surface",			"LUMP_PHYSCOLLIDESURFACE",			LUMP_PHYSCOLLIDESURFACE},
	{"Water Overlays",					"LUMP_WATEROVERLAYS",				LUMP_WATEROVERLAYS},
	{"Leaf Ambient index HDR",			"LUMP_LEAF_AMBIENT_INDEX_HDR",		LUMP_LEAF_AMBIENT_INDEX_HDR},
	{"Leaf Ambient index",				"LUMP_LEAF_AMBIENT_INDEX",			LUMP_LEAF_AMBIENT_INDEX},
	{"Lighting (HDR)",					"LUMP_LIGHTING_HDR",				LUMP_LIGHTING_HDR},
	{"World Lights (HDR)",				"LUMP_WORLDLIGHTS_HDR",				LUMP_WORLDLIGHTS_HDR},
	{"Leaf Ambient Lighting (HDR)",		"LUMP_LEAF_AMBIENT_LIGHTING_HDR",	LUMP_LEAF_AMBIENT_LIGHTING_HDR},
	{"Leaf Ambient Lighting",			"LUMP_LEAF_AMBIENT_LIGHTING",		LUMP_LEAF_AMBIENT_LIGHTING},
	{"*** DEAD ***",					"LUMP_XZIPPAKFILE",					LUMP_XZIPPAKFILE},
	{"Faces (HDR)",						"LUMP_FACES_HDR",					LUMP_FACES_HDR},
	{"Flags",							"LUMP_MAP_FLAGS",					LUMP_MAP_FLAGS},
	{"Fade Overlays",					"LUMP_OVERLAY_FADES",				LUMP_OVERLAY_FADES},
};

bool	g_bQuiet;
bool	g_bAsPercent;
bool	g_bAsBytes;	
bool	g_bSortByOffset;
bool	g_bSortBySize;
bool	g_bFriendlyNames;

//-----------------------------------------------------------------------------
// Convert lump ID to descriptive Name
//-----------------------------------------------------------------------------
const char *BSP_LumpNumToName( int lumpNum )
{
	int i;

	for ( i=0; i<ARRAYSIZE( g_lumpNames ); ++i )
	{
		if ( g_lumpNames[i].lumpNum == lumpNum )
		{
			if ( g_bFriendlyNames )
			{
				return g_lumpNames[i].pFriendlyName;
			}
			else
			{
				return g_lumpNames[i].pName;
			}
		}
	}

	return "???";
}

//-----------------------------------------------------------------------------
// Extract Lump Pak
//-----------------------------------------------------------------------------
void ExtractZip( const char *pFilename, void *pBSPFile )
{
	dheader_t *pBSPHeader = (dheader_t *)pBSPFile;

	if ( pBSPHeader->lumps[LUMP_PAKFILE].filelen )
	{
		Msg( "Extracting Zip to %s\n", pFilename );

		FILE *fp = fopen( pFilename, "wb" );
		if ( !fp )
		{
			Warning( "Failed to create %s\n", pFilename );
			return;
		}

		fwrite( (unsigned char *)pBSPFile + pBSPHeader->lumps[LUMP_PAKFILE].fileofs, pBSPHeader->lumps[LUMP_PAKFILE].filelen, 1, fp );
		fclose( fp );
	}
	else
	{
		Msg( "Nothing to do!\n" );
	}
}

// compare function for qsort below
static dheader_t *g_pSortBSPHeader;
static int LumpCompare( const void *pElem1, const void *pElem2 )
{
	int lump1 = *(byte *)pElem1;
	int lump2 = *(byte *)pElem2;

	int fileOffset1 = g_pSortBSPHeader->lumps[lump1].fileofs;
	int fileOffset2 = g_pSortBSPHeader->lumps[lump2].fileofs;

	int fileSize1 = g_pSortBSPHeader->lumps[lump1].filelen;
	int fileSize2 = g_pSortBSPHeader->lumps[lump2].filelen;

	if ( g_bSortByOffset )
	{
		// invalid or empty lumps will get sorted together
		if ( !fileSize1 )
		{
			fileOffset1 = 0;
		}

		if ( !fileSize2 )
		{
			fileOffset2 = 0;
		}

		// compare by offset
		if ( fileOffset1 < fileOffset2 )
		{
			return -1;
		}
		else if ( fileOffset1 > fileOffset2 )
		{
			return 1;
		}
	}
	else if ( g_bSortBySize )
	{
		if ( fileSize1 < fileSize2 )
		{
			return -1;
		}
		else if ( fileSize1 > fileSize2 )
		{
			return 1;
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Spew Info
//-----------------------------------------------------------------------------
void DumpInfo( const char *pFilename, void *pBSPFile, int bspSize )
{
	const char		*pName;
	dheader_t		*pBSPHeader;

	pBSPHeader = (dheader_t *)pBSPFile;

	Msg( "\n" );
	Msg( "%s\n", pFilename );

	// sort by offset order
	int readOrder[HEADER_LUMPS];
	for ( int i=0; i<HEADER_LUMPS; i++ )
	{
		readOrder[i] = i;
	}

	if ( g_bSortByOffset || g_bSortBySize )
	{
		g_pSortBSPHeader = pBSPHeader;
		qsort( readOrder, HEADER_LUMPS, sizeof( int ), LumpCompare );
	}

	for ( int i=0; i<HEADER_LUMPS; ++i )
	{
		int lump = readOrder[i];
		pName = BSP_LumpNumToName( lump );
		if ( !pName )
			continue;

		if ( g_bSortByOffset )
		{
			Msg( "[Offset: 0x%8.8x] ", pBSPHeader->lumps[lump].fileofs );
		}

		if ( g_bAsPercent )
		{
			Msg( "%5.2f%s (%2d) %s\n", 100.0f*pBSPHeader->lumps[lump].filelen/( float )bspSize, "%%", lump, pName );
		}
		else if ( g_bAsBytes )
		{
			Msg( "%8d: (%2d) %s\n", pBSPHeader->lumps[lump].filelen, lump, pName );
		}
		else
		{
			Msg( "%5.2f MB: (%2d) %s\n", pBSPHeader->lumps[lump].filelen/( 1024.0f*1024.0f ), lump, pName );
		}
	}

	Msg( "-------\n" );
	if ( g_bAsBytes )
	{
		Msg( "%8d: %s\n", bspSize, "Total Bytes" );
	}
	else
	{
		Msg( "%6.2f MB %s\n", bspSize/( 1024.0f*1024.0f ), "Total" );
	}
}

//-----------------------------------------------------------------------------
// Load the bsp file
//-----------------------------------------------------------------------------
bool LoadBSPFile( const char* pFilename, void **ppBSPBuffer, int *pBSPSize )
{
	CByteswap	byteSwap;

	*ppBSPBuffer = NULL;
	*pBSPSize    = 0;

	FILE *fp = fopen( pFilename, "rb" );
	if ( fp )
	{
		fseek( fp, 0, SEEK_END );
		int size = ftell( fp );
		fseek( fp, 0, SEEK_SET );

		*ppBSPBuffer = malloc( size );
		if ( !*ppBSPBuffer )
		{
			Warning( "Failed to alloc %d bytes\n", size );
			goto cleanUp;
		}

		*pBSPSize = size;
		fread( *ppBSPBuffer, size, 1, fp );
		fclose( fp );
	}
	else
	{
		if ( !g_bQuiet )
		{
			Warning( "Missing %s\n", pFilename );
		}
		goto cleanUp;
	}	

	dheader_t *pBSPHeader = (dheader_t *)*ppBSPBuffer;

	if ( pBSPHeader->ident != IDBSPHEADER )
	{
		if ( pBSPHeader->ident != BigLong( IDBSPHEADER ) )
		{
			if ( !g_bQuiet )
			{
				Warning( "BSP %s has bad id: got %d, expected %d\n", pFilename, pBSPHeader->ident, IDBSPHEADER );
			}
			goto cleanUp;
		}
		else
		{
			// bsp is for 360, swap the header
			byteSwap.ActivateByteSwapping( true );
			byteSwap.SwapFieldsToTargetEndian( pBSPHeader );
		}
	}	

	if ( pBSPHeader->version < MINBSPVERSION || pBSPHeader->version > BSPVERSION )
	{
		if ( !g_bQuiet )
		{
			Warning( "BSP %s has bad version: got %d, expected %d\n", pFilename, pBSPHeader->version, BSPVERSION );
		}
		goto cleanUp;
	}	

	// sucess
	return true;

cleanUp:
	if ( *ppBSPBuffer )
	{
		free( *ppBSPBuffer );
		*ppBSPBuffer = NULL;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Usage
//-----------------------------------------------------------------------------
void Usage( void )
{
	Msg( "usage: bspinfo <bspfile> [options]\n" );
	Msg( "options:\n" );
	Msg( "-percent, -p:       Show as percentages\n" );
	Msg( "-bytes, -b:         Show as bytes\n" );
	Msg( "-q:                 Quiet, no header, no errors\n" );
	Msg( "-names:             Show friendly lump names\n" );
	Msg( "-so:                Sort by offset\n" );
	Msg( "-ss:                Sort by size\n" );
	Msg( "-extract <zipname>: Extract pak file\n" );

	exit( -1 );
}

//-----------------------------------------------------------------------------
// Purpose: default output func
//-----------------------------------------------------------------------------
SpewRetval_t OutputFunc( SpewType_t spewType, char const *pMsg )
{
	printf( pMsg );

	if ( spewType == SPEW_ERROR )
	{
		return SPEW_ABORT;
	}
	return ( spewType == SPEW_ASSERT ) ? SPEW_DEBUGGER : SPEW_CONTINUE; 
}

//-----------------------------------------------------------------------------
//	main
//
//-----------------------------------------------------------------------------
int main( int argc, char* argv[] )
{
	char bspPath[MAX_PATH];

	// set the valve library printer
	SpewOutputFunc( OutputFunc );

	CommandLine()->CreateCmdLine( argc, argv );

	Msg( "\nXBSPINFO - Valve Xbox 360 BSP Info ( Build: %s %s )\n", __DATE__, __TIME__ );
	Msg( "( C ) Copyright 1996-2006, Valve Corporation, All rights reserved.\n\n" );

	if ( argc < 2 || CommandLine()->FindParm( "?" ) || CommandLine()->FindParm( "-h" ) || CommandLine()->FindParm( "-help" ) )
	{
		Usage();
	}

	if ( argc >= 2 && argv[1][0] != '-' )
	{
		strcpy( bspPath, argv[1] );
	}
	else
	{
		Usage();
	}

	g_bQuiet = CommandLine()->FindParm( "-q" ) != 0;
	g_bAsPercent = CommandLine()->FindParm( "-p" ) != 0 || CommandLine()->FindParm( "-percent" ) != 0;
	g_bAsBytes = CommandLine()->FindParm( "-b" ) != 0 || CommandLine()->FindParm( "-bytes" ) != 0;
	g_bSortByOffset = CommandLine()->FindParm( "-so" ) != 0;
	g_bSortBySize = CommandLine()->FindParm( "-ss" ) != 0;
	g_bFriendlyNames = CommandLine()->FindParm( "-names" ) != 0;

	void *pBSPBuffer;
	int bspSize;
	if ( LoadBSPFile( bspPath, &pBSPBuffer, &bspSize ) )
	{
		const char *pZipName = CommandLine()->ParmValue( "-extract", "" );
		if ( pZipName && pZipName[0] )
		{
			ExtractZip( pZipName, pBSPBuffer );
		}
		else
		{
			DumpInfo( bspPath, pBSPBuffer, bspSize );
		}

		free( pBSPBuffer );
	}

	return ( 0 );
}