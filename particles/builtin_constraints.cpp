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
#include "tier1/UtlStringMap.h"
#include "tier1/strtools.h"
#include "mathlib/halton.h"
#include "bspflags.h"
#include "const.h"
#include "particles_internal.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


class C_OP_ConstrainDistance : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_ConstrainDistance );


	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK;
	}


	bool EnforceConstraint( int nStartBlock,
							int nEndBlock,
							CParticleCollection *pParticles,
							void *pContext,
							int nNumValidParticlesInLastChunk ) const;

	float m_fMinDistance, m_fMaxDistance;
	int m_nControlPointNumber;
	Vector m_CenterOffset;
	bool m_bGlobalCenter;

};

#ifdef NDEBUG
#define CHECKSYSTEM( p ) 0
#else
static void CHECKSYSTEM( CParticleCollection *pParticles )
{
//	Assert( pParticles->m_nActiveParticles <= pParticles->m_pDef->m_nMaxParticles );
	for ( int i = 0; i < pParticles->m_nActiveParticles; ++i )
	{
		const float *xyz = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, i );
		const float *xyz_prev = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_PREV_XYZ, i );
		Assert( IsFinite( xyz[0] ) );
		Assert( IsFinite( xyz[4] ) );
		Assert( IsFinite( xyz[8] ) );
		Assert( IsFinite( xyz_prev[0] ) );
		Assert( IsFinite( xyz_prev[4] ) );
		Assert( IsFinite( xyz_prev[8] ) );
	}
}
#endif

bool C_OP_ConstrainDistance::EnforceConstraint( int nStartBlock,
												int nNumBlocks,
												CParticleCollection *pParticles,
												void *pContext, int nNumValidParticlesInLastChunk ) const
{
	size_t nStride;
	FourVectors *pXYZ=pParticles->Get4VAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ,
															 &nStride );
	pXYZ += nStride * nStartBlock;
	fltx4 SIMDMinDist=ReplicateX4( m_fMinDistance );
	fltx4 SIMDMaxDist=ReplicateX4( m_fMaxDistance );
	fltx4 SIMDMinDist2=ReplicateX4( m_fMinDistance*m_fMinDistance );
	fltx4 SIMDMaxDist2=ReplicateX4( m_fMaxDistance*m_fMaxDistance );

	Vector vecCenter;
	if ( m_bGlobalCenter )
		vecCenter = m_CenterOffset;
	else
	{
		pParticles->GetControlPointAtTime( m_nControlPointNumber, pParticles->m_flCurTime, &vecCenter );
		vecCenter += pParticles->TransformAxis( m_CenterOffset, true, m_nControlPointNumber );
	}
	FourVectors Center;
	Center.DuplicateVector( vecCenter );
	
	bool bChangedSomething = false;
	do
	{
		FourVectors pts = *(pXYZ);
		pts -= Center;
		fltx4 dist_squared= pts * pts;
		fltx4 TooFarMask = CmpGtSIMD( dist_squared, SIMDMaxDist2 );
		fltx4 TooCloseMask = CmpLtSIMD( dist_squared, SIMDMinDist2 );
		fltx4 NeedAdjust = OrSIMD( TooFarMask, TooCloseMask );
		if ( IsAnyNegative( NeedAdjust ) )				// any out of bounds?
		{
			// change squared distance into approximate rsqr root
			fltx4 guess = ReciprocalSqrtEstSaturateSIMD(dist_squared);
			// newton iteration for 1/sqrt(x) : y(n+1)=1/2 (y(n)*(3-x*y(n)^2));
			guess=MulSIMD(guess,SubSIMD(Four_Threes,MulSIMD(dist_squared,MulSIMD(guess,guess))));
			guess=MulSIMD(Four_PointFives,guess);
			pts *= guess;

			FourVectors clamp_far=pts;
			clamp_far *= SIMDMaxDist;
			clamp_far += Center;
			FourVectors clamp_near=pts;
			clamp_near *= SIMDMinDist;
			clamp_near += Center;
			pts.x = MaskedAssign( TooCloseMask, clamp_near.x, MaskedAssign( TooFarMask, clamp_far.x, pXYZ->x ));
			pts.y = MaskedAssign( TooCloseMask, clamp_near.y, MaskedAssign( TooFarMask, clamp_far.y, pXYZ->y ));
			pts.z = MaskedAssign( TooCloseMask, clamp_near.z, MaskedAssign( TooFarMask, clamp_far.z, pXYZ->z ));
			*(pXYZ) = pts;
			bChangedSomething = true;
		}
		pXYZ += nStride;
	} while (--nNumBlocks);
	return bChangedSomething;
}

DEFINE_PARTICLE_OPERATOR( C_OP_ConstrainDistance, "Constrain distance to control point", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_OP_ConstrainDistance ) 
	DMXELEMENT_UNPACK_FIELD( "minimum distance", "0", float, m_fMinDistance )
	DMXELEMENT_UNPACK_FIELD( "maximum distance", "100", float, m_fMaxDistance )
	DMXELEMENT_UNPACK_FIELD( "control point number", "0", int, m_nControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "offset of center", "0 0 0", Vector, m_CenterOffset )
	DMXELEMENT_UNPACK_FIELD( "global center point", "0", bool, m_bGlobalCenter )
END_PARTICLE_OPERATOR_UNPACK( C_OP_ConstrainDistance )

class C_OP_ConstrainDistanceToPath : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_ConstrainDistanceToPath );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const 
	{ 
		return ( 1ULL << m_PathParameters.m_nStartControlPointNumber ) |
			( 1ULL << m_PathParameters.m_nEndControlPointNumber ); 
	}

	bool EnforceConstraint( int nStartBlock,
							int nEndBlock,
							CParticleCollection *pParticles,
							void *pContext, int nNumValidParticlesInLastChunk ) const;

	float m_fMinDistance;

	float m_flMaxDistance0, m_flMaxDistanceMid, m_flMaxDistance1;
	CPathParameters m_PathParameters;

	float m_flTravelTime;

};

