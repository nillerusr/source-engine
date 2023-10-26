// A Swarm grub, bursts out of gooey things and crawls about looking icky (non-violent)

#include "cbase.h"
#include "asw_grub.h"
#include "asw_marine.h"
#include "te_effect_dispatch.h"
#include "npc_antlion.h"
#include "npc_bullseye.h"
#include "npcevent.h"
#include "asw_marine.h"
#include "soundenvelope.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	SWARM_GRUB_MODEL	"models/swarm/Grubs/Grub.mdl"

const int ASW_GRUB_MIN_JUMP_DIST = 48;
const int ASW_GRUB_MAX_JUMP_DIST = 170;

#define PARASITE_IGNORE_WORLD_COLLISION_TIME 0.5

IMPLEMENT_SERVERCLASS_ST( CASW_Grub, DT_ASW_Grub )
	
END_SEND_TABLE()

ConVar asw_grub_speedboost( "asw_grub_speedboost", "1.4", FCVAR_CHEAT , "boost speed for the grubs" );
ConVar asw_debug_grubs("asw_debug_grubs", "0", FCVAR_CHEAT, "If set, grubs will print debug messages for various things");
extern ConVar sv_gravity;

int ACT_ASW_GRUB_IDLE;

CASW_Grub::CASW_Grub( void )// : CASW_Alien()
{
	m_bMidJump = false;
	m_bCommittedToJump = false;
}

LINK_ENTITY_TO_CLASS( asw_grub_advanced, CASW_Grub );

//IMPLEMENT_SERVERCLASS_ST( CASW_Grub, DT_ASW_Grub )
	
//END_SEND_TABLE()

BEGIN_DATADESC( CASW_Grub )
	DEFINE_FIELD( m_bCommittedToJump, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bMidJump, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vecCommittedJumpPos, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_flNextNPCThink, FIELD_TIME ),
	DEFINE_FIELD( m_flIgnoreWorldCollisionTime, FIELD_TIME ),
	DEFINE_FIELD( m_bJumpFromGoo, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flGooJumpDistance, FIELD_FLOAT ),
	DEFINE_FIELD( m_flGooJumpAngle, FIELD_FLOAT ),
	DEFINE_THINKFUNC( LeapThink ),
	DEFINE_ENTITYFUNC( LeapTouch ),
	DEFINE_ENTITYFUNC( NormalTouch ),
END_DATADESC()

enum
{
	SCHED_GRUB_JUMP_FROM_GOO = LAST_ASW_ALIEN_SHARED_SCHEDULE,
	SCHED_ASW_GRUB_WANDER_ANGRILY,
};

enum
{
	TASK_GRUB_JUMP_FROM_GOO = LAST_ASW_ALIEN_SHARED_TASK,
};

extern int AE_HEADCRAB_JUMPATTACK;

void CASW_Grub::Spawn( void )
{
	SetModel( SWARM_GRUB_MODEL );
	Precache();
	BaseClass::Spawn();
	SetModel( SWARM_GRUB_MODEL );

	SetHullType(HULL_TINY);
	SetHullSizeNormal();

	SetViewOffset( Vector(6, 0, 11) ) ;		// Position of the eyes relative to NPC's origin.

	SetNavType( NAV_GROUND );
	SetBloodColor( BLOOD_COLOR_GREEN );
	m_NPCState	= NPC_STATE_NONE;
	m_iHealth	= 1;

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	if ( m_SquadName != NULL_STRING )
	{
		CapabilitiesAdd( bits_CAP_SQUAD );
	}
	SetCollisionGroup( ASW_COLLISION_GROUP_GRUBS );
	
	//SetCollisionGroup( COLLISION_GROUP_DEBRIS );
	CapabilitiesAdd( bits_CAP_MOVE_GROUND );
	//if ( HasSpawnFlags( SF_ANTLION_USE_GROUNDCHECKS ) == false )
	CapabilitiesAdd( bits_CAP_SKIP_NAV_GROUND_CHECK );
	NPCInit();	

	BaseClass::Spawn();

	SetEfficiency( AIE_EFFICIENT );
	SetCollisionGroup( ASW_COLLISION_GROUP_GRUBS );
	
	//RemoveSolidFlags( FSOLID_NOT_STANDABLE );
	SetBlocksLOS(false);
	AddFlag( FL_FLY );
}

