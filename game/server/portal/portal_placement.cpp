//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "portal_placement.h"
#include "portal_shareddefs.h"
#include "prop_portal_shared.h"
#include "func_noportal_volume.h"
#include "BasePropDoor.h"
#include "collisionutils.h"
#include "decals.h"
#include "physicsshadowclone.h"


#define MAXIMUM_BUMP_DISTANCE ( ( PORTAL_HALF_WIDTH * 2.0f ) * ( PORTAL_HALF_WIDTH * 2.0f ) + ( PORTAL_HALF_HEIGHT * 2.0f ) * ( PORTAL_HALF_HEIGHT * 2.0f ) ) / 2.0f


struct CPortalCornerFitData
{
	trace_t trCornerTrace;
	Vector ptIntersectionPoint;
	Vector vIntersectionDirection;
	Vector vBumpDirection;
	bool bCornerIntersection;
	bool bSoftBump;
};


CUtlVector<CBaseEntity *> g_FuncBumpingEntityList;
bool g_bBumpedByLinkedPortal;


ConVar sv_portal_placement_debug ("sv_portal_placement_debug", "0", FCVAR_REPLICATED );
ConVar sv_portal_placement_never_bump ("sv_portal_placement_never_bump", "0", FCVAR_REPLICATED | FCVAR_CHEAT );


bool IsMaterialInList( const csurface_t &surface, char *g_ppszMaterials[] )
{
	char szLowerName[ 256 ];
	Q_strcpy( szLowerName, surface.name );
	Q_strlower( szLowerName );

	int iMaterial = 0;

	while ( g_ppszMaterials[ iMaterial ] )
	{
		if ( Q_strstr( szLowerName, g_ppszMaterials[ iMaterial ] ) )
			return true;

		++iMaterial;
	}

	return false;
}

bool IsNoPortalMaterial( const csurface_t &surface )
{
	if ( surface.flags & SURF_NOPORTAL )
		return true;

	const surfacedata_t *pdata = physprops->GetSurfaceData( surface.surfaceProps );
	if ( pdata->game.material == CHAR_TEX_GLASS )
		return true;

	// Skipping all studio models
	if ( StringHasPrefix( surface.name, "**studio**" ) )
		return true;

	return false;
}

bool IsPassThroughMaterial( const csurface_t &surface )
{
	if ( surface.flags & SURF_SKY )
		return true;

	if ( IsMaterialInList( surface, g_ppszPortalPassThroughMaterials ) )
		return true;

	return false;
}


void TracePortals( const CProp_Portal *pIgnorePortal, const Vector &vForward, const Vector &vStart, const Vector &vEnd, trace_t &tr )
{
	UTIL_ClearTrace( tr );

	Ray_t ray;
	ray.Init( vStart, vEnd );

	trace_t trTemp;

	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if( iPortalCount != 0 )
	{
		CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
		for( int i = 0; i != iPortalCount; ++i )
		{
			CProp_Portal *pTempPortal = pPortals[i];
			if( pTempPortal != pIgnorePortal && pTempPortal->m_bActivated )
			{
				Vector vOtherOrigin = pTempPortal->GetAbsOrigin();
				QAngle qOtherAngles = pTempPortal->GetAbsAngles();

				Vector vLinkedForward;
				AngleVectors( qOtherAngles, &vLinkedForward, NULL, NULL );

				// If they're not on the same face then don't worry about overlap
				if ( vForward.Dot( vLinkedForward ) < 0.95f )
					continue;

				UTIL_IntersectRayWithPortalOBBAsAABB( pTempPortal, ray, &trTemp );

				if ( trTemp.fraction < 1.0f && trTemp.fraction < tr.fraction )
				{
					tr = trTemp;
				}
			}
		}
	}
}

bool TraceBumpingEntities( const Vector &vStart, const Vector &vEnd, trace_t &tr )
{
	UTIL_ClearTrace( tr );

	// We use this so portal bumpers can't squeeze a portal into not fitting
	bool bClosestIsSoftBumper = false;

	// Trace to the surface to see if there's a rotating door in the way
	CBaseEntity *list[1024];

	Ray_t ray;
	ray.Init( vStart, vEnd );

	int nCount = UTIL_EntitiesAlongRay( list, 1024, ray, 0 );

	for ( int i = 0; i < nCount; i++ )
	{
		trace_t trTemp;
		UTIL_ClearTrace( trTemp );

		bool bSoftBumper = false;

		if ( FClassnameIs( list[i], "func_portal_bumper" ) )
		{
			bSoftBumper = true;
			enginetrace->ClipRayToEntity( ray, MASK_ALL, list[i], &trTemp );
			if ( trTemp.startsolid )
			{
				trTemp.fraction = 1.0f;
			}
		}
		else if ( FClassnameIs( list[i], "trigger_portal_cleanser" ) )
		{
			enginetrace->ClipRayToEntity( ray, MASK_ALL, list[i], &trTemp );
			if ( trTemp.startsolid )
			{
				trTemp.fraction = 1.0f;
			}
		}
		else if ( FClassnameIs( list[i], "func_noportal_volume" ) )
		{
			if ( static_cast<CFuncNoPortalVolume*>( list[i] )->IsActive() )
			{
				enginetrace->ClipRayToEntity( ray, MASK_ALL, list[i], &trTemp );

				// Bump by an extra 2 units so that the portal isn't touching the no portal volume
				Vector vDelta = trTemp.endpos - trTemp.startpos;
				float fLength = VectorNormalize( vDelta ) - 2.0f;
				if ( fLength < 0.0f )
					fLength = 0.0f;
				trTemp.fraction = fLength / ray.m_Delta.Length();
				trTemp.endpos = trTemp.startpos + vDelta * fLength;
			}
			else
				trTemp.fraction = 1.0f;
		}
		else if( FClassnameIs( list[i], "prop_door_rotating" ) )
		{
			// Check more precise door collision
			CBasePropDoor *pRotatingDoor = static_cast<CBasePropDoor *>( list[i] );

			pRotatingDoor->TestCollision( ray, 0, trTemp );
		}

		// If this is the closest and has only bumped once (for soft bumpers)
		if ( trTemp.fraction < tr.fraction && ( !bSoftBumper || !g_FuncBumpingEntityList.HasElement( list[i] ) ) )
		{
			tr = trTemp;
			bClosestIsSoftBumper = bSoftBumper;
		}
	}

	return bClosestIsSoftBumper;
}

