//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// A simple tool for converting SpeedTree .spt files into .smd files
// for use in Source
//
//===========================================================================//

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "filesystem_tools.h"
#include "cmdlib.h"
#include "mathlib/mathlib.h"

#include "tier0/icommandline.h"

#include "SpeedTreeRT.h"


void OutputMaterialFile( char *filestub, bool bEnableAlphaTest, float alphaTest=0.0f )
{
	char filename[MAX_PATH];
	_snprintf( filename, MAX_PATH, "%smaterials\\Trees\\%s_VertexLit.vmt", gamedir, filestub );
	FILE *file = fopen( filename, "wt" );
	if( !file )
		return;

	fprintf( file, "\"VertexLitGeneric\"\n" );
	fprintf( file, "{\n" );
	fprintf( file, "\t\"$basetexture\" \"trees/%s\"\n", filestub);
	fprintf( file, "\t\"$model\" \"1\"\n" );
	fprintf( file, "\t\"$alphatest\" \"%d\"\n", bEnableAlphaTest?1:0 );
	fprintf( file, "\t\"$alphatestreference\" \"%f\"\n", alphaTest );
	fprintf( file, "}\n\n" );

	fclose( file );
}


void OutputLeafMaterialFile( char *filestub, bool bEnableAlphaTest, float alphaTest=0.0f, float *pCenter=NULL )
{
	char filename[MAX_PATH];
	_snprintf( filename, MAX_PATH, "%smaterials\\Trees\\%s_TreeLeaf.vmt", gamedir, filestub );
	FILE *file = fopen( filename, "wt" );
	if( !file )
		return;

	fprintf( file, "\"TreeLeaf\"\n" );
	fprintf( file, "{\n" );
	fprintf( file, "\t\"$basetexture\" \"trees/%s\"\n", filestub);
	fprintf( file, "\t\"$model\" \"1\"\n" );
	fprintf( file, "\t\"$alphatest\" \"%d\"\n", bEnableAlphaTest?1:0 );
	fprintf( file, "\t\"$alphatestreference\" \"%f\"\n", alphaTest );
	fprintf( file, "\t\"$leafcenter\" \"[ %f %f %f ]\"\n", pCenter[0], pCenter[1], pCenter[2] );
	fprintf( file, "}\n\n" );

	fclose( file );
}


