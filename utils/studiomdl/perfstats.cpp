//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include <stdlib.h>
#include <tier0/dbg.h>
#include "interface.h"
#include "istudiorender.h"
#include "studio.h"
#include "optimize.h"
#include "cmdlib.h"
#include "studiomdl.h"
#include "perfstats.h"

extern void MdlError( char const *pMsg, ... );

static StudioRenderConfig_t s_StudioRenderConfig;

class CStudioDataCache : public CBaseAppSystem<IStudioDataCache>
{
public:
	bool VerifyHeaders( studiohdr_t *pStudioHdr );
	vertexFileHeader_t *CacheVertexData( studiohdr_t *pStudioHdr );
};

static CStudioDataCache	g_StudioDataCache;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CStudioDataCache, IStudioDataCache, STUDIO_DATA_CACHE_INTERFACE_VERSION, g_StudioDataCache );


/*
=================
VerifyHeaders

Minimal presence and header validation, no data loads
Return true if successful, false otherwise.
=================
*/
bool CStudioDataCache::VerifyHeaders( studiohdr_t *pStudioHdr )
{
	// default valid
	return true;
}

/*
=================
CacheVertexData

Cache model's specified dynamic data
=================
*/
vertexFileHeader_t *CStudioDataCache::CacheVertexData( studiohdr_t *pStudioHdr )
{
	// minimal implementation - return persisted data
	return (vertexFileHeader_t*)pStudioHdr->pVertexBase;
}

static void UpdateStudioRenderConfig( void )
{
	memset( &s_StudioRenderConfig, 0, sizeof(s_StudioRenderConfig) );

	s_StudioRenderConfig.bEyeMove = true;
	s_StudioRenderConfig.fEyeShiftX = 0.0f;
	s_StudioRenderConfig.fEyeShiftY = 0.0f;
	s_StudioRenderConfig.fEyeShiftZ = 0.0f;
	s_StudioRenderConfig.fEyeSize = 10.0f;
	s_StudioRenderConfig.bSoftwareSkin = false;
	s_StudioRenderConfig.bNoHardware = false;
	s_StudioRenderConfig.bNoSoftware = false;
	s_StudioRenderConfig.bTeeth = true;
	s_StudioRenderConfig.drawEntities = true;
	s_StudioRenderConfig.bFlex = true;
	s_StudioRenderConfig.bEyes = true;
	s_StudioRenderConfig.bWireframe = false;
	s_StudioRenderConfig.bDrawNormals = false;
	s_StudioRenderConfig.skin = 0;
	s_StudioRenderConfig.maxDecalsPerModel = 0;
	s_StudioRenderConfig.bWireframeDecals = false;
	s_StudioRenderConfig.fullbright = false;
	s_StudioRenderConfig.bSoftwareLighting = false;
	s_StudioRenderConfig.bShowEnvCubemapOnly = false;
	g_pStudioRender->UpdateConfig( s_StudioRenderConfig );
}

static SpewOutputFunc_t				s_pSavedSpewFunc;

SpewRetval_t NullSpewOutputFunc( SpewType_t spewType, const tchar *pMsg )
{
	switch( spewType )
	{
	case SPEW_WARNING:
		return SPEW_CONTINUE;
	case SPEW_MESSAGE:
	case SPEW_ASSERT:
	case SPEW_ERROR:
	case SPEW_LOG:
		Assert( s_pSavedSpewFunc );
		if( s_pSavedSpewFunc )
		{
			return s_pSavedSpewFunc( spewType, pMsg );
		}
		break;
	}
	Assert( 0 );
	return SPEW_CONTINUE;
}

