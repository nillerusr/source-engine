//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Control Zone entity
//
// $NoKeywords: $
//=============================================================================//

#ifndef CONTROLZONE_H
#define CONTROLZONE_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Defines a team zone of control
//			Usually the parent of many trigger entities
//-----------------------------------------------------------------------------
class CControlZone : public CBaseEntity
{
public:
	DECLARE_CLASS( CControlZone, CBaseEntity );
	DECLARE_SERVERCLASS();

	void Spawn( void );
	void StartTouch( CBaseEntity * );
	void EndTouch( CBaseEntity * );
	void Think( void );

	virtual int UpdateTransmitState();

	// input functions
	void InputLockControllingTeam( inputdata_t &inputdata );
	void InputSetTeam( inputdata_t &inputdata );

	// internal methods
	void LockControllingTeam( void );
	void ReevaluateControllingTeam( void );
	void SetControllingTeam( CBaseEntity *pActivator, int newTeam );

	// outputs
	int GetControllingTeam( void ) { return m_ControllingTeam.Get(); };
	COutputInt m_ControllingTeam; // outputs the team currently controlling this spot, whenever it changes - this is -1 when contended

public:
	// Data
	CNetworkVar( int, m_nZoneNumber );
	int m_iDefendingTeam;				// the original defeind team
	int m_iLocked;						// no more changes, until a reset it called
	int m_iLockAfterChange;				// auto-lock after the control zone changes hands through combat
	float m_flTimeTillCaptured;			// time that the control zone has to be uncontested for it to succesfully change teams
	float m_flTimeTillContested;		// time that the control zone has to be contested for for it to change to Contested mode (no team)
	int m_iTryingToChangeToTeam;		// the team is trying to change to
	
	CUtlVector< CHandle<CBaseTFPlayer> > m_ZonePlayerList; // List of all players in the zone at the moment
	int m_iPlayersInZone[MAX_TF_TEAMS+1];	// count of players in the zone divided by team
	
	DECLARE_DATADESC();
};

#endif // CONTROLZONE_H
