//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: particle system code
//
//===========================================================================//

#include "tier0/platform.h"
#include "particles/particles.h"
#include "filesystem.h"
#include "tier2/tier2.h"
#include "tier2/fileutils.h"
#include "tier2/renderutils.h"
#include "tier1/UtlStringMap.h"
#include "tier1/strtools.h"
#include "dmxloader/dmxelement.h"
#include "psheet.h"
#include "bspflags.h"
#include "const.h"
#include "particles_internal.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


void CParticleOperatorInstance::InitScalarAttributeRandomRangeBlock( 
	int attr_num, float fMin, float fMax,
	CParticleCollection *pParticles, int start_block, int n_blocks ) const
{
	size_t attr_stride;
	fltx4 *pAttr = pParticles->GetM128AttributePtrForWrite( attr_num, &attr_stride );
	pAttr += attr_stride * start_block;
	fltx4 val0 = ReplicateX4( fMin );
	fltx4 val_d = ReplicateX4( fMax - fMin );
	int nRandContext = GetSIMDRandContext();
	while( n_blocks-- )
	{
		*( pAttr ) = AddSIMD( val0, MulSIMD( RandSIMD( nRandContext ), val_d ) );
		pAttr += attr_stride;
	}
	ReleaseSIMDRandContext( nRandContext );

}

void CParticleOperatorInstance::InitScalarAttributeRandomRangeExpBlock( 
	int attr_num, float fMin, float fMax, float fExp,
	CParticleCollection *pParticles, int start_block, int n_blocks ) const
{
	size_t attr_stride;
	fltx4 *pAttr = pParticles->GetM128AttributePtrForWrite( attr_num, &attr_stride );
	pAttr += attr_stride * start_block;
	fltx4 val0 = ReplicateX4( fMin );
	fltx4 val_d = ReplicateX4( fMax - fMin );
	//fltx4 val_e = ReplicateX4( fExp );
	int nExp = (int)(4.0f * fExp);
	int nRandContext = GetSIMDRandContext();
	while( n_blocks-- )
	{
		*( pAttr ) = AddSIMD( val0, MulSIMD( Pow_FixedPoint_Exponent_SIMD( RandSIMD( nRandContext ), nExp ), val_d ) );
		pAttr += attr_stride;
	}
	ReleaseSIMDRandContext( nRandContext );
}

void CParticleOperatorInstance::AddScalarAttributeRandomRangeBlock( 
	int nAttributeId, float fMin, float fMax, float fExp,
	CParticleCollection *pParticles, int nStartBlock, int nBlockCount, bool bRandomlyInvert ) const
{
	size_t nAttrStride;
	fltx4 *pAttr = pParticles->GetM128AttributePtrForWrite( nAttributeId, &nAttrStride );
	pAttr += nAttrStride * nStartBlock;
	fltx4 val0 = ReplicateX4( fMin );
	fltx4 val_d = ReplicateX4( fMax - fMin );
	int nRandContext = GetSIMDRandContext();
	if ( !bRandomlyInvert )
	{
		if ( fExp != 1.0f )
		{
			int nExp = (int)(4.0f * fExp);
			while( nBlockCount-- )
			{
				*( pAttr ) = AddSIMD( *pAttr, AddSIMD( val0, MulSIMD( Pow_FixedPoint_Exponent_SIMD( RandSIMD( nRandContext ), nExp ), val_d ) ) );
				pAttr += nAttrStride;
			}
		}
		else
		{
			while( nBlockCount-- )
			{
				*pAttr = AddSIMD( *pAttr, AddSIMD( val0, MulSIMD( RandSIMD( nRandContext ), val_d ) ) );
				pAttr += nAttrStride;
			}
		}
	}
	else
	{
		fltx4 fl4NegOne = ReplicateX4( -1.0f );
		if ( fExp != 1.0f )
		{
			int nExp = (int)(4.0f * fExp);
			while( nBlockCount-- )
			{
				fltx4 fl4RandVal = AddSIMD( val0, MulSIMD( Pow_FixedPoint_Exponent_SIMD( RandSIMD( nRandContext ), nExp ), val_d ) );
				fltx4 fl4Sign = MaskedAssign( CmpGeSIMD( RandSIMD( nRandContext ), Four_PointFives ), Four_Ones, fl4NegOne ); 
				*pAttr = AddSIMD( *pAttr, MulSIMD( fl4RandVal, fl4Sign ) );
				pAttr += nAttrStride;
			}
		}
		else
		{
			while( nBlockCount-- )
			{
				fltx4 fl4RandVal = AddSIMD( val0, MulSIMD( RandSIMD( nRandContext ), val_d ) );
				fltx4 fl4Sign = MaskedAssign( CmpGeSIMD( RandSIMD( nRandContext ), Four_PointFives ), Four_Ones, fl4NegOne ); 
				*pAttr = AddSIMD( *pAttr, MulSIMD( fl4RandVal, fl4Sign ) );
				pAttr += nAttrStride;
			}
		}
	}
	ReleaseSIMDRandContext( nRandContext );
}


class C_INIT_CreateOnModel : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_CreateOnModel );

	int m_nControlPointNumber;
	int m_nForceInModel;
	float m_flHitBoxScale;
	Vector m_vecDirectionBias;


	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | 
			PARTICLE_ATTRIBUTE_HITBOX_RELATIVE_XYZ_MASK | PARTICLE_ATTRIBUTE_HITBOX_INDEX_MASK;

	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_CreateOnModel, "Position on Model Random", OPERATOR_PI_POSITION );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateOnModel ) 
	DMXELEMENT_UNPACK_FIELD( "control_point_number", "0", int, m_nControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "force to be inside model", "0", int, m_nForceInModel )
	DMXELEMENT_UNPACK_FIELD( "hitbox scale", "1.0", int, m_flHitBoxScale )
	DMXELEMENT_UNPACK_FIELD( "direction bias", "0 0 0", Vector, m_vecDirectionBias )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateOnModel )

void C_INIT_CreateOnModel::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	pParticles->UpdateHitBoxInfo( m_nControlPointNumber );
	while( nParticleCount )
	{
		Vector vecPnts[100];								// minimize stack usage
		Vector vecUVW[100];
		int nHitBoxIndex[100];
		int nToDo = min( (int)ARRAYSIZE( vecPnts ), nParticleCount );

		Assert( m_nControlPointNumber <= pParticles->GetHighestControlPoint() );

		g_pParticleSystemMgr->Query()->GetRandomPointsOnControllingObjectHitBox( 
			pParticles, m_nControlPointNumber,
			nToDo, m_flHitBoxScale, m_nForceInModel, vecPnts, m_vecDirectionBias, vecUVW, 
			nHitBoxIndex );
		
		for( int i=0; i<nToDo; i++)
		{
			float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
			float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
			float *pHitboxRelXYZ = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_HITBOX_RELATIVE_XYZ, start_p );
			int *pHitboxIndex = pParticles->GetIntAttributePtrForWrite( PARTICLE_ATTRIBUTE_HITBOX_INDEX, start_p );
			start_p++;

			Vector randpos = vecPnts[i];
			xyz[0] = randpos.x;
			xyz[4] = randpos.y;
			xyz[8] = randpos.z;
			if ( pxyz && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_PREV_XYZ_MASK ) )
			{
				pxyz[0] = randpos.x;
				pxyz[4] = randpos.y;
				pxyz[8] = randpos.z;
			}
			if ( pHitboxRelXYZ && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_HITBOX_RELATIVE_XYZ_MASK ) )
			{
				pHitboxRelXYZ[0] = vecUVW[i].x;
				pHitboxRelXYZ[4] = vecUVW[i].y;
				pHitboxRelXYZ[8] = vecUVW[i].z;
			}
			if ( pHitboxIndex && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_HITBOX_INDEX_MASK ) )
			{
				*pHitboxIndex = nHitBoxIndex[i];
			}

		}
		nParticleCount -= nToDo;
	}
}
		

static inline void RandomPointOnUnitSphere( int nRandContext, FourVectors &out )
{
	// generate 4 random points on the unit sphere. uses Marsaglia (1972) method from
	// http://mathworld.wolfram.com/SpherePointPicking.html

	fltx4 f4x1 = SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ); // -1..1
	fltx4 f4x2 = SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ); // -1..1
	fltx4 f4x1SQ = MulSIMD( f4x1, f4x1 );
	fltx4 f4x2SQ = MulSIMD( f4x2, f4x2 );
	fltx4 badMask = CmpGeSIMD( AddSIMD( f4x1SQ, f4x2SQ ), Four_Ones );
	while( IsAnyNegative( badMask ) )
	{
		f4x1 = MaskedAssign( badMask, SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ), f4x1 );
		f4x2 = MaskedAssign( badMask, SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ), f4x2 );
		f4x1SQ = MulSIMD( f4x1, f4x1 );
		f4x2SQ = MulSIMD( f4x2, f4x2 );
		badMask = CmpGeSIMD( AddSIMD( f4x1SQ, f4x2SQ ), Four_Ones );
	}
	// now, we have 2 points on the unit circle
	fltx4 f4OuterArea = SqrtEstSIMD( SubSIMD( Four_Ones, SubSIMD( f4x1SQ, f4x2SQ ) ) );
	out.x = MulSIMD( AddSIMD( f4x1, f4x1 ), f4OuterArea );
	out.y = MulSIMD( AddSIMD( f4x2, f4x2 ), f4OuterArea );
	out.z = SubSIMD( Four_Ones, MulSIMD( Four_Twos, AddSIMD( f4x1, f4x2 ) ) );
}

static inline void RandomPointInUnitSphere( int nRandContext, FourVectors &out )
{
	// generate 4 random points inside the unit sphere. uses rejection method.
	out.x = SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ); // -1..1
	out.y = SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ); // -1..1
	out.z = SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ); // -1..1
	fltx4 f4xSQ = MulSIMD( out.x, out.x );
	fltx4 f4ySQ = MulSIMD( out.y, out.y );
	fltx4 f4zSQ = MulSIMD( out.z, out.z );
	fltx4 badMask = CmpGtSIMD( AddSIMD( AddSIMD( f4xSQ, f4ySQ ), f4zSQ ), Four_Ones );
	while( IsAnyNegative( badMask ) )
	{
		out.x = MaskedAssign( badMask, SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ), out.x );
		out.y = MaskedAssign( badMask, SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ), out.y );
		out.z = MaskedAssign( badMask, SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ), out.z );
		f4xSQ = MulSIMD( out.x, out.x );
		f4ySQ = MulSIMD( out.y, out.y );
		f4zSQ = MulSIMD( out.z, out.z );
		badMask = CmpGeSIMD( AddSIMD( AddSIMD( f4xSQ, f4ySQ ), f4zSQ ), Four_Ones );
	}
}



class C_INIT_CreateWithinSphere : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_CreateWithinSphere );

	float m_fRadiusMin;
	float m_fRadiusMax;
	Vector m_vecDistanceBias, m_vecDistanceBiasAbs;
	int m_nControlPointNumber;
	float m_fSpeedMin;
	float m_fSpeedMax;
	float m_fSpeedRandExp;
	bool m_bLocalCoords;
	bool m_bDistanceBiasAbs;
	bool m_bUseHighestEndCP;
	bool m_bDistanceBias;
	float m_flEndCPGrowthTime;
	
	Vector m_LocalCoordinateSystemSpeedMin;
	Vector m_LocalCoordinateSystemSpeedMax;
	int m_nCreateInModel;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		if ( !m_bUseHighestEndCP )
			return 1ULL << m_nControlPointNumber;
		return ~( ( 1ULL << m_nControlPointNumber ) - 1 );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
										int start_block, int n_blocks, int nAttributeWriteMask,
										void *pContext ) const;

	void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		m_nControlPointNumber = max( 0, min( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
		m_bDistanceBias = ( m_vecDistanceBias.x != 1.0f ) || ( m_vecDistanceBias.y != 1.0f ) || ( m_vecDistanceBias.z != 1.0f );
		m_bDistanceBiasAbs = ( m_vecDistanceBiasAbs.x != 0.0f ) || ( m_vecDistanceBiasAbs.y != 0.0f ) || ( m_vecDistanceBiasAbs.z != 0.0f );
	}

	void Render( CParticleCollection *pParticles ) const;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_CreateWithinSphere, "Position Within Sphere Random", OPERATOR_PI_POSITION );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateWithinSphere ) 
	DMXELEMENT_UNPACK_FIELD( "distance_min", "0", float, m_fRadiusMin )
	DMXELEMENT_UNPACK_FIELD( "distance_max", "0", float, m_fRadiusMax )
	DMXELEMENT_UNPACK_FIELD( "distance_bias", "1 1 1", Vector, m_vecDistanceBias )
	DMXELEMENT_UNPACK_FIELD( "distance_bias_absolute_value", "0 0 0", Vector, m_vecDistanceBiasAbs )
	DMXELEMENT_UNPACK_FIELD( "bias in local system", "0", bool, m_bLocalCoords )
	DMXELEMENT_UNPACK_FIELD( "control_point_number", "0", int, m_nControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "speed_min", "0", float, m_fSpeedMin )
	DMXELEMENT_UNPACK_FIELD( "speed_max", "0", float, m_fSpeedMax )
	DMXELEMENT_UNPACK_FIELD( "speed_random_exponent", "1", float, m_fSpeedRandExp )
	DMXELEMENT_UNPACK_FIELD( "speed_in_local_coordinate_system_min", "0 0 0", Vector, m_LocalCoordinateSystemSpeedMin )
	DMXELEMENT_UNPACK_FIELD( "speed_in_local_coordinate_system_max", "0 0 0", Vector, m_LocalCoordinateSystemSpeedMax )
	DMXELEMENT_UNPACK_FIELD( "create in model", "0", int, m_nCreateInModel )
	DMXELEMENT_UNPACK_FIELD( "randomly distribute to highest supplied Control Point", "0", bool, m_bUseHighestEndCP )
	DMXELEMENT_UNPACK_FIELD( "randomly distribution growth time", "0", float, m_flEndCPGrowthTime )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateWithinSphere )


ConVar r_sse_s( "r_sse_s", "1", 0, "sse ins for particle sphere create" );

void C_INIT_CreateWithinSphere::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	for( ; nParticleCount--; start_p++ )
	{
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
		int nCurrentControlPoint = m_nControlPointNumber;
		if ( m_bUseHighestEndCP )
		{
			//hack for growth time instead of using strength as currenly initializers don't support it.
			float flStrength = 1.0;
			if ( m_flEndCPGrowthTime != 0.0f )
			{
				flStrength = min ( pParticles->m_flCurTime, m_flEndCPGrowthTime ) / m_flEndCPGrowthTime ;
			}
			int nHighestControlPoint = floor ( pParticles->GetHighestControlPoint() * flStrength );
			nCurrentControlPoint = pParticles->RandomInt( m_nControlPointNumber, nHighestControlPoint );
		}
		Vector randpos, randDir;
		for( int nTryCtr = 0 ; nTryCtr < 10; nTryCtr++ )
		{
			float flLength = pParticles->RandomVectorInUnitSphere( &randpos );
			
			// Absolute value and biasing for creating hemispheres and ovoids.
			if ( m_bDistanceBiasAbs	)
			{
				if ( m_vecDistanceBiasAbs.x	!= 0.0f )
				{
					randpos.x = fabs(randpos.x);
				}
				if ( m_vecDistanceBiasAbs.y	!= 0.0f )
				{
					randpos.y = fabs(randpos.y);
				}
				if ( m_vecDistanceBiasAbs.z	!= 0.0f )
				{
					randpos.z = fabs(randpos.z);
				}
			}
			randpos *= m_vecDistanceBias;
			randpos.NormalizeInPlace();
			
			
			randDir = randpos;
			randpos *= Lerp( flLength, m_fRadiusMin, m_fRadiusMax );
			
			if ( !m_bDistanceBias || !m_bLocalCoords )
			{
				Vector vecControlPoint;
				pParticles->GetControlPointAtTime( nCurrentControlPoint, *ct, &vecControlPoint );
				randpos += vecControlPoint;
			}
			else
			{
				matrix3x4_t mat;
				pParticles->GetControlPointTransformAtTime( nCurrentControlPoint, *ct, &mat );
				Vector vecTransformLocal = vec3_origin;
				VectorTransform( randpos, mat, vecTransformLocal );
				randpos = vecTransformLocal;
			}
			
			// now, force to be in model if we can
			if (
				( m_nCreateInModel == 0 ) || 
				(g_pParticleSystemMgr->Query()->MovePointInsideControllingObject( 
					pParticles, pParticles->m_ControlPoints[nCurrentControlPoint].m_pObject, &randpos ) ) )
				break;
		}
		
		xyz[0] = randpos.x;
		xyz[4] = randpos.y;
		xyz[8] = randpos.z;

		// FIXME: Remove this into a speed setting initializer
		if ( pxyz && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_PREV_XYZ_MASK ) )
		{
			Vector poffset(0,0,0);
			if ( m_fSpeedMax > 0.0 )
			{
				float rand_speed = pParticles->RandomFloatExp( m_fSpeedMin, m_fSpeedMax, m_fSpeedRandExp );
				poffset.x -= rand_speed * randDir.x;
				poffset.y -= rand_speed * randDir.y;
				poffset.z -= rand_speed * randDir.z;
			}
			poffset -=
				pParticles->RandomFloat( m_LocalCoordinateSystemSpeedMin.x, m_LocalCoordinateSystemSpeedMax.x )*
				pParticles->m_ControlPoints[ nCurrentControlPoint ].m_ForwardVector;
			poffset -=
				pParticles->RandomFloat( m_LocalCoordinateSystemSpeedMin.y, m_LocalCoordinateSystemSpeedMax.y )*
				pParticles->m_ControlPoints[ nCurrentControlPoint ].m_RightVector;
			poffset -=
				pParticles->RandomFloat( m_LocalCoordinateSystemSpeedMin.z, m_LocalCoordinateSystemSpeedMax.z )*
				pParticles->m_ControlPoints[ nCurrentControlPoint ].m_UpVector;

			poffset *= pParticles->m_flPreviousDt;
			randpos += poffset;
			pxyz[0] = randpos.x;
			pxyz[4] = randpos.y;
			pxyz[8] = randpos.z;
		}
	}
}