bool TracePortalCorner( const CProp_Portal *pIgnorePortal, const Vector &vOrigin, const Vector &vCorner, const Vector &vForward, int iPlacedBy, ITraceFilter *pTraceFilterPortalShot, trace_t &tr, bool &bSoftBump )
{
	Vector vOriginToCorner = vCorner - vOrigin;

	// Check for surface edge
	trace_t trSurfaceEdge;
	UTIL_TraceLine( vOrigin - vForward, vCorner - vForward, MASK_SHOT_PORTAL, pTraceFilterPortalShot, &trSurfaceEdge );

	if ( trSurfaceEdge.startsolid )
	{
		float fTotalFraction = trSurfaceEdge.fractionleftsolid;

		while ( trSurfaceEdge.startsolid && trSurfaceEdge.fractionleftsolid > 0.0f && fTotalFraction < 1.0f )
		{
			UTIL_TraceLine( vOrigin + vOriginToCorner * ( fTotalFraction + 0.05f ) - vForward, vCorner + vOriginToCorner * ( fTotalFraction + 0.05f ) - vForward, MASK_SHOT_PORTAL, pTraceFilterPortalShot, &trSurfaceEdge );

			if ( trSurfaceEdge.startsolid )
			{
				fTotalFraction += trSurfaceEdge.fractionleftsolid + 0.05f;
			}
		}

		if ( fTotalFraction < 1.0f )
		{
			UTIL_TraceLine( vOrigin + vOriginToCorner * ( fTotalFraction + 0.05f ) - vForward, vOrigin - vForward, MASK_SHOT_PORTAL, pTraceFilterPortalShot, &trSurfaceEdge );

			if ( trSurfaceEdge.startsolid )
			{
				trSurfaceEdge.fraction = 1.0f;
			}
			else
			{
				trSurfaceEdge.fraction = fTotalFraction;
				trSurfaceEdge.plane.normal = -trSurfaceEdge.plane.normal;
			}
		}
		else
		{
			trSurfaceEdge.fraction = 1.0f;
		}
	}
	else
	{
		trSurfaceEdge.fraction = 1.0f;
	}

	// Check for enclosing wall
	trace_t trEnclosingWall;
	UTIL_TraceLine( vOrigin + vForward, vCorner + vForward, MASK_SOLID_BRUSHONLY|CONTENTS_MONSTER, pTraceFilterPortalShot, &trEnclosingWall );

	if ( trSurfaceEdge.fraction < trEnclosingWall.fraction )
	{
		trEnclosingWall.fraction = trSurfaceEdge.fraction;
		trEnclosingWall.plane.normal = trSurfaceEdge.plane.normal;
	}

	trace_t trPortal;
	trace_t trBumpingEntity;

	if ( iPlacedBy != PORTAL_PLACED_BY_FIXED )
		TracePortals( pIgnorePortal, vForward, vOrigin + vForward, vCorner + vForward, trPortal );
	else
		UTIL_ClearTrace( trPortal );

	bool bSoftBumper = TraceBumpingEntities( vOrigin + vForward, vCorner + vForward, trBumpingEntity );

	if ( trEnclosingWall.fraction >= 1.0f && trPortal.fraction >= 1.0f && trBumpingEntity.fraction >= 1.0f )
	{
		UTIL_ClearTrace( tr );
		return false;
	}

	if ( trEnclosingWall.fraction <= trPortal.fraction && trEnclosingWall.fraction <= trBumpingEntity.fraction )
	{
		tr = trEnclosingWall;
		bSoftBump = false;
	}
	else if ( trPortal.fraction <= trEnclosingWall.fraction && trPortal.fraction <= trBumpingEntity.fraction )
	{
		tr = trPortal;
		g_bBumpedByLinkedPortal = true;
		bSoftBump = false;
	}
	else if ( !trBumpingEntity.startsolid && trBumpingEntity.fraction <= trEnclosingWall.fraction && trBumpingEntity.fraction <= trPortal.fraction )
	{
		tr = trBumpingEntity;
		bSoftBump = bSoftBumper;
	}
	else
	{
		UTIL_ClearTrace( tr );
		return false;
	}

	return true;
}

Vector FindBumpVectorInCorner( const Vector &ptCorner1, const Vector &ptCorner2, const Vector &ptIntersectionPoint1, const Vector &ptIntersectionPoint2, const Vector &vIntersectionDirection1, const Vector &vIntersectionDirection2, const Vector &vIntersectionBumpDirection1, const Vector &vIntersectionBumpDirection2 )
{
	Vector ptClosestSegment1, ptClosestSegment2;
	float fT1, fT2;

	CalcLineToLineIntersectionSegment( ptIntersectionPoint1, ptIntersectionPoint1 + vIntersectionDirection1, 
		ptIntersectionPoint2, ptIntersectionPoint2 + vIntersectionDirection2,
		&ptClosestSegment1, &ptClosestSegment2, &fT1, &fT2 );

	Vector ptLineIntersection = ( ptClosestSegment1 + ptClosestSegment2 ) * 0.5f;

	// The 2 corner trace intersections and the intersection of those lines makes a triangle.
	// We want to make a similar triangle where the base is large enough to fit the edge of the portal

	// Get the the small triangle's legs and leg lengths
	Vector vShortLeg = ptIntersectionPoint1 - ptLineIntersection;
	Vector vShortLeg2 = ptIntersectionPoint2 - ptLineIntersection;

	float fShortLegLength = vShortLeg.Length();
	float fShortLeg2Length = vShortLeg2.Length();

	if ( fShortLegLength == 0.0f || fShortLeg2Length == 0.0f )
	{
		// FIXME: Our triangle is actually a point or a line, so there's nothing we can do
		return vec3_origin;
	}

	// Normalized legs
	vShortLeg /= fShortLegLength;
	vShortLeg2 /= fShortLeg2Length;

	// Check if corners are aligned with one of the legs
	Vector vCornerToCornerNorm = ptCorner2 - ptCorner1;
	VectorNormalize( vCornerToCornerNorm );

	float fPortalEdgeDotLeg = vCornerToCornerNorm.Dot( vShortLeg );
	float fPortalEdgeDotLeg2 = vCornerToCornerNorm.Dot( vShortLeg2 );

	if ( fPortalEdgeDotLeg < -0.9999f || fPortalEdgeDotLeg > 0.9999f || fPortalEdgeDotLeg2 < -0.9999f || fPortalEdgeDotLeg2 > 0.9999f )
	{
		// Do a one corner bump with corner 1
		float fBumpDistance1 = CalcDistanceToLine( ptCorner1, ptIntersectionPoint1, ptIntersectionPoint1 + vIntersectionDirection1 );

		fBumpDistance1 += PORTAL_BUMP_FORGIVENESS;

		// Do a one corner bump with corner 2
		float fBumpDistance2 = CalcDistanceToLine( ptCorner2, ptIntersectionPoint2, ptIntersectionPoint2 + vIntersectionDirection2 );

		fBumpDistance2 += PORTAL_BUMP_FORGIVENESS;

		return vIntersectionBumpDirection1 * fBumpDistance1 + vIntersectionBumpDirection2 * fBumpDistance2;
	}

	float fLegsDot = vShortLeg.Dot( vShortLeg2 );

	// Need to know if the triangle is pointing toward the portal or away from the portal
	/*bool bPointingTowardPortal = true;

	Vector vLineIntersectionToCornerNorm = ptCorner1 - ptLineIntersection;
	VectorNormalize( vLineIntersectionToCornerNorm );

	if ( vLineIntersectionToCornerNorm.Dot( vShortLeg2 ) < fLegsDot )
	{
	bPointingTowardPortal = false;
	}

	if ( !bPointingTowardPortal )*/
	{
		// Get the small triangle's base length
		float fLongBaseLength = ptCorner1.DistTo( ptCorner2 );

		// Get the large triangle's base length
		float fShortLeg2Angle = acosf( vCornerToCornerNorm.Dot( -vShortLeg ) );
		float fShortBaseAngle = acosf( fLegsDot );
		float fShortLegAngle = M_PI_F - fShortBaseAngle - fShortLeg2Angle;

		if ( sinf( fShortLegAngle ) == 0.0f )
		{
			return Vector( 1000.0f, 1000.0f, 1000.0f );
		}

		float fShortBaseLength = sinf( fShortBaseAngle ) * ( fShortLegLength / sinf( fShortLegAngle ) );

		// Avoid divide by zero
		if ( fShortBaseLength == 0.0f )
		{
			return Vector( 0.0f, 0.0f, 0.0f );
		}

		// Use ratio to get the big triangles leg length
		float fLongLegLength = fLongBaseLength * ( fShortLegLength / fShortBaseLength );

		// Get the relative point on the large triangle
		Vector ptNewCornerPos = ptLineIntersection + vShortLeg * fLongLegLength;

		// Bump by the same amount the corner has to move to fit
		return ptNewCornerPos - ptCorner1;
	}
	/*else
	{
	return Vector( 0.0f, 0.0f, 0.0f );
	}*/
}


