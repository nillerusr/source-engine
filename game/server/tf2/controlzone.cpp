//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Complete definition of the ControlZone behavioral entity
//
// $NoKeywords: $
//=============================================================================//

#include "tf_shareddefs.h"
#include "cbase.h"
#include "EntityOutput.h"
#include "tf_player.h"
#include "controlzone.h"
#include "team.h"

//-----------------------------------------------------------------------------
// Purpose: Since the control zone is a data only class, force it to always be sent ( shouldn't change often so )
//  bandwidth usage should be small.
// Input  : **ppSendTable - 
//			*recipient - 
//			*pvs - 
//			clientArea - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int CControlZone::UpdateTransmitState()
{
	if ( IsEffectActive( EF_NODRAW ) )
	{
		return SetTransmitState( FL_EDICT_DONTSEND );
	}
	else
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}
}

IMPLEMENT_SERVERCLASS_ST(CControlZone, DT_ControlZone)
	SendPropInt( SENDINFO(m_nZoneNumber), 8, SPROP_UNSIGNED ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( trigger_controlzone, CControlZone);

BEGIN_DATADESC( CControlZone )

	// outputs
	DEFINE_OUTPUT( m_ControllingTeam, "ControllingTeam" ),

	// inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "SetTeam", InputSetTeam ),
	DEFINE_INPUTFUNC( FIELD_VOID, "LockTeam", InputLockControllingTeam ),

	// keys 
	DEFINE_KEYFIELD_NOT_SAVED( m_iLockAfterChange, FIELD_INTEGER, "LockAfterChange" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_flTimeTillCaptured, FIELD_FLOAT, "UncontestedTime" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_flTimeTillContested, FIELD_FLOAT, "ContestedTime" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_nZoneNumber, FIELD_INTEGER, "ZoneNumber" ),

END_DATADESC()



// Control Zone Ent Flags
#define CZF_DONT_USE_TOUCHES		1

//-----------------------------------------------------------------------------
// Purpose: Initializes the control zone
//			Records who was the original controlling team (for control locking)
//-----------------------------------------------------------------------------
void CControlZone::Spawn( void )
{
	// set the starting controlling team
	m_ControllingTeam.Set( GetTeamNumber(), this, this );

	// remember who the original controlling team was (for control locking)
	m_iDefendingTeam = GetTeamNumber();

	// Solid
	SetSolid( SOLID_BSP );
	AddSolidFlags( FSOLID_TRIGGER );
	SetMoveType( MOVETYPE_NONE );
	SetModel( STRING( GetModelName() ) );    // set size and link into world

	// TF2 rules
	m_flTimeTillContested = 10.0;	// Go to contested 10 seconds after enemies enter the zone
	m_flTimeTillCaptured = 5.0;		// Go to captured state as soon as only one team holds the zone	

	if ( m_nZoneNumber == 0 )
	{
		Warning( "Warning, trigger_controlzone without Zone Number set\n" );
	}

	m_ZonePlayerList.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: Records that a player has entered the zone, and updates it's state
//			according, maybe starting to change team.
// Input  : *pOther - the entity that left the zone
//-----------------------------------------------------------------------------
void CControlZone::StartTouch( CBaseEntity *pOther )
{
	CBaseTFPlayer *pl = ToBaseTFPlayer( pOther );
	if ( !pl )
		return;

	CHandle< CBaseTFPlayer > hHandle;
	hHandle = pl;

	m_ZonePlayerList.AddToTail( hHandle );

	ReevaluateControllingTeam();

	// Set this player's current zone to this zone
	pl->SetCurrentZone( this );
}

//-----------------------------------------------------------------------------
// Purpose: Records that a player has left the zone, and updates it's state
//			according, maybe starting to change team.
// Input  : *pOther - the entity that left the zone
//-----------------------------------------------------------------------------
void CControlZone::EndTouch( CBaseEntity *pOther )
{
	CBaseTFPlayer *pl = ToBaseTFPlayer( pOther );
	if ( !pl )
		return;

	CHandle< CBaseTFPlayer > hHandle;
	hHandle = pl;
	m_ZonePlayerList.FindAndRemove( hHandle );

	ReevaluateControllingTeam();

	// Unset this player's current zone if it's this one
	if ( pl->GetCurrentZone() == this )
		pl->SetCurrentZone( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if it's time to change controllers
//-----------------------------------------------------------------------------
void CControlZone::ReevaluateControllingTeam( void )
{
	// Count the number of players in each team
	int i;
	memset( m_iPlayersInZone, 0, sizeof( m_iPlayersInZone ) );
	for ( i = 0; i < m_ZonePlayerList.Size(); i++ )
	{
		if ( m_ZonePlayerList[i] != NULL && (m_ZonePlayerList[i]->GetTeamNumber() > 0) )
		{
			m_iPlayersInZone[ m_ZonePlayerList[i]->GetTeamNumber() ] += 1;
		}
	}

	// Abort immediately if we're not using touches to changes teams
	if ( HasSpawnFlags( CZF_DONT_USE_TOUCHES ) )
		return;

	// if we're locked in place, no changes can occur to controlling team except through an explicit map ResetTeam
	if ( m_iLocked )
		return;

	bool foundAnyTeam = false;
	int teamFound = 0;

	// check to see if any teams have no players
	for ( i = 0; i < GetNumberOfTeams(); i++ )
	{
		if ( m_iPlayersInZone[i] )
		{
			if ( foundAnyTeam )
			{
				// we've already found a team, so it's being contested;
				teamFound = ZONE_CONTESTED;
				break;
			}

			foundAnyTeam = true;
			teamFound = i;			
		}
	}

	// no one in the area!
	if ( teamFound == 0 )
	{
		// just leave it as it is, let it continue to change team
		// exception: if the zone state is contested, and there aren't any players in the zone,
		// just return to the team who used to own the zone.
		if ( GetTeamNumber() == ZONE_CONTESTED )
		{
			ChangeTeam(m_iDefendingTeam);
			SetControllingTeam( this, m_iDefendingTeam );
		}

		return;
	}

	// if it's the same controlling team, don't worry about it
	if ( teamFound == GetTeamNumber() )
	{
		// the right team is in control, don't even think of switching
		m_iTryingToChangeToTeam = 0;
		SetNextThink( TICK_NEVER_THINK );
		return;
	}

	// Find out if the zone isn't owned by anyone at all (hasn't been touched since the map started, and it started un-owned)
	bool bHasBeenOwned = true;
	if ( m_iDefendingTeam == 0 && GetTeamNumber() == 0 ) 
		bHasBeenOwned = false;

	// if it's not contested, always go to contested mode 
	if ( GetTeamNumber() != ZONE_CONTESTED && teamFound != GetTeamNumber() )
	{
		// Unowned zones are captured immediately (no contesting stage)
		if ( bHasBeenOwned )
			teamFound = ZONE_CONTESTED;
	}

	// if it's the team we're trying to change to, don't worry about it
	if ( teamFound == m_iTryingToChangeToTeam )
		return;

	// set up the time to change to the new team soon
	m_iTryingToChangeToTeam = teamFound;

	// changing from contested->uncontested and visa-versa have different delays
	if ( m_iTryingToChangeToTeam != ZONE_CONTESTED )
	{
		if ( !bHasBeenOwned )
		{
			DevMsg( 1, "trigger_controlzone: (%s) changing team to %d NOW\n", GetDebugName(), m_iTryingToChangeToTeam );
			SetNextThink( gpGlobals->curtime + 0.1f );
		}
		else
		{
			DevMsg( 1, "trigger_controlzone: (%s) changing team to %d in %.2f seconds\n", GetDebugName(), m_iTryingToChangeToTeam, m_flTimeTillCaptured );
			SetNextThink( gpGlobals->curtime + m_flTimeTillCaptured );
		}
	}
	else
	{
		DevMsg( 1, "trigger_controlzone: (%s) changing to contested in %f seconds\n", GetDebugName(), m_flTimeTillContested );
		SetNextThink( gpGlobals->curtime + m_flTimeTillContested );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if an uncontested territory is ready to change state
//			to the new controlling team.
//-----------------------------------------------------------------------------
void CControlZone::Think( void )
{
	if ( m_iTryingToChangeToTeam != 0 )
	{
		// held zone long enough
		SetControllingTeam( this, m_iTryingToChangeToTeam );

		// lock against further change if set
		if ( m_iLockAfterChange )
		{
			LockControllingTeam();
		}

		// Re-evaluate controlling team if we were changing to Contested (enemy may have withdrawn)
		if ( GetTeamNumber() == ZONE_CONTESTED )
		{
			ReevaluateControllingTeam();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: set it so the team can no longer change, until a set controlling team action occurs
//-----------------------------------------------------------------------------
void CControlZone::InputLockControllingTeam( inputdata_t &inputdata )
{
	LockControllingTeam();
}


//-----------------------------------------------------------------------------
// Purpose: Input handler that sets the controlling team to the activator's team.
//-----------------------------------------------------------------------------
void CControlZone::InputSetTeam( inputdata_t &inputdata )
{
	// Abort if it's already the defending team
	if ( inputdata.pActivator->GetTeamNumber() == GetTeamNumber() )
		return;

	// set the new team
	ChangeTeam(inputdata.pActivator->GetTeamNumber());
	SetControllingTeam( inputdata.pActivator, GetTeamNumber() );
}


//-----------------------------------------------------------------------------
// Purpose: Changes the team controlling this zone
// Input  : newTeam - the new team to change to
//-----------------------------------------------------------------------------
void CControlZone::SetControllingTeam( CBaseEntity *pActivator, int newTeam )
{
	DevMsg( 1, "trigger_controlzone: (%s) changing team to: %d\n", GetDebugName(), newTeam );

	// remember this team as the defenders of the zone
	m_iDefendingTeam = GetTeamNumber();

	// reset state, firing the output
	ChangeTeam(newTeam);
	m_ControllingTeam.Set( GetTeamNumber(), pActivator, this );
	m_iLocked = FALSE;
	m_iTryingToChangeToTeam = 0;
	SetNextThink( TICK_NEVER_THINK );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CControlZone::LockControllingTeam( void )
{
	// never lock a zone in contested mode
	if ( GetTeamNumber() == ZONE_CONTESTED )
		return;
		
	// zones never lock to the defenders
	if ( GetTeamNumber() == m_iDefendingTeam )
		return;

	m_iLocked = TRUE;
}