void C_INIT_CreateWithinSphere::InitNewParticlesBlock( CParticleCollection *pParticles, 
													   int start_block, int n_blocks, int nAttributeWriteMask,
													   void *pContext ) const
{
	// sse-favorable settings
	bool bMustUseScalar = m_bUseHighestEndCP || m_nCreateInModel;
	if ( m_bDistanceBias && m_bLocalCoords )
		bMustUseScalar = true;

	if ( ( !bMustUseScalar ) && 
		 // (( nAttributeWriteMask & PARTICLE_ATTRIBUTE_PREV_XYZ_MASK ) == 0 ) &&
		 r_sse_s.GetInt() )
	{
		C4VAttributeWriteIterator pXYZ( PARTICLE_ATTRIBUTE_XYZ, pParticles );
		pXYZ += start_block;
		C4VAttributeWriteIterator pPrevXYZ( PARTICLE_ATTRIBUTE_PREV_XYZ, pParticles );
		pPrevXYZ += start_block;
		CM128AttributeIterator pCT( PARTICLE_ATTRIBUTE_CREATION_TIME, pParticles );
		pCT += start_block;
		
		// now, calculate the terms we need for interpolating control points
		FourVectors v4PrevControlPointPosition;
		v4PrevControlPointPosition.DuplicateVector( pParticles->m_ControlPoints[m_nControlPointNumber].m_PrevPosition );
		FourVectors v4ControlPointDelta;
		v4ControlPointDelta.DuplicateVector( pParticles->m_ControlPoints[m_nControlPointNumber].m_Position );
		v4ControlPointDelta -= v4PrevControlPointPosition;

		float flOODT = ( pParticles->m_flDt > 0.0 ) ? ( 1.0 / pParticles->m_flDt ) : 0.0;
		fltx4 fl4OODt = ReplicateX4( flOODT );
		fltx4 fl4PrevTime = ReplicateX4( pParticles->m_flCurTime - pParticles->m_flDt );
		int nContext = GetSIMDRandContext();

		FourVectors v4DistanceBias;
		v4DistanceBias.DuplicateVector( m_vecDistanceBias );
		FourVectors v4ConditionalAbsMask;
		for( int nComp = 0 ; nComp < 3; nComp++ )
		{
			v4ConditionalAbsMask[nComp] = ( m_vecDistanceBiasAbs[nComp] > 0 ) ?
				LoadAlignedSIMD( ( const float *) g_SIMD_clear_signmask ) :
				LoadAlignedSIMD( ( const float *) g_SIMD_AllOnesMask );
		}
		fltx4 fl4RadiusMin = ReplicateX4( m_fRadiusMin );
		fltx4 fl4RadiusSpread = ReplicateX4( m_fRadiusMax - m_fRadiusMin );
		int nPowSSEMask = 4.0 * m_fSpeedRandExp;

		bool bDoRandSpeed =
			( m_fSpeedMax > 0. ) || 
			( m_LocalCoordinateSystemSpeedMax.x != 0 ) ||
			( m_LocalCoordinateSystemSpeedMax.y != 0 ) ||
			( m_LocalCoordinateSystemSpeedMax.z != 0 ) ||
			( m_LocalCoordinateSystemSpeedMin.x != 0 ) ||
			( m_LocalCoordinateSystemSpeedMin.y != 0 ) ||
			( m_LocalCoordinateSystemSpeedMin.z != 0 );


		fltx4 fl4SpeedMin = ReplicateX4( m_fSpeedMin );
		fltx4 fl4SpeedRange = ReplicateX4( m_fSpeedMax - m_fSpeedMin );

		fltx4 fl4LocalSpeedMinX = ReplicateX4( m_LocalCoordinateSystemSpeedMin.x );
		fltx4 fl4LocalSpeedXSpread = ReplicateX4( m_LocalCoordinateSystemSpeedMax.x - 
												  m_LocalCoordinateSystemSpeedMin.x );
		fltx4 fl4LocalSpeedMinY = ReplicateX4( m_LocalCoordinateSystemSpeedMin.y );
		fltx4 fl4LocalSpeedYSpread = ReplicateX4( m_LocalCoordinateSystemSpeedMax.y - 
												  m_LocalCoordinateSystemSpeedMin.y );
		fltx4 fl4LocalSpeedMinZ = ReplicateX4( m_LocalCoordinateSystemSpeedMin.z );
		fltx4 fl4LocalSpeedZSpread = ReplicateX4( m_LocalCoordinateSystemSpeedMax.z - 
												  m_LocalCoordinateSystemSpeedMin.z );

		FourVectors v4CPForward;
		v4CPForward.DuplicateVector( pParticles->m_ControlPoints[m_nControlPointNumber].m_ForwardVector );
		FourVectors v4CPUp;
		v4CPUp.DuplicateVector( pParticles->m_ControlPoints[m_nControlPointNumber].m_UpVector );
		FourVectors v4CPRight;
		v4CPRight.DuplicateVector( pParticles->m_ControlPoints[m_nControlPointNumber].m_RightVector );

		fltx4 fl4PreviousDt = ReplicateX4( pParticles->m_flPreviousDt );

		while( n_blocks-- )
		{
			FourVectors v4RandPos;
			RandomPointInUnitSphere( nContext, v4RandPos );

			fltx4 fl4Length = v4RandPos.length();

			// conditional absolute value
			v4RandPos.x = AndSIMD( v4RandPos.x, v4ConditionalAbsMask.x );
			v4RandPos.y = AndSIMD( v4RandPos.y, v4ConditionalAbsMask.y );
			v4RandPos.z = AndSIMD( v4RandPos.z, v4ConditionalAbsMask.z );

			v4RandPos *= v4DistanceBias;
			v4RandPos.VectorNormalizeFast();
			
			FourVectors v4randDir = v4RandPos;
			
			// lerp radius
			v4RandPos *= AddSIMD( fl4RadiusMin, MulSIMD( fl4Length, fl4RadiusSpread ) );
			v4RandPos += v4PrevControlPointPosition;

			FourVectors cpnt = v4ControlPointDelta;
			cpnt *= MulSIMD( SubSIMD( *pCT, fl4PrevTime ), fl4OODt );
			v4RandPos += cpnt;

			*(pXYZ) = v4RandPos;

			if ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_PREV_XYZ_MASK )
			{
				if ( bDoRandSpeed )
				{
					fltx4 fl4Rand_speed = Pow_FixedPoint_Exponent_SIMD( RandSIMD( nContext ), nPowSSEMask );
					fl4Rand_speed = AddSIMD( fl4SpeedMin, MulSIMD( fl4SpeedRange, fl4Rand_speed ) );
					v4randDir *= fl4Rand_speed;

					// local speed
					FourVectors v4LocalOffset = v4CPForward;
					v4LocalOffset *= AddSIMD( fl4LocalSpeedMinX, 
											  MulSIMD( fl4LocalSpeedXSpread, RandSIMD( nContext ) ) );
					v4randDir += v4LocalOffset;

					v4LocalOffset = v4CPRight;
					v4LocalOffset *= AddSIMD( fl4LocalSpeedMinY, 
											  MulSIMD( fl4LocalSpeedYSpread, RandSIMD( nContext ) ) );
					v4randDir += v4LocalOffset;


					v4LocalOffset = v4CPUp;
					v4LocalOffset *= AddSIMD( fl4LocalSpeedMinZ, 
											  MulSIMD( fl4LocalSpeedZSpread, RandSIMD( nContext ) ) );
					v4randDir += v4LocalOffset;
					v4randDir *= fl4PreviousDt;
					v4RandPos -= v4randDir;
				}
				*(pPrevXYZ) = v4RandPos;

			}



			++pXYZ;
			++pPrevXYZ;
			++pCT;
		}
		ReleaseSIMDRandContext( nContext );

	}
	else
		CParticleOperatorInstance::InitNewParticlesBlock( pParticles, start_block, n_blocks, nAttributeWriteMask, pContext );

}


//-----------------------------------------------------------------------------
// Render visualization
//-----------------------------------------------------------------------------
void C_INIT_CreateWithinSphere::Render( CParticleCollection *pParticles ) const
{					   
	Vector vecOrigin;
	pParticles->GetControlPointAtTime( m_nControlPointNumber, pParticles->m_flCurTime, &vecOrigin );
	RenderWireframeSphere( vecOrigin, m_fRadiusMin, 16, 8, Color( 192, 192, 0, 255 ), false );
	RenderWireframeSphere( vecOrigin, m_fRadiusMax, 16, 8, Color( 128, 128, 0, 255 ), false );
}




class C_INIT_CreateWithinBox : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_CreateWithinBox );

	Vector m_vecMin;
	Vector m_vecMax;
	int m_nControlPointNumber;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;

	void Render( CParticleCollection *pParticles ) const;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_CreateWithinBox, "Position Within Box Random", OPERATOR_PI_POSITION );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateWithinBox ) 
	DMXELEMENT_UNPACK_FIELD( "min", "0 0 0", Vector, m_vecMin )
	DMXELEMENT_UNPACK_FIELD( "max", "0 0 0", Vector, m_vecMax )
	DMXELEMENT_UNPACK_FIELD( "control point number", "0", int, m_nControlPointNumber )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateWithinBox )


void C_INIT_CreateWithinBox::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	int nControlPointNumber = max( 0, min( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
	for( ; nParticleCount--; start_p++ )
	{
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );

		Vector randpos;
		pParticles->RandomVector( m_vecMin, m_vecMax, &randpos );

		Vector vecControlPoint;
		pParticles->GetControlPointAtTime( nControlPointNumber, *ct, &vecControlPoint );
		randpos += vecControlPoint;

		xyz[0] = randpos.x;
		xyz[4] = randpos.y;
		xyz[8] = randpos.z;
		if ( pxyz && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_PREV_XYZ_MASK ) )
		{
			pxyz[0] = randpos.x;
			pxyz[4] = randpos.y;
			pxyz[8] = randpos.z;
		}
	}
}

//-----------------------------------------------------------------------------
// Render visualization
//-----------------------------------------------------------------------------
void C_INIT_CreateWithinBox::Render( CParticleCollection *pParticles ) const
{					   
	Vector vecOrigin;
	pParticles->GetControlPointAtTime( m_nControlPointNumber, pParticles->m_flCurTime, &vecOrigin );
	RenderWireframeBox( vecOrigin, vec3_angle, m_vecMin, m_vecMax, Color( 192, 192, 0, 255 ), false );
}



//-----------------------------------------------------------------------------
// Position Offset Initializer
// offsets initial position of particles within a random vector range,
// while still respecting spherical/conical spacial and velocity initialization
//-----------------------------------------------------------------------------
class C_INIT_PositionOffset : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_PositionOffset );

	Vector m_OffsetMin;
	Vector m_OffsetMax;
	int m_nControlPointNumber;
	bool m_bLocalCoords;
	bool m_bProportional;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_CREATION_TIME_MASK | PARTICLE_ATTRIBUTE_RADIUS_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;

	void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		m_nControlPointNumber = max( 0, min( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
	}

	bool InitMultipleOverride ( void ) { return true; }

	void Render( CParticleCollection *pParticles ) const;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_PositionOffset, "Position Modify Offset Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_PositionOffset ) 
	DMXELEMENT_UNPACK_FIELD( "control_point_number", "0", int, m_nControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "offset min", "0 0 0", Vector, m_OffsetMin )
	DMXELEMENT_UNPACK_FIELD( "offset max", "0 0 0", Vector, m_OffsetMax )
	DMXELEMENT_UNPACK_FIELD( "offset in local space 0/1", "0", bool, m_bLocalCoords )
	DMXELEMENT_UNPACK_FIELD( "offset proportional to radius 0/1", "0", bool, m_bProportional )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_PositionOffset )


void C_INIT_PositionOffset::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	for( ; nParticleCount--; start_p++ )
	{
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		const float *radius = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_RADIUS, start_p );
		
		Vector randpos;
		
		if ( m_bProportional )
		{
			pParticles->RandomVector( (m_OffsetMin * *radius), (m_OffsetMax * *radius), &randpos );
		}
		else
		{
			pParticles->RandomVector( m_OffsetMin, m_OffsetMax, &randpos );
		}

		if ( m_bLocalCoords )
		{
			matrix3x4_t mat;
			pParticles->GetControlPointTransformAtTime( m_nControlPointNumber, *ct, &mat );
			Vector vecTransformLocal = vec3_origin;
			VectorRotate( randpos, mat, vecTransformLocal );
			randpos = vecTransformLocal;
		}

		xyz[0] += randpos.x;
		xyz[4] += randpos.y;
		xyz[8] += randpos.z;
		pxyz[0] += randpos.x;
		pxyz[4] += randpos.y;
		pxyz[8] += randpos.z;
	}
}


//-----------------------------------------------------------------------------
// Render visualization
//-----------------------------------------------------------------------------
void C_INIT_PositionOffset::Render( CParticleCollection *pParticles ) const
{					   
	Vector vecOrigin (0,0,0);
	Vector vecMinExtent = m_OffsetMin;
	Vector vecMaxExtent = m_OffsetMax;
	if ( m_bLocalCoords )
	{
		matrix3x4_t mat;
		pParticles->GetControlPointTransformAtTime( m_nControlPointNumber, pParticles->m_flCurTime, &mat );
		VectorRotate( m_OffsetMin, mat, vecMinExtent );
		VectorRotate( m_OffsetMax, mat, vecMaxExtent ); 
	}
	else
	{
		pParticles->GetControlPointAtTime( m_nControlPointNumber, pParticles->m_flCurTime, &vecOrigin );
	}
	RenderWireframeBox( vecOrigin, vec3_angle, vecMinExtent , vecMaxExtent , Color( 192, 192, 0, 255 ), false );
}


//-----------------------------------------------------------------------------
//
// Velocity-based Operators
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Random velocity initializer
//-----------------------------------------------------------------------------
class C_INIT_VelocityRandom : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_VelocityRandom );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		if ( m_bHasLocalSpeed )
			return 1ULL << m_nControlPointNumber;
		return 0;
	}

	virtual bool InitMultipleOverride() { return true; }

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;

	void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		m_nControlPointNumber = max( 0, min( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
		m_bHasLocalSpeed = ( m_LocalCoordinateSystemSpeedMin != vec3_origin ) || ( m_LocalCoordinateSystemSpeedMax != vec3_origin );  
		if ( m_fSpeedMax < m_fSpeedMin )
		{
			V_swap( m_fSpeedMin, m_fSpeedMax );
		}
	}

private:
	int m_nControlPointNumber;
	float m_fSpeedMin;
	float m_fSpeedMax;
	Vector m_LocalCoordinateSystemSpeedMin;
	Vector m_LocalCoordinateSystemSpeedMax;
	bool m_bHasLocalSpeed;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_VelocityRandom, "Velocity Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_VelocityRandom ) 
	DMXELEMENT_UNPACK_FIELD( "control_point_number", "0", int, m_nControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "random_speed_min", "0", float, m_fSpeedMin )
	DMXELEMENT_UNPACK_FIELD( "random_speed_max", "0", float, m_fSpeedMax )
	DMXELEMENT_UNPACK_FIELD( "speed_in_local_coordinate_system_min", "0 0 0", Vector, m_LocalCoordinateSystemSpeedMin )
	DMXELEMENT_UNPACK_FIELD( "speed_in_local_coordinate_system_max", "0 0 0", Vector, m_LocalCoordinateSystemSpeedMax )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_VelocityRandom )


void C_INIT_VelocityRandom::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	for( ; nParticleCount--; start_p++ )
	{
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
			
		Vector vecVelocity( 0.0f, 0.0f, 0.0f );
		if ( m_bHasLocalSpeed )
		{
			Vector vecRandomSpeed, vecForward, vecUp, vecRight;
			pParticles->RandomVector( m_LocalCoordinateSystemSpeedMin, m_LocalCoordinateSystemSpeedMax, &vecRandomSpeed );
			pParticles->GetControlPointOrientationAtTime( m_nControlPointNumber, *ct, &vecForward, &vecRight, &vecUp );
			VectorMA( vecVelocity, vecRandomSpeed.x, vecForward, vecVelocity );
			VectorMA( vecVelocity, -vecRandomSpeed.y, vecRight, vecVelocity );
			VectorMA( vecVelocity, vecRandomSpeed.z, vecUp, vecVelocity );
		}

		if ( m_fSpeedMax > 0.0f )
		{
			Vector vecRandomSpeed;
			pParticles->RandomVector( m_fSpeedMin, m_fSpeedMax, &vecRandomSpeed );
			vecVelocity += vecRandomSpeed;
		}

		vecVelocity *= pParticles->m_flPreviousDt;
		pxyz[0] -= vecVelocity.x;
		pxyz[4] -= vecVelocity.y;
		pxyz[8] -= vecVelocity.z;
	}
}


//-----------------------------------------------------------------------------
// Initial Velocity Noise Operator
//-----------------------------------------------------------------------------
class C_INIT_InitialVelocityNoise : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_InitialVelocityNoise );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_XYZ_MASK;
	}
	
	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;	

	void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const;

	void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		m_nControlPointNumber = max( 0, min( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
	}

	virtual bool InitMultipleOverride() { return true; }

	Vector	m_vecAbsVal, m_vecAbsValInv, m_vecOffsetLoc;
	float	m_flOffset;
	Vector	m_vecOutputMin;
	Vector	m_vecOutputMax;
	float	m_flNoiseScale, m_flNoiseScaleLoc;
	int		nRemainingBlocks, m_nControlPointNumber;
	bool	m_bLocalSpace;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_InitialVelocityNoise, "Velocity Noise", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_InitialVelocityNoise )
	DMXELEMENT_UNPACK_FIELD( "Control Point Number","0",int,m_nControlPointNumber)
	DMXELEMENT_UNPACK_FIELD( "Time Noise Coordinate Scale","1",float,m_flNoiseScale)
	DMXELEMENT_UNPACK_FIELD( "Spatial Noise Coordinate Scale","0.01",float,m_flNoiseScaleLoc)
	DMXELEMENT_UNPACK_FIELD( "Time Coordinate Offset","0", float, m_flOffset )
	DMXELEMENT_UNPACK_FIELD( "Spatial Coordinate Offset","0 0 0", Vector, m_vecOffsetLoc )
	DMXELEMENT_UNPACK_FIELD( "Absolute Value","0 0 0", Vector, m_vecAbsVal )
	DMXELEMENT_UNPACK_FIELD( "Invert Abs Value","0 0 0", Vector, m_vecAbsValInv )
	DMXELEMENT_UNPACK_FIELD( "output minimum","0 0 0", Vector, m_vecOutputMin )
	DMXELEMENT_UNPACK_FIELD( "output maximum","1 1 1", Vector, m_vecOutputMax )
	DMXELEMENT_UNPACK_FIELD( "Apply Velocity in Local Space (0/1)","0", bool, m_bLocalSpace )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_InitialVelocityNoise );


void C_INIT_InitialVelocityNoise::InitNewParticlesBlock( CParticleCollection *pParticles, 
								   int start_block, int n_blocks, int nAttributeWriteMask,
								   void *pContext ) const
{
	float		flAbsScaleX, flAbsScaleY, flAbsScaleZ;
	fltx4 		fl4AbsValX, fl4AbsValY, fl4AbsValZ;
	fl4AbsValX = CmpEqSIMD( Four_Zeros, Four_Zeros ); 
	fl4AbsValY = fl4AbsValX;
	fl4AbsValZ = fl4AbsValX;
	flAbsScaleX = 0.5;
	flAbsScaleY = 0.5; 
	flAbsScaleZ = 0.5;

	// Set up single if check for absolute value inversion inside the loop
	bool m_bNoiseAbs = ( m_vecAbsValInv.x != 0.0f ) || ( m_vecAbsValInv.y != 0.0f ) || ( m_vecAbsValInv.z != 0.0f );
	// Set up values for more optimal absolute value calculations inside the loop
	if ( m_vecAbsVal.x	!= 0.0f )
	{
		fl4AbsValX = LoadAlignedSIMD( (float *) g_SIMD_clear_signmask );
		flAbsScaleX = 1.0;
	}
	if ( m_vecAbsVal.y	!= 0.0f )
	{
		fl4AbsValY = LoadAlignedSIMD( (float *) g_SIMD_clear_signmask );
		flAbsScaleY = 1.0;
	}
	if ( m_vecAbsVal.z	!= 0.0f )
	{
		fl4AbsValZ = LoadAlignedSIMD( (float *) g_SIMD_clear_signmask );
		flAbsScaleZ = 1.0;
	}

	float ValueScaleX, ValueScaleY, ValueScaleZ, ValueBaseX, ValueBaseY, ValueBaseZ;

	ValueScaleX = ( flAbsScaleX *(m_vecOutputMax.x-m_vecOutputMin.x ) );
	ValueBaseX = (m_vecOutputMin.x+ ( ( 1.0 - flAbsScaleX ) *( m_vecOutputMax.x-m_vecOutputMin.x ) ) );

	ValueScaleY = ( flAbsScaleY *(m_vecOutputMax.y-m_vecOutputMin.y ) );
	ValueBaseY = (m_vecOutputMin.y+ ( ( 1.0 - flAbsScaleY ) *( m_vecOutputMax.y-m_vecOutputMin.y ) ) );

	ValueScaleZ = ( flAbsScaleZ *(m_vecOutputMax.z-m_vecOutputMin.z ) );
	ValueBaseZ = (m_vecOutputMin.z+ ( ( 1.0 - flAbsScaleZ ) *( m_vecOutputMax.z-m_vecOutputMin.z ) ) );

	fltx4 fl4ValueBaseX = ReplicateX4( ValueBaseX );
	fltx4 fl4ValueBaseY = ReplicateX4( ValueBaseY );
	fltx4 fl4ValueBaseZ = ReplicateX4( ValueBaseZ );

	fltx4 fl4ValueScaleX = ReplicateX4( ValueScaleX );
	fltx4 fl4ValueScaleY = ReplicateX4( ValueScaleY );
	fltx4 fl4ValueScaleZ = ReplicateX4( ValueScaleZ );

	float CoordScale = m_flNoiseScale;
	float CoordScaleLoc = m_flNoiseScaleLoc;

	Vector ofs_y = Vector( 100000.5, 300000.25, 9000000.75 );
	Vector ofs_z = Vector( 110000.25, 310000.75, 9100000.5 );

	size_t attr_stride;

	const FourVectors *xyz = pParticles->Get4VAttributePtr( PARTICLE_ATTRIBUTE_XYZ, &attr_stride );
	xyz += attr_stride * start_block;
	FourVectors *pxyz = pParticles->Get4VAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, &attr_stride );
	pxyz += attr_stride * start_block;
	const fltx4 *pCreationTime = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, &attr_stride );
	pCreationTime += attr_stride * start_block;

	// setup
	fltx4 fl4Offset = ReplicateX4( m_flOffset );
	FourVectors fvOffsetLoc;
	fvOffsetLoc.DuplicateVector( m_vecOffsetLoc );
	CParticleSIMDTransformation CPTransform;
	float flCreationTime = SubFloat( *pCreationTime, 0 );
	pParticles->GetControlPointTransformAtTime( m_nControlPointNumber, flCreationTime, &CPTransform );

	while( n_blocks-- )
	{	
		FourVectors fvCoordLoc = *xyz;
		fvCoordLoc += fvOffsetLoc;

		FourVectors fvCoord;
		fvCoord.x = AddSIMD(*pCreationTime, fl4Offset);
		fvCoord.y = AddSIMD(*pCreationTime, fl4Offset);
		fvCoord.z = AddSIMD(*pCreationTime, fl4Offset);
		fvCoordLoc *= CoordScaleLoc;
		fvCoord *= CoordScale;
		fvCoord += fvCoordLoc;

		FourVectors fvCoord2 = fvCoord;
		FourVectors fvOffsetTemp;
		fvOffsetTemp.DuplicateVector( ofs_y );
		fvCoord2 +=  fvOffsetTemp;
		FourVectors fvCoord3 = fvCoord;
		fvOffsetTemp.DuplicateVector( ofs_z );
		fvCoord3 += fvOffsetTemp;

		fltx4 fl4NoiseX;
		fltx4 fl4NoiseY;
		fltx4 fl4NoiseZ;

		fl4NoiseX = NoiseSIMD( fvCoord );

		fl4NoiseY = NoiseSIMD( fvCoord2 );

		fl4NoiseZ = NoiseSIMD( fvCoord3 );

		fl4NoiseX = AndSIMD ( fl4NoiseX, fl4AbsValX );
		fl4NoiseY = AndSIMD ( fl4NoiseY, fl4AbsValY );
		fl4NoiseZ = AndSIMD ( fl4NoiseZ, fl4AbsValZ );

		if ( m_bNoiseAbs )
		{
			if ( m_vecAbsValInv.x	!= 0.0f )
			{
				fl4NoiseX = SubSIMD( Four_Ones, fl4NoiseX );
			}

			if ( m_vecAbsValInv.y	!= 0.0f )
			{											   
				fl4NoiseY = SubSIMD( Four_Ones, fl4NoiseY );
			}
			if ( m_vecAbsValInv.z	!= 0.0f )
			{
				fl4NoiseZ = SubSIMD( Four_Ones, fl4NoiseZ );
			}
		}

		FourVectors fvOffset;

		fvOffset.x = AddSIMD( fl4ValueBaseX, ( MulSIMD( fl4ValueScaleX , fl4NoiseX ) ) );
		fvOffset.y = AddSIMD( fl4ValueBaseY, ( MulSIMD( fl4ValueScaleY , fl4NoiseY ) ) );
		fvOffset.z = AddSIMD( fl4ValueBaseZ, ( MulSIMD( fl4ValueScaleZ , fl4NoiseZ ) ) );

		fvOffset *= pParticles->m_flPreviousDt;  

		if ( m_bLocalSpace )
		{
			CPTransform.VectorRotate( fvOffset );
		}

		*pxyz -= fvOffset;

		xyz += attr_stride;
		pxyz += attr_stride;
		pCreationTime += attr_stride;

	}
}


