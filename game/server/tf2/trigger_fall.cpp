//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Used at the bottom of maps where objects should fall away to infinity
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "triggers.h"

//-----------------------------------------------------------------------------
// Purpose: Used at the bottom of maps where objects should fall away to infinity
//-----------------------------------------------------------------------------
class CTriggerFall : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerFall, CBaseTrigger );
public:
	void Spawn( void );
	void FallTouch( CBaseEntity *pOther );
	
	DECLARE_DATADESC();
	
	// Outputs
	COutputEvent m_OnFallingObject;
};

BEGIN_DATADESC( CTriggerFall )

	// Function Pointers
	DEFINE_FUNCTION( FallTouch ),

	// Outputs
	DEFINE_OUTPUT( m_OnFallingObject, "OnFallingObject" ),

END_DATADESC()


LINK_ENTITY_TO_CLASS( trigger_fall, CTriggerFall );


//-----------------------------------------------------------------------------
// Purpose: Called when spawning, after keyvalues have been handled.
//-----------------------------------------------------------------------------
void CTriggerFall::Spawn( void )
{
	BaseClass::Spawn();
	InitTrigger();
	SetTouch( FallTouch );
}

//-----------------------------------------------------------------------------
// Purpose: Make the object fall away
// Input  : pOther - The entity that is touching us.
//-----------------------------------------------------------------------------
void CTriggerFall::FallTouch( CBaseEntity *pOther )
{
	// If it's a player, just kill him for now
	if ( pOther->IsPlayer() )
	{
		if ( pOther->IsAlive() == false )
			return;

		pOther->TakeDamage( CTakeDamageInfo( this, this, 200, DMG_FALL ) );
	}
	else 
	{
		// Just remove the entity
		UTIL_Remove( pOther );
	}

	// Fire our output
	m_OnFallingObject.FireOutput( pOther, this );
}
