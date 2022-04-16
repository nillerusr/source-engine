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
	SetPaintBackgroundEnabled( false );

	m_hNumberFont = pScheme->GetFont( "HudNumbersSmall" );
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHistoryResource::Init( void )
{
	HOOK_HUD_MESSAGE( CHudHistoryResource, ItemPickup );

	m_iHistoryGap = 0;
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHistoryResource::Reset( void )
{
	m_PickupHistory.RemoveAll();
	m_iCurrentHistorySlot = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Set a new minimum size gap between history icons
//-----------------------------------------------------------------------------
void CHudHistoryResource::SetHistoryGap( int iNewHistoryGap )
{
	if ( iNewHistoryGap > m_iHistoryGap )
	{
		m_iHistoryGap = iNewHistoryGap;
	}
}

void CHudHistoryResource::AddToHistory( C_BaseCombatWeapon *weapon )
{
	// Check to see if the pic would have to be drawn too high. If so, start again from the bottom
	if ( ((HISTORY_PICKUP_GAP * m_iCurrentHistorySlot) + HISTORY_PICKUP_PICK_HEIGHT) > HISTORY_PICKUP_HEIGHT_MAX )
	{	
		m_iCurrentHistorySlot = 0;
	}

	// ensure the size 
	m_PickupHistory.EnsureCount(m_iCurrentHistorySlot + 1);
	
	// default to just writing to the first slot
	HIST_ITEM *freeslot = &m_PickupHistory[m_iCurrentHistorySlot++];
	freeslot->type = HISTSLOT_WEAP;
	freeslot->iId = weapon->entindex();
	freeslot->m_hWeapon = weapon;
	freeslot->iCount = 0;
	freeslot->DisplayTime = gpGlobals->curtime + hud_drawhistory_time.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Add a new entry to the pickup history
//-----------------------------------------------------------------------------
void CHudHistoryResource::AddToHistory( int iType, int iId, int iCount )
{
	// Ignore adds with no count
	if ( iType == HISTSLOT_AMMO && !iCount )
		return;  

	// Check to see if the pic would have to be drawn too high. If so, start again from the bottom
	if ( ((HISTORY_PICKUP_GAP * m_iCurrentHistorySlot) + HISTORY_PICKUP_PICK_HEIGHT) > HISTORY_PICKUP_HEIGHT_MAX )
	{	
		m_iCurrentHistorySlot = 0;
	}
	
	// ensure the size 
	m_PickupHistory.EnsureCount(m_iCurrentHistorySlot + 1);
	
	// default to just writing to the first slot
	HIST_ITEM *freeslot = &m_PickupHistory[m_iCurrentHistorySlot++];
	freeslot->type = iType;
	freeslot->iId = iId;
	freeslot->m_hWeapon = NULL;
	freeslot->iCount = iCount;
	freeslot->DisplayTime = gpGlobals->curtime + hud_drawhistory_time.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Add a new entry to the pickup history
//-----------------------------------------------------------------------------
void CHudHistoryResource::AddToHistory( int iType, const char *szName, int iCount )
{
	if ( iType != HISTSLOT_ITEM )
		return;

	// Check to see if the pic would have to be drawn too high. If so, start again from the bottom
	if ( ((HISTORY_PICKUP_GAP * m_iCurrentHistorySlot) + HISTORY_PICKUP_PICK_HEIGHT) > HISTORY_PICKUP_HEIGHT_MAX )
	{
		m_iCurrentHistorySlot = 0;
	}

	// ensure the size 
	m_PickupHistory.EnsureCount(m_iCurrentHistorySlot + 1);
	
	// default to just writing to the first slot
	HIST_ITEM *freeslot = &m_PickupHistory[m_iCurrentHistorySlot++];

	// Get the item's icon
	CHudTexture *i = gHUD.GetIcon( szName );
	if ( i == NULL )
		return;  

	freeslot->iId = 1;
	freeslot->icon = i;
	freeslot->type = iType;
	freeslot->m_hWeapon  = NULL;
	freeslot->iCount = iCount;
	freeslot->DisplayTime = gpGlobals->curtime + hud_drawhistory_time.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Handle an item pickup event from the server
//-----------------------------------------------------------------------------
void CHudHistoryResource::MsgFunc_ItemPickup(bf_read &msg)
{
	char szString[2048];
	
	msg.ReadString( szString, sizeof(szString) );
	
	// Add the item to the history
	AddToHistory( HISTSLOT_ITEM, szString );
}

//-----------------------------------------------------------------------------
// Purpose: If there aren't any items in the history, clear it out.
//-----------------------------------------------------------------------------
void CHudHistoryResource::CheckClearHistory( void )
{
	for ( int i = 0; i < m_PickupHistory.Count(); i++ )
	{
		if ( m_PickupHistory[i].type )
			return;
	}

	m_iCurrentHistorySlot = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudHistoryResource::ShouldDraw( void )
{
	return ( CHudElement::ShouldDraw() && m_iCurrentHistorySlot );
}

//-----------------------------------------------------------------------------
// Purpose: Draw the pickup history
//-----------------------------------------------------------------------------
void CHudHistoryResource::Paint( void )
{
	for ( int i = 0; i < m_PickupHistory.Count(); i++ )
	{
		if ( m_PickupHistory[i].type )
		{
			m_PickupHistory[i].DisplayTime = MIN( m_PickupHistory[i].DisplayTime, gpGlobals->curtime + hud_drawhistory_time.GetFloat() );
			if ( m_PickupHistory[i].DisplayTime <= gpGlobals->curtime )
			{  // pic drawing time has expired
				memset( &m_PickupHistory[i], 0, sizeof(HIST_ITEM) );
				CheckClearHistory();
				continue;
			}

			float elapsed = m_PickupHistory[i].DisplayTime - gpGlobals->curtime;
			float scale = elapsed * 80;

			int r, g, b, nUnused;
			(gHUD.m_clrYellowish).GetColor( r, g, b, nUnused );

			Color clrAmmo( r, g, b, MIN( scale, 255 ) );

			int nHudElemWidth, nHudElemHeight;
			GetSize( nHudElemWidth, nHudElemHeight );

			switch ( m_PickupHistory[i].type )
			{
				case HISTSLOT_AMMO:
				{
					CHudTexture *icon = gWR.GetAmmoIconFromWeapon( m_PickupHistory[i].iId );
					if ( icon  )   
					{ 
						// Draw the pic
						int ypos = nHudElemHeight - ( HISTORY_PICKUP_PICK_HEIGHT + ( HISTORY_PICKUP_GAP * i ) );
						int xpos = nHudElemWidth - 24;

						// the dll has to make sure it has sent info the weapons you need
						icon->DrawSelf( xpos, ypos, clrAmmo );

						//Offset the number to sit properly next to the icon
						ypos -= ( surface()->GetFontTall( m_hNumberFont ) - icon->Height() ) / 2;

						vgui::surface()->DrawSetTextFont( m_hNumberFont );
						vgui::surface()->DrawSetTextColor( clrAmmo );
						vgui::surface()->DrawSetTextPos( GetWide() - ( ITEM_GUTTER_SIZE * 0.85f ), ypos );

						if ( m_PickupHistory[i].iCount )
						{
							char sz[ 32 ];
							int len = Q_snprintf( sz, sizeof( sz ), "%i", m_PickupHistory[i].iCount );
						
							for ( int ch = 0; ch < len; ch++ )
							{
								char c = sz[ ch ];
								vgui::surface()->DrawUnicodeChar( c );
							}
						} 
					}
				}
				break;
			case HISTSLOT_WEAP:
				{
					C_BaseCombatWeapon *pWeapon = m_PickupHistory[i].m_hWeapon;
					if ( !pWeapon )
						return;

					if ( !pWeapon->HasAmmo() )
					{
						// if the weapon doesn't have ammo, display it as red
						Color clrReddish( 255, 16, 16, 255 );
						clrReddish.GetColor( r, g, b, nUnused );
						clrAmmo.SetColor( r, g, b, MIN( scale, 255 ) );
					}

					int ypos = nHudElemHeight - (HISTORY_PICKUP_PICK_HEIGHT + (HISTORY_PICKUP_GAP * i));
					int xpos = nHudElemWidth - pWeapon->GetSpriteInactive()->Width();

					pWeapon->GetSpriteInactive()->DrawSelf( xpos, ypos, clrAmmo );
				}
				break;
			case HISTSLOT_ITEM:
				{
					if ( !m_PickupHistory[i].iId )
						continue;

					CHudTexture *icon = m_PickupHistory[i].icon;
					if ( !icon )
						continue;

					int ypos = ScreenHeight() - ( HISTORY_PICKUP_PICK_HEIGHT + ( HISTORY_PICKUP_GAP * i ) );
					int xpos = ScreenWidth() - icon->Width() - 10;

					icon->DrawSelf( xpos, ypos, clrAmmo );
				}
				break;
			default:
				{
					// Unknown history type???!!!
					Assert( 0 );
				}
				break;
			}
		}
	}
}


