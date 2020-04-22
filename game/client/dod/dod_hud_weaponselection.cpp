//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_selection.h"
#include "iclientmode.h"
#include "history_resource.h"

#include <KeyValues.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/Panel.h>

#include <string.h>
#include "weapon_dodbase.h"

float GetScale( int nIconWidth, int nIconHeight, int nWidth, int nHeight );

//-----------------------------------------------------------------------------
// Purpose: cstrike weapon selection hud element
//-----------------------------------------------------------------------------
class CHudWeaponSelection : public CBaseHudWeaponSelection, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudWeaponSelection, vgui::Panel );

public:
	CHudWeaponSelection( const char *pElementName );

	virtual void VidInit();
	virtual bool ShouldDraw();
	virtual void OnWeaponPickup( C_BaseCombatWeapon *pWeapon );

	virtual void CycleToNextWeapon( void );
	virtual void CycleToPrevWeapon( void );

	virtual C_BaseCombatWeapon *GetWeaponInSlot( int iSlot, int iSlotPos );
	virtual void SelectWeaponSlot( int iSlot );

	virtual C_BaseCombatWeapon	*GetSelectedWeapon( void )
	{ 
		return m_hSelectedWeapon;
	}

	virtual void OpenSelection( void );
	virtual void HideSelection( void );

	virtual void LevelInit();

	virtual void SelectWeapon( void );
	virtual void CancelWeaponSelection( void );

	virtual bool IsHudMenuPreventingWeaponSelection() { return false; }

protected:
	virtual void OnThink();
	virtual void Paint();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	virtual bool IsWeaponSelectable()
	{ 
		if ( IsInSelectionMode() )
		{
			return true;
		}

		return false;
	}

private:
	C_BaseCombatWeapon *FindNextWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition);
	C_BaseCombatWeapon *FindPrevWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition);

	virtual	void SetSelectedWeapon( C_BaseCombatWeapon *pWeapon ) 
	{ 
		m_hSelectedWeapon = pWeapon;
	}

	void DrawBox( int x, int y, int wide, int tall, int type, int number, int valid );

private:

	int m_iBackgroundTexture;

	CPanelAnimationVarAliasType( float, m_flSmallBoxWide, "SmallBoxWide", "108", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flSmallBoxTall, "SmallBoxTall", "72", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flSmallBoxCutSize, "SmallBoxCutSize", "5", "proportional_float" );

	CPanelAnimationVarAliasType( float, m_flLargeBoxWide, "LargeBoxWide", "108", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flLargeBoxTall, "LargeBoxTall", "72", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flLargeBoxCutSize, "LargeBoxCutSize", "5", "proportional_float" );

	CPanelAnimationVarAliasType( float, m_flBoxGap, "BoxGap", "8", "proportional_float" );

	CPanelAnimationVarAliasType( float, m_flSelectionNumberXPos, "SelectionNumberXPos", "4", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flSelectionNumberYPos, "SelectionNumberYPos", "4", "proportional_float" );

	CPanelAnimationVarAliasType( float, m_flIconXPos, "IconXPos", "16", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flIconYPos, "IconYPos", "8", "proportional_float" );

	CPanelAnimationVarAliasType( float, m_flTextYPos, "TextYPos", "54", "proportional_float" );

	CPanelAnimationVar( float, m_flAlphaOverride, "Alpha", "255" );
	CPanelAnimationVar( float, m_flSelectionAlphaOverride, "SelectionAlpha", "255" );

	CPanelAnimationVar( Color, m_ActiveTextColor, "ActiveTextColor", "255 255 255 255" );
	CPanelAnimationVar( Color, m_InactiveTextColor, "InactiveTextColor", "80 80 80 255" );

	CPanelAnimationVar( vgui::HFont, m_hNumberFont, "NumberFont", "HudSelectionNumbers" );
	CPanelAnimationVar( vgui::HFont, m_hTextFont, "TextFont", "HudSelectionText" );

	CPanelAnimationVar( Color, m_TextColor, "TextColor", "SelectionTextFg" );
	CPanelAnimationVar( Color, m_NumberColor, "NumberColor", "SelectionNumberFg" );
	CPanelAnimationVar( Color, m_EmptyBoxColor, "EmptyBoxColor", "SelectionEmptyBoxBg" );
	CPanelAnimationVar( Color, m_BoxColor, "BoxColor", "SelectionBoxBg" );
	CPanelAnimationVar( Color, m_SelectedBoxColor, "SelectedBoxClor", "SelectionSelectedBoxBg" );

	CPanelAnimationVar( float, m_flWeaponPickupGrowTime, "SelectionGrowTime", "0.1" );

	CPanelAnimationVar( float, m_flTextScan, "TextScan", "1.0" );

	CPanelAnimationVar( int, m_iMaxSlots, "MaxSlots", "6" );
	CPanelAnimationVar( bool, m_bPlaySelectionSounds, "PlaySelectSounds", "1" );

	CPanelAnimationVar( Color, m_ActiveBoxColor, "ActiveBoxColor", "255 255 255 255" );
	CPanelAnimationVar( Color, m_ActiveBoxBorder, "ActiveBoxBorder", "255 255 255 255" );
	CPanelAnimationVar( Color, m_InactiveBoxColor, "InactiveBoxColor", "0 0 0 0" );
	CPanelAnimationVar( Color, m_InactiveBoxBorder, "InactiveBoxBorder", "255 255 255 255" );

	CPanelAnimationVar( Color, m_NotUseableColor, "NotUseableColor", "0 0 0 0" );
};

