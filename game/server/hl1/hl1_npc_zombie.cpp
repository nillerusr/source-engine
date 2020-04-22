//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A slow-moving, once-human headcrab victim with only melee attacks.
//
// UNDONE: Make head take 100% damage, body take 30% damage.
// UNDONE: Don't flinch every time you get hit.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "game.h"
#include "ai_default.h"
#include "ai_schedule.h"
#include "ai_hull.h"
#include "ai_route.h"
#include "npcevent.h"
#include "hl1_npc_zombie.h"
#include "gib.h"
//#include "AI_Interactions.h"
#include "ndebugoverlay.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"

ConVar	sk_zombie_health( "sk_zombie_health","50");
ConVar  sk_zombie_dmg_one_slash( "sk_zombie_dmg_one_slash", "20" );
ConVar  sk_zombie_dmg_both_slash( "sk_zombie_dmg_both_slash", "40" );



LINK_ENTITY_TO_CLASS( monster_zombie, CNPC_Zombie );


//=========================================================
// Spawn
//=========================================================
void CNPC_Zombie::Spawn()
{
	Precache( );

	SetModel( "models/zombie.mdl" );
	
	SetRenderColor( 255, 255, 255, 255 );
	
	SetHullType(HULL_HUMAN);
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	m_bloodColor		= BLOOD_COLOR_GREEN;
    m_iHealth			= sk_zombie_health.GetFloat();
	//pev->view_ofs		= VEC_VIEW;// position of the eyes relative to monster's origin.
	m_flFieldOfView		= 0.5;
	m_NPCState			= NPC_STATE_NONE;
	CapabilitiesClear();
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_DOORS_GROUP );

	NPCInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_Zombie::Precache()
{
	PrecacheModel( "models/zombie.mdl" );

	PrecacheScriptSound( "Zombie.AttackHit" );
	PrecacheScriptSound( "Zombie.AttackMiss" );
	PrecacheScriptSound( "Zombie.Pain" );
	PrecacheScriptSound( "Zombie.Idle" );
	PrecacheScriptSound( "Zombie.Alert" );
	PrecacheScriptSound( "Zombie.Attack" );

	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose: Returns this monster's place in the relationship table.
//-----------------------------------------------------------------------------
Class_T	CNPC_Zombie::Classify( void )
{
	return CLASS_ALIEN_MONSTER; 
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CNPC_Zombie::HandleAnimEvent( animevent_t *pEvent )
{
	Vector v_forward, v_right;
	switch( pEvent->event )
	{
		case ZOMBIE_AE_ATTACK_RIGHT:
		{
			// do stuff for this event.
	//		ALERT( at_console, "Slash right!\n" );

			Vector vecMins = GetHullMins();
			Vector vecMaxs = GetHullMaxs();
			vecMins.z = vecMins.x;
			vecMaxs.z = vecMaxs.x;

			CBaseEntity *pHurt = CheckTraceHullAttack( 70, vecMins, vecMaxs, sk_zombie_dmg_one_slash.GetFloat(), DMG_SLASH );
			CPASAttenuationFilter filter( this );
			if ( pHurt )
			{
				if ( pHurt->GetFlags() & ( FL_NPC | FL_CLIENT ) )
				{
					pHurt->ViewPunch( QAngle( 5, 0, 18 ) );
					
					GetVectors( &v_forward, &v_right, NULL );

					pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() - v_right * 100 );
				}
				// Play a random attack hit sound
				EmitSound( filter, entindex(), "Zombie.AttackHit" );
			}
			else // Play a random attack miss sound
				EmitSound( filter, entindex(), "Zombie.AttackMiss" );

			if ( random->RandomInt( 0, 1 ) )
				 AttackSound();
		}
		break;

		case ZOMBIE_AE_ATTACK_LEFT:
		{
			// do stuff for this event.
	//		ALERT( at_console, "Slash left!\n" );
			Vector vecMins = GetHullMins();
			Vector vecMaxs = GetHullMaxs();
			vecMins.z = vecMins.x;
			vecMaxs.z = vecMaxs.x;

			CBaseEntity *pHurt = CheckTraceHullAttack( 70, vecMins, vecMaxs, sk_zombie_dmg_one_slash.GetFloat(), DMG_SLASH );
			
			CPASAttenuationFilter filter2( this );
			if ( pHurt )
			{
				if ( pHurt->GetFlags() & ( FL_NPC | FL_CLIENT ) )
				{
					pHurt->ViewPunch( QAngle ( 5, 0, -18 ) );
					
					GetVectors( &v_forward, &v_right, NULL );

					pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() - v_right * 100 );
				}
				EmitSound( filter2, entindex(), "Zombie.AttackHit" );
			}
			else
			{
				EmitSound( filter2, entindex(), "Zombie.AttackMiss" );
			}

			if ( random->RandomInt( 0,1 ) ) 
				 AttackSound();
		}
		break;

		case ZOMBIE_AE_ATTACK_BOTH:
		{
			// do stuff for this event.
			Vector vecMins = GetHullMins();
			Vector vecMaxs = GetHullMaxs();
			vecMins.z = vecMins.x;
			vecMaxs.z = vecMaxs.x;

			CBaseEntity *pHurt = CheckTraceHullAttack( 70, vecMins, vecMaxs, sk_zombie_dmg_both_slash.GetFloat(), DMG_SLASH );
			

			CPASAttenuationFilter filter3( this );
			if ( pHurt )
			{
				if ( pHurt->GetFlags() & ( FL_NPC | FL_CLIENT ) )
				{
					pHurt->ViewPunch( QAngle ( 5, 0, 0 ) );
					
					GetVectors( &v_forward, &v_right, NULL );
					pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() - v_right * 100 );
				}
				EmitSound( filter3, entindex(), "Zombie.AttackHit" );
			}
			else
				EmitSound( filter3, entindex(),"Zombie.AttackMiss" );

			if ( random->RandomInt( 0,1 ) )
				 AttackSound();
		}
		break;

		default:
			BaseClass::HandleAnimEvent( pEvent );
			break;
	}
}


