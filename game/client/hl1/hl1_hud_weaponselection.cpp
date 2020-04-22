//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_selection.h"
#include "iclientmode.h"
#include "ammodef.h"
#include "input.h"

#include <KeyValues.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Panel.h>

#include <string.h>


#define HL1_MAX_WEAPON_SLOTS	5


//-----------------------------------------------------------------------------
// Purpose: hl2 weapon selection hud element
//-----------------------------------------------------------------------------
class CHudWeaponSelection : public CBaseHudWeaponSelection, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudWeaponSelection, vgui::Panel );

public:
	CHudWeaponSelection(const char *pElementName );

	bool ShouldDraw();

	void CycleToNextWeapon( void );
	void CycleToPrevWeapon( void );

	C_BaseCombatWeapon *GetWeaponInSlot( int iSlot, int iSlotPos );
	void SelectWeaponSlot( int iSlot );

	C_BaseCombatWeapon	*GetSelectedWeapon( void )
	{ 
		return m_hSelectedWeapon;
	}

	void VidInit( void );
	C_BaseCombatWeapon *GetNextActivePos( int iSlot, int iSlotPos );

protected:
	void Paint();
	void ApplySchemeSettings(vgui::IScheme *pScheme);

private:
	C_BaseCombatWeapon *FindNextWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition);
	C_BaseCombatWeapon *FindPrevWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition);

	void FastWeaponSwitch( int iWeaponSlot );

	void				DrawAmmoBar( C_BaseCombatWeapon *pWeapon, int x, int y, int nWidth, int nHeight );
	int					DrawBar( int x, int y, int width, int height, float f );

	void SetSelectedWeapon( C_BaseCombatWeapon *pWeapon ) 
	{ 
		m_hSelectedWeapon = pWeapon;
	}

	CHudTexture		*icon_buckets[ HL1_MAX_WEAPON_SLOTS ];
	CHudTexture		*icon_selection;
	Color			m_clrReddish;
	Color			m_clrGreenish;
};

DECLARE_HUDELEMENT( CHudWeaponSelection );

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudWeaponSelection::CHudWeaponSelection( const char *pElementName ) : CBaseHudWeaponSelection(pElementName), BaseClass(NULL, "HudWeaponSelection")
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
}

void CHudWeaponSelection::VidInit()
{
	Reset();

	for ( int i = 0; i < HL1_MAX_WEAPON_SLOTS; i++ )
	{
		char szNumString[ 10 ];

		sprintf( szNumString, "bucket%d", i );
		icon_buckets[ i ] = gHUD.GetIcon( szNumString );
	}
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
		return false;

	return ( m_bSelectionVisible ) ? true : false;
}

//-------------------------------------------------------------------------
// Purpose: draws the selection area
//-------------------------------------------------------------------------
void CHudWeaponSelection::Paint()
{
	if (!ShouldDraw())
		return;

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	// find and display our current selection
	C_BaseCombatWeapon *pSelectedWeapon = GetSelectedWeapon();
	if ( !pSelectedWeapon )
		return;

	int iActiveSlot = (pSelectedWeapon ? pSelectedWeapon->GetSlot() : -1);

	int xpos = 10;

	// iterate over all the weapon slots
	for ( int i = 0; i < HL1_MAX_WEAPON_SLOTS; i++ )
	{
		int nWidth;
		int r1, g1, b1, a1;

		(gHUD.m_clrYellowish).GetColor( r1, g1, b1, a1 );

		int ypos = 10;

		icon_buckets[ i ]->DrawSelf( xpos, ypos, Color( r1, g1, b1, 255 ) );
		
		ypos = icon_buckets[ i ]->Height() + 10;

		if ( i == iActiveSlot )
		{
			bool bFirstItem = true;
			nWidth = icon_buckets[ i ]->Width();

			for ( int slotpos = 0; slotpos < MAX_WEAPON_POSITIONS; slotpos++ )
			{
				C_BaseCombatWeapon *pWeapon = GetWeaponInSlot( i, slotpos );
				if ( !pWeapon )
					continue;

				// icons use old system, drawing in screen space
				if ( pWeapon->GetSpriteActive() )
				{
					int r, g, b, a;

					(gHUD.m_clrYellowish).GetColor( r, g, b, a );

					if (pWeapon == pSelectedWeapon)
					{
						pWeapon->GetSpriteActive()->DrawSelf( xpos, ypos, Color( r, g, b, a ) );

						if ( !icon_selection )
						{
							icon_selection = gHUD.GetIcon( "selection" );
						}

						if ( icon_selection )
						{
							icon_selection->DrawSelf( xpos, ypos, Color( r, g, b, a ) );
						}
					}
					else
					{
						if ( pWeapon->HasAmmo() )
						{
							a = 192;
						}
						else
						{
							m_clrReddish.GetColor( r, g, b, a );
							a = 128;
						}

						if ( pWeapon->GetSpriteInactive() )
						{
							pWeapon->GetSpriteInactive()->DrawSelf( xpos, ypos, Color( r, g, b, a ) );
						}
					}
				}

				// Draw Ammo Bar
				DrawAmmoBar( pWeapon, xpos + 10, ypos, 20, 4 );

				ypos += pWeapon->GetSpriteActive()->Height() + 5;

				if ( bFirstItem )
				{
					nWidth		= pWeapon->GetSpriteActive()->Width();
					bFirstItem	= false;
				}
			}
		}
		else
		{
			// Draw Row of weapons.
			for ( int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++ )
			{
				int r2, g2, b2, a2;
				C_BaseCombatWeapon *pWeapon = GetWeaponInSlot( i, iPos );
				
				if ( !pWeapon )
					continue;

				if ( pWeapon->HasAmmo() )
				{
					(gHUD.m_clrYellowish).GetColor( r2, g2, b2, a2 );
					a2 = 128;
				}
				else
				{
					m_clrReddish.GetColor( r2, g2, b2, a2 );
					a2 = 96;
				}

				Color clrBox( r2, g2, b2, a2  );
				vgui::surface()->DrawSetColor( clrBox );
				vgui::surface()->DrawFilledRect( xpos, ypos, xpos + icon_buckets[ i ]->Width(), ypos + icon_buckets[ i ]->Height() );

				ypos += icon_buckets[ i ]->Height() + 5;
			}

			nWidth = icon_buckets[ i ]->Width();
		}

		// advance position
		xpos += nWidth + 5;
	}
}

