//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Item pickup history displayed onscreen when items are picked up.
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "history_resource.h"
#include "hud_macros.h"
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>
#include "iclientmode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

extern ConVar hud_drawhistory_time;

#define HISTORY_PICKUP_GAP				(m_iHistoryGap + 5)
#define HISTORY_PICKUP_PICK_HEIGHT		(32 + (m_iHistoryGap * 2))
#define HISTORY_PICKUP_HEIGHT_MAX		(GetTall() - 100)
#define	ITEM_GUTTER_SIZE				48


DECLARE_HUDELEMENT( CHudHistoryResource );
DECLARE_HUD_MESSAGE( CHudHistoryResource, ItemPickup );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudHistoryResource::CHudHistoryResource( const char *pElementName ) :
	CHudElement( pElementName ), BaseClass( NULL, "HudHistoryResource" )
{	
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
	SetHiddenBits( HIDEHUD_MISCSTATUS );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pScheme - 
//-----------------------------------------------------------------------------
void CHudHistoryResource::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHistoryResource::Init( void )
{
	HOOK_HUD_MESSAGE( CHudHistoryResource, ItemPickup );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHistoryResource::Reset( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Set a new minimum size gap between history icons
//-----------------------------------------------------------------------------
void CHudHistoryResource::SetHistoryGap( int iNewHistoryGap )
{
}

void CHudHistoryResource::AddToHistory( int iType, int iId, int iCount )
{
}

void CHudHistoryResource::AddToHistory( int iType, const char *szName, int iCount )
{
}

void CHudHistoryResource::AddToHistory( C_BaseCombatWeapon *weapon )
{
}

//-----------------------------------------------------------------------------
// Purpose: Handle an item pickup event from the server
//-----------------------------------------------------------------------------
void CHudHistoryResource::MsgFunc_ItemPickup( bf_read &msg )
{
}

//-----------------------------------------------------------------------------
// Purpose: If there aren't any items in the history, clear it out.
//-----------------------------------------------------------------------------
void CHudHistoryResource::CheckClearHistory( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudHistoryResource::ShouldDraw( void )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Draw the pickup history
//-----------------------------------------------------------------------------
void CHudHistoryResource::Paint( void )
{
}


