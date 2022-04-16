//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include <KeyValues.h>
#include "dod_weapon_parse.h"
#include "dod_shareddefs.h"

// criteria that we parse out of the file.
// tells us which player animation states
// should use the alternate wpn p model
static struct
{
	const char *m_pCriteriaName;
	int m_iFlagValue;
} g_AltWpnCritera[] =
{
	{ "ALTWPN_CRITERIA_FIRING",		ALTWPN_CRITERIA_FIRING },
	{ "ALTWPN_CRITERIA_RELOADING",	ALTWPN_CRITERIA_RELOADING },
	{ "ALTWPN_CRITERIA_DEPLOYED",	ALTWPN_CRITERIA_DEPLOYED },
	{ "ALTWPN_CRITERIA_DEPLOYED_RELOAD", ALTWPN_CRITERIA_DEPLOYED_RELOAD },
	{ "ALTWPN_CRITERIA_PRONE_DEPLOYED_RELOAD", ALTWPN_CRITERIA_PRONE_DEPLOYED_RELOAD }
};

FileWeaponInfo_t* CreateWeaponInfo()
{
	return new CDODWeaponInfo;
}


CDODWeaponInfo::CDODWeaponInfo()
{
	m_szReloadModel[0] = '\0';
	m_szDeployedModel[0] = '\0';
	m_szDeployedReloadModel[0] = '\0';
	m_szProneDeployedReloadModel[0] = '\0';
}


void CDODWeaponInfo::Parse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	BaseClass::Parse( pKeyValuesData, szWeaponName );

	m_iCrosshairMinDistance		= pKeyValuesData->GetInt( "CrosshairMinDistance", 4 );
	m_iCrosshairDeltaDistance	= pKeyValuesData->GetInt( "CrosshairDeltaDistance", 3 );
	m_iMuzzleFlashType			= pKeyValuesData->GetFloat( "MuzzleFlashType", 0 );
	m_flMuzzleFlashScale		= pKeyValuesData->GetFloat( "MuzzleFlashScale", 0.5 );

	m_iDamage				= pKeyValuesData->GetInt( "Damage", 1 );
	m_flAccuracy			= pKeyValuesData->GetFloat( "Accuracy", 1.0 );
	m_flSecondaryAccuracy	= pKeyValuesData->GetFloat( "SecondaryAccuracy", 1.0 );
	m_flAccuracyMovePenalty = pKeyValuesData->GetFloat( "AccuracyMovePenalty", 0.1 );
	m_flRecoil				= pKeyValuesData->GetFloat( "Recoil", 99.0 );
	m_flPenetration			= pKeyValuesData->GetFloat( "Penetration", 1.0 );
	m_flFireDelay			= pKeyValuesData->GetFloat( "FireDelay", 0.1 );
	m_flSecondaryFireDelay	= pKeyValuesData->GetFloat( "SecondaryFireDelay", 0.1 );
	m_flTimeToIdleAfterFire = pKeyValuesData->GetFloat( "IdleTimeAfterFire", 1.0 );
	m_flIdleInterval		= pKeyValuesData->GetFloat( "IdleInterval", 1.0 );
	m_bCanDrop				= ( pKeyValuesData->GetInt( "CanDrop", 1 ) > 0 );
	m_iBulletsPerShot		= pKeyValuesData->GetInt( "BulletsPerShot", 1 );
	
	m_iHudClipHeight		= pKeyValuesData->GetInt( "HudClipHeight", 0 );
	m_iHudClipBaseHeight	= pKeyValuesData->GetInt( "HudClipBaseHeight", 0 );
	m_iHudClipBulletHeight	= pKeyValuesData->GetInt( "HudClipBulletHeight", 0 );

	m_iAmmoPickupClips		= pKeyValuesData->GetInt( "AmmoPickupClips", 2 );
	
	m_iDefaultAmmoClips		= pKeyValuesData->GetInt( "DefaultAmmoClips", 0 );

	m_flViewModelFOV		= pKeyValuesData->GetFloat( "ViewModelFOV", 90.0f );

//	const char *pAnimEx = pKeyValuesData->GetString( "PlayerAnimationExtension", "error" );
//	Q_strncpy( m_szAnimExtension, pAnimEx, sizeof( m_szAnimExtension ) );

	// if this key exists, use this for reload animations instead of anim_prefix
