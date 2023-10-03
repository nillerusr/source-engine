//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Weapons Resource implementation
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "history_resource.h"
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>
#include "c_baseplayer.h"
#include "hud.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

WeaponsResource gWR;

void FreeHudTextureList( CUtlDict< CHudTexture *, int >& list );

static CHudTexture *FindHudTextureInDict( CUtlDict< CHudTexture *, int >& list, const char *psz )
{
	int idx = list.Find( psz );
	if ( idx == list.InvalidIndex() )
		return NULL;

	return list[ idx ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
WeaponsResource::WeaponsResource( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
WeaponsResource::~WeaponsResource( void )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void WeaponsResource::Init( void )
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void WeaponsResource::Reset( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Load all the sprites needed for all registered weapons
//-----------------------------------------------------------------------------
void WeaponsResource::LoadAllWeaponSprites( void )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		if ( player->GetWeapon(i) )
		{
			LoadWeaponSprites( player->GetWeapon(i)->GetWeaponFileInfoHandle() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void WeaponsResource::LoadWeaponSprites( WEAPON_FILE_INFO_HANDLE hWeaponFileInfo )
{
	// WeaponsResource is a friend of C_BaseCombatWeapon
	FileWeaponInfo_t *pWeaponInfo = GetFileWeaponInfoFromHandle( hWeaponFileInfo );

	if ( !pWeaponInfo )
		return;

	// Already parsed the hud elements?
	if ( pWeaponInfo->bLoadedHudElements )
		return;

	pWeaponInfo->bLoadedHudElements = true;

	pWeaponInfo->iconActive = NULL;
	pWeaponInfo->iconInactive = NULL;
	pWeaponInfo->iconAmmo = NULL;
	pWeaponInfo->iconAmmo2 = NULL;
	pWeaponInfo->iconCrosshair = NULL;
	pWeaponInfo->iconAutoaim = NULL;
	pWeaponInfo->iconSmall = NULL;

	char sz[128];
	Q_snprintf(sz, sizeof( sz ), "scripts/%s", pWeaponInfo->szClassName);

	CUtlDict< CHudTexture *, int > tempList;

	LoadHudTextures( tempList, sz, g_pGameRules->GetEncryptionKey() );

	if ( !tempList.Count() )
	{
		// no sprite description file for weapon, use default small blocks
		pWeaponInfo->iconActive = HudIcons().GetIcon( "selection" );
		pWeaponInfo->iconInactive = HudIcons().GetIcon( "selection" );
		pWeaponInfo->iconAmmo = HudIcons().GetIcon( "bucket1" );
		return;
	}

	CHudTexture *p;
	p = FindHudTextureInDict( tempList, "crosshair" );
	if ( p )
	{
		pWeaponInfo->iconCrosshair = HudIcons().AddUnsearchableHudIconToList( *p );
	}

	p = FindHudTextureInDict( tempList, "autoaim" );
	if ( p )
	{
		pWeaponInfo->iconAutoaim = HudIcons().AddUnsearchableHudIconToList( *p );
	}

	p = FindHudTextureInDict( tempList, "zoom" );
	if ( p )
	{
		pWeaponInfo->iconZoomedCrosshair = HudIcons().AddUnsearchableHudIconToList( *p );
	}
	else
	{
		pWeaponInfo->iconZoomedCrosshair = pWeaponInfo->iconCrosshair; //default to non-zoomed crosshair
	}

	p = FindHudTextureInDict( tempList, "zoom_autoaim" );
	if ( p )
	{
		pWeaponInfo->iconZoomedAutoaim = HudIcons().AddUnsearchableHudIconToList( *p );
	}
	else
	{
		pWeaponInfo->iconZoomedAutoaim = pWeaponInfo->iconZoomedCrosshair;  //default to zoomed crosshair
	}

	p = FindHudTextureInDict( tempList, "weapon" );
	if ( p )
	{
		pWeaponInfo->iconInactive = HudIcons().AddUnsearchableHudIconToList( *p );
		if ( pWeaponInfo->iconInactive )
		{
			pWeaponInfo->iconInactive->Precache();
		}
	}

	p = FindHudTextureInDict( tempList, "weapon_s" );
	if ( p )
	{
		pWeaponInfo->iconActive = HudIcons().AddUnsearchableHudIconToList( *p );
		if ( pWeaponInfo->iconActive )
		{
			pWeaponInfo->iconActive->Precache();
		}
	}

	p = FindHudTextureInDict( tempList, "weapon_small" );
	if ( p )
	{
		pWeaponInfo->iconSmall = HudIcons().AddUnsearchableHudIconToList( *p );
		if ( pWeaponInfo->iconSmall )
		{
			pWeaponInfo->iconSmall->Precache();
		}
	}

	p = FindHudTextureInDict( tempList, "ammo" );
	if ( p )
	{
		pWeaponInfo->iconAmmo = HudIcons().AddUnsearchableHudIconToList( *p );
		if ( pWeaponInfo->iconAmmo )
		{
			pWeaponInfo->iconAmmo->Precache();
		}
	}

	p = FindHudTextureInDict( tempList, "ammo2" );
	if ( p )
	{
		pWeaponInfo->iconAmmo2 = HudIcons().AddUnsearchableHudIconToList( *p );
		if ( pWeaponInfo->iconAmmo2 )
		{
			pWeaponInfo->iconAmmo2->Precache();
		}
	}

	// Blast data into all of the Huds
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		CHudHistoryResource *pHudHR = GET_HUDELEMENT( CHudHistoryResource );	
		if( !pHudHR )
			continue;

		if ( pWeaponInfo->iconInactive )
		{
			pHudHR->SetHistoryGap( pWeaponInfo->iconInactive->Height() );
		}
		if ( pWeaponInfo->iconActive )
		{
			pHudHR->SetHistoryGap( pWeaponInfo->iconActive->Height() );
		}
		if ( pWeaponInfo->iconAmmo )
		{
			pHudHR->SetHistoryGap( pWeaponInfo->iconAmmo->Height() );
		}
		if ( pWeaponInfo->iconAmmo2 )
		{
			pHudHR->SetHistoryGap( pWeaponInfo->iconAmmo2->Height() );
		}
	}

	FreeHudTextureList( tempList );
}

//-----------------------------------------------------------------------------
// Purpose: Helper function to return a Ammo pointer from id
//-----------------------------------------------------------------------------
CHudTexture *WeaponsResource::GetAmmoIconFromWeapon( int iAmmoId )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return NULL;

	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		C_BaseCombatWeapon *weapon = player->GetWeapon( i );
		if ( !weapon )
			continue;

		if ( weapon->GetPrimaryAmmoType() == iAmmoId )
		{
			return weapon->GetWpnData().iconAmmo;
		}
		else if ( weapon->GetSecondaryAmmoType() == iAmmoId )
		{
			return weapon->GetWpnData().iconAmmo2;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Get a pointer to a weapon using this ammo
//-----------------------------------------------------------------------------
const FileWeaponInfo_t *WeaponsResource::GetWeaponFromAmmo( int iAmmoId )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return NULL;

	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		C_BaseCombatWeapon *weapon = player->GetWeapon( i );
		if ( !weapon )
			continue;

		if ( weapon->GetPrimaryAmmoType() == iAmmoId )
		{
			return &weapon->GetWpnData();
		}
		else if ( weapon->GetSecondaryAmmoType() == iAmmoId )
		{
			return &weapon->GetWpnData();
		}
	}

	return NULL;
}

