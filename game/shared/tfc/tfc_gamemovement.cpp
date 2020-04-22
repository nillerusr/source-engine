//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#include "gamemovement.h"
#include "tfc_gamerules.h"
#include "tfc_shareddefs.h"
#include "in_buttons.h"
#include "movevars_shared.h"


#ifdef CLIENT_DLL
	#include "c_tfc_player.h"
#else
	#include "tfc_player.h"
#endif


class CTFCGameMovement : public CGameMovement
{
public:
	DECLARE_CLASS( CTFCGameMovement, CGameMovement );

	CTFCGameMovement();

	virtual void ProcessMovement( CBasePlayer *pBasePlayer, CMoveData *pMove );
	virtual bool CanAccelerate();
	virtual bool CheckJumpButton();


private:
	
	CTFCPlayer *m_pTFCPlayer;
};


// Expose our interface.
static CTFCGameMovement g_GameMovement;
IGameMovement *g_pGameMovement = ( IGameMovement * )&g_GameMovement;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameMovement, IGameMovement,INTERFACENAME_GAMEMOVEMENT, g_GameMovement );


// ---------------------------------------------------------------------------------------- //
// CTFCGameMovement.
// ---------------------------------------------------------------------------------------- //

CTFCGameMovement::CTFCGameMovement()
{
	m_vecViewOffsetNormal = TFC_PLAYER_VIEW_OFFSET;
	m_pTFCPlayer = NULL;
}


void CTFCGameMovement::ProcessMovement( CBasePlayer *pBasePlayer, CMoveData *pMove )
{
	m_pTFCPlayer = ToTFCPlayer( pBasePlayer );
	Assert( m_pTFCPlayer );

	BaseClass::ProcessMovement( pBasePlayer, pMove );
}


bool CTFCGameMovement::CanAccelerate()
{
	// Only allow the player to accelerate when in certain states.
	TFCPlayerState curState = m_pTFCPlayer->m_Shared.State_Get();
	if ( curState == STATE_ACTIVE )
	{
		return player->GetWaterJumpTime() == 0;
	}
	else if ( player->IsObserver() )
	{
		return true;
	}
	else
	{	
		return false;
	}
}


bool CTFCGameMovement::CheckJumpButton()
{
	if ( BaseClass::CheckJumpButton() )
	{
		m_pTFCPlayer->DoAnimationEvent( PLAYERANIMEVENT_JUMP );
		return true;
	}
	else
	{
		return false;
	}
}

