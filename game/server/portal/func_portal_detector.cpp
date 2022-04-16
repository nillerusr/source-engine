//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A volume in which no portal can be placed. Keeps a global list loaded in from the map
//			and provides an interface with which prop_portal can get this list and avoid successfully
//			creating portals wholly or partially inside the volume.
//
// $NoKeywords: $
//======================================================================================//

#include "cbase.h"
#include "func_portal_detector.h"
#include "prop_portal_shared.h"
#include "portal_shareddefs.h"
#include "portal_util_shared.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Spawnflags
#define SF_START_INACTIVE			0x01


LINK_ENTITY_TO_CLASS( func_portal_detector, CFuncPortalDetector );

BEGIN_DATADESC( CFuncPortalDetector )

	DEFINE_FIELD( m_bActive, FIELD_BOOLEAN ),
	DEFINE_KEYFIELD( m_iLinkageGroupID, FIELD_INTEGER, "LinkageGroupID" ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),

	DEFINE_OUTPUT( m_OnStartTouchPortal1, "OnStartTouchPortal1" ),
	DEFINE_OUTPUT( m_OnStartTouchPortal2, "OnStartTouchPortal2" ),
	DEFINE_OUTPUT( m_OnStartTouchLinkedPortal, "OnStartTouchLinkedPortal" ),
	DEFINE_OUTPUT( m_OnStartTouchBothLinkedPortals, "OnStartTouchBothLinkedPortals" ),

	DEFINE_FUNCTION( IsActive ),

END_DATADESC()


void CFuncPortalDetector::Spawn()
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
	SetSolid( SOLID_VPHYSICS );		// we may want slanted walls, so we'll use OBB
	AddSolidFlags( FSOLID_NOT_SOLID );
}

void CFuncPortalDetector::OnActivate( void )
{
	Vector vMin, vMax;
	CollisionProp()->WorldSpaceAABB( &vMin, &vMax );

	Vector vBoxCenter = ( vMin + vMax ) * 0.5f;
	Vector vBoxExtents = ( vMax - vMin ) * 0.5f;

	bool bTouchedPortal1 = false;
	bool bTouchedPortal2 = false;

	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if( iPortalCount != 0 )
	{
		CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
		for( int i = 0; i != iPortalCount; ++i )
		{
			CProp_Portal *pTempPortal = pPortals[i];

			//require that it's active and/or linked?

			if( pTempPortal->GetLinkageGroup() == m_iLinkageGroupID && UTIL_IsBoxIntersectingPortal( vBoxCenter, vBoxExtents, pTempPortal ) )
			{
				if( pTempPortal->IsPortal2() )
				{
					m_OnStartTouchPortal2.FireOutput( pTempPortal, this );

					if ( pTempPortal->IsActivedAndLinked() )
					{
						bTouchedPortal2 = true;
						m_OnStartTouchLinkedPortal.FireOutput( pTempPortal, this );
					}
				}
				else
				{
					m_OnStartTouchPortal1.FireOutput( pTempPortal, this );

					if ( pTempPortal->IsActivedAndLinked() )
					{
						bTouchedPortal1 = true;
						m_OnStartTouchLinkedPortal.FireOutput( pTempPortal, this );
					}
				}
			}
		}
	}

	if ( bTouchedPortal1 && bTouchedPortal2 )
	{
		m_OnStartTouchBothLinkedPortals.FireOutput( this, this );
	}
}

void CFuncPortalDetector::InputDisable( inputdata_t &inputdata )
{
	m_bActive = false;
}

void CFuncPortalDetector::InputEnable( inputdata_t &inputdata )
{
	m_bActive = true;

	OnActivate();
}

void CFuncPortalDetector::InputToggle( inputdata_t &inputdata )
{
	m_bActive = !m_bActive;

	if ( m_bActive )
	{
		OnActivate();
	}
}