CASW_Grub::~CASW_Grub()
{

}

void CASW_Grub::Precache( void )
{
	PrecacheModel( SWARM_GRUB_MODEL );
	//PrecacheModel( "Models/Swarm/Grubs/GrubGib1.mdl" );
	//PrecacheModel( "Models/Swarm/Grubs/GrubGib2.mdl" );
	//PrecacheModel( "Models/Swarm/Grubs/GrubGib3.mdl" );
	PrecacheModel( "Models/Swarm/Grubs/GrubGib4.mdl" );
	//PrecacheModel( "Models/Swarm/Grubs/GrubGib5.mdl" );
	//PrecacheModel( "Models/Swarm/Grubs/GrubGib6.mdl" );

	PrecacheScriptSound("ASW_Parasite.Death");
	PrecacheScriptSound("ASW_Parasite.Attack");
	PrecacheScriptSound("ASW_Parasite.Idle");

	BaseClass::Precache();
}

float CASW_Grub::GetIdealSpeed() const
{
	return asw_grub_speedboost.GetFloat() * BaseClass::GetIdealSpeed() * m_flPlaybackRate;
}


float CASW_Grub::GetIdealAccel( ) const
{
	return GetIdealSpeed() * 1.5f;
}

float CASW_Grub::MaxYawSpeed( void )
{
	return 128.0f;

	switch ( GetActivity() )
	{
	case ACT_IDLE:		
		return 64.0f;
		break;
	
	case ACT_WALK:
		return 64.0f;
		break;
	
	default:
	case ACT_RUN:
		return 64.0f;
		break;
	}

	return 64.0f;
}

// don't collide with other grubs during jump
bool CASW_Grub::ShouldCollide(int collisionGroup, int contentsMask)
{
	/*
	if (m_bMidJump)
		return false;
	if (m_bMidJump &&
		(collisionGroup == COLLISION_GROUP_NONE || collisionGroup == ASW_COLLISION_GROUP_GRUBS) )
	{
		return false;
	}
	if (collisionGroup == ASW_COLLISION_GROUP_GRUBS)
		return false;
	*/
	return BaseClass::ShouldCollide(collisionGroup, contentsMask);
}

void CASW_Grub::AlertSound()
{
	//EmitSound("ASW_Drone.Alert");
	//IdleSound();
}

void CASW_Grub::PainSound( const CTakeDamageInfo &info )
{
	if (gpGlobals->curtime > m_fNextPainSound)
	{
		EmitSound("ASW_Parasite.Death");
		m_fNextPainSound = gpGlobals->curtime + 0.5f;
	}
}

void CASW_Grub::AttackSound()
{
	//EmitSound("ASW_Parasite.Attack");
}

void CASW_Grub::IdleSound()
{	
}

