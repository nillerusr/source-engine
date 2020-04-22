//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The Infiltrator Player Class
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "orders.h"
#include "tf_player.h"
#include "tf_class_infiltrator.h"
#include "EntityList.h"
#include "basecombatweapon.h"
#include "menu_base.h"
#include "tf_obj.h"
#include "tf_team.h"
#include "weapon_builder.h"

ConVar	class_infiltrator_speed( "class_infiltrator_speed","200", FCVAR_NONE, "Infiltrator movement speed" );

//=============================================================================
//
// Infiltrator Data Table
//
BEGIN_SEND_TABLE_NOBASE( CPlayerClassInfiltrator, DT_PlayerClassInfiltratorData )
END_SEND_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CPlayerClassInfiltrator::GetClassModelString( int nTeam )
{
	static const char *string = "models/player/spy.mdl";
	return string;
}

// Infiltrator
CPlayerClassInfiltrator::CPlayerClassInfiltrator( CBaseTFPlayer *pPlayer, TFClass iClass ) : CPlayerClass( pPlayer, iClass )
{
	for (int i = 0; i < MAX_TF_TEAMS; ++i)
	{
		SetClassModel( MAKE_STRING(GetClassModelString(i)), i ); 
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlayerClassInfiltrator::~CPlayerClassInfiltrator()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassInfiltrator::ClassActivate( void )
{
	BaseClass::ClassActivate();

	// Setup movement data.
	SetupMoveData();

	//m_hAssassinationWeapon = NULL;
	m_hSwappedWeapon = NULL;
	
	m_bCanConsumeCorpses = false;
	m_flStartCamoAt = 0.0f; 

	memset( &m_ClassData, 0, sizeof( m_ClassData ) );
}


//-----------------------------------------------------------------------------
// Purpose: Register for precaching.
//-----------------------------------------------------------------------------
void PrecacheInfiltrator(void *pUser)
{
}
PRECACHE_REGISTER_FN(PrecacheInfiltrator);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassInfiltrator::RespawnClass( void )
{
	BaseClass::RespawnClass();

	m_flStartCamoAt = gpGlobals->curtime + INFILTRATOR_CAMOTIME_AFTER_SPAWN;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPlayerClassInfiltrator::ResupplyAmmo( float flFraction, ResupplyReason_t reason )
{
	bool bGiven = false;
	if ((reason == RESUPPLY_ALL_FROM_STATION) || (reason == RESUPPLY_AMMO_FROM_STATION))
	{
		if (ResupplyAmmoType( 200 * flFraction, "Bullets" ))
			bGiven = true;
		if (ResupplyAmmoType( 20 * flFraction, "Limpets" ))
			bGiven = true;
	}

	if ( BaseClass::ResupplyAmmo(flFraction, reason) )
		bGiven = true;
	return bGiven;
}

//-----------------------------------------------------------------------------
// Purpose: Set infiltrator class specific movement data here.
//-----------------------------------------------------------------------------
void CPlayerClassInfiltrator::SetupMoveData( void )
{
	// Setup Class statistics
	m_flMaxWalkingSpeed = class_infiltrator_speed.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPlayerClassInfiltrator::SetupSizeData( void )
{
	// Initially set the player to the base player class standing hull size.
	m_pPlayer->SetCollisionBounds( INFILTRATORCLASS_HULL_STAND_MIN, INFILTRATORCLASS_HULL_STAND_MAX );
	m_pPlayer->SetViewOffset( INFILTRATORCLASS_VIEWOFFSET_STAND );
	m_pPlayer->m_Local.m_flStepSize = INFILTRATORCLASS_STEPSIZE;	
}

//-----------------------------------------------------------------------------
// Purpose: New technology has been gained. Recalculate any class specific technology dependencies.
//-----------------------------------------------------------------------------
void CPlayerClassInfiltrator::GainedNewTechnology( CBaseTechnology *pTechnology )
{
	// Consume corpse technology?
	m_bCanConsumeCorpses = m_pPlayer->HasNamedTechnology( "inf_consume_corpse" );

	BaseClass::GainedNewTechnology( pTechnology );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this player's allowed to build another one of the specified objects
//-----------------------------------------------------------------------------
int CPlayerClassInfiltrator::CanBuild( int iObjectType )
{
	return BaseClass::CanBuild( iObjectType );
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame by postthink
//-----------------------------------------------------------------------------
void CPlayerClassInfiltrator::ClassThink( void )
{
	BaseClass::ClassThink();

	CheckForAssassination();
}

//-----------------------------------------------------------------------------
// Purpose: The player's just had his camo cleared
//-----------------------------------------------------------------------------
void CPlayerClassInfiltrator::ClearCamouflage( void )
{
	float flNewTime = gpGlobals->curtime + INFILTRATOR_RECAMO_TIME;
	if ( flNewTime > m_flStartCamoAt )
	{
		m_flStartCamoAt = flNewTime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Player's finished disguising.
//-----------------------------------------------------------------------------
void CPlayerClassInfiltrator::FinishedDisguising( void )
{
	// Remove my camo
	m_pPlayer->ClearCamouflage();
}

//-----------------------------------------------------------------------------
// Purpose: Player's lost his disguise.
//-----------------------------------------------------------------------------
void CPlayerClassInfiltrator::StopDisguising( void )
{
	// Remove & restart my camo
	m_pPlayer->ClearCamouflage();
}

//-----------------------------------------------------------------------------
// Purpose: Handle custom commands for this playerclass
//-----------------------------------------------------------------------------
bool CPlayerClassInfiltrator::ClientCommand( const char *pcmd )
{
	// Toggle thermal vision
	if ( FStrEq( pcmd, "special" ) )
	{
		Assert( m_pPlayer );
		// Toggle
		m_pPlayer->SetUsingThermalVision( !m_pPlayer->IsUsingThermalVision() );
		return true;
	}

	return BaseClass::ClientCommand( pcmd );
}


//-----------------------------------------------------------------------------
// Purpose: Check to see if I can assassinate anyone
//-----------------------------------------------------------------------------
void CPlayerClassInfiltrator::CheckForAssassination( void )
{
	/*
	// Find my assassination weapon if I haven't already
	if ( m_hAssassinationWeapon == NULL )
	{
		m_hAssassinationWeapon = (CWeaponInfiltrator*)m_pPlayer->Weapon_OwnsThisType( "weapon_infiltrator" );
	}

	// No Assasination weapon?
	if ( m_hAssassinationWeapon == NULL )
		return;

	// If we have a target, bring the assassination weapon up.
	if ( m_hAssassinationWeapon->GetAssassinationTarget() )
	{
		if ( m_pPlayer->GetActiveWeapon() != m_hAssassinationWeapon.Get() )
		{
			m_hSwappedWeapon = m_pPlayer->GetActiveWeapon();
			m_pPlayer->Weapon_Switch( m_hAssassinationWeapon );
		}
	}
	else
	{
		// We have no target. If the assassination weapon's up, put it away.
		if ( m_pPlayer->GetActiveWeapon() == m_hAssassinationWeapon.Get() )
		{
			// Don't switch until the weapon's finished killing people
			if ( m_pPlayer->GetActiveWeapon()->m_flNextPrimaryAttack <= gpGlobals->curtime )
			{
				m_pPlayer->Weapon_Switch( m_hSwappedWeapon );
				m_hSwappedWeapon = NULL;
			}
		}
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassInfiltrator::SetPlayerHull( void )
{
	if ( m_pPlayer->GetFlags() & FL_DUCKING )
	{
		m_pPlayer->SetCollisionBounds( INFILTRATORCLASS_HULL_DUCK_MIN, INFILTRATORCLASS_HULL_DUCK_MAX );
	}
	else
	{
		m_pPlayer->SetCollisionBounds( INFILTRATORCLASS_HULL_STAND_MIN, INFILTRATORCLASS_HULL_STAND_MAX );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassInfiltrator::ResetViewOffset( void )
{
	if ( m_pPlayer )
	{
		m_pPlayer->SetViewOffset( INFILTRATORCLASS_VIEWOFFSET_STAND );
	}
}
