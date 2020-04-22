//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The Recon Player Class
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_player.h"
#include "tf_class_recon.h"
#include "tf_reconvars.h"
#include "in_buttons.h"
#include "basecombatweapon.h"
#include "tf_obj.h"
#include "weapon_builder.h"
#include "tf_team.h"
#include "tf_func_resource.h"
#include "orders.h"
#include "engine/IEngineSound.h"


extern short g_sModelIndexFireball;

ConVar	class_recon_speed( "class_recon_speed","250", FCVAR_NONE, "Recon movement speed" );

// ------------------------------------------------------------------------ //
// Globals.
// ------------------------------------------------------------------------ //

//=============================================================================
//
// Recon Data Table
//
BEGIN_SEND_TABLE_NOBASE( CPlayerClassRecon, DT_PlayerClassReconData )
	SendPropInt		( SENDINFO_STRUCTELEM( m_ClassData.m_nJumpCount ), 3, SPROP_UNSIGNED ),
	SendPropFloat	( SENDINFO_STRUCTELEM( m_ClassData.m_flSuppressionJumpTime ), 32, SPROP_NOSCALE ),
	SendPropFloat	( SENDINFO_STRUCTELEM( m_ClassData.m_flSuppressionImpactTime ), 32, SPROP_NOSCALE ),
	SendPropFloat	( SENDINFO_STRUCTELEM( m_ClassData.m_flStickTime ), 32, SPROP_NOSCALE ),
	SendPropFloat	( SENDINFO_STRUCTELEM( m_ClassData.m_flActiveJumpTime ), 32, SPROP_NOSCALE ),
	SendPropFloat	( SENDINFO_STRUCTELEM( m_ClassData.m_flImpactDist ), 32, SPROP_NOSCALE ),
	SendPropVector	( SENDINFO_STRUCTELEM( m_ClassData.m_vecImpactNormal ), -1, SPROP_NORMAL ),
	SendPropVector	( SENDINFO_STRUCTELEM( m_ClassData.m_vecUnstickVelocity ), -1, SPROP_COORD ),
	SendPropInt		( SENDINFO_STRUCTELEM( m_ClassData.m_bTrailParticles ), 1, SPROP_UNSIGNED ),
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CPlayerClassRecon::GetClassModelString( int nTeam )
{
	static const char *string = "models/player/recon.mdl";
	return string;
}

// ------------------------------------------------------------------------ //
// CPlayerClassRecon
// ------------------------------------------------------------------------ //
CPlayerClassRecon::CPlayerClassRecon( CBaseTFPlayer *pPlayer, TFClass iClass ) : CPlayerClass( pPlayer, iClass )
{
	for (int i = 0; i < MAX_TF_TEAMS; ++i)
	{
		SetClassModel( MAKE_STRING(GetClassModelString(i)), i ); 
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlayerClassRecon::~CPlayerClassRecon()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassRecon::ClassActivate( void )
{
	BaseClass::ClassActivate();

	// Setup movement data.
	SetupMoveData();

	m_bHasRadarScanner = false;

	memset( &m_ClassData, 0, sizeof( m_ClassData ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassRecon::ClassDeactivate( void )
{
	BaseClass::ClassDeactivate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPlayerClassRecon::ResupplyAmmo( float flFraction, ResupplyReason_t reason )
{
	bool bGiven = false;

	if ((reason == RESUPPLY_ALL_FROM_STATION) || (reason == RESUPPLY_GRENADES_FROM_STATION))
	{
	}

	if ((reason == RESUPPLY_ALL_FROM_STATION) || (reason == RESUPPLY_AMMO_FROM_STATION))
	{
	}

	if ( BaseClass::ResupplyAmmo(flFraction, reason) )
		bGiven = true;
	return bGiven;
}

//-----------------------------------------------------------------------------
// Purpose: Set recon class specific movement data here.
//-----------------------------------------------------------------------------
void CPlayerClassRecon::SetupMoveData( void )
{
	// Setup Class statistics
	m_flMaxWalkingSpeed = class_recon_speed.GetFloat();

	m_ClassData.m_nJumpCount = 0;
	m_ClassData.m_flSuppressionJumpTime = -99999;
	m_ClassData.m_flSuppressionImpactTime = -99999;
	m_ClassData.m_flActiveJumpTime = -99999;
	m_ClassData.m_flStickTime = -99999;
	m_ClassData.m_flImpactDist = -99999;
	m_ClassData.m_vecImpactNormal.Init();
	m_ClassData.m_vecUnstickVelocity.Init();
	m_ClassData.m_bTrailParticles = false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPlayerClassRecon::SetupSizeData( void )
{
	// Initially set the player to the base player class standing hull size.
	m_pPlayer->SetCollisionBounds( RECONCLASS_HULL_STAND_MIN, RECONCLASS_HULL_STAND_MAX );
	m_pPlayer->SetViewOffset( RECONCLASS_VIEWOFFSET_STAND );
	m_pPlayer->m_Local.m_flStepSize = RECONCLASS_STEPSIZE;	
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this player's allowed to build another one of the specified objects
//-----------------------------------------------------------------------------
int CPlayerClassRecon::CanBuild( int iObjectType )
{
	return BaseClass::CanBuild( iObjectType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassRecon::ClassThink()
{
	BaseClass::ClassThink();

	m_ClassData.m_bTrailParticles = (m_pPlayer->IsAlive() && !(m_pPlayer->GetFlags() & FL_ONGROUND));
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassRecon::CreatePersonalOrder()
{
	if ( CreateInitialOrder() )
		return;

	BaseClass::CreatePersonalOrder();
}

//-----------------------------------------------------------------------------
// Purpose: New technology has been gained. Recalculate any class specific technology dependencies.
//-----------------------------------------------------------------------------
void CPlayerClassRecon::GainedNewTechnology( CBaseTechnology *pTechnology )
{
	// Radar Scanner
	if ( m_pPlayer->HasNamedTechnology( "rec_b_radar_scanners" ) )
	{
		m_bHasRadarScanner = true;
	}
	else
	{
		m_bHasRadarScanner = false;
	}

	BaseClass::GainedNewTechnology( pTechnology );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassRecon::SetPlayerHull( void )
{
	if ( m_pPlayer->GetFlags() & FL_DUCKING )
	{
		m_pPlayer->SetCollisionBounds( RECONCLASS_HULL_DUCK_MIN, RECONCLASS_HULL_DUCK_MAX );
	}
	else
	{
		m_pPlayer->SetCollisionBounds( RECONCLASS_HULL_STAND_MIN, RECONCLASS_HULL_STAND_MAX );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassRecon::ResetViewOffset( void )
{
	if ( m_pPlayer )
	{
		m_pPlayer->SetViewOffset( RECONCLASS_VIEWOFFSET_STAND );
	}
}