bool C_OP_ConstrainDistanceToPath::EnforceConstraint( int nStartBlock,
													  int nNumBlocks,
													  CParticleCollection *pParticles,
													  void *pContext,
													  int nNumValidParticlesInLastChunk ) const
{
	C4VAttributeWriteIterator pXYZ( PARTICLE_ATTRIBUTE_XYZ, pParticles );
	pXYZ += nStartBlock;

	CM128AttributeIterator pCreationTime( PARTICLE_ATTRIBUTE_CREATION_TIME, pParticles );
	pCreationTime += nStartBlock;


	Vector StartPnt, EndPnt, MidP;

	pParticles->CalculatePathValues( m_PathParameters, pParticles->m_flCurTime,
									 &StartPnt, &MidP, &EndPnt );

	fltx4 CurTime = ReplicateX4( pParticles->m_flCurTime );
	fltx4 TimeScale= ReplicateX4( 1.0/(max(0.001f,  m_flTravelTime ) ) );

	// calculate radius spline
	bool bConstantRadius = true;
	fltx4 Rad0=ReplicateX4(m_flMaxDistance0);
	fltx4 Radm=Rad0;

	if ( m_flMaxDistanceMid >= 0.0 )
	{
		bConstantRadius = ( m_flMaxDistanceMid == m_flMaxDistance0 );
		Radm=ReplicateX4( m_flMaxDistanceMid);
	}
	fltx4 Rad1=Radm;
	if ( m_flMaxDistance1 >= 0.0 )
	{
		bConstantRadius &= ( m_flMaxDistance1 == m_flMaxDistance0 );
		Rad1=ReplicateX4( m_flMaxDistance1 );
	}
	
	fltx4 RadmMinusRad0=SubSIMD( Radm, Rad0);
	fltx4 Rad1MinusRadm=SubSIMD( Rad1, Radm);
	
	fltx4 SIMDMinDist=ReplicateX4( m_fMinDistance );
	fltx4 SIMDMinDist2=ReplicateX4( m_fMinDistance*m_fMinDistance );

	fltx4 SIMDMaxDist=MaxSIMD( Rad0, MaxSIMD( Radm, Rad1 ) );
	fltx4 SIMDMaxDist2=MulSIMD( SIMDMaxDist, SIMDMaxDist);

	bool bChangedSomething = false;
	FourVectors StartP;
	StartP.DuplicateVector( StartPnt );
		
	FourVectors MiddleP;
	MiddleP.DuplicateVector( MidP );

	// form delta terms needed for quadratic bezier
	FourVectors Delta0;
	Delta0.DuplicateVector( MidP-StartPnt );

	FourVectors Delta1;
	Delta1.DuplicateVector( EndPnt-MidP );
	do
	{
		fltx4 TScale=MinSIMD(
			Four_Ones,
			MulSIMD( TimeScale, SubSIMD( CurTime, *pCreationTime ) ) );

		// bezier(a,b,c,t)=lerp( lerp(a,b,t),lerp(b,c,t),t)
		FourVectors L0 = Delta0;
		L0 *= TScale;
		L0 += StartP;

		FourVectors L1= Delta1;
		L1 *= TScale;
		L1 += MiddleP;

		FourVectors Center = L1;
		Center -= L0;
		Center *= TScale;
		Center += L0;

		FourVectors pts = *(pXYZ);
		pts -= Center;

		// calculate radius at the point. !!speed!! - use speical case for constant radius

		fltx4 dist_squared= pts * pts;
		fltx4 TooFarMask = CmpGtSIMD( dist_squared, SIMDMaxDist2 );
		if ( ( !bConstantRadius) && ( ! IsAnyNegative( TooFarMask ) ) )
		{
			// need to calculate and adjust for true radius =- we've only trivilally rejected note
			// voodoo here - we update simdmaxdist for true radius, but not max dist^2, since
			// that's used only for the trivial reject case, which we've already done
			fltx4 R0=AddSIMD( Rad0, MulSIMD( RadmMinusRad0, TScale ) );
			fltx4 R1=AddSIMD( Radm, MulSIMD( Rad1MinusRadm, TScale ) );
			SIMDMaxDist = AddSIMD( R0, MulSIMD( SubSIMD( R1, R0 ), TScale) );
			
			// now that we know the true radius, update our mask
			TooFarMask = CmpGtSIMD( dist_squared, MulSIMD( SIMDMaxDist, SIMDMaxDist ) );
		}

		fltx4 TooCloseMask = CmpLtSIMD( dist_squared, SIMDMinDist2 );
		fltx4 NeedAdjust = OrSIMD( TooFarMask, TooCloseMask );
		if ( IsAnyNegative( NeedAdjust ) )				// any out of bounds?
		{
			if ( ! bConstantRadius )
			{
				// need to calculate and adjust for true radius =- we've only trivilally rejected

			}
			
			// change squared distance into approximate rsqr root
			fltx4 guess=ReciprocalSqrtEstSIMD(dist_squared);
			// newton iteration for 1/sqrt(x) : y(n+1)=1/2 (y(n)*(3-x*y(n)^2));
			guess=MulSIMD(guess,SubSIMD(Four_Threes,MulSIMD(dist_squared,MulSIMD(guess,guess))));
			guess=MulSIMD(Four_PointFives,guess);
			pts *= guess;
			
			FourVectors clamp_far=pts;
			clamp_far *= SIMDMaxDist;
			clamp_far += Center;
			FourVectors clamp_near=pts;
			clamp_near *= SIMDMinDist;
			clamp_near += Center;
			pts.x = MaskedAssign( TooCloseMask, clamp_near.x, MaskedAssign( TooFarMask, clamp_far.x, pXYZ->x ));
			pts.y = MaskedAssign( TooCloseMask, clamp_near.y, MaskedAssign( TooFarMask, clamp_far.y, pXYZ->y ));
			pts.z = MaskedAssign( TooCloseMask, clamp_near.z, MaskedAssign( TooFarMask, clamp_far.z, pXYZ->z ));
			*(pXYZ) = pts;
			bChangedSomething = true;
		}
		++pXYZ;
		++pCreationTime;
	} while (--nNumBlocks);
	return bChangedSomething;
}

