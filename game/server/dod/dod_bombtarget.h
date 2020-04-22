//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_BOMBTARGET_H
#define DOD_BOMBTARGET_H

#ifdef _WIN32
#pragma once
#endif

#include "baseanimating.h"
#include "dod_control_point.h"

#define DOD_BOMB_TARGET_MODEL_ARMED			"models/weapons/w_tnt.mdl"
#define DOD_BOMB_TARGET_MODEL_TARGET		"models/weapons/w_tnt_red.mdl"
#define DOD_BOMB_TARGET_MODEL_UNAVAILABLE	"models/weapons/w_tnt_grey.mdl"

class CDODBombTarget;

class CDODBombTargetStateInfo
{
public:
	BombTargetState m_iState;
	const char *m_pStateName;

	void (CDODBombTarget::*pfnEnterState)();	// Init and deinit the state.
	void (CDODBombTarget::*pfnLeaveState)();
	void (CDODBombTarget::*pfnThink)();	// Do a PreThink() in this state.
	void (CDODBombTarget::*pfnUse)( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};


struct DefusingPlayer 
{
	CHandle<CDODPlayer> m_pPlayer;

	float m_flDefuseTimeoutTime;
	float m_flDefuseCompleteTime;
};

class CDODBombTarget : public CBaseAnimating
{
public:
	DECLARE_CLASS( CDODBombTarget, CBaseAnimating );
	DECLARE_DATADESC();

	DECLARE_NETWORKCLASS();

	CDODBombTarget() {}

	virtual void Spawn( void );
	virtual void Precache( void );

	// Set these flags so players can use the bomb target ( for planting or defusing )
	virtual int	ObjectCaps( void );

	// Inputs
	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );

	// State Functions
	void State_Transition( BombTargetState newState );
	void State_Enter( BombTargetState newState );
	void State_Leave();
	void State_Think();
	void State_Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int State_Get( void ) { return m_iState; }

	CDODBombTargetStateInfo* State_LookupInfo( BombTargetState state );

	// Enter
	void State_Enter_INACTIVE( void );
	void State_Enter_ACTIVE( void );
	void State_Enter_ARMED( void );

	// Leave
	void State_Leave_Armed( void );

	// Think
	void State_Think_ARMED( void );

	// Use
	void State_Use_ACTIVE( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void State_Use_ARMED( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	float GetBombTimerLength( void );

	// Get the dod_control_point that we're linked to
	CControlPoint *GetControlPoint( void );

	bool CanPlantHere( CDODPlayer *pPlayer );
	void CompletePlanting( CDODPlayer *pPlantingPlayer );

	void DefuseBlocked( CDODPlayer *pAttacker );
	void PlantBlocked( CDODPlayer *pAttacker );

	int GetTimerAddSeconds( void ) { return m_iTimerAddSeconds; }

	int GetBombingTeam( void ) { return m_iBombingTeam; }

	DefusingPlayer *FindDefusingPlayer( CDODPlayer *pPlayer );
	void Explode( void );
	void BombDefused( CDODPlayer *pDefuser );
	void ResetDefuse( int index );

	bool CanPlayerStartDefuse( CDODPlayer *pPlayer );
	bool CanPlayerContinueDefusing( CDODPlayer *pPlayer, DefusingPlayer *pDefuseRecord );
private:

	// Control Point we're linked to
	string_t m_iszCapPointName;
	CControlPoint	*m_pControlPoint;

	// Outputs
	COutputEvent	m_OnBecomeActive;
	COutputEvent	m_OnBecomeInactive;
	COutputEvent	m_OnBombPlanted;
	COutputEvent	m_OnBombExploded;
	COutputEvent	m_OnBombDisarmed;

	// state
	CNetworkVar( int, m_iState );
	CDODBombTargetStateInfo *m_pCurStateInfo;
	bool m_bStartDisabled;

	// timers for armed state
	float m_flExplodeTime;
	float m_flNextAnnounce;

	// player that planted this bomb
	CHandle<CDODPlayer> m_pPlantingPlayer;

	// The team that is allowed to plant bombs here
	CNetworkVar( int, m_iBombingTeam );

	// network the model indices of active bomb so we can change per-team
	CNetworkVar( int, m_iTargetModel );
	CNetworkVar( int, m_iUnavailableModel );

	int m_iTimerAddSeconds;

	// List of defusing players and time until completion
	CUtlVector< DefusingPlayer > m_DefusingPlayers;

private:
	CDODBombTarget( const CDODBombTarget & );
};

#endif //DOD_BOMBTARGET_H