void OutputTreeGeometry( CSpeedTreeRT &speedTree, CSpeedTreeRT::SGeometry &sGeom, char *filename )
{
	FILE *file = fopen( filename, "wt" );
	if( !file )
		return;

	fprintf( file, "version 1\n" );

	fprintf( file, "nodes\n" );
	fprintf( file, "0 \"Tree\" -1\n" );
	fprintf( file, "end\n" );

	fprintf( file, "skeleton\n" );
	fprintf( file, "time 0\n" );
	fprintf( file, "0 0.0 0.0 0.0 0.0 0.0 0.0\n" );
	fprintf( file, "end\n" );

	fprintf( file, "triangles\n" );

	CSpeedTreeRT::STextures sTextures;
	speedTree.GetTextures( sTextures );

	char branchTextureName[ MAX_PATH ];
	_splitpath( sTextures.m_pBranchTextureFilename, NULL, NULL, branchTextureName, NULL );

	float alphaTest = sGeom.m_fBranchAlphaTestValue/255.0f;
	OutputMaterialFile( branchTextureName, false, alphaTest );

	for( int nStrip=0;nStrip<sGeom.m_sBranches.m_usNumStrips;nStrip++ )
	{
		int nStripLength = sGeom.m_sBranches.m_pStripLengths[ nStrip ];
		const unsigned short *pStripIndices = sGeom.m_sBranches.m_pStrips[ nStrip ];
		for( int i=0;i<nStripLength-2;i++ )
		{
			int nIndices[3] = { pStripIndices[i], pStripIndices[i+1], pStripIndices[i+2] };

			if( i%2 )
			{
				int tmp = nIndices[2];
				nIndices[2] = nIndices[1];
				nIndices[1] = tmp;
			}

			fprintf( file, "%s_VertexLit\n", branchTextureName );

			for( int j=0;j<3;j++ )
			{
				const float *pPos = &sGeom.m_sBranches.m_pCoords[ nIndices[j]*3 ];
				const float *pNormal = &sGeom.m_sBranches.m_pNormals[ nIndices[j]*3 ];
				const float *pTexCoord = &sGeom.m_sBranches.m_pTexCoords0[ nIndices[j]*2 ];

				fprintf( file, "0 %f %f %f %f %f %f %f %f 0\n", pPos[0], pPos[1], pPos[2],
																pNormal[0], pNormal[1], pNormal[2],
																pTexCoord[0], pTexCoord[1] );
			}
		}
	}

	for( unsigned int i=0;i<sTextures.m_uiFrondTextureCount;i++ )
	{
		char filestub[MAX_PATH];
		_splitpath( sTextures.m_pFrondTextureFilenames[i], NULL, NULL, filestub, NULL );

		alphaTest = sGeom.m_fFrondAlphaTestValue/255.0f;
		OutputMaterialFile( filestub, true, alphaTest );
	}

	for( int nStrip=0;nStrip<sGeom.m_sFronds.m_usNumStrips;nStrip++ )
	{
		int nStripLength = sGeom.m_sFronds.m_pStripLengths[ nStrip ];
		const unsigned short *pStripIndices = sGeom.m_sFronds.m_pStrips[ nStrip ];
		for( int i=0;i<nStripLength-2;i++ )
		{
			int nIndices[3] = { pStripIndices[i], pStripIndices[i+1], pStripIndices[i+2] };

			if( i%2 )
			{
				int tmp = nIndices[2];
				nIndices[2] = nIndices[1];
				nIndices[1] = tmp;
			}

			char frondTextureName[ MAX_PATH ];
			_splitpath( sTextures.m_pFrondTextureFilenames[0], NULL, NULL, frondTextureName, NULL );
			fprintf( file, "%s_VertexLit\n", frondTextureName );

			for( int j=0;j<3;j++ )
			{
				const float *pPos = &sGeom.m_sFronds.m_pCoords[ nIndices[j]*3 ];
				const float *pNormal = &sGeom.m_sFronds.m_pNormals[ nIndices[j]*3 ];
				const float *pTexCoord = &sGeom.m_sFronds.m_pTexCoords0[ nIndices[j]*2 ];

				fprintf( file, "0 %f %f %f %f %f %f %f %f 0\n", pPos[0], pPos[1], pPos[2],
																pNormal[0], pNormal[1], pNormal[2],
																pTexCoord[0], pTexCoord[1] );
			}
		}
	}

	float *pLeafCentres = new float[ sTextures.m_uiLeafTextureCount*3 ];
	int   *pLeafCounts = new int[ sTextures.m_uiLeafTextureCount ];
	for( unsigned int i=0;i<sTextures.m_uiLeafTextureCount;i++ )
	{
		pLeafCentres[ i*3   ] = 0.0f;
		pLeafCentres[ i*3+1 ] = 0.0f;
		pLeafCentres[ i*3+2 ] = 0.0f;
		pLeafCounts[ i ] = 0;
	}

	CSpeedTreeRT::SGeometry::SLeaf &leaves = sGeom.m_sLeaves0;
	for( int i=0;i<leaves.m_usLeafCount;i++ )
	{
		int index = leaves.m_pLeafMapIndices[i] / 2;
		char leafTextureName[ MAX_PATH ];
		_splitpath( sTextures.m_pLeafTextureFilenames[ index ], NULL, NULL, leafTextureName, NULL );

		const float *pPos = &leaves.m_pCenterCoords[ i*3 ];
		const float *pCoords = leaves.m_pLeafMapCoords[ i ];
		const float *pTex = leaves.m_pLeafMapTexCoords[i];

		float d[3] = { pCoords[8] - pCoords[0], pCoords[9] - pCoords[1], pCoords[10] - pCoords[2] };
		float size = sqrtf( d[0]*d[0] + d[1]*d[1] + d[2]*d[2] ) * 0.5f;
		
		fprintf( file, "%s_TreeLeaf\n", leafTextureName );
		fprintf( file, "0 %f %f %f %f %f 0.0 %f %f\n", pPos[0], pPos[1], pPos[2], pTex[0], pTex[1], -size, -size );
		fprintf( file, "0 %f %f %f %f %f 0.0 %f %f\n", pPos[0], pPos[1], pPos[2], pTex[2], pTex[3],  size, -size );
		fprintf( file, "0 %f %f %f %f %f 0.0 %f %f\n", pPos[0], pPos[1], pPos[2], pTex[4], pTex[5],  size,  size );
																				
		fprintf( file, "%s_TreeLeaf\n", leafTextureName );						
		fprintf( file, "0 %f %f %f %f %f 0.0 %f %f\n", pPos[0], pPos[1], pPos[2], pTex[0], pTex[1], -size, -size );
		fprintf( file, "0 %f %f %f %f %f 0.0 %f %f\n", pPos[0], pPos[1], pPos[2], pTex[4], pTex[5],  size,  size );
		fprintf( file, "0 %f %f %f %f %f 0.0 %f %f\n", pPos[0], pPos[1], pPos[2], pTex[6], pTex[7], -size,  size );

		pLeafCentres[ index*3   ] += pPos[0];
		pLeafCentres[ index*3+1 ] += pPos[1];
		pLeafCentres[ index*3+2 ] += pPos[2];
		pLeafCounts[ index ]++;
	}

	for( unsigned int i=0;i<sTextures.m_uiLeafTextureCount;i++ )
	{
		float oneOnCount = 1.0f / pLeafCounts[i];

		pLeafCentres[ i*3   ] *= oneOnCount;
		pLeafCentres[ i*3+1 ] *= oneOnCount;
		pLeafCentres[ i*3+2 ] *= oneOnCount;
	}

	for( unsigned int i=0;i<sTextures.m_uiLeafTextureCount;i++ )
	{
		char filestub[ MAX_PATH ];
		_splitpath( sTextures.m_pLeafTextureFilenames[i], NULL, NULL, filestub, NULL );

		alphaTest = sGeom.m_sLeaves0.m_fAlphaTestValue/255.0f;
		OutputLeafMaterialFile( filestub, true, alphaTest, &pLeafCentres[i*3] );
	}

	fprintf( file, "end\n\n" );

	fclose( file );
}