DEFINE_PARTICLE_OPERATOR( C_OP_ConstrainDistanceToPath, "Constrain distance to path between two control points", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_OP_ConstrainDistanceToPath ) 
	DMXELEMENT_UNPACK_FIELD( "minimum distance", "0", float, m_fMinDistance )
	DMXELEMENT_UNPACK_FIELD( "maximum distance", "100", float, m_flMaxDistance0 )
	DMXELEMENT_UNPACK_FIELD( "maximum distance middle", "-1", float, m_flMaxDistanceMid )
	DMXELEMENT_UNPACK_FIELD( "maximum distance end", "-1", float, m_flMaxDistance1 )
	DMXELEMENT_UNPACK_FIELD( "travel time", "10", float, m_flTravelTime )
	DMXELEMENT_UNPACK_FIELD( "random bulge", "0", float, m_PathParameters.m_flBulge )
	DMXELEMENT_UNPACK_FIELD( "start control point number", "0", int, m_PathParameters.m_nStartControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "end control point number", "0", int, m_PathParameters.m_nEndControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "bulge control 0=random 1=orientation of start pnt 2=orientation of end point", "0", int, m_PathParameters.m_nBulgeControl )
	DMXELEMENT_UNPACK_FIELD( "mid point position", "0.5", float, m_PathParameters.m_flMidPoint )
END_PARTICLE_OPERATOR_UNPACK( C_OP_ConstrainDistanceToPath )



class C_OP_PlanarConstraint : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_PlanarConstraint );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_RADIUS_MASK;
	}

	virtual uint64 GetReadControlPointMask() const 
	{ 
		return 1ULL << m_nControlPointNumber; 
	}

	bool EnforceConstraint( int nStartBlock,
							int nEndBlock,
							CParticleCollection *pParticles,
							void *pContext, int nNumValidParticlesInLastChunk ) const;

	Vector m_PointOnPlane;
	Vector m_PlaneNormal;
	int m_nControlPointNumber;
	bool m_bGlobalOrigin;
	bool m_bGlobalNormal;

};

bool C_OP_PlanarConstraint::EnforceConstraint( int nStartBlock,
											   int nNumBlocks,
											   CParticleCollection *pParticles,
											   void *pContext,
											   int nNumValidParticlesInLastChunk ) const
{
	C4VAttributeWriteIterator pXYZ( PARTICLE_ATTRIBUTE_XYZ, pParticles );
	pXYZ += nStartBlock;

	CM128AttributeIterator pRadius( PARTICLE_ATTRIBUTE_RADIUS, pParticles );
	pRadius += nStartBlock;

	// now, transform and offset parameters
	FourVectors PlaneNormal;
	PlaneNormal.DuplicateVector( 
		pParticles->TransformAxis( m_PlaneNormal, ! m_bGlobalNormal, m_nControlPointNumber ) );
	PlaneNormal.VectorNormalize();

	FourVectors PlanePoint;
	if ( m_bGlobalOrigin )
	{
		PlanePoint.DuplicateVector( m_PointOnPlane );
	}
	else
	{
		Vector ofs=pParticles->TransformAxis( m_PointOnPlane, true, m_nControlPointNumber );
		Vector vecCenter;
		pParticles->GetControlPointAtTime( m_nControlPointNumber, 
										   pParticles->m_flCurTime, &vecCenter );
		PlanePoint.DuplicateVector( ofs + vecCenter );
	}


	bool bChangedSomething = false;
	do
	{
		FourVectors pts = *pXYZ;
		pts -= PlanePoint;
		fltx4 PlaneEq=pts * PlaneNormal;
		// where planeeq<0, inside
		PlaneEq = SubSIMD( PlaneEq, *pRadius );
		fltx4 BadPts=CmpLtSIMD( PlaneEq, Four_Zeros );
		if ( IsAnyNegative( BadPts ) )
		{
			bChangedSomething = true;
			// project points to plane surface
			fltx4 PenetrationDistance=MinSIMD( Four_Zeros, PlaneEq );
			FourVectors PenetrationVector = PlaneNormal;
			PenetrationVector *= PenetrationDistance;
			(*pXYZ) -= PenetrationVector;
		}
		++pXYZ;
		++pRadius;
	} while (--nNumBlocks);
	return bChangedSomething;
}

DEFINE_PARTICLE_OPERATOR( C_OP_PlanarConstraint, "Prevent passing through a plane", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_OP_PlanarConstraint ) 
	DMXELEMENT_UNPACK_FIELD( "control point number", "0", int, m_nControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "plane point", "0 0 0", Vector, m_PointOnPlane )
	DMXELEMENT_UNPACK_FIELD( "plane normal", "0 0 1", Vector, m_PlaneNormal )
	DMXELEMENT_UNPACK_FIELD( "global origin", "0", bool, m_bGlobalOrigin )
	DMXELEMENT_UNPACK_FIELD( "global normal", "0", bool, m_bGlobalNormal )
END_PARTICLE_OPERATOR_UNPACK( C_OP_PlanarConstraint )




static Vector s_OrientationRelativeTraceVectors[] = {
	Vector( 0, .1962, .784929 ),
	Vector( -.1962, 0, .784929 ),
	Vector( .1962, 0, .784929 ),
	Vector( 0, -.1962, .78929 ),
};

