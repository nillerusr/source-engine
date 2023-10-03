//========= Copyright © 1996-2007, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "blob_networkbypass.h"
#include "ispsharedmemory.h"

#ifndef CLIENT_DLL
#include "npc_surface.h"
#endif


#include "tier0/memdbgon.h"

BlobNetworkBypass_t *g_pBlobNetworkBypass;

#ifdef CLIENT_DLL
CInterpolatedVar< Vector > s_PositionInterpolators[BLOB_MAX_LEVEL_PARTICLES];
CInterpolatedVar< float > s_RadiusInterpolators[BLOB_MAX_LEVEL_PARTICLES];
CInterpolatedVar< Vector > s_ClosestSurfDirInterpolators[BLOB_MAX_LEVEL_PARTICLES];
BlobParticleInterpolation_t g_BlobParticleInterpolation;
void BlobNetworkBypass_CustomDemoDataCallback( uint8 *pData, size_t iSize );
#endif

class CBlobParticleNetworkBypassAutoGame : public CAutoGameSystemPerFrame
{
public:
	virtual bool Init()
	{
		m_pSharedMemory = engine->GetSinglePlayerSharedMemorySpace( "BlobParticleNetworkBypass" );
		m_pSharedMemory->Init( sizeof( BlobNetworkBypass_t ) );
		g_pBlobNetworkBypass = (BlobNetworkBypass_t *)m_pSharedMemory->Base();

#ifdef CLIENT_DLL
		float fInterpAmount = TICK_INTERVAL * (C_BaseEntity::IsSimulatingOnAlternateTicks()?2:1);
		
		for( int i = 0; i != BLOB_MAX_LEVEL_PARTICLES; ++i )
		{
			s_PositionInterpolators[i].Setup( &g_BlobParticleInterpolation.vInterpolatedPositions[i], LATCH_ANIMATION_VAR ); //LATCH_SIMULATION_VAR, LATCH_ANIMATION_VAR
			s_PositionInterpolators[i].SetInterpolationAmount( fInterpAmount ); //fInterpAmount
			s_RadiusInterpolators[i].Setup( &g_BlobParticleInterpolation.vInterpolatedRadii[i], LATCH_ANIMATION_VAR ); //LATCH_SIMULATION_VAR, LATCH_ANIMATION_VAR
			s_RadiusInterpolators[i].SetInterpolationAmount( fInterpAmount ); //fInterpAmount
			s_ClosestSurfDirInterpolators[i].Setup( &g_BlobParticleInterpolation.vInterpolatedClosestSurfDir[i], LATCH_ANIMATION_VAR ); //LATCH_SIMULATION_VAR, LATCH_ANIMATION_VAR
			s_ClosestSurfDirInterpolators[i].SetInterpolationAmount( fInterpAmount ); //fInterpAmount
		}

		m_iOldHighestIndexUsed = 0;
		memset( &m_bOldInUse, 0, sizeof( m_bOldInUse ) );

		engine->RegisterDemoCustomDataCallback( MAKE_STRING( "BlobNetworkBypass_CustomDemoDataCallback" ), BlobNetworkBypass_CustomDemoDataCallback );
#endif
		return true;
	}

	virtual void Shutdown()
	{
		m_pSharedMemory->Release();
		m_pSharedMemory = NULL;
		g_pBlobNetworkBypass = NULL;
	}

#ifdef CLIENT_DLL
	virtual void PreRender( void );
	unsigned int m_iOldHighestIndexUsed;
	CBitVec<BLOB_MAX_LEVEL_PARTICLES> m_bOldInUse;
#else
	virtual void PreClientUpdate()
	{
		CNPC_Surface::UpdateBypassParticleData();
	}
#endif

	ISPSharedMemory *m_pSharedMemory;
};

static CBlobParticleNetworkBypassAutoGame s_CBPNBAG;




#ifndef CLIENT_DLL
int AllocateBlobNetworkBypassIndex( void )
{
	int retval;
	if( g_pBlobNetworkBypass->iNumParticlesAllocated == g_pBlobNetworkBypass->iHighestIndexUsed )
	{
		//no holes in the allocations, allocate from the end
		retval = g_pBlobNetworkBypass->iHighestIndexUsed;
		++g_pBlobNetworkBypass->iHighestIndexUsed;
	}
	else
	{
		CBitVec<BLOB_MAX_LEVEL_PARTICLES> notUsed;
		g_pBlobNetworkBypass->bCurrentlyInUse.Not( &notUsed );
		retval = notUsed.FindNextSetBit( 0 );
		Assert( retval < (int)g_pBlobNetworkBypass->iHighestIndexUsed );
	}

	++g_pBlobNetworkBypass->iNumParticlesAllocated;

	g_pBlobNetworkBypass->bCurrentlyInUse.Set( retval );
	return retval;
}

