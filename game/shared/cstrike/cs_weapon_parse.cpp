//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include <KeyValues.h>
#include "cs_weapon_parse.h"
#include "cs_shareddefs.h"
#include "weapon_csbase.h"
#include "icvar.h"
#include "cs_gamerules.h"
#include "cs_blackmarket.h"


//--------------------------------------------------------------------------------------------------------
struct WeaponTypeInfo
{
	CSWeaponType type;
	const char * name;
};


//--------------------------------------------------------------------------------------------------------
WeaponTypeInfo s_weaponTypeInfo[] =
{
	{ WEAPONTYPE_KNIFE,			"Knife" },
	{ WEAPONTYPE_PISTOL,		"Pistol" },
	{ WEAPONTYPE_SUBMACHINEGUN,	"Submachine Gun" },	// First match is printable
	{ WEAPONTYPE_SUBMACHINEGUN,	"submachinegun" },
	{ WEAPONTYPE_SUBMACHINEGUN,	"smg" },
	{ WEAPONTYPE_RIFLE,			"Rifle" },
	{ WEAPONTYPE_SHOTGUN,		"Shotgun" },
	{ WEAPONTYPE_SNIPER_RIFLE,	"Sniper Rifle" },	// First match is printable
	{ WEAPONTYPE_SNIPER_RIFLE,	"SniperRifle" },
	{ WEAPONTYPE_MACHINEGUN,	"Machine Gun" },		// First match is printable
	{ WEAPONTYPE_MACHINEGUN,	"machinegun" },
	{ WEAPONTYPE_MACHINEGUN,	"mg" },
	{ WEAPONTYPE_C4,			"C4" },
	{ WEAPONTYPE_GRENADE,		"Grenade" },
};


struct WeaponNameInfo
{
	CSWeaponID id;
	const char *name;
};

WeaponNameInfo s_weaponNameInfo[] =
{
	{ WEAPON_P228,				"weapon_p228" },
	{ WEAPON_GLOCK,				"weapon_glock" },
	{ WEAPON_SCOUT,				"weapon_scout" },
	{ WEAPON_HEGRENADE,			"weapon_hegrenade" },
	{ WEAPON_XM1014,			"weapon_xm1014" },
	{ WEAPON_C4,				"weapon_c4" },
	{ WEAPON_MAC10,				"weapon_mac10" },
	{ WEAPON_AUG,				"weapon_aug" },
	{ WEAPON_SMOKEGRENADE,		"weapon_smokegrenade" },
	{ WEAPON_ELITE,				"weapon_elite" },
	{ WEAPON_FIVESEVEN,			"weapon_fiveseven" },
	{ WEAPON_UMP45,				"weapon_ump45" },
	{ WEAPON_SG550,				"weapon_sg550" },

	{ WEAPON_GALIL,				"weapon_galil" },
	{ WEAPON_FAMAS,				"weapon_famas" },
	{ WEAPON_USP,				"weapon_usp" },
	{ WEAPON_AWP,				"weapon_awp" },
	{ WEAPON_MP5NAVY,			"weapon_mp5navy" },
	{ WEAPON_M249,				"weapon_m249" },
	{ WEAPON_M3,				"weapon_m3" },
	{ WEAPON_M4A1,				"weapon_m4a1" },
	{ WEAPON_TMP,				"weapon_tmp" },
	{ WEAPON_G3SG1,				"weapon_g3sg1" },
	{ WEAPON_FLASHBANG,			"weapon_flashbang" },
	{ WEAPON_DEAGLE,			"weapon_deagle" },
	{ WEAPON_SG552,				"weapon_sg552" },
	{ WEAPON_AK47,				"weapon_ak47" },
	{ WEAPON_KNIFE,				"weapon_knife" },
	{ WEAPON_P90,				"weapon_p90" },

	// not sure any of these are needed
	{ WEAPON_SHIELDGUN,			"weapon_shieldgun" },
	{ WEAPON_KEVLAR,			"weapon_kevlar" },
	{ WEAPON_ASSAULTSUIT,		"weapon_assaultsuit" },
	{ WEAPON_NVG,				"weapon_nvg" },

	{ WEAPON_NONE,				"weapon_none" },
};