void C_INIT_InitialVelocityNoise::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	float	flAbsScaleX, flAbsScaleY, flAbsScaleZ;
	int		nAbsValX, nAbsValY, nAbsValZ;
	nAbsValX = 0xffffffff; 
	nAbsValY = 0xffffffff;
	nAbsValZ = 0xffffffff;
	flAbsScaleX = 0.5;
	flAbsScaleY = 0.5; 
	flAbsScaleZ = 0.5;
	// Set up single if check for absolute value inversion inside the loop
	bool m_bNoiseAbs = ( m_vecAbsValInv.x != 0.0f ) || ( m_vecAbsValInv.y != 0.0f ) || ( m_vecAbsValInv.z != 0.0f );
	// Set up values for more optimal absolute value calculations inside the loop
	if ( m_vecAbsVal.x	!= 0.0f )
	{
		nAbsValX = 0x7fffffff;
		flAbsScaleX = 1.0;
	}
	if ( m_vecAbsVal.y	!= 0.0f )
	{
		nAbsValY = 0x7fffffff;
		flAbsScaleY = 1.0;
	}
	if ( m_vecAbsVal.z	!= 0.0f )
	{
		nAbsValZ = 0x7fffffff;
		flAbsScaleZ = 1.0;
	}

	float ValueScaleX, ValueScaleY, ValueScaleZ, ValueBaseX, ValueBaseY, ValueBaseZ;

	ValueScaleX = ( flAbsScaleX *(m_vecOutputMax.x-m_vecOutputMin.x ) );
	ValueBaseX = (m_vecOutputMin.x+ ( ( 1.0 - flAbsScaleX ) *( m_vecOutputMax.x-m_vecOutputMin.x ) ) );

	ValueScaleY = ( flAbsScaleY *(m_vecOutputMax.y-m_vecOutputMin.y ) );
	ValueBaseY = (m_vecOutputMin.y+ ( ( 1.0 - flAbsScaleY ) *( m_vecOutputMax.y-m_vecOutputMin.y ) ) );

	ValueScaleZ = ( flAbsScaleZ *(m_vecOutputMax.z-m_vecOutputMin.z ) );
	ValueBaseZ = (m_vecOutputMin.z+ ( ( 1.0 - flAbsScaleZ ) *( m_vecOutputMax.z-m_vecOutputMin.z ) ) );


	float CoordScale = m_flNoiseScale;
	float CoordScaleLoc = m_flNoiseScaleLoc;

	Vector ofs_y = Vector( 100000.5, 300000.25, 9000000.75 );
	Vector ofs_z = Vector( 110000.25, 310000.75, 9100000.5 );

	for( ; nParticleCount--; start_p++ )
	{	
		const float *xyz = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, start_p );		
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
		const float *pCreationTime = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
	
		Vector Coord, Coord2, Coord3, CoordLoc;
		SetVectorFromAttribute( CoordLoc, xyz );
		CoordLoc += m_vecOffsetLoc;

		float Offset = m_flOffset;
		Coord = Vector ( (*pCreationTime + Offset), (*pCreationTime + Offset), (*pCreationTime + Offset) );

		Coord *= CoordScale;
		CoordLoc *= CoordScaleLoc;
		Coord += CoordLoc;

		Coord2 = ( Coord );
		Coord3 = ( Coord );

		fltx4 flNoise128;
		FourVectors fvNoise;

		fvNoise.DuplicateVector( Coord );
		flNoise128 = NoiseSIMD( fvNoise );
		float flNoiseX = SubFloat( flNoise128, 0 );

		fvNoise.DuplicateVector( Coord2 + ofs_y );
		flNoise128 = NoiseSIMD( fvNoise );
		float flNoiseY = SubFloat( flNoise128, 0 );

		fvNoise.DuplicateVector( Coord3 + ofs_z );
		flNoise128 = NoiseSIMD( fvNoise );
		float flNoiseZ = SubFloat( flNoise128, 0 );

		*( (int *) &flNoiseX)  &= nAbsValX;
		*( (int *) &flNoiseY)  &= nAbsValY;
		*( (int *) &flNoiseZ)  &= nAbsValZ;

		if ( m_bNoiseAbs )
		{
			if ( m_vecAbsValInv.x	!= 0.0f )
			{
				flNoiseX = 1.0 - flNoiseX;
			}

			if ( m_vecAbsValInv.y	!= 0.0f )
			{											   
				flNoiseY = 1.0 - flNoiseY;
			}
			if ( m_vecAbsValInv.z	!= 0.0f )
			{
				flNoiseZ = 1.0 - flNoiseZ;
			}
		}

		Vector poffset;
		poffset.x = ( ValueBaseX + ( ValueScaleX * flNoiseX ) );
		poffset.y = ( ValueBaseY + ( ValueScaleY * flNoiseY ) );
		poffset.z = ( ValueBaseZ + ( ValueScaleZ * flNoiseZ ) );

		poffset *= pParticles->m_flPreviousDt;  

		if ( m_bLocalSpace )
		{
			matrix3x4_t mat;
			pParticles->GetControlPointTransformAtTime( m_nControlPointNumber, *pCreationTime, &mat );
			Vector vecTransformLocal = vec3_origin;
			VectorRotate( poffset, mat, vecTransformLocal );
			poffset = vecTransformLocal;
		}
		pxyz[0] -= poffset.x;
		pxyz[4] -= poffset.y;
		pxyz[8] -= poffset.z;
	}
}




class C_INIT_RandomLifeTime : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomLifeTime );

	float m_fLifetimeMin;
	float m_fLifetimeMax;
	float m_fLifetimeRandExponent;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask, void *pContext ) const;

	void InitNewParticlesBlock( CParticleCollection *pParticles, 
										int start_block, int n_blocks, int nAttributeWriteMask,
										void *pContext ) const
	{
		if ( m_fLifetimeRandExponent != 1.0f )
		{
			InitScalarAttributeRandomRangeExpBlock( PARTICLE_ATTRIBUTE_LIFE_DURATION,
													m_fLifetimeMin, m_fLifetimeMax, m_fLifetimeRandExponent,
													pParticles, start_block, n_blocks );
		}
		else
		{
			InitScalarAttributeRandomRangeBlock( PARTICLE_ATTRIBUTE_LIFE_DURATION,
													m_fLifetimeMin, m_fLifetimeMax, pParticles, start_block, n_blocks );
		}

	}

};

DEFINE_PARTICLE_OPERATOR( C_INIT_RandomLifeTime, "Lifetime Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomLifeTime ) 
	DMXELEMENT_UNPACK_FIELD( "lifetime_min", "0", float, m_fLifetimeMin )
	DMXELEMENT_UNPACK_FIELD( "lifetime_max", "0", float, m_fLifetimeMax )
	DMXELEMENT_UNPACK_FIELD( "lifetime_random_exponent", "1", float, m_fLifetimeRandExponent )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomLifeTime )

void C_INIT_RandomLifeTime::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	for( ; nParticleCount--; start_p++ )
	{
		float *dtime = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, start_p );
		*dtime = pParticles->RandomFloatExp( m_fLifetimeMin, m_fLifetimeMax, m_fLifetimeRandExponent );
	}
}


//-----------------------------------------------------------------------------
// Random radius
//-----------------------------------------------------------------------------
class C_INIT_RandomRadius : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomRadius );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_RADIUS_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask, void *pContext ) const;

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
										int start_block, int n_blocks, int nAttributeWriteMask,
										void *pContext ) const
	{
		if ( m_flRadiusRandExponent != 1.0f )
		{
			InitScalarAttributeRandomRangeExpBlock( PARTICLE_ATTRIBUTE_RADIUS,
				m_flRadiusMin, m_flRadiusMax, m_flRadiusRandExponent,
				pParticles, start_block, n_blocks );
		}
		else
		{
			InitScalarAttributeRandomRangeBlock( PARTICLE_ATTRIBUTE_RADIUS,
				m_flRadiusMin, m_flRadiusMax, 
				pParticles, start_block, n_blocks );
		}

	}

	float m_flRadiusMin;
	float m_flRadiusMax;
	float m_flRadiusRandExponent;
};


DEFINE_PARTICLE_OPERATOR( C_INIT_RandomRadius, "Radius Random", OPERATOR_PI_RADIUS );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomRadius ) 
	DMXELEMENT_UNPACK_FIELD( "radius_min", "1", float, m_flRadiusMin )
	DMXELEMENT_UNPACK_FIELD( "radius_max", "1", float, m_flRadiusMax )
	DMXELEMENT_UNPACK_FIELD( "radius_random_exponent", "1", float, m_flRadiusRandExponent )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomRadius )

void C_INIT_RandomRadius::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask,
	void *pContext) const
{
	for( ; nParticleCount--; start_p++ )
	{
		float *r = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_RADIUS, start_p );
		*r = pParticles->RandomFloatExp( m_flRadiusMin, m_flRadiusMax, m_flRadiusRandExponent );
	}
}


//-----------------------------------------------------------------------------
// Random alpha
//-----------------------------------------------------------------------------
class C_INIT_RandomAlpha : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomAlpha );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_ALPHA_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	virtual void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		m_flAlphaMin = m_nAlphaMin / 255.0f;
		m_flAlphaMax = m_nAlphaMax / 255.0f;
	}

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const
	{
		if ( m_flAlphaRandExponent != 1.0f )
		{
			InitScalarAttributeRandomRangeExpBlock( PARTICLE_ATTRIBUTE_ALPHA,
				m_flAlphaMin, m_flAlphaMax, m_flAlphaRandExponent,
				pParticles, start_block, n_blocks );
		}
		else
		{
			InitScalarAttributeRandomRangeBlock( PARTICLE_ATTRIBUTE_ALPHA,
				m_flAlphaMin, m_flAlphaMax,
				pParticles, start_block, n_blocks );
		}
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p, int nParticleCount, int nAttributeWriteMask, void *pContext ) const
	{
		for( ; nParticleCount--; start_p++ )
		{
			float *pAlpha = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_ALPHA, start_p );
			*pAlpha = pParticles->RandomFloatExp( m_flAlphaMin, m_flAlphaMax, m_flAlphaRandExponent );
		}
	}

	int m_nAlphaMin;
	int m_nAlphaMax;
	float m_flAlphaMin;
	float m_flAlphaMax;
	float m_flAlphaRandExponent;
};


DEFINE_PARTICLE_OPERATOR( C_INIT_RandomAlpha, "Alpha Random", OPERATOR_PI_ALPHA );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomAlpha ) 
	DMXELEMENT_UNPACK_FIELD( "alpha_min", "255", int, m_nAlphaMin )
	DMXELEMENT_UNPACK_FIELD( "alpha_max", "255", int, m_nAlphaMax )
	DMXELEMENT_UNPACK_FIELD( "alpha_random_exponent", "1", float, m_flAlphaRandExponent )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomAlpha )


//-----------------------------------------------------------------------------
// Random rotation
//-----------------------------------------------------------------------------
class CGeneralRandomRotation : public CParticleOperatorInstance
{
protected:
	virtual int GetAttributeToInit( void ) const = 0;

	uint32 GetWrittenAttributes( void ) const
	{
		return (1 << GetAttributeToInit() );
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	virtual void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		m_flRadians = m_flDegrees * ( M_PI / 180.0f );
		m_flRadiansMin = m_flDegreesMin * ( M_PI / 180.0f );
		m_flRadiansMax = m_flDegreesMax * ( M_PI / 180.0f );
	}

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const
	{
		if ( m_flRotationRandExponent != 1.0f )
		{
			InitScalarAttributeRandomRangeExpBlock(  GetAttributeToInit(),
				m_flRadiansMin, m_flRadiansMax, m_flRotationRandExponent,
				pParticles, start_block, n_blocks );
		}
		else
		{
			InitScalarAttributeRandomRangeBlock(  GetAttributeToInit(),
				m_flRadiansMin, m_flRadiansMax,
				pParticles, start_block, n_blocks );
		}
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p, int nParticleCount, int nAttributeWriteMask, void *pContext ) const
	{
		for( ; nParticleCount--; start_p++ )
		{
			float *drot = pParticles->GetFloatAttributePtrForWrite( GetAttributeToInit(), start_p );
			*drot = m_flRadians + pParticles->RandomFloatExp( m_flRadiansMin, m_flRadiansMax, m_flRotationRandExponent );
		}
	}

	// User-specified range
	float m_flDegreesMin;
	float m_flDegreesMax;
	float m_flDegrees;

	// Converted range
	float m_flRadiansMin;
	float m_flRadiansMax;
	float m_flRadians;
	float m_flRotationRandExponent;
};


class CAddGeneralRandomRotation : public CParticleOperatorInstance
{
protected:
	virtual int GetAttributeToInit( void ) const = 0;

	uint32 GetWrittenAttributes( void ) const
	{
		return (1 << GetAttributeToInit() );
	}

	uint32 GetReadAttributes( void ) const
	{
		return (1 << GetAttributeToInit() );
	}

	virtual bool InitMultipleOverride() { return true; }

	virtual void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		m_flRadians = m_flDegrees * ( M_PI / 180.0f );
		m_flRadiansMin = m_flDegreesMin * ( M_PI / 180.0f );
		m_flRadiansMax = m_flDegreesMax * ( M_PI / 180.0f );
	}

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const
	{
		AddScalarAttributeRandomRangeBlock( GetAttributeToInit(),
			m_flRadiansMin, m_flRadiansMax, m_flRotationRandExponent,
			pParticles, start_block, n_blocks, m_bRandomlyFlipDirection );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p, int nParticleCount, int nAttributeWriteMask, void *pContext ) const
	{
		if ( !m_bRandomlyFlipDirection )
		{
			for( ; nParticleCount--; start_p++ )
			{
				float *pAttr = pParticles->GetFloatAttributePtrForWrite( GetAttributeToInit(), start_p );
				*pAttr += m_flRadians + pParticles->RandomFloatExp( m_flRadiansMin, m_flRadiansMax, m_flRotationRandExponent );
			}
		}
		else
		{
			for( ; nParticleCount--; start_p++ )
			{
				float *pAttr = pParticles->GetFloatAttributePtrForWrite( GetAttributeToInit(), start_p );
				float flSpeed = m_flRadians + pParticles->RandomFloatExp( m_flRadiansMin, m_flRadiansMax, m_flRotationRandExponent );
				bool bFlip = ( pParticles->RandomFloat( -1.0f, 1.0f ) >= 0.0f );
				*pAttr += bFlip ? -flSpeed : flSpeed; 
			}
		}
	}

	// User-specified range
	float m_flDegreesMin;
	float m_flDegreesMax;
	float m_flDegrees;

	// Converted range
	float m_flRadiansMin;
	float m_flRadiansMax;
	float m_flRadians;
	float m_flRotationRandExponent;
	bool m_bRandomlyFlipDirection;
};


//-----------------------------------------------------------------------------
// Random rotation
//-----------------------------------------------------------------------------
class C_INIT_RandomRotation : public CGeneralRandomRotation
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomRotation );

	virtual int GetAttributeToInit( void ) const
	{
		return PARTICLE_ATTRIBUTE_ROTATION;
	}
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RandomRotation, "Rotation Random", OPERATOR_PI_ROTATION );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomRotation ) 
	DMXELEMENT_UNPACK_FIELD( "rotation_initial", "0", float, m_flDegrees )
	DMXELEMENT_UNPACK_FIELD( "rotation_offset_min", "0", float, m_flDegreesMin )
	DMXELEMENT_UNPACK_FIELD( "rotation_offset_max", "360", float, m_flDegreesMax )
	DMXELEMENT_UNPACK_FIELD( "rotation_random_exponent", "1", float, m_flRotationRandExponent )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomRotation )


//-----------------------------------------------------------------------------
// Random rotation speed
//-----------------------------------------------------------------------------
class C_INIT_RandomRotationSpeed : public CAddGeneralRandomRotation
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomRotationSpeed );

	virtual int GetAttributeToInit( void ) const
	{
		return PARTICLE_ATTRIBUTE_ROTATION_SPEED;
	}
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RandomRotationSpeed, "Rotation Speed Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomRotationSpeed ) 
	DMXELEMENT_UNPACK_FIELD( "rotation_speed_constant", "0", float, m_flDegrees )
	DMXELEMENT_UNPACK_FIELD( "rotation_speed_random_min", "0", float, m_flDegreesMin )
	DMXELEMENT_UNPACK_FIELD( "rotation_speed_random_max", "360", float, m_flDegreesMax )
	DMXELEMENT_UNPACK_FIELD( "rotation_speed_random_exponent", "1", float, m_flRotationRandExponent )
	DMXELEMENT_UNPACK_FIELD( "randomly_flip_direction", "1", bool, m_bRandomlyFlipDirection )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomRotationSpeed )


//-----------------------------------------------------------------------------
// Random yaw
//-----------------------------------------------------------------------------
class C_INIT_RandomYaw : public CGeneralRandomRotation
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomYaw );

	virtual int GetAttributeToInit( void ) const
	{
		return PARTICLE_ATTRIBUTE_YAW;
	}
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RandomYaw, "Rotation Yaw Random", OPERATOR_PI_YAW );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomYaw ) 
	DMXELEMENT_UNPACK_FIELD( "yaw_initial", "0", float, m_flDegrees )
	DMXELEMENT_UNPACK_FIELD( "yaw_offset_min", "0", float, m_flDegreesMin )
	DMXELEMENT_UNPACK_FIELD( "yaw_offset_max", "360", float, m_flDegreesMax )
	DMXELEMENT_UNPACK_FIELD( "yaw_random_exponent", "1", float, m_flRotationRandExponent )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomYaw )


