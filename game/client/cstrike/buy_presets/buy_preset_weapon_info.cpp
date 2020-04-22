//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: BuyPresetWeapon implementation, and misc knowledge relating to weapons
//
//=============================================================================

#include "cbase.h"

#include "buy_preset_debug.h"
#include "buy_presets.h"
#include "weapon_csbase.h"
#include "cs_ammodef.h"
#include "cs_gamerules.h"
#include "cstrike/bot/shared_util.h"
#include <vgui/ILocalize.h>
#include <vgui_controls/Controls.h>
#include "c_cs_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------------------------------
struct WeaponDisplayNameInfo
{
	CSWeaponID id;
	const char *displayName;
};


//--------------------------------------------------------------------------------------------------------------
// NOTE: Array must be NULL-terminated
static WeaponDisplayNameInfo weaponDisplayNameInfo[] = 
{
	{ WEAPON_P228,		"#Cstrike_TitlesTXT_P228" },
	{ WEAPON_GLOCK,		"#Cstrike_TitlesTXT_Glock18" },
	{ WEAPON_SCOUT,		"#Cstrike_TitlesTXT_Scout" },
	{ WEAPON_XM1014,	"#Cstrike_TitlesTXT_AutoShotgun" },
	{ WEAPON_MAC10,		"#Cstrike_TitlesTXT_Mac10_Short" },
	{ WEAPON_AUG,		"#Cstrike_TitlesTXT_Aug" },
	{ WEAPON_ELITE,		"#Cstrike_TitlesTXT_Beretta96G" },
	{ WEAPON_FIVESEVEN,	"#Cstrike_TitlesTXT_ESFiveSeven" },
	{ WEAPON_UMP45,		"#Cstrike_TitlesTXT_KMUMP45" },
	{ WEAPON_SG550,		"#Cstrike_TitlesTXT_SG550" },
	{ WEAPON_GALIL,		"#Cstrike_TitlesTXT_Galil" },
	{ WEAPON_FAMAS,		"#Cstrike_TitlesTXT_Famas" },
	{ WEAPON_USP,		"#Cstrike_TitlesTXT_USP45" },
	{ WEAPON_AWP,		"#Cstrike_TitlesTXT_ArcticWarfareMagnum" },
	{ WEAPON_MP5NAVY,	"#Cstrike_TitlesTXT_mp5navy" },
	{ WEAPON_M249,		"#Cstrike_TitlesTXT_ESM249" },
	{ WEAPON_M3,		"#Cstrike_TitlesTXT_Leone12" },
	{ WEAPON_M4A1,		"#Cstrike_TitlesTXT_M4A1_Short" },
	{ WEAPON_TMP,		"#Cstrike_TitlesTXT_tmp" },
	{ WEAPON_G3SG1,		"#Cstrike_TitlesTXT_G3SG1" },
	{ WEAPON_DEAGLE,	"#Cstrike_TitlesTXT_DesertEagle" },
	{ WEAPON_SG552,		"#Cstrike_TitlesTXT_SG552" },
	{ WEAPON_AK47,		"#Cstrike_TitlesTXT_AK47" },
	{ WEAPON_P90,		"#Cstrike_TitlesTXT_ESC90" },
	{ WEAPON_SHIELDGUN,	"#Cstrike_TitlesTXT_TactShield" },

	{ WEAPON_NONE,		"#Cstrike_CurrentWeapon" }
};


//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the display name for a weapon, based on it's weaponID
 */
const wchar_t* WeaponIDToDisplayName( CSWeaponID weaponID )
{
	for( int i=0; weaponDisplayNameInfo[i].displayName; ++i )
		if ( weaponDisplayNameInfo[i].id == weaponID )
			return g_pVGuiLocalize->Find( weaponDisplayNameInfo[i].displayName );

	return NULL;
}


//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
BuyPresetWeapon::BuyPresetWeapon()
{
	m_name = NULL;
	m_weaponID = WEAPON_NONE;
	m_ammoType = AMMO_CLIPS;
	m_ammoAmount = 0;
	m_fillAmmo = true;
}


