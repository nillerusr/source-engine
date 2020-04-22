//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A volume which bumps portal placement. Keeps a global list loaded in from the map
//			and provides an interface with which prop_portal can get this list and avoid successfully
//			creating portals partially inside the volume.
//
// $NoKeywords: $
//======================================================================================//

#include "cbase.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// Spawnflags
#define SF_START_INACTIVE			0x01


class CFuncPortalBumper : public CBaseEntity
{
public:
	DECLARE_CLASS( CFuncPortalBumper, CBaseEntity );

	CFuncPortalBumper();

	// Overloads from base entity
	virtual void	Spawn( void );

	// Inputs to flip functionality on and off
	void InputActivate( inputdata_t &inputdata );
	void InputDeactivate( inputdata_t &inputdata );
	void InputToggle( inputdata_t &inputdata );

	// misc public methods
	bool IsActive() { return m_bActive; }	// is this area currently bumping portals

	DECLARE_DATADESC();

private:
	bool					m_bActive;			// are we currently blocking portals


};


LINK_ENTITY_TO_CLASS( func_portal_bumper, CFuncPortalBumper );

BEGIN_DATADESC( CFuncPortalBumper )

	DEFINE_FIELD( m_bActive, FIELD_BOOLEAN ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Deactivate",  InputDeactivate ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Activate", InputActivate ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle",  InputToggle ),

	DEFINE_FUNCTION( IsActive ),

END_DATADESC()


CFuncPortalBumper::CFuncPortalBumper()
{
	m_bActive = true;
}

void CFuncPortalBumper::Spawn()
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

void CFuncPortalBumper::InputActivate( inputdata_t &inputdata )
{
	m_bActive = true;
}

void CFuncPortalBumper::InputDeactivate( inputdata_t &inputdata )
{
	m_bActive = false;
}

void CFuncPortalBumper::InputToggle( inputdata_t &inputdata )
{
	m_bActive = !m_bActive;
}