void CHudWeaponSelection::DrawAmmoBar( C_BaseCombatWeapon *pWeapon, int x, int y, int nWidth, int nHeight )
{
	if ( pWeapon->GetPrimaryAmmoType() != -1 )
	{
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();

		int nAmmoCount = pPlayer->GetAmmoCount( pWeapon->GetPrimaryAmmoType() );
		if ( !nAmmoCount )
			return;

		float flPct = (float)nAmmoCount / (float)GetAmmoDef()->MaxCarry( pWeapon->GetPrimaryAmmoType() );
		
		x = DrawBar( x, y, nWidth, nHeight, flPct );

		// Do we have secondary ammo too?
		if ( pWeapon->GetSecondaryAmmoType() != -1 )
		{
			flPct = (float)pPlayer->GetAmmoCount( pWeapon->GetSecondaryAmmoType() ) / (float)GetAmmoDef()->MaxCarry( pWeapon->GetSecondaryAmmoType() );

			x += 5; //!!!

			DrawBar( x, y, nWidth, nHeight, flPct );
		}
	}
}

int CHudWeaponSelection::DrawBar( int x, int y, int nWidth, int nHeight, float flPct )
{
	Color clrBar;
	int r, g, b, a;

	if ( flPct < 0 )
	{
		flPct = 0;
	}
	else if ( flPct > 1 )
	{
		flPct = 1;
	}

	if ( flPct )
	{
		int nBarWidth = flPct * nWidth;

		// Always show at least one pixel if we have ammo.
		if (nBarWidth <= 0)
			nBarWidth = 1;

		m_clrGreenish.GetColor( r, g, b, a );

		clrBar.SetColor( r, g, b, 255  );
		vgui::surface()->DrawSetColor( clrBar );
		vgui::surface()->DrawFilledRect( x, y, x + nBarWidth, y + nHeight );

		x += nBarWidth;
		nWidth -= nBarWidth;
	}

	(gHUD.m_clrYellowish).GetColor( r, g, b, a );

	clrBar.SetColor( r, g, b, 128  );
	vgui::surface()->DrawSetColor( clrBar );
	vgui::surface()->DrawFilledRect( x, y, x + nWidth, y + nHeight );

	return ( x + nWidth );
}