void CWorldCollideContextData::SetBaseTrace(  int nIndex, Vector const &rayStart, Vector const &traceDir, int nCollisionGroup, bool bKeepMisses )
{
	CBaseTrace tr;
	Vector rayEnd = rayStart + traceDir;
	g_pParticleSystemMgr->Query()->TraceLine( rayStart, rayEnd, MASK_SOLID, NULL, nCollisionGroup, &tr );
	if ( tr.fraction < 1.0 )
	{
		m_bPlaneActive[nIndex] = true;
		m_PointOnPlane[nIndex].DuplicateVector( rayStart + tr.fraction * traceDir );
		m_PlaneNormal[nIndex].DuplicateVector( tr.plane.normal );
		m_TraceStartPnt[nIndex].DuplicateVector( rayStart );
		m_TraceEndPnt[nIndex].DuplicateVector( rayEnd );
	}
	else
	{
		if ( bKeepMisses )
		{
			m_PlaneNormal[nIndex].x = Four_Zeros;
			m_PlaneNormal[nIndex].y = Four_Zeros;
			m_PlaneNormal[nIndex].z = Four_Zeros;
			m_TraceStartPnt[nIndex].DuplicateVector( rayStart );
			m_TraceEndPnt[nIndex].DuplicateVector( rayEnd );
			m_bPlaneActive[nIndex] = true;
		}
		else
			m_bPlaneActive[nIndex] = false;
	}
}

void CWorldCollideContextData::CalculatePlanes( CParticleCollection *pParticles, int nCollisionMode,
												int nCollisionGroup, Vector const *pCPOffset, 
												float flDistanceTolerance )
{
	// fire some rays to find the convex around the control point
	if ( m_nActivePlanes && ( nCollisionMode == COLLISION_MODE_INITIAL_TRACE_DOWN ) )
		return;
	Vector rayStart = pParticles->GetControlPointAtCurrentTime( 0 ); // allow config + offset

	if ( pCPOffset )
		rayStart += *pCPOffset;

	if ( ( m_flLastUpdateTime > 0. ) && ( ( rayStart - m_vecLastUpdateOrigin ).LengthSqr() < Square( flDistanceTolerance ) ) )
		return;

	m_vecLastUpdateOrigin = rayStart;
	m_nActivePlanes = 0;
	switch( nCollisionMode )
	{
		case COLLISION_MODE_INITIAL_TRACE_DOWN:
		{
			SetBaseTrace( 0, rayStart, 1000.0 * Vector( -1, 0, 0 ), nCollisionGroup, false );
			m_nActivePlanes = 1;
			m_nNumFixedPlanes = 1;
			break;
		}
		case COLLISION_MODE_PER_FRAME_PLANESET:
		{
			int nIndexOut = 0;
			for( int i = -1; i <= 1; i++ )
				for( int j = -1; j <= 1; j++ )
					for( int k = -1; k <= 1; k++ )
					{
						if ( i || j || k )
						{
							SetBaseTrace( nIndexOut++, rayStart, 1000.0 * Vector( i, j, k ), nCollisionGroup, false );
						}
					}
			m_nNumFixedPlanes = nIndexOut;
			m_nActivePlanes = nIndexOut;
		}
		// Long missing break. Added to Source2 in change 700053.
		// It's a bug, but changing it now could cause regressions, so
		// leaving it for now until someone decides it's worth fixing.
#ifdef FP_EXCEPTIONS_ENABLED
		// This break is necessary when exceptions are enabled because otherwise
		// m_bPlaneActive[21] is set even though that plane is filled with
		// NaNs. We should perhaps put this break in, but we need to do
		// careful particle testing.
		break;
#endif

		case COLLISION_MODE_USE_NEAREST_TRACE:
		{
			int nIndexOut = 0;
			for( int i = -1; i <= 1; i++ )
				for( int j = -1; j <= 1; j++ )
					for( int k = -1; k <= 1; k++ )
					{
						if ( i || j || k )
						{
							SetBaseTrace( nIndexOut++, rayStart, 1000.0 * Vector( i, j, k ), nCollisionGroup, true );
						}
					}
			m_nNumFixedPlanes = nIndexOut;
			m_nActivePlanes = nIndexOut;
		}
	}
}

class C_OP_WorldCollideConstraint : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_WorldCollideConstraint );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_RADIUS_MASK;
	}

	virtual uint64 GetReadControlPointMask() const 
	{ 
		return 1ULL << 0; 
	}

	size_t GetRequiredContextBytes( ) const
	{
		return sizeof( CWorldCollideContextData );
	}

	bool EnforceConstraint( int nStartBlock,
							int nEndBlock,
							CParticleCollection *pParticles,
							void *pContext, int nNumValidParticlesInLastChunk ) const;

	void SetupConstraintPerFrameData( CParticleCollection *pParticles,
									  void *pContext ) const;
};


void C_OP_WorldCollideConstraint::SetupConstraintPerFrameData( CParticleCollection *pParticles,
															   void *pContext ) const
{
	CWorldCollideContextData *pCtx =
		reinterpret_cast<CWorldCollideContextData *>( pContext );
	pCtx->CalculatePlanes( pParticles, COLLISION_MODE_PER_FRAME_PLANESET, COLLISION_GROUP_NONE );
}

bool C_OP_WorldCollideConstraint::EnforceConstraint( int nStartBlock,
													 int nNumBlocks,
													 CParticleCollection *pParticles,
													 void *pContext,
													 int nNumValidParticlesInLastChunk ) const
{
	C4VAttributeWriteIterator pXYZ( PARTICLE_ATTRIBUTE_XYZ, pParticles );
	pXYZ += nStartBlock;

	CM128AttributeIterator pRadius( PARTICLE_ATTRIBUTE_RADIUS, pParticles );
	pRadius += nStartBlock;

	CWorldCollideContextData *pCtx = 
		reinterpret_cast<CWorldCollideContextData *>( pContext );

	bool bChangedSomething = false;
	do
	{
		for( int i=0; i < pCtx->m_nActivePlanes; i++ )
		{
			if ( pCtx->m_bPlaneActive[ i ] )
			{
				FourVectors pts = *pXYZ;
				pts -= pCtx->m_PointOnPlane[i];
				fltx4 PlaneEq=pts * pCtx->m_PlaneNormal[i];
				// where planeeq<0, inside
				PlaneEq = SubSIMD( PlaneEq, *pRadius );
				fltx4 BadPts=CmpLtSIMD( PlaneEq, Four_Zeros );
				if ( IsAnyNegative( BadPts ) )
				{
					bChangedSomething = true;
					// project points to plane surface
					fltx4 PenetrationDistance=MinSIMD( Four_Zeros, PlaneEq );
					FourVectors PenetrationVector = pCtx->m_PlaneNormal[i];
					PenetrationVector *= PenetrationDistance;
					(*pXYZ) -= PenetrationVector;
				}
			}
		}
		++pXYZ;
		++pRadius;
	} while (--nNumBlocks);
	return bChangedSomething;
}