bool CASW_Grub::CorpseGib( const CTakeDamageInfo &info )
{
	CEffectData	data;
	
	data.m_vOrigin = WorldSpaceCenter();
	data.m_vNormal = data.m_vOrigin - info.GetDamagePosition();
	VectorNormalize( data.m_vNormal );
	
	data.m_flScale = RemapVal( m_iHealth, 0, -500, 1, 3 );
	data.m_flScale = clamp( data.m_flScale, 1, 3 );
	data.m_fFlags = (IsOnFire() || (info.GetDamageType() & DMG_BURN)) ? ASW_GIBFLAG_ON_FIRE : 0;

	Msg("grubgib flags: %d burn=%d dt=%d\n", data.m_fFlags, (info.GetDamageType() & DMG_BURN), info.GetDamageType());
	DispatchEffect( "GrubGib", data );

	//CSoundEnt::InsertSound( SOUND_PHYSICS_DANGER, GetAbsOrigin(), 256, 0.5f, this );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Does a jump attack at the given position.
// Input  : bRandomJump - Just hop in a random direction.
//			vecPos - Position to jump at, ignored if bRandom is set to true.
//			bThrown - 
//-----------------------------------------------------------------------------
void CASW_Grub::JumpAttack( bool bRandomJump, const Vector &vecPos, bool bThrown )
{
	Vector vecJumpVel;
	if ( !bRandomJump )
	{
		float gravity = sv_gravity.GetFloat();
		if ( gravity <= 1 )
		{
			gravity = 1;
		}

		// How fast does the headcrab need to travel to reach the position given gravity?
		float flActualHeight = vecPos.z - GetAbsOrigin().z;
		float height = flActualHeight;
		if ( height < 16 )
		{
			height = 60; //16;
		}
		else
		{
			float flMaxHeight = bThrown ? 400 : 120;
			if ( height > flMaxHeight )
			{
				height = flMaxHeight;
			}
		}

		// overshoot the jump by an additional 8 inches
		// NOTE: This calculation jumps at a position INSIDE the box of the enemy (player)
		// so if you make the additional height too high, the crab can land on top of the
		// enemy's head.  If we want to jump high, we'll need to move vecPos to the surface/outside
		// of the enemy's box.
		
		float additionalHeight = 0;
		if ( height < 32 )
		{
			additionalHeight = 8;
		}

		height += additionalHeight;

		// NOTE: This equation here is from vf^2 = vi^2 + 2*a*d
		float speed = sqrt( 2 * gravity * height );
		float time = speed / gravity;

		// add in the time it takes to fall the additional height
		// So the impact takes place on the downward slope at the original height
		time += sqrt( (2 * additionalHeight) / gravity );

		// Scale the sideways velocity to get there at the right time
		VectorSubtract( vecPos, GetAbsOrigin(), vecJumpVel );
		vecJumpVel /= time;

		// Speed to offset gravity at the desired height.
		vecJumpVel.z = speed;

		// Don't jump too far/fast.
		float flJumpSpeed = vecJumpVel.Length();
		float flMaxSpeed = bThrown ? 1000.0f : 650.0f;
		if ( flJumpSpeed > flMaxSpeed )
		{
			vecJumpVel *= flMaxSpeed / flJumpSpeed;
		}
	}
	else
	{
		//
		// Jump hop, don't care where.
		//
		Vector forward, up;
		QAngle angFacing = GetLocalAngles();
		angFacing.y = m_flGooJumpAngle;
		AngleVectors( angFacing, &forward, NULL, &up );
		vecJumpVel = Vector( forward.x, forward.y, forward.z) * random->RandomFloat(100, 150);	// random->RandomFloat(0, 30)
	}

	AttackSound();
	Leap( vecJumpVel );
}



void CASW_Grub::Leap( const Vector &vecVel )
{
	SetTouch( &CASW_Grub::LeapTouch );

	SetCondition( COND_FLOATING_OFF_GROUND );
	SetGroundEntity( NULL );

	m_flIgnoreWorldCollisionTime = gpGlobals->curtime + PARASITE_IGNORE_WORLD_COLLISION_TIME;

	if( HasHeadroom() )
	{
		// Take him off ground so engine doesn't instantly reset FL_ONGROUND.
		UTIL_SetOrigin( this, GetLocalOrigin() + Vector( 0, 0, 1 ) );
	}

	SetAbsVelocity( vecVel );

	// Think every frame so the player sees the headcrab where he actually is...
	m_bMidJump = true;
	SetThink( &CASW_Grub::LeapThink );
	SetNextThink( gpGlobals->curtime );

	//SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
}

void CASW_Grub::LeapThink( void )
{
	if (gpGlobals->curtime > m_flNextNPCThink)
	{
		NPCThink();
		m_flNextNPCThink = gpGlobals->curtime + 0.1;
	}

	if( GetFlags() & FL_ONGROUND )
	{
		SetThink( &CASW_Grub::CallNPCThink );
		SetNextThink( gpGlobals->curtime + 0.1 );
		return;
	}

	SetNextThink( gpGlobals->curtime );
}

void CASW_Grub::NormalTouch(CBaseEntity* pOther)
{
	if (!pOther || !pOther->CollisionProp() || pOther->Classify() != CLASS_ASW_MARINE)	// only get squashed by marines
		return;
	// are we within the marine's x/y?
	//Vector vecMyFlatPos = GetAbsOrigin();
	//vecMyFlatPos.z = pOther->GetAbsOrigin().z;
	//if (!CollisionProp()->IsPointInBounds(vecMyFlatPos))
		//return;
	// are we underneath the marine?
	float z_diff = pOther->CollisionProp()->OBBCenter().z - GetAbsOrigin().z;
	//float other_half_height = pOther->CollisionProp()->OBBSize().z * 0.5f;
	//if (z_diff > other_half_height)
	if (pOther->GetAbsOrigin().z > GetAbsOrigin().z)	// is he higher than me?
	{
		// squash this
		if (asw_debug_grubs.GetBool())
		{
			Msg("Squashed by a marine (my z=%f his z=%f diff=%f dist=%f)\n",
				GetAbsOrigin().z, pOther->GetAbsOrigin().z, z_diff, pOther->GetAbsOrigin().DistTo(GetAbsOrigin()));
		}
		Squash(pOther);
	}
	else
	{
		if (asw_debug_grubs.GetBool())
		{
			Msg("Not squashed by a marine (my z=%f his z=%f diff=%f dist=%f)\n",
				GetAbsOrigin().z, pOther->GetAbsOrigin().z, z_diff, pOther->GetAbsOrigin().DistTo(GetAbsOrigin()));
		}
	}
}

void CASW_Grub::Squash(CBaseEntity* pSquasher)
{
	Event_Killed( CTakeDamageInfo( pSquasher, pSquasher, 200, DMG_CRUSH ) );

	EmitSound( "NPC_AntlionGrub.Squash" );
	
	trace_t	tr;
	Vector vecDir( 0, 0, -1.0f );

	tr.endpos = GetLocalOrigin();
	tr.endpos[2] += 8.0f;

	MakeDamageBloodDecal( 4, 0.8f, &tr, vecDir );

	SetTouch( NULL );

	m_iHealth = 0;
	CorpseGib( CTakeDamageInfo( pSquasher, pSquasher, 200, DMG_CRUSH ) );
	UTIL_Remove(this);
}

//-----------------------------------------------------------------------------
// Purpose: LeapTouch - this is the headcrab's touch function when it is in the air.
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CASW_Grub::LeapTouch( CBaseEntity *pOther )
{
	m_bMidJump = false;	

	if ( IRelationType( pOther ) == D_HT )
	{
		// Don't hit if back on ground
		if ( !( GetFlags() & FL_ONGROUND ) )
		{
	 		if ( pOther->m_takedamage != DAMAGE_NO )
			{
				//BiteSound();
				//TouchDamage( pOther );
			}
			else
			{
				//ImpactSound();
			}
		}
		else
		{
			//ImpactSound();
		}
	}
	else if( !(GetFlags() & FL_ONGROUND) )
	{
		// Still in the air...
		if( gpGlobals->curtime < m_flIgnoreWorldCollisionTime )
		{
			// Headcrabs try to ignore the world, static props, and friends for a 
			// fraction of a second after they jump. This is because they often brush
			// doorframes or props as they leap, and touching those objects turns off
			// this touch function, which can cause them to hit the player and not bite.
			// A timer probably isn't the best way to fix this, but it's one of our 
			// safer options at this point (sjb).
			return;
		}

		if( !pOther->IsSolid() )
		{
			// Touching a trigger or something.
			return;
		}

		// don't collide with other grubs
		if (pOther->Classify() == CLASS_ASW_GRUB)
			return;

		// don't collide with marines
		if (pOther->Classify() == CLASS_ASW_MARINE)
			return;
	}

	// make sure we're solid
	RemoveSolidFlags( FSOLID_NOT_SOLID );

	// make it so we collide with various things again
	SetCollisionGroup(ASW_COLLISION_GROUP_GRUBS);
	m_takedamage		= DAMAGE_YES;	// doesn't take damage until he's landed

	// Shut off the touch function.
	SetTouch( &CASW_Grub::NormalTouch );
	SetThink ( &CASW_Grub::CallNPCThink );
}

bool CASW_Grub::HasHeadroom()
{
	trace_t tr;
	UTIL_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, 1 ), MASK_NPCSOLID, this, GetCollisionGroup(), &tr );

	return (tr.fraction == 1.0);
}