DECLARE_HUDELEMENT( CHudWeaponSelection );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudWeaponSelection::CHudWeaponSelection( const char *pElementName ) : CBaseHudWeaponSelection(pElementName), BaseClass(NULL, "HudWeaponSelection")
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_WEAPONSELECTION | HIDEHUD_PLAYERDEAD );

	m_iBackgroundTexture = vgui::surface()->DrawGetTextureId( "vgui/white" );
	if ( m_iBackgroundTexture == -1 )
	{
		m_iBackgroundTexture = vgui::surface()->CreateNewTextureID();
	}
	vgui::surface()->DrawSetTextureFile( m_iBackgroundTexture, "vgui/white" , true, true );
}

void CHudWeaponSelection::VidInit(void)
{
	// If we've already loaded weapons, let's get new sprites
	gWR.LoadAllWeaponSprites();

	Reset();
}
	
//-----------------------------------------------------------------------------
// Purpose: sets up display for showing weapon pickup
//-----------------------------------------------------------------------------
void CHudWeaponSelection::OnWeaponPickup( C_BaseCombatWeapon *pWeapon )
{
	// show tnt pickup panel
	C_WeaponDODBase *pDODWpn = dynamic_cast<C_WeaponDODBase *>( pWeapon );

	if ( pDODWpn && pDODWpn->GetDODWpnData().m_WeaponType == WPN_TYPE_BOMB )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "dod_tnt_pickup" );
		if ( event )
		{
			gameeventmanager->FireEventClientSide( event );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: updates animation status
//-----------------------------------------------------------------------------
void CHudWeaponSelection::OnThink()
{

}

//-----------------------------------------------------------------------------
// Purpose: returns true if the panel should draw
//-----------------------------------------------------------------------------
bool CHudWeaponSelection::ShouldDraw()
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
	{
		if ( IsInSelectionMode() )
		{
			HideSelection();
		}
		return false;
	}

	bool bret = CBaseHudWeaponSelection::ShouldDraw();
	if ( !bret )
	{
		return false;
	}

	return ( m_bSelectionVisible ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudWeaponSelection::LevelInit()
{
	CHudElement::LevelInit();
	
	m_iMaxSlots = clamp( m_iMaxSlots, 0, MAX_WEAPON_SLOTS );
}

//-------------------------------------------------------------------------
// Purpose: draws the selection area
//-------------------------------------------------------------------------
void CHudWeaponSelection::Paint(void)
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
	{
		return;
	}

	// find and display our current selection
	C_BaseCombatWeapon *pSelectedWeapon = GetSelectedWeapon();
	if ( !pSelectedWeapon )
	{
		return;
	}

	int x, y, w, h;
	GetBounds( x, y, w, h );

	int xpos = 0;
	int ypos = h;

	int nType = 0;

	Color bright( 255,255,255,255 );
		
	// iterate over all the weapon slots
	for ( int iSlot = m_iMaxSlots - 1; iSlot >=0; iSlot-- )
	{
		for( int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++ )
		{
			C_BaseCombatWeapon *pWeapon = GetWeaponInSlot(iSlot, iPos);
		
			if ( !pWeapon )
			{
				continue;
			}

			if ( !pWeapon->CanBeSelected() )
			{
				continue;
			}

			if ( pWeapon->GetSpriteActive() )
			{
				const CHudTexture *pWpnSprite = pWeapon->GetSpriteActive();
				Color finalColor;

				Assert( pWpnSprite );

				float finalBoxWide, finalBoxTall;

				if ( pWeapon == pSelectedWeapon )
				{
					finalBoxWide = m_flLargeBoxWide;
					finalBoxTall = m_flLargeBoxTall;
					finalColor = m_ActiveBoxColor;
					nType = 1;
				}
				else
				{
					finalBoxWide = m_flSmallBoxWide;
					finalBoxTall = m_flSmallBoxTall;
					finalColor = m_InactiveBoxColor;
					nType = 0;
				}

				xpos = ( w / 2.0 ) - ( finalBoxWide / 2.0f );
				ypos -= finalBoxTall;

				int sprBoxWidth = finalBoxWide - XRES(4);	// minus 4 (2 left, 2 right)
				int sprBoxHeight = finalBoxTall - YRES(4);	// minus 4 (2 top, 2 bottom)

				int sprWidth = pWpnSprite->Width();
				int sprHeight = pWpnSprite->Height();

				float scale = GetScale( sprWidth, sprHeight, sprBoxWidth, sprBoxHeight );

				sprWidth = sprWidth * scale;
				sprHeight = sprHeight * scale;

                int wpnX = xpos + ( finalBoxWide - sprWidth ) / 2;
				int wpnY = ypos + ( finalBoxTall - sprHeight ) / 2;

				bool bHasAmmo = true;

				if ( pWeapon->UsesClipsForAmmo1() )
				{
					C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();

					if ( pPlayer )
					{
						int ammo = pWeapon->Clip1() + pPlayer->GetAmmoCount( pWeapon->GetPrimaryAmmoType() );

						if ( ammo <= 0 )
						{
							bHasAmmo = false;
						}
					}
				}

				DrawBox( xpos, ypos, finalBoxWide, finalBoxTall, nType, iSlot + 1, ( pWeapon->CanDeploy() && bHasAmmo ) ) ;
				pWpnSprite->DrawSelf( wpnX, wpnY, sprWidth, sprHeight, bright );
		
				ypos -= m_flBoxGap;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: draws a selection box
//-----------------------------------------------------------------------------
void CHudWeaponSelection::DrawBox( int x, int y, int wide, int tall, int type, int number, int valid )
{
	vgui::Vertex_t verts[5];

	int nwide = wide - 1;
	int ntall = tall - 1;

	int nCutSize;

	vgui::surface()->DrawSetTexture( m_iBackgroundTexture );

	if ( type == 1 )
	{
		nCutSize = m_flLargeBoxCutSize;
		vgui::surface()->DrawSetColor( Color( m_ActiveBoxColor ) );
	}
	else
	{
		nCutSize = m_flSmallBoxCutSize;
		vgui::surface()->DrawSetColor( Color( m_InactiveBoxColor ) );
	}

	if ( !valid )
	{
		vgui::surface()->DrawSetColor( Color( m_NotUseableColor ) );
	}

	verts[0].Init( Vector2D( x, y ) );
	verts[1].Init( Vector2D( x + nwide - nCutSize, y ) );
	verts[2].Init( Vector2D( x + nwide, y + nCutSize ) );
	verts[3].Init( Vector2D( x + nwide, y + ntall ) );
	verts[4].Init( Vector2D( x, y + ntall ) );

	vgui::surface()->DrawTexturedPolygon( 5, verts );

	if ( type == 1 )
	{
		vgui::surface()->DrawSetColor( Color( m_ActiveBoxBorder ) );
	}
	else
	{
		vgui::surface()->DrawSetColor( Color( m_InactiveBoxBorder ) );
	}

	vgui::surface()->DrawTexturedPolyLine( verts, 5 );

	// draw the number
	if ( number >= 0 )
	{
		vgui::surface()->DrawSetTextFont( m_hNumberFont );

		if ( type == 1 )
		{
			vgui::surface()->DrawSetTextColor( Color( m_ActiveTextColor ) );
		}
		else
		{
			vgui::surface()->DrawSetTextColor( Color( m_InactiveTextColor ) );
		}

		wchar_t wch = '0' + number;
		vgui::surface()->DrawSetTextPos( x + m_flSelectionNumberXPos, y + ntall - m_flSelectionNumberYPos );
		vgui::surface()->DrawUnicodeChar( wch );
	}
}

//-----------------------------------------------------------------------------
// Purpose: hud scheme settings
//-----------------------------------------------------------------------------
void CHudWeaponSelection::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: Opens weapon selection control
//-----------------------------------------------------------------------------
void CHudWeaponSelection::OpenSelection( void )
{
	Assert( !IsInSelectionMode() );

	CBaseHudWeaponSelection::OpenSelection();
	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "OpenWeaponSelectionMenu" );
}

//-----------------------------------------------------------------------------
// Purpose: Closes weapon selection control
//-----------------------------------------------------------------------------
void CHudWeaponSelection::HideSelection( void )
{
	CBaseHudWeaponSelection::HideSelection();
	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "CloseWeaponSelectionMenu" );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the next available weapon item in the weapon selection
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHudWeaponSelection::FindNextWeaponInWeaponSelection( int iCurrentSlot, int iCurrentPosition )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
	{
		return NULL;
	}

	C_BaseCombatWeapon *pNextWeapon = NULL;

	// search all the weapons looking for the closest next
	int iLowestNextSlot = MAX_WEAPON_SLOTS;
	int iLowestNextPosition = MAX_WEAPON_POSITIONS;
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		C_BaseCombatWeapon *pWeapon = pPlayer->GetWeapon( i );
		if ( !pWeapon )
		{
			continue;
		}

		if ( pWeapon->CanBeSelected() )
		{
			int weaponSlot = pWeapon->GetSlot(), weaponPosition = pWeapon->GetPosition();

			// see if this weapon is further ahead in the selection list
			if ( weaponSlot > iCurrentSlot || ( weaponSlot == iCurrentSlot && weaponPosition > iCurrentPosition ) )
			{
				// see if this weapon is closer than the current lowest
				if ( weaponSlot < iLowestNextSlot || ( weaponSlot == iLowestNextSlot && weaponPosition < iLowestNextPosition ) )
				{
					iLowestNextSlot = weaponSlot;
					iLowestNextPosition = weaponPosition;
					pNextWeapon = pWeapon;
				}
			}
		}
	}

	return pNextWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the prior available weapon item in the weapon selection
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHudWeaponSelection::FindPrevWeaponInWeaponSelection( int iCurrentSlot, int iCurrentPosition )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
	{
		return NULL;
	}

	C_BaseCombatWeapon *pPrevWeapon = NULL;

	// search all the weapons looking for the closest next
	int iLowestPrevSlot = -1;
	int iLowestPrevPosition = -1;
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		C_BaseCombatWeapon *pWeapon = pPlayer->GetWeapon( i );
		if ( !pWeapon )
		{
			continue;
		}

		if ( pWeapon->CanBeSelected() )
		{
			int weaponSlot = pWeapon->GetSlot(), weaponPosition = pWeapon->GetPosition();

			// see if this weapon is further ahead in the selection list
			if ( weaponSlot < iCurrentSlot || (weaponSlot == iCurrentSlot && weaponPosition < iCurrentPosition) )
			{
				// see if this weapon is closer than the current lowest
				if ( weaponSlot > iLowestPrevSlot || (weaponSlot == iLowestPrevSlot && weaponPosition > iLowestPrevPosition) )
				{
					iLowestPrevSlot = weaponSlot;
					iLowestPrevPosition = weaponPosition;
					pPrevWeapon = pWeapon;
				}
			}
		}
	}

	return pPrevWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: Moves the selection to the next item in the menu
//-----------------------------------------------------------------------------
void CHudWeaponSelection::CycleToNextWeapon( void )
{
	// Get the local player.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
	{
		return;
	}

	C_BaseCombatWeapon *pNextWeapon = NULL;
	if ( IsInSelectionMode() )
	{
		// find the next selection spot
		C_BaseCombatWeapon *pWeapon = GetSelectedWeapon();
		if ( !pWeapon )
		{
			return;
		}

		pNextWeapon = FindNextWeaponInWeaponSelection( pWeapon->GetSlot(), pWeapon->GetPosition() );
	}
	else
	{
		// open selection at the current place
		pNextWeapon = pPlayer->GetActiveWeapon();
		if ( pNextWeapon )
		{
			pNextWeapon = FindNextWeaponInWeaponSelection( pNextWeapon->GetSlot(), pNextWeapon->GetPosition() );
		}
	}

	if ( !pNextWeapon )
	{
		// wrap around back to start
		pNextWeapon = FindNextWeaponInWeaponSelection( -1, -1 );
	}

	if ( pNextWeapon )
	{
		SetSelectedWeapon( pNextWeapon );

		if ( hud_fastswitch.GetInt() > 0 )
		{
			SelectWeapon();
		}
		else if ( !IsInSelectionMode() )
		{
			OpenSelection();
		}

		// Play the "cycle to next weapon" sound
		if ( m_bPlaySelectionSounds )
		{
			pPlayer->EmitSound( "Player.WeaponSelectionMoveSlot" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Moves the selection to the previous item in the menu
//-----------------------------------------------------------------------------
void CHudWeaponSelection::CycleToPrevWeapon( void )
{
	// Get the local player.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
	{
		return;
	}

	C_BaseCombatWeapon *pNextWeapon = NULL;
	if ( IsInSelectionMode() )
	{
		// find the next selection spot
		C_BaseCombatWeapon *pWeapon = GetSelectedWeapon();
		if ( !pWeapon )
		{
			return;
		}

		pNextWeapon = FindPrevWeaponInWeaponSelection( pWeapon->GetSlot(), pWeapon->GetPosition() );
	}
	else
	{
		// open selection at the current place
		pNextWeapon = pPlayer->GetActiveWeapon();
		if ( pNextWeapon )
		{
			pNextWeapon = FindPrevWeaponInWeaponSelection( pNextWeapon->GetSlot(), pNextWeapon->GetPosition() );
		}
	}

	if ( !pNextWeapon )
	{
		// wrap around back to end of weapon list
		pNextWeapon = FindPrevWeaponInWeaponSelection( MAX_WEAPON_SLOTS, MAX_WEAPON_POSITIONS );
	}

	if ( pNextWeapon )
	{
		SetSelectedWeapon( pNextWeapon );

		if( hud_fastswitch.GetInt() > 0 )
		{
			SelectWeapon();
		}
		else if ( !IsInSelectionMode() )
		{
			OpenSelection();
		}

		// Play the "cycle to next weapon" sound
		if( m_bPlaySelectionSounds )
		{
			pPlayer->EmitSound( "Player.WeaponSelectionMoveSlot" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the weapon in the specified slot
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHudWeaponSelection::GetWeaponInSlot( int iSlot, int iSlotPos )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
	{
		return NULL;
	}

	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		C_BaseCombatWeapon *pWeapon = player->GetWeapon( i );
		
		if ( pWeapon == NULL )
		{
			continue;
		}

		if ( pWeapon->GetSlot() == iSlot && pWeapon->GetPosition() == iSlotPos )
		{
			return pWeapon;
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Moves selection to the specified slot
//-----------------------------------------------------------------------------
void CHudWeaponSelection::SelectWeaponSlot( int iSlot )
{
	// iSlot is one higher than it should be, since it's the number key, not the 0-based index into the weapons
	--iSlot;

	// Get the local player.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
	{
		return;
	}

	// Don't try and read past our possible number of slots
	if ( iSlot > MAX_WEAPON_SLOTS )
	{
		return;
	}
	
	// Make sure the player's allowed to switch weapons
	if ( pPlayer->IsAllowedToSwitchWeapons() == false )
	{
		return;
	}

	int slotPos = 0;
	C_BaseCombatWeapon *pActiveWeapon = GetSelectedWeapon();

	// start later in the list
	if ( IsInSelectionMode() && pActiveWeapon && pActiveWeapon->GetSlot() == iSlot )
	{
		slotPos = pActiveWeapon->GetPosition() + 1;
	}

	// find the weapon in this slot
	pActiveWeapon = GetNextActivePos( iSlot, slotPos );
	if ( !pActiveWeapon )
	{
		pActiveWeapon = GetNextActivePos( iSlot, 0 );
	}
	
	if ( pActiveWeapon != NULL )
	{
		// Mark the change
		SetSelectedWeapon( pActiveWeapon );

		if( hud_fastswitch.GetInt() > 0 )
		{
			SelectWeapon();
		}
		else if ( !IsInSelectionMode() )
		{
			// open the weapon selection
			OpenSelection();
		}
	}

	if ( m_bPlaySelectionSounds )
	{
		pPlayer->EmitSound( "Player.WeaponSelectionMoveSlot" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Player has chosen to draw the currently selected weapon
//-----------------------------------------------------------------------------
void CHudWeaponSelection::SelectWeapon( void )
{
	if ( !GetSelectedWeapon() )
	{
		engine->ClientCmd( "cancelselect\n" );
		return;
	}

	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
	{
		return;
	}

	// Don't allow selections of weapons that can't be selected (out of ammo, etc)
	if ( GetSelectedWeapon()->CanBeSelected() )
	{
		SetWeaponSelected();
	
		m_hSelectedWeapon = NULL;
	
		engine->ClientCmd( "cancelselect\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Abort selecting a weapon
//-----------------------------------------------------------------------------
void CHudWeaponSelection::CancelWeaponSelection( void )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
	{
		return;
	}

	// Fastswitches happen in a single frame, so the Weapon Selection HUD Element isn't visible
	// yet, but it's going to be next frame. We need to ask it if it thinks it's going to draw,
	// instead of checking it's IsActive flag.
	if ( ShouldDraw() )
	{
		HideSelection();

		m_hSelectedWeapon = NULL;
	}
	else
	{
		engine->ClientCmd("escape");
	}
}