DEFINE_PARTICLE_OPERATOR( C_OP_WorldCollideConstraint, "Prevent passing through static part of world", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_OP_WorldCollideConstraint ) 
END_PARTICLE_OPERATOR_UNPACK( C_OP_WorldCollideConstraint )



class C_OP_WorldTraceConstraint : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_WorldTraceConstraint );

	uint32 GetWrittenAttributes( void ) const
	{
		int nRet = PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ;
		if ( m_bKillonContact )
			nRet |= PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK;
		return nRet;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_RADIUS_MASK;
	}

	virtual uint64 GetReadControlPointMask() const 
	{ 
		return 1ULL << 0; 
	}

	Vector m_vecCpOffset;
	int m_nCollisionMode;
	float m_flBounceAmount;
	float m_flSlideAmount;
	float m_flRadiusScale;
	float m_flCpMovementTolerance;
	float m_flTraceTolerance;

	bool m_bKillonContact;

	virtual bool IsFinalConstraint( void ) const
	{
		return ( m_flBounceAmount != 0. ) || ( m_flSlideAmount != 0. );
	}

	void InitializeContextData( CParticleCollection *pParticles,
								void *pContext ) const
	{
	}

	char m_CollisionGroupName[128];
	int m_nCollisionGroupNumber;
	bool m_bBrushOnly;

	void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement );

	bool EnforceConstraint( int nStartBlock,
							int nEndBlock,
							CParticleCollection *pParticles,
							void *pContext,
							int nNumValidParticlesInLastChunk ) const;
	template<bool bKillOnContact, bool bCached> bool EnforceConstraintInternal( int nStartBlock,
																				int nEndBlock,
																				CParticleCollection *pParticles,
																				void *pContext, int nNumValidParticlesInLastChunk ) const;
};

void C_OP_WorldTraceConstraint::InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
{
	m_nCollisionGroupNumber = g_pParticleSystemMgr->Query()->GetCollisionGroupFromName( m_CollisionGroupName );
}


struct ISectData_t
{
	fltx4 m_ISectT;											// "t" of intersection
	fltx4 m_LeftOverT;										// "left-over" amount
	FourVectors m_ISectNormal;								// normal at intersection if any
};



