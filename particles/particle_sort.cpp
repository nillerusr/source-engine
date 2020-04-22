//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: particle system code
//
//===========================================================================//

#include <algorithm>
#include "tier0/platform.h"
#include "tier0/vprof.h"
#include "particles/particles.h"
#include "psheet.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static ALIGN16 ParticleRenderData_t s_SortedIndexList[MAX_PARTICLES_IN_A_SYSTEM] ALIGN16_POST;


enum EParticleSortKeyType
{
	SORT_KEY_NONE,
	SORT_KEY_DISTANCE,
	SORT_KEY_CREATION_TIME,
};


template<EParticleSortKeyType eSortKeyMode> void s_GenerateData( Vector CameraPos, CParticleVisibilityData *pVisibilityData, CParticleCollection *pParticles )
{
	fltx4 *pOutUnSorted = reinterpret_cast<fltx4 *>( s_SortedIndexList );

	C4VAttributeIterator pXYZ( PARTICLE_ATTRIBUTE_XYZ, pParticles );
	CM128AttributeIterator pCreationTimeStamp( PARTICLE_ATTRIBUTE_CREATION_TIME, pParticles );
	CM128AttributeIterator pAlpha( PARTICLE_ATTRIBUTE_ALPHA, pParticles );
	CM128AttributeIterator pAlpha2( PARTICLE_ATTRIBUTE_ALPHA2, pParticles );
 	CM128AttributeIterator pRadius( PARTICLE_ATTRIBUTE_RADIUS, pParticles );

	int nParticles = pParticles->m_nActiveParticles;

	FourVectors EyePos;
	EyePos.DuplicateVector( CameraPos );


	fltx4 fl4AlphaVis = ReplicateX4( pVisibilityData->m_flAlphaVisibility );
	fltx4 fl4RadVis = ReplicateX4( pVisibilityData->m_flRadiusVisibility );

	// indexing. We will generate the index as float and use magicf2i to convert to integer
	fltx4 fl4OutIdx = g_SIMD_0123; // 0 1 2 3

	fl4OutIdx = AddSIMD( fl4OutIdx, Four_2ToThe23s);							// fix as int

	bool bUseVis = pVisibilityData->m_bUseVisibility;
	bool bCameraBias = pVisibilityData->m_flCameraBias != 0.0f;
	fltx4 fl4Bias = ReplicateX4( pVisibilityData->m_flCameraBias );

	fltx4 fl4AlphaScale = ReplicateX4( 255.0 );

	do
	{
		fltx4 fl4X = pXYZ->x;
		fltx4 fl4Y = pXYZ->y;
		fltx4 fl4Z = pXYZ->z;
		
		fltx4 fl4SortKey;
		if ( eSortKeyMode == SORT_KEY_DISTANCE )
		{
			fltx4 Xdiff = SubSIMD( EyePos.x, fl4X );
			fltx4 Ydiff = SubSIMD( EyePos.y, fl4Y );
			fltx4 Zdiff = SubSIMD( EyePos.z, fl4Z );

			if ( bCameraBias )
			{
				FourVectors v4CameraBias;
				v4CameraBias.x = Xdiff;
				v4CameraBias.y = Ydiff;
				v4CameraBias.z = Zdiff;
				//v4CameraBias = VectorNormalizeFast( v4CameraBias );
				v4CameraBias.VectorNormalizeFast();
				v4CameraBias *= fl4Bias;
				fl4X = SubSIMD( fl4X, v4CameraBias.x );
				fl4Y = SubSIMD( fl4Y, v4CameraBias.y );
				fl4Z = SubSIMD( fl4Z, v4CameraBias.z );

				Xdiff = SubSIMD( EyePos.x, fl4X );
				Ydiff = SubSIMD( EyePos.y, fl4Y );
				Zdiff = SubSIMD( EyePos.z, fl4Z );
			}

			fl4SortKey = AddSIMD( MulSIMD( Xdiff, Xdiff ),
								  AddSIMD( MulSIMD( Ydiff, Ydiff ),
										   MulSIMD( Zdiff, Zdiff ) ) );
		}
		else 
		{
			Assert ( eSortKeyMode == SORT_KEY_CREATION_TIME || eSortKeyMode == SORT_KEY_NONE );
			fl4SortKey = *pCreationTimeStamp;
		}

		fltx4 fl4FinalAlpha = MulSIMD( *pAlpha, *pAlpha2 );
		fltx4 fl4FinalRadius = *pRadius;

		if ( bUseVis )
		{
			fl4FinalAlpha = MaxSIMD ( Four_Zeros, MinSIMD( Four_Ones, MulSIMD( fl4FinalAlpha, fl4AlphaVis) ) );
			fl4FinalRadius = MulSIMD( fl4FinalRadius, fl4RadVis );
		}

		// convert float 0..1 to int 0..255
		fl4FinalAlpha = AddSIMD( MulSIMD( fl4FinalAlpha, fl4AlphaScale ), Four_2ToThe23s );

		// now, we will use simd transpose to write the output
		fltx4 i4Indices = AndSIMD( fl4OutIdx, 	LoadAlignedSIMD( (float *) g_SIMD_Low16BitsMask ) );
		TransposeSIMD( fl4SortKey, i4Indices, fl4FinalRadius, fl4FinalAlpha );
		pOutUnSorted[0] = fl4SortKey;
		pOutUnSorted[1] = i4Indices;
		pOutUnSorted[2] = fl4FinalRadius;
		pOutUnSorted[3] = fl4FinalAlpha;
		
		pOutUnSorted += 4;
		fl4OutIdx = AddSIMD( fl4OutIdx, Four_Fours );

		nParticles -= 4;

		++pXYZ;
		++pAlpha;
		++pAlpha2;
		++pRadius;
	} while( nParticles > 0 );								// we're not called with 0

}