//-----------------------------------------------------------------------------
// Purpose: hud scheme settings
//-----------------------------------------------------------------------------
void CHudWeaponSelection::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(false);

	// set our size
	int screenWide, screenTall;
	int x, y;
	GetPos(x, y);
	GetHudSize(screenWide, screenTall);
	SetBounds(0, y, screenWide, screenTall - y);

	m_clrReddish	= pScheme->GetColor( "Reddish", Color( 255, 16, 16, 255 ) );
	m_clrGreenish	= pScheme->GetColor( "Greenish", Color( 255, 16, 16, 255 ) );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the next available weapon item in the weapon selection
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHudWeaponSelection::FindNextWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition)
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return NULL;

	C_BaseCombatWeapon *pNextWeapon = NULL;

	// search all the weapons looking for the closest next
	int iLowestNextSlot = HL1_MAX_WEAPON_SLOTS;
	int iLowestNextPosition = MAX_WEAPON_POSITIONS;
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		C_BaseCombatWeapon *pWeapon = pPlayer->GetWeapon(i);
		if ( !pWeapon )
			continue;

		if ( pWeapon->CanBeSelected() )
		{
			int weaponSlot = pWeapon->GetSlot(), weaponPosition = pWeapon->GetPosition();

			// see if this weapon is further ahead in the selection list
			if ( weaponSlot > iCurrentSlot || (weaponSlot == iCurrentSlot && weaponPosition > iCurrentPosition) )
			{
				// see if this weapon is closer than the current lowest
				if ( weaponSlot < iLowestNextSlot || (weaponSlot == iLowestNextSlot && weaponPosition < iLowestNextPosition) )
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
C_BaseCombatWeapon *CHudWeaponSelection::FindPrevWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition)
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return NULL;

	C_BaseCombatWeapon *pPrevWeapon = NULL;

	// search all the weapons looking for the closest next
	int iLowestPrevSlot = -1;
	int iLowestPrevPosition = -1;
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		C_BaseCombatWeapon *pWeapon = pPlayer->GetWeapon(i);
		if ( !pWeapon )
			continue;

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
		return;

	C_BaseCombatWeapon *pNextWeapon = NULL;
	if ( IsInSelectionMode() )
	{
		// find the next selection spot
		C_BaseCombatWeapon *pWeapon = GetSelectedWeapon();
		if ( !pWeapon )
			return;

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
		pNextWeapon = FindNextWeaponInWeaponSelection(-1, -1);
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
		pPlayer->EmitSound( "Player.WeaponSelectionMoveSlot" );
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
		return;

	C_BaseCombatWeapon *pNextWeapon = NULL;
	if ( IsInSelectionMode() )
	{
		// find the next selection spot
		C_BaseCombatWeapon *pWeapon = GetSelectedWeapon();
		if ( !pWeapon )
			return;

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
		pNextWeapon = FindPrevWeaponInWeaponSelection( HL1_MAX_WEAPON_SLOTS, MAX_WEAPON_POSITIONS );
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
		pPlayer->EmitSound( "Player.WeaponSelectionMoveSlot" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the weapon in the specified slot
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHudWeaponSelection::GetWeaponInSlot( int iSlot, int iSlotPos )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return NULL;

	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		C_BaseCombatWeapon *pWeapon = player->GetWeapon(i);
		
		if ( pWeapon == NULL )
			continue;

		if ( pWeapon->GetSlot() == iSlot && pWeapon->GetPosition() == iSlotPos )
			return pWeapon;
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
		return;

	// Don't try and read past our possible number of slots
	if ( iSlot > HL1_MAX_WEAPON_SLOTS )
		return;
	
	// Make sure the player's allowed to switch weapons
	if ( pPlayer->IsAllowedToSwitchWeapons() == false )
		return;

	// do a fast switch if set
	if ( hud_fastswitch.GetBool() )
	{
		FastWeaponSwitch( iSlot );
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
		SetSelectedWeapon( pActiveWeapon );
		
		if( hud_fastswitch.GetInt() > 0 )
		{
			// only one active item in bucket, so change directly to weapon
			SelectWeapon();
		}
		else if ( !IsInSelectionMode() )
		{
			// open the weapon selection
			OpenSelection();
		}
	}

	pPlayer->EmitSound( "Player.WeaponSelectionMoveSlot" );
}

//-----------------------------------------------------------------------------
// Purpose: Opens the next weapon in the slot
//-----------------------------------------------------------------------------
void CHudWeaponSelection::FastWeaponSwitch( int iWeaponSlot )
{
	// get the slot the player's weapon is in
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	// see where we should start selection
	int iPosition = -1;
	C_BaseCombatWeapon *pActiveWeapon = pPlayer->GetActiveWeapon();
	if ( pActiveWeapon && pActiveWeapon->GetSlot() == iWeaponSlot )
	{
		// start after this weapon
		iPosition = pActiveWeapon->GetPosition();
	}

	C_BaseCombatWeapon *pNextWeapon = NULL;

	// search for the weapon after the current one
	pNextWeapon = FindNextWeaponInWeaponSelection(iWeaponSlot, iPosition);
	// make sure it's in the same bucket
	if ( !pNextWeapon || pNextWeapon->GetSlot() != iWeaponSlot )
	{
		// just look for any weapon in this slot
		pNextWeapon = FindNextWeaponInWeaponSelection(iWeaponSlot, -1);
	}

	// see if we found a weapon that's different from the current and in the selected slot
	if ( pNextWeapon && pNextWeapon != pActiveWeapon && pNextWeapon->GetSlot() == iWeaponSlot )
	{
		// select the new weapon
		::input->MakeWeaponSelection( pNextWeapon );
	}
	else if ( pNextWeapon != pActiveWeapon )
	{
		// error sound
		pPlayer->EmitSound( "Player.DenyWeaponSelection" );
	}
}

C_BaseCombatWeapon *CHudWeaponSelection::GetNextActivePos( int iSlot, int iSlotPos )
{
	if ( iSlot >= HL1_MAX_WEAPON_SLOTS )
		return NULL;

	return CBaseHudWeaponSelection::GetNextActivePos( iSlot, iSlotPos );
}