bool FitPortalOnSurface( const CProp_Portal *pIgnorePortal, Vector &vOrigin, const Vector &vForward, const Vector &vRight, 
						 const Vector &vTopEdge, const Vector &vBottomEdge, const Vector &vRightEdge, const Vector &vLeftEdge, 
						 int iPlacedBy, ITraceFilter *pTraceFilterPortalShot, 
						 int iRecursions /*= 0*/, const CPortalCornerFitData *pPortalCornerFitData /*= 0*/, const int *p_piIntersectionIndex /*= 0*/, const int *piIntersectionCount /*= 0*/ )
{
	// Don't infinitely recurse
	if ( iRecursions >= 6 )
	{
		return false;
	}

	Vector pptCorner[ 4 ];

	// Get corner points
	pptCorner[ 0 ] = vOrigin + vTopEdge + vLeftEdge;
	pptCorner[ 1 ] = vOrigin + vTopEdge + vRightEdge;
	pptCorner[ 2 ] = vOrigin + vBottomEdge + vLeftEdge;
	pptCorner[ 3 ] = vOrigin + vBottomEdge + vRightEdge;

	// Corner data
	CPortalCornerFitData sFitData[ 4 ];
	int piIntersectionIndex[ 4 ];
	int iIntersectionCount = 0;

	// Gather data we already know
	if ( pPortalCornerFitData )
	{
		for ( int iIntersection = 0; iIntersection < 4; ++iIntersection )
		{
			sFitData[ iIntersection ] = pPortalCornerFitData[ iIntersection ];
		}
	}
	else
	{
		memset( sFitData, 0, sizeof( sFitData ) );
	}

	if ( p_piIntersectionIndex )
	{
		for ( int iIntersection = 0; iIntersection < 4; ++iIntersection )
		{
			piIntersectionIndex[ iIntersection ] = p_piIntersectionIndex[ iIntersection ];
		}
	}
	else
	{
		memset( piIntersectionIndex, 0, sizeof( piIntersectionIndex ) );
	}

	if ( piIntersectionCount )
	{
		iIntersectionCount = *piIntersectionCount;
	}

	int iOldIntersectionCount = iIntersectionCount;

	// Find intersections from center to each corner
	for ( int iIntersection = 0; iIntersection < 4; ++iIntersection )
	{
		// HACK: In weird cases intersection count can go over 3 and index outside of our arrays. Don't let this happen!
		if ( iIntersectionCount < 4 )
		{
			// Don't recompute intersection data that we already have
			if ( !sFitData[ iIntersection ].bCornerIntersection )
			{
				// Test intersection of the current corner
				sFitData[ iIntersection ].bCornerIntersection = TracePortalCorner( pIgnorePortal, vOrigin, pptCorner[ iIntersection ], vForward, iPlacedBy, pTraceFilterPortalShot, sFitData[ iIntersection ].trCornerTrace, sFitData[ iIntersection ].bSoftBump );

				// If the intersection has no normal, ignore it
				if ( sFitData[ iIntersection ].trCornerTrace.plane.normal.IsZero() )
					sFitData[ iIntersection ].bCornerIntersection = false;

				// If it intersected
				if ( sFitData[ iIntersection ].bCornerIntersection )
				{
					sFitData[ iIntersection ].ptIntersectionPoint = vOrigin + ( pptCorner[ iIntersection ] - vOrigin ) * sFitData[ iIntersection ].trCornerTrace.fraction;
					VectorNormalize( sFitData[ iIntersection ].trCornerTrace.plane.normal );
					sFitData[ iIntersection ].vIntersectionDirection = sFitData[ iIntersection ].trCornerTrace.plane.normal.Cross( vForward );
					VectorNormalize( sFitData[ iIntersection ].vIntersectionDirection );
					sFitData[ iIntersection ].vBumpDirection = vForward.Cross( sFitData[ iIntersection ].vIntersectionDirection );
					VectorNormalize( sFitData[ iIntersection ].vBumpDirection );

					piIntersectionIndex[ iIntersectionCount ] = iIntersection;

					if ( sv_portal_placement_debug.GetBool() )
					{
						for ( int iIntersection = 0; iIntersection < 4; ++iIntersection )
						{
							NDebugOverlay::Line( sFitData[ iIntersection ].ptIntersectionPoint - sFitData[ iIntersection ].vIntersectionDirection * 32.0f, 
								sFitData[ iIntersection ].ptIntersectionPoint + sFitData[ iIntersection ].vIntersectionDirection * 32.0f, 
								0, 0, 255, true, 0.5f );
						}
					}

					++iIntersectionCount;
				}
			}
			else
			{
				// We shouldn't be intersecting with any old corners
				sFitData[ iIntersection ].trCornerTrace.fraction = 1.0f;
			}
		}
	}

	for ( int iIntersection = 0; iIntersection < 4; ++iIntersection )
	{
		// Remember soft bumpers so we don't bump with it twice
		if ( sFitData[ iIntersection ].bSoftBump )
		{
			g_FuncBumpingEntityList.AddToTail( sFitData[ iIntersection ].trCornerTrace.m_pEnt );
		}
	}

	// If no new intersections were found then it already fits
	if ( iOldIntersectionCount == iIntersectionCount )
	{
		return true;
	}

	switch ( iIntersectionCount )
	{
	case 0:
		{
			// If no corners intersect it already fits
			return true;
		}
		break;

	case 1:
		{
			float fBumpDistance = CalcDistanceToLine( pptCorner[ piIntersectionIndex[ 0 ] ], 
				sFitData[ piIntersectionIndex[ 0 ] ].ptIntersectionPoint, 
				sFitData[ piIntersectionIndex[ 0 ] ].ptIntersectionPoint + sFitData[ piIntersectionIndex[ 0 ] ].vIntersectionDirection );

			fBumpDistance += PORTAL_BUMP_FORGIVENESS;

			vOrigin += sFitData[ piIntersectionIndex[ 0 ] ].vBumpDirection * fBumpDistance;

			return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, iPlacedBy, pTraceFilterPortalShot, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );
		}
		break;

	case 2:
		{
			if ( sFitData[ piIntersectionIndex[ 0 ] ].ptIntersectionPoint == sFitData[ piIntersectionIndex[ 1 ] ].ptIntersectionPoint )
			{
				return false;
			}

			float fDot = sFitData[ piIntersectionIndex[ 0 ] ].vBumpDirection.Dot( sFitData[ piIntersectionIndex[ 1 ] ].vBumpDirection );

			// If there are parallel intersections try scooting it away from a near wall
			if ( fDot < -0.9f )
			{
				// Check if perpendicular wall is near
				trace_t trPerpWall1;
				bool bSoftBump1;
				bool bDir1 = TracePortalCorner( pIgnorePortal, vOrigin, vOrigin + sFitData[ piIntersectionIndex[ 0 ] ].vIntersectionDirection * PORTAL_HALF_WIDTH * 2.0f, vForward, iPlacedBy, pTraceFilterPortalShot, trPerpWall1, bSoftBump1 );

				trace_t trPerpWall2;
				bool bSoftBump2;
				bool bDir2 = TracePortalCorner( pIgnorePortal, vOrigin, vOrigin + sFitData[ piIntersectionIndex[ 0 ] ].vIntersectionDirection * -PORTAL_HALF_WIDTH * 2.0f, vForward, iPlacedBy, pTraceFilterPortalShot, trPerpWall2, bSoftBump2 );

				// No fit if there's blocking walls on both sides it can't fit
				if ( bDir1 && bDir2 )
				{
					if ( bSoftBump1 )
						bDir1 = false;
					else if ( bSoftBump2 )
						bDir1 = true;
					else
						return false;
				}

				// If there's no assumption to make, just pick a direction.
				if ( !bDir1 && !bDir2 )
				{
					bDir1 = true;
				}

				// Bump the portal
				if ( bDir1 )
				{
					vOrigin += sFitData[ piIntersectionIndex[ 0 ] ].vIntersectionDirection * -PORTAL_HALF_WIDTH;
				}
				else
				{
					vOrigin += sFitData[ piIntersectionIndex[ 0 ] ].vIntersectionDirection * PORTAL_HALF_WIDTH;
				}

				// Prepare data for recursion
				iIntersectionCount = 0;
				sFitData[ piIntersectionIndex[ 0 ] ].bCornerIntersection = false;
				sFitData[ piIntersectionIndex[ 1 ] ].bCornerIntersection = false;

				return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, iPlacedBy, pTraceFilterPortalShot, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );	
			}

			// If they are the same there's an easy way
			if ( fDot > 0.9f )
			{
				// Get the closest intersection to the portal's center
				int iClosestIntersection = ( ( vOrigin.DistTo( sFitData[ piIntersectionIndex[ 0 ] ].ptIntersectionPoint ) < vOrigin.DistTo( sFitData[ piIntersectionIndex[ 1 ] ].ptIntersectionPoint ) ) ? ( 0 ) : ( 1 ) );

				// Find the largest amount that the portal needs to bump for the corner to pass the intersection
				float pfBumpDistance[ 2 ];

				for ( int iIntersection = 0; iIntersection < 2; ++iIntersection )
				{
					pfBumpDistance[ iIntersection ] = CalcDistanceToLine( pptCorner[ piIntersectionIndex[ iIntersection ] ], 
						sFitData[ piIntersectionIndex[ iClosestIntersection ] ].ptIntersectionPoint, 
						sFitData[ piIntersectionIndex[ iClosestIntersection ] ].ptIntersectionPoint + sFitData[ piIntersectionIndex[ iClosestIntersection ] ].vIntersectionDirection );

					pfBumpDistance[	iIntersection ] += PORTAL_BUMP_FORGIVENESS;
				}

				int iLargestBump = ( ( pfBumpDistance[ 0 ] > pfBumpDistance[ 1 ] ) ? ( 0 ) : ( 1 ) );

				// Bump the portal
				vOrigin += sFitData[ piIntersectionIndex[ iClosestIntersection ] ].vBumpDirection * pfBumpDistance[ iLargestBump ];

				// If they were parallel to the intersection line don't invalidate both before recursion
				if ( pfBumpDistance[ 0 ] == pfBumpDistance[ 1 ] )
				{
					sFitData[ piIntersectionIndex[ 0 ] ].bCornerIntersection = false;
					sFitData[ piIntersectionIndex[ 1 ] ].bCornerIntersection = false;
					iIntersectionCount = 0;

					return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, iPlacedBy, pTraceFilterPortalShot, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );
				}
				else
				{
					// Prepare data for recursion
					if ( iLargestBump != iClosestIntersection )
					{
						sFitData[ piIntersectionIndex[ iLargestBump ] ] = sFitData[ piIntersectionIndex[ iClosestIntersection ] ];
					}
					sFitData[ piIntersectionIndex[ ( ( iLargestBump == 0 ) ? ( 1 ) : ( 0 ) ) ] ].bCornerIntersection = false;
					piIntersectionIndex[ 0 ] = piIntersectionIndex[ iLargestBump ];
					iIntersectionCount = 1;

					return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, iPlacedBy, pTraceFilterPortalShot, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );
				}
			}

			// Intersections are angled, bump based on math using the corner
			vOrigin += FindBumpVectorInCorner( pptCorner[ piIntersectionIndex[ 0 ] ], pptCorner[ piIntersectionIndex[ 1 ] ], 
				sFitData[ piIntersectionIndex[ 0 ] ].ptIntersectionPoint, sFitData[ piIntersectionIndex[ 1 ] ].ptIntersectionPoint,
				sFitData[ piIntersectionIndex[ 0 ] ].vIntersectionDirection, sFitData[ piIntersectionIndex[ 1 ] ].vIntersectionDirection,
				sFitData[ piIntersectionIndex[ 0 ] ].vBumpDirection, sFitData[ piIntersectionIndex[ 1 ] ].vBumpDirection );

			return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, iPlacedBy, pTraceFilterPortalShot, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );
		}
		break;

	case 3:
		{
			// Get the relationships of the intersections
			float fDot[ 3 ];
			fDot[ 0 ] = sFitData[ piIntersectionIndex[ 0 ] ].vBumpDirection.Dot( sFitData[ piIntersectionIndex[ 1 ] ].vBumpDirection );
			fDot[ 1 ] = sFitData[ piIntersectionIndex[ 1 ] ].vBumpDirection.Dot( sFitData[ piIntersectionIndex[ 2 ] ].vBumpDirection );
			fDot[ 2 ] = sFitData[ piIntersectionIndex[ 2 ] ].vBumpDirection.Dot( sFitData[ piIntersectionIndex[ 0 ] ].vBumpDirection );

			int iSimilarWalls = 0;

			for ( int iDot = 0; iDot < 3; ++iDot )
			{
				// If there are parallel intersections try scooting it away from a near wall
				if ( fDot[ iDot ] < -0.99f )
				{
					// Check if perpendicular wall is near
					trace_t trPerpWall1;
					bool bSoftBump1;
					bool bDir1 = TracePortalCorner( pIgnorePortal, vOrigin, vOrigin + sFitData[ piIntersectionIndex[ iDot ] ].vIntersectionDirection * PORTAL_HALF_WIDTH * 2.0f, vForward, iPlacedBy, pTraceFilterPortalShot, trPerpWall1, bSoftBump1 );

					trace_t trPerpWall2;
					bool bSoftBump2;
					bool bDir2 = TracePortalCorner( pIgnorePortal, vOrigin, vOrigin + sFitData[ piIntersectionIndex[ iDot ] ].vIntersectionDirection * -PORTAL_HALF_WIDTH * 2.0f, vForward, iPlacedBy, pTraceFilterPortalShot, trPerpWall2, bSoftBump2 );

					// No fit if there's blocking walls on both sides it can't fit
					if ( bDir1 && bDir2 )
					{
						if ( bSoftBump1 )
							bDir1 = false;
						else if ( bSoftBump2 )
							bDir1 = true;
						else
							return false;
					}

					// If there's no assumption to make, just pick a direction.
					if ( !bDir1 && !bDir2 )
					{
						bDir1 = true;
					}

					// Bump the portal
					if ( bDir1 )
					{
						vOrigin += sFitData[ piIntersectionIndex[ iDot ] ].vIntersectionDirection * -PORTAL_HALF_WIDTH;
					}
					else
					{
						vOrigin += sFitData[ piIntersectionIndex[ iDot ] ].vIntersectionDirection * PORTAL_HALF_WIDTH;
					}

					// Prepare data for recursion
					iIntersectionCount = 0;
					sFitData[ piIntersectionIndex[ 0 ] ].bCornerIntersection = false;
					sFitData[ piIntersectionIndex[ 1 ] ].bCornerIntersection = false;
					sFitData[ piIntersectionIndex[ 2 ] ].bCornerIntersection = false;

					return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, iPlacedBy, pTraceFilterPortalShot, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );
				}
				// Count similar intersections
				else if ( fDot[ iDot ] > 0.99f )
				{
					++iSimilarWalls;
				}
			}

			// If no intersections are similar
			if ( iSimilarWalls == 0 )
			{
				// Total the angles between the intersections
				float fAngleTotal = 0.0f;
				for ( int iDot = 0; iDot < 3; ++iDot )
				{
					fAngleTotal += acosf( fDot[ iDot ] );
				}

				// If it's in a triangle, it can't be fit
				if ( M_PI_F - 0.01f < fAngleTotal && fAngleTotal < M_PI_F + 0.01f )
				{
					// If any of the bumps are soft, give it another try
					if ( sFitData[ piIntersectionIndex[ 0 ] ].bSoftBump || sFitData[ piIntersectionIndex[ 1 ] ].bSoftBump || sFitData[ piIntersectionIndex[ 2 ] ].bSoftBump )
					{
						// Prepare data for recursion
						iIntersectionCount = 0;
						sFitData[ piIntersectionIndex[ 0 ] ].bCornerIntersection = false;
						sFitData[ piIntersectionIndex[ 1 ] ].bCornerIntersection = false;
						sFitData[ piIntersectionIndex[ 2 ] ].bCornerIntersection = false;

						return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, iPlacedBy, pTraceFilterPortalShot, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );
					}
					else
					{
						return false;
					}
				}
			}

			// If the intersections are all similar there's an easy way
			if ( iSimilarWalls == 3 )
			{
				// Get the closest intersection to the portal's center
				int iClosestIntersection = 0;
				float fClosestDistance = vOrigin.DistTo( sFitData[ piIntersectionIndex[ 0 ] ].ptIntersectionPoint );

				float fDistance = vOrigin.DistTo( sFitData[ piIntersectionIndex[ 1 ] ].ptIntersectionPoint );
				if ( fClosestDistance > fDistance )
				{
					iClosestIntersection = 1;
					fClosestDistance = fDistance;
				}

				fDistance = vOrigin.DistTo( sFitData[ piIntersectionIndex[ 2 ] ].ptIntersectionPoint );
				if ( fClosestDistance > fDistance )
				{
					iClosestIntersection = 2;
					fClosestDistance = fDistance;
				}

				// Find the largest amount that the portal needs to bump for the corner to pass the intersection
				float pfBumpDistance[ 3 ];

				for ( int iIntersection = 0; iIntersection < 3; ++iIntersection )
				{
					pfBumpDistance[ iIntersection ] = CalcDistanceToLine( pptCorner[ piIntersectionIndex[ iIntersection ] ], 
						sFitData[ piIntersectionIndex[ iClosestIntersection ] ].ptIntersectionPoint, 
						sFitData[ piIntersectionIndex[ iClosestIntersection ] ].ptIntersectionPoint + sFitData[ piIntersectionIndex[ iClosestIntersection ] ].vIntersectionDirection );
					pfBumpDistance[ iIntersection ] += PORTAL_BUMP_FORGIVENESS;
				}

				int iLargestBump = ( ( pfBumpDistance[ 0 ] > pfBumpDistance[ 1 ] ) ? ( 0 ) : ( 1 ) );

				iLargestBump = ( ( pfBumpDistance[ iLargestBump ] > pfBumpDistance[ 2 ] ) ? ( iLargestBump ) : ( 2 ) );

				// Bump the portal
				vOrigin += sFitData[ piIntersectionIndex[ iClosestIntersection ] ].vBumpDirection * pfBumpDistance[ iLargestBump ];

				// Prepare data for recursion
				int iStillIntersecting = 0;

				for ( int iIntersection = 0; iIntersection < 3; ++iIntersection )
				{
					// Invalidate corners that were closer to the intersection line
					if ( pfBumpDistance[ iIntersection ] != pfBumpDistance[ iLargestBump ] )
					{
						sFitData[ piIntersectionIndex[ iIntersection ] ].bCornerIntersection = false;
						--iIntersectionCount;
					}
					else
					{
						sFitData[ piIntersectionIndex[ iIntersection ] ] = sFitData[ piIntersectionIndex[ iClosestIntersection ] ];
						piIntersectionIndex[ iStillIntersecting ] = piIntersectionIndex[ iIntersection ];
						++iStillIntersecting;
					}
				}

				return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, iPlacedBy, pTraceFilterPortalShot, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );
			}

			// Get info for which corners are diagonal from each other
			float fLongestDist = 0.0f;
			int iLongestDist = 0;

			for ( int iIntersection = 0; iIntersection < 3; ++iIntersection )
			{
				float fDist = pptCorner[ piIntersectionIndex[ iIntersection ] ].DistTo( pptCorner[ piIntersectionIndex[ ( iIntersection + 1 ) % 3 ] ] );

				if ( fLongestDist < fDist )
				{
					fLongestDist = fDist;
					iLongestDist = iIntersection;
				}
			}

			int iIndex1, iIndex2, iIndex3;

			switch ( iLongestDist )
			{
			case 0:
				iIndex1 = 0;
				iIndex2 = 1;
				iIndex3 = 2;
				break;

			case 1:
				iIndex1 = 1;
				iIndex2 = 2;
				iIndex3 = 0;
				break;

			default:
				iIndex1 = 2;
				iIndex2 = 0;
				iIndex3 = 1;
				break;
			}

			// If corner is 90 degrees there my be an easy way
			float fCornerDot = sFitData[ piIntersectionIndex[ iIndex1 ] ].vIntersectionDirection.Dot( sFitData[ piIntersectionIndex[ iIndex2 ] ].vIntersectionDirection );

			if ( fCornerDot < 0.0001f && fCornerDot > -0.0001f )
			{
				// Check if portal is aligned perfectly with intersection normals
				float fPortalDot = sFitData[ piIntersectionIndex[ iIndex1 ] ].vIntersectionDirection.Dot( vRight );

				if ( ( fPortalDot < 0.0001f && fPortalDot > -0.0001f ) || fPortalDot > 0.9999f || fPortalDot < -0.9999f )
				{
					float fBump1 = CalcDistanceToLine( pptCorner[ piIntersectionIndex[ iIndex1 ] ], 
						sFitData[ piIntersectionIndex[ iIndex1 ] ].ptIntersectionPoint, 
						sFitData[ piIntersectionIndex[ iIndex1 ] ].ptIntersectionPoint + sFitData[ piIntersectionIndex[ iIndex1 ] ].vIntersectionDirection );

					fBump1 += PORTAL_BUMP_FORGIVENESS;

					float fBump2 = CalcDistanceToLine( pptCorner[ piIntersectionIndex[ iIndex2 ] ], 
						sFitData[ piIntersectionIndex[ iIndex2 ] ].ptIntersectionPoint, 
						sFitData[ piIntersectionIndex[ iIndex2 ] ].ptIntersectionPoint + sFitData[ piIntersectionIndex[ iIndex2 ] ].vIntersectionDirection );

					fBump2 += PORTAL_BUMP_FORGIVENESS;

					// Bump portal
					vOrigin += sFitData[ piIntersectionIndex[ iIndex1 ] ].vBumpDirection * fBump1;
					vOrigin += sFitData[ piIntersectionIndex[ iIndex2 ] ].vBumpDirection * fBump2;

					// Prepare recursion data
					iIntersectionCount = 0;
					sFitData[ piIntersectionIndex[ iIndex1 ] ].bCornerIntersection = false;
					sFitData[ piIntersectionIndex[ iIndex2 ] ].bCornerIntersection = false;
					sFitData[ piIntersectionIndex[ iIndex3 ] ].bCornerIntersection = false;

					return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, iPlacedBy, pTraceFilterPortalShot, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );
				}
			}

			vOrigin += FindBumpVectorInCorner( pptCorner[ piIntersectionIndex[ iIndex1 ] ], pptCorner[ piIntersectionIndex[ iIndex2 ] ], 
				sFitData[ piIntersectionIndex[ iIndex1 ] ].ptIntersectionPoint, sFitData[ piIntersectionIndex[ iIndex2 ] ].ptIntersectionPoint,
				sFitData[ piIntersectionIndex[ iIndex1 ] ].vIntersectionDirection, sFitData[ piIntersectionIndex[ iIndex2 ] ].vIntersectionDirection,
				sFitData[ piIntersectionIndex[ iIndex1 ] ].vBumpDirection, sFitData[ piIntersectionIndex[ iIndex2 ] ].vBumpDirection );

			// Prepare data for recursion
			iIntersectionCount = 0;
			sFitData[ piIntersectionIndex[ iIndex3 ] ].bCornerIntersection = false;

			return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, iPlacedBy, pTraceFilterPortalShot, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );				
		}
		break;

	default:
		{	
			if ( sFitData[ piIntersectionIndex[ 0 ] ].bSoftBump || sFitData[ piIntersectionIndex[ 1 ] ].bSoftBump || sFitData[ piIntersectionIndex[ 2 ] ].bSoftBump || sFitData[ piIntersectionIndex[ 3 ] ].bSoftBump )
			{
				// Prepare data for recursion
				iIntersectionCount = 0;
				sFitData[ piIntersectionIndex[ 0 ] ].bCornerIntersection = false;
				sFitData[ piIntersectionIndex[ 1 ] ].bCornerIntersection = false;
				sFitData[ piIntersectionIndex[ 2 ] ].bCornerIntersection = false;
				sFitData[ piIntersectionIndex[ 3 ] ].bCornerIntersection = false;

				return FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, iPlacedBy, pTraceFilterPortalShot, iRecursions + 1, sFitData, piIntersectionIndex, &iIntersectionCount );
			}
			else
			{
				// All corners intersect with no soft bumps, so it can't be fit
				return false;
			}
		}
		break;
	}

	return true;
}

