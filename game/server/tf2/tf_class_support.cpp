//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The Support/Suppression Player Class
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "tf_class_support.h"
#include "basecombatweapon.h"
#include "tf_obj.h"
#include "in_buttons.h"
#include "menu_base.h"
#include "tf_team.h"
#include "weapon_builder.h"

ConVar	class_support_speed( "class_support_speed","200", FCVAR_NONE, "Support movement speed" );

//=============================================================================
//
// Support Data Table
//
BEGIN_SEND_TABLE_NOBASE( CPlayerClassSupport, DT_PlayerClassSupportData )
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CPlayerClassSupport::GetClassModelString( int nTeam )
{
	static const char *string = "models/player/alien_support.mdl";
	return string;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlayerClassSupport::CPlayerClassSupport( CBaseTFPlayer *pPlayer, TFClass iClass ) : CPlayerClass( pPlayer, iClass )
{
	for (int i = 1; i <= MAX_TF_TEAMS; ++i)
	{
		SetClassModel( MAKE_STRING(GetClassModelString(i)), i ); 
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlayerClassSupport::~CPlayerClassSupport()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassSupport::ClassActivate( void )
{
	BaseClass::ClassActivate();

// Any objects created/owned by class should be allocated and destroyed here
	// Setup movement data.
	SetupMoveData();

	m_pPlayer->SetCollisionBounds( SUPPORTCLASS_HULL_STAND_MIN, SUPPORTCLASS_HULL_STAND_MAX );

	memset( &m_ClassData, 0, sizeof( m_ClassData ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassSupport::ClassDeactivate( void )
{
	BaseClass::ClassDeactivate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassSupport::CreateClass( void )
{
	BaseClass::CreateClass();

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPlayerClassSupport::ResupplyAmmo( float flFraction, ResupplyReason_t reason )
{
	bool bGiven = false;

	// On respawn, resupply base weapon ammo
	if ( reason == RESUPPLY_RESPAWN )
	{
	}

	if ( BaseClass::ResupplyAmmo(flFraction, reason) )
		bGiven = true;
	return bGiven;
}

//-----------------------------------------------------------------------------
// Purpose: Set support class specific movement data here.
//-----------------------------------------------------------------------------
void CPlayerClassSupport::SetupMoveData( void )
{
	// Setup Class statistics
	m_flMaxWalkingSpeed = class_support_speed.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPlayerClassSupport::SetupSizeData( void )
{
	// Initially set the player to the base player class standing hull size.
	m_pPlayer->SetCollisionBounds( SUPPORTCLASS_HULL_STAND_MIN, SUPPORTCLASS_HULL_STAND_MAX );
	m_pPlayer->SetViewOffset( SUPPORTCLASS_VIEWOFFSET_STAND );
	m_pPlayer->m_Local.m_flStepSize = SUPPORTCLASS_STEPSIZE;	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassSupport::SetPlayerHull( void )
{
	if ( m_pPlayer->GetFlags() & FL_DUCKING )
	{
		m_pPlayer->SetCollisionBounds( SUPPORTCLASS_HULL_DUCK_MIN, SUPPORTCLASS_HULL_DUCK_MAX );
	}
	else
	{
		m_pPlayer->SetCollisionBounds( SUPPORTCLASS_HULL_STAND_MIN, SUPPORTCLASS_HULL_STAND_MAX );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassSupport::ResetViewOffset( void )
{
	if ( m_pPlayer )
	{
		m_pPlayer->SetViewOffset( SUPPORTCLASS_VIEWOFFSET_STAND );
	}
}