void CASW_Grub::StartTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
		case TASK_GRUB_JUMP_FROM_GOO:
		{
			DoJumpFromGoo();
			break;
		}
		default:
		{
			BaseClass::StartTask( pTask );
		}
	}
}

void CASW_Grub::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_GRUB_JUMP_FROM_GOO:
			GetMotor()->UpdateYaw();
			if ( FacingIdeal() )
			{
				TaskComplete();
			}
			break;
		default:
			BaseClass::RunTask( pTask );
			break;	
	}	
}

int CASW_Grub::SelectSchedule()
{
	if ( m_bJumpFromGoo )
	{
		m_bJumpFromGoo = false;
		return SCHED_GRUB_JUMP_FROM_GOO;
	}

	return BaseClass::SelectSchedule();
}

void CASW_Grub::SetJumpFromGoo(bool bDoJump, float flJumpAngle, float flJumpDistance)
{
	m_bJumpFromGoo = true;
	m_flGooJumpDistance = flJumpDistance;
	m_flGooJumpAngle = flJumpAngle;
}

void CASW_Grub::DoJumpFromGoo()
{
	Vector dir = vec3_origin;
	AngleVectors(GetAbsAngles(), &dir);
	Vector vecJumpPos = GetAbsOrigin() + dir * m_flGooJumpDistance;

	SetActivity( ACT_JUMP );
	StudioFrameAdvanceManual( 0.0 );
	SetParent( NULL );
	RemoveFlag( FL_FLY );
	AddEffects( EF_NOINTERP );

	GetMotor()->SetIdealYaw( m_flGooJumpAngle );

	// do a random jump forward
	JumpAttack( true, vecJumpPos, false );
}