void ReleaseBlobNetworkBypassIndex( int iIndex )
{
	Assert( g_pBlobNetworkBypass->bCurrentlyInUse.IsBitSet( iIndex ) );
	g_pBlobNetworkBypass->bCurrentlyInUse.Clear( iIndex );
	g_pBlobNetworkBypass->vParticlePositions[iIndex] = vec3_origin;
	g_pBlobNetworkBypass->vParticleRadii[iIndex] = 1.0f;
	g_pBlobNetworkBypass->vParticleClosestSurfDir[iIndex] = vec3_origin;
	--g_pBlobNetworkBypass->iNumParticlesAllocated;
	Assert( iIndex < (int)g_pBlobNetworkBypass->iHighestIndexUsed );
	if( iIndex == ((int)g_pBlobNetworkBypass->iHighestIndexUsed - 1) )
	{
		//search for newest high index
		int iOldHighestIntUsed = g_pBlobNetworkBypass->iHighestIndexUsed / BITS_PER_INT;
		for( int i = iOldHighestIntUsed; i >= 0; --i )
		{
			if( (g_pBlobNetworkBypass->bCurrentlyInUse.GetDWord( i ) & (-1)) != 0 )
			{
				int iLowBit = i * BITS_PER_INT;
				int iHighBit = iLowBit + BITS_PER_INT;
				for( int j = iHighBit; --j >= iLowBit; )
				{
					if( g_pBlobNetworkBypass->bCurrentlyInUse.IsBitSet( j ) )
					{
						g_pBlobNetworkBypass->iHighestIndexUsed = (uint32)j + 1;
						break;
					}
				}
				break;
			}
		}
	}

	Assert( g_pBlobNetworkBypass->iHighestIndexUsed >= g_pBlobNetworkBypass->iNumParticlesAllocated );
}

#else