void FitPortalAroundOtherPortals( const CProp_Portal *pIgnorePortal, Vector &vOrigin, const Vector &vForward, const Vector &vRight, const Vector &vUp )
{
	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if( iPortalCount != 0 )
	{
		CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
		for( int i = 0; i != iPortalCount; ++i )
		{
			CProp_Portal *pTempPortal = pPortals[i];
			if( pTempPortal != pIgnorePortal && pTempPortal->m_bActivated )
			{
				Vector vOtherOrigin = pTempPortal->GetAbsOrigin();
				QAngle qOtherAngles = pTempPortal->GetAbsAngles();

				Vector vLinkedForward;
				AngleVectors( qOtherAngles, &vLinkedForward, NULL, NULL );

				// If they're not on the same face then don't worry about overlap
				if ( vForward.Dot( vLinkedForward ) < 0.95f )
					continue;

				Vector vDiff = vOrigin - pTempPortal->GetLocalOrigin();

				Vector vDiffProjRight = vDiff.Dot( vRight ) * vRight;
				Vector vDiffProjUp = vDiff.Dot( vUp ) * vUp;

				float fProjRightLength = VectorNormalize( vDiffProjRight );
				float fProjUpLength = VectorNormalize( vDiffProjUp );

				if ( fProjRightLength < 1.0f )
				{
					vDiffProjRight = vRight;
				}

				if ( fProjUpLength < PORTAL_HALF_HEIGHT && fProjRightLength < PORTAL_HALF_WIDTH )
				{
					vOrigin += vDiffProjRight * ( PORTAL_HALF_WIDTH - fProjRightLength + 1.0f );
				}
			}
		}
	}
}

