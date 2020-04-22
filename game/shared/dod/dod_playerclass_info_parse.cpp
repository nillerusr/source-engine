//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "dod_playerclass_info_parse.h"
#include "dod_shareddefs.h"
#include "weapon_dodbase.h"
#include <KeyValues.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

FilePlayerClassInfo_t* CreatePlayerClassInfo()
{
	return new CDODPlayerClassInfo;
}

CDODPlayerClassInfo::CDODPlayerClassInfo()
{
	m_iTeam= TEAM_UNASSIGNED;
	
	m_iPrimaryWeapon= WEAPON_NONE;
	m_iSecondaryWeapon= WEAPON_NONE;
	m_iMeleeWeapon= WEAPON_NONE;
	
	m_iNumGrensType1 = 0;
	m_iGrenType1 = WEAPON_NONE;

	m_iNumGrensType2 = 0;
	m_iGrenType2 = WEAPON_NONE;

	m_iNumBandages = 0;
		
	m_iHelmetGroup= HELMET_GROUP_0;
	m_iHairGroup= HELMET_GROUP_0;

	m_iDropHelmet = HELMET_ALLIES;

	m_szLimitCvar[0] = '\0';
	m_bClassLimitMGMerge = false;
}

int AliasToWeaponID( const char *alias )
{
	if (alias)
	{
		for( int i=0; s_WeaponAliasInfo[i] != NULL; ++i )
			if (!Q_stricmp( s_WeaponAliasInfo[i], alias ))
				return i;
	}

	return WEAPON_NONE;
}

void CDODPlayerClassInfo::Parse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	BaseClass::Parse( pKeyValuesData, szWeaponName );

	m_iTeam= pKeyValuesData->GetInt( "team", TEAM_UNASSIGNED );

	// Figure out what team can have this player class
	m_iTeam = TEAM_UNASSIGNED;
	const char *pTeam = pKeyValuesData->GetString( "team", NULL );
	if ( pTeam )
	{
		if ( Q_stricmp( pTeam, "ALLIES" ) == 0 )
		{
			m_iTeam = TEAM_ALLIES;
		}
		else if ( Q_stricmp( pTeam, "AXIS" ) == 0 )
		{
			m_iTeam = TEAM_AXIS;
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
	
	const char *pszPrimaryWeapon = pKeyValuesData->GetString( "primaryweapon", NULL );
	m_iPrimaryWeapon = AliasToWeaponID( pszPrimaryWeapon );
	Assert( m_iPrimaryWeapon != WEAPON_NONE );	// require player to have a primary weapon

	const char *pszSecondaryWeapon = pKeyValuesData->GetString( "secondaryweapon", NULL );

	if ( pszSecondaryWeapon )
	{
        m_iSecondaryWeapon = AliasToWeaponID( pszSecondaryWeapon );
		Assert( m_iSecondaryWeapon != WEAPON_NONE );
	}
	else 
		m_iSecondaryWeapon = WEAPON_NONE;

	const char *pszMeleeWeapon = pKeyValuesData->GetString( "meleeweapon", NULL );
	if ( pszMeleeWeapon )
	{
		m_iMeleeWeapon = AliasToWeaponID( pszMeleeWeapon );
        Assert( m_iMeleeWeapon != WEAPON_NONE );
	}
	else
		m_iMeleeWeapon = WEAPON_NONE;

	m_iNumGrensType1 = pKeyValuesData->GetInt( "numgrens", 0 );
	if ( m_iNumGrensType1 > 0 )
	{
		const char *pszGrenType1 = pKeyValuesData->GetString( "grenadetype", NULL );
		m_iGrenType1 = AliasToWeaponID( pszGrenType1 );
		Assert( m_iGrenType1 != WEAPON_NONE );
	}

	m_iNumGrensType2 = pKeyValuesData->GetInt( "numgrens2", 0 );
	if ( m_iNumGrensType2 > 0 )
	{
		const char *pszGrenType2 = pKeyValuesData->GetString( "grenadetype2", NULL );
		m_iGrenType2 = AliasToWeaponID( pszGrenType2 );
		Assert( m_iGrenType2 != WEAPON_NONE );
	}

	m_iNumBandages = pKeyValuesData->GetInt( "numbandages", 0 );

	m_iHelmetGroup	= pKeyValuesData->GetInt( "helmetgroup", 0 );
	m_iHairGroup	= pKeyValuesData->GetInt( "hairgroup", 0 );

	// Which helmet model to generate 
	const char *pszHelmetModel = pKeyValuesData->GetString( "drophelmet", "HELMET_ALLIES" );

	if( pszHelmetModel )
	{
		if ( Q_stricmp( pszHelmetModel, "HELMET_ALLIES" ) == 0 )
		{
			m_iDropHelmet = HELMET_ALLIES;
		}
		else if ( Q_stricmp( pszHelmetModel, "HELMET_AXIS" ) == 0 )
		{
			m_iDropHelmet = HELMET_AXIS;
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

	Q_strncpy( m_szLimitCvar, pKeyValuesData->GetString( "limitcvar", "!! Missing limit cvar on Player Class" ), sizeof(m_szLimitCvar) );

	Assert( Q_strlen( m_szLimitCvar ) > 0 && "Every class must specify a limitcvar" );

	m_bClassLimitMGMerge = ( pKeyValuesData->GetInt( "mergemgclass" ) > 0 );

	// HUD player status health images (when the player is hurt)
	Q_strncpy( m_szClassHealthImage, pKeyValuesData->GetString( "healthimage", "white" ), sizeof( m_szClassHealthImage ) );
	Q_strncpy( m_szClassHealthImageBG, pKeyValuesData->GetString( "healthimagebg", "white" ), sizeof( m_szClassHealthImageBG ) );
}