//	Q_strncpy( m_szReloadAnimPrefix, pKeyValuesData->GetString( "reload_anim_prefix", "" ), MAX_WEAPON_PREFIX );

	m_flBotAudibleRange = pKeyValuesData->GetFloat( "BotAudibleRange", 2000.0f );
	
	m_iTracerType = pKeyValuesData->GetInt( "Tracer", 0 );

	//Weapon Type
	const char *pTypeString = pKeyValuesData->GetString( "WeaponType", NULL );
	
	m_WeaponType = WPN_TYPE_UNKNOWN;
	if ( !pTypeString )
	{
		Assert( false );
	}
	else if ( Q_stricmp( pTypeString, "Melee" ) == 0 )
	{
		m_WeaponType = WPN_TYPE_MELEE;
	}
	else if ( Q_stricmp( pTypeString, "Camera" ) == 0 )
	{
		m_WeaponType = WPN_TYPE_CAMERA;
	}
	else if ( Q_stricmp( pTypeString, "Grenade" ) == 0 )
	{
		m_WeaponType = WPN_TYPE_GRENADE;
	}
	else if ( Q_stricmp( pTypeString, "Pistol" ) == 0 )
	{
		m_WeaponType = WPN_TYPE_PISTOL;
	}
	else if ( Q_stricmp( pTypeString, "Rifle" ) == 0 )
	{
		m_WeaponType = WPN_TYPE_RIFLE;
	}
	else if ( Q_stricmp( pTypeString, "Sniper" ) == 0 )
	{
		m_WeaponType = WPN_TYPE_SNIPER;
	}
	else if ( Q_stricmp( pTypeString, "SubMG" ) == 0 )
	{
		m_WeaponType = WPN_TYPE_SUBMG;
	}
	else if ( Q_stricmp( pTypeString, "MG" ) == 0 )
	{
		m_WeaponType = WPN_TYPE_MG;
	}
	else if ( Q_stricmp( pTypeString, "Bazooka" ) == 0 )
	{
		m_WeaponType = WPN_TYPE_BAZOOKA;
	}
	else if ( Q_stricmp( pTypeString, "Bandage" ) == 0 )
	{
		m_WeaponType = WPN_TYPE_BANDAGE;
	}
	else if ( Q_stricmp( pTypeString, "Sidearm" ) == 0 )
	{
		m_WeaponType = WPN_TYPE_SIDEARM;
	}
	else if ( Q_stricmp( pTypeString, "RifleGrenade" ) == 0 )
	{
		m_WeaponType = WPN_TYPE_RIFLEGRENADE;
	}
	else if ( Q_stricmp( pTypeString, "Bomb" ) == 0 )
	{
		m_WeaponType = WPN_TYPE_BOMB;
	}
	else
	{
		Assert( false );
	}

	Q_strncpy( m_szReloadModel, pKeyValuesData->GetString( "reloadmodel" ), sizeof( m_szReloadModel ) );
	Q_strncpy( m_szDeployedModel, pKeyValuesData->GetString( "deployedmodel" ), sizeof( m_szDeployedModel ) );
	Q_strncpy( m_szDeployedReloadModel, pKeyValuesData->GetString( "deployedreloadmodel" ), sizeof( m_szDeployedReloadModel ) );
	Q_strncpy( m_szProneDeployedReloadModel, pKeyValuesData->GetString( "pronedeployedreloadmodel" ), sizeof( m_szProneDeployedReloadModel ) );


	m_iAltWpnCriteria = ALTWPN_CRITERIA_NONE;

	for ( int i=0; i < ARRAYSIZE( g_AltWpnCritera ); i++ )
	{
		int iVal = pKeyValuesData->GetInt( g_AltWpnCritera[i].m_pCriteriaName, 0 );
		if ( iVal == 1 )
		{
			m_iAltWpnCriteria |= g_AltWpnCritera[i].m_iFlagValue;
		}
	}

	const char *szNormalOffset = pKeyValuesData->GetString( "vm_normal_offset", "0 0 0" );
	const char *szProneOffset = pKeyValuesData->GetString( "vm_prone_offset", "0 0 0" );
	const char *szIronSightOffset = pKeyValuesData->GetString( "vm_ironsight_offset", "0 0 0" );

	sscanf( szNormalOffset, "%f %f %f", &m_vecViewNormalOffset[0], &m_vecViewNormalOffset[1], &m_vecViewNormalOffset[2]);
	sscanf( szProneOffset, "%f %f %f", &m_vecViewProneOffset[0], &m_vecViewProneOffset[1], &m_vecViewProneOffset[2]);
	sscanf( szIronSightOffset, "%f %f %f", &m_vecIronSightOffset[0], &m_vecIronSightOffset[1], &m_vecIronSightOffset[2]);

	m_iDefaultTeam = TEAM_ALLIES;

	const char *pDefaultTeam = pKeyValuesData->GetString( "default_team", NULL );

	if ( pDefaultTeam )
	{
		if ( FStrEq( pDefaultTeam, "Axis" ) )
		{
			m_iDefaultTeam = TEAM_AXIS;
		}
		else if ( FStrEq( pDefaultTeam, "Allies" ) )
		{
			m_iDefaultTeam = TEAM_ALLIES;
		}
		else
		{
			Assert( !"invalid param to \"default_team\" in weapon scripts\n" );
		}
	}
}