static float DamageForce( const Vector &size, float damage )
{ 
	float force = damage * ((32 * 32 * 72.0) / (size.x * size.y * size.z)) * 5;
	
	if ( force > 1000.0) 
	{
		force = 1000.0;
	}

	return force;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pInflictor - 
//			pAttacker - 
//			flDamage - 
//			bitsDamageType - 
// Output : int
//-----------------------------------------------------------------------------
int CNPC_Zombie::OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo info = inputInfo;

	// Take 30% damage from bullets
	if ( info.GetDamageType() == DMG_BULLET )
	{
		Vector vecDir = GetAbsOrigin() - info.GetInflictor()->WorldSpaceCenter();
		VectorNormalize( vecDir );
		float flForce = DamageForce( WorldAlignSize(), info.GetDamage() );
		SetAbsVelocity( GetAbsVelocity() + vecDir * flForce );
		info.ScaleDamage( 0.3f );
	}

	// HACK HACK -- until we fix this.
	if ( IsAlive() )
		 PainSound( info );
	
	return BaseClass::OnTakeDamage_Alive( info );
}

void CNPC_Zombie::PainSound( const CTakeDamageInfo &info )
{
	if ( random->RandomInt(0,5) < 2)
	{
		CPASAttenuationFilter filter( this );
		EmitSound( filter, entindex(), "Zombie.Pain" );
	}
}

void CNPC_Zombie::AlertSound( void )
{
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Zombie.Alert" );
}

void CNPC_Zombie::IdleSound( void )
{
	// Play a random idle sound
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Zombie.Idle" );
}

void CNPC_Zombie::AttackSound( void )
{
	// Play a random attack sound
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Zombie.Attack" );
}

int CNPC_Zombie::MeleeAttack1Conditions ( float flDot, float flDist )
{
	if ( flDist > 64)
	{
		return COND_TOO_FAR_TO_ATTACK;
	}
	else if (flDot < 0.7)
	{
		return 0;
	}
	else if (GetEnemy() == NULL)
	{
		return 0;
	}

	return COND_CAN_MELEE_ATTACK1;
}

void CNPC_Zombie::RemoveIgnoredConditions ( void )
{
	if ( GetActivity() == ACT_MELEE_ATTACK1 )
	{
		// Nothing stops an attacking zombie.
		ClearCondition( COND_LIGHT_DAMAGE );
		ClearCondition( COND_HEAVY_DAMAGE );
	}

	if (( GetActivity() == ACT_SMALL_FLINCH ) || ( GetActivity() == ACT_BIG_FLINCH ))
	{
		if (m_flNextFlinch < gpGlobals->curtime)
			m_flNextFlinch = gpGlobals->curtime + ZOMBIE_FLINCH_DELAY;
	}

	BaseClass::RemoveIgnoredConditions();
}
