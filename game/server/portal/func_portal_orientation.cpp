//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//======================================================================================//

#include "cbase.h"
#include "func_portal_orientation.h"
#include "prop_portal_shared.h"
#include "portal_shareddefs.h"
#include "portal_util_shared.h"
#include "portal_placement.h"
#include "collisionutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CEntityClassList<CFuncPortalOrientation> g_FuncPortalOrientationVolumeList;
template <> CFuncPortalOrientation *CEntityClassList<CFuncPortalOrientation>::m_pClassList = NULL;

CFuncPortalOrientation* GetPortalOrientationVolumeList()
{
	return g_FuncPortalOrientationVolumeList.m_pClassList;
}

//-----------------------------------------------------------------------------
// Purpose: Test for func_orientation_volume ents which could effect the placement angles of a portal.
// Input  : vecCurAngles - Default angles to place (may change)
//			vecCurOrigin - origin of the portal on placement
//			pPortal - The portal attempting to place
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool UTIL_TestForOrientationVolumes( QAngle& vecCurAngles, const Vector& vecCurOrigin, const CProp_Portal* pPortal )
{
	if ( !pPortal )
		return false;

	// Walk list of orientation volumes, obb test each with candidate portal
	CFuncPortalOrientation *pList = g_FuncPortalOrientationVolumeList.m_pClassList;
	while ( pList )
	{
		if ( !pList->IsActive() )
		{
			pList = pList->m_pNext;
			continue;
		}

		if ( IsOBBIntersectingOBB( vecCurOrigin, vecCurAngles, CProp_Portal_Shared::vLocalMins, CProp_Portal_Shared::vLocalMaxs, 
			pList->GetAbsOrigin(), pList->GetCollideable()->GetCollisionAngles(), pList->GetCollideable()->OBBMins(), pList->GetCollideable()->OBBMaxs() ) )
		{
			QAngle vecGoalAngles;
			// Ent is marked to match angles of it's linked partner
			if ( pList->m_bMatchLinkedAngles )
			{
				// This feature requires a linked portal on a floor or ceiling. Bail without effecting
				// the placement angles if we fail those requirements.
				CProp_Portal* pLinked = pPortal->m_hLinkedPortal.Get();
				if ( !pLinked || !(AnglesAreEqual( vecCurAngles.x, -90.0f, 0.1f ) || AnglesAreEqual( vecCurAngles.x, 90.0f, 0.1f )) )
					return false;

				vecGoalAngles = pLinked->GetAbsAngles();
				vecCurAngles.y = 0.0f;
				vecCurAngles.z = vecGoalAngles.z;
			}
			// Match the angles loaded in from the map
			else
			{
				vecGoalAngles = pList->m_vecAnglesToFace;
				vecCurAngles = vecGoalAngles;
			}

			return true;
		}
		pList = pList->m_pNext;
	}

	return false;
}

LINK_ENTITY_TO_CLASS( func_portal_orientation, CFuncPortalOrientation ); 

BEGIN_DATADESC( CFuncPortalOrientation )

	DEFINE_FIELD( m_iListIndex, FIELD_INTEGER ),

	//DEFINE_FIELD ( m_pNext, CFuncPortalOrientation ),

	DEFINE_KEYFIELD( m_bDisabled, FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_KEYFIELD ( m_bMatchLinkedAngles, FIELD_BOOLEAN, "MatchLinkedAngles" ),
	DEFINE_KEYFIELD ( m_vecAnglesToFace, FIELD_VECTOR, "AnglesToFace" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),

END_DATADESC()

CFuncPortalOrientation::CFuncPortalOrientation()
{
	g_FuncPortalOrientationVolumeList.Insert( this );
}

CFuncPortalOrientation::~CFuncPortalOrientation()
{
	g_FuncPortalOrientationVolumeList.Remove( this );
}

void CFuncPortalOrientation::Spawn()
{
	BaseClass::Spawn();

	// Bind to our model, cause we need the extents for bounds checking
	SetModel( STRING( GetModelName() ) );
	SetRenderMode( kRenderNone );	// Don't draw
	SetSolid( SOLID_VPHYSICS );	// we may want slanted walls, so we'll use OBB
	AddSolidFlags( FSOLID_NOT_SOLID );
}

//-----------------------------------------------------------------------------
// Purpose: Test for portals inside our volume when we switch on, and forcibly rotate them
//-----------------------------------------------------------------------------
void CFuncPortalOrientation::OnActivate( void )
{
	if ( !GetCollideable() || m_bDisabled )
		return;

	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if( iPortalCount != 0 )
	{
		CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
		for( int i = 0; i != iPortalCount; ++i )
		{
			CProp_Portal *pTempPortal = pPortals[i];
			if( IsOBBIntersectingOBB( pTempPortal->GetAbsOrigin(), pTempPortal->GetAbsAngles(), CProp_Portal_Shared::vLocalMins, CProp_Portal_Shared::vLocalMaxs, 
			GetAbsOrigin(), GetCollideable()->GetCollisionAngles(), GetCollideable()->OBBMins(), GetCollideable()->OBBMaxs() ) )
			{
				QAngle angNewAngles;
				if ( m_bMatchLinkedAngles )
				{
					CProp_Portal* pLinked = pTempPortal->m_hLinkedPortal.Get();
					if ( !pLinked )
						return;

					angNewAngles = pTempPortal->m_hLinkedPortal->GetAbsAngles();
				}
				else
				{
					angNewAngles = m_vecAnglesToFace;
				}

				pTempPortal->PlacePortal( pTempPortal->GetAbsOrigin(), angNewAngles, PORTAL_ANALOG_SUCCESS_NO_BUMP );
			}
		}
	}
}

void CFuncPortalOrientation::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;

	OnActivate();
}

void CFuncPortalOrientation::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
}