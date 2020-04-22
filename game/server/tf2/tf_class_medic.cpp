//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The Medic Player Class
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basecombatweapon.h"
#include "tf_player.h"
#include "tf_class_medic.h"
#include "tf_obj.h"
#include "tf_obj_resupply.h"
#include "tf_obj_resourcepump.h"
#include "weapon_builder.h"
#include "tf_team.h"
#include "orders.h"
#include "entitylist.h"
#include "tf_func_resource.h"
#include "order_resupply.h"
#include "order_resourcepump.h"
#include "order_heal.h"
#include "weapon_twohandedcontainer.h"
#include "weapon_combatshield.h"

// Radius buff defines
#define RADIUS_BUFF_RANGE			512.0

#define MEDIC_HEAL_TIME_AFTER_DAMAGE		10.0

// Medic
ConVar	class_medic_speed( "class_medic_speed","200", FCVAR_NONE, "Medic movement speed" );

//=============================================================================
//
// Medic Data Table
//
BEGIN_SEND_TABLE_NOBASE( CPlayerClassMedic, DT_PlayerClassMedicData )
END_SEND_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CPlayerClassMedic::GetClassModelString( int nTeam )
{
	if (nTeam == TEAM_HUMANS)
		return "models/player/human_medic.mdl";
	else
		return "models/player/recon.mdl";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlayerClassMedic::CPlayerClassMedic( CBaseTFPlayer *pPlayer, TFClass iClass ) : CPlayerClass( pPlayer, iClass )
{
	for (int i = 0; i < MAX_TF_TEAMS; ++i)
	{
		SetClassModel( MAKE_STRING(GetClassModelString(i)), i ); 
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPlayerClassMedic::SetupSizeData( void )
{
	// Initially set the player to the base player class standing hull size.
	m_pPlayer->SetCollisionBounds( MEDICCLASS_HULL_STAND_MIN, MEDICCLASS_HULL_STAND_MAX );
	m_pPlayer->SetViewOffset( MEDICCLASS_VIEWOFFSET_STAND );
	m_pPlayer->m_Local.m_flStepSize = MEDICCLASS_STEPSIZE;	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlayerClassMedic::~CPlayerClassMedic()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassMedic::ClassActivate( void )
{
	BaseClass::ClassActivate();

	// Setup movement data.
	SetupMoveData();
	
	m_hWpnShield = NULL;
	m_hWpnPlasma = NULL;

	m_bHasAutoRepair = false;

	m_flNextHealTime = 0;

	memset( &m_ClassData, 0, sizeof( m_ClassData ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassMedic::ClassDeactivate( void )
{
	BaseClass::ClassDeactivate();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassMedic::CreateClass( void )
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
void CPlayerClassMedic::RespawnClass( void )
{
	BaseClass::RespawnClass();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPlayerClassMedic::ResupplyAmmo( float flFraction, ResupplyReason_t reason )
{
	bool bGiven = false;

	if ((reason == RESUPPLY_ALL_FROM_STATION) || (reason == RESUPPLY_GRENADES_FROM_STATION))
	{
		if (ResupplyAmmoType( 3 * flFraction, "Grenades" ))
			bGiven = true;
	}

	if ((reason == RESUPPLY_ALL_FROM_STATION) || (reason == RESUPPLY_AMMO_FROM_STATION))
	{
	}

	if ( reason == RESUPPLY_RESPAWN )
	{
	}

	if ( BaseClass::ResupplyAmmo(flFraction, reason) )
		bGiven = true;
	return bGiven;
}

//-----------------------------------------------------------------------------
// Purpose: Set medic class specific movement data here.
//-----------------------------------------------------------------------------
void CPlayerClassMedic::SetupMoveData( void )
{
	// Setup Class statistics
	m_flMaxWalkingSpeed = class_medic_speed.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame by postthink
//-----------------------------------------------------------------------------
void CPlayerClassMedic::ClassThink( void )
{
	// Heal me if it's time (and I'm not dead!)
	if ( m_pPlayer->IsAlive() && m_flNextHealTime && m_flNextHealTime < gpGlobals->curtime )
	{
		if ( m_pPlayer->GetHealth() < m_pPlayer->GetMaxHealth() )
		{
			// Heal the player
			m_pPlayer->TakeHealth( 1.0, DMG_GENERIC );
			m_flNextHealTime = gpGlobals->curtime + 0.1;
		}
		else
		{
			m_flNextHealTime = 0;
		}
	}

	BaseClass::ClassThink();
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this player's allowed to build another one of the specified objects
//-----------------------------------------------------------------------------
int CPlayerClassMedic::CanBuild( int iObjectType )
{
	return BaseClass::CanBuild( iObjectType );
}

//-----------------------------------------------------------------------------
// Purpose: Update abilities based on new technologies gained
//-----------------------------------------------------------------------------
void CPlayerClassMedic::GainedNewTechnology( CBaseTechnology *pTechnology )
{
	// Autorepair
	m_bHasAutoRepair = m_pPlayer->HasNamedTechnology( "obj_autorepair" );

	BaseClass::GainedNewTechnology( pTechnology );
}

//-----------------------------------------------------------------------------
// Purpose: Create a personal order for this player
//-----------------------------------------------------------------------------
void CPlayerClassMedic::CreatePersonalOrder( void )
{
	if ( CreateInitialOrder() )
		return;

	// Check for healing orders.
	if ( COrderHeal::CreateOrder( this ) )
		return;

	// Should they build a resource pump?
	if ( COrderResourcePump::CreateOrder( this ) )
		return;

	// Build a resupply station?
	if ( COrderResupply::CreateOrder( this ) )
		return;

	BaseClass::CreatePersonalOrder();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassMedic::SetPlayerHull( void )
{
	if ( m_pPlayer->GetFlags() & FL_DUCKING )
	{
		m_pPlayer->SetCollisionBounds( MEDICCLASS_HULL_DUCK_MIN, MEDICCLASS_HULL_DUCK_MAX );
	}
	else
	{
		m_pPlayer->SetCollisionBounds( MEDICCLASS_HULL_STAND_MIN, MEDICCLASS_HULL_STAND_MAX );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassMedic::GetPlayerHull( bool bDucking, Vector &vecMin, Vector &vecMax )
{
	if ( bDucking )
	{
		VectorCopy( MEDICCLASS_HULL_DUCK_MIN, vecMin );
		VectorCopy( MEDICCLASS_HULL_DUCK_MAX, vecMax );
	}
	else
	{
		VectorCopy( MEDICCLASS_HULL_STAND_MIN, vecMin );
		VectorCopy( MEDICCLASS_HULL_STAND_MAX, vecMax );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassMedic::ResetViewOffset( void )
{
	if ( m_pPlayer )
	{
		m_pPlayer->SetViewOffset( MEDICCLASS_VIEWOFFSET_STAND );
	}
}

//-----------------------------------------------------------------------------
// Purpose: The player has taken damage. Return the damage done.
//-----------------------------------------------------------------------------
float CPlayerClassMedic::OnTakeDamage( const CTakeDamageInfo &info )
{
	// Stop healing when taking damage
	m_flNextHealTime = gpGlobals->curtime + MEDIC_HEAL_TIME_AFTER_DAMAGE;

	return BaseClass::OnTakeDamage( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassMedic::InitVCollision( void )
{
	CPhysCollide *pStandModel = PhysCreateBbox( MEDICCLASS_HULL_STAND_MIN, MEDICCLASS_HULL_STAND_MAX );
	CPhysCollide *pCrouchModel = PhysCreateBbox( MEDICCLASS_HULL_DUCK_MIN, MEDICCLASS_HULL_DUCK_MAX );
	m_pPlayer->SetupVPhysicsShadow( pStandModel, "tfplayer_medic_stand", pCrouchModel, "tfplayer_medic_crouch" );
}
