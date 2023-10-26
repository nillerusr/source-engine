#include "cbase.h"
#include "asw_weapon_blink.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#include "prediction.h"
#else
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "npcevent.h"
#include "shot_manipulator.h"
#include "asw_path_utils.h"
#include "ai_network.h"
#include "ai_waypoint.h"
#include "ai_node.h"
#include "asw_trace_filter_doors.h"
#include "ai_navigator.h"
#include "ai_pathfinder.h"
#endif
#include "asw_marine_gamemovement.h"
#include "asw_melee_system.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Blink, DT_ASW_Weapon_Blink )

BEGIN_NETWORK_TABLE( CASW_Weapon_Blink, DT_ASW_Weapon_Blink )
#ifdef CLIENT_DLL
	RecvPropFloat	( RECVINFO( m_flPower ) ),
	RecvPropVector	( RECVINFO( m_vecAbilityDestination ) ),
#else
	SendPropFloat	( SENDINFO( m_flPower ), 0, SPROP_NOSCALE ),
	SendPropVector  ( SENDINFO( m_vecAbilityDestination ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CASW_Weapon_Blink )
	DEFINE_PRED_FIELD_TOL( m_flPower, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),		
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( asw_weapon_blink, CASW_Weapon_Blink );
PRECACHE_WEAPON_REGISTER( asw_weapon_blink );

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Blink )
	DEFINE_FIELD( m_flSoonestPrimaryAttack, FIELD_TIME ),
END_DATADESC()

ConVar asw_blink_debug( "asw_blink_debug", "0", FCVAR_CHEAT );

#else


#endif /* not client */

