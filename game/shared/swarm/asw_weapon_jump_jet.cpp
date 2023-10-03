#include "cbase.h"
#include "asw_weapon_jump_jet.h"
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

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Jump_Jet, DT_ASW_Weapon_Jump_Jet )

BEGIN_NETWORK_TABLE( CASW_Weapon_Jump_Jet, DT_ASW_Weapon_Jump_Jet )
#ifdef CLIENT_DLL
#else
	SendPropExclude( "DT_ASW_Weapon_Blink", "m_flPower" ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CASW_Weapon_Jump_Jet )
	
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( asw_weapon_jump_jet, CASW_Weapon_Jump_Jet );
PRECACHE_WEAPON_REGISTER( asw_weapon_jump_jet );

#ifndef CLIENT_DLL

BEGIN_DATADESC( CASW_Weapon_Jump_Jet )

END_DATADESC()

extern ConVar asw_blink_debug;

#else


#endif /* not client */

extern ConVar asw_blink_range;
ConVar asw_jump_jet_time( "asw_jump_jet_time", "1.32", FCVAR_REPLICATED );

CASW_Weapon_Jump_Jet::CASW_Weapon_Jump_Jet()
{
}


CASW_Weapon_Jump_Jet::~CASW_Weapon_Jump_Jet()
{
}

void CASW_Weapon_Jump_Jet::Spawn()
{
	BaseClass::Spawn();
}

void CASW_Weapon_Jump_Jet::PrimaryAttack( void )
{
	CASW_Player *pPlayer = GetCommander();
	if (!pPlayer)
		return;

	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine || pMarine->GetCurrentMeleeAttack() || pMarine->GetForcedActionRequest() != 0 )
		return;

	if ( m_iClip1 <= 0 ) 
		return;

	if ( pMarine->IsInhabited() )
	{
		// Get the server to verify destination and do a forced action to actually jet
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

		pMarine->RequestForcedAction( FORCED_ACTION_JUMP_JET );
#endif	
	}
}

#ifdef CLIENT_DLL
#endif

void CASW_Weapon_Jump_Jet::DoJumpJet()
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine || pMarine->m_iJumpJetting != JJ_NONE )
		return;

	pMarine->m_iJumpJetting = JJ_JUMP_JETS;
	pMarine->m_vecJumpJetStart = pMarine->GetAbsOrigin();
	pMarine->m_vecJumpJetEnd = m_vecAbilityDestination;
	pMarine->m_flJumpJetStartTime = gpGlobals->curtime;
	pMarine->m_flJumpJetEndTime = gpGlobals->curtime + asw_jump_jet_time.GetFloat();

	ASWMeleeSystem()->StartMeleeAttack( ASWMeleeSystem()->GetMeleeAttackByName( "JumpJet" ), pMarine, ASWGameMovement()->GetMoveData() );

#ifndef CLIENT_DLL
	pMarine->OnWeaponFired( this, 1 );
#endif

	m_iClip1 -= 1;

#ifndef CLIENT_DLL
	DestroyIfEmpty( true );
#endif
}