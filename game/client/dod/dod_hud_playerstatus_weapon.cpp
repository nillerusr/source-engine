//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include <KeyValues.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ImagePanel.h>

#include "c_dod_team.h"
#include "c_dod_playerresource.h"
#include "c_dod_player.h"

#include "weapon_dodbase.h"

#include "dod_hud_playerstatus_weapon.h"

float GetScale( int nIconWidth, int nIconHeight, int nWidth, int nHeight );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudCurrentWeapon::Paint()
{
	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();
	if ( pPlayer )
	{
		C_WeaponDODBase *pWeapon = pPlayer->GetActiveDODWeapon();

		if ( pWeapon )
		{
			const CHudTexture *pWpnSprite = pWeapon->GetSpriteActive();

			if ( pWpnSprite )
			{
				int x, y, w, h;
				GetBounds( x, y, w, h );

				int spriteWidth = pWpnSprite->Width(), spriteHeight = pWpnSprite->Height();
				float scale = GetScale( spriteWidth, spriteHeight, w, h );

				spriteWidth *= scale;
				spriteHeight *= scale;

				int xpos = ( w / 2.0f ) - ( spriteWidth / 2.0f );
				int ypos = ( h / 2.0f ) - ( spriteHeight / 2.0f );

				pWpnSprite->DrawSelf( xpos, ypos, spriteWidth, spriteHeight, Color( 255, 255, 255, 255 ) );
			}
		}
	}
}




