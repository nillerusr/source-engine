//=============================================================================//
//
// Purpose:		Metrocop husks.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "npc_husk_police.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Husks generally use 90% of the original NPC's health

ConVar	sk_husk_police_health( "sk_husk_police_health", "80" ); // From 90

//-----------------------------------------------------------------------------

LINK_ENTITY_TO_CLASS( npc_husk_police, CNPC_HuskPolice );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_HuskPolice )

	DEFINE_USEFUNC( HuskUse ),

	DEFINE_BASE_HUSK_DATADESC()

END_DATADESC()

//---------------------------------------------------------
// VScript
//---------------------------------------------------------
BEGIN_ENT_SCRIPTDESC( CNPC_HuskPolice, CAI_BaseActor, "Husk metrocop." )

	DEFINE_BASE_HUSK_SCRIPTDESC()

END_SCRIPTDESC()

//---------------------------------------------------------
// Custom Client entity
//---------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST( CNPC_HuskPolice, DT_NPC_HuskPolice )

	DEFINE_BASE_HUSK_SENDPROPS()

END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNPC_HuskPolice::CNPC_HuskPolice()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_HuskPolice::Spawn( void )
{
	BaseClass::Spawn();

	m_flFieldOfView = 0.5f;

	SetHealth( sk_husk_police_health.GetInt() );
	SetMaxHealth( GetHealth() );

	SetUse( &CNPC_HuskPolice::HuskUse );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_HuskPolice::Precache()
{
	if ( GetModelName() == NULL_STRING )
	{
		switch (m_tEzVariant)
		{
			case EZ_VARIANT_ATHENAEUM:
				SetModelName( MAKE_STRING( "models/husks/athenaeum/husk_police.mdl" ) );
				break;

			default:
				SetModelName( MAKE_STRING( "models/husks/husk_police.mdl" ) );
				break;
		}
	}

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_HuskPolice::GetGameTextSpeechParams( hudtextparms_t &params )
{
	if (GetEZVariant() == EZ_VARIANT_ATHENAEUM)
	{
		params.r1 = 72; params.g1 = 64; params.b1 = 128;
	}
	else
	{
		params.r1 = 128; params.g1 = 96; params.b1 = 64;
	}
	
	return BaseClass::GetGameTextSpeechParams( params );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_HuskPolice::HuskUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	BaseClass::PrecriminalUse( pActivator, pCaller, useType, value );
	
	Disposition_t relation = IRelationType( pActivator );
	if ( (relation == D_HT || relation == D_FR) && !FInViewCone( pActivator ) )
	{
		// Don't like being used without warning
		MakeAngry( pActivator );
		UpdateEnemyMemory( pActivator, pActivator->GetAbsOrigin(), pActivator );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
NPC_STATE CNPC_HuskPolice::SelectIdealState( void )
{
	switch ( m_NPCState )
	{
	case NPC_STATE_COMBAT:
		{
			if ( GetEnemy() == NULL )
			{
				if ( !HasCondition( COND_ENEMY_DEAD ) )
				{
					// Lost track of my enemy. Patrol.
					SetCondition( COND_HUSK_POLICE_SHOULD_PATROL );
				}
				return NPC_STATE_ALERT;
			}
		}

	default:
		{
			return BaseClass::SelectIdealState();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CNPC_HuskPolice::SelectAlertSchedule( void )
{
	if ( HasCondition( COND_HUSK_POLICE_SHOULD_PATROL ) )
		return SCHED_PATROL_WALK;

	return BaseClass::SelectAlertSchedule();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CNPC_HuskPolice::SelectCombatSchedule( void )
{
	if ( IsSuspicious() )
	{
		// Only face the enemy
		return SCHED_COMBAT_FACE;
	}

	return BaseClass::SelectCombatSchedule();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
WeaponProficiency_t CNPC_HuskPolice::CalcWeaponProficiency( CBaseCombatWeapon *pWeapon )
{
	if( pWeapon->ClassMatches( gm_isz_class_SMG1 ) )
	{
		return WEAPON_PROFICIENCY_AVERAGE;
	}
	else if ( pWeapon->ClassMatches( gm_isz_class_Pistol ) || pWeapon->ClassMatches( "weapon_pulsepistol" ) )
	{
		return WEAPON_PROFICIENCY_POOR;
	}
	else if (FClassnameIs( pWeapon, "weapon_smg2" ))
	{
		return WEAPON_PROFICIENCY_AVERAGE;
	}

	return WEAPON_PROFICIENCY_AVERAGE; // Override base
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_husk_police, CNPC_HuskPolice )

	DECLARE_CONDITION( COND_HUSK_POLICE_SHOULD_PATROL )

 AI_END_CUSTOM_NPC()
