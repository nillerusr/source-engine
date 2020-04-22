//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hud_base_account.h"
#include "c_cs_player.h"
#include "clientmode_csnormal.h"

using namespace vgui;

class CHudAccount : public CHudBaseAccount
{
public:
	DECLARE_CLASS_SIMPLE( CHudAccount, CHudBaseAccount );

	CHudAccount( const char *name );

	virtual bool ShouldDraw();
	virtual int	GetPlayerAccount( void );
	virtual vgui::AnimationController *GetAnimationController( void );
};

DECLARE_HUDELEMENT( CHudAccount );

CHudAccount::CHudAccount( const char *pName ) :
CHudBaseAccount( "HudAccount" )
{
	SetHiddenBits( HIDEHUD_PLAYERDEAD );
	SetIndent( false ); // don't indent small numbers in the drawing code - we're doing it manually
}

bool CHudAccount::ShouldDraw()
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pPlayer )
	{
		return !pPlayer->IsObserver();
	}
	else
	{
		return false;
	}
}

// How much money does the player have
int	CHudAccount::GetPlayerAccount( void )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if( !pPlayer )
		return 0;

	return (int)pPlayer->GetAccount();
}

vgui::AnimationController *CHudAccount::GetAnimationController( void )
{
	vgui::AnimationController *pController = g_pClientMode->GetViewportAnimationController();

	Assert( pController );

	return pController;
}