static void WorldIntersectTNew( FourVectors const *pStartPnt, FourVectors const *pEndPnt, 
								int nCollisionGroup, int nMask, ISectData_t *pISectData,
								int nCollisionMode, CWorldCollideContextData *pCtx, fltx4 const &fl4ParticleValidMask,
								float flTolerance = 0.0 )
{
	pISectData->m_ISectT = Four_Zeros;
	pISectData->m_LeftOverT = Four_Zeros;
	pISectData->m_ISectNormal.x = Four_Zeros;
	pISectData->m_ISectNormal.y = Four_Zeros;
	pISectData->m_ISectNormal.z = Four_Zeros;

	if ( pCtx )
	{
		pISectData->m_ISectT = Four_Twos;
		// do simd interseciton against planes
		if ( nCollisionMode == COLLISION_MODE_USE_NEAREST_TRACE )
		{
			// find which of our traces is closest to our start / end points
			pISectData->m_ISectT = Four_Twos;				// no hit

			FourVectors v4PointOnPlane;
			FourVectors v4PlaneNormal;
			fltx4 fl4ClosestDist = Four_FLT_MAX;
			for( int i = 0 ; i < pCtx->m_nActivePlanes; i++ )
			{
				if ( pCtx->m_bPlaneActive[i] )
				{
					fltx4 fl4TrialDistance = MaxSIMD( 
						pStartPnt->DistSqrToLineSegment( pCtx->m_TraceStartPnt[i], pCtx->m_TraceEndPnt[i] ),
						pEndPnt->DistSqrToLineSegment( pCtx->m_TraceStartPnt[i], pCtx->m_TraceEndPnt[i] ) );
					fltx4 fl4Nearestmask = CmpLeSIMD( fl4TrialDistance, fl4ClosestDist );
					fl4ClosestDist = MaskedAssign( fl4ClosestDist, fl4TrialDistance, fl4Nearestmask );
					v4PointOnPlane.x = MaskedAssign( fl4Nearestmask, pCtx->m_PointOnPlane[i].x, v4PointOnPlane.x );
					v4PointOnPlane.y = MaskedAssign( fl4Nearestmask, pCtx->m_PointOnPlane[i].y, v4PointOnPlane.y );
					v4PointOnPlane.z = MaskedAssign( fl4Nearestmask, pCtx->m_PointOnPlane[i].z, v4PointOnPlane.z );
					v4PlaneNormal.x = MaskedAssign( fl4Nearestmask, pCtx->m_PlaneNormal[i].x, v4PlaneNormal.x );
					v4PlaneNormal.y = MaskedAssign( fl4Nearestmask, pCtx->m_PlaneNormal[i].y, v4PlaneNormal.y );
					v4PlaneNormal.z = MaskedAssign( fl4Nearestmask, pCtx->m_PlaneNormal[i].z, v4PlaneNormal.z );
				}
			}
			fltx4 fl4OutOfRange = AndSIMD( fl4ParticleValidMask, 
										   CmpGtSIMD( fl4ClosestDist, ReplicateX4( flTolerance ) ) );
			if ( IsAnyNegative( fl4OutOfRange ) )
			{
				nMask = TestSignSIMD( fl4OutOfRange );
				for(int i=0; i < 4; i++ )
				{
					if ( nMask & ( 1 << i ) )
					{
						Vector start = pStartPnt->Vec( i );
						Vector delta = pEndPnt->Vec( i ) - start;
						
						float ln = delta.Length();

						float traceScale = max( 5.0, 300.0 / ( ln + .01 ) );

						Vector end = start + delta * traceScale;

						CBaseTrace tr;
						g_pParticleSystemMgr->Query()->TraceLine( start, end,
																  nMask, NULL, nCollisionGroup, &tr );
		
						if ( tr.fraction < 1.0 )
						{
							SubFloat( v4PointOnPlane.x, i ) = start.x + ( tr.fraction * ( end.x - start.x ) );
							SubFloat( v4PointOnPlane.y, i ) = start.y + ( tr.fraction * ( end.y - start.y ) );
							SubFloat( v4PointOnPlane.z, i ) = start.z + ( tr.fraction * ( end.z - start.z ) );
							SubFloat( v4PlaneNormal.x, i ) = tr.plane.normal.x;
							SubFloat( v4PlaneNormal.y, i ) = tr.plane.normal.y;
							SubFloat( v4PlaneNormal.z, i ) = tr.plane.normal.z;
						}
						else
						{
							// no hit.  a normal of 0 will prevent the crossing check from ever
							// finding a crossing, since it will check for (p - origin ) dot normal
							// < 0
							SubFloat( v4PlaneNormal.x, i ) = 0;
							SubFloat( v4PlaneNormal.y, i ) = 0;
							SubFloat( v4PlaneNormal.z, i ) = 0;
						}
					}
				}
			}
			FourVectors v4StartD = *pStartPnt;
			FourVectors v4EndD = *pEndPnt;
			v4StartD -= v4PointOnPlane;
			v4EndD -= v4PointOnPlane;
			fltx4 fl4StartDist = v4StartD * v4PlaneNormal;
			fltx4 fl4EndDist = v4EndD * v4PlaneNormal;
			fltx4 fl4CrossMask = AndSIMD( CmpGeSIMD( fl4StartDist, Four_Zeros ), CmpLtSIMD( fl4EndDist, Four_Zeros ) );
			fl4CrossMask = AndSIMD( fl4CrossMask, fl4ParticleValidMask );
			if ( IsAnyNegative( fl4CrossMask ) )
			{
				// a hit!
				fltx4 fl4T = DivSIMD( fl4StartDist, SubSIMD( fl4StartDist, fl4EndDist ) );
				fl4CrossMask = AndSIMD( fl4CrossMask, CmpLtSIMD( fl4T, pISectData->m_ISectT ) );
				if ( IsAnyNegative( fl4CrossMask ) )
				{
					pISectData->m_ISectT = MaskedAssign( fl4CrossMask, fl4T, pISectData->m_ISectT );
					pISectData->m_ISectNormal.x = MaskedAssign( fl4CrossMask, v4PlaneNormal.x, pISectData->m_ISectNormal.x );
					pISectData->m_ISectNormal.y = MaskedAssign( fl4CrossMask, v4PlaneNormal.y, pISectData->m_ISectNormal.y );
					pISectData->m_ISectNormal.z = MaskedAssign( fl4CrossMask, v4PlaneNormal.z, pISectData->m_ISectNormal.z );
				}
			}
		}
		pISectData->m_LeftOverT = MaxSIMD( Four_Zeros, SubSIMD( Four_Ones, pISectData->m_ISectT ) );
	}
}

static void WorldIntersectT( FourVectors const *pStartPnt, FourVectors const *pEndPnt, 
							 int nCollisionGroup, int nMask, ISectData_t *pISectData,
							 CWorldCollideContextData *pCtx )
{
	pISectData->m_ISectT = Four_Zeros;
	pISectData->m_LeftOverT = Four_Zeros;
	pISectData->m_ISectNormal.x = Four_Zeros;
	pISectData->m_ISectNormal.y = Four_Zeros;
	pISectData->m_ISectNormal.z = Four_Zeros;

	if ( pCtx )
	{
		pISectData->m_ISectT = Four_Twos;
		// do simd interseciton against planes
		for( int i=0 ; i < pCtx->m_nActivePlanes; i++ )
		{
			if ( pCtx->m_bPlaneActive[ i ] )
			{
				FourVectors v4StartD = *pStartPnt;
				FourVectors v4EndD = *pEndPnt;
				v4StartD -= pCtx->m_PointOnPlane[i];
				v4EndD -= pCtx->m_PointOnPlane[i];
				fltx4 fl4StartDist = v4StartD * pCtx->m_PlaneNormal[i];
				fltx4 fl4EndDist = v4EndD * pCtx->m_PlaneNormal[i];
				fltx4 fl4CrossMask = AndSIMD( CmpGeSIMD( fl4StartDist, Four_Zeros ), CmpLtSIMD( fl4EndDist, Four_Zeros ) );
				if ( IsAnyNegative( fl4CrossMask ) )
				{
#ifdef FP_EXCEPTIONS_ENABLED
					// Wherever fl4CrossMask is zero we need to ensure that fl4StartDist does
					// not equal fl4EndDist to avoid divide-by-zero.
					//fl4FadeWindow = OrSIMD( AndSIMD( fl4GoodMask, fl4EndTime ), AndNotSIMD( fl4GoodMask, fl4EndTime ) );
					fl4EndDist = AddSIMD( fl4EndDist, AndNotSIMD( fl4CrossMask, Four_Ones ) );
#endif
					// a hit!
					fltx4 fl4T = DivSIMD( fl4StartDist, SubSIMD( fl4StartDist, fl4EndDist ) );
					fl4CrossMask = AndSIMD( fl4CrossMask, CmpLtSIMD( fl4T, pISectData->m_ISectT ) );
					if ( IsAnyNegative( fl4CrossMask ) )
					{
						pISectData->m_ISectT = MaskedAssign( fl4CrossMask, fl4T, pISectData->m_ISectT );
						pISectData->m_ISectNormal.x = MaskedAssign( fl4CrossMask, pCtx->m_PlaneNormal[i].x, pISectData->m_ISectNormal.x );
						pISectData->m_ISectNormal.y = MaskedAssign( fl4CrossMask, pCtx->m_PlaneNormal[i].y, pISectData->m_ISectNormal.y );
						pISectData->m_ISectNormal.z = MaskedAssign( fl4CrossMask, pCtx->m_PlaneNormal[i].z, pISectData->m_ISectNormal.z );
					}
				}
			}
		}
		pISectData->m_LeftOverT = MaxSIMD( Four_Zeros, SubSIMD( Four_Ones, pISectData->m_ISectT ) );
	}
	else
	{
		// assumes they don't start solid
		for(int i=0; i < 4; i++ )
		{
			Vector start=pStartPnt->Vec( i );
			Vector end=pEndPnt->Vec( i );
			Assert( start.IsValid() );
			Assert( end.IsValid() );

			CBaseTrace tr;
			g_pParticleSystemMgr->Query()->TraceLine( start, end,
													  nMask, NULL, nCollisionGroup, &tr );
		
			SubFloat( pISectData->m_ISectT, i ) = tr.fraction;
			if ( tr.startsolid )
			{
				SubFloat( pISectData->m_LeftOverT, i ) = 0;					// don't bounce if stuck
			}
			else
			{
				SubFloat( pISectData->m_LeftOverT, i ) = 1.0 - tr.fraction;
			}
			SubFloat( pISectData->m_ISectNormal.x, i ) = tr.plane.normal.x;
			SubFloat( pISectData->m_ISectNormal.y, i ) = tr.plane.normal.y;
			SubFloat( pISectData->m_ISectNormal.z, i ) = tr.plane.normal.z;
		}
	}
}