//-----------------------------------------------------------------------------
// Random color
//-----------------------------------------------------------------------------
class C_INIT_RandomColor : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomColor );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_TINT_RGB_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	struct C_OP_RandomColorContext_t
	{
		Vector m_vPrevPosition;
	};

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( C_OP_RandomColorContext_t );
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		C_OP_RandomColorContext_t *pCtx=reinterpret_cast<C_OP_RandomColorContext_t *>( pContext );
		pCtx->m_vPrevPosition = vec3_origin;
	}

	virtual void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		m_flNormColorMin[0] = (float) m_ColorMin[0] / 255.0f;
		m_flNormColorMin[1] = (float) m_ColorMin[1] / 255.0f;
		m_flNormColorMin[2] = (float) m_ColorMin[2] / 255.0f;

		m_flNormColorMax[0] = (float) m_ColorMax[0] / 255.0f;
		m_flNormColorMax[1] = (float) m_ColorMax[1] / 255.0f;
		m_flNormColorMax[2] = (float) m_ColorMax[2] / 255.0f;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p, int nParticleCount, int nAttributeWriteMask, void *pContext ) const
	{
		C_OP_RandomColorContext_t *pCtx=reinterpret_cast<C_OP_RandomColorContext_t *>( pContext );

		Color	tint( 255, 255, 255, 255 );

		// If we're factoring in luminosity or tint, then get our lighting info for this position
		if ( m_flTintPerc )
		{
			if ( pParticles->m_pParent && pParticles->m_pParent->m_LocalLightingCP == m_nTintCP )
			{
				tint = pParticles->m_pParent->m_LocalLighting;
			}
			else
			{
				// FIXME: Really, we want the emission point for each particle, but for now, we do it more cheaply
				// Get our control point
				Vector vecOrigin;
				pParticles->GetControlPointAtTime( m_nTintCP, pParticles->m_flCurTime, &vecOrigin );

				if ( ( ( pCtx->m_vPrevPosition - vecOrigin ).Length() >= m_flUpdateThreshold ) || ( pParticles->m_LocalLightingCP == -1 ) )
					{
						g_pParticleSystemMgr->Query()->GetLightingAtPoint( vecOrigin, tint );
						pParticles->m_LocalLighting = tint;
						pParticles->m_LocalLightingCP = m_nTintCP;
						pCtx->m_vPrevPosition = vecOrigin;
					}
				else
					tint = pParticles->m_LocalLighting;

			}
			tint[0] = max ( m_TintMin[0], min( tint[0], m_TintMax[0] ) );
			tint[1] = max ( m_TintMin[1], min( tint[1], m_TintMax[1] ) );
			tint[2] = max ( m_TintMin[2], min( tint[2], m_TintMax[2] ) );	
		}

		float randomPerc;
		float *pColor;
		for( ; nParticleCount--; start_p++ )
		{
			pColor = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_TINT_RGB, start_p );
			
			randomPerc = pParticles->RandomFloat( 0.0f, 1.0f );
			
			// Randomly choose a range between the two colors
			pColor[0] = m_flNormColorMin[0] + ( ( m_flNormColorMax[0] - m_flNormColorMin[0] ) * randomPerc );
			pColor[4] = m_flNormColorMin[1] + ( ( m_flNormColorMax[1] - m_flNormColorMin[1] ) * randomPerc );
			pColor[8] = m_flNormColorMin[2] + ( ( m_flNormColorMax[2] - m_flNormColorMin[2] ) * randomPerc );

			// Tint the particles
			if ( m_flTintPerc )
			{
				pColor[0] = Lerp( m_flTintPerc, (float) pColor[0], (float) tint.r() / 255.0f );
				pColor[4] = Lerp( m_flTintPerc, (float) pColor[4], (float) tint.g() / 255.0f );
				pColor[8] = Lerp( m_flTintPerc, (float) pColor[8], (float) tint.b() / 255.0f );
			}
		}
	}

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const
	{
		C_OP_RandomColorContext_t *pCtx=reinterpret_cast<C_OP_RandomColorContext_t *>( pContext );

		Color	tint( 255, 255, 255, 255 );

		size_t attr_stride;

		FourVectors *pColor = pParticles->Get4VAttributePtrForWrite( PARTICLE_ATTRIBUTE_TINT_RGB, &attr_stride );
		
		pColor += attr_stride * start_block;
		
		FourVectors fvColorMin;
		fvColorMin.DuplicateVector( Vector (m_flNormColorMin[0], m_flNormColorMin[1], m_flNormColorMin[2] ) );
		FourVectors fvColorWidth;
		fvColorWidth.DuplicateVector( Vector (m_flNormColorMax[0] - m_flNormColorMin[0], m_flNormColorMax[1] - m_flNormColorMin[1], m_flNormColorMax[2] - m_flNormColorMin[2] ) );

		int nRandContext = GetSIMDRandContext();

		// If we're factoring in luminosity or tint, then get our lighting info for this position
		if ( m_flTintPerc )
		{
			if ( pParticles->m_pParent && pParticles->m_pParent->m_LocalLightingCP == m_nTintCP )
			{
				tint = pParticles->m_pParent->m_LocalLighting;
			}
			else
			{
				// FIXME: Really, we want the emission point for each particle, but for now, we do it more cheaply
				// Get our control point
				Vector vecOrigin;
				pParticles->GetControlPointAtTime( m_nTintCP, pParticles->m_flCurTime, &vecOrigin );

				if ( ( ( pCtx->m_vPrevPosition - vecOrigin ).Length() >= m_flUpdateThreshold ) || ( pParticles->m_LocalLightingCP == -1 ) )
				{
					g_pParticleSystemMgr->Query()->GetLightingAtPoint( vecOrigin, tint );
					pParticles->m_LocalLighting = tint;
					pParticles->m_LocalLightingCP = m_nTintCP;
					pCtx->m_vPrevPosition = vecOrigin;
				}
				else
					tint = pParticles->m_LocalLighting;
			}

			tint[0] = max ( m_TintMin[0], min( tint[0], m_TintMax[0] ) );
			tint[1] = max ( m_TintMin[1], min( tint[1], m_TintMax[1] ) );
			tint[2] = max ( m_TintMin[2], min( tint[2], m_TintMax[2] ) );

			FourVectors fvTint;
			fvTint.DuplicateVector( Vector ( tint[0], tint[1], tint[2] ) );
			fltx4 fl4Divisor = ReplicateX4( 1.0f / 255.0f );
			fvTint *= fl4Divisor;
			fltx4 fl4TintPrc = ReplicateX4( m_flTintPerc );

			while( n_blocks-- )
			{
				FourVectors fvColor = fvColorWidth;
				FourVectors fvColor2 = fvTint;
				fvColor *= RandSIMD( nRandContext );
				fvColor += fvColorMin;
				fvColor2 -= fvColor;
				fvColor2 *= fl4TintPrc;
				fvColor2 += fvColor;
				*pColor = fvColor2;
				pColor += attr_stride;
			}
		}
		else
		{
			while( n_blocks-- )
			{
				FourVectors fvColor = fvColorWidth;
				fvColor *= RandSIMD( nRandContext );
				fvColor += fvColorMin;
				*pColor = fvColor;
				pColor += attr_stride;
			}
		}
		ReleaseSIMDRandContext( nRandContext );
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nTintCP;
	}

	float	m_flNormColorMin[3];
	float	m_flNormColorMax[3];
	Color	m_ColorMin;
	Color	m_ColorMax;
	Color	m_TintMin;
	Color	m_TintMax;
	float	m_flTintPerc;
	float	m_flUpdateThreshold;
	int		m_nTintCP;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RandomColor, "Color Random", OPERATOR_PI_TINT_RGB );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomColor ) 
	DMXELEMENT_UNPACK_FIELD( "color1", "255 255 255 255", Color, m_ColorMin )
	DMXELEMENT_UNPACK_FIELD( "color2", "255 255 255 255", Color, m_ColorMax )
	DMXELEMENT_UNPACK_FIELD( "tint_perc", "0.0", float, m_flTintPerc )
	DMXELEMENT_UNPACK_FIELD( "tint control point", "0", int, m_nTintCP )
	DMXELEMENT_UNPACK_FIELD( "tint clamp min", "0 0 0 0", Color, m_TintMin )
	DMXELEMENT_UNPACK_FIELD( "tint clamp max", "255 255 255 255", Color, m_TintMax )
	DMXELEMENT_UNPACK_FIELD( "tint update movement threshold", "32", float, m_flUpdateThreshold )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomColor )


//-----------------------------------------------------------------------------
// Trail Length
//-----------------------------------------------------------------------------
class C_INIT_RandomTrailLength : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomTrailLength );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_TRAIL_LENGTH_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	virtual void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
	}

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const
	{
		if ( m_flLengthRandExponent != 1.0f )
		{
			InitScalarAttributeRandomRangeExpBlock( PARTICLE_ATTRIBUTE_TRAIL_LENGTH,
				m_flMinLength, m_flMaxLength, m_flLengthRandExponent,
				pParticles, start_block, n_blocks );
		}
		else
		{
			InitScalarAttributeRandomRangeBlock( PARTICLE_ATTRIBUTE_TRAIL_LENGTH,
				m_flMinLength, m_flMaxLength,
				pParticles, start_block, n_blocks );
		}
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p, int nParticleCount, int nAttributeWriteMask, void *pContext ) const
	{
		float *pLength;
		for( ; nParticleCount--; start_p++ )
		{
			pLength = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_TRAIL_LENGTH, start_p );
			*pLength = pParticles->RandomFloatExp( m_flMinLength, m_flMaxLength, m_flLengthRandExponent );
		}
	}

	float m_flMinLength;
	float m_flMaxLength;
	float m_flLengthRandExponent;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RandomTrailLength, "Trail Length Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomTrailLength ) 
	DMXELEMENT_UNPACK_FIELD( "length_min", "0.1", float, m_flMinLength )
	DMXELEMENT_UNPACK_FIELD( "length_max", "0.1", float, m_flMaxLength )
	DMXELEMENT_UNPACK_FIELD( "length_random_exponent", "1", float, m_flLengthRandExponent )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomTrailLength )

//-----------------------------------------------------------------------------
// Random sequence
//-----------------------------------------------------------------------------
class C_INIT_RandomSequence : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomSequence );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	virtual void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		// TODO: Validate the ranges here!
	}

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const
	{
		InitScalarAttributeRandomRangeBlock( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER,
			m_nSequenceMin, m_nSequenceMax,
			pParticles, start_block, n_blocks );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p, int nParticleCount, int nAttributeWriteMask, void *pContext ) const
	{
		float *pSequence;
		for( ; nParticleCount--; start_p++ )
		{
			pSequence = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER, start_p );
			*pSequence = pParticles->RandomInt( m_nSequenceMin, m_nSequenceMax );
		}
	}

	int m_nSequenceMin;
	int m_nSequenceMax;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RandomSequence, "Sequence Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomSequence ) 
	DMXELEMENT_UNPACK_FIELD( "sequence_min", "0", int, m_nSequenceMin )
	DMXELEMENT_UNPACK_FIELD( "sequence_max", "0", int, m_nSequenceMax )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomSequence )

 
//-----------------------------------------------------------------------------
// Position Warp Initializer
// Scales initial position and velocity of particles within a random vector range
//-----------------------------------------------------------------------------
class C_INIT_PositionWarp : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_PositionOffset );

	Vector m_vecWarpMin;
	Vector m_vecWarpMax;
	int m_nControlPointNumber;
	float m_flWarpTime, m_flWarpStartTime;
	bool m_bInvertWarp;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;

	void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		m_nControlPointNumber = max( 0, min( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
	}

	bool InitMultipleOverride ( void ) { return true; }

};

DEFINE_PARTICLE_OPERATOR( C_INIT_PositionWarp, "Position Modify Warp Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_PositionWarp ) 
	DMXELEMENT_UNPACK_FIELD( "control point number", "0", int, m_nControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "warp min", "1 1 1", Vector, m_vecWarpMin )
	DMXELEMENT_UNPACK_FIELD( "warp max", "1 1 1", Vector, m_vecWarpMax )
	DMXELEMENT_UNPACK_FIELD( "warp transition time (treats min/max as start/end sizes)", "0", float , m_flWarpTime )
	DMXELEMENT_UNPACK_FIELD( "warp transition start time", "0", float , m_flWarpStartTime )
	DMXELEMENT_UNPACK_FIELD( "reverse warp (0/1)", "0", bool , m_bInvertWarp )	
END_PARTICLE_OPERATOR_UNPACK( C_INIT_PositionWarp )


void C_INIT_PositionWarp::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	Vector vecWarpStart = m_vecWarpMin;
	Vector vecWarpEnd = m_vecWarpMax;

	if ( m_bInvertWarp )
	{
		vecWarpStart = m_vecWarpMax;
		vecWarpEnd = m_vecWarpMin;
	}

	for( ; nParticleCount--; start_p++ )
	{
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		
		Vector randpos;
		
		if ( m_flWarpTime != 0.0f )
		{ 
			float flWarpEnd = m_flWarpStartTime + m_flWarpTime;
			float flPercentage = RemapValClamped( *ct, m_flWarpStartTime, flWarpEnd, 0.0, 1.0 );
			VectorLerp( vecWarpStart, vecWarpEnd, flPercentage, randpos );
		}
		else
		{
			pParticles->RandomVector( m_vecWarpMin, m_vecWarpMax, &randpos );
		}


		matrix3x4_t mat;
		pParticles->GetControlPointTransformAtTime( m_nControlPointNumber, *ct, &mat );
		Vector vecTransformLocal = vec3_origin;
		Vector vecParticlePosition, vecParticlePosition_prev ;
		SetVectorFromAttribute( vecParticlePosition, xyz ); 
		SetVectorFromAttribute( vecParticlePosition_prev, pxyz );
		// rotate particles from world space into local
		VectorITransform( vecParticlePosition, mat, vecTransformLocal );
		// multiply position by desired amount
		vecTransformLocal.x *= randpos.x;
		vecTransformLocal.y *= randpos.y;
		vecTransformLocal.z *= randpos.z;
		// rotate back into world space
		VectorTransform( vecTransformLocal, mat, vecParticlePosition );
		// rinse, repeat
		VectorITransform( vecParticlePosition_prev, mat, vecTransformLocal ); 
		vecTransformLocal.x *= randpos.x;
		vecTransformLocal.y *= randpos.y;
		vecTransformLocal.z *= randpos.z;
		VectorTransform( vecTransformLocal, mat, vecParticlePosition_prev );
		// set positions into floats
		SetVectorAttribute( xyz, vecParticlePosition ); 
		SetVectorAttribute( pxyz, vecParticlePosition_prev ); 
	}
}


//-----------------------------------------------------------------------------
// noise initializer
//-----------------------------------------------------------------------------
class C_INIT_CreationNoise : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_CreationNoise );

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK | PARTICLE_ATTRIBUTE_XYZ_MASK;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;

	void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const;

	virtual bool IsScrubSafe() { return true; }
	int		m_nFieldOutput;
	bool	m_bAbsVal, m_bAbsValInv;
	float	m_flOffset;
	float	m_flOutputMin;
	float	m_flOutputMax;
	float	m_flNoiseScale, m_flNoiseScaleLoc;
	Vector  m_vecOffsetLoc;
	float   m_flWorldTimeScale;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_CreationNoise, "Remap Noise to Scalar", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_CreationNoise )
	DMXELEMENT_UNPACK_FIELD( "time noise coordinate scale","0.1",float,m_flNoiseScale)
	DMXELEMENT_UNPACK_FIELD( "spatial noise coordinate scale","0.001",float,m_flNoiseScaleLoc)
	DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "3", int, m_nFieldOutput, "intchoice particlefield_scalar" )
	DMXELEMENT_UNPACK_FIELD( "time coordinate offset","0", float, m_flOffset )
	DMXELEMENT_UNPACK_FIELD( "spatial coordinate offset","0 0 0", Vector, m_vecOffsetLoc )
	DMXELEMENT_UNPACK_FIELD( "absolute value","0", bool, m_bAbsVal )
	DMXELEMENT_UNPACK_FIELD( "invert absolute value","0", bool, m_bAbsValInv )
	DMXELEMENT_UNPACK_FIELD( "output minimum","0", float, m_flOutputMin )
	DMXELEMENT_UNPACK_FIELD( "output maximum","1", float, m_flOutputMax )
	DMXELEMENT_UNPACK_FIELD( "world time noise coordinate scale","0", float, m_flWorldTimeScale )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_CreationNoise );




void C_INIT_CreationNoise::InitNewParticlesBlock( CParticleCollection *pParticles, 
												 int start_block, int n_blocks, int nAttributeWriteMask,
												 void *pContext ) const
{
	float		flAbsScale;
	fltx4 		fl4AbsVal;
	fl4AbsVal = CmpEqSIMD( Four_Zeros, Four_Zeros ); 
	flAbsScale = 0.5;

	// Set up values for more optimal absolute value calculations inside the loop
	if ( m_bAbsVal )
	{
		fl4AbsVal = LoadAlignedSIMD( (float *) g_SIMD_clear_signmask );
		flAbsScale = 1.0;
	}

	float fMin = m_flOutputMin;
	float fMax = m_flOutputMax;	

	if ( ATTRIBUTES_WHICH_ARE_ANGLES & (1 << m_nFieldOutput ) )
	{
		fMin *= ( M_PI / 180.0f );
		fMax *= ( M_PI / 180.0f );
	}	

	float CoordScale = m_flNoiseScale;
	float CoordScaleLoc = m_flNoiseScaleLoc;

	float ValueScale, ValueBase;
	ValueScale = ( flAbsScale *( fMax - fMin ) );
	ValueBase = ( fMin+ ( ( 1.0 - flAbsScale ) *( fMax - fMin ) ) );

	fltx4 fl4ValueBase = ReplicateX4( ValueBase );
	fltx4 fl4ValueScale = ReplicateX4( ValueScale );

	size_t attr_stride;

	fltx4 *pAttr = pParticles->GetM128AttributePtrForWrite( m_nFieldOutput, &attr_stride );
	pAttr += attr_stride * start_block;
	const FourVectors *pxyz = pParticles->Get4VAttributePtr( PARTICLE_ATTRIBUTE_XYZ, &attr_stride );
	pxyz += attr_stride * start_block;
	const fltx4 *pCreationTime = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, &attr_stride );
	pCreationTime += attr_stride * start_block;

	//setup
	fltx4 fl4Offset = ReplicateX4( m_flOffset );
	FourVectors fvOffsetLoc;
	fvOffsetLoc.DuplicateVector( m_vecOffsetLoc );
	FourVectors fvCoordBase;
	fvCoordBase.x = AddSIMD(*pCreationTime, fl4Offset);
	fvCoordBase.y = AddSIMD(*pCreationTime, fl4Offset);
	fvCoordBase.z = AddSIMD(*pCreationTime, fl4Offset);
	fvCoordBase *= CoordScale;

	while( n_blocks-- )
	{	
		FourVectors fvCoordLoc = *pxyz;
		fvCoordLoc += fvOffsetLoc;
		FourVectors fvCoord = fvCoordBase;
		fvCoordLoc *= CoordScaleLoc;
		fvCoord += fvCoordLoc;

		fltx4 fl4Noise;

		fl4Noise = NoiseSIMD( fvCoord );

		fl4Noise = AndSIMD ( fl4Noise, fl4AbsVal );

		if ( m_bAbsValInv )
		{
			fl4Noise = SubSIMD( Four_Ones, fl4Noise );
		}

		fltx4 fl4InitialNoise;

		fl4InitialNoise = AddSIMD( fl4ValueBase, ( MulSIMD( fl4ValueScale, fl4Noise ) ) );

		if ( ATTRIBUTES_WHICH_ARE_0_TO_1 & (1 << m_nFieldOutput ) )
		{
			fl4InitialNoise = MinSIMD( Four_Ones, fl4InitialNoise );
			fl4InitialNoise = MaxSIMD( Four_Zeros, fl4InitialNoise );
		}

		*( pAttr ) = fl4InitialNoise;

		pAttr += attr_stride;
		pxyz += attr_stride;

	}
}