//--------------------------------------------------------------------------------------------------------------


CCSWeaponInfo g_EquipmentInfo[MAX_EQUIPMENT];

void PrepareEquipmentInfo( void )
{
	memset( g_EquipmentInfo, 0, ARRAYSIZE( g_EquipmentInfo ) );

	g_EquipmentInfo[2].SetWeaponPrice( CSGameRules()->GetBlackMarketPriceForWeapon( WEAPON_KEVLAR ) );
	g_EquipmentInfo[2].SetDefaultPrice( KEVLAR_PRICE );
	g_EquipmentInfo[2].SetPreviousPrice( CSGameRules()->GetBlackMarketPreviousPriceForWeapon( WEAPON_KEVLAR ) );
	g_EquipmentInfo[2].m_iTeam = TEAM_UNASSIGNED;
	Q_strcpy( g_EquipmentInfo[2].szClassName, "weapon_vest" );

#ifdef CLIENT_DLL
	g_EquipmentInfo[2].iconActive = new CHudTexture;
	g_EquipmentInfo[2].iconActive->cCharacterInFont = 't';
#endif

	g_EquipmentInfo[1].SetWeaponPrice( CSGameRules()->GetBlackMarketPriceForWeapon( WEAPON_ASSAULTSUIT ) );
	g_EquipmentInfo[1].SetDefaultPrice( ASSAULTSUIT_PRICE );
	g_EquipmentInfo[1].SetPreviousPrice( CSGameRules()->GetBlackMarketPreviousPriceForWeapon( WEAPON_ASSAULTSUIT ) );
	g_EquipmentInfo[1].m_iTeam = TEAM_UNASSIGNED;
	Q_strcpy( g_EquipmentInfo[1].szClassName, "weapon_vesthelm" );

#ifdef CLIENT_DLL
	g_EquipmentInfo[1].iconActive = new CHudTexture;
	g_EquipmentInfo[1].iconActive->cCharacterInFont = 'u';
#endif

	g_EquipmentInfo[0].SetWeaponPrice( CSGameRules()->GetBlackMarketPriceForWeapon( WEAPON_NVG ) );
	g_EquipmentInfo[0].SetPreviousPrice( CSGameRules()->GetBlackMarketPreviousPriceForWeapon( WEAPON_NVG ) );
	g_EquipmentInfo[0].SetDefaultPrice( NVG_PRICE );
	g_EquipmentInfo[0].m_iTeam = TEAM_UNASSIGNED;
	Q_strcpy( g_EquipmentInfo[0].szClassName, "weapon_nvgs" );

#ifdef CLIENT_DLL
	g_EquipmentInfo[0].iconActive = new CHudTexture;
	g_EquipmentInfo[0].iconActive->cCharacterInFont = 's';
#endif

}

//--------------------------------------------------------------------------------------------------------------
CCSWeaponInfo * GetWeaponInfo( CSWeaponID weaponID )
{
	if ( weaponID == WEAPON_NONE )
		return NULL;

	if ( weaponID >= WEAPON_KEVLAR )
	{
		int iIndex = (WEAPON_MAX - weaponID) - 1;

		return &g_EquipmentInfo[iIndex];

	}

	const char *weaponName = WeaponIdAsString(weaponID);
	WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( weaponName );
	if ( hWpnInfo == GetInvalidWeaponInfoHandle() )
	{
		return NULL;
	}

	CCSWeaponInfo *pWeaponInfo = dynamic_cast< CCSWeaponInfo* >( GetFileWeaponInfoFromHandle( hWpnInfo ) );

	return pWeaponInfo;
}

//--------------------------------------------------------------------------------------------------------
const char* WeaponClassAsString( CSWeaponType weaponType )
{
	for ( int i = 0; i < ARRAYSIZE(s_weaponTypeInfo); ++i )
	{
		if ( s_weaponTypeInfo[i].type == weaponType )
		{
			return s_weaponTypeInfo[i].name;
		}
	}

	return NULL;
}


//--------------------------------------------------------------------------------------------------------
CSWeaponType WeaponClassFromString( const char* weaponType )
{
	for ( int i = 0; i < ARRAYSIZE(s_weaponTypeInfo); ++i )
	{
		if ( !Q_stricmp( s_weaponTypeInfo[i].name, weaponType ) )
		{
			return s_weaponTypeInfo[i].type;
		}
	}

	return WEAPONTYPE_UNKNOWN;
}


