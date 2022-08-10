//========= Copyright © 1996-2004, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================
#ifndef TF_PLAYERANIMSTATE_H
#define TF_PLAYERANIMSTATE_H
#ifdef _WIN32
#pragma once
#endif

#include "convar.h"
#include "multiplayer_animstate.h"

#if defined( CLIENT_DLL )
class C_TFPlayer;
#define CTFPlayer C_TFPlayer
#else
class CTFPlayer;
#endif

// ------------------------------------------------------------------------------------------------ //
// CPlayerAnimState declaration.
// ------------------------------------------------------------------------------------------------ //
class CTFPlayerAnimState : public CMultiPlayerAnimState
{
public:
	
	DECLARE_CLASS( CTFPlayerAnimState, CMultiPlayerAnimState );

	CTFPlayerAnimState();
	CTFPlayerAnimState( CBasePlayer *pPlayer, MultiPlayerMovementData_t &movementData );
	~CTFPlayerAnimState();

	void InitTF( CTFPlayer *pPlayer );
	CTFPlayer *GetTFPlayer( void )							{ return m_pTFPlayer; }

	virtual void ClearAnimationState();
	virtual Activity TranslateActivity( Activity actDesired );
	virtual void Update( float eyeYaw, float eyePitch );

	void	DoAnimationEvent( PlayerAnimEvent_t event, int nData = 0 );

	bool	HandleMoving( Activity &idealActivity );
	bool	HandleJumping( Activity &idealActivity );
	bool	HandleDucking( Activity &idealActivity );
	bool	HandleSwimming( Activity &idealActivity );


private:
	
	CTFPlayer   *m_pTFPlayer;
	bool		m_bInAirWalk;

	float		m_flHoldDeployedPoseUntilTime;
};

CTFPlayerAnimState *CreateTFPlayerAnimState( CTFPlayer *pPlayer );

#endif // TF_PLAYERANIMSTATE_H
