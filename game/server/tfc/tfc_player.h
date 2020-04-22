//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL1.
//
// $NoKeywords: $
//=============================================================================//

#ifndef TFC_PLAYER_H
#define TFC_PLAYER_H
#pragma once


#include "player.h"
#include "server_class.h"
#include "tfc_playeranimstate.h"
#include "tfc_shareddefs.h"
#include "tfc_player_shared.h"


class CTFCPlayer;
class CTFGoal;
class CTFGoalItem;


// Function table for each player state.
class CPlayerStateInfo
{
public:
	TFCPlayerState m_iPlayerState;
	const char *m_pStateName;
	
	void (CTFCPlayer::*pfnEnterState)();	// Init and deinit the state.
	void (CTFCPlayer::*pfnLeaveState)();

	void (CTFCPlayer::*pfnThink)();			// Called every frame.
};


//=============================================================================
// >> CounterStrike player
//=============================================================================
class CTFCPlayer : public CBasePlayer
{
public:
	DECLARE_CLASS( CTFCPlayer, CBasePlayer );
	DECLARE_SERVERCLASS();


	CTFCPlayer();
	~CTFCPlayer();

	static CTFCPlayer *CreatePlayer( const char *className, edict_t *ed );
	static CTFCPlayer* Instance( int iEnt );

	// This passes the event to the client's and server's CPlayerAnimState.
	void DoAnimationEvent( PlayerAnimEvent_t event );

	virtual void PostThink();
	virtual void InitialSpawn();
	virtual void Spawn();
	virtual void Precache();
	virtual bool ClientCommand( const CCommand &args );
	virtual void ChangeTeam( int iTeamNum ) OVERRIDE;
	virtual int	TakeHealth( float flHealth, int bitsDamageType );
	virtual void Event_Killed( const CTakeDamageInfo &info );

	void ClientHearVox( const char *pSentence );
	void DisplayLocalItemStatus( CTFGoal *pGoal );
	

public:

	// Is this entity an ally (on our team)?
	bool IsAlly( CBaseEntity *pEnt ) const;

	TFCPlayerState State_Get() const;				// Get the current state.

	void TF_AddFrags( int nFrags );

	void ResetMenu();

	// On fire..
	int GetNumFlames() const;
	void SetNumFlames( int nFlames );

	void ForceRespawn();

	void TeamFortress_SetSpeed();
	void TeamFortress_CheckClassStats();
	void TeamFortress_SetSkin();
	void TeamFortress_RemoveLiveGrenades();
	void TeamFortress_RemoveRockets();
	void TeamFortress_DetpackStop( void );
	
	BOOL TeamFortress_RemoveDetpacks( void );
	void RemovePipebombs( void );
	void RemoveOwnedEnt( char *pEntName );

// SPY STUFF
public:

	void Spy_RemoveDisguise();
	void TeamFortress_SpyCalcName();
	void Spy_ResetExternalWeaponModel( void );


// ENGINEER STUFF
public:

	void Engineer_RemoveBuildings();

	// Building 
	BOOL	is_building;	 	// TRUE for an ENGINEER if they're building something
	EHANDLE building;			// The building the ENGINEER is using
	float	building_wait;		// Used to prevent using a building again immediately
	EHANDLE real_owner;
	float	has_dispenser;		// TRUE if engineer has a dispenser
	float	has_sentry;			// TRUE if engineer has a sentry
	float	has_entry_teleporter;	// TRUE if engineer has an entry teleporter
	float	has_exit_teleporter;	// TRUE if engineer has an exit teleporter


// DEMO STUFF
public:

	int		m_iPipebombCount;


public:

	// Get the class info associated with us.
	const CTFCPlayerClassInfo* GetClassInfo() const;

	// Helpers to ease porting...
	int tp_grenades_1() const { return GetClassInfo()->m_iGrenadeType1; }
	int tp_grenades_2() const { return GetClassInfo()->m_iGrenadeType2; }
	int no_grenades_1() const { return GetAmmoCount( TFC_AMMO_GRENADES1 ); }
	int no_grenades_2() const { return GetAmmoCount( TFC_AMMO_GRENADES2 ); }

	
public:
	
	CTFCPlayerShared m_Shared;

	int	    item_list;			// Used to keep track of which goalitems are 
								// affecting the player at any time.
								// GoalItems use it to keep track of their own 
								// mask to apply to a player's item_list

	float		armortype;
	//float		armorvalue; // Use CBasePlayer::m_ArmorValue.
	int			armorclass;			// Type of armor being worn
	float		armor_allowed;

	float invincible_finished;
	float invisible_finished;
	float super_damage_finished;
	float radsuit_finished;

	int	lives;				// The number of lives you have left
	int is_unableto_spy_or_teleport;

	BOOL	bRemoveGrenade; // removes the primed grenade if set

	// Replacement_Model Stuff 
	string_t	replacement_model;
	int			replacement_model_body;
	int			replacement_model_skin;
	int			replacement_model_flags;
	
	// Spy
	int		undercover_team;		// The team the Spy is pretending to be in
	int		undercover_skin;		// The skin the Spy is pretending to have
	EHANDLE	undercover_target;		// The player the Spy is pretending to be
	BOOL		is_feigning;		// TRUE for a SPY if they're feigning death
	float	immune_to_check;
	BOOL	is_undercover;		// TRUE for a SPY if they're undercover

	// TEAMFORTRESS VARIABLES
	int		no_sentry_message;
	int		no_dispenser_message;

	// teleporter variables
	int		no_entry_teleporter_message;
	int		no_exit_teleporter_message;

	BOOL	is_detpacking;	 	// TRUE for a DEMOMAN if they're setting a detpack

	float	current_menu;		// is set to the number of the current menu, is 0 if they are not in a menu

// State management.
private:

	void State_Transition( TFCPlayerState newState );
	void State_Enter( TFCPlayerState newState );
	void State_Leave();
	CPlayerStateInfo* State_LookupInfo( TFCPlayerState state );

	CPlayerStateInfo *m_pCurStateInfo;

	void State_Enter_WELCOME();
	void State_Enter_PICKINGTEAM();
	void State_Enter_PICKINGCLASS();
	void State_Enter_ACTIVE();
	void State_Enter_OBSERVER_MODE();
	void State_Enter_DYING();


private:

	friend void Bot_Think( CTFCPlayer *pBot );
	void HandleCommand_JoinTeam( const char *pTeamName );
	void HandleCommand_JoinClass( const char *pClassName );

	void GiveDefaultItems();

	void TFCPlayerThink();

	void PhysObjectSleep();
	void PhysObjectWake();

	void GetIntoGame();


private:

	// Copyed from EyeAngles() so we can send it to the client.
	CNetworkQAngle( m_angEyeAngles );

	ITFCPlayerAnimState *m_PlayerAnimState;

	int m_iLegDamage;
};


inline CTFCPlayer *ToTFCPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

#ifdef _DEBUG
	Assert( dynamic_cast<CTFCPlayer*>( pEntity ) != 0 );
#endif
	return static_cast< CTFCPlayer* >( pEntity );
}


inline const CTFCPlayerClassInfo* CTFCPlayer::GetClassInfo() const
{
	return GetTFCClassInfo( m_Shared.GetPlayerClass() );
}


#endif	// TFC_PLAYER_H