//--------------------------------------------------------------------------------------------------------
CSWeaponType WeaponClassFromWeaponID( CSWeaponID weaponID )
{
	const char *weaponStr = WeaponIDToAlias( weaponID );
	const char *translatedAlias = GetTranslatedWeaponAlias( weaponStr );

	char wpnName[128];
	Q_snprintf( wpnName, sizeof( wpnName ), "weapon_%s", translatedAlias );
	WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( wpnName );
	if ( hWpnInfo != GetInvalidWeaponInfoHandle() )
	{
		CCSWeaponInfo *pWeaponInfo = dynamic_cast< CCSWeaponInfo* >( GetFileWeaponInfoFromHandle( hWpnInfo ) );
		if ( pWeaponInfo )
		{
			return pWeaponInfo->m_WeaponType;
		}
	}

	return WEAPONTYPE_UNKNOWN;
}


//--------------------------------------------------------------------------------------------------------
const char * WeaponIdAsString( CSWeaponID weaponID )
{
	for ( int i = 0; i < ARRAYSIZE(s_weaponNameInfo); ++i )
	{
		if (s_weaponNameInfo[i].id == weaponID )
			return s_weaponNameInfo[i].name;
	}

	return NULL;
}


//--------------------------------------------------------------------------------------------------------
CSWeaponID WeaponIdFromString( const char *szWeaponName )
{
	for ( int i = 0; i < ARRAYSIZE(s_weaponNameInfo); ++i )
	{
		if ( Q_stricmp(s_weaponNameInfo[i].name, szWeaponName) == 0 )
			return s_weaponNameInfo[i].id;
	}

	return WEAPON_NONE;
}


//--------------------------------------------------------------------------------------------------------
void ParseVector( KeyValues *keyValues, const char *keyName, Vector& vec )
{
	vec.x = vec.y = vec.z = 0.0f;

	if ( !keyValues || !keyName )
		return;

	const char *vecString = keyValues->GetString( keyName, "0 0 0" );
	if ( vecString && *vecString )
	{
		float x = 0.0f, y = 0.0f, z = 0.0f;
		if ( 3 == sscanf( vecString, "%f %f %f", &x, &y, &z ) )
		{
			vec.x = x;
			vec.y = y;
			vec.z = z;
		}
	}
}


FileWeaponInfo_t* CreateWeaponInfo()
{
	return new CCSWeaponInfo;
}


CCSWeaponInfo::CCSWeaponInfo()
{
	m_flMaxSpeed = 1; // This should always be set in the script.
	m_szAddonModel[0] = 0;
}

int	CCSWeaponInfo::GetWeaponPrice( void ) const
{
	return m_iWeaponPrice;
}

int	CCSWeaponInfo::GetDefaultPrice( void )
{
	return m_iDefaultPrice;
}

int	CCSWeaponInfo::GetPrevousPrice( void )
{
	return m_iPreviousPrice;
}


