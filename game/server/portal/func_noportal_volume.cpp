//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A volume in which no portal can be placed. Keeps a global list loaded in from the map
//			and provides an interface with which prop_portal can get this list and avoid successfully
//			creating portals wholly or partially inside the volume.
//
// $NoKeywords: $
//======================================================================================//

#include "cbase.h"
#include "func_noportal_volume.h"
#include "prop_portal_shared.h"
#include "portal_shareddefs.h"
#include "portal_util_shared.h"
#include "collisionutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Spawnflags
#define SF_START_INACTIVE			0x01

CEntityClassList<CFuncNoPortalVolume> g_FuncNoPortalVolumeList;
template <> CFuncNoPortalVolume *CEntityClassList<CFuncNoPortalVolume>::m_pClassList = NULL;

CFuncNoPortalVolume* GetNoPortalVolumeList()
{
	return g_FuncNoPortalVolumeList.m_pClassList;
}


LINK_ENTITY_TO_CLASS( func_noportal_volume, CFuncNoPortalVolume );

BEGIN_DATADESC( CFuncNoPortalVolume )

	DEFINE_FIELD( m_bActive, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iListIndex, FIELD_INTEGER ),
	// No need to save this, its rebuilt on construct
	//DEFINE_FIELD( m_pNext, FIELD_CLASSPTR ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Deactivate",  InputDeactivate ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Activate", InputActivate ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle",  InputToggle ),

	DEFINE_FUNCTION( GetIndex ),
	DEFINE_FUNCTION( IsActive ),

END_DATADESC()


CFuncNoPortalVolume::CFuncNoPortalVolume()
{
	m_bActive = true;

	// Add me to the global list
	g_FuncNoPortalVolumeList.Insert( this );
}

CFuncNoPortalVolume::~CFuncNoPortalVolume()
{
	g_FuncNoPortalVolumeList.Remove( this );
}


void CFuncNoPortalVolume::Spawn()
{
	BaseClass::Spawn();

	if ( m_spawnflags & SF_START_INACTIVE )
	{
		m_bActive = false;
	}
	else
	{
		m_bActive = true;
	}

	// Bind to our model, cause we need the extents for bounds checking
	SetModel( STRING( GetModelName() ) );
	SetRenderMode( kRenderNone );	// Don't draw
	SetSolid( SOLID_VPHYSICS );	// we may want slanted walls, so we'll use OBB
	AddSolidFlags( FSOLID_NOT_SOLID );
}

void CFuncNoPortalVolume::OnActivate( void )
{
	if ( !GetCollideable() )
		return;

	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if( iPortalCount != 0 )
	{
		CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
		for( int i = 0; i != iPortalCount; ++i )
		{
			CProp_Portal *pTempPortal = pPortals[i];
			if( pTempPortal->m_bActivated && 
				IsOBBIntersectingOBB( pTempPortal->GetAbsOrigin(), pTempPortal->GetAbsAngles(), CProp_Portal_Shared::vLocalMins, CProp_Portal_Shared::vLocalMaxs, 
									  GetAbsOrigin(), GetCollideable()->GetCollisionAngles(), GetCollideable()->OBBMins(), GetCollideable()->OBBMaxs() ) )
			{
				pTempPortal->DoFizzleEffect( PORTAL_FIZZLE_KILLED, false );
				pTempPortal->Fizzle();
			}
		}
	}
}

void CFuncNoPortalVolume::InputActivate( inputdata_t &inputdata )
{
	m_bActive = true;

	OnActivate();
}

void CFuncNoPortalVolume::InputDeactivate( inputdata_t &inputdata )
{
	m_bActive = false;
}

void CFuncNoPortalVolume::InputToggle( inputdata_t &inputdata )
{
	m_bActive = !m_bActive;

	if ( m_bActive )
	{
		OnActivate();
	}
}