bool IsPortalIntersectingNoPortalVolume( const Vector &vOrigin, const QAngle &qAngles, const Vector &vForward )
{
	// Walk the no portal volume list, check each with box-box intersection
	for ( CFuncNoPortalVolume *pNoPortalEnt = GetNoPortalVolumeList(); pNoPortalEnt != NULL; pNoPortalEnt = pNoPortalEnt->m_pNext )
	{
		// Skip inactive no portal zones
		if ( !pNoPortalEnt->IsActive() )
		{
			continue;
		}

		Vector vMin;
		Vector vMax;
		pNoPortalEnt->GetCollideable()->WorldSpaceSurroundingBounds( &vMin, &vMax );

		Vector vBoxCenter = ( vMin + vMax ) * 0.5f;
		Vector vBoxExtents = ( vMax - vMin ) * 0.5f;

		// Take bump forgiveness into account on non major axies
		vBoxExtents += Vector( ( ( vForward.x > 0.5f || vForward.x < -0.5f ) ? ( 0.0f ) : ( -PORTAL_BUMP_FORGIVENESS ) ),
							   ( ( vForward.y > 0.5f || vForward.y < -0.5f ) ? ( 0.0f ) : ( -PORTAL_BUMP_FORGIVENESS ) ),
							   ( ( vForward.z > 0.5f || vForward.z < -0.5f ) ? ( 0.0f ) : ( -PORTAL_BUMP_FORGIVENESS ) ) );

		if ( UTIL_IsBoxIntersectingPortal( vBoxCenter, vBoxExtents, vOrigin, qAngles ) )
		{
			if ( sv_portal_placement_debug.GetBool() )
			{
				NDebugOverlay::Box( Vector( 0.0f, 0.0f, 0.0f ), vMin, vMax, 0, 255, 0, 128, 0.5f );
				UTIL_Portal_NDebugOverlay( vOrigin, qAngles, 0, 0, 255, 128, false, 0.5f );

				DevMsg( "Portal placed in no portal volume.\n" );
			}

			return true; 
		}
	}

	// Passed the list, so we didn't hit any func_noportal_volumes
	return false;
}

