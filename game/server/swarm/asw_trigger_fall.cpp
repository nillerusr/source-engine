//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
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
class CASW_Trigger_Fall : public CBaseTrigger
{
	DECLARE_CLASS( CASW_Trigger_Fall, CBaseTrigger );
public:
	void Spawn( void );
	void FallTouch( CBaseEntity *pOther );
	
	DECLARE_DATADESC();
	
	// Outputs
	COutputEvent m_OnFallingObject;
};

BEGIN_DATADESC( CASW_Trigger_Fall )

	// Function Pointers
	DEFINE_FUNCTION( FallTouch ),

	// Outputs
	DEFINE_OUTPUT( m_OnFallingObject, "OnFallingObject" ),

END_DATADESC()


LINK_ENTITY_TO_CLASS( asw_trigger_fall, CASW_Trigger_Fall );


//-----------------------------------------------------------------------------
// Purpose: Called when spawning, after keyvalues have been handled.
//-----------------------------------------------------------------------------
void CASW_Trigger_Fall::Spawn( void )
{
	BaseClass::Spawn();
	InitTrigger();
	SetTouch( &CASW_Trigger_Fall::FallTouch );
}

//-----------------------------------------------------------------------------
// Purpose: Make the object fall away
// Input  : pOther - The entity that is touching us.
//-----------------------------------------------------------------------------
void CASW_Trigger_Fall::FallTouch( CBaseEntity *pOther )
{
	// If it's a player, just kill him for now
	if ( pOther->IsNPC() )
	{
		if ( pOther->IsAlive() == false )
			return;

		pOther->TakeDamage( CTakeDamageInfo( this, this, 500, DMG_FALL ) );
	}
	else 
	{
		// Just remove the entity
		//UTIL_Remove( pOther );
	}

	// Fire our output
	m_OnFallingObject.FireOutput( pOther, this );
}