#define TREATASINT(x) ( *(  ( (int32 const *)( &(x) ) ) ) )

static bool SortLessFunc( const ParticleRenderData_t &left, const ParticleRenderData_t &right )
{
	return TREATASINT( left.m_flSortKey ) < TREATASINT( right.m_flSortKey );
	
}


void CParticleCollection::GenerateSortedIndexList( Vector vecCamera, CParticleVisibilityData *pVisibilityData, bool bSorted )
{
	VPROF_BUDGET( "CParticleCollection::GenerateSortedIndexList", VPROF_BUDGETGROUP_PARTICLE_RENDERING );

	if ( bSorted )
	{
		s_GenerateData<SORT_KEY_DISTANCE>( vecCamera, pVisibilityData, this );
	}
	else
		s_GenerateData<SORT_KEY_NONE>( vecCamera, pVisibilityData, this );

// check data
#if 0
	bool bBad = false;
	for( int i = 0; i < m_nActiveParticles; i++ )
	{
		Assert( s_SortedIndexList[i].m_nIndex == i );
		if ( s_SortedIndexList[i].m_nIndex != i )
			bBad = true;
	}
	if ( bBad )
	{
		s_GenerateData<SORT_KEY_NONE>( vecCamera, pVisibilityData, this );
	}
#endif

#ifndef SWDS
	if ( bSorted )
	{
		// sort the output in place
		std::make_heap( s_SortedIndexList, s_SortedIndexList + m_nActiveParticles, SortLessFunc );
		std::sort_heap( s_SortedIndexList, s_SortedIndexList + m_nActiveParticles, SortLessFunc );
	}
#endif
}


const ParticleRenderData_t *CParticleCollection::GetRenderList( IMatRenderContext *pRenderContext, bool bSorted, int *pNparticles, CParticleVisibilityData *pVisibilityData)
{
	if ( bSorted )
		bSorted = m_pDef->m_bShouldSort;

	Vector vecCamera;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCamera );
	*pNparticles = m_nActiveParticles;
	GenerateSortedIndexList( vecCamera, pVisibilityData, bSorted );
	return s_SortedIndexList+m_nActiveParticles;
}