void OutputTreeTextures( CSpeedTreeRT &speedTree )
{
	CSpeedTreeRT::STextures sTextures;
	speedTree.GetTextures( sTextures );

	CSpeedTreeRT::SGeometry sGeom;
	speedTree.GetGeometry( sGeom, SpeedTree_AllGeometry );



	char filestub[MAX_PATH];
	_splitpath( sTextures.m_pBranchTextureFilename, NULL, NULL, filestub, NULL );

	float alphaTest = sGeom.m_fBranchAlphaTestValue/255.0f;
	OutputMaterialFile( filestub, false, alphaTest );

	for( unsigned int i=0;i<sTextures.m_uiFrondTextureCount;i++ )
	{
		_splitpath( sTextures.m_pFrondTextureFilenames[i], NULL, NULL, filestub, NULL );

		alphaTest = sGeom.m_fFrondAlphaTestValue/255.0f;
		OutputMaterialFile( filestub, true, alphaTest );
	}

	for( unsigned int i=0;i<sTextures.m_uiLeafTextureCount;i++ )
	{
		_splitpath( sTextures.m_pLeafTextureFilenames[i], NULL, NULL, filestub, NULL );

		alphaTest = sGeom.m_sLeaves0.m_fAlphaTestValue/255.0f;
		OutputLeafMaterialFile( filestub, true, alphaTest );
	}
}


void OutputQCFile( char *treeName )
{
	char smdFileName[MAX_PATH];
	sprintf( smdFileName, "%s.smd", treeName );

	char qcFileName[MAX_PATH];
	sprintf( qcFileName, "%s.qc", treeName );

	char smdAnimFileName[MAX_PATH];
	sprintf( smdAnimFileName, "%s_anim.smd", treeName );

	char mdlFileName[MAX_PATH];
	sprintf( mdlFileName, "%s.mdl", treeName );

	FILE *file = fopen( qcFileName, "wt" );

	fprintf( file, "$modelname %s\n", mdlFileName );
	fprintf( file, "$cdmaterials trees\n" );
	fprintf( file, "$scale 1\n" );
	fprintf( file, "$model %s \"%s\"\n", treeName, smdFileName );
	fprintf( file, "$sequence idle \"%s_anim\" loop fps 15\n", treeName );

	fclose( file );

	file = fopen( smdAnimFileName, "wt" );

	fprintf( file, "version 1\n" );
	fprintf( file, "nodes\n" );
	fprintf( file, "0 \"Tree\" -1\n" );
	fprintf( file, "end\n" );
	fprintf( file, "skeleton\n" );
	fprintf( file, "time 0\n" );
	fprintf( file, "0 0.0 0.0 0.0 0.0 0.0 0.0\n" );
	fprintf( file, "end\n" );

	fclose( file );
}


void main( int argc, char **argv )
{
	CommandLine()->CreateCmdLine( argc, argv );
	if( CommandLine()->ParmCount()!=2 )
	{
		printf( "usage : sptconvert <SPT file name>\n" );
		exit(0);
	}

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );

	CmdLib_InitFileSystem( CommandLine()->GetParm(1) );

	CSpeedTreeRT speedTree;

	char treeName[MAX_PATH];
	_splitpath( argv[1], NULL, NULL, treeName, NULL );

	char smdFileName[MAX_PATH];
	sprintf( smdFileName, "%s.smd", treeName );

	char qcFileName[MAX_PATH];
	sprintf( qcFileName, "%s.qc", treeName );

	OutputQCFile( treeName );

	if( speedTree.LoadTree( argv[1] ) )
	{
		if( speedTree.Compute( NULL, 1, false ) )
		{
			CSpeedTreeRT::SGeometry sGeom;
			speedTree.GetGeometry( sGeom, SpeedTree_AllGeometry );

			OutputTreeGeometry( speedTree, sGeom, smdFileName );
		}
		else
		{
			// Trouble with compute
		}
	}
	else
	{
		// Trouble with load
	}
}