void SpewPerfStats( studiohdr_t *pStudioHdr, const char *pFilename, unsigned int flags )
{
	char							fileName[260];
	vertexFileHeader_t				*pNewVvdHdr;
	vertexFileHeader_t				*pVvdHdr = 0;
	OptimizedModel::FileHeader_t	*pVtxHdr = 0;
	studiohwdata_t					studioHWData;
	int								vvdSize = 0;
	const char						*prefix[] = {".dx80.vtx", ".dx90.vtx", ".sw.vtx"};
	s_pSavedSpewFunc				= NULL;
	if( !( flags & SPEWPERFSTATS_SHOWSTUDIORENDERWARNINGS ) )
	{
		s_pSavedSpewFunc = GetSpewOutputFunc();
		SpewOutputFunc( NullSpewOutputFunc );
	}

	// no stats on these
	if (!pStudioHdr->numbodyparts)
		return;

	// Need to update the render config to spew perf stats.
	UpdateStudioRenderConfig();

	// persist the vvd data
	Q_StripExtension( pFilename, fileName, sizeof( fileName ) );
	strcat( fileName, ".vvd" );

	if (FileExists( fileName ))
	{
		vvdSize = LoadFile( fileName, (void**)&pVvdHdr );
	}
	else
	{
		MdlError( "Could not open '%s'\n", fileName );
	}

	// validate header
	if (pVvdHdr->id != MODEL_VERTEX_FILE_ID)
	{
		MdlError( "Bad id for '%s' (got %d expected %d)\n", fileName, pVvdHdr->id, MODEL_VERTEX_FILE_ID);
	}
	if (pVvdHdr->version != MODEL_VERTEX_FILE_VERSION)
	{
		MdlError( "Bad version for '%s' (got %d expected %d)\n", fileName, pVvdHdr->version, MODEL_VERTEX_FILE_VERSION);
	}
	if (pVvdHdr->checksum != pStudioHdr->checksum)
	{
		MdlError( "Bad checksum for '%s' (got %d expected %d)\n", fileName, pVvdHdr->checksum, pStudioHdr->checksum);
	}

	if (pVvdHdr->numFixups)
	{
		// need to perform mesh relocation fixups
		// allocate a new copy
		pNewVvdHdr = (vertexFileHeader_t *)malloc( vvdSize );
		if (!pNewVvdHdr)
		{
			MdlError( "Error allocating %d bytes for Vertex File '%s'\n", vvdSize, fileName );
		}

		Studio_LoadVertexes( pVvdHdr, pNewVvdHdr, 0, true );

		// discard original
		free( pVvdHdr );

		pVvdHdr = pNewVvdHdr;
	}
	
	// iterate all ???.vtx files
	for (int j=0; j<sizeof(prefix)/sizeof(prefix[0]); j++)
	{
		// make vtx filename
		Q_StripExtension( pFilename, fileName, sizeof( fileName ) );
		strcat( fileName, prefix[j] );

		// persist the vtx data
		if (FileExists(fileName))
		{
			LoadFile( fileName, (void**)&pVtxHdr );
		}
		else
		{
			MdlError( "Could not open '%s'\n", fileName );
		}

		// validate header
		if (pVtxHdr->version != OPTIMIZED_MODEL_FILE_VERSION)
		{
			MdlError( "Bad version for '%s' (got %d expected %d)\n", fileName, pVtxHdr->version, OPTIMIZED_MODEL_FILE_VERSION );
		}
		if (pVtxHdr->checkSum != pStudioHdr->checksum)
		{
			MdlError( "Bad checksum for '%s' (got %d expected %d)\n", fileName, pVtxHdr->checkSum, pStudioHdr->checksum );
		}

		// studio render will request these through cache interface
		pStudioHdr->pVertexBase = (void *)pVvdHdr;
		pStudioHdr->pIndexBase  = (void *)pVtxHdr;

		g_pStudioRender->LoadModel( pStudioHdr, pVtxHdr, &studioHWData );

		if( flags & SPEWPERFSTATS_SHOWPERF )
		{
			if(  flags & SPEWPERFSTATS_SPREADSHEET )
			{
				printf( "%s,%s,%d,", fileName, prefix[j], studioHWData.m_NumLODs - studioHWData.m_RootLOD );
			}
			else
			{
				printf( "\n" );
				printf( "Performance Stats: %s\n", fileName );
				printf( "------------------\n" );
			}
		}

		int i;
		if( flags & SPEWPERFSTATS_SHOWPERF )
		{
			for( i = studioHWData.m_RootLOD; i < studioHWData.m_NumLODs; i++ )
			{
				DrawModelInfo_t drawModelInfo;
				drawModelInfo.m_Skin = 0;
				drawModelInfo.m_Body = 0;
				drawModelInfo.m_HitboxSet = 0;
				drawModelInfo.m_pClientEntity = 0;
				drawModelInfo.m_pColorMeshes = 0;
				drawModelInfo.m_pStudioHdr = pStudioHdr;
				drawModelInfo.m_pHardwareData = &studioHWData;	
				CUtlBuffer statsOutput( 0, 0, CUtlBuffer::TEXT_BUFFER );
				if( !( flags & SPEWPERFSTATS_SPREADSHEET ) )
				{
					printf( "LOD:%d\n", i );
				}
				drawModelInfo.m_Lod = i;

				DrawModelResults_t results;
				g_pStudioRender->GetPerfStats( &results, drawModelInfo, &statsOutput );
				if( flags & SPEWPERFSTATS_SPREADSHEET )
				{
					printf( "%d,%d,%d,", results.m_ActualTriCount, results.m_NumBatches, results.m_NumMaterials  );
				}
				else
				{
					printf( "    actual tris:%d\n", ( int )results.m_ActualTriCount );
					printf( "    texture memory bytes: %d (only valid in a rendering app)\n", ( int )results.m_TextureMemoryBytes );
					printf( ( char * )statsOutput.Base() );
				}
			}
			if( flags & SPEWPERFSTATS_SPREADSHEET )
			{
				printf( "\n" );
			}
		}
		g_pStudioRender->UnloadModel( &studioHWData );
		free(pVtxHdr);
	}

	if (pVvdHdr)
		free(pVvdHdr);

	if( !( flags & SPEWPERFSTATS_SHOWSTUDIORENDERWARNINGS ) )
	{
		SpewOutputFunc( s_pSavedSpewFunc );
	}
}