bool IsPortalOverlappingOtherPortals( const CProp_Portal *pIgnorePortal, const Vector &vOrigin, const QAngle &qAngles, bool bFizzle /*= false*/ )
{
	bool bOverlappedOtherPortal = false;

	Vector vForward;
	AngleVectors( qAngles, &vForward, NULL, NULL );

	Vector vPortalOBBMin = CProp_Portal_Shared::vLocalMins + Vector( 1.0f, 1.0f, 1.0f );
	Vector vPortalOBBMax = CProp_Portal_Shared::vLocalMaxs - Vector( 1.0f, 1.0f, 1.0f );

	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if( iPortalCount != 0 )
	{
		CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
		for( int i = 0; i != iPortalCount; ++i )
		{
			CProp_Portal *pTempPortal = pPortals[i];
			if( pTempPortal != pIgnorePortal && pTempPortal->m_bActivated )
			{
				Vector vOtherOrigin = pTempPortal->GetAbsOrigin();
				QAngle qOtherAngles = pTempPortal->GetAbsAngles();

				Vector vLinkedForward;
				AngleVectors( qOtherAngles, &vLinkedForward, NULL, NULL );

				// If they're not on the same face then don't worry about overlap
				if ( vForward.Dot( vLinkedForward ) < 0.95f )
					continue;

				if ( IsOBBIntersectingOBB( vOrigin, qAngles, vPortalOBBMin, vPortalOBBMax, 
										   vOtherOrigin, qOtherAngles, vPortalOBBMin, vPortalOBBMax, 0.0f ) )
				{
					if ( sv_portal_placement_debug.GetBool() )
					{
						UTIL_Portal_NDebugOverlay( vOrigin, qAngles, 0, 0, 255, 128, false, 0.5f );
						UTIL_Portal_NDebugOverlay( pTempPortal, 255, 0, 0, 128, false, 0.5f );

						DevMsg( "Portal overlapped another portal.\n" );
					}

					if ( bFizzle )
					{
						pTempPortal->DoFizzleEffect( PORTAL_FIZZLE_KILLED, false );
						pTempPortal->Fizzle();
						bOverlappedOtherPortal = true;
					}
					else
					{
						return true;
					}
				}
			}
		}
	}

	return bOverlappedOtherPortal;
}

