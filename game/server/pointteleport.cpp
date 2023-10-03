//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Teleports a named entity to a given position and restores
//			it's physics state
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"


#include "in_buttons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	SF_TELEPORT_TO_SPAWN_POS	0x00000001
#define	SF_TELEPORT_INTO_DUCK		0x00000002 ///< episodic only: player should be ducked after this teleport

class CPointTeleport : public CBaseEntity
{
	DECLARE_CLASS( CPointTeleport, CBaseEntity );
public:
	void	Activate( void );

	void InputTeleport( inputdata_t &inputdata );
	void InputTeleportEntity( inputdata_t &inputdata );
	void InputTeleportToCurrentPos( inputdata_t &inputdata );

	int	ObjectCaps( void )
	{ 
		return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION); 
	}

private:
	
	void	DoTeleport( inputdata_t &inputdata, const Vector &vecOrigin, const QAngle &angRotation, bool bOverrideTarget = false );
	bool	EntityMayTeleport( CBaseEntity *pTarget );

	Vector m_vSaveOrigin;
	QAngle m_vSaveAngles;

	DECLARE_DATADESC();
};


LINK_ENTITY_TO_CLASS( point_teleport, CPointTeleport );


BEGIN_DATADESC( CPointTeleport )

	DEFINE_FIELD( m_vSaveOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( m_vSaveAngles, FIELD_VECTOR ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Teleport", InputTeleport ),
	DEFINE_INPUTFUNC( FIELD_STRING, "TeleportEntity", InputTeleportEntity ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TeleportToCurrentPos", InputTeleportToCurrentPos ),

END_DATADESC()


//-----------------------------------------------------------------------------
// Returns true if the entity may be teleported
//-----------------------------------------------------------------------------
bool CPointTeleport::EntityMayTeleport( CBaseEntity *pTarget )
{
	if ( pTarget->GetMoveParent() != NULL )
	{
		// Passengers in a vehicle are allowed to teleport; their behavior handles it
		CBaseCombatCharacter *pBCC = pTarget->MyCombatCharacterPointer();
		if ( pBCC == NULL || ( pBCC != NULL && pBCC->IsInAVehicle() == false ) )
			return false;
	}

	return true;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CPointTeleport::Activate( void )
{
	// Start with our origin point
	m_vSaveOrigin = GetAbsOrigin();
	m_vSaveAngles = GetAbsAngles();

	// Save off the spawn position of the target if instructed to do so
	if ( m_spawnflags & SF_TELEPORT_TO_SPAWN_POS )
	{
		CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, m_target );
		if ( pTarget )
		{
			// If teleport object is in a movement hierarchy, remove it first
			if ( EntityMayTeleport( pTarget ) )
			{
				// Save the points
				m_vSaveOrigin = pTarget->GetAbsOrigin();
				m_vSaveAngles = pTarget->GetAbsAngles();
			}
			else
			{
				Warning("ERROR: (%s) can't teleport object (%s) as it has a parent (%s)!\n",GetDebugName(),pTarget->GetDebugName(),pTarget->GetMoveParent()->GetDebugName());
				BaseClass::Activate();
				return;
			}
		}
		else
		{
			Warning("ERROR: (%s) target '%s' not found. Deleting.\n", GetDebugName(), STRING(m_target));
			UTIL_Remove( this );
			return;
		}
	}

	BaseClass::Activate();
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CPointTeleport::InputTeleport( inputdata_t &inputdata )
{
	DoTeleport( inputdata, m_vSaveOrigin, m_vSaveAngles );
}

//------------------------------------------------------------------------------
// Purpose: Teleport the specified entity instead of the Teleporter's pre
//			determined entity.
//------------------------------------------------------------------------------
void CPointTeleport::InputTeleportEntity( inputdata_t &inputdata )
{
	DoTeleport( inputdata, m_vSaveOrigin, m_vSaveAngles, true );
}

//------------------------------------------------------------------------------
// Teleport the target to wherever the point_teleport entity is currently. The Teleport
// input teleports to the initial position of the point_teleport, so this input
// was added to avoid breaking old content.
//------------------------------------------------------------------------------
void CPointTeleport::InputTeleportToCurrentPos( inputdata_t &inputdata )
{
	if ( m_spawnflags & SF_TELEPORT_TO_SPAWN_POS )
	{
		// This is a nonsensical spawnflag in combination with this input.
		Warning( "%s: TeleportToCurrentPos input received; ignoring 'Teleport Home' spawnflag.\n", GetDebugName() );
	}

	DoTeleport( inputdata, GetAbsOrigin(), GetAbsAngles() );
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void CPointTeleport::DoTeleport( inputdata_t &inputdata, const Vector &vecOrigin, const QAngle &angRotation, bool bOverrideTarget )
{
	// Attempt to find the entity in question
	CBaseEntity *pTarget;
	if( bOverrideTarget )
	{
		// Use the inputdata to find the entity that the designer supplied in the parameter override 
		pTarget = gEntList.FindEntityByName( NULL, inputdata.value.String(), this, inputdata.pActivator, inputdata.pCaller );
	}
	else
	{
		// Default behavior: Just find the entity that I am hardwired in Hammer to teleport.
		pTarget = gEntList.FindEntityByName( NULL, m_target, this, inputdata.pActivator, inputdata.pCaller );
	}

	if ( pTarget == NULL )
		return;

	// If teleport object is in a movement hierarchy, remove it first
	if ( EntityMayTeleport( pTarget ) == false )
	{
		Warning("ERROR: (%s) can't teleport object (%s) as it has a parent (%s)!\n",GetDebugName(),pTarget->GetDebugName(),pTarget->GetMoveParent()->GetDebugName());
		return;
	}

	// in episodic, we have a special spawn flag that forces Gordon into a duck
#ifdef HL2_EPISODIC
	if ( (m_spawnflags & SF_TELEPORT_INTO_DUCK) && pTarget->IsPlayer() ) 
	{
		CBasePlayer *pPlayer = ToBasePlayer( pTarget );
		if ( pPlayer != NULL )
		{
			pPlayer->m_nButtons |= IN_DUCK;
			pPlayer->AddFlag( FL_DUCKING );
			pPlayer->m_Local.m_bDucked = true;
			pPlayer->m_Local.m_bDucking = true;
			pPlayer->m_Local.m_nDuckTimeMsecs = 0;
			pPlayer->SetViewOffset( VEC_DUCK_VIEW );
			pPlayer->SetCollisionBounds( VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
		}
	}		
#endif

	pTarget->Teleport( &vecOrigin, &angRotation, NULL );
}

