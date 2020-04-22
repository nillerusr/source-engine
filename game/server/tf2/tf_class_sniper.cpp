//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The Sniper Player Class
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "tf_class_sniper.h"
#include "in_buttons.h"
#include "tf_team.h"
#include "tf_class_support.h"
#include "order_killmortarguy.h"


bool IsEntityVisibleToTactical( int iLocalTeamNumber, int iLocalTeamPlayers, 
	int iLocalTeamScanners, int iEntIndex, const char *pEntName, int pEntTeamNumber, const Vector &pEntOrigin );

ConVar	class_sniper_speed( "class_sniper_speed","200", FCVAR_NONE, "Sniper movement speed" );

// Stationary Camo
#define SNIPER_MAX_HIDE_LEVEL			60
#define SNIPER_HIDE_INCREASE_PER_SEC	15
#define SNIPER_FIRE_UNHIDE_TIME			5.0

//=============================================================================
//
// Sniper Data Table
//
BEGIN_SEND_TABLE_NOBASE( CPlayerClassSniper, DT_PlayerClassSniperData )
END_SEND_TABLE()



//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CPlayerClassSniper::GetClassModelString( int nTeam )
{
	static const char *string = "models/player/sniper.mdl";
	return string;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlayerClassSniper::CPlayerClassSniper( CBaseTFPlayer *pPlayer, TFClass iClass ) : CPlayerClass( pPlayer, iClass )
{
	for (int i = 0; i < MAX_TF_TEAMS; ++i)
	{
		SetClassModel( MAKE_STRING(GetClassModelString(i)), i ); 
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlayerClassSniper::~CPlayerClassSniper()
{
}
 

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassSniper::ClassActivate( void )
{
	BaseClass::ClassActivate();

	// Setup movement data.
	SetupMoveData();

	m_bHiding = false;
	m_flHideTransparency = 0.0f;
	m_flLastHideUpdate = 0.0f;

	m_bCanHide = false;

	memset( &m_ClassData, 0, sizeof( m_ClassData ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassSniper::ClassDeactivate( void )
{
	BaseClass::ClassDeactivate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassSniper::RespawnClass( void )
{
	BaseClass::RespawnClass();
	
	// Hiding values
	m_bHiding = false;
	m_flHideTransparency = m_flLastHideUpdate = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPlayerClassSniper::ResupplyAmmo( float flFraction, ResupplyReason_t reason )
{
	bool bGiven = false;

	if ((reason == RESUPPLY_ALL_FROM_STATION) || (reason == RESUPPLY_AMMO_FROM_STATION))
	{
		if (ResupplyAmmoType( 100 * flFraction, "Bullets" ))
			bGiven = true;
	}

	if ( BaseClass::ResupplyAmmo(flFraction, reason) )
		bGiven = true;

	return bGiven;
}

//-----------------------------------------------------------------------------
// Purpose: Set sniper class specific movement data here.
//-----------------------------------------------------------------------------
void CPlayerClassSniper::SetupMoveData( void )
{
	// Setup Class statistics
	m_flMaxWalkingSpeed = class_sniper_speed.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPlayerClassSniper::SetupSizeData( void )
{
	// Initially set the player to the base player class standing hull size.
	m_pPlayer->SetCollisionBounds( SNIPERCLASS_HULL_STAND_MIN, SNIPERCLASS_HULL_STAND_MAX );
	m_pPlayer->SetViewOffset( SNIPERCLASS_VIEWOFFSET_STAND );
	m_pPlayer->m_Local.m_flStepSize = SNIPERCLASS_STEPSIZE;	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CPlayerClassSniper::GetDeployTime( void )
{ 
	return 2.8; 
};

//-----------------------------------------------------------------------------
// Purpose: Called every frame by the player PostThink()
//-----------------------------------------------------------------------------
void CPlayerClassSniper::ClassThink( void )
{
	// Only hide if we have the technology 
	if ( m_bCanHide )
	{
		CheckHiding();
	}

	BaseClass::ClassThink();
}

//-----------------------------------------------------------------------------
// Purpose: New technology has been gained. Recalculate any class specific technology dependencies.
//-----------------------------------------------------------------------------
void CPlayerClassSniper::GainedNewTechnology( CBaseTechnology *pTechnology )
{
	// Stealthed on deploy
	if ( m_pPlayer->HasNamedTechnology( "sniper_deploy_stealth" ) )
	{
		m_bCanHide = true;
	}
	else
	{
		m_bCanHide = false;
	}

	BaseClass::GainedNewTechnology( pTechnology );
}


//-----------------------------------------------------------------------------
// Purpose: If the player's prone, slowly make him transparent
//-----------------------------------------------------------------------------
void CPlayerClassSniper::CheckHiding( void )
{
	// Can't hide or stay hidden if taking EMP damage
	if ( m_pPlayer->HasPowerup(POWERUP_EMP) )
	{
		m_pPlayer->ClearCamouflage();
		return;
	}

	// See if the player's just fired
	if ( m_pPlayer->m_afButtonReleased & IN_ATTACK )
	{
		// Immediately mostly unhide if so
		m_pPlayer->ClearCamouflage();
	}
	else if ( !m_pPlayer->IsDeployed() )
	{
		// Not deployed? Immediately unhide if so
		m_pPlayer->ClearCamouflage();
	}
	else
	{
		// We're deployed, hide if we haven't fired for a bit.
		if ( m_pPlayer->LastAttackTime() + SNIPER_FIRE_UNHIDE_TIME < gpGlobals->curtime )
		{
			m_pPlayer->SetCamouflaged( SNIPER_MAX_HIDE_LEVEL, SNIPER_HIDE_INCREASE_PER_SEC );
		}
		else
		{
			m_pPlayer->ClearCamouflage();
		}
	}
}


void CPlayerClassSniper::CreatePersonalOrder( void )
{
	if ( CreateInitialOrder() )
		return;
	
	// Kill a support guy?
	if ( COrderKillMortarGuy::CreateOrder( this ) )
		return;

	BaseClass::CreatePersonalOrder();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassSniper::SetPlayerHull( void )
{
	if ( m_pPlayer->GetFlags() & FL_DUCKING )
	{
		m_pPlayer->SetCollisionBounds( SNIPERCLASS_HULL_DUCK_MIN, SNIPERCLASS_HULL_DUCK_MAX );
	}
	else
	{
		m_pPlayer->SetCollisionBounds( SNIPERCLASS_HULL_STAND_MIN, SNIPERCLASS_HULL_STAND_MAX );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassSniper::ResetViewOffset( void )
{
	if ( m_pPlayer )
	{
		m_pPlayer->SetViewOffset( SNIPERCLASS_VIEWOFFSET_STAND );
	}
}