void CBlobParticleNetworkBypassAutoGame::PreRender( void )
{
	if( engine->IsRecordingDemo() && g_pBlobNetworkBypass->bDataUpdated )
	{
		//record the update, TODO: compress the data by omitting the holes

		int iMaxIndex = MAX(g_pBlobNetworkBypass->iHighestIndexUsed, m_iOldHighestIndexUsed);
		int iBitMax = (iMaxIndex / BITS_PER_INT) + 1;

		size_t iDataSize = sizeof( int ) + sizeof( float ) + sizeof( int ) + sizeof( int ) + (sizeof( int ) * iBitMax) +
							iMaxIndex*( sizeof( Vector ) + sizeof( float ) + sizeof( Vector ) );
		uint8 *pData = new uint8 [iDataSize];
		uint8 *pWrite = pData;

		//let the receiver know how much of each array to expect
		*(int *)pWrite = LittleDWord( iMaxIndex );
		pWrite += sizeof( int );

		//write the update timestamp
		*(float *)pWrite = g_pBlobNetworkBypass->fTimeDataUpdated;
		pWrite += sizeof( float );

		//record usage information, also helps us effectively compress the subsequent data by omitting the holes.
		*(int *)pWrite = LittleDWord( g_pBlobNetworkBypass->iHighestIndexUsed );
		pWrite += sizeof( int );

		*(int *)pWrite = LittleDWord( g_pBlobNetworkBypass->iNumParticlesAllocated );
		pWrite += sizeof( int );

		int *pIntParser = (int *)&g_pBlobNetworkBypass->bCurrentlyInUse;
		for( int i = 0; i != iBitMax; ++i )
		{
			//convert and write the bitfield integers
			*(int *)pWrite = LittleDWord( *pIntParser );
			pWrite += sizeof( int );
			++pIntParser;
		}

		//write positions
		memcpy( pWrite, g_pBlobNetworkBypass->vParticlePositions, sizeof( Vector ) * iMaxIndex );
		pWrite += sizeof( Vector ) * iMaxIndex;

		//write radii
		memcpy( pWrite, g_pBlobNetworkBypass->vParticleRadii, sizeof( float ) * iMaxIndex );
		pWrite += sizeof( float ) * iMaxIndex;

		//write closest surface direction
		memcpy( pWrite, g_pBlobNetworkBypass->vParticleClosestSurfDir, sizeof( Vector ) * iMaxIndex );
		pWrite += sizeof( Vector ) * iMaxIndex;

		engine->RecordDemoCustomData( BlobNetworkBypass_CustomDemoDataCallback, pData, iDataSize );

		Assert( pWrite == (pData + iDataSize) );

		delete []pData;
	}

	//invalidate interpolation on freed indices, do a quick update for brand new indices
	{
		//operate on smaller chunks based on the assumption that LARGE portions of the end of the bitvecs are empty
		CBitVec<BITS_PER_INT> *pCurrentlyInUse = (CBitVec<BITS_PER_INT> *)&g_pBlobNetworkBypass->bCurrentlyInUse;
		CBitVec<BITS_PER_INT> *pOldInUse = (CBitVec<BITS_PER_INT> *)&m_bOldInUse;
		int iStop = (MAX(g_pBlobNetworkBypass->iHighestIndexUsed, m_iOldHighestIndexUsed) / BITS_PER_INT) + 1;
		int iBaseIndex = 0;

		//float fNewIndicesUpdateTime = g_pBlobNetworkBypass->bPositionsUpdated ? g_pBlobNetworkBypass->fTimeDataUpdated : gpGlobals->curtime;

		for( int i = 0; i != iStop; ++i )
		{
			CBitVec<BITS_PER_INT> bInUseXOR;
			pCurrentlyInUse->Xor( *pOldInUse, &bInUseXOR ); //find bits that changed
			
			int j = 0;
			while( (j = bInUseXOR.FindNextSetBit( j )) != -1 )
			{
				int iChangedUsageIndex = iBaseIndex + j;
				
				if( pOldInUse->IsBitSet( iChangedUsageIndex ) )
				{
					//index no longer used
					g_BlobParticleInterpolation.vInterpolatedPositions[iChangedUsageIndex] = vec3_origin;
					s_PositionInterpolators[iChangedUsageIndex].ClearHistory();
					g_BlobParticleInterpolation.vInterpolatedRadii[iChangedUsageIndex] = 1.0f;
					s_RadiusInterpolators[iChangedUsageIndex].ClearHistory();
					g_BlobParticleInterpolation.vInterpolatedClosestSurfDir[iChangedUsageIndex] = vec3_origin;
					s_ClosestSurfDirInterpolators[iChangedUsageIndex].ClearHistory();
				}
				else
				{
					//index just started being used. Assume we got an out of band update to the position
					g_BlobParticleInterpolation.vInterpolatedPositions[iChangedUsageIndex] = g_pBlobNetworkBypass->vParticlePositions[iChangedUsageIndex];
					s_PositionInterpolators[iChangedUsageIndex].Reset( gpGlobals->curtime );
					g_BlobParticleInterpolation.vInterpolatedRadii[iChangedUsageIndex] = g_pBlobNetworkBypass->vParticleRadii[iChangedUsageIndex];
					s_RadiusInterpolators[iChangedUsageIndex].Reset( gpGlobals->curtime );
					g_BlobParticleInterpolation.vInterpolatedClosestSurfDir[iChangedUsageIndex] = g_pBlobNetworkBypass->vParticleClosestSurfDir[iChangedUsageIndex];
					s_ClosestSurfDirInterpolators[iChangedUsageIndex].Reset( gpGlobals->curtime );
					//s_PositionInterpolators[iChangedUsageIndex].NoteChanged( gpGlobals->curtime, fNewIndicesUpdateTime, true );
				}

				++j;
				if( j == BITS_PER_INT )
					break;
			}
			iBaseIndex += BITS_PER_INT;
			++pCurrentlyInUse;
			++pOldInUse;
		}

		memcpy( &m_bOldInUse, &g_pBlobNetworkBypass->bCurrentlyInUse, sizeof( m_bOldInUse ) );
		m_iOldHighestIndexUsed = g_pBlobNetworkBypass->iHighestIndexUsed;
	}

	if( g_pBlobNetworkBypass->iHighestIndexUsed == 0 )
		return;

	static ConVarRef cl_interpREF( "cl_interp" );
	//now do the interpolation of positions still in use
	{
		float fInterpTime = gpGlobals->curtime - cl_interpREF.GetFloat();

		CBitVec<BITS_PER_INT> *pIntParser = (CBitVec<BITS_PER_INT> *)&g_pBlobNetworkBypass->bCurrentlyInUse;
		int iStop = (g_pBlobNetworkBypass->iHighestIndexUsed / BITS_PER_INT) + 1;
		int iBaseIndex = 0;
		for( int i = 0; i != iStop; ++i )
		{
			int j = 0;
			while( (j = pIntParser->FindNextSetBit( j )) != -1 )
			{
				int iUpdateIndex = iBaseIndex + j;

				if( g_pBlobNetworkBypass->bDataUpdated )
				{
					g_BlobParticleInterpolation.vInterpolatedPositions[iUpdateIndex] = g_pBlobNetworkBypass->vParticlePositions[iUpdateIndex];
					s_PositionInterpolators[iUpdateIndex].NoteChanged( gpGlobals->curtime, g_pBlobNetworkBypass->fTimeDataUpdated, true );
					g_BlobParticleInterpolation.vInterpolatedRadii[iUpdateIndex] = g_pBlobNetworkBypass->vParticleRadii[iUpdateIndex];
					s_RadiusInterpolators[iUpdateIndex].NoteChanged( gpGlobals->curtime, g_pBlobNetworkBypass->fTimeDataUpdated, true );
					g_BlobParticleInterpolation.vInterpolatedClosestSurfDir[iUpdateIndex] = g_pBlobNetworkBypass->vParticleClosestSurfDir[iUpdateIndex];
					s_ClosestSurfDirInterpolators[iUpdateIndex].NoteChanged( gpGlobals->curtime, g_pBlobNetworkBypass->fTimeDataUpdated, true );
					//s_PositionInterpolators[iUpdateIndex].AddToHead( gpGlobals->curtime, &g_pBlobNetworkBypass->vParticlePositions[iUpdateIndex], false );
				}

				s_PositionInterpolators[iUpdateIndex].Interpolate( fInterpTime );
				s_RadiusInterpolators[iUpdateIndex].Interpolate( fInterpTime );
				s_ClosestSurfDirInterpolators[iUpdateIndex].Interpolate( fInterpTime );

				++j;
				if( j == BITS_PER_INT )
					break;
			}
			iBaseIndex += BITS_PER_INT;
			++pIntParser;
		}

		g_pBlobNetworkBypass->bDataUpdated = false;
	}
}


