//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The Defender Player Class
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "tf_class_defender.h"
#include "tf_obj.h"
#include "tf_obj_sentrygun.h"
#include "basecombatweapon.h"
#include "weapon_builder.h"
#include "weapon_limpetmine.h"
#include "tf_team.h"
#include "orders.h"
#include "order_repair.h"
#include "order_buildsentrygun.h"
#include "weapon_twohandedcontainer.h"
#include "weapon_combatshield.h"
#include "tf_vehicle_teleport_station.h"

ConVar	class_defender_speed( "class_defender_speed","200", FCVAR_NONE, "Defender movement speed" );


// An object must be this close to a sentry gun to be considered covered by it.
#define DEFENDER_SENTRY_COVERED_DIST	1000


//=============================================================================
//
// Defender Data Table
//
BEGIN_SEND_TABLE_NOBASE( CPlayerClassDefender, DT_PlayerClassDefenderData )
END_SEND_TABLE()


bool OrderCreator_BuildSentryGun( CPlayerClassDefender *pClass )
{
	return COrderBuildSentryGun::CreateOrder( pClass );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CPlayerClassDefender::GetClassModelString( int nTeam )
{
	if (nTeam == TEAM_HUMANS)
		return "models/player/human_defender.mdl";
	else
		return "models/player/defender.mdl";
}

//-----------------------------------------------------------------------------
// Purpose: Defender
//-----------------------------------------------------------------------------
CPlayerClassDefender::CPlayerClassDefender( CBaseTFPlayer *pPlayer, TFClass iClass ) : CPlayerClass( pPlayer, iClass )
{
	for (int i = 0; i < MAX_TF_TEAMS; ++i)
	{
		SetClassModel( MAKE_STRING(GetClassModelString(i)), i ); 
	}
}

CPlayerClassDefender::~CPlayerClassDefender()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassDefender::ClassActivate( void )
{
	BaseClass::ClassActivate();

	// Setup movement data.
	SetupMoveData();

	m_iNumberOfSentriesAllowed = 0;
	m_bHasSmarterSentryguns = false;
	m_bHasSensorSentryguns = false;
	m_bHasMachinegun = false;
	m_bHasRocketlauncher = false;
	m_bHasAntiair = false;

	m_hWpnShield = NULL;
	m_hWpnPlasma = NULL;
	memset( &m_ClassData, 0, sizeof( m_ClassData ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassDefender::ClassDeactivate( void )
{
	BaseClass::ClassDeactivate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassDefender::CreateClass( void )
{
	BaseClass::CreateClass();

	// Create our two handed weapon layout
	m_hWpnPlasma = static_cast< CBaseTFCombatWeapon * >( m_pPlayer->GiveNamedItem( "weapon_combat_burstrifle" ) );
	m_hWpnShield = m_pPlayer->GetCombatShield();
	CWeaponTwoHandedContainer *p = ( CWeaponTwoHandedContainer * )m_pPlayer->Weapon_OwnsThisType( "weapon_twohandedcontainer" );
	if ( !p )
	{
		p = static_cast< CWeaponTwoHandedContainer * >( m_pPlayer->GiveNamedItem( "weapon_twohandedcontainer" ) );
	}
	if ( p && m_hWpnShield.Get() && m_hWpnPlasma.Get() )
	{
		m_hWpnShield->SetReflectViewModelAnimations( true );
		p->SetWeapons( m_hWpnPlasma, m_hWpnShield );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPlayerClassDefender::ResupplyAmmo( float flFraction, ResupplyReason_t reason )
{
	bool bGiven = false;
	if ((reason == RESUPPLY_ALL_FROM_STATION) || (reason == RESUPPLY_AMMO_FROM_STATION))
	{
		if (ResupplyAmmoType( 20 * flFraction, "Limpets" ))
			bGiven = true;

		// Defender doesn't use rockets, but his sentryguns do
		if (ResupplyAmmoType( 50 * flFraction, "Rockets" ))
			bGiven = true;
	}

	if ((reason == RESUPPLY_ALL_FROM_STATION) || (reason == RESUPPLY_GRENADES_FROM_STATION))
	{
	}

	// On respawn, resupply base weapon ammo
	if ( reason == RESUPPLY_RESPAWN )
	{
	}

	if ( BaseClass::ResupplyAmmo(flFraction, reason) )
		bGiven = true;

	return bGiven;
}


//-----------------------------------------------------------------------------
// Purpose: Set defender class specific movement data here.
//-----------------------------------------------------------------------------
void CPlayerClassDefender::SetupMoveData( void )
{
	// Setup Class statistics
	m_flMaxWalkingSpeed = class_defender_speed.GetFloat();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPlayerClassDefender::SetupSizeData( void )
{
	// Initially set the player to the base player class standing hull size.
	m_pPlayer->SetCollisionBounds( DEFENDERCLASS_HULL_STAND_MIN, DEFENDERCLASS_HULL_STAND_MAX );
	m_pPlayer->SetViewOffset( DEFENDERCLASS_VIEWOFFSET_STAND );
	m_pPlayer->m_Local.m_flStepSize = DEFENDERCLASS_STEPSIZE;	
}


//-----------------------------------------------------------------------------
// Purpose: New technology has been gained. Recalculate any class specific technology dependencies.
//-----------------------------------------------------------------------------
void CPlayerClassDefender::GainedNewTechnology( CBaseTechnology *pTechnology )
{
	// Calculate the number of sentryguns allowed
	if ( m_pPlayer->HasNamedTechnology( "sentrygun_three" ) )
	{
		m_iNumberOfSentriesAllowed = 3;
	}
	else if ( m_pPlayer->HasNamedTechnology( "sentrygun_two" ) )
	{
		m_iNumberOfSentriesAllowed = 2;
	}
	else
	{
		m_iNumberOfSentriesAllowed = 1;
	}

	m_bHasSmarterSentryguns = false;
	m_bHasSensorSentryguns = false;
	m_bHasRocketlauncher = false;

	// Sentrygun levels
	if ( m_pPlayer->HasNamedTechnology( "sentrygun_ai" ) )
	{
		m_bHasSmarterSentryguns = true;
	}
	if ( m_pPlayer->HasNamedTechnology( "sentrygun_sensors" ) )
	{
		m_bHasSensorSentryguns = true;
	}

	// Sentrygun types
	if ( m_pPlayer->HasNamedTechnology( "sentrygun_rocket" ) )
	{
		m_bHasRocketlauncher = true;
	}

	UpdateSentrygunTechnology();
	BaseClass::GainedNewTechnology( pTechnology );
}

//-----------------------------------------------------------------------------
// Purpose: Tell all sentryguns what level of technology the Defender has
//-----------------------------------------------------------------------------
void CPlayerClassDefender::UpdateSentrygunTechnology( void )
{
	for (int i = 0; i < m_pPlayer->GetObjectCount(); i++)
	{
		CBaseObject *pObj = m_pPlayer->GetObject(i);
		if ( pObj && pObj->IsSentrygun() )
		{
			CObjectSentrygun *pSentry = static_cast<CObjectSentrygun *>(pObj);
			pSentry->SetTechnology( m_bHasSmarterSentryguns, m_bHasSensorSentryguns );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this player's allowed to build another one of the specified objects
//-----------------------------------------------------------------------------
int CPlayerClassDefender::CanBuild( int iObjectType )
{
	// First, check to see if we've got the technology
	if ( iObjectType == OBJ_SENTRYGUN_ROCKET_LAUNCHER )
	{
		if ( !m_bHasRocketlauncher ) 
			return CB_NOT_RESEARCHED;
	}

	return BaseClass::CanBuild( iObjectType );
}


int CPlayerClassDefender::CanBuildSentryGun()
{
	return 
		CanBuild( OBJ_SENTRYGUN_ROCKET_LAUNCHER ) == CB_CAN_BUILD ||
		CanBuild( OBJ_SENTRYGUN_PLASMA ) == CB_CAN_BUILD;
}


//-----------------------------------------------------------------------------
// Purpose: Object has been built by this player
//-----------------------------------------------------------------------------
void CPlayerClassDefender::FinishedObject( CBaseObject *pObject )
{
	UpdateSentrygunTechnology();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassDefender::PlayerDied( CBaseEntity *pAttacker )
{
	CWeaponLimpetmine *weapon = (CWeaponLimpetmine*)m_pPlayer->Weapon_OwnsThisType( "weapon_limpetmine" );
	if ( weapon )
	{
		weapon->RemoveDeployedLimpets();
	}

	BaseClass::PlayerDied( pAttacker );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassDefender::SetPlayerHull( void )
{
	if ( m_pPlayer->GetFlags() & FL_DUCKING )
	{
		m_pPlayer->SetCollisionBounds( DEFENDERCLASS_HULL_DUCK_MIN, DEFENDERCLASS_HULL_DUCK_MAX );
	}
	else
	{
		m_pPlayer->SetCollisionBounds( DEFENDERCLASS_HULL_STAND_MIN, DEFENDERCLASS_HULL_STAND_MAX );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassDefender::GetPlayerHull( bool bDucking, Vector &vecMin, Vector &vecMax )
{
	if ( bDucking )
	{
		VectorCopy( DEFENDERCLASS_HULL_DUCK_MIN, vecMin );
		VectorCopy( DEFENDERCLASS_HULL_DUCK_MAX, vecMax );
	}
	else
	{
		VectorCopy( DEFENDERCLASS_HULL_STAND_MIN, vecMin );
		VectorCopy( DEFENDERCLASS_HULL_STAND_MAX, vecMax );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassDefender::ResetViewOffset( void )
{
	if ( m_pPlayer )
	{
		m_pPlayer->SetViewOffset( DEFENDERCLASS_VIEWOFFSET_STAND );
	}
}

void CPlayerClassDefender::CreatePersonalOrder()
{
	if ( CreateInitialOrder() )
		return;

	if( COrderRepair::CreateOrder_RepairFriendlyObjects( this ) )
		return;

	// Alternate between sentrygun and sandbag orders.
	if ( OrderCreator_BuildSentryGun( this ) )
	{
		return;
	}
	
	BaseClass::CreatePersonalOrder();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassDefender::InitVCollision( void )
{
	CPhysCollide *pStandModel = PhysCreateBbox( DEFENDERCLASS_HULL_STAND_MIN, DEFENDERCLASS_HULL_STAND_MAX );
	CPhysCollide *pCrouchModel = PhysCreateBbox( DEFENDERCLASS_HULL_DUCK_MIN, DEFENDERCLASS_HULL_DUCK_MAX );
	m_pPlayer->SetupVPhysicsShadow( pStandModel, "tfplayer_defender_stand", pCrouchModel, "tfplayer_defender_crouch" );
}