ConVar asw_blink_charge_time( "asw_blink_charge_time", "30.0", FCVAR_CHEAT | FCVAR_REPLICATED );
ConVar asw_blink_range( "asw_blink_range", "2000", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_blink_time( "asw_blink_time", "0.6", FCVAR_REPLICATED | FCVAR_CHEAT );

CASW_Weapon_Blink::CASW_Weapon_Blink()
{
	m_flSoonestPrimaryAttack = 0;
	m_flPower = asw_blink_charge_time.GetFloat();
}


CASW_Weapon_Blink::~CASW_Weapon_Blink()
{

}

void CASW_Weapon_Blink::Spawn()
{
	BaseClass::Spawn();

	m_flPower = asw_blink_charge_time.GetFloat();
}

bool CASW_Weapon_Blink::OffhandActivate()
{
	if (!GetMarine() || GetMarine()->GetFlags() & FL_FROZEN)	// don't allow this if the marine is frozen
		return false;

	PrimaryAttack();	

	return true;
}

#ifdef GAME_DLL
bool CASW_Weapon_Blink::SetBlinkDestination()
{
	CASW_Player *pPlayer = GetCommander();
	if ( !pPlayer )
		return false;

	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return false;

	Vector vecStart = pPlayer->GetCrosshairTracePos() + Vector( 0, 0, 30 );
	Vector vecEnd = pPlayer->GetCrosshairTracePos() - Vector( 0, 0, 30 );
	trace_t tr;
	UTIL_TraceHull( vecStart, vecEnd, pMarine->WorldAlignMins(), pMarine->WorldAlignMaxs(), MASK_PLAYERSOLID_BRUSHONLY, pMarine, COLLISION_GROUP_PLAYER_MOVEMENT, &tr );
	if ( tr.startsolid || tr.allsolid )
	{
		m_vecInvalidDestination = vecStart;
		return false;
	}

	if ( pMarine->GetAbsOrigin().DistTo( tr.endpos ) > asw_blink_range.GetFloat() )
	{
		m_vecInvalidDestination = tr.endpos;
		return false;
	}

	Vector vecDest = tr.endpos;

	// now see if we can build an AI path from the marine to this spot
	bool bValidRoute = false;

	if ( !pMarine->GetPathfinder() )
	{
		m_vecInvalidDestination = vecDest;
		return false;
	}

	AI_Waypoint_t *pRoute = pMarine->GetPathfinder()->BuildRoute( pMarine->GetAbsOrigin(), vecDest, NULL, 30, NAV_GROUND, bits_BUILD_GROUND | bits_BUILD_IGNORE_NPCS );
	if ( pRoute && !UTIL_ASW_DoorBlockingRoute( pRoute, true ) )
	{
		if ( !UTIL_ASW_BrushBlockingRoute( pRoute, MASK_PLAYERSOLID_BRUSHONLY, COLLISION_GROUP_PLAYER_MOVEMENT ) )
		{
			// if end node of the route is too Z different, then abort, to stop people jumping on top of walls
			AI_Waypoint_t *pLast = pRoute->GetLast();
			if ( pLast )
			{
				AI_Waypoint_t *pNode = pLast->GetPrev();
				if ( !pNode || fabs( pNode->GetPos().z - pLast->GetPos().z ) < 80.0f )
				{
					bValidRoute = true;
				}
			}
		}
	}
	
	if ( !bValidRoute )
	{
		// find the closest node to the dest and try to path there instead
		CAI_Network *pNetwork = pMarine->GetNavigator() ? pMarine->GetNavigator()->GetNetwork() : NULL;
		if ( pNetwork )
		{
			int nNode = pNetwork->NearestNodeToPoint( vecDest, false );
			if ( nNode != NO_NODE )
			{
				CAI_Node *pNode = pNetwork->GetNode( nNode );
				if ( pNode && pNode->GetType() == NODE_GROUND )
				{
					vecDest = pNode->GetOrigin();
					if ( pRoute )
					{
						ASWPathUtils()->DeleteRoute( pRoute );
						pRoute = NULL;
					}
					pRoute = pMarine->GetPathfinder()->BuildRoute( pMarine->GetAbsOrigin(), vecDest, NULL, 30, NAV_GROUND, bits_BUILD_GROUND | bits_BUILD_IGNORE_NPCS );
					if ( pRoute && !UTIL_ASW_DoorBlockingRoute( pRoute, true ) )
					{
						if ( !UTIL_ASW_BrushBlockingRoute( pRoute, MASK_PLAYERSOLID_BRUSHONLY, COLLISION_GROUP_PLAYER_MOVEMENT ) )
						{
							bValidRoute = true;
						}
					}
					if ( !bValidRoute )
					{
						m_vecInvalidDestination = vecDest;
					}
				}
			}
		}
	}

	if ( !bValidRoute )
	{
		if ( pRoute )
		{
			ASWPathUtils()->DeleteRoute( pRoute );
			pRoute = NULL;
		}
		return false;
	}

	if ( asw_blink_debug.GetBool() )
	{
		ASWPathUtils()->DebugDrawRoute( pMarine->GetAbsOrigin(), pRoute );
	}

	m_vecAbilityDestination = vecDest;

	if ( pRoute )
	{
		ASWPathUtils()->DeleteRoute( pRoute );
		pRoute = NULL;
	}

	return true;
}
#endif

void CASW_Weapon_Blink::PrimaryAttack( void )
{
	CASW_Player *pPlayer = GetCommander();
	if (!pPlayer)
		return;

	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine || pMarine->GetCurrentMeleeAttack() || pMarine->GetForcedActionRequest() != 0 )
		return;

	if ( GetPower() < asw_blink_charge_time.GetFloat() )
	{
#ifdef CLIENT_DLL
		EmitSound( "ASW_Weapon.InsufficientBattery" );
#else
		if ( gpGlobals->maxClients <= 1 )
		{
			EmitSound( "ASW_Weapon.InsufficientBattery" );
		}
#endif
		return;
	}

	if ( pMarine->IsInhabited() )
	{
		// Get the server to verify destination and do a forced action to actually blink
#ifdef GAME_DLL
		if ( !SetBlinkDestination() )
		{
			CSingleUserRecipientFilter user( pPlayer );
			UserMessageBegin( user, "ASWInvalidDesination" );
			WRITE_SHORT( pMarine->entindex() );
			WRITE_FLOAT( m_vecInvalidDestination.x );
			WRITE_FLOAT( m_vecInvalidDestination.y );
			WRITE_FLOAT( m_vecInvalidDestination.z );
			MessageEnd();
			return;
		}

		pMarine->RequestForcedAction( FORCED_ACTION_BLINK );
		pMarine->OnWeaponFired( this, 1 );
#endif	
	}
}