void CCSWeaponInfo::Parse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	BaseClass::Parse( pKeyValuesData, szWeaponName );

	m_flMaxSpeed = (float)pKeyValuesData->GetInt( "MaxPlayerSpeed", 1 );

	m_iDefaultPrice = m_iWeaponPrice = pKeyValuesData->GetInt( "WeaponPrice", -1 );
	if ( m_iWeaponPrice == -1 )
	{
		// This weapon should have the price in its script.
		Assert( false );
	}

	if ( CSGameRules()->IsBlackMarket() )
	{
		CSWeaponID iWeaponID = AliasToWeaponID( GetTranslatedWeaponAlias ( szWeaponName ) );

		m_iDefaultPrice = m_iWeaponPrice;
		m_iPreviousPrice = CSGameRules()->GetBlackMarketPreviousPriceForWeapon( iWeaponID );
		m_iWeaponPrice = CSGameRules()->GetBlackMarketPriceForWeapon( iWeaponID );
	}
		
	m_flArmorRatio				= pKeyValuesData->GetFloat( "WeaponArmorRatio", 1 );
	m_iCrosshairMinDistance		= pKeyValuesData->GetInt( "CrosshairMinDistance", 4 );
	m_iCrosshairDeltaDistance	= pKeyValuesData->GetInt( "CrosshairDeltaDistance", 3 );
	m_bCanUseWithShield			= !!pKeyValuesData->GetInt( "CanEquipWithShield", false );
	m_flMuzzleScale				= pKeyValuesData->GetFloat( "MuzzleFlashScale", 1 );

	const char *pMuzzleFlashStyle = pKeyValuesData->GetString( "MuzzleFlashStyle", "CS_MUZZLEFLASH_NORM" );
	
	if( pMuzzleFlashStyle )
	{
		if ( Q_stricmp( pMuzzleFlashStyle, "CS_MUZZLEFLASH_X" ) == 0 )
		{
			m_iMuzzleFlashStyle = CS_MUZZLEFLASH_X;
		}
		else if ( Q_stricmp( pMuzzleFlashStyle, "CS_MUZZLEFLASH_NONE" ) == 0 )
		{
			m_iMuzzleFlashStyle = CS_MUZZLEFLASH_NONE;
		}
		else
		{
			m_iMuzzleFlashStyle = CS_MUZZLEFLASH_NORM;
		}
	}
	else
	{
		Assert( false );
	}

	m_iPenetration		= pKeyValuesData->GetInt( "Penetration", 1 );
	m_iDamage			= pKeyValuesData->GetInt( "Damage", 42 ); // Douglas Adams 1952 - 2001
	m_flRange			= pKeyValuesData->GetFloat( "Range", 8192.0f );
	m_flRangeModifier	= pKeyValuesData->GetFloat( "RangeModifier", 0.98f );
	m_iBullets			= pKeyValuesData->GetInt( "Bullets", 1 );
	m_flCycleTime		= pKeyValuesData->GetFloat( "CycleTime", 0.15 );
	m_bAccuracyQuadratic= pKeyValuesData->GetInt( "AccuracyQuadratic", 0 );
	m_flAccuracyDivisor	= pKeyValuesData->GetFloat( "AccuracyDivisor", -1 ); // -1 = off
	m_flAccuracyOffset	= pKeyValuesData->GetFloat( "AccuracyOffset", 0 );
	m_flMaxInaccuracy	= pKeyValuesData->GetFloat( "MaxInaccuracy", 0 );

	// new accuracy model parameters
	m_fSpread[0]				= pKeyValuesData->GetFloat("Spread", 0.0f);
	m_fInaccuracyCrouch[0]		= pKeyValuesData->GetFloat("InaccuracyCrouch", 0.0f);
	m_fInaccuracyStand[0]		= pKeyValuesData->GetFloat("InaccuracyStand", 0.0f);
	m_fInaccuracyJump[0]		= pKeyValuesData->GetFloat("InaccuracyJump", 0.0f);
	m_fInaccuracyLand[0]		= pKeyValuesData->GetFloat("InaccuracyLand", 0.0f);
	m_fInaccuracyLadder[0]		= pKeyValuesData->GetFloat("InaccuracyLadder", 0.0f);
	m_fInaccuracyImpulseFire[0]	= pKeyValuesData->GetFloat("InaccuracyFire", 0.0f);
	m_fInaccuracyMove[0]		= pKeyValuesData->GetFloat("InaccuracyMove", 0.0f);

	m_fSpread[1]				= pKeyValuesData->GetFloat("SpreadAlt", 0.0f);
	m_fInaccuracyCrouch[1]		= pKeyValuesData->GetFloat("InaccuracyCrouchAlt", 0.0f);
	m_fInaccuracyStand[1]		= pKeyValuesData->GetFloat("InaccuracyStandAlt", 0.0f);
	m_fInaccuracyJump[1]		= pKeyValuesData->GetFloat("InaccuracyJumpAlt", 0.0f);
	m_fInaccuracyLand[1]		= pKeyValuesData->GetFloat("InaccuracyLandAlt", 0.0f);
	m_fInaccuracyLadder[1]		= pKeyValuesData->GetFloat("InaccuracyLadderAlt", 0.0f);
	m_fInaccuracyImpulseFire[1]	= pKeyValuesData->GetFloat("InaccuracyFireAlt", 0.0f);
	m_fInaccuracyMove[1]		= pKeyValuesData->GetFloat("InaccuracyMoveAlt", 0.0f);

	m_fInaccuracyReload			= pKeyValuesData->GetFloat("InaccuracyReload", 0.0f);
	m_fInaccuracyAltSwitch		= pKeyValuesData->GetFloat("InaccuracyAltSwitch", 0.0f);

	m_fRecoveryTimeCrouch		= pKeyValuesData->GetFloat("RecoveryTimeCrouch", 1.0f);
	m_fRecoveryTimeStand		= pKeyValuesData->GetFloat("RecoveryTimeStand", 1.0f);

	m_flTimeToIdleAfterFire	= pKeyValuesData->GetFloat( "TimeToIdle", 2 );
	m_flIdleInterval	= pKeyValuesData->GetFloat( "IdleInterval", 20 );

	// Figure out what team can have this weapon.
	m_iTeam = TEAM_UNASSIGNED;
	const char *pTeam = pKeyValuesData->GetString( "Team", NULL );
	if ( pTeam )
	{
		if ( Q_stricmp( pTeam, "CT" ) == 0 )
		{
			m_iTeam = TEAM_CT;
		}
		else if ( Q_stricmp( pTeam, "TERRORIST" ) == 0 )
		{
			m_iTeam = TEAM_TERRORIST;
		}
		else if ( Q_stricmp( pTeam, "ANY" ) == 0 )
		{
			m_iTeam = TEAM_UNASSIGNED;
		}
		else
		{
			Assert( false );
		}
	}
	else
	{
		Assert( false );
	}

	
	const char *pWrongTeamMsg = pKeyValuesData->GetString( "WrongTeamMsg", "" );
	Q_strncpy( m_WrongTeamMsg, pWrongTeamMsg, sizeof( m_WrongTeamMsg ) );

	const char *pShieldViewModel = pKeyValuesData->GetString( "shieldviewmodel", "" );
	Q_strncpy( m_szShieldViewModel, pShieldViewModel, sizeof( m_szShieldViewModel ) );
	
	const char *pAnimEx = pKeyValuesData->GetString( "PlayerAnimationExtension", "m4" );
	Q_strncpy( m_szAnimExtension, pAnimEx, sizeof( m_szAnimExtension ) );

	// Default is 2000.
	m_flBotAudibleRange = pKeyValuesData->GetFloat( "BotAudibleRange", 2000.0f );
	
	const char *pTypeString = pKeyValuesData->GetString( "WeaponType", "" );
	m_WeaponType = WeaponClassFromString(pTypeString);

	m_bFullAuto = pKeyValuesData->GetBool("FullAuto");

	// Read the addon model.
	Q_strncpy( m_szAddonModel, pKeyValuesData->GetString( "AddonModel" ), sizeof( m_szAddonModel ) );

	// Read the dropped model.
	Q_strncpy( m_szDroppedModel, pKeyValuesData->GetString( "DroppedModel" ), sizeof( m_szDroppedModel ) );

	// Read the silencer model.
	Q_strncpy( m_szSilencerModel, pKeyValuesData->GetString( "SilencerModel" ), sizeof( m_szSilencerModel ) );

#ifndef CLIENT_DLL
	// Enforce consistency for the weapon here, since that way we don't need to save off the model bounds
	// for all time.
	// Moved to pure_server_minimal.txt
//	engine->ForceExactFile( UTIL_VarArgs("scripts/%s.ctx", szWeaponName ) );

	// Model bounds are rounded to the nearest integer, then extended by 1
	engine->ForceModelBounds( szWorldModel, Vector( -15, -12, -18 ), Vector( 44, 16, 19 ) );
	if ( m_szAddonModel[0] )
	{
		engine->ForceModelBounds( m_szAddonModel, Vector( -5, -5, -6 ), Vector( 13, 5, 7 ) );
	}
	if ( m_szSilencerModel[0] )
	{
		engine->ForceModelBounds( m_szSilencerModel, Vector( -15, -12, -18 ), Vector( 44, 16, 19 ) );
	}
#endif // !CLIENT_DLL
}