bool C_OP_WorldTraceConstraint::EnforceConstraint( int nStartBlock,
												   int nNumBlocks,
												   CParticleCollection *pParticles,
												   void *pContext, int nNumValidParticlesInLastChunk ) const
{
	if ( m_nCollisionMode == COLLISION_MODE_USE_NEAREST_TRACE )
	{
		if ( m_bKillonContact )
			return EnforceConstraintInternal<true, true>( nStartBlock, nNumBlocks, pParticles, pContext, nNumValidParticlesInLastChunk );
		else
			return EnforceConstraintInternal<false, true>( nStartBlock, nNumBlocks, pParticles, pContext, nNumValidParticlesInLastChunk );
	}
	else
	{
		if ( m_bKillonContact )
			return EnforceConstraintInternal<true, false>( nStartBlock, nNumBlocks, pParticles, pContext, nNumValidParticlesInLastChunk );
		else
			return EnforceConstraintInternal<false, false>( nStartBlock, nNumBlocks, pParticles, pContext, nNumValidParticlesInLastChunk );
	}
}

template<bool bKillonContact, bool bCached> bool C_OP_WorldTraceConstraint::EnforceConstraintInternal( 
	int nStartBlock,
	int nNumBlocks,
	CParticleCollection *pParticles,
	void *pContext, int nNumValidParticlesInLastChunk ) const
{
	C4VAttributeWriteIterator pPrevXYZ( PARTICLE_ATTRIBUTE_PREV_XYZ, pParticles );
	pPrevXYZ += nStartBlock;

	C4VAttributeWriteIterator pXYZ( PARTICLE_ATTRIBUTE_XYZ, pParticles );
	pXYZ += nStartBlock;

	CM128AttributeIterator pRadius( PARTICLE_ATTRIBUTE_RADIUS, pParticles );
	pRadius += nStartBlock;

	CM128AttributeWriteIterator pLifetime;

	if ( bKillonContact )
	{
		pLifetime.Init( PARTICLE_ATTRIBUTE_LIFE_DURATION, pParticles );
		pLifetime += nStartBlock;
	}
	
	
	fltx4 bounceScale = ReplicateX4( m_flBounceAmount );

	fltx4 slideScale = ReplicateX4( m_flSlideAmount );

	bool bBouncingOrSliding = ( m_flBounceAmount != 0.0 ) || ( m_flSlideAmount != 0.0 );

	fltx4 radAdjustScale = ReplicateX4( m_flRadiusScale );

	bool bChangedSomething = false;

	int nMask = MASK_SOLID;

	if ( m_bBrushOnly )
		nMask = MASK_SOLID_BRUSHONLY;
	

	CWorldCollideContextData **ppCtx;
	if ( pParticles->m_pParent )
		ppCtx = &( pParticles->m_pParent->m_pCollisionCacheData[m_nCollisionMode] );
	else
		ppCtx = &( pParticles->m_pCollisionCacheData[m_nCollisionMode] );

	CWorldCollideContextData *pCtx = NULL;
	if ( ( m_nCollisionMode == COLLISION_MODE_PER_FRAME_PLANESET ) ||
		 ( m_nCollisionMode == COLLISION_MODE_USE_NEAREST_TRACE ) ||
		 ( m_nCollisionMode == COLLISION_MODE_INITIAL_TRACE_DOWN ) )
	{
		if ( ! *ppCtx )
		{
			*ppCtx = new CWorldCollideContextData;
			(*ppCtx)->m_nActivePlanes = 0;
			(*ppCtx)->m_flLastUpdateTime = -1.0;
		}
		pCtx = *ppCtx;
		if ( pCtx->m_flLastUpdateTime != pParticles->m_flCurTime )
		{
			pCtx->CalculatePlanes( pParticles, m_nCollisionMode, m_nCollisionGroupNumber, &m_vecCpOffset, m_flCpMovementTolerance );
			pCtx->m_flLastUpdateTime = pParticles->m_flCurTime;
		}
	}
	float flTol = m_flTraceTolerance * m_flTraceTolerance;
	do
	{
		// compute radius adjust factor for intersection
		
		fltx4 radiusFactor = MulSIMD( *pRadius, radAdjustScale );
		
		// compute movement delta
		FourVectors delta = *pXYZ;
		delta -= *pPrevXYZ;
		
		
		// now, add two components - the non-intersecting movement vector, and the
		// then the movement vector with the components normal to the plane removed.
		FourVectors deltanormalized = delta;
		fltx4 len2 = delta * delta;
		
		fltx4 bBadDeltas = CmpLeSIMD( len2, Four_Zeros );
		
		len2 = ReciprocalSqrtEstSIMD( len2 );
		
		deltanormalized *= AndNotSIMD( bBadDeltas, len2 );
		
		
		FourVectors endPnt = *pXYZ;
		
		FourVectors radadjust = deltanormalized;
		radadjust *= radiusFactor;
		
		endPnt += radadjust;
		
		ISectData_t iData;
		
		if ( bCached )
		{
			fltx4 fl4TailMask;
			if ( nNumBlocks > 1 )
				fl4TailMask = LoadAlignedIntSIMD( g_SIMD_AllOnesMask );
			else
				fl4TailMask = LoadAlignedIntSIMD( g_SIMD_SkipTailMask[nNumValidParticlesInLastChunk] );
			
			WorldIntersectTNew( pPrevXYZ, &endPnt, m_nCollisionGroupNumber, nMask, &iData, m_nCollisionMode, pCtx, fl4TailMask, flTol );
		}
		else
			WorldIntersectT( pPrevXYZ, &endPnt, m_nCollisionGroupNumber, nMask, &iData, pCtx );

		
		fltx4 didhit = CmpLtSIMD( iData.m_ISectT, Four_Ones );
		// mask off zero-length deltas
		didhit = AndNotSIMD( bBadDeltas, didhit );
		
		if ( IsAnyNegative( didhit ) )						// any penetration?
		{

			bChangedSomething = true;
			if ( bKillonContact )
			{	
				*pLifetime = MaskedAssign( didhit, Four_Zeros, *pLifetime );
			}
			else
			{
				FourVectors newPnt = delta;
				newPnt *= iData.m_ISectT;
				newPnt += *pPrevXYZ;

				if ( bBouncingOrSliding )
				{
					// need to compute movement due to sliding and bouncing, and add it to the point,
					// and also compute the new velocity, adjust prev pnt to reflect that new velocity

					FourVectors bouncePart = VectorReflect( deltanormalized, iData.m_ISectNormal );
					bouncePart *= bounceScale;
					FourVectors newVel = bouncePart;

					bouncePart *= iData.m_LeftOverT;
					newPnt += bouncePart;

					FourVectors slidePart = VectorSlide( delta, iData.m_ISectNormal );
					slidePart *= slideScale;
					newVel += slidePart;

					slidePart *= iData.m_LeftOverT;

					newPnt += slidePart;

					FourVectors newPrev = newPnt;
					newPrev -= newVel;
					pPrevXYZ->x = MaskedAssign( didhit, newPrev.x, pPrevXYZ->x );
					pPrevXYZ->y = MaskedAssign( didhit, newPrev.y, pPrevXYZ->y );
					pPrevXYZ->z = MaskedAssign( didhit, newPrev.z, pPrevXYZ->z );
				}
				pXYZ->x = MaskedAssign( didhit, newPnt.x, pXYZ->x );
				pXYZ->y = MaskedAssign( didhit, newPnt.y, pXYZ->y );
				pXYZ->z = MaskedAssign( didhit, newPnt.z, pXYZ->z );
			}

			CHECKSYSTEM( pParticles );
		}
		++pXYZ;
		++pPrevXYZ;
		++pRadius;
		if ( bKillonContact )
			++pLifetime;
	} while (--nNumBlocks);
	return bChangedSomething;
}



