//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef PORTAL_PLAYERANIMSTATE_H
#define PORTAL_PLAYERANIMSTATE_H
#ifdef _WIN32
#pragma once
#endif


#include "convar.h"
#include "multiplayer_animstate.h"


#if defined( CLIENT_DLL )
	class C_Portal_Player;
	#define CPortal_Player C_Portal_Player
#else
	class CPortal_Player;
#endif

//enum PlayerAnimEvent_t
//{
//	PLAYERANIMEVENT_FIRE_GUN=0,
//	PLAYERANIMEVENT_THROW_GRENADE,
//	PLAYERANIMEVENT_ROLL_GRENADE,
//	PLAYERANIMEVENT_JUMP,
//	PLAYERANIMEVENT_RELOAD,
//	PLAYERANIMEVENT_SECONDARY_ATTACK,
//
//	PLAYERANIMEVENT_HS_NONE,
//	PLAYERANIMEVENT_CANCEL_GESTURES,	// cancel current gesture
//
//	PLAYERANIMEVENT_COUNT
//};

// ------------------------------------------------------------------------------------------------ //
// CPlayerAnimState declaration.
// ------------------------------------------------------------------------------------------------ //
class CPortalPlayerAnimState : public CMultiPlayerAnimState
{
public:
	
	DECLARE_CLASS( CPortalPlayerAnimState, CMultiPlayerAnimState );

	CPortalPlayerAnimState();
	CPortalPlayerAnimState( CBasePlayer *pPlayer, MultiPlayerMovementData_t &movementData );
	~CPortalPlayerAnimState();

	void InitPortal( CPortal_Player *pPlayer );
	CPortal_Player *GetPortalPlayer( void )							{ return m_pPortalPlayer; }

	virtual void ClearAnimationState();

	virtual Activity TranslateActivity( Activity actDesired );

	void	DoAnimationEvent( PlayerAnimEvent_t event, int nData = 0 );

	void    Teleport( const Vector *pNewOrigin, const QAngle *pNewAngles, CPortal_Player* pPlayer );

	bool	HandleMoving( Activity &idealActivity );
	bool	HandleJumping( Activity &idealActivity );
	bool	HandleDucking( Activity &idealActivity );

private:
	
	CPortal_Player   *m_pPortalPlayer;
	bool		m_bInAirWalk;

	float		m_flHoldDeployedPoseUntilTime;
};


CPortalPlayerAnimState* CreatePortalPlayerAnimState( CPortal_Player *pPlayer );


// If this is set, then the game code needs to make sure to send player animation events
// to the local player if he's the one being watched.
extern ConVar cl_showanimstate;


#endif // PORTAL_PLAYERANIMSTATE_H