void CASW_Weapon_Blink::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "ASW_Weapon.BatteryCharged" );
	PrecacheScriptSound( "ASW_Weapon.InsufficientBattery" );
}

// this weapon doesn't reload
bool CASW_Weapon_Blink::Reload()
{
	return false;
}

void CASW_Weapon_Blink::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	UpdatePower();

	if ( m_bInReload )
		return;
	
	CBasePlayer *pOwner = GetCommander();
	if ( !pOwner )
		return;

	//Allow a refire as fast as the player can click
	if ( ( ( pOwner->m_nButtons & IN_ATTACK ) == false ) && ( m_flSoonestPrimaryAttack < gpGlobals->curtime ) )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime - 0.1f;
	}
}

void CASW_Weapon_Blink::HandleFireOnEmpty()
{
	// Nightvision doesn't require ammo, so when firing empty, activate the NV
	PrimaryAttack();
}

void CASW_Weapon_Blink::UpdatePower()
{
	if ( GetPower() < asw_blink_charge_time.GetFloat() )
	{
		float flNewPower = GetPower() + gpGlobals->frametime * 1.4f;
		flNewPower = MIN( asw_blink_charge_time.GetFloat(), flNewPower );
		m_flPower = flNewPower;

		if ( flNewPower >= asw_blink_charge_time.GetFloat() )
		{
#ifdef CLIENT_DLL
			EmitSound( "ASW_Weapon.BatteryCharged" );
#else
			if ( gpGlobals->maxClients <= 1 )
			{
				EmitSound( "ASW_Weapon.BatteryCharged" );
			}
#endif
		}
	}
}

#ifdef CLIENT_DLL
#endif

int CASW_Weapon_Blink::ASW_SelectWeaponActivity(int idealActivity)
{
	// we just use the normal 'no weapon' anims for this
	return idealActivity;
}

// needs to be fully charged to activate
float CASW_Weapon_Blink::GetMinBatteryChargeToActivate()
{
	return 1.0f;
}

float CASW_Weapon_Blink::GetBatteryCharge()
{
	return GetPower() / asw_blink_charge_time.GetFloat();
}

void CASW_Weapon_Blink::DoBlink()
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return;

	pMarine->m_iJumpJetting = JJ_BLINK;
	pMarine->m_vecJumpJetStart = pMarine->GetAbsOrigin();
	pMarine->m_vecJumpJetEnd = m_vecAbilityDestination;
	pMarine->m_flJumpJetStartTime = gpGlobals->curtime;
	pMarine->m_flJumpJetEndTime = gpGlobals->curtime + asw_blink_time.GetFloat();

	ASWMeleeSystem()->StartMeleeAttack( ASWMeleeSystem()->GetMeleeAttackByName( "Blink" ), pMarine, ASWGameMovement()->GetMoveData() );

	// TODO:
	/*
#ifdef GAME_DLL
	// create a small stun volume at the start
	CASW_Weapon_EffectVolume *pEffect = (CASW_Weapon_EffectVolume*) CreateEntityByName("asw_weapon_effect_volume");
	if ( pEffect )
	{
		pEffect->SetAbsOrigin( pMarine->GetAbsOrigin() );
		pEffect->SetOwnerEntity( pMarine );
		pEffect->SetOwnerWeapon( NULL );
		pEffect->SetEffectFlag( ASW_WEV_ELECTRIC_BIG );
		pEffect->SetDuration( pSkill->GetValue( CASW_Skill_Details::Duration, GetSkillPoints() ) );
		pEffect->Spawn();
	}
#endif
	*/

	m_flPower = 0.0f;

	// TODO: Check for charges being zero
}