bool IsPortalOnValidSurface( const Vector &vOrigin, const Vector &vForward, const Vector &vRight, const Vector &vUp, ITraceFilter *traceFilterPortalShot )
{
	trace_t tr;

	// Check if corners are on a no portal material
	for ( int iCorner = 0; iCorner < 5; ++iCorner )
	{
		Vector ptCorner = vOrigin;

		if ( iCorner < 4 )
		{
			if ( iCorner / 2 == 0 )
				ptCorner += vUp * ( PORTAL_HALF_HEIGHT - PORTAL_BUMP_FORGIVENESS * 1.1f ); //top
			else
				ptCorner += vUp * -( PORTAL_HALF_HEIGHT - PORTAL_BUMP_FORGIVENESS * 1.1f ); //bottom

			if ( iCorner % 2 == 0 )
				ptCorner += vRight * -( PORTAL_HALF_WIDTH - PORTAL_BUMP_FORGIVENESS * 1.1f ); //left
			else
				ptCorner += vRight * ( PORTAL_HALF_WIDTH - PORTAL_BUMP_FORGIVENESS * 1.1f ); //right
		}

		Ray_t ray;
		ray.Init( ptCorner + vForward, ptCorner - vForward );
		enginetrace->TraceRay( ray, MASK_SOLID_BRUSHONLY, traceFilterPortalShot, &tr );

		if ( tr.startsolid )
		{
			// Portal center/corner in solid
			if ( sv_portal_placement_debug.GetBool() )
			{
				DevMsg( "Portal center or corner placed inside solid.\n" );
			}

			return false;
		}

		if ( tr.fraction == 1.0f )
		{
			// Check if there's a portal bumper to act as a surface
			TraceBumpingEntities( ptCorner + vForward, ptCorner - vForward, tr );

			if ( tr.fraction == 1.0f )
			{
				// No surface behind the portal
				if ( sv_portal_placement_debug.GetBool() )
				{
					DevMsg( "Portal corner has no surface behind it.\n" );
				}

				return false;
			}
		}

		if ( tr.m_pEnt && FClassnameIs( tr.m_pEnt, "func_door" ) )
		{
			if ( sv_portal_placement_debug.GetBool() )
			{
				DevMsg( "Portal placed on func_door.\n" );
			}

			return false;
		}

		if ( IsPassThroughMaterial( tr.surface ) )
		{
			if ( sv_portal_placement_debug.GetBool() )
			{
				DevMsg( "Portal placed on a pass through material.\n" );
			}

			return false;
		}

		if ( IsNoPortalMaterial( tr.surface ) )
		{
			if ( sv_portal_placement_debug.GetBool() )
			{
				DevMsg( "Portal placed on a no portal material.\n" );
			}

			return false;
		}
	}

	return true;
}

