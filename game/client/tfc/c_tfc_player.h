//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_TFC_PLAYER_H
#define C_TFC_PLAYER_H
#ifdef _WIN32
#pragma once
#endif


#include "tfc_playeranimstate.h"
#include "c_baseplayer.h"
#include "tfc_shareddefs.h"
#include "baseparticleentity.h"
#include "tfc_player_shared.h"


class C_TFCPlayer : public C_BasePlayer
{
public:
	DECLARE_CLASS( C_TFCPlayer, C_BasePlayer );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_INTERPOLATION();

	C_TFCPlayer();
	~C_TFCPlayer();

	static C_TFCPlayer* GetLocalTFCPlayer();

	virtual const QAngle& GetRenderAngles();
	virtual void UpdateClientSideAnimation();
	virtual void PostDataUpdate( DataUpdateType_t updateType );
	virtual void ProcessMuzzleFlashEvent();


public:
	
	CTFCPlayerShared m_Shared;


// Called by shared code.
public:

	void DoAnimationEvent( PlayerAnimEvent_t event, int nData );

	ITFCPlayerAnimState *m_PlayerAnimState;


	QAngle	m_angEyeAngles;
	CInterpolatedVar< QAngle >	m_iv_angEyeAngles;


private:
	C_TFCPlayer( const C_TFCPlayer & );
};


inline C_TFCPlayer* ToTFCPlayer( CBasePlayer *pPlayer )
{
	Assert( dynamic_cast< C_TFCPlayer* >( pPlayer ) != NULL );
	return static_cast< C_TFCPlayer* >( pPlayer );
}


#endif // C_TFC_PLAYER_H