void BlobNetworkBypass_CustomDemoDataCallback( uint8 *pData, size_t iSize )
{
	// FIXME: need a version number!

	uint8 *pParse = pData;
	int iMaxIndex = LittleDWord( *(int *)pParse );
	pParse += sizeof( int );

	int iBitMax = (iMaxIndex / BITS_PER_INT) + 1;

	Assert( iSize == (sizeof( int ) + sizeof( float ) + sizeof( int ) + sizeof( int ) + (sizeof( int ) * iBitMax) +
			iMaxIndex*( sizeof( Vector ) + sizeof( float ) + sizeof( Vector ) )) );
	
	g_pBlobNetworkBypass->fTimeDataUpdated = *(float *)pParse;
	pParse += sizeof( float );

	g_pBlobNetworkBypass->iHighestIndexUsed = LittleDWord( *(int *)pParse );
	pParse += sizeof( int );

	g_pBlobNetworkBypass->iNumParticlesAllocated = LittleDWord( *(int *)pParse );
	pParse += sizeof( int );

	int *pIntParser = (int *)&g_pBlobNetworkBypass->bCurrentlyInUse;
	for( int i = 0; i != iBitMax; ++i )
	{
		//read and convert the bitfield integers
		*pIntParser = LittleDWord( *(int *)pParse );
		pParse += sizeof( int );
		++pIntParser;
	}

	//read positions
	memcpy( g_pBlobNetworkBypass->vParticlePositions, pParse, sizeof( Vector ) * iMaxIndex );
	pParse += sizeof( Vector ) * iMaxIndex;

	//read radii
	memcpy( g_pBlobNetworkBypass->vParticleRadii, pParse, sizeof( float ) * iMaxIndex );
	pParse += sizeof( float ) * iMaxIndex;

	//read closest surface direction
	memcpy( g_pBlobNetworkBypass->vParticleClosestSurfDir, pParse, sizeof( Vector ) * iMaxIndex );
	pParse += sizeof( Vector ) * iMaxIndex;

	g_pBlobNetworkBypass->bDataUpdated = true;

	Assert( pParse == (pData + iSize) );
}

#endif