void C_INIT_CreationNoise::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	float	flAbsScale;
	int		nAbsVal;
	nAbsVal = 0xffffffff; 
	flAbsScale = 0.5;
	if ( m_bAbsVal )
	{
		nAbsVal = 0x7fffffff;
		flAbsScale = 1.0;
	}

	float fMin = m_flOutputMin;
	float fMax = m_flOutputMax;

	if ( ATTRIBUTES_WHICH_ARE_ANGLES & (1 << m_nFieldOutput ) )
	{
		fMin *= ( M_PI / 180.0f );
		fMax *= ( M_PI / 180.0f );
	}

	float CoordScale = m_flNoiseScale;
	float CoordScaleLoc = m_flNoiseScaleLoc;

    float ValueScale, ValueBase;
	ValueScale = ( flAbsScale *( fMax - fMin ) );
	ValueBase = ( fMin+ ( ( 1.0 - flAbsScale ) *( fMax - fMin ) ) );
	
	Vector CoordLoc, CoordWorldTime, CoordBase;
	const float *pCreationTime = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
	float Offset = m_flOffset;
	CoordBase = Vector ( (*pCreationTime + Offset), (*pCreationTime + Offset), (*pCreationTime + Offset) );
	CoordBase *= CoordScale;
	CoordWorldTime = Vector( (Plat_MSTime() * m_flWorldTimeScale), (Plat_MSTime() * m_flWorldTimeScale), (Plat_MSTime() * m_flWorldTimeScale) );
	CoordBase += CoordWorldTime;

	for( ; nParticleCount--; start_p++ )
	{	
		const float *pxyz = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, start_p );
		float *pAttr = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );	

		Vector Coord = CoordBase;

		CoordLoc.x = pxyz[0]; 
		CoordLoc.y = pxyz[4];
		CoordLoc.z = pxyz[8];
		CoordLoc += m_vecOffsetLoc;

		CoordLoc *= CoordScaleLoc;
		Coord += CoordLoc;

		fltx4 flNoise128;
		FourVectors fvNoise;

		fvNoise.DuplicateVector( Coord );
		flNoise128 = NoiseSIMD( fvNoise );
		float flNoise = SubFloat( flNoise128, 0 );

		*( (int *) &flNoise)  &= nAbsVal;

		if ( m_bAbsValInv )
		{
			flNoise = 1.0 - flNoise;
		}
		    
		float flInitialNoise = ( ValueBase + ( ValueScale * flNoise ) );

		if ( ATTRIBUTES_WHICH_ARE_0_TO_1 & (1 << m_nFieldOutput ) )
		{
			flInitialNoise = clamp(flInitialNoise, 0.0f, 1.0f );
		}

		*( pAttr ) = flInitialNoise;
	}
}






class C_INIT_CreateAlongPath : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_CreateAlongPath );

	float m_fMaxDistance;
	struct CPathParameters m_PathParams;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		uint64 nStartMask = ( 1ULL << m_PathParams.m_nStartControlPointNumber ) - 1;
		uint64 nEndMask = ( 1ULL << ( m_PathParams.m_nEndControlPointNumber + 1 ) ) - 1;
		return nEndMask & (~nStartMask);
	}

	void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		m_PathParams.ClampControlPointIndices();
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;

};

DEFINE_PARTICLE_OPERATOR( C_INIT_CreateAlongPath, "Position Along Path Random", OPERATOR_PI_POSITION );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateAlongPath ) 
	DMXELEMENT_UNPACK_FIELD( "maximum distance", "0", float, m_fMaxDistance )
	DMXELEMENT_UNPACK_FIELD( "bulge", "0", float, m_PathParams.m_flBulge )
	DMXELEMENT_UNPACK_FIELD( "start control point number", "0", int, m_PathParams.m_nStartControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "end control point number", "0", int, m_PathParams.m_nEndControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "bulge control 0=random 1=orientation of start pnt 2=orientation of end point", "0", int, m_PathParams.m_nBulgeControl )
	DMXELEMENT_UNPACK_FIELD( "mid point position", "0.5", float, m_PathParams.m_flMidPoint )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateAlongPath )


void C_INIT_CreateAlongPath::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	for( ; nParticleCount--; start_p++ )
	{
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );


		Vector StartPnt, MidP, EndPnt;
		pParticles->CalculatePathValues( m_PathParams, *ct, &StartPnt, &MidP, &EndPnt);

		float t=pParticles->RandomFloat( 0.0, 1.0 );
		
		Vector randpos;
		pParticles->RandomVector( -m_fMaxDistance, m_fMaxDistance, &randpos );

		// form delta terms needed for quadratic bezier
		Vector Delta0=MidP-StartPnt;
		Vector Delta1 = EndPnt-MidP;

		Vector L0 = StartPnt+t*Delta0;
		Vector L1 = MidP+t*Delta1;

		Vector Pnt = L0+(L1-L0)*t;

		Pnt+=randpos;

		xyz[0] = Pnt.x;
		xyz[4] = Pnt.y;
		xyz[8] = Pnt.z;
		if ( pxyz && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_PREV_XYZ_MASK ) )
		{
			pxyz[0] = Pnt.x;
			pxyz[4] = Pnt.y;
			pxyz[8] = Pnt.z;
		}
	}
}





class C_INIT_MoveBetweenPoints : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_MoveBetweenPoints );

	float m_flSpeedMin, m_flSpeedMax;
	float m_flEndSpread;
	float m_flStartOffset;
	int m_nEndControlPointNumber;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nEndControlPointNumber;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;

};

DEFINE_PARTICLE_OPERATOR( C_INIT_MoveBetweenPoints, "Move Particles Between 2 Control Points", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_MoveBetweenPoints ) 
	DMXELEMENT_UNPACK_FIELD( "minimum speed", "1", float, m_flSpeedMin )
	DMXELEMENT_UNPACK_FIELD( "maximum speed", "1", float, m_flSpeedMax )
	DMXELEMENT_UNPACK_FIELD( "end spread", "0", float, m_flEndSpread )
	DMXELEMENT_UNPACK_FIELD( "start offset", "0", float, m_flStartOffset )
	DMXELEMENT_UNPACK_FIELD( "end control point", "1", int, m_nEndControlPointNumber )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_MoveBetweenPoints )


void C_INIT_MoveBetweenPoints::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	bool bMoveStartPnt = ( m_flStartOffset > 0.0 );
	for( ; nParticleCount--; start_p++ )
	{
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		float *pPrevXYZ = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );

		float *dtime = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, start_p );


		Vector StartPnt( pxyz[0], pxyz[4], pxyz[8] );

		Vector vecControlPoint;

		pParticles->GetControlPointAtTime( m_nEndControlPointNumber, *ct, &vecControlPoint );

		Vector randpos(0,0,0);

		if ( m_flEndSpread > 0.0 )
		{
			pParticles->RandomVectorInUnitSphere( &randpos );
			randpos *= m_flEndSpread;
		}
		
		vecControlPoint += randpos;

		Vector vDelta = vecControlPoint - StartPnt;
		float flLen = VectorLength( vDelta );

		if ( bMoveStartPnt )
		{
			StartPnt += ( m_flStartOffset/(flLen+FLT_EPSILON) ) * vDelta;
			vDelta = vecControlPoint - StartPnt;			
			flLen = VectorLength( vDelta );
		}

		float flVel = pParticles->RandomFloat( m_flSpeedMin, m_flSpeedMax );

		*dtime = flLen/( flVel+FLT_EPSILON);

		Vector poffset = vDelta * (flVel/flLen ) ;

		poffset *= pParticles->m_flPreviousDt;

		if ( bMoveStartPnt )
		{
			pxyz[0] = StartPnt.x;
			pxyz[1] = StartPnt.y;
			pxyz[2] = StartPnt.z;
		}

		pPrevXYZ[0] = pxyz[0] - poffset.x;
		pPrevXYZ[4] = pxyz[4] - poffset.y;
		pPrevXYZ[8] = pxyz[8] - poffset.z;
	}
}




//-----------------------------------------------------------------------------
// Remap Scalar Initializer
//-----------------------------------------------------------------------------
class C_INIT_RemapScalar : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RemapScalar );

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 1 << m_nFieldInput;
	}

	bool InitMultipleOverride ( void ) { return true; }
	
	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	int		m_nFieldInput;
	int		m_nFieldOutput;
	float	m_flInputMin;
	float	m_flInputMax;
	float	m_flOutputMin;
	float	m_flOutputMax;
	float	m_flStartTime;
	float	m_flEndTime;
	bool	m_bScaleInitialRange;
	bool	m_bActiveRange;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RemapScalar, "Remap Initial Scalar", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_RemapScalar )
	DMXELEMENT_UNPACK_FIELD( "emitter lifetime start time (seconds)", "-1", float, m_flStartTime )
	DMXELEMENT_UNPACK_FIELD( "emitter lifetime end time (seconds)", "-1", float, m_flEndTime )
	DMXELEMENT_UNPACK_FIELD_USERDATA( "input field", "8", int, m_nFieldInput, "intchoice particlefield_scalar" )
	DMXELEMENT_UNPACK_FIELD( "input minimum","0", float, m_flInputMin )
	DMXELEMENT_UNPACK_FIELD( "input maximum","1", float, m_flInputMax )
	DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "3", int, m_nFieldOutput, "intchoice particlefield_scalar" )
	DMXELEMENT_UNPACK_FIELD( "output minimum","0", float, m_flOutputMin )
	DMXELEMENT_UNPACK_FIELD( "output maximum","1", float, m_flOutputMax )
	DMXELEMENT_UNPACK_FIELD( "output is scalar of initial random range","0", bool, m_bScaleInitialRange )
	DMXELEMENT_UNPACK_FIELD( "only active within specified input range","0", bool, m_bActiveRange )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RemapScalar )

void C_INIT_RemapScalar::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	const float *pCreationTime;
	// clamp the result to 0 and 1 if it's alpha
	float flMin=m_flOutputMin;
	float flMax=m_flOutputMax;
	if ( ATTRIBUTES_WHICH_ARE_0_TO_1 & ( 1 << m_nFieldOutput ) )
	{
		flMin = clamp(m_flOutputMin, 0.0f, 1.0f );
		flMax = clamp(m_flOutputMax, 0.0f, 1.0f );
	}

	// FIXME: SSE-ize
	for( ; nParticleCount--; start_p++ )
	{
		pCreationTime = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		// using raw creation time to map to emitter lifespan
		float flLifeTime = *pCreationTime;  

		float flInput;
		if ( ATTRIBUTES_WHICH_ARE_INTS & ( 1 << m_nFieldInput ) )
		{
			const int *pInput = pParticles->GetIntAttributePtr( m_nFieldInput, start_p );
			flInput = float( *pInput );
		}
		else
		{
			const float *pInput = pParticles->GetFloatAttributePtr( m_nFieldInput, start_p );
			flInput = *pInput;
		}

		// only use within start/end time frame and, if set, active input range
		if ( ( ( ( flLifeTime < m_flStartTime ) || ( flLifeTime >= m_flEndTime ) ) && ( ( m_flStartTime != -1.0f) && ( m_flEndTime != -1.0f) ) ) || ( m_bActiveRange && ( flInput < m_flInputMin || flInput > m_flInputMax ) ) )
			continue;

		float *pOutput = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );
		float flOutput = RemapValClamped( flInput, m_flInputMin, m_flInputMax, flMin, flMax  );
		if ( m_bScaleInitialRange )
		{
			flOutput = *pOutput * flOutput;
		}
		if ( ATTRIBUTES_WHICH_ARE_INTS & ( 1 << m_nFieldOutput ) )
		{
			*pOutput = int ( flOutput );
		}
		else
		{
			*pOutput = flOutput;
		}
	}
}




//-----------------------------------------------------------------------------
// Inherit Velocity Initializer
// Causes particles to inherit the velocity of their CP at spawn
// 
//-----------------------------------------------------------------------------
class C_INIT_InheritVelocity : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_InheritVelocity );

	int m_nControlPointNumber;
	float m_flVelocityScale;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK ;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_CREATION_TIME;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		m_nControlPointNumber = max( 0, min( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
	}

	bool InitMultipleOverride ( void ) { return true; }

};

DEFINE_PARTICLE_OPERATOR( C_INIT_InheritVelocity, "Velocity Inherit from Control Point", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_InheritVelocity ) 
DMXELEMENT_UNPACK_FIELD( "control point number", "0", int, m_nControlPointNumber )
DMXELEMENT_UNPACK_FIELD( "velocity scale", "1", float, m_flVelocityScale )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_InheritVelocity )


void C_INIT_InheritVelocity::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	for( ; nParticleCount--; start_p++ )
	{
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		
		Vector vecControlPoint;
		pParticles->GetControlPointAtTime( m_nControlPointNumber, *ct, &vecControlPoint );
		Vector vecControlPointPrev;
		pParticles->GetControlPointAtPrevTime( m_nControlPointNumber, &vecControlPointPrev );

		Vector vecDeltaPos = (vecControlPoint - vecControlPointPrev);
		//Vector vecDeltaPos = (vecControlPoint - vecControlPointPrev) * pParticles->m_flDt;
		vecDeltaPos.x *= m_flVelocityScale;
		vecDeltaPos.y *= m_flVelocityScale;
		vecDeltaPos.z *= m_flVelocityScale;

		xyz[0] += vecDeltaPos.x;
		xyz[4] += vecDeltaPos.y;
		xyz[8] += vecDeltaPos.z;
	}
}


//-----------------------------------------------------------------------------
// Pre-Age Noise
// Sets particle creation time back to treat newly spawned particle as if 
// part of its life has already elapsed.
//-----------------------------------------------------------------------------
class C_INIT_AgeNoise : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_AgeNoise );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK | PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	bool InitMultipleOverride ( void ) { return true; }

	bool	m_bAbsVal, m_bAbsValInv;
	float	m_flOffset;
	float	m_flAgeMin;
	float	m_flAgeMax;
	float	m_flNoiseScale, m_flNoiseScaleLoc;
	Vector  m_vecOffsetLoc;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_AgeNoise, "Lifetime Pre-Age Noise", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_AgeNoise )
DMXELEMENT_UNPACK_FIELD( "time noise coordinate scale","1.0",float,m_flNoiseScale)
DMXELEMENT_UNPACK_FIELD( "spatial noise coordinate scale","1.0",float,m_flNoiseScaleLoc)
DMXELEMENT_UNPACK_FIELD( "time coordinate offset","0", float, m_flOffset )
DMXELEMENT_UNPACK_FIELD( "spatial coordinate offset","0 0 0", Vector, m_vecOffsetLoc )
DMXELEMENT_UNPACK_FIELD( "absolute value","0", bool, m_bAbsVal )
DMXELEMENT_UNPACK_FIELD( "invert absolute value","0", bool, m_bAbsValInv )
DMXELEMENT_UNPACK_FIELD( "start age minimum","0", float, m_flAgeMin )
DMXELEMENT_UNPACK_FIELD( "start age maximum","1", float, m_flAgeMax )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_AgeNoise );

void C_INIT_AgeNoise::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	float	flAbsScale;
	int		nAbsVal;
	nAbsVal = 0xffffffff; 
	flAbsScale = 0.5;
	if ( m_bAbsVal )
	{
		nAbsVal = 0x7fffffff;
		flAbsScale = 1.0;
	}

	float fMin = m_flAgeMin;
	float fMax = m_flAgeMax;

	float CoordScale = m_flNoiseScale;
	float CoordScaleLoc = m_flNoiseScaleLoc;

	for( ; nParticleCount--; start_p++ )
	{	
		const float *pxyz = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, start_p );
		const float *pCreationTime = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		const float *pLifespan = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_LIFE_DURATION, start_p );
		float *pAttr = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );		

		float ValueScale, ValueBase;

		Vector Coord, CoordLoc;
		CoordLoc.x = pxyz[0]; 
		CoordLoc.y = pxyz[4];
		CoordLoc.z = pxyz[8];
		CoordLoc += m_vecOffsetLoc;

		float Offset = m_flOffset;
		Coord = Vector ( (*pCreationTime + Offset), (*pCreationTime + Offset), (*pCreationTime + Offset) );
		Coord *= CoordScale;
		CoordLoc *= CoordScaleLoc;
		Coord += CoordLoc;

		fltx4 flNoise128;
		FourVectors fvNoise;

		fvNoise.DuplicateVector( Coord );
		flNoise128 = NoiseSIMD( fvNoise );
		float flNoise = SubFloat( flNoise128, 0 );

		*( (int *) &flNoise)  &= nAbsVal;

		ValueScale = ( flAbsScale *( fMax - fMin ) );
		ValueBase = ( fMin+ ( ( 1.0 - flAbsScale ) *( fMax - fMin ) ) );

		if ( m_bAbsValInv )
		{
			flNoise = 1.0 - flNoise;
		}

		float flInitialNoise = ( ValueBase + ( ValueScale * flNoise ) );


		flInitialNoise = clamp(flInitialNoise, 0.0f, 1.0f );
		flInitialNoise *= *pLifespan;

		*( pAttr ) = *pCreationTime - flInitialNoise;
	}
}




//-----------------------------------------------------------------------------
// LifeTime Sequence Length
//-----------------------------------------------------------------------------
class C_INIT_SequenceLifeTime : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_SequenceLifeTime );

	float m_flFramerate;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER_MASK;
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask, void *pContext ) const;

};

DEFINE_PARTICLE_OPERATOR( C_INIT_SequenceLifeTime, "Lifetime From Sequence", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_SequenceLifeTime ) 
DMXELEMENT_UNPACK_FIELD( "Frames Per Second", "30", float, m_flFramerate )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_SequenceLifeTime )

void C_INIT_SequenceLifeTime::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	if ( ( m_flFramerate != 0.0f ) && ( pParticles->m_Sheet() ) )
	{
		for( ; nParticleCount--; start_p++ )
		{
			const float *flSequence = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER, start_p );
			float *dtime = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, start_p );
			int nSequence = *flSequence;

			if ( pParticles->m_Sheet()->m_flFrameSpan[nSequence] != 0 )
			{
				*dtime = pParticles->m_Sheet()->m_flFrameSpan[nSequence] / m_flFramerate;
			}
			else
			{
				*dtime = 1.0;
			}
		}
	}
}




//-----------------------------------------------------------------------------
// Create In Hierarchy
//-----------------------------------------------------------------------------
class C_INIT_CreateInHierarchy : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_CreateInHierarchy );

	float m_fMaxDistance;
	float m_flGrowthTime;
	//float m_flTraceDist; 
	float m_flDesiredMidPoint;
	int m_nOrientation;
	float m_flBulgeFactor;
	int m_nDesiredEndPoint;
	int m_nDesiredStartPoint;
	bool m_bUseHighestEndCP;
	Vector m_vecDistanceBias, m_vecDistanceBiasAbs;
	bool m_bDistanceBias, m_bDistanceBiasAbs;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		uint64 nStartMask = ( 1ULL << m_nDesiredStartPoint ) - 1;
		uint64 nEndMask = m_bUseHighestEndCP ? 0xFFFFFFFFFFFFFFFFll : ( 1ULL << ( m_nDesiredEndPoint + 1 ) ) - 1;
		return nEndMask & (~nStartMask);
	}

	void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		//fixme - confirm CPs
		//		m_PathParams.ClampControlPointIndices();
		m_bDistanceBias = ( m_vecDistanceBias.x != 1.0f ) || ( m_vecDistanceBias.y != 1.0f ) || ( m_vecDistanceBias.z != 1.0f );
		m_bDistanceBiasAbs = ( m_vecDistanceBiasAbs.x != 0.0f ) || ( m_vecDistanceBiasAbs.y != 0.0f ) || ( m_vecDistanceBiasAbs.z != 0.0f );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;

};

DEFINE_PARTICLE_OPERATOR( C_INIT_CreateInHierarchy, "Position In CP Hierarchy", OPERATOR_PI_POSITION );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateInHierarchy ) 
	DMXELEMENT_UNPACK_FIELD( "maximum distance", "0", float, m_fMaxDistance )
	DMXELEMENT_UNPACK_FIELD( "bulge", "0", float, m_flBulgeFactor )
	DMXELEMENT_UNPACK_FIELD( "start control point number", "0", int, m_nDesiredStartPoint )
	DMXELEMENT_UNPACK_FIELD( "end control point number", "1", int, m_nDesiredEndPoint )
	DMXELEMENT_UNPACK_FIELD( "bulge control 0=random 1=orientation of start pnt 2=orientation of end point", "0", int, m_nOrientation )
	DMXELEMENT_UNPACK_FIELD( "mid point position", "0.5", float, m_flDesiredMidPoint )
	DMXELEMENT_UNPACK_FIELD( "growth time", "0.0", float, m_flGrowthTime )
	//DMXELEMENT_UNPACK_FIELD( "trace distance for optional culling", "0.0", float, m_flTraceDist )
	DMXELEMENT_UNPACK_FIELD( "use highest supplied end point", "0", bool, m_bUseHighestEndCP )
	DMXELEMENT_UNPACK_FIELD( "distance_bias", "1 1 1", Vector, m_vecDistanceBias )
	DMXELEMENT_UNPACK_FIELD( "distance_bias_absolute_value", "0 0 0", Vector, m_vecDistanceBiasAbs )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateInHierarchy )


