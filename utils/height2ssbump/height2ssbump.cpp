//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "tier0/platform.h"
#include "tier0/progressbar.h"
#include "bitmap/float_bm.h"
#include "mathlib/mathlib.h"
#include "tier2/tier2.h"
#include "tier0/memdbgon.h"

static void PrintArgSummaryAndExit( void )
{
	printf( "format is 'height2ssbump filename.tga bumpscale'\n" );
	printf( "options:\n-r NUM\tSet the number of rays (default = 250. more rays take more time).\n");
	printf( "-n\tGenerate a conventional normal map\n" );
	printf( "-A\tGenerate ambient occlusion in the alpha channel\n" );
	printf( "-f NUM\tSet smoothing filter radius (0=no filter, default = 10)\n");
	printf( "-D\tWrite out the filtered result as filterd.tga\n");
	exit(1);
}

void main(int argc,char **argv)
{
	InitCommandLineProgram( argc, argv );
	int nCurArg = 1;

	bool bNormalOnly = false;
	float flFilterRadius = 10.0;
	bool bWriteFiltered = false;

	uint32 nSSBumpFlags = 0;

	int nNumRays = 250;

	while ( ( nCurArg < argc ) && ( argv[nCurArg][0] == '-' ) )
	{
		switch( argv[nCurArg][1] )
		{
			case 'n':										// lighting from normal only
			{
				bNormalOnly = true;
			}
			break;

			case 'r':										// set # of rays
			{
				nNumRays = atoi( argv[++nCurArg] );
			}
			break;

			case 'f':										// filter radius
			{
				flFilterRadius = atof( argv[++nCurArg] );
			}
			break;

			case 'd':										// detail texture
			{
				nSSBumpFlags |= SSBUMP_MOD2X_DETAIL_TEXTURE;
			}
			break;

			case 'A':										// ambeint occlusion
			{
				nSSBumpFlags |= SSBUMP_OPTION_NONDIRECTIONAL;
			}
			break;

			case 'D':										// debug - write filtered output
			{
				bWriteFiltered = true;
			}
			break;

			case '?':										// args
			{
				PrintArgSummaryAndExit();
			}
			break;

			default:
				printf("unrecogized option %s\n", argv[nCurArg] );
				PrintArgSummaryAndExit();
		}
		nCurArg++;
		
	}
	argc -= ( nCurArg - 1 );
	argv += ( nCurArg - 1 );

	if ( argc != 3 )
	{
		PrintArgSummaryAndExit();
	}
	ReportProgress( "reading src texture", 0, 0);
	FloatBitMap_t src_texture( argv[1] );
	src_texture.TileableBilateralFilter( flFilterRadius, 2.0 / 255.0 );
	if ( bWriteFiltered )
		src_texture.WriteTGAFile( "filtered.tga" );

	FloatBitMap_t * out;

	if ( bNormalOnly )
		out = src_texture.ComputeBumpmapFromHeightInAlphaChannel( atof( argv[2] ) );
	else
		out = src_texture.ComputeSelfShadowedBumpmapFromHeightInAlphaChannel( 
			atof( argv[2] ), nNumRays, nSSBumpFlags );

	char oname[1024];
	strcpy( oname, argv[1] );
	char * dot = strchr( oname, '.' );
	if ( ! dot )
		dot = oname + strlen( oname );
	if ( bNormalOnly )
		strcpy( dot, "-bump.tga" );
	else
		strcpy( dot, "-ssbump.tga" );
	out->WriteTGAFile( oname );
	delete out;
}