void CASW_Grub::IdleInGoo(bool b)
{
	if (b)
	{
		SetActivity((Activity) ACT_ASW_GRUB_IDLE);
	}
	bDoGooIdle = b;
}

Activity CASW_Grub::TranslateActivity( Activity baseAct, Activity *pIdealWeaponActivity )
{
	Activity translated = BaseClass::TranslateActivity(baseAct, pIdealWeaponActivity);

	if (translated == ACT_IDLE && bDoGooIdle)
	{
		return (Activity) ACT_ASW_GRUB_IDLE;
	}

	return translated;
}

int CASW_Grub::TranslateSchedule( int scheduleType )
{
	switch( scheduleType )
	{
		case SCHED_IDLE_STAND:
		case SCHED_IDLE_WALK:
		case SCHED_ALERT_STAND:
		case SCHED_ALERT_FACE:
		case SCHED_COMBAT_FACE:
			return SCHED_ASW_GRUB_WANDER_ANGRILY;
	}

	return BaseClass::TranslateSchedule( scheduleType );
}

int CASW_Grub::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	if ( failedSchedule != SCHED_ASW_GRUB_WANDER_ANGRILY &&
		 ( failedSchedule == SCHED_TAKE_COVER_FROM_ENEMY || 
		   failedSchedule == SCHED_CHASE_ENEMY_FAILED ) )
	{
		return SCHED_ASW_GRUB_WANDER_ANGRILY;
	}

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

// since grubs are just decorative, make sure they're not wasting CPU
void CASW_Grub::UpdateEfficiency( bool bInPVS )	
{
	SetEfficiency( AIE_VERY_EFFICIENT ); 
	//SetMoveEfficiency( AIME_EFFICIENT ); 
	SetMoveEfficiency( AIME_NORMAL ); 
}

bool CASW_Grub::BlocksLOS( void ) 
{
	return false;
}

AI_BEGIN_CUSTOM_NPC( asw_grub, CASW_Grub )
	DECLARE_ANIMEVENT( AE_HEADCRAB_JUMPATTACK )
	DECLARE_TASK( TASK_GRUB_JUMP_FROM_GOO )
	DECLARE_ACTIVITY( ACT_ASW_GRUB_IDLE )

	DEFINE_SCHEDULE
	(
		SCHED_GRUB_JUMP_FROM_GOO,

		"	Tasks"
		"		TASK_GRUB_JUMP_FROM_GOO			0"
		""
		"	Interrupts"
	)
	DEFINE_SCHEDULE
	(
		SCHED_ASW_GRUB_WANDER_ANGRILY,

		"	Tasks"
		"		TASK_WANDER						480240" // 48 units to 240 units.
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			4"
		""
		"	Interrupts"
	)
AI_END_CUSTOM_NPC()