void C_INIT_CreateInHierarchy::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	int nEndCP;
	float flGrowth;
	struct CPathParameters PathParams;
	PathParams.m_flBulge = m_flBulgeFactor;
	PathParams.m_nBulgeControl = m_nOrientation;
	PathParams.m_flMidPoint = m_flDesiredMidPoint;
	int nRealEndPoint;

	if ( m_bUseHighestEndCP )
	{
		nRealEndPoint = pParticles->GetHighestControlPoint();
	}
	else
	{
		nRealEndPoint = m_nDesiredEndPoint;
	}

	for( ; nParticleCount--; start_p++ )
	{
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );

		if ( ( pParticles->m_flCurTime <= m_flGrowthTime ) && ( nRealEndPoint > 0 ) )
		{
			float flCurrentEndCP = RemapValClamped( *ct, 0.0f, m_flGrowthTime, min( m_nDesiredStartPoint + 1, nRealEndPoint ), nRealEndPoint );
			nEndCP =  pParticles->RandomInt( min( m_nDesiredStartPoint + 1, (int)flCurrentEndCP ), flCurrentEndCP );

			// clamp growth to the appropriate values...
			float flEndTime = flCurrentEndCP / float(nRealEndPoint) ;
			flGrowth = RemapValClamped( *ct, 0.0f, m_flGrowthTime, 0.0, flEndTime );
		}
		else
		{
			int nLowestStartPoint =  min( m_nDesiredStartPoint + 1, nRealEndPoint );
			nEndCP =  pParticles->RandomInt( nLowestStartPoint, nRealEndPoint );
			flGrowth = 1.0;
		}


		PathParams.m_nStartControlPointNumber = pParticles->m_ControlPoints[nEndCP].m_nParent;
		PathParams.m_nEndControlPointNumber = nEndCP;
		Vector StartPnt, MidP, EndPnt;

		pParticles->CalculatePathValues( PathParams, *ct, &StartPnt, &MidP, &EndPnt);
		EndPnt *= flGrowth;

		float t=pParticles->RandomFloat( 0.0, 1.0 );
		
		Vector randpos;
		pParticles->RandomVector( -m_fMaxDistance, m_fMaxDistance, &randpos );

		if ( m_bDistanceBiasAbs	)
		{
			if ( m_vecDistanceBiasAbs.x	!= 0.0f )
			{
				randpos.x = fabs(randpos.x);
			}
			if ( m_vecDistanceBiasAbs.y	!= 0.0f )
			{
				randpos.y = fabs(randpos.y);
			}
			if ( m_vecDistanceBiasAbs.z	!= 0.0f )
			{
				randpos.z = fabs(randpos.z);
			}
		}
		randpos *= m_vecDistanceBias;

		// form delta terms needed for quadratic bezier
		Vector Delta0=MidP-StartPnt;
		Vector Delta1 = EndPnt-MidP;

		Vector L0 = StartPnt+t*Delta0;
		Vector L1 = MidP+t*Delta1;

		Vector Pnt = L0+(L1-L0)*t;

		Pnt+=randpos;
		// Optional Culling based on configurable trace distance.  Failing particle are destroyed
		//disabled for now.
		//if ( m_flTraceDist != 0.0f )
		//{
		//	// Trace down
		//	Vector TraceDir=Vector(0, 0, -1);
		//	// now set the trace distance
		//	// note - probably need to offset Pnt upwards for some fudge factor on irregular surfaces
		//	CBaseTrace tr;
		//	Vector RayStart=Pnt;
		//	float flRadius = m_flTraceDist;
		//	g_pParticleSystemMgr->Query()->TraceLine( RayStart, ( RayStart + ( TraceDir * flRadius ) ), MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );
		//	if ( tr.fraction == 1.0 )
		//	{
		//		//If the trace hit nothing, kill the particle.
		//		pParticles->KillParticle( start_p );
		//	}
		//	else
		//	{
		//		//If we hit something, set particle position to collision position
		//		Pnt += tr.endpos;
		//		//FIXME - if we add a concept of a particle normal (for example, aligned quads or decals, set it here)
		//	}
		//}

		xyz[0] = Pnt.x;
		xyz[4] = Pnt.y;
		xyz[8] = Pnt.z;
		if ( pxyz && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_PREV_XYZ_MASK ) )
		{
			pxyz[0] = Pnt.x;
			pxyz[4] = Pnt.y;
			pxyz[8] = Pnt.z;
		}
	}
}



//-----------------------------------------------------------------------------
// Remap initial Scalar to Vector Initializer
//-----------------------------------------------------------------------------
class C_INIT_RemapScalarToVector : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RemapScalarToVector );

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 1 << m_nFieldInput;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	int		m_nFieldInput;
	int		m_nFieldOutput;
	float	m_flInputMin;
	float	m_flInputMax;
	Vector	m_vecOutputMin;
	Vector	m_vecOutputMax;
	float	m_flStartTime;
	float	m_flEndTime;
	bool	m_bScaleInitialRange;
	int		m_nControlPointNumber;
	bool	m_bLocalCoords;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RemapScalarToVector, "Remap Scalar to Vector", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_RemapScalarToVector )
DMXELEMENT_UNPACK_FIELD( "emitter lifetime start time (seconds)", "-1", float, m_flStartTime )
DMXELEMENT_UNPACK_FIELD( "emitter lifetime end time (seconds)", "-1", float, m_flEndTime )
DMXELEMENT_UNPACK_FIELD_USERDATA( "input field", "8", int, m_nFieldInput, "intchoice particlefield_scalar" )
DMXELEMENT_UNPACK_FIELD( "input minimum","0", float, m_flInputMin )
DMXELEMENT_UNPACK_FIELD( "input maximum","1", float, m_flInputMax )
DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "0", int, m_nFieldOutput, "intchoice particlefield_vector" )
DMXELEMENT_UNPACK_FIELD( "output minimum","0 0 0", Vector, m_vecOutputMin )
DMXELEMENT_UNPACK_FIELD( "output maximum","1 1 1", Vector, m_vecOutputMax )
DMXELEMENT_UNPACK_FIELD( "output is scalar of initial random range","0", bool, m_bScaleInitialRange )
DMXELEMENT_UNPACK_FIELD( "use local system", "1", bool, m_bLocalCoords )
DMXELEMENT_UNPACK_FIELD( "control_point_number", "0", int, m_nControlPointNumber )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RemapScalarToVector )

void C_INIT_RemapScalarToVector::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	const float *pCreationTime;
	// FIXME: SSE-ize
	for( ; nParticleCount--; start_p++ )
	{
		pCreationTime = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		// using raw creation time to map to emitter lifespan
		float flLifeTime = *pCreationTime;  

		// only use within start/end time frame
		if ( ( ( flLifeTime < m_flStartTime ) || ( flLifeTime >= m_flEndTime ) ) && ( ( m_flStartTime != -1.0f) && ( m_flEndTime != -1.0f) ) )
			continue;

		const float *pInput = pParticles->GetFloatAttributePtr( m_nFieldInput, start_p );
		float *pOutput = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );
		Vector vecOutput = vec3_origin;
		vecOutput.x = RemapValClamped( *pInput, m_flInputMin, m_flInputMax, m_vecOutputMin.x, m_vecOutputMax.x  );
		vecOutput.y = RemapValClamped( *pInput, m_flInputMin, m_flInputMax, m_vecOutputMin.y, m_vecOutputMax.y  );
		vecOutput.z = RemapValClamped( *pInput, m_flInputMin, m_flInputMax, m_vecOutputMin.z, m_vecOutputMax.z  );


		if ( m_nFieldOutput == 0 )
		{
			float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
			if ( !m_bLocalCoords )
			{
				Vector vecControlPoint;
				pParticles->GetControlPointAtTime( m_nControlPointNumber, *pCreationTime, &vecControlPoint );
				vecOutput += vecControlPoint;
				Vector vecOutputPrev = vecOutput;
				if ( m_bScaleInitialRange )
				{
					Vector vecScaleInitial;
					Vector vecScaleInitialPrev;
					SetVectorFromAttribute ( vecScaleInitial, pOutput );
					SetVectorFromAttribute ( vecScaleInitialPrev, pxyz );
					vecOutput *= vecScaleInitial;
					vecOutputPrev *= vecScaleInitialPrev;
				}
				SetVectorAttribute( pOutput, vecOutput );
				SetVectorAttribute( pxyz, vecOutputPrev ); 
			}
			else
			{
				matrix3x4_t mat;
				pParticles->GetControlPointTransformAtTime( m_nControlPointNumber, *pCreationTime, &mat );
				Vector vecTransformLocal = vec3_origin;
				VectorTransform( vecOutput, mat, vecTransformLocal );
				vecOutput = vecTransformLocal;
				Vector vecOutputPrev = vecOutput;
				if ( m_bScaleInitialRange )
				{
					Vector vecScaleInitial;
					Vector vecScaleInitialPrev;
					SetVectorFromAttribute ( vecScaleInitial, pOutput );
					SetVectorFromAttribute ( vecScaleInitialPrev, pxyz );
					vecOutput *= vecScaleInitial;
					vecOutputPrev *= vecScaleInitialPrev;
				}
				SetVectorAttribute( pOutput, vecOutput );
				SetVectorAttribute( pxyz, vecOutput ); 
			}
		}
		else
		{
			if ( m_bScaleInitialRange )
			{
				Vector vecScaleInitial;
				SetVectorFromAttribute ( vecScaleInitial, pOutput );
				vecOutput *= vecScaleInitial;
			}
			SetVectorAttribute( pOutput, vecOutput ); 
		}
	}
}


//-----------------------------------------------------------------------------
// Create particles sequentially along a path
//-----------------------------------------------------------------------------
struct SequentialPathContext_t
{
	int		m_nParticleCount;
	float	m_flStep;
	int		m_nCountAmount;
};
class C_INIT_CreateSequentialPath : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_CreateSequentialPath );

	float m_fMaxDistance;
	float m_flNumToAssign;
	bool m_bLoop;
	struct CPathParameters m_PathParams;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		uint64 nStartMask = ( 1ULL << m_PathParams.m_nStartControlPointNumber ) - 1;
		uint64 nEndMask = ( 1ULL << ( m_PathParams.m_nEndControlPointNumber + 1 ) ) - 1;
		return nEndMask & (~nStartMask);
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		SequentialPathContext_t *pCtx = reinterpret_cast<SequentialPathContext_t *>( pContext );
		pCtx->m_nParticleCount = 0;
		if ( m_flNumToAssign > 1.0f )
		{
			pCtx->m_flStep = 1.0f / ( m_flNumToAssign - 1 );
		}
		else
		{
			pCtx->m_flStep = 0.0f;
		}
		pCtx->m_nCountAmount = 1;
	}

	void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		m_PathParams.ClampControlPointIndices();


	}

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( SequentialPathContext_t );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

};

DEFINE_PARTICLE_OPERATOR( C_INIT_CreateSequentialPath, "Position Along Path Sequential", OPERATOR_PI_POSITION );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateSequentialPath ) 
DMXELEMENT_UNPACK_FIELD( "maximum distance", "0", float, m_fMaxDistance )
DMXELEMENT_UNPACK_FIELD( "bulge", "0", float, m_PathParams.m_flBulge )
DMXELEMENT_UNPACK_FIELD( "start control point number", "0", int, m_PathParams.m_nStartControlPointNumber )
DMXELEMENT_UNPACK_FIELD( "end control point number", "0", int, m_PathParams.m_nEndControlPointNumber )
DMXELEMENT_UNPACK_FIELD( "bulge control 0=random 1=orientation of start pnt 2=orientation of end point", "0", int, m_PathParams.m_nBulgeControl )
DMXELEMENT_UNPACK_FIELD( "mid point position", "0.5", float, m_PathParams.m_flMidPoint )
DMXELEMENT_UNPACK_FIELD( "particles to map from start to end", "100", float, m_flNumToAssign )
DMXELEMENT_UNPACK_FIELD( "restart behavior (0 = bounce, 1 = loop )", "1", bool, m_bLoop )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateSequentialPath )


void C_INIT_CreateSequentialPath::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	// NOTE: Using C_OP_ContinuousEmitter:: avoids a virtual function call
	SequentialPathContext_t *pCtx = reinterpret_cast<SequentialPathContext_t *>( pContext );

	for( ; nParticleCount--; start_p++ )
	{
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );

		Vector StartPnt, MidP, EndPnt;
		pParticles->CalculatePathValues( m_PathParams, *ct, &StartPnt, &MidP, &EndPnt);
		if ( pCtx->m_nParticleCount >= m_flNumToAssign || pCtx->m_nParticleCount < 0 )
		{
			if ( m_bLoop )
			{
				pCtx->m_nParticleCount = 0;
			}
			else
			{
				pCtx->m_nCountAmount *= -1;
				pCtx->m_nParticleCount = min ( pCtx->m_nParticleCount, (int)( m_flNumToAssign - 1) );
				pCtx->m_nParticleCount = max ( pCtx->m_nParticleCount, 1 );
			}
		}

		float t= pCtx->m_nParticleCount * pCtx->m_flStep;

		Vector randpos;
		pParticles->RandomVector( -m_fMaxDistance, m_fMaxDistance, &randpos );

		// form delta terms needed for quadratic bezier
		Vector Delta0=MidP-StartPnt;
		Vector Delta1 = EndPnt-MidP;

		Vector L0 = StartPnt+t*Delta0;
		Vector L1 = MidP+t*Delta1;

		Vector Pnt = L0+(L1-L0)*t;

		Pnt+=randpos;

		xyz[0] = Pnt.x;
		xyz[4] = Pnt.y;
		xyz[8] = Pnt.z;
		if ( pxyz && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_PREV_XYZ_MASK ) )
		{
			pxyz[0] = Pnt.x;
			pxyz[4] = Pnt.y;
			pxyz[8] = Pnt.z;
		}
		pCtx->m_nParticleCount += pCtx->m_nCountAmount;
	}
}


//-----------------------------------------------------------------------------
//   Initial Repulsion Velocity - repulses the particles from nearby surfaces 
//	 on spawn
//-----------------------------------------------------------------------------
class C_INIT_InitialRepulsionVelocity : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_InitialRepulsionVelocity );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK | PARTICLE_ATTRIBUTE_RADIUS_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;	

	void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		m_nCollisionGroupNumber = g_pParticleSystemMgr->Query()->GetCollisionGroupFromName( m_CollisionGroupName );
		m_nControlPointNumber = max( 0, min( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
	}

	bool InitMultipleOverride ( void ) { return true; }

	char m_CollisionGroupName[128];
	int m_nCollisionGroupNumber;
	Vector	m_vecOutputMin;
	Vector	m_vecOutputMax;
	int		nRemainingBlocks;
	int		m_nControlPointNumber;
	bool	m_bPerParticle;
	bool	m_bTranslate;
	bool	m_bProportional;
	float	m_flTraceLength;
	bool	m_bPerParticleTR;
	bool	m_bInherit;
	int		m_nChildCP;
	int		m_nChildGroupID;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_InitialRepulsionVelocity, "Velocity Repulse from World", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_InitialRepulsionVelocity )
DMXELEMENT_UNPACK_FIELD( "minimum velocity","0 0 0", Vector, m_vecOutputMin )
DMXELEMENT_UNPACK_FIELD( "maximum velocity","1 1 1", Vector, m_vecOutputMax )
DMXELEMENT_UNPACK_FIELD_STRING( "collision group", "NONE", m_CollisionGroupName )
DMXELEMENT_UNPACK_FIELD( "control_point_number", "0", int, m_nControlPointNumber )
DMXELEMENT_UNPACK_FIELD( "Per Particle World Collision Tests", "0", bool, m_bPerParticle )
DMXELEMENT_UNPACK_FIELD( "Use radius for Per Particle Trace Length", "0", bool, m_bPerParticleTR )
DMXELEMENT_UNPACK_FIELD( "Offset instead of accelerate", "0", bool, m_bTranslate )
DMXELEMENT_UNPACK_FIELD( "Offset proportional to radius 0/1", "0", bool, m_bProportional )
DMXELEMENT_UNPACK_FIELD( "Trace Length", "64.0", float, m_flTraceLength )
DMXELEMENT_UNPACK_FIELD( "Inherit from Parent", "0", bool, m_bInherit )
DMXELEMENT_UNPACK_FIELD( "control points to broadcast to children (n + 1)", "-1", int, m_nChildCP )
DMXELEMENT_UNPACK_FIELD( "Child Group ID to affect", "0", int, m_nChildGroupID )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_InitialRepulsionVelocity );


void C_INIT_InitialRepulsionVelocity::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{

	Vector	d[6];

	//All cardinal directions
	d[0] = Vector(  1,  0,  0 );
	d[1] = Vector( -1,  0,  0 );
	d[2] = Vector(  0,  1,  0 );
	d[3] = Vector(  0, -1,  0 );
	d[4] = Vector(  0,  0,  1 );
	d[5] = Vector(  0,  0, -1 );

	//Init the results
	Vector resultDirection;
	float resultForce;
	if ( m_bPerParticle )
	{
		for( ; nParticleCount--; start_p++ )
		{	

			float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
			float *pxyz_prev = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
			const float *radius = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_RADIUS, start_p );
			Vector vecCurrentPos;
			SetVectorFromAttribute( vecCurrentPos, pxyz );

			resultDirection.Init();
			resultForce = 0.0f;

			//Get the aggregate force vector
			for ( int i = 0; i < 6; i++ )
			{
				//Press out
				float flTraceDistance = m_flTraceLength;
				if ( m_bPerParticleTR )
				{
					flTraceDistance = *radius;
				}
				Vector endpos = vecCurrentPos + ( d[i] * flTraceDistance );

				//Trace into the world
				CBaseTrace tr;
				g_pParticleSystemMgr->Query()->TraceLine( vecCurrentPos, endpos, CONTENTS_SOLID, NULL, m_nCollisionGroupNumber, &tr );

				//Push back a proportional amount to the probe
				d[i] = -d[i] * (1.0f-tr.fraction);

				assert(( 1.0f - tr.fraction ) >= 0.0f );

				resultForce += 1.0f-tr.fraction;
				resultDirection += d[i];
			}

			//If we've hit nothing, then point up
			if ( resultDirection == vec3_origin )
			{
				resultDirection = Vector( 0, 0, 1 );
				resultForce = 0.0f;
			}

			//Just return the direction
			VectorNormalize( resultDirection );
			resultDirection *= resultForce;

			Vector vecRepulsionAmount;

			vecRepulsionAmount.x = Lerp( resultForce, m_vecOutputMin.x, m_vecOutputMax.x );
			vecRepulsionAmount.y = Lerp( resultForce, m_vecOutputMin.y, m_vecOutputMax.y );
			vecRepulsionAmount.z = Lerp( resultForce, m_vecOutputMin.z, m_vecOutputMax.z );


			vecRepulsionAmount *= resultDirection;


			if ( m_bProportional )
			{
				vecRepulsionAmount *= *radius;
			}

			pxyz[0] += vecRepulsionAmount.x;
			pxyz[4] += vecRepulsionAmount.y;
			pxyz[8] += vecRepulsionAmount.z;

			if ( m_bTranslate )
			{
				pxyz_prev[0] += vecRepulsionAmount.x;
				pxyz_prev[4] += vecRepulsionAmount.y;
				pxyz_prev[8] += vecRepulsionAmount.z;
			}
		}
	}
	else
	{
		
		Vector vecRepulsionAmount;

		if ( m_bInherit )
		{
			float *ct = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
			pParticles->GetControlPointAtTime( m_nControlPointNumber, *ct, &resultDirection );
			Vector vecPassedForce;
			pParticles->GetControlPointAtTime( m_nControlPointNumber+1, *ct, &vecPassedForce );

			vecRepulsionAmount.x = Lerp( vecPassedForce.x, m_vecOutputMin.x, m_vecOutputMax.x );
			vecRepulsionAmount.y = Lerp( vecPassedForce.x, m_vecOutputMin.y, m_vecOutputMax.y );
			vecRepulsionAmount.z = Lerp( vecPassedForce.x, m_vecOutputMin.z, m_vecOutputMax.z );

			vecRepulsionAmount *= resultDirection;
		}
		else
		{
			float *ct = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
			Vector vecControlPoint;
			pParticles->GetControlPointAtTime( m_nControlPointNumber, *ct, &vecControlPoint );

			Vector vecCurrentPos = vecControlPoint;

			resultDirection.Init();
			resultForce = 0.0f;

			//Get the aggregate force vector
			for ( int i = 0; i < 6; i++ )
			{
				//Press out
				Vector endpos = vecCurrentPos + ( d[i] * m_flTraceLength );

				//Trace into the world
				CBaseTrace tr;
				g_pParticleSystemMgr->Query()->TraceLine( vecCurrentPos, endpos, CONTENTS_SOLID, NULL, m_nCollisionGroupNumber, &tr );

				//Push back a proportional amount to the probe
				d[i] = -d[i] * (1.0f-tr.fraction);

				assert(( 1.0f - tr.fraction ) >= 0.0f );

				resultForce += 1.0f-tr.fraction;
				resultDirection += d[i];
			}

			//If we've hit nothing, then point up
			if ( resultDirection == vec3_origin )
			{
				resultDirection = Vector( 0, 0, 1 );
				resultForce = 0.0f;
			}

			//Just return the direction
			VectorNormalize( resultDirection );
			resultDirection *= resultForce;

			vecRepulsionAmount.x = Lerp( resultForce, m_vecOutputMin.x, m_vecOutputMax.x );
			vecRepulsionAmount.y = Lerp( resultForce, m_vecOutputMin.y, m_vecOutputMax.y );
			vecRepulsionAmount.z = Lerp( resultForce, m_vecOutputMin.z, m_vecOutputMax.z );

			vecRepulsionAmount *= resultDirection;

			if ( m_nChildCP != -1 )
			{
				for( CParticleCollection *pChild = pParticles->m_Children.m_pHead; pChild; pChild = pChild->m_pNext )
				{
					if ( pChild->GetGroupID() == m_nChildGroupID )
					{
						Vector vecPassForce = Vector(resultForce, 0, 0);
						pChild->SetControlPoint( m_nChildCP, resultDirection );
						pChild->SetControlPoint( m_nChildCP+1, vecPassForce );
					}
				}
			}
		}
		
		for( ; nParticleCount--; start_p++ )
		{	

			float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
			float *pxyz_prev = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
			const float *radius = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_RADIUS, start_p );

			if ( m_bProportional )
			{
				vecRepulsionAmount *= *radius;
			}

			pxyz[0] += vecRepulsionAmount.x;
			pxyz[4] += vecRepulsionAmount.y;
			pxyz[8] += vecRepulsionAmount.z;

			if ( m_bTranslate )
			{
				pxyz_prev[0] += vecRepulsionAmount.x;
				pxyz_prev[4] += vecRepulsionAmount.y;
				pxyz_prev[8] += vecRepulsionAmount.z;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Random Yaw Flip
//-----------------------------------------------------------------------------
class C_INIT_RandomYawFlip : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomYawFlip );
	
	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_YAW_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask, void *pContext ) const;

	float m_flPercent;

};