float VerifyPortalPlacement( const CProp_Portal *pIgnorePortal, Vector &vOrigin, QAngle &qAngles, int iPlacedBy, bool bTest /*= false*/ )
{
	Vector vOriginalOrigin = vOrigin;

	Vector vForward, vRight, vUp;
	AngleVectors( qAngles, &vForward, &vRight, &vUp );

	VectorNormalize( vForward );
	VectorNormalize( vRight );
	VectorNormalize( vUp );

	trace_t tr;
	CTraceFilterSimpleClassnameList baseFilter( pIgnorePortal, COLLISION_GROUP_NONE );
	UTIL_Portal_Trace_Filter( &baseFilter );
	baseFilter.AddClassnameToIgnore( "prop_portal" );
	CTraceFilterTranslateClones traceFilterPortalShot( &baseFilter );

	// Check if center is on a surface
	Ray_t ray;
	ray.Init( vOrigin + vForward, vOrigin - vForward );
	enginetrace->TraceRay( ray, MASK_SHOT_PORTAL, &traceFilterPortalShot, &tr );

	if ( tr.fraction == 1.0f )
	{
		if ( sv_portal_placement_debug.GetBool() )
		{
			UTIL_Portal_NDebugOverlay( vOrigin, qAngles, 0, 0, 255, 128, false, 0.5f );
			DevMsg( "Portal center has no surface behind it.\n" );
		}

		return PORTAL_ANALOG_SUCCESS_INVALID_SURFACE;
	}

	// Check if the surface is moving
	Vector vVelocityCheck;
	AngularImpulse vAngularImpulseCheck;

	IPhysicsObject *pPhysicsObject = tr.m_pEnt->VPhysicsGetObject();

	if ( pPhysicsObject )
	{
		pPhysicsObject->GetVelocity( &vVelocityCheck, &vAngularImpulseCheck );
	}
	else
	{
		tr.m_pEnt->GetVelocity( &vVelocityCheck, &vAngularImpulseCheck );
	}

	if ( vVelocityCheck != vec3_origin || vAngularImpulseCheck != vec3_origin )
	{
		if ( sv_portal_placement_debug.GetBool() )
		{
			DevMsg( "Portal was on moving surface.\n" );
		}

		return PORTAL_ANALOG_SUCCESS_INVALID_SURFACE;
	}

	// Check for invalid materials
	if ( IsPassThroughMaterial( tr.surface ) )
	{
		if ( sv_portal_placement_debug.GetBool() )
		{
			UTIL_Portal_NDebugOverlay( vOrigin, qAngles, 0, 0, 255, 128, false, 0.5f );
			DevMsg( "Portal placed on a pass through material.\n" );
		}

		return PORTAL_ANALOG_SUCCESS_PASSTHROUGH_SURFACE;
	}

	if ( IsNoPortalMaterial( tr.surface ) )
	{
		if ( sv_portal_placement_debug.GetBool() )
		{
			UTIL_Portal_NDebugOverlay( vOrigin, qAngles, 0, 0, 255, 128, false, 0.5f );
			DevMsg( "Portal placed on a no portal material.\n" );
		}

		return PORTAL_ANALOG_SUCCESS_INVALID_SURFACE;
	}

	// Get pointer to liked portal if it might be in the way
	g_bBumpedByLinkedPortal = false;

	if ( iPlacedBy == PORTAL_PLACED_BY_PLAYER && !sv_portal_placement_never_bump.GetBool() )
	{
		// Bump away from linked portal so it can be fit next to it
		FitPortalAroundOtherPortals( pIgnorePortal, vOrigin, vForward, vRight, vUp );
	}

	float fBumpDistance = 0.0f;

	if ( !sv_portal_placement_never_bump.GetBool() )
	{
		// Fit onto surface and auto bump
		g_FuncBumpingEntityList.RemoveAll();

		Vector vTopEdge = vUp * ( PORTAL_HALF_HEIGHT - PORTAL_BUMP_FORGIVENESS );
		Vector vBottomEdge = -vTopEdge;
		Vector vRightEdge = vRight * ( PORTAL_HALF_WIDTH - PORTAL_BUMP_FORGIVENESS );
		Vector vLeftEdge = -vRightEdge;

		if ( !FitPortalOnSurface( pIgnorePortal, vOrigin, vForward, vRight, vTopEdge, vBottomEdge, vRightEdge, vLeftEdge, iPlacedBy, &traceFilterPortalShot ) )
		{
			if ( g_bBumpedByLinkedPortal )
			{
				return PORTAL_ANALOG_SUCCESS_OVERLAP_LINKED;
			}

			if ( sv_portal_placement_debug.GetBool() )
			{
				UTIL_Portal_NDebugOverlay( vOrigin, qAngles, 0, 0, 255, 128, false, 0.5f );
				DevMsg( "Portal was unable to fit on surface.\n" );
			}

			return PORTAL_ANALOG_SUCCESS_CANT_FIT;
		}

		// Check if it's moved too far from it's original location
		fBumpDistance = vOrigin.DistToSqr( vOriginalOrigin );

		if ( fBumpDistance > MAXIMUM_BUMP_DISTANCE )
		{
			if ( sv_portal_placement_debug.GetBool() )
			{
				UTIL_Portal_NDebugOverlay( vOrigin, qAngles, 0, 0, 255, 128, false, 0.5f );
				DevMsg( "Portal adjusted too far from it's original location.\n" );
			}

			return PORTAL_ANALOG_SUCCESS_CANT_FIT;
		}

		//if we're less than a unit from floor, we're going to bump to match it exactly and help game movement code run smoothly
		if( vUp.z > 0.7f )
		{
			Vector vSmallForward = vForward * 0.05f;
			trace_t FloorTrace;
			UTIL_TraceLine( vOrigin + vSmallForward, vOrigin + vSmallForward - (vUp * (PORTAL_HALF_HEIGHT + 1.5f)), MASK_SOLID_BRUSHONLY, &traceFilterPortalShot, &FloorTrace );
			if( FloorTrace.fraction < 1.0f )
			{
				//we hit floor in that 1 extra unit, now doublecheck to make sure we didn't hit something else
				trace_t FloorTrace_Verify;
				UTIL_TraceLine( vOrigin + vSmallForward, vOrigin + vSmallForward - (vUp * (PORTAL_HALF_HEIGHT - 0.1f)), MASK_SOLID_BRUSHONLY, &traceFilterPortalShot, &FloorTrace_Verify );
				if( FloorTrace_Verify.fraction == 1.0f )
				{
					//if we're in here, we're definitely in a floor matching configuration, bump down to match the floor better
					vOrigin = FloorTrace.endpos + (vUp * PORTAL_HALF_HEIGHT) - vSmallForward;// - vUp * PORTAL_WALL_MIN_THICKNESS;
				}
			}
		}
	}

	// Fail if it's in a no portal volume
	if ( IsPortalIntersectingNoPortalVolume( vOrigin, qAngles, vForward ) )
	{
		return PORTAL_ANALOG_SUCCESS_INVALID_VOLUME;
	}

	// Fail if it's overlapping the linked portal
	if ( bTest && IsPortalOverlappingOtherPortals( pIgnorePortal, vOrigin, qAngles ) )
	{
		return PORTAL_ANALOG_SUCCESS_OVERLAP_LINKED;
	}

	// Fail if it's on a flagged surface material
	if ( !IsPortalOnValidSurface( vOrigin, vForward, vRight, vUp, &traceFilterPortalShot ) )
	{
		if ( sv_portal_placement_debug.GetBool() )
		{
			UTIL_Portal_NDebugOverlay( vOrigin, qAngles, 0, 0, 255, 128, false, 0.5f );
		}
		return PORTAL_ANALOG_SUCCESS_INVALID_SURFACE;
	}

	float fAnalogSuccessMultiplier = 1.0f - ( fBumpDistance / MAXIMUM_BUMP_DISTANCE );
	fAnalogSuccessMultiplier *= fAnalogSuccessMultiplier;
	fAnalogSuccessMultiplier *= fAnalogSuccessMultiplier;

	return fAnalogSuccessMultiplier * ( PORTAL_ANALOG_SUCCESS_NO_BUMP - PORTAL_ANALOG_SUCCESS_BUMPED ) + PORTAL_ANALOG_SUCCESS_BUMPED;
}