DEFINE_PARTICLE_OPERATOR( C_OP_WorldTraceConstraint, "Collision via traces", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_OP_WorldTraceConstraint ) 
	DMXELEMENT_UNPACK_FIELD( "collision mode", "0", int, m_nCollisionMode )
	DMXELEMENT_UNPACK_FIELD( "amount of bounce", "0", float, m_flBounceAmount )
	DMXELEMENT_UNPACK_FIELD( "amount of slide", "0", float, m_flSlideAmount )
	DMXELEMENT_UNPACK_FIELD( "radius scale", "1", float, m_flRadiusScale )
	DMXELEMENT_UNPACK_FIELD( "brush only", "0", bool, m_bBrushOnly )
	DMXELEMENT_UNPACK_FIELD_STRING( "collision group", "NONE", m_CollisionGroupName )
	DMXELEMENT_UNPACK_FIELD( "control point offset for fast collisions", "0 0 0", Vector, m_vecCpOffset )
	DMXELEMENT_UNPACK_FIELD( "control point movement distance tolerance", "5", float, m_flCpMovementTolerance )
	DMXELEMENT_UNPACK_FIELD( "kill particle on collision", "0", bool, m_bKillonContact )
	DMXELEMENT_UNPACK_FIELD( "trace accuracy tolerance", "24", float, m_flTraceTolerance )
END_PARTICLE_OPERATOR_UNPACK( C_OP_WorldTraceConstraint )

void AddBuiltInParticleConstraints( void )
{
	REGISTER_PARTICLE_OPERATOR( FUNCTION_CONSTRAINT, C_OP_ConstrainDistance );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_CONSTRAINT, C_OP_PlanarConstraint );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_CONSTRAINT, C_OP_WorldCollideConstraint );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_CONSTRAINT, C_OP_WorldTraceConstraint );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_CONSTRAINT, C_OP_ConstrainDistanceToPath );
}