DEFINE_PARTICLE_OPERATOR( C_INIT_RandomYawFlip, "Rotation Yaw Flip Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomYawFlip ) 
DMXELEMENT_UNPACK_FIELD( "Flip Percentage", ".5", float, m_flPercent )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomYawFlip )

void C_INIT_RandomYawFlip::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	for( ; nParticleCount--; start_p++ )
	{
		float flChance = pParticles->RandomFloat( 0.0, 1.0 );
		if ( flChance < m_flPercent )
		{
			float flRadians = 180 * ( M_PI / 180.0f );
			float *drot = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_YAW, start_p );
			*drot += flRadians;
		}
	}
}



//-----------------------------------------------------------------------------
// Random second sequence
//-----------------------------------------------------------------------------
class C_INIT_RandomSecondSequence : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomSecondSequence );
 
	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER1_MASK;
	}
 
	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}
 
	virtual void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		// TODO: Validate the ranges here!
	}

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const
	{
		InitScalarAttributeRandomRangeBlock( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER1,
			m_nSequenceMin, m_nSequenceMax,
			pParticles, start_block, n_blocks );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p, int nParticleCount, int nAttributeWriteMask, void *pContext ) const
	{
		float *pSequence;
		for( ; nParticleCount--; start_p++ )
		{
			pSequence = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER1, start_p );
			*pSequence = pParticles->RandomInt( m_nSequenceMin, m_nSequenceMax );
		}
	}
 
	int m_nSequenceMin;
	int m_nSequenceMax;
};
 
DEFINE_PARTICLE_OPERATOR( C_INIT_RandomSecondSequence, "Sequence Two Random", OPERATOR_GENERIC );
 
BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomSecondSequence ) 
	DMXELEMENT_UNPACK_FIELD( "sequence_min", "0", int, m_nSequenceMin )
	DMXELEMENT_UNPACK_FIELD( "sequence_max", "0", int, m_nSequenceMax )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomSecondSequence )



//-----------------------------------------------------------------------------
// Remap CP to Scalar Initializer
//-----------------------------------------------------------------------------
class C_INIT_RemapCPtoScalar : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RemapCPtoScalar );

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nCPInput;
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	virtual void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		m_nField = int (clamp (m_nField, 0, 2));
	}

	int		m_nCPInput;                                                             
	int		m_nFieldOutput;
	int		m_nField;
	float	m_flInputMin;
	float	m_flInputMax;
	float	m_flOutputMin;
	float	m_flOutputMax;
	float	m_flStartTime;
	float	m_flEndTime;
	bool	m_bScaleInitialRange;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RemapCPtoScalar, "Remap Control Point to Scalar", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_RemapCPtoScalar )
DMXELEMENT_UNPACK_FIELD( "emitter lifetime start time (seconds)", "-1", float, m_flStartTime )
DMXELEMENT_UNPACK_FIELD( "emitter lifetime end time (seconds)", "-1", float, m_flEndTime )
DMXELEMENT_UNPACK_FIELD( "input control point number", "0", int, m_nCPInput )
DMXELEMENT_UNPACK_FIELD( "input minimum","0", float, m_flInputMin )
DMXELEMENT_UNPACK_FIELD( "input maximum","1", float, m_flInputMax )
DMXELEMENT_UNPACK_FIELD( "input field 0-2 X/Y/Z","0", int, m_nField )
DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "3", int, m_nFieldOutput, "intchoice particlefield_scalar" )
DMXELEMENT_UNPACK_FIELD( "output minimum","0", float, m_flOutputMin )
DMXELEMENT_UNPACK_FIELD( "output maximum","1", float, m_flOutputMax )
DMXELEMENT_UNPACK_FIELD( "output is scalar of initial random range","0", bool, m_bScaleInitialRange )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RemapCPtoScalar )

void C_INIT_RemapCPtoScalar::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	const float *pCreationTime;
	// clamp the result to 0 and 1 if it's alpha
	float flMin=m_flOutputMin;
	float flMax=m_flOutputMax;
	if ( ATTRIBUTES_WHICH_ARE_0_TO_1 & ( 1 << m_nFieldOutput ) )
	{
		flMin = clamp(m_flOutputMin, 0.0f, 1.0f );
		flMax = clamp(m_flOutputMax, 0.0f, 1.0f );
	}
	Vector vecControlPoint;
	float *ct = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
	pParticles->GetControlPointAtTime( m_nCPInput, *ct, &vecControlPoint );

	float flInput = vecControlPoint[m_nField];

	// FIXME: SSE-ize
	for( ; nParticleCount--; start_p++ )
	{
		pCreationTime = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		// using raw creation time to map to emitter lifespan
		float flLifeTime = *pCreationTime;  

		// only use within start/end time frame
		if ( ( ( flLifeTime < m_flStartTime ) || ( flLifeTime >= m_flEndTime ) ) && ( ( m_flStartTime != -1.0f) && ( m_flEndTime != -1.0f) ) )
			continue;


		float *pOutput = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );
		float flOutput = RemapValClamped( flInput, m_flInputMin, m_flInputMax, flMin, flMax  );
		if ( m_bScaleInitialRange )
		{
			flOutput = *pOutput * flOutput;
		}
		if ( ATTRIBUTES_WHICH_ARE_INTS & ( 1 << m_nFieldOutput ) )
		{
			*pOutput = int ( flOutput );
		}
		else
		{
			*pOutput = flOutput;
		}
	}
}



//-----------------------------------------------------------------------------
// Remap CP to Vector Initializer
//-----------------------------------------------------------------------------
class C_INIT_RemapCPtoVector : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RemapCPtoVector );

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		uint64 nMask = ( 1ULL << m_nCPInput );
		if ( m_nLocalSpaceCP != -1 )
		{
			nMask |= ( 1ULL << m_nLocalSpaceCP );
		}
		return nMask;
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	virtual void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		m_nField = int (clamp (m_nField, 0, 2));
	}

	int		m_nCPInput;                                                             
	int		m_nFieldOutput;
	int		m_nField;
	Vector	m_vInputMin;
	Vector	m_vInputMax;
	Vector	m_vOutputMin;
	Vector	m_vOutputMax;
	float	m_flStartTime;
	float	m_flEndTime;
	bool	m_bScaleInitialRange;
	bool	m_bOffset;
	bool	m_bAccelerate;
	int		m_nLocalSpaceCP;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RemapCPtoVector, "Remap Control Point to Vector", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_RemapCPtoVector )
DMXELEMENT_UNPACK_FIELD( "emitter lifetime start time (seconds)", "-1", float, m_flStartTime )
DMXELEMENT_UNPACK_FIELD( "emitter lifetime end time (seconds)", "-1", float, m_flEndTime )
DMXELEMENT_UNPACK_FIELD( "input control point number", "0", int, m_nCPInput )
DMXELEMENT_UNPACK_FIELD( "input minimum","0 0 0", Vector, m_vInputMin )
DMXELEMENT_UNPACK_FIELD( "input maximum","0 0 0", Vector, m_vInputMax )
DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "0", int, m_nFieldOutput, "intchoice particlefield_vector" )
DMXELEMENT_UNPACK_FIELD( "output minimum","0 0 0", Vector, m_vOutputMin )
DMXELEMENT_UNPACK_FIELD( "output maximum","0 0 0", Vector, m_vOutputMax )
DMXELEMENT_UNPACK_FIELD( "output is scalar of initial random range","0", bool, m_bScaleInitialRange )
DMXELEMENT_UNPACK_FIELD( "offset position","0", bool, m_bOffset )
DMXELEMENT_UNPACK_FIELD( "accelerate position","0", bool, m_bAccelerate )
DMXELEMENT_UNPACK_FIELD( "local space CP","-1", int, m_nLocalSpaceCP )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RemapCPtoVector )

void C_INIT_RemapCPtoVector::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	Vector vecControlPoint;
	pParticles->GetControlPointAtTime( m_nCPInput, pParticles->m_flCurTime, &vecControlPoint );
	Vector vOutputMinLocal = m_vOutputMin;
	Vector vOutputMaxLocal = m_vOutputMax;
	if ( m_nLocalSpaceCP != -1 )
	{
		matrix3x4_t mat;
		pParticles->GetControlPointTransformAtTime( m_nLocalSpaceCP, pParticles->m_flCurTime, &mat );
		Vector vecTransformLocal = vec3_origin;
		VectorRotate( vOutputMinLocal, mat, vecTransformLocal );
		vOutputMinLocal = vecTransformLocal;
		VectorRotate( vOutputMaxLocal, mat, vecTransformLocal );
		vOutputMaxLocal = vecTransformLocal;
	}

	// FIXME: SSE-ize
	for( ; nParticleCount--; start_p++ )
	{
		const float *pCreationTime = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		// using raw creation time to map to emitter lifespan
		float flLifeTime = *pCreationTime;  

		// only use within start/end time frame
		if ( ( ( flLifeTime < m_flStartTime ) || ( flLifeTime >= m_flEndTime ) ) && ( ( m_flStartTime != -1.0f) && ( m_flEndTime != -1.0f) ) )
			continue;


		float *pOutput = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );

		Vector vOutput;
		vOutput.x = RemapValClamped( vecControlPoint.x, m_vInputMin.x, m_vInputMax.x, vOutputMinLocal.x, vOutputMaxLocal.x );
		vOutput.y = RemapValClamped( vecControlPoint.y, m_vInputMin.y, m_vInputMax.y, vOutputMinLocal.y, vOutputMaxLocal.y );
		vOutput.z = RemapValClamped( vecControlPoint.z, m_vInputMin.z, m_vInputMax.z, vOutputMinLocal.z, vOutputMaxLocal.z );		

		if ( m_bScaleInitialRange )
		{
			Vector vOrgValue;
			SetVectorFromAttribute ( vOrgValue, pOutput );
			vOutput *= vOrgValue;
		}
		if ( m_nFieldOutput == 6 )
		{
			pOutput[0] = max( 0.0f, min( vOutput.x, 1.0f) );
			pOutput[4] = max( 0.0f, min( vOutput.y, 1.0f) );
			pOutput[8] = max( 0.0f, min( vOutput.z, 1.0f) );
		}
		else
		{
			float *pXYZ_Prev = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
			Vector vXYZPrev;
			if ( m_bAccelerate )
			{
				if ( m_bOffset )
				{
					Vector vOrgValue;
					SetVectorFromAttribute ( vOrgValue, pOutput );
					SetVectorFromAttribute ( vXYZPrev, pXYZ_Prev );
					vOutput += vOrgValue;
					vXYZPrev += vOutput;
					vOutput += vOutput * pParticles->m_flDt;
					SetVectorAttribute ( pOutput, vOutput );
					SetVectorAttribute ( pXYZ_Prev, vXYZPrev );
				}
				else
				{
					vOutput *= pParticles->m_flDt;
					SetVectorAttribute ( pOutput, vOutput );
				}

			}
			else
			{
				vXYZPrev = vOutput;
				if ( m_bOffset )
				{
					Vector vOrgValue;
					SetVectorFromAttribute ( vOrgValue, pOutput );
					SetVectorFromAttribute ( vXYZPrev, pXYZ_Prev );
					vOutput += vOrgValue;
					vXYZPrev += vOutput;

				}
				SetVectorAttribute ( pOutput, vOutput );
				SetVectorAttribute ( pXYZ_Prev, vXYZPrev );
			}
		}
	}
}



class C_INIT_CreateFromParentParticles : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_CreateFromParentParticles );

	struct ParentParticlesContext_t
	{
		int		m_nCurrentParentParticle;
	};

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		ParentParticlesContext_t *pCtx = reinterpret_cast<ParentParticlesContext_t *>( pContext );
		pCtx->m_nCurrentParentParticle = 0;
	}

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( ParentParticlesContext_t );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	float m_flVelocityScale;
	bool  m_bRandomDistribution;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_CreateFromParentParticles, "Position From Parent Particles", OPERATOR_PI_POSITION );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateFromParentParticles )
DMXELEMENT_UNPACK_FIELD( "Inherited Velocity Scale","0", float, m_flVelocityScale )
DMXELEMENT_UNPACK_FIELD( "Random Parent Particle Distribution","0", bool, m_bRandomDistribution )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateFromParentParticles )

void C_INIT_CreateFromParentParticles::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	if ( !pParticles->m_pParent )
	{
		for( ; nParticleCount--; start_p++ )
		{
			float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
			float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );

			SetVectorAttribute( xyz, vec3_origin );
			SetVectorAttribute( pxyz, vec3_origin );
		}
		return;
	}
	ParentParticlesContext_t *pCtx = reinterpret_cast<ParentParticlesContext_t *>( pContext );
	int nActiveParticles = pParticles->m_pParent->m_nActiveParticles;


	if ( nActiveParticles == 0 )
	{
		while( nParticleCount-- )
		{
			float *lifespan = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, start_p );
			*lifespan = 0.0f;
			start_p++;
		}
		return;
	}		

	nActiveParticles = max ( 0, nActiveParticles - 1 );

	for( ; nParticleCount--; start_p++ )
	{
		if ( m_bRandomDistribution )
		{
			pCtx->m_nCurrentParentParticle = pParticles->RandomInt( 0, nActiveParticles );
		}
		else if ( pCtx->m_nCurrentParentParticle > nActiveParticles )
		{
			pCtx->m_nCurrentParentParticle = 0;
		}
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		const float *pParent_xyz = pParticles->m_pParent->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, pCtx->m_nCurrentParentParticle );
		const float *pParent_pxyz = pParticles->m_pParent->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_PREV_XYZ, pCtx->m_nCurrentParentParticle );

		Vector vecParentXYZ;
		Vector vecParentPrevXYZ;
		Vector vecScaledXYZ;

		float flPrevTime = pParticles->m_flCurTime - pParticles->m_flDt;
		float flSubFrame = RemapValClamped( *ct, flPrevTime, pParticles->m_flCurTime, 0, 1 );
		

		vecParentXYZ.x = pParent_xyz[0];
		vecParentXYZ.y = pParent_xyz[4];
		vecParentXYZ.z = pParent_xyz[8];
		vecParentPrevXYZ.x = pParent_pxyz[0];
		vecParentPrevXYZ.y = pParent_pxyz[4];
		vecParentPrevXYZ.z = pParent_pxyz[8];

		VectorLerp( vecParentPrevXYZ, vecParentXYZ, flSubFrame, vecParentXYZ );
		VectorLerp( vecParentXYZ, vecParentPrevXYZ, m_flVelocityScale, vecScaledXYZ );
		SetVectorAttribute( pxyz, vecScaledXYZ );
		SetVectorAttribute( xyz, vecParentXYZ );

		pCtx->m_nCurrentParentParticle++;
	}
}




//-----------------------------------------------------------------------------
// Distance to CP Initializer
//-----------------------------------------------------------------------------
class C_INIT_DistanceToCPInit : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_DistanceToCPInit );

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nStartCP;
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		m_nCollisionGroupNumber = g_pParticleSystemMgr->Query()->GetCollisionGroupFromName( m_CollisionGroupName );
		m_nStartCP = max( 0, min( MAX_PARTICLE_CONTROL_POINTS-1, m_nStartCP ) );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	int		m_nFieldOutput;
	float	m_flInputMin;
	float	m_flInputMax;
	float	m_flOutputMin;
	float	m_flOutputMax;
	int		m_nStartCP;
	bool	m_bLOS;
	char	m_CollisionGroupName[128];
	int		m_nCollisionGroupNumber;
	float	m_flMaxTraceLength;
	float	m_flLOSScale;
	bool	m_bScaleInitialRange;
	bool	m_bActiveRange;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_DistanceToCPInit, "Remap Initial Distance to Control Point to Scalar", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_DistanceToCPInit )
DMXELEMENT_UNPACK_FIELD( "distance minimum","0", float, m_flInputMin )
DMXELEMENT_UNPACK_FIELD( "distance maximum","128", float, m_flInputMax )
DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "3", int, m_nFieldOutput, "intchoice particlefield_scalar" )
DMXELEMENT_UNPACK_FIELD( "output minimum","0", float, m_flOutputMin )
DMXELEMENT_UNPACK_FIELD( "output maximum","1", float, m_flOutputMax )
DMXELEMENT_UNPACK_FIELD( "control point","0", int, m_nStartCP )
DMXELEMENT_UNPACK_FIELD( "ensure line of sight","0", bool, m_bLOS )
DMXELEMENT_UNPACK_FIELD_STRING( "LOS collision group", "NONE", m_CollisionGroupName )
DMXELEMENT_UNPACK_FIELD( "Maximum Trace Length", "-1", float, m_flMaxTraceLength )
DMXELEMENT_UNPACK_FIELD( "LOS Failure Scalar", "0", float, m_flLOSScale )
DMXELEMENT_UNPACK_FIELD( "output is scalar of initial random range","0", bool, m_bScaleInitialRange )
DMXELEMENT_UNPACK_FIELD( "only active within specified distance","0", bool, m_bActiveRange )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_DistanceToCPInit )