//--------------------------------------------------------------------------------------------------------------
BuyPresetWeapon::BuyPresetWeapon( CSWeaponID weaponID )
{
	m_name = WeaponIDToDisplayName( weaponID );
	m_weaponID = weaponID;
	m_ammoType = AMMO_CLIPS;
	m_ammoAmount = (weaponID == WEAPON_NONE) ? 0 : 1;
	m_fillAmmo = true;
}


//--------------------------------------------------------------------------------------------------------------
BuyPresetWeapon& BuyPresetWeapon::operator= (const BuyPresetWeapon& other)
{
	m_name = other.m_name;
	m_weaponID = other.m_weaponID;
	m_ammoType = other.m_ammoType;
	m_ammoAmount = other.m_ammoAmount;
	m_fillAmmo = other.m_fillAmmo;

	return *this;
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Sets the WEAPON_* weapon ID (and resets ammo, etc)
 */
void BuyPresetWeapon::SetWeaponID( CSWeaponID weaponID )
{
	m_name = WeaponIDToDisplayName( weaponID );
	m_weaponID = weaponID;
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the number of clips that will have to be bought for the specified BuyPresetWeapon
 */
int CalcClipsNeeded( const BuyPresetWeapon *pWeapon, const CCSWeaponInfo *pInfo, const int ammo[MAX_AMMO_TYPES] )
{
	if ( !pWeapon || !pInfo )
		return 0;

	int maxRounds = GetCSAmmoDef()->MaxCarry( pInfo->iAmmoType );
	int buySize = GetCSAmmoDef()->GetBuySize( pInfo->iAmmoType );

	int numClips = 0;
	if ( buySize && pInfo->iAmmoType >= 0 )
	{
		switch ( pWeapon->GetAmmoType() )
		{
		case AMMO_CLIPS:
			numClips = pWeapon->GetAmmoAmount();
			if ( pWeapon->GetWeaponID() == WEAPON_NONE && numClips >= 4 )
			{
				numClips = ceil(maxRounds/(float)buySize);
			}
			numClips = MIN( ceil(maxRounds/(float)buySize), numClips );

			numClips -= ammo[pInfo->iAmmoType]/buySize;
			if ( numClips < 0 || ammo[pInfo->iAmmoType] == maxRounds )
			{
				numClips = 0;
			}
			break;
		case AMMO_ROUNDS:
			{
				int roundsNeeded = pWeapon->GetAmmoAmount() - ammo[pInfo->iAmmoType];
				if ( roundsNeeded > 0 )
				{
					numClips = ceil(roundsNeeded/(float)buySize);
				}
				else
				{
					numClips = 0;
				}
			}
			break;
		case AMMO_PERCENT:
			{
				int roundsNeeded = maxRounds*pWeapon->GetAmmoAmount() - ammo[pInfo->iAmmoType];
				roundsNeeded *= 0.01f;
				if ( roundsNeeded > 0 )
				{
					numClips = ceil(roundsNeeded/(float)buySize);
				}
				else
				{
					numClips = 0;
				}
			}
			break;
		}
	}
	return numClips;
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns true if the client can currently buy the weapon according to class/gamemode restrictions.  Returns
 *  true also if the player owns the weapon already.
 */
bool CanBuyWeapon( CSWeaponID currentPrimaryID, CSWeaponID currentSecondaryID, CSWeaponID weaponID )
{
	if ( currentPrimaryID == WEAPON_SHIELDGUN && weaponID == WEAPON_ELITE )
	{
		 return false;
	}

	if ( currentSecondaryID == WEAPON_ELITE && weaponID == WEAPON_SHIELDGUN )
	{
		 return false;
	}

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return false;

	CCSWeaponInfo *info = GetWeaponInfo( weaponID );
	if ( !info )
		return false;

	/// @TODO: assasination maps have a specific set of weapons that can be used in them.
	if ( info->m_iTeam != TEAM_UNASSIGNED && pPlayer->GetTeamNumber() != info->m_iTeam )
		return false;

	return true;
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns true if the CSWeaponType is a primary class
 */
static bool IsPrimaryWeaponClassID( CSWeaponType classId )
{
	switch (classId)
	{
	case WEAPONTYPE_SUBMACHINEGUN:
	case WEAPONTYPE_SHOTGUN:
	case WEAPONTYPE_MACHINEGUN:
	case WEAPONTYPE_RIFLE:
	case WEAPONTYPE_SNIPER_RIFLE:
		return true;
	}

	return false;
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns true if the weapon ID is for a primary weapon
 */
static bool IsPrimaryWeaponID( CSWeaponID id )
{
	if ( id == WEAPON_SHIELDGUN )
		return true;

	CCSWeaponInfo *info = GetWeaponInfo( id );
	if ( !info )
		return false;

	return IsPrimaryWeaponClassID( info->m_WeaponType );
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns true if the CSWeaponType is a secondary class
 */
static bool IsSecondaryWeaponClassID( CSWeaponType classId )
{
	return (classId == WEAPONTYPE_PISTOL);
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns true if the weapon ID is for a secondary weapon
 */
static bool IsSecondaryWeaponID( CSWeaponID id )
{
	if ( id == WEAPON_SHIELDGUN )
		return false;

	CCSWeaponInfo *info = GetWeaponInfo( id );
	if ( !info )
		return false;

	return IsSecondaryWeaponClassID( info->m_WeaponType );
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Fills the ammo array with the ammo currently owned by the local player
 */
void FillClientAmmo( int ammo[MAX_AMMO_TYPES] )
{
	int i;
	for ( i=0; i<MAX_AMMO_TYPES; ++i )
	{
		ammo[i] = 0;
	}

	C_CSPlayer *localPlayer = CCSPlayer::GetLocalCSPlayer();
	if ( !localPlayer )
		return;

	for ( i=0; i<WEAPON_MAX; ++i )
	{
		CSWeaponID gameWeaponID = (CSWeaponID)i;
		if ( gameWeaponID == WEAPON_NONE || gameWeaponID >= WEAPON_SHIELDGUN )
			continue;

		const CCSWeaponInfo *info = GetWeaponInfo( gameWeaponID );
		if ( !info )
			continue;

		int clientAmmoType = info->iAmmoType;
		int clientAmmoCount = localPlayer->GetAmmoCount( clientAmmoType );
		if ( clientAmmoCount > 0 )
		{
			ammo[ clientAmmoType ] = MAX( ammo[ clientAmmoType ], clientAmmoCount );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: returns the weapon in the specified slot
//-----------------------------------------------------------------------------
CWeaponCSBase *GetWeaponInSlot( int iSlot, int iSlotPos )
{
	C_CSPlayer *player = C_CSPlayer::GetLocalCSPlayer();
	if ( !player )
		return NULL;

	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		CWeaponCSBase *pWeapon = dynamic_cast< CWeaponCSBase * >(player->GetWeapon(i));
		
		if ( pWeapon == NULL )
			continue;

		if ( pWeapon->GetSlot() == iSlot && pWeapon->GetPosition() == iSlotPos )
			return pWeapon;
	}

	return NULL;
}


//--------------------------------------------------------------------------------------------------------------
/**
 *  Returns the client's WEAPON_* value for the currently owned weapon, or WEAPON_NONE if no weapon is owned
 */
CSWeaponID GetClientWeaponID( bool primary )
{
	C_CSPlayer *localPlayer = CCSPlayer::GetLocalCSPlayer();
	if ( !localPlayer )
		return WEAPON_NONE;

	int slot = (primary)?0:1;
	CWeaponCSBase *pWeapon = GetWeaponInSlot( slot, slot );
	if ( !pWeapon )
		return WEAPON_NONE;

	return pWeapon->GetWeaponID();
}

//--------------------------------------------------------------------------------------------------------------
