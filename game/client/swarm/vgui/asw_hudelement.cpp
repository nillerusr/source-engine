#include "cbase.h"
#include "asw_hudelement.h"
#include "asw_gamerules.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_draw_hud;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CASW_HudElement::CASW_HudElement( const char *pElementName ) : CHudElement( pElementName )
{
}

//-----------------------------------------------------------------------------
// Purpose: Hide all the ASW hud in certain cases
//-----------------------------------------------------------------------------
bool CASW_HudElement::ShouldDraw( void )
{
	if (!CHudElement::ShouldDraw())
		return false;

	if (engine->IsLevelMainMenuBackground())
		return false;

	if (ASWGameRules())
	{
		if (ASWGameRules()->IsIntroMap() || ASWGameRules()->IsOutroMap())
			return false;
	}

	C_ASW_Player *pASWPlayer = C_ASW_Player::GetLocalASWPlayer();
	// hide things due to turret control
	if ( ( m_iHiddenBits & HIDEHUD_REMOTE_TURRET ) && (pASWPlayer && pASWPlayer->GetMarine() && pASWPlayer->GetMarine()->IsControllingTurret()) )
		return false;
	if ( ( m_iHiddenBits & HIDEHUD_PLAYERDEAD ) && ( !pASWPlayer || !pASWPlayer->GetMarine() || pASWPlayer->GetMarine()->GetHealth()<= 0) )
		return false;

	if ( !asw_draw_hud.GetBool() )
		return false;

	return true;
}