void C_INIT_DistanceToCPInit::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	// clamp the result to 0 and 1 if it's alpha
	float flMin=m_flOutputMin;
	float flMax=m_flOutputMax;
	if ( ATTRIBUTES_WHICH_ARE_0_TO_1 & ( 1 << m_nFieldOutput ) )
	{
		flMin = clamp(m_flOutputMin, 0.0f, 1.0f );
		flMax = clamp(m_flOutputMax, 0.0f, 1.0f );
	}
	Vector vecControlPoint1 = pParticles->GetControlPointAtCurrentTime( m_nStartCP );

	// FIXME: SSE-ize
	for( ; nParticleCount--; start_p++ )
	{
		Vector vecPosition2;
		const float *pXYZ = pParticles->GetFloatAttributePtr(PARTICLE_ATTRIBUTE_XYZ, start_p );
		vecPosition2 = Vector(pXYZ[0], pXYZ[4], pXYZ[8]); 
		Vector vecDelta = vecControlPoint1 - vecPosition2;
		float flDistance = vecDelta.Length();
		if ( m_bActiveRange && ( flDistance < m_flInputMin || flDistance > m_flInputMax ) )
		{
			continue;
		}
		if ( m_bLOS )
		{
			Vector vecEndPoint = vecPosition2;
			if ( m_flMaxTraceLength != -1.0f && m_flMaxTraceLength < flDistance )
			{
				VectorNormalize(vecEndPoint);
				vecEndPoint *= m_flMaxTraceLength;
				vecEndPoint += vecControlPoint1;
			}
			CBaseTrace tr;
			g_pParticleSystemMgr->Query()->TraceLine( vecControlPoint1, vecEndPoint, MASK_OPAQUE_AND_NPCS, NULL , m_nCollisionGroupNumber, &tr );
			if (tr.fraction != 1.0f)
			{
				flDistance *= tr.fraction * m_flLOSScale;
			}

		}

		float flOutput = RemapValClamped( flDistance, m_flInputMin, m_flInputMax, flMin, flMax  );
		if ( m_bScaleInitialRange )
		{
			const float *pInitialOutput = pParticles->GetFloatAttributePtr( m_nFieldOutput, start_p );
			flOutput = *pInitialOutput * flOutput;
		}
		float *pOutput = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );

		*pOutput = flOutput;
	}
}




class C_INIT_LifespanFromVelocity : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_LifespanFromVelocity );

	Vector m_vecComponentScale;
	float m_flTraceOffset;
	float m_flMaxTraceLength;
	float m_flTraceTolerance;
	int m_nCollisionGroupNumber;
	int m_nMaxPlanes;
	int m_nAllowedPlanes;
	char	m_CollisionGroupName[128];
	

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	void InitializeContextData( CParticleCollection *pParticles,
		void *pContext ) const
	{
	}

	size_t GetRequiredContextBytes( ) const
	{
		return sizeof( CWorldCollideContextData );
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		m_nCollisionGroupNumber = g_pParticleSystemMgr->Query()->GetCollisionGroupFromName( m_CollisionGroupName );
		m_nAllowedPlanes = ( min ( MAX_WORLD_PLANAR_CONSTRAINTS, m_nMaxPlanes ) - 1 );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const;

};

DEFINE_PARTICLE_OPERATOR( C_INIT_LifespanFromVelocity, "Lifetime from Time to Impact", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_LifespanFromVelocity )
DMXELEMENT_UNPACK_FIELD_STRING( "trace collision group", "NONE", m_CollisionGroupName )
DMXELEMENT_UNPACK_FIELD( "maximum trace length", "1024", float, m_flMaxTraceLength )
DMXELEMENT_UNPACK_FIELD( "trace offset", "0", float, m_flTraceOffset )
DMXELEMENT_UNPACK_FIELD( "trace recycle tolerance", "64", float, m_flTraceTolerance )
DMXELEMENT_UNPACK_FIELD( "maximum points to cache", "16", int, m_nMaxPlanes )
DMXELEMENT_UNPACK_FIELD( "bias distance", "1 1 1", Vector, m_vecComponentScale )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_LifespanFromVelocity )


void C_INIT_LifespanFromVelocity::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	CWorldCollideContextData **ppCtx;
	if ( pParticles->m_pParent )
		ppCtx = &( pParticles->m_pParent->m_pCollisionCacheData[COLLISION_MODE_INITIAL_TRACE_DOWN] );
	else
		ppCtx = &( pParticles->m_pCollisionCacheData[COLLISION_MODE_INITIAL_TRACE_DOWN] );

	CWorldCollideContextData *pCtx = NULL;
	if ( ! *ppCtx )
	{
		*ppCtx = new CWorldCollideContextData;
		(*ppCtx)->m_nActivePlanes = 0;
		(*ppCtx)->m_nActivePlanes = 0;
		(*ppCtx)->m_nNumFixedPlanes = 0;
	}
	pCtx = *ppCtx;

	float flTol = m_flTraceTolerance * m_flTraceTolerance;

	//Trace length takes the max trace and subtracts the offset to get the actual total.
	float flTotalTraceDist = m_flMaxTraceLength - m_flTraceOffset;

	//Offset percentage to account for if we've hit something within the offset (but not spawn) area
	float flOffsetPct = m_flMaxTraceLength / ( flTotalTraceDist + FLT_EPSILON );

	FourVectors v4ComponentScale;
	v4ComponentScale.DuplicateVector( m_vecComponentScale );
	while( nParticleCount-- )
	{
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		float *pPrevXYZ = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );

		float *dtime = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, start_p );


		Vector vecXYZ( pxyz[0], pxyz[4], pxyz[8] );
		Vector vecXYZ_Prev( pPrevXYZ[0], pPrevXYZ[4], pPrevXYZ[8] );

		//Calculate velocity and account for frame delta time
		Vector vDelta = vecXYZ - vecXYZ_Prev;
		float flVelocity = VectorLength( vDelta );
		flVelocity /= pParticles->m_flPreviousDt;
		
		fltx4 fl4TraceOffset = ReplicateX4( m_flTraceOffset );

		//Normalize the delta and get the offset to use from the normalized delta times the offset
		VectorNormalize( vDelta );
		Vector vecOffset = vDelta * m_flTraceOffset;

		Vector vecStartPnt = vecXYZ + vecOffset;
		Vector vecEndPnt = ( vDelta * flTotalTraceDist ) + vecStartPnt;

		// Use SIMD section to interface with plane cache, even though we're not SIMD here
		// Test versus existing Data
		FourVectors fvStartPnt;
		fvStartPnt.DuplicateVector( vecStartPnt );
		FourVectors fvEndPnt;
		fvEndPnt.DuplicateVector( vecEndPnt );
		FourVectors v4PointOnPlane;
		FourVectors v4PlaneNormal;
		FourVectors v4Delta;
		fltx4 fl4ClosestDist = Four_FLT_MAX;
		for( int i = 0 ; i < pCtx->m_nActivePlanes; i++ )
		{
			if ( pCtx->m_bPlaneActive[i] )
			{
				fltx4 fl4TrialDistance = MaxSIMD( 
					fvStartPnt.DistSqrToLineSegment( pCtx->m_TraceStartPnt[i], pCtx->m_TraceEndPnt[i] ),
					fvEndPnt.DistSqrToLineSegment( pCtx->m_TraceStartPnt[i], pCtx->m_TraceEndPnt[i] ) );
				// If the trial distance is closer than the existing closest, replace.
				if ( !IsAllGreaterThan( fl4TrialDistance, fl4ClosestDist ) )
				{
					fl4ClosestDist = fl4TrialDistance;
					v4PointOnPlane = pCtx->m_PointOnPlane[i];
				}
			}
		}
		fl4ClosestDist = fabs( fl4ClosestDist );
		// If we're outside the tolerance range, do a new trace and store it.
		if ( IsAllGreaterThan( fl4ClosestDist, ReplicateX4( flTol ) ) )
		{
			//replace this with fast raycaster when available
			CBaseTrace tr;
			tr.plane.normal = vec3_invalid;
			g_pParticleSystemMgr->Query()->TraceLine( vecStartPnt, vecEndPnt, CONTENTS_SOLID, NULL , m_nCollisionGroupNumber, &tr );

			//Set the lifespan to 0 if we start solid, our trace distance is 0, or we hit within the offset area
			if ( ( tr.fraction < ( 1 - flOffsetPct ) ) || tr.startsolid || flTotalTraceDist == 0.0f )
			{
				*dtime = 0.0f;
				fl4TraceOffset = ReplicateX4( 0.0f );
				fvStartPnt.DuplicateVector( vec3_origin );
				v4PointOnPlane.DuplicateVector( vec3_origin );
			}
			else
			{
				int nIndex = pCtx->m_nNumFixedPlanes;
				Vector vPointOnPlane =  vecStartPnt + ( tr.fraction * ( vecEndPnt - vecStartPnt ) ) ;
				pCtx->m_bPlaneActive[nIndex] = true;
				pCtx->m_PointOnPlane[nIndex].DuplicateVector( vPointOnPlane );
				pCtx->m_PlaneNormal[nIndex].DuplicateVector( tr.plane.normal );
				pCtx->m_TraceStartPnt[nIndex].DuplicateVector( vecStartPnt );
				pCtx->m_TraceEndPnt[nIndex].DuplicateVector( vecEndPnt );

				fvStartPnt.DuplicateVector( vecStartPnt );
				v4PointOnPlane.DuplicateVector( vPointOnPlane );

				pCtx->m_nNumFixedPlanes = pCtx->m_nNumFixedPlanes + 1;
				if ( pCtx->m_nNumFixedPlanes > m_nAllowedPlanes )
					pCtx->m_nNumFixedPlanes = 0;
				pCtx->m_nActivePlanes = min( m_nAllowedPlanes, pCtx->m_nActivePlanes + 1 );
			}
		}

		fvStartPnt -= v4PointOnPlane;
		//Scale components to remove undesired axis
		fvStartPnt *= v4ComponentScale;
		//Find the length of the trace
		//Need to use the adjusted value of the trace length and collision point to account for the offset
		fltx4 fl4Dist = AddSIMD ( fvStartPnt.length(), fl4TraceOffset );
		flVelocity += FLT_EPSILON;
		//Divide by Velocity to get Lifespan
		*dtime = SubFloat( fl4Dist, 0) / flVelocity;

	}
}


void C_INIT_LifespanFromVelocity::InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const
{
		CWorldCollideContextData **ppCtx;
		if ( pParticles->m_pParent )
			ppCtx = &( pParticles->m_pParent->m_pCollisionCacheData[COLLISION_MODE_INITIAL_TRACE_DOWN] );
		else
			ppCtx = &( pParticles->m_pCollisionCacheData[COLLISION_MODE_INITIAL_TRACE_DOWN] );

		CWorldCollideContextData *pCtx = NULL;
		if ( ! *ppCtx )
		{
			*ppCtx = new CWorldCollideContextData;
			(*ppCtx)->m_nActivePlanes = 0;
			(*ppCtx)->m_nActivePlanes = 0;
			(*ppCtx)->m_nNumFixedPlanes = 0;
		}
		pCtx = *ppCtx;

		float flTol = m_flTraceTolerance * m_flTraceTolerance;

		size_t attr_stride;

		FourVectors *pXYZ = pParticles->Get4VAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, &attr_stride );
		pXYZ += attr_stride * start_block;
		FourVectors *pPrev_XYZ = pParticles->Get4VAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, &attr_stride );
		pPrev_XYZ += attr_stride * start_block;
		fltx4 *pLifespan = pParticles->GetM128AttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, &attr_stride );
		pLifespan += attr_stride * start_block;

		//Trace length takes the max trace and subtracts the offset to get the actual total.
		float flTotalTraceDist = m_flMaxTraceLength - m_flTraceOffset;
		fltx4 fl4TotalTraceDist = ReplicateX4( flTotalTraceDist );

		//Offset percentage to account for if we've hit something within the offset (but not spawn) area
		float flOffsetPct = m_flMaxTraceLength / ( flTotalTraceDist + FLT_EPSILON );

		fltx4 fl4PrevDT = ReplicateX4( 1.0f / pParticles->m_flPreviousDt );

		FourVectors v4ComponentScale;
		v4ComponentScale.DuplicateVector( m_vecComponentScale );

		while( n_blocks-- )
		{
			// Determine Velocity
			FourVectors fvDelta = *pXYZ;
			fvDelta -= *pPrev_XYZ;
			fltx4 fl4Velocity = fvDelta.length();
			fl4Velocity = MulSIMD ( fl4Velocity, fl4PrevDT );

			fltx4 fl4TraceOffset = ReplicateX4( m_flTraceOffset );

			//Normalize the delta and get the offset to use from the normalized delta times the offset
			FourVectors fvDeltaNormalized = fvDelta;
			fvDeltaNormalized.VectorNormalizeFast();
			FourVectors fvOffset = fvDeltaNormalized;
			fvOffset *= m_flTraceOffset;

			//Start/Endpoints for our traces
			FourVectors fvStartPnt = *pXYZ;
			fvStartPnt += fvOffset;
			FourVectors fvEndPnt = fvDeltaNormalized;
			fvEndPnt *= fl4TotalTraceDist;
			fvEndPnt += fvStartPnt;

			// Test versus existing Data
			FourVectors v4PointOnPlane;
			FourVectors v4PlaneNormal;
			fltx4 fl4ClosestDist = Four_FLT_MAX;
			for( int i = 0 ; i < pCtx->m_nActivePlanes; i++ )
			{
				if ( pCtx->m_bPlaneActive[i] )
				{
					fltx4 fl4TrialDistance = MaxSIMD( 
						fvStartPnt.DistSqrToLineSegment( pCtx->m_TraceStartPnt[i], pCtx->m_TraceEndPnt[i] ),
						fvEndPnt.DistSqrToLineSegment( pCtx->m_TraceStartPnt[i], pCtx->m_TraceEndPnt[i] ) );
					fltx4 fl4Nearestmask = CmpLeSIMD( fl4TrialDistance, fl4ClosestDist );
					fl4ClosestDist = MaskedAssign( fl4ClosestDist, fl4TrialDistance, fl4Nearestmask );
					v4PointOnPlane.x = MaskedAssign( fl4Nearestmask, pCtx->m_PointOnPlane[i].x, v4PointOnPlane.x );
					v4PointOnPlane.y = MaskedAssign( fl4Nearestmask, pCtx->m_PointOnPlane[i].y, v4PointOnPlane.y );
					v4PointOnPlane.z = MaskedAssign( fl4Nearestmask, pCtx->m_PointOnPlane[i].z, v4PointOnPlane.z );
				}
			}
			
			// If we're outside the tolerance range, do a new trace and store it.
			fltx4 fl4OutOfRange = CmpGtSIMD( fl4ClosestDist, ReplicateX4( flTol ) );
			if ( IsAnyNegative( fl4OutOfRange ) )
			{
				int nMask = TestSignSIMD( fl4OutOfRange );
				for(int i=0; i < 4; i++ )
				{
					if ( nMask & ( 1 << i ) )
					{
						Vector start = fvStartPnt.Vec( i );
						Vector end = fvEndPnt.Vec( i );

						//replace this with fast raycaster when available
						CBaseTrace tr;
						tr.plane.normal = vec3_invalid;
						g_pParticleSystemMgr->Query()->TraceLine( start, end, CONTENTS_SOLID, NULL , m_nCollisionGroupNumber, &tr );

						//Set the lifespan to 0 if we start solid, our trace distance is 0, or we hit within the offset area
						if ( ( tr.fraction < ( 1 - flOffsetPct )  ) || tr.startsolid || flTotalTraceDist == 0.0f )
						{
							SubFloat( fvStartPnt.x, i ) = 0.0f;
							SubFloat( fvStartPnt.y, i ) = 0.0f;
							SubFloat( fvStartPnt.z, i ) = 0.0f;
							SubFloat( v4PointOnPlane.x, i ) = 0.0f;
							SubFloat( v4PointOnPlane.y, i ) = 0.0f;
							SubFloat( v4PointOnPlane.z, i ) = 0.0f;
							SubFloat( fl4TraceOffset, i ) = 0.0f;
						}
						else
						{
							int nIndex = pCtx->m_nNumFixedPlanes;
							Vector vPointOnPlane =  start + ( tr.fraction * ( end - start ) ) ;
							SubFloat( v4PointOnPlane.x, i ) = vPointOnPlane.x;
							SubFloat( v4PointOnPlane.y, i ) = vPointOnPlane.y;
							SubFloat( v4PointOnPlane.z, i ) = vPointOnPlane.z;
							pCtx->m_bPlaneActive[nIndex] = true;
							pCtx->m_PointOnPlane[nIndex].DuplicateVector( vPointOnPlane );
							pCtx->m_PlaneNormal[nIndex].DuplicateVector( tr.plane.normal );
							pCtx->m_TraceStartPnt[nIndex].DuplicateVector( start );
							pCtx->m_TraceEndPnt[nIndex].DuplicateVector( end );
							pCtx->m_nNumFixedPlanes = pCtx->m_nNumFixedPlanes + 1;
							if ( pCtx->m_nNumFixedPlanes > m_nAllowedPlanes )
								pCtx->m_nNumFixedPlanes = 0;
							pCtx->m_nActivePlanes = min( m_nAllowedPlanes, pCtx->m_nActivePlanes + 1 );
						}
					}
				}
			}

			//Find the length of the trace
			fvStartPnt -= v4PointOnPlane;
			fvStartPnt *= v4ComponentScale;
			//Need to use the adjusted value of the trace length and collision point to account for the offset
			fltx4 fl4Dist = AddSIMD ( fvStartPnt.length(), fl4TraceOffset );
			fl4Velocity = AddSIMD( fl4Velocity, Four_Epsilons );
			//Divide by Velocity to get Lifespan
			*pLifespan = DivSIMD( fl4Dist, fl4Velocity );

			pXYZ += attr_stride;
			pPrev_XYZ += attr_stride;
			pLifespan += attr_stride;
		}
}





class C_INIT_CreateFromPlaneCache : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_CreateFromPlaneCache );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	size_t GetRequiredContextBytes( ) const
	{
		return sizeof( CWorldCollideContextData );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_CreateFromPlaneCache, "Position from Parent Cache", OPERATOR_PI_POSITION );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateFromPlaneCache )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateFromPlaneCache )

void C_INIT_CreateFromPlaneCache::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	if ( !pParticles->m_pParent )
	{
		for( ; nParticleCount--; start_p++ )
		{
			float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
			float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );

			SetVectorAttribute( xyz, vec3_origin );
			SetVectorAttribute( pxyz, vec3_origin );
		}
		return;
	}


	CWorldCollideContextData **ppCtx;
	if ( pParticles->m_pParent )
		ppCtx = &( pParticles->m_pParent->m_pCollisionCacheData[COLLISION_MODE_INITIAL_TRACE_DOWN] );
	else
		ppCtx = &( pParticles->m_pCollisionCacheData[COLLISION_MODE_INITIAL_TRACE_DOWN] );

	CWorldCollideContextData *pCtx = NULL;
	if ( ! *ppCtx )
	{
		*ppCtx = new CWorldCollideContextData;
		(*ppCtx)->m_nActivePlanes = 0;
		(*ppCtx)->m_nNumFixedPlanes = 0;
		FourVectors fvEmpty;
		fvEmpty.DuplicateVector( vec3_origin );
		(*ppCtx)->m_PointOnPlane[0] = fvEmpty;
	}
	pCtx = *ppCtx;
	if ( pCtx->m_nActivePlanes > 0 )
	{
		for( ; nParticleCount--; start_p++ )
		{ 
			int nIndex = pParticles->RandomInt( 0, pCtx->m_nActivePlanes - 1 );
			if ( pCtx->m_PlaneNormal[nIndex].Vec( 0 ) == vec3_invalid )
			{
				float *plifespan = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, start_p );
				*plifespan = 0.0f;
			}
			else
			{
				float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
				float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
				FourVectors fvPoint = pCtx->m_PointOnPlane[nIndex];
				Vector vPoint = fvPoint.Vec( 0 );
				SetVectorAttribute( xyz, vPoint );
				SetVectorAttribute( pxyz, vPoint );
			}
		}
	}
	else
	{
		for( ; nParticleCount--; start_p++ )
		{ 
			float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
			float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
			SetVectorAttribute( xyz, vec3_origin );
			SetVectorAttribute( pxyz, vec3_origin );
		}
	}
}





//
//
//
//
 
//-----------------------------------------------------------------------------
// Purpose: Add all operators to be considered active, here
//-----------------------------------------------------------------------------
void AddBuiltInParticleInitializers( void )
{
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_CreateAlongPath );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_MoveBetweenPoints );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_CreateWithinSphere );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_VelocityRandom );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_CreateOnModel );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_CreateWithinBox );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomRotationSpeed );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomLifeTime );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomAlpha );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomRadius );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomRotation );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomYaw );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomColor );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomTrailLength );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomSequence );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_PositionOffset );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_PositionWarp );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_CreationNoise );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_InitialVelocityNoise );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RemapScalar );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_InheritVelocity );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_AgeNoise ); 
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_SequenceLifeTime ); 
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_CreateInHierarchy );  
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RemapScalarToVector );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_CreateSequentialPath );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_InitialRepulsionVelocity );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomYawFlip );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomSecondSequence );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RemapCPtoScalar );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RemapCPtoVector );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_CreateFromParentParticles );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_DistanceToCPInit );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_LifespanFromVelocity );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_CreateFromPlaneCache );
}

