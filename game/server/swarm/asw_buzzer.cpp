#include "cbase.h"
#include "soundenvelope.h"
#include "asw_buzzer.h"
#include "ai_default.h"
#include "ai_node.h"
#include "ai_navigator.h"
#include "ai_pathfinder.h"
#include "ai_moveprobe.h"
#include "ai_memory.h"
#include "ai_squad.h"
#include "ai_route.h"
#include "explode.h"
#include "basegrenade_shared.h"
#include "ndebugoverlay.h"
#include "decals.h"
#include "gib.h"
#include "game.h"			
#include "ai_interactions.h"
#include "IEffects.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "movevars_shared.h"
#include "npcevent.h"
#include "props.h"
#include "te_effect_dispatch.h"
#include "ai_squadslot.h"
#include "world.h"
#include "func_break.h"
#include "physics_impact_damage.h"
#include "weapon_physcannon.h"
#include "physics_prop_ragdoll.h"
#include "soundent.h"
#include "ammodef.h"
#include "asw_fx_shared.h"
#include "asw_spawner.h"
#include "asw_ai_senses.h"
#include "asw_util_shared.h"
#include "asw_gamerules.h"
#include "EntityFlame.h"
#include "asw_burning.h"
#include "asw_marine.h"
#include "asw_gamestats.h"
#include "asw_director.h"
#include "asw_weapon.h"
#include "asw_game_resource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// When the engine is running and the buzzer is operating under power
// we don't let gravity affect him.
#define ASW_BUZZER_GRAVITY 0.000

//#define ASW_BUZZER_MODEL "models/manhack.mdl"
#define ASW_BUZZER_MODEL "models/aliens/buzzer/buzzer.mdl"

#define ASW_BUZZER_GIB_COUNT			5 
#define ASW_BUZZER_INGORE_WATER_DIST	384

// Sound stuff
#define ASW_BUZZER_PITCH_DIST1		512
#define ASW_BUZZER_MIN_PITCH1		(100)
#define ASW_BUZZER_MAX_PITCH1		(160)
#define ASW_BUZZER_WATER_PITCH1	(85)
#define ASW_BUZZER_VOLUME1			0.55

#define ASW_BUZZER_PITCH_DIST2		400
#define ASW_BUZZER_MIN_PITCH2		(85)
#define ASW_BUZZER_MAX_PITCH2		(190)
#define ASW_BUZZER_WATER_PITCH2	(90)

#define ASW_BUZZER_NOISEMOD_HIDE 5000

#define ASW_BUZZER_BODYGROUP_BLADE	1
#define ASW_BUZZER_BODYGROUP_BLUR	2
#define ASW_BUZZER_BODYGROUP_OFF	0
#define ASW_BUZZER_BODYGROUP_ON	1

#define	ASW_BUZZER_CHARGE_MIN_DIST	200

ConVar	sk_asw_buzzer_health( "sk_asw_buzzer_health","30", FCVAR_CHEAT, "Health of the buzzer");
ConVar	sk_asw_buzzer_melee_dmg( "sk_asw_buzzer_melee_dmg","15", FCVAR_CHEAT, "Damage caused by buzzer");
ConVar	sk_asw_buzzer_melee_interval( "sk_asw_buzzer_melee_interval", "1.5", FCVAR_CHEAT, "Min time between causing damage to marines");
ConVar	sk_asw_buzzer_v2( "sk_asw_buzzer_v2","1", FCVAR_CHEAT, "");
ConVar asw_buzzer_poison_duration("asw_buzzer_poison_duration", "0.6f", FCVAR_CHEAT, "Base buzzer poison blur duration. This scales up to double the value based on mission difficulty.");
extern ConVar showhitlocation;
extern ConVar asw_debug_alien_damage;
extern ConVar asw_alien_speed_scale_easy;
extern ConVar asw_alien_speed_scale_normal;
extern ConVar asw_alien_speed_scale_hard;
extern ConVar asw_alien_speed_scale_insane;
extern ConVar asw_stun_grenade_time;

extern void		SpawnBlood(Vector vecSpot, const Vector &vAttackDir, int bloodColor, float flDamage);
extern float	GetFloorZ(const Vector &origin);

envelopePoint_t envBuzzerMoanIgnited[] =
{
	{	1.0f, 1.0f,
		0.5f, 1.0f,
	},
	{	1.0f, 1.0f,
		30.0f, 30.0f,
	},
	{	0.0f, 0.0f,
		0.5f, 1.0f,
	},
};

envelopePoint_t envDefaultBuzzerMoanVolumeFast[] =
{
	{	1.0f, 1.0f,
		0.1f, 0.1f,
	},
	{	0.0f, 0.0f,
		0.2f, 0.3f,
	},
};


//-----------------------------------------------------------------------------
// Manhack schedules.
//-----------------------------------------------------------------------------
enum BuzzerSchedules
{
	SCHED_ASW_BUZZER_ATTACK_HOVER = LAST_SHARED_SCHEDULE,	
	SCHED_ASW_BUZZER_REGROUP,
	SCHED_ASW_BUZZER_SWARM_IDLE,
	SCHED_ASW_BUZZER_SWARM,
	SCHED_ASW_BUZZER_SWARM_FAILURE,
	SCHED_ASW_BUZZER_ORDER_MOVE,
};


//-----------------------------------------------------------------------------
// Manhack tasks.
//-----------------------------------------------------------------------------
enum BuzzerTasks
{
	TASK_ASW_BUZZER_HOVER = LAST_SHARED_TASK,
	TASK_ASW_BUZZER_FIND_SQUAD_CENTER,
	TASK_ASW_BUZZER_FIND_SQUAD_MEMBER,
	TASK_ASW_BUZZER_MOVEAT_SAVEPOSITION,
	TASK_ASW_BUZZER_BUILD_PATH_TO_ORDER,
};

BEGIN_DATADESC( CASW_Buzzer )

	DEFINE_FIELD( m_vForceVelocity,			FIELD_VECTOR),

	DEFINE_FIELD( m_vTargetBanking,			FIELD_VECTOR),
	DEFINE_FIELD( m_vForceMoveTarget,			FIELD_POSITION_VECTOR),
	DEFINE_FIELD( m_fForceMoveTime,			FIELD_TIME),
	DEFINE_FIELD( m_vSwarmMoveTarget,			FIELD_POSITION_VECTOR),
	DEFINE_FIELD( m_fSwarmMoveTime,			FIELD_TIME),
	DEFINE_FIELD( m_fEnginePowerScale,		FIELD_FLOAT),

	DEFINE_FIELD( m_flNextEngineSoundTime,	FIELD_TIME),
	DEFINE_FIELD( m_flEngineStallTime,		FIELD_TIME),
	DEFINE_FIELD( m_flNextBurstTime,			FIELD_TIME ),
	DEFINE_FIELD( m_flWaterSuspendTime,		FIELD_TIME),

	// Death

	DEFINE_FIELD( m_bDirtyPitch,			FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bGib,					FIELD_BOOLEAN),
	DEFINE_FIELD( m_bHeld,					FIELD_BOOLEAN),	
	DEFINE_FIELD( m_vecLoiterPosition,		FIELD_POSITION_VECTOR),
	DEFINE_FIELD( m_fSuicideTime,	FIELD_TIME),

	DEFINE_FIELD( m_flWingFlapSpeed,				FIELD_FLOAT),	

	DEFINE_FIELD( m_iPanel1, FIELD_INTEGER ),
	DEFINE_FIELD( m_iPanel2, FIELD_INTEGER ),
	DEFINE_FIELD( m_iPanel3, FIELD_INTEGER ),
	DEFINE_FIELD( m_iPanel4, FIELD_INTEGER ),

	DEFINE_FIELD( m_nLastWaterLevel,			FIELD_INTEGER ),
	DEFINE_FIELD( m_bDoSwarmBehavior,			FIELD_BOOLEAN ),

	DEFINE_FIELD( m_nEnginePitch1,				FIELD_INTEGER ),
	DEFINE_FIELD( m_flEnginePitch1Time,			FIELD_TIME ),	

	// Physics Influence
	DEFINE_FIELD( m_hPhysicsAttacker, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flLastPhysicsInfluenceTime, FIELD_TIME ),

	DEFINE_FIELD( m_flBurstDuration,	FIELD_FLOAT ),
	DEFINE_FIELD( m_vecBurstDirection,	FIELD_VECTOR ),
	DEFINE_FIELD( m_bShowingHostile,	FIELD_BOOLEAN ),

	DEFINE_FIELD( m_hSpawner, FIELD_EHANDLE ),

	DEFINE_FIELD(m_fNextPainSound, FIELD_FLOAT),
	DEFINE_SOUNDPATCH( m_pMoanSound ),
	DEFINE_FIELD( m_flMoanPitch, FIELD_FLOAT ),
	DEFINE_FIELD( m_flNextMoanSound, FIELD_TIME ),
	DEFINE_FIELD( m_bOnFire, FIELD_BOOLEAN ),
	DEFINE_FIELD(m_fHurtSlowMoveTime, FIELD_TIME),
	DEFINE_FIELD(m_flElectroStunSlowMoveTime, FIELD_TIME),
	DEFINE_FIELD(m_bElectroStunned, FIELD_BOOLEAN),
	DEFINE_FIELD( m_bHoldoutAlien, FIELD_BOOLEAN ),

	// Function Pointers
	DEFINE_INPUTFUNC( FIELD_VOID,	"DisableSwarm", InputDisableSwarm ),

END_DATADESC()


LINK_ENTITY_TO_CLASS( asw_buzzer, CASW_Buzzer );

IMPLEMENT_SERVERCLASS_ST(CASW_Buzzer, DT_ASW_Buzzer)
	SendPropIntWithMinusOneFlag	(SENDINFO(m_nEnginePitch1), 8 ),
	SendPropFloat(SENDINFO(m_flEnginePitch1Time), 0, SPROP_NOSCALE),	
	SendPropBool(SENDINFO(m_bOnFire)),
	SendPropBool(SENDINFO(m_bElectroStunned)),
END_SEND_TABLE()

CASW_Buzzer::CASW_Buzzer()
{
#ifdef _DEBUG
	m_vForceMoveTarget.Init();
	m_vSwarmMoveTarget.Init();
	m_vTargetBanking.Init();
	m_vForceVelocity.Init();
#endif
	m_bDirtyPitch = true;
	m_nLastWaterLevel = 0;
	m_nEnginePitch1 = 0;
	m_flEnginePitch1Time = 0;
	m_flEnginePitch1Time = 0;
	m_bDoSwarmBehavior = true;
	m_fNextPainSound = 0;
	m_bHoldoutAlien = false;
	m_flLastDamageTime = 0;
}

CASW_Buzzer::~CASW_Buzzer()
{
}

//-----------------------------------------------------------------------------
// Purpose: Turns the buzzer into a physics corpse when dying.
//-----------------------------------------------------------------------------
void CASW_Buzzer::Event_Dying(void)
{
	SetHullSizeNormal();
	BaseClass::Event_Dying();
}

void CASW_Buzzer::OnRestore()
{
	BaseClass::OnRestore();
	m_LagCompensation.Init(this);
}

void CASW_Buzzer::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();	

	if( m_flWaterSuspendTime > gpGlobals->curtime )
	{
		// Stuck in water!

		// Reduce engine power so that the buzzer lifts out of the water slowly.
		m_fEnginePowerScale = 0.75;
	}

	// ----------------------------------------
	//	Am I in water?
	// ----------------------------------------
	if ( GetWaterLevel() > 0 )
	{
		if( m_nLastWaterLevel == 0 )
		{
			Splash( WorldSpaceCenter() );
		}

		if( IsAlive() )
		{
			// If I've been out of water for 2 seconds or more, I'm eligible to be stuck in water again.
			if( gpGlobals->curtime - m_flWaterSuspendTime > 2.0 )
			{
				m_flWaterSuspendTime = gpGlobals->curtime + 1.0;
			}
		}
	}
	else
	{
		if( m_nLastWaterLevel != 0 )
		{
			Splash( WorldSpaceCenter() );
		}
	}

	m_nLastWaterLevel = GetWaterLevel();
}

void CASW_Buzzer::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	g_vecAttackDir = vecDir;

	if ( info.GetDamageType() & DMG_BULLET)
	{
		//g_pEffects->Ricochet(ptr->endpos,ptr->plane.normal);
	}

	if ( info.GetDamageType() & DMG_CLUB )
	{
		// Clubbed!
//		UTIL_Smoke(GetAbsOrigin(), random->RandomInt(10, 15), 10);
		//g_pEffects->Sparks( ptr->endpos, 1, 1, &ptr->plane.normal );		
	}
	UTIL_ASW_DroneBleed( ptr->endpos + m_LagCompensation.GetLagCompensationOffset(), vecDir, 4 );

	//BaseClass::TraceAttack( info, vecDir, ptr );

	m_fNoDamageDecal = false;
	if ( m_takedamage == DAMAGE_NO )
		return;

	CTakeDamageInfo subInfo = info;

	SetLastHitGroup( ptr->hitgroup );
	m_nForceBone = ptr->physicsbone;		// save this bone for physics forces

	Assert( m_nForceBone > -255 && m_nForceBone < 256 );

	bool bDebug = showhitlocation.GetBool();

	switch ( ptr->hitgroup )
	{
	case HITGROUP_GENERIC:
		if( bDebug ) DevMsg("Hit Location: Generic\n");
		break;

	// hit gear, react but don't bleed
	case HITGROUP_GEAR:
		subInfo.SetDamage( 0.01 );
		ptr->hitgroup = HITGROUP_GENERIC;
		if( bDebug ) DevMsg("Hit Location: Gear\n");
		break;

	case HITGROUP_HEAD:
		subInfo.ScaleDamage( GetHitgroupDamageMultiplier(ptr->hitgroup, info) );
		if( bDebug ) DevMsg("Hit Location: Head\n");
		break;

	case HITGROUP_CHEST:
		subInfo.ScaleDamage( GetHitgroupDamageMultiplier(ptr->hitgroup, info) );
		if( bDebug ) DevMsg("Hit Location: Chest\n");
		break;

	case HITGROUP_STOMACH:
		subInfo.ScaleDamage( GetHitgroupDamageMultiplier(ptr->hitgroup, info) );
		if( bDebug ) DevMsg("Hit Location: Stomach\n");
		break;

	case HITGROUP_LEFTARM:
	case HITGROUP_RIGHTARM:
		subInfo.ScaleDamage( GetHitgroupDamageMultiplier(ptr->hitgroup, info) );
		if( bDebug ) DevMsg("Hit Location: Left/Right Arm\n");
		break
			;
	case HITGROUP_LEFTLEG:
	case HITGROUP_RIGHTLEG:
		subInfo.ScaleDamage( GetHitgroupDamageMultiplier(ptr->hitgroup, info) );
		if( bDebug ) DevMsg("Hit Location: Left/Right Leg\n");
		break;

	default:
		if( bDebug ) DevMsg("Hit Location: UNKNOWN\n");
		break;
	}

	if ( subInfo.GetDamage() >= 1.0 && !(subInfo.GetDamageType() & DMG_SHOCK ) )
	{
		if( !IsPlayer() || ( IsPlayer() && gpGlobals->maxClients > 1 ) )
		{
			// NPC's always bleed. Players only bleed in multiplayer.
			SpawnBlood( ptr->endpos, vecDir, BloodColor(), subInfo.GetDamage() );// a little surface blood.
		}

		ASWTraceBleed( subInfo.GetDamage(), vecDir, ptr, subInfo.GetDamageType() );

		if ( ptr->hitgroup == HITGROUP_HEAD && m_iHealth - subInfo.GetDamage() > 0 )
		{
			m_fNoDamageDecal = true;
		}
	}

	// Airboat gun will impart major force if it's about to kill him....
	if ( info.GetDamageType() & DMG_AIRBOAT )
	{
		if ( subInfo.GetDamage() >= GetHealth() )
		{
			float flMagnitude = subInfo.GetDamageForce().Length();
			if ( (flMagnitude != 0.0f) && (flMagnitude < 400.0f * 65.0f) )
			{
				subInfo.ScaleDamageForce( 400.0f * 65.0f / flMagnitude );
			}
		}
	}

	if( info.GetInflictor() )
	{
		subInfo.SetInflictor( info.GetInflictor() );
	}
	else
	{
		subInfo.SetInflictor( info.GetAttacker() );
	}

	AddMultiDamage( subInfo, this );
}

void CASW_Buzzer::ASWTraceBleed( float flDamage, const Vector &vecDir, trace_t *ptr, int bitsDamageType )
{
	if ((BloodColor() == DONT_BLEED) || (BloodColor() == BLOOD_COLOR_MECH))
	{
		return;
	}

	if (flDamage == 0)
		return;

	if (! (bitsDamageType & (DMG_CRUSH | DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_CLUB | DMG_AIRBOAT)))
		return;

	// make blood decal on the wall!
	trace_t Bloodtr;
	Vector vecTraceDir;
	float flNoise;
	int cCount;
	int i;

#ifdef GAME_DLL
	if ( !IsAlive() )
	{
		// dealing with a dead npc.
		if ( GetMaxHealth() <= 0 )
		{
			// no blood decal for a npc that has already decalled its limit.
			return;
		}
		else
		{
			m_iMaxHealth -= 1;
		}
	}
#endif

	if (flDamage < 10)
	{
		flNoise = 0.1;
		cCount = 1;
	}
	else if (flDamage < 25)
	{
		flNoise = 0.2;
		cCount = 2;
	}
	else
	{
		flNoise = 0.3;
		cCount = 4;
	}

	float flTraceDist = (bitsDamageType & DMG_AIRBOAT) ? 384 : 172;
	for ( i = 0 ; i < cCount ; i++ )
	{
		vecTraceDir = vecDir * -1;// trace in the opposite direction the shot came from (the direction the shot is going)

		vecTraceDir.x += random->RandomFloat( -flNoise, flNoise );
		vecTraceDir.y += random->RandomFloat( -flNoise, flNoise );
		vecTraceDir.z += random->RandomFloat( -flNoise, flNoise );

		// Don't bleed on grates.
		Vector vecEndPos = ptr->endpos + m_LagCompensation.GetLagCompensationOffset();
		AI_TraceLine( vecEndPos, vecEndPos + vecTraceDir * -flTraceDist, MASK_SOLID_BRUSHONLY & ~CONTENTS_GRATE, this, COLLISION_GROUP_NONE, &Bloodtr);

		if ( Bloodtr.fraction != 1.0 )
		{
			UTIL_BloodDecalTrace( &Bloodtr, BloodColor() );
		}
	}
}

void CASW_Buzzer::DeathSound(  const CTakeDamageInfo &info )
{
	CPASAttenuationFilter filter2( this, "ASW_Buzzer.Death" );
	EmitSound( filter2, entindex(), "ASW_Buzzer.Death" );
}

void CASW_Buzzer::PainSound(  const CTakeDamageInfo &info )
{
	// no pain sound on burning
	if (info.GetDamageType() & DMG_BURN)
		return;

	if (gpGlobals->curtime > m_fNextPainSound)
	{
		CPASAttenuationFilter filter2( this, "ASW_Buzzer.Pain" );
		EmitSound( filter2, entindex(), "ASW_Buzzer.Pain" );
		m_fNextPainSound = gpGlobals->curtime + 0.5f;
	}
}

bool CASW_Buzzer::ShouldGib( const CTakeDamageInfo &info )
{
	return true;	 // always gib buzzers
	//return ( m_bGib );
}

void CASW_Buzzer::Event_Killed( const CTakeDamageInfo &info )
{
	if (ASWGameRules())
	{
		ASWGameRules()->AlienKilled(this, info);
	}	

	CASW_GameStats.Event_AlienKilled( this, info );

	if ( ASWDirector() )
		ASWDirector()->Event_AlienKilled( this, info );

	// notify our spawner, so it can spit out more buzzers if need be
	if (m_hSpawner.Get())
		m_hSpawner->AlienKilled(this);

	// turn off the blur!
	SetBodygroup( ASW_BUZZER_BODYGROUP_BLUR, ASW_BUZZER_BODYGROUP_OFF );

	UTIL_ASW_BuzzerDeath( GetAbsOrigin() );

	if ( m_nEnginePitch1 <= 0 )
	{
		// Probably this buzzer was killed immediately after spawning. Turn the sound
		// on right now so that we can pitch it up for the crash!
		SoundInit();
	}

	// Always gib when clubbed or blasted or crushed, or just randomly
	if ( ( info.GetDamageType() & (DMG_CLUB|DMG_CRUSH|DMG_BLAST) ) || ( random->RandomInt( 0, 1 ) ) )
	{
		m_bGib = true;
	}
	else
	{
		m_bGib = false;
	}

	BaseClass::Event_Killed( info );

	UTIL_Remove( this );
}

void CASW_Buzzer::HitPhysicsObject( CBaseEntity *pOther )
{
	IPhysicsObject *pOtherPhysics = pOther->VPhysicsGetObject();
	Vector pos, posOther;
	// Put the force on the line between the buzzer origin and hit object origin
	VPhysicsGetObject()->GetPosition( &pos, NULL );
	pOtherPhysics->GetPosition( &posOther, NULL );
	Vector dir = posOther - pos;
	VectorNormalize(dir);
	// size/2 is approx radius
	pos += dir * WorldAlignSize().x * 0.5;
	Vector cross;

	// UNDONE: Use actual buzzer up vector so the fake blade is
	// in the right plane?
	// Get a vector in the x/y plane in the direction of blade spin (clockwise)
	CrossProduct( dir, Vector(0,0,1), cross );
	VectorNormalize( cross );
	// force is a 30kg object going 100 in/s
	pOtherPhysics->ApplyForceOffset( cross * 30 * 100, pos );
}

//-----------------------------------------------------------------------------
// Take damage from a vehicle; it's like a really big crowbar 
//-----------------------------------------------------------------------------
void CASW_Buzzer::TakeDamageFromVehicle( int index, gamevcollisionevent_t *pEvent )
{
	// Use the vehicle velocity to determine the damage
	int otherIndex = !index;
	CBaseEntity *pOther = pEvent->pEntities[otherIndex];

	float flSpeed = pEvent->preVelocity[ otherIndex ].Length();
	flSpeed = clamp( flSpeed, 300.0f, 600.0f );
	float flDamage = SimpleSplineRemapVal( flSpeed, 300.0f, 600.0f, 0.0f, 1.0f );
	if ( flDamage == 0.0f )
		return;

	flDamage *= 20.0f;

	Vector damagePos;
	pEvent->pInternalData->GetContactPoint( damagePos );

	Vector damageForce = 2.0f * pEvent->postVelocity[index] * pEvent->pObjects[index]->GetMass();
	if ( damageForce == vec3_origin )
	{
		// This can happen if this entity is a func_breakable, and can't move.
		// Use the velocity of the entity that hit us instead.
		damageForce = 2.0f * pEvent->postVelocity[!index] * pEvent->pObjects[!index]->GetMass();
	}
	Assert( damageForce != vec3_origin );
	CTakeDamageInfo dmgInfo( pOther, pOther, damageForce, damagePos, flDamage, DMG_CRUSH );
	TakeDamage( dmgInfo );
}


//-----------------------------------------------------------------------------
// Take damage from combine ball
//-----------------------------------------------------------------------------
void CASW_Buzzer::TakeDamageFromPhysicsImpact( int index, gamevcollisionevent_t *pEvent )
{
	CBaseEntity *pHitEntity = pEvent->pEntities[!index];

	// NOTE: Bypass the normal impact energy scale here.
	float flDamageScale = 1.0f;
	int damageType = 0;
	float damage = CalculateDefaultPhysicsDamage( index, pEvent, flDamageScale, true, damageType );
	if ( damage == 0 )
		return;

	Vector damagePos;
	pEvent->pInternalData->GetContactPoint( damagePos );
	Vector damageForce = pEvent->postVelocity[index] * pEvent->pObjects[index]->GetMass();
	if ( damageForce == vec3_origin )
	{
		// This can happen if this entity is motion disabled, and can't move.
		// Use the velocity of the entity that hit us instead.
		damageForce = pEvent->postVelocity[!index] * pEvent->pObjects[!index]->GetMass();
	}

	// FIXME: this doesn't pass in who is responsible if some other entity "caused" this collision
	PhysCallbackDamage( this, CTakeDamageInfo( pHitEntity, pHitEntity, damageForce, damagePos, damage, damageType ), *pEvent, index );
}


#define ASW_BUZZER_SMASH_TIME	0.35		// How long after being thrown from a physcannon that a buzzer is eligible to die from impact
void CASW_Buzzer::VPhysicsCollision( int index, gamevcollisionevent_t *pEvent )
{
	BaseClass::VPhysicsCollision( index, pEvent );

	int otherIndex = !index;
	CBaseEntity *pHitEntity = pEvent->pEntities[otherIndex];

	if ( pHitEntity )
	{
		// It can take physics damage if it rams into a vehicle
		if ( pHitEntity->GetServerVehicle() )
		{
			TakeDamageFromVehicle( index, pEvent );
		}
		else if ( m_iHealth <= 0 )
		{
			TakeDamageFromPhysicsImpact( index, pEvent );
		}

		StopBurst( true );
	}
}


void CASW_Buzzer::VPhysicsShadowCollision( int index, gamevcollisionevent_t *pEvent )
{
	int otherIndex = !index;
	CBaseEntity *pOther = pEvent->pEntities[otherIndex];

	if ( pOther->GetMoveType() == MOVETYPE_VPHYSICS )
	{
		HitPhysicsObject( pOther );
	}
	BaseClass::VPhysicsShadowCollision( index, pEvent );
}

int	CASW_Buzzer::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	// Hafta make a copy of info cause we might need to scale damage.(sjb)
	CTakeDamageInfo tdInfo = info;

	// undo lag compensation if we're getting hurt			- TODO: this is incorrect if multiple rounds were meant to hit us within this player command - would happen with the shotguns if they were hitscan?
	m_LagCompensation.UndoLaggedPosition();

	// scale damage based on weapons that specifically shoot flyers
	if ( info.GetAttacker() && info.GetAttacker()->Classify() == CLASS_ASW_MARINE )
	{
		CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>( info.GetAttacker() );
		if ( pMarine )
		{
			CASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();
			if ( pWeapon )
			{
				float damage = tdInfo.GetDamage();
				//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pWeapon, damage, mod_damage_flyers );
				tdInfo.SetDamage( damage );
			}
		}
	}
	
	
	m_flWingFlapSpeed = 20.0;

	Vector vecDamageDir = tdInfo.GetDamageForce();
	VectorNormalize( vecDamageDir );


	// no being knocked back by shots
	m_vForceVelocity = vec3_origin;	//vecDamageDir * info.GetDamage() * 20.0f;

	tdInfo.SetDamageForce( vecDamageDir ); //tdInfo.GetDamageForce() * 20 );

	// come to a complete stop if hurt
	//SetCurrentVelocity( vec3_origin );
	if (m_fHurtSlowMoveTime < gpGlobals->curtime + 0.4f)
		m_fHurtSlowMoveTime = gpGlobals->curtime + 0.4f;

	//VPhysicsTakeDamage( info );

	if (asw_debug_alien_damage.GetBool())
	{
		Msg("%d %s hurt by %f dmg\n", entindex(), GetClassname(), info.GetDamage());
	}

	int nRetVal = BaseClass::OnTakeDamage_Alive( tdInfo );
	if ( nRetVal )
	{
		// if we take fire or blast damage, catch on fire
		if (nRetVal > 0 &&
			( (info.GetDamageType() & DMG_BURN) || (info.GetDamageType() & DMG_BLAST) )
			)
			ASW_Ignite(30.0f, 0, info.GetAttacker(), info.GetWeapon() );
	}

	// make the alien move slower for 0.5 seconds
	if (info.GetDamageType() & DMG_SHOCK)
	{
		ElectroStun( asw_stun_grenade_time.GetFloat() );
	}

	CASW_Marine* pMarine = dynamic_cast<CASW_Marine*>(info.GetAttacker());
	if (pMarine)
		pMarine->HurtAlien(this, info);

	return nRetVal;
}

bool CASW_Buzzer::CorpseGib( const CTakeDamageInfo &info )
{
	Vector			vecGibVelocity;
	AngularImpulse	vecGibAVelocity;

	m_LagCompensation.UndoLaggedPosition();

	if( info.GetDamageType() & DMG_CLUB )
	{
		// If clubbed to death, break apart before the attacker's eyes!
		vecGibVelocity = g_vecAttackDir * -150;

		vecGibAVelocity.x = random->RandomFloat( -2000, 2000 );
		vecGibAVelocity.y = random->RandomFloat( -2000, 2000 );
		vecGibAVelocity.z = random->RandomFloat( -2000, 2000 );
	}
	else
	{
		// Shower the pieces with my velocity.
		vecGibVelocity = GetCurrentVelocity();

		vecGibAVelocity.x = random->RandomFloat( -500, 500 );
		vecGibAVelocity.y = random->RandomFloat( -500, 500 );
		vecGibAVelocity.z = random->RandomFloat( -500, 500 );
	}

	PropBreakableCreateAll( GetModelIndex(), NULL, GetAbsOrigin(), GetAbsAngles(), vecGibVelocity, vecGibAVelocity, 1.0, 60, COLLISION_GROUP_DEBRIS );

	RemoveDeferred();

	return true;
}


// Purpose: Explode the buzzer if it's damaged while crashing
int	CASW_Buzzer::OnTakeDamage_Dying( const CTakeDamageInfo &info )
{
	// Ignore damage for the first 1 second of crashing behavior.
	// If we don't do this, buzzers always just explode under 
	// sustained fire.
	VPhysicsTakeDamage( info );
	
	return 0;
}

//-----------------------------------------------------------------------------
// Turn on the engine sound if we're gagged!
//-----------------------------------------------------------------------------
void CASW_Buzzer::OnStateChange( NPC_STATE OldState, NPC_STATE NewState )
{
	if( m_vNoiseMod.z == ASW_BUZZER_NOISEMOD_HIDE && !(m_spawnflags & SF_NPC_WAIT_FOR_SCRIPT) )
	{
		// This buzzer should get a normal noisemod now.
		float flNoiseMod = random->RandomFloat( 1.7, 2.3 );
		
		// Just bob up and down.
		SetNoiseMod( 0, 0, flNoiseMod );
	}

	if( NewState != NPC_STATE_IDLE && (m_spawnflags & SF_NPC_GAG) && (m_nEnginePitch1 <= 0) )
	{
		m_spawnflags &= ~SF_NPC_GAG;
		SoundInit();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not the given activity would translate to flying.
//-----------------------------------------------------------------------------
bool CASW_Buzzer::IsFlyingActivity( Activity baseAct )
{
	return ((baseAct == ACT_FLY || baseAct == ACT_IDLE || baseAct == ACT_RUN || baseAct == ACT_WALK));
}

Activity CASW_Buzzer::NPC_TranslateActivity( Activity baseAct )
{
	if (IsFlyingActivity( baseAct ))
	{
		return (Activity)ACT_FLY;
	}

	return BaseClass::NPC_TranslateActivity( baseAct );
}

int CASW_Buzzer::TranslateSchedule( int scheduleType ) 
{
	switch ( scheduleType )
	{
	case SCHED_MELEE_ATTACK1:
		{
			return SCHED_ASW_BUZZER_ATTACK_HOVER;
			break;
		}
	case SCHED_BACK_AWAY_FROM_ENEMY:
		{
			return SCHED_ASW_BUZZER_REGROUP;
			break;
		}
	case SCHED_CHASE_ENEMY:
		{
			// If we're waiting for our next attack opportunity, just swarm
			if ( m_flNextBurstTime > gpGlobals->curtime )
			{
				return SCHED_ASW_BUZZER_SWARM;
			}

			if ( !m_bDoSwarmBehavior || OccupyStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
			{
				return SCHED_CHASE_ENEMY;
			}
			else
			{
				return SCHED_ASW_BUZZER_SWARM;
			}
		}
	case SCHED_COMBAT_FACE:
		{
			// Don't care about facing enemy, handled automatically
			return TranslateSchedule( SCHED_CHASE_ENEMY );
			break;
		}
	case SCHED_WAKE_ANGRY:
		{
			return TranslateSchedule( SCHED_CHASE_ENEMY );			
			break;
		}

	case SCHED_IDLE_STAND:
	case SCHED_ALERT_STAND:
	case SCHED_ALERT_FACE:
		{
			if ( m_pSquad && m_bDoSwarmBehavior )
			{
				return SCHED_ASW_BUZZER_SWARM_IDLE;
			}
			else
			{
				return BaseClass::TranslateSchedule(scheduleType);
			}
		}

	case SCHED_CHASE_ENEMY_FAILED:
		{
			// Relentless bastard! Doesn't fail (fail not valid anyway)
			return TranslateSchedule( SCHED_CHASE_ENEMY );
			break;
		}

	}
	return BaseClass::TranslateSchedule(scheduleType);
}

#define MAX_LOITER_DIST_SQR 144 // (12 inches sqr)
void CASW_Buzzer::Loiter()
{
	//NDebugOverlay::Line( GetAbsOrigin(), m_vecLoiterPosition, 255, 255, 255, false, 0.1 );

	// Friendly buzzer is loitering.
	if( !m_bHeld )
	{
		float distSqr = m_vecLoiterPosition.DistToSqr(GetAbsOrigin());

		if( distSqr > MAX_LOITER_DIST_SQR )
		{
			Vector vecDir = m_vecLoiterPosition - GetAbsOrigin();
			VectorNormalize( vecDir );

			// Move back to our loiter position.
			if( gpGlobals->curtime > m_fTimeNextLoiterPulse )
			{
				// Apply a pulse of force if allowed right now.
				if( distSqr > MAX_LOITER_DIST_SQR * 4.0f )
				{
					//Msg("Big Pulse\n");
					m_vForceVelocity = vecDir * 12.0f;
				}
				else
				{
					//Msg("Small Pulse\n");
					m_vForceVelocity = vecDir * 6.0f;
				}

				m_fTimeNextLoiterPulse = gpGlobals->curtime + 1.0f;
			}
			else
			{
				m_vForceVelocity = vec3_origin;
			}
		}
		else
		{
			// Counteract velocity to slow down.
			Vector velocity = GetCurrentVelocity();
			m_vForceVelocity = velocity * -0.5;
		}
	}
}

void CASW_Buzzer::MaintainGroundHeight( void )
{
	float zSpeed = GetCurrentVelocity().z;

	if (  m_NPCState != NPC_STATE_IDLE && zSpeed > 32.0f )
	{
		//Msg("[X] ");
		return;
	}

	const float minGroundHeight = 52.0f;

	trace_t	tr;
	AI_TraceHull(	GetAbsOrigin(), 
		GetAbsOrigin() - Vector( 0, 0, minGroundHeight ), 
		GetHullMins(), 
		GetHullMaxs(), 
		(MASK_NPCSOLID_BRUSHONLY), 
		this, 
		COLLISION_GROUP_NONE, 
		&tr );

	if ( tr.fraction != 1.0f )
	{
		float speedAdj = MAX( 16, (-zSpeed*0.5f) );
		//Msg("[%f] ", speedAdj);
		m_vForceVelocity += Vector(0,0,1) * ( speedAdj * ( 1.0f - tr.fraction ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles movement towards the last move target.
// Input  : flInterval - 
//-----------------------------------------------------------------------------
bool CASW_Buzzer::OverrideMove( float flInterval )
{			
	FlapWings( flInterval );
	if( IsLoitering() )
	{
		//Msg("Loitering ");
		Loiter();
	}
	else
	{
		//Msg("Maint. ");
		MaintainGroundHeight();
	}

	// So cops, etc. will try to avoid them
	if ( !HasSpawnFlags( SF_ASW_BUZZER_NO_DANGER_SOUNDS ) && !m_bHeld )
	{
		CSoundEnt::InsertSound( SOUND_DANGER, GetAbsOrigin(), 75, flInterval, this );
	}

	// -----------------------------------------------------------------
	//  If I'm being forced to move somewhere
	// ------------------------------------------------------------------
	if (m_fForceMoveTime > gpGlobals->curtime)
	{
		//Msg("MoveToTarget ");
		MoveToTarget(flInterval, m_vForceMoveTarget);
	}
	// -----------------------------------------------------------------
	// If I have a route, keep it updated and move toward target
	// ------------------------------------------------------------------
	else if (GetNavigator()->IsGoalActive())
	{
		bool bReducible = GetNavigator()->GetPath()->GetCurWaypoint()->IsReducible();
		const float strictTolerance = 64.0;
		//NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + Vector(0, 0, 10 ), 255, 0, 0, true, 0.1);
		//Msg("ProgressFlyPath ");
  		if ( ProgressFlyPath( flInterval, GetEnemy(), MoveCollisionMask(), bReducible, strictTolerance ) == AINPP_COMPLETE )
			return true;
	}
	// -----------------------------------------------------------------
	// If I'm supposed to swarm somewhere, try to go there
	// ------------------------------------------------------------------
	else if (m_fSwarmMoveTime > gpGlobals->curtime)
	{
		//Msg("MoveToTarget Sw ");
		MoveToTarget(flInterval, m_vSwarmMoveTarget);
	}
	// -----------------------------------------------------------------
	// If I don't have anything better to do, just decelerate
	// -------------------------------------------------------------- ----
	else
	{
		//Msg("Decelerate ");
		float	myDecay	 = 9.5;
		Decelerate( flInterval, myDecay );

		m_vTargetBanking = vec3_origin;

		// -------------------------------------
		// If I have an enemy turn to face him
		// -------------------------------------
		if (GetEnemy())
		{
			TurnHeadToTarget(flInterval, GetEnemy()->GetAbsOrigin() );
		}
	}

	// asw temp
	m_vForceVelocity = vec3_origin;

	if ( m_iHealth <= 0 )
	{
		//Msg("MED\n");
		// Crashing!!
		MoveExecute_Dead(flInterval);
	}
	else
	{
		//Msg("MEA\n");
		// Alive!
		MoveExecute_Alive(flInterval);
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CASW_Buzzer::TurnHeadRandomly(float flInterval )
{
	float desYaw = random->RandomFloat(0,360);

	float	iRate	 = 0.8;
	// Make frame rate independent
	float timeToUse = flInterval;
	while (timeToUse > 0)
	{
		m_fHeadYaw	   = (iRate * m_fHeadYaw) + (1-iRate)*desYaw;
		timeToUse =- 0.1;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CASW_Buzzer::MoveToTarget(float flInterval, const Vector &vMoveTarget)
{
	if (flInterval <= 0)
	{
		return;
	}

	// -----------------------------------------
	// Don't steer if engine's have stalled
	// -----------------------------------------
	if ( gpGlobals->curtime < m_flEngineStallTime || m_iHealth <= 0 )
		return;

	if ( GetEnemy() != NULL )
	{
		TurnHeadToTarget( flInterval, GetEnemy()->EyePosition() );
	}
	else
	{
		TurnHeadToTarget( flInterval, vMoveTarget );
	}

	// -------------------------------------
	// Move towards our target
	// -------------------------------------
	float	myAccel;
	float	myZAccel = 300.0f;
	float	myDecay	 = 0.3f;

	Vector targetDir;
	float flDist;

	// If we're bursting, just head straight
	if ( m_flBurstDuration > gpGlobals->curtime )
	{
		float zDist = 500;

		// Steer towards our enemy if we're able to
		if ( GetEnemy() != NULL )
		{
			Vector steerDir = ( GetEnemy()->WorldSpaceCenter() - GetAbsOrigin() );
			zDist = fabs( steerDir.z );
			VectorNormalize( steerDir );

			float useTime = flInterval;
			while ( useTime > 0.0f )
			{
				m_vecBurstDirection += ( steerDir * 4.0f );
				useTime -= 0.1f;
			}

			m_vecBurstDirection.z = steerDir.z;

			VectorNormalize( m_vecBurstDirection );
		}

		// Debug visualizations
		/*
		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + ( targetDir * 64.0f ), 255, 0, 0, true, 2.1f );
		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + ( steerDir * 64.0f ), 0, 255, 0, true, 2.1f );
		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + ( m_vecBurstDirection * 64.0f ), 0, 0, 255, true, 2.1f );
		NDebugOverlay::Cross3D( GetAbsOrigin() , -Vector(8,8,8), Vector(8,8,8), 255, 0, 0, true, 2.1f );
		*/

		targetDir = m_vecBurstDirection;

		flDist	= FLT_MAX;
		myDecay	 = 0.3f;
		myAccel	 = 500;
		myZAccel = MIN( 500, zDist / flInterval );
	}
	else
	{
		Vector vecCurrentDir = GetCurrentVelocity();
		VectorNormalize( vecCurrentDir );

		targetDir = vMoveTarget - GetAbsOrigin();
		flDist = VectorNormalize( targetDir );
		
		float flDot = DotProduct( targetDir, vecCurrentDir );

		// Otherwise we should steer towards our goal
		if( flDot > 0.25 )
		{
			// If my target is in front of me, my flight model is a bit more accurate.
			myAccel = 300;
		}
		else
		{
			// Have a harder time correcting my course if I'm currently flying away from my target.
			myAccel = 200;
		}
	}

	// Clamp lateral acceleration
	if ( myAccel > ( flDist / flInterval ) )
	{
		myAccel = flDist / flInterval;
	}

	/*
	// Boost vertical movement
	if ( targetDir.z > 0 )
	{
		// Z acceleration is faster when we thrust upwards.
		// This is to help keep buzzers out of water. 
		myZAccel *= 5.0;
	}
	*/

	// Clamp vertical movement
	if ( myZAccel > flDist / flInterval )
	{
		myZAccel = flDist / flInterval;
	}

	// Scale by our engine force
	myAccel *= m_fEnginePowerScale;
	myZAccel *= m_fEnginePowerScale;
	
	MoveInDirection( flInterval, targetDir, myAccel, myZAccel, myDecay );

	// calc relative banking targets
	Vector forward, right;
	GetVectors( &forward, &right, NULL );
	m_vTargetBanking.x	= 40 * DotProduct( forward, targetDir );
	m_vTargetBanking.z	= 40 * DotProduct( right, targetDir );
	m_vTargetBanking.y	= 0.0;
}

void CASW_Buzzer::TurnHeadToTarget( float flInterval, const Vector &moveTarget )
{
	if ( m_iHealth <= 0 )
		return;

	BaseClass::TurnHeadToTarget( flInterval, moveTarget );
}


//-----------------------------------------------------------------------------
// Purpose: Ignore water if I'm close to my enemy
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CASW_Buzzer::MoveCollisionMask(void)
{
	return MASK_NPCSOLID;
}


//-----------------------------------------------------------------------------
// Purpose: Make a splash effect
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CASW_Buzzer::Splash( const Vector &vecSplashPos )
{
	CEffectData	data;

	data.m_fFlags = 0;
	data.m_vOrigin = vecSplashPos;
	data.m_vNormal = Vector( 0, 0, 1 );

	data.m_flScale = 8.0f;

	int contents = GetWaterType();

	// Verify we have valid contents
	if ( !( contents & (CONTENTS_SLIME|CONTENTS_WATER)))
	{
		// We're leaving the water so we have to reverify what it was
		trace_t	tr;
		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector( 0, 0, 256 ), (CONTENTS_WATER|CONTENTS_SLIME), this, COLLISION_GROUP_NONE, &tr );

		// Re-validate this
		if ( !(tr.contents&(CONTENTS_WATER|CONTENTS_SLIME)) )
		{
			//NOTENOTE: We called a splash but we don't seem to be near water?
			Assert( 0 );
			return;
		}

		contents = tr.contents;
	}
	
	// Mark us if we're in slime
	if ( contents & CONTENTS_SLIME )
	{
		data.m_fFlags |= FX_WATER_IN_SLIME;
	}

	DispatchEffect( "watersplash", data );
}

//-----------------------------------------------------------------------------
// Computes the slice bounce velocity
//-----------------------------------------------------------------------------
void CASW_Buzzer::ComputeSliceBounceVelocity( CBaseEntity *pHitEntity, trace_t &tr )
{
	if( pHitEntity->IsAlive() && FClassnameIs( pHitEntity, "func_breakable_surf" ) )
	{
		// We want to see if the buzzer hits a breakable pane of glass. To keep from checking
		// The classname of the HitEntity on each impact, we only do this check if we hit 
		// something that's alive. Anyway, prevent the buzzer bouncing off the pane of glass,
		// since this impact will shatter the glass and let the buzzer through.
		return;
	}

	Vector vecDir;
	
	// If the buzzer isn't bouncing away from whatever he sliced, force it.
	VectorSubtract( WorldSpaceCenter(), pHitEntity->WorldSpaceCenter(), vecDir );
	VectorNormalize( vecDir );
	vecDir *= 200;
	vecDir[2] = 0.0f;
	
	// Knock it away from us
	if ( VPhysicsGetObject() != NULL )
	{
		VPhysicsGetObject()->ApplyForceOffset( vecDir * 4, GetAbsOrigin() );
	}

	// Also set our velocity
	SetCurrentVelocity( vecDir );
}
	
//-----------------------------------------------------------------------------
// Purpose: We've touched something that we can hurt. Slice it!
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CASW_Buzzer::Slice( CBaseEntity *pHitEntity, float flInterval, trace_t &tr )
{
	// Don't hurt the player if I'm in water
	if( GetWaterLevel() > 0 && pHitEntity->IsPlayer() )
		return;

	if ( pHitEntity->m_takedamage == DAMAGE_NO )
		return;

	// don't damage marines again so soon
	if ( pHitEntity && pHitEntity->Classify() == CLASS_ASW_MARINE )
	{
		if ( ( gpGlobals->curtime - m_flLastDamageTime ) < sk_asw_buzzer_melee_interval.GetFloat() )
			return;
	}

	// Damage must be scaled by flInterval so framerate independent
	float flDamage = ASWGameRules()->ModifyAlienDamageBySkillLevel(sk_asw_buzzer_melee_dmg.GetFloat()) * flInterval;

	// cap it somewhat
	if (flDamage > 30)
	{
		flDamage = 30;
	}

	//if ( pHitEntity->IsPlayer() )
	//{
		//flDamage *= 2.0f;
	//}
	

	//else if ( pHitEntity->IsNPC() && HasPhysicsAttacker( ASW_BUZZER_SMASH_TIME ) )
	//{
		// NOTE: The else here is essential.
		// The physics attacker *will* be set even when the buzzer is held
		//flDamage = pHitEntity->GetHealth();
	//}
	if ( dynamic_cast<CBaseProp*>(pHitEntity) || dynamic_cast<CBreakable*>(pHitEntity) )
	{
		// If we hit a prop, we want it to break immediately
		flDamage = pHitEntity->GetHealth();
	}
	//else if ( pHitEntity->IsNPC() && IRelationType( pHitEntity ) == D_HT  && FClassnameIs( pHitEntity, "npc_combine_s" ) ) 
	//{
		//flDamage *= 6.0f;
	//}

	if (flDamage < 1.0f)
	{
		flDamage = 1.0f;
	}

	CTakeDamageInfo info( this, this, flDamage, DMG_SLASH | DMG_BLURPOISON );

	Vector dir = (tr.endpos - tr.startpos);
	if ( dir == vec3_origin )
	{
		dir = tr.m_pEnt->GetAbsOrigin() - GetAbsOrigin();
	}
	CalculateMeleeDamageForce( &info, dir, tr.endpos );
	pHitEntity->TakeDamage( info );

	// Spawn some extra blood where we hit
	if ( pHitEntity->BloodColor() == DONT_BLEED )
	{
		CEffectData data;
		Vector velocity = GetCurrentVelocity();

		data.m_vOrigin = tr.endpos;
		data.m_vAngles = GetAbsAngles();

		VectorNormalize( velocity );
		
		data.m_vNormal = ( tr.plane.normal + velocity ) * 0.5;;

		//DispatchEffect( "ManhackSparks", data );

		EmitSound( "ASW_Buzzer.Attack" );

		//TODO: What we really want to do is get a material reference and emit the proper sprayage! - jdw
	}
	else
	{
		SpawnBlood(tr.endpos, g_vecAttackDir, pHitEntity->BloodColor(), 6 );
		EmitSound( "ASW_Buzzer.Attack" );
	}

	// Pop back a little bit after hitting the player
	ComputeSliceBounceVelocity( pHitEntity, tr );

	// Save off when we last hit something
	m_flLastDamageTime = gpGlobals->curtime;

	// Reset our state and give the player time to react
	StopBurst( true );
}

//-----------------------------------------------------------------------------
// Purpose: We've touched something solid. Just bump it.
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CASW_Buzzer::Bump( CBaseEntity *pHitEntity, float flInterval, trace_t &tr )
{
	if ( !VPhysicsGetObject() )
		return;

	if ( pHitEntity->GetMoveType() == MOVETYPE_VPHYSICS && pHitEntity->Classify()!=CLASS_ASW_BUZZER )
	{
		HitPhysicsObject( pHitEntity );
	}

	// We've hit something so deflect our velocity based on the surface
	// norm of what we've hit
	if (flInterval > 0)
	{
		float moveLen = ( (GetCurrentVelocity() * flInterval) * (1.0 - tr.fraction) ).Length();

		Vector moveVec	= moveLen*tr.plane.normal/flInterval;

		// If I'm totally dead, don't bounce me up
		if (m_iHealth <=0 && moveVec.z > 0)
		{
			moveVec.z = 0;
		}

		// If I'm right over the ground don't push down
		if (moveVec.z < 0)
		{
			float floorZ = GetFloorZ(GetAbsOrigin());
			if (abs(GetAbsOrigin().z - floorZ) < 36)
			{
				moveVec.z = 0;
			}
		}

		Vector myUp;
		VPhysicsGetObject()->LocalToWorldVector( &myUp, Vector( 0.0, 0.0, 1.0 ) );

		// plane must be something that could hit the blades
		if (fabs( DotProduct( myUp, tr.plane.normal ) ) < 0.25 )
		{	
			// add some spin, but only if we're not already going fast..
			Vector vecVelocity;
			AngularImpulse vecAngVelocity;
			VPhysicsGetObject()->GetVelocity( &vecVelocity, &vecAngVelocity );
			float flDot = DotProduct( myUp, vecAngVelocity );
			if ( fabs(flDot) < 100 )
			{
				//AngularImpulse torque = myUp * (1000 - flDot * 10);
				AngularImpulse torque = myUp * (1000 - flDot * 2);

				// don't want buzzers to spin!
				//VPhysicsGetObject()->ApplyTorqueCenter( torque );
			}
			
			if (!(m_spawnflags	& SF_NPC_GAG))
			{
				EmitSound( "ASW_Buzzer.Attack" );
			}
		}
		
		VectorNormalize( moveVec );
		float hitAngle = -DotProduct( tr.plane.normal, -moveVec );

		Vector vReflection = 2.0 * tr.plane.normal * hitAngle + -moveVec;

		float deflectSpeed = clamp( GetCurrentVelocity().Length(), 100, 400 );

		SetCurrentVelocity( GetCurrentVelocity() + vReflection * deflectSpeed );
	}

	// -------------------------------------------------------------
	// If I'm on a path check LOS to my next node, and fail on path
	// if I don't have LOS.  Note this is the only place I do this,
	// so the buzzer has to collide before failing on a path
	// -------------------------------------------------------------
	if (GetNavigator()->IsGoalActive() && !(GetNavigator()->GetPath()->CurWaypointFlags() & bits_WP_TO_PATHCORNER) )
	{
		AIMoveTrace_t moveTrace;
		GetMoveProbe()->MoveLimit( NAV_GROUND, GetAbsOrigin(), GetNavigator()->GetCurWaypointPos(), 
			MoveCollisionMask(), GetEnemy(), &moveTrace );

		if (IsMoveBlocked( moveTrace ) && 
			!moveTrace.pObstruction->ClassMatches( GetClassname() ))
		{
			TaskFail(FAIL_NO_ROUTE);
			GetNavigator()->ClearGoal();
			return;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CASW_Buzzer::CheckCollisions(float flInterval)
{
	if ( !VPhysicsGetObject() )
		return;

	// Trace forward to see if I hit anything. 
	Vector vecTraceDir, vecCheckPos;
	VPhysicsGetObject()->GetVelocity( &vecTraceDir, NULL );
	vecTraceDir *= flInterval;	

	VectorAdd( GetAbsOrigin(), vecTraceDir, vecCheckPos );
	
	trace_t			tr;
	CBaseEntity*	pHitEntity = NULL;
	
	AI_TraceHull(	GetAbsOrigin(), 
					vecCheckPos, 
					GetHullMins(), 
					GetHullMaxs(),
					MoveCollisionMask(),
					this,
					COLLISION_GROUP_NONE,
					&tr );

	if ( (tr.fraction != 1.0 || tr.startsolid) && tr.m_pEnt)
	{
		PhysicsMarkEntitiesAsTouching( tr.m_pEnt, tr );
		pHitEntity = tr.m_pEnt;

		if ( pHitEntity != NULL && 
			 pHitEntity->m_takedamage == DAMAGE_YES && 
			 pHitEntity->Classify() != CLASS_ASW_BUZZER && 
			 gpGlobals->curtime > m_flWaterSuspendTime )
		{
			// Slice this thing
			Slice( pHitEntity, flInterval, tr );
			m_flWingFlapSpeed = 20.0;
		}
		else
		{
			// Just bump into this thing.
			Bump( pHitEntity, flInterval, tr );
			m_flWingFlapSpeed = 20.0;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
// Output :
//-----------------------------------------------------------------------------
#define tempTIME_STEP = 0.5;
void CASW_Buzzer::PlayFlySound(void)
{
	float flEnemyDist;

	if( GetEnemy() )
	{
		flEnemyDist = (GetAbsOrigin() - GetEnemy()->GetAbsOrigin()).Length();
	}
	else
	{
		flEnemyDist = FLT_MAX;
	}

	if( m_spawnflags & SF_NPC_GAG )
	{
		// Quiet!
		return;
	}

	if( m_flWaterSuspendTime > gpGlobals->curtime )
	{
		// Just went in water. Slow the motor!!
		if( m_bDirtyPitch )
		{
			m_nEnginePitch1 = ASW_BUZZER_WATER_PITCH1;
			m_flEnginePitch1Time = gpGlobals->curtime + 0.5f;
			m_bDirtyPitch = false;
		}
	}
	// Spin sound based on distance from enemy (unless we're crashing)
	else if (GetEnemy() && IsAlive() )
	{
		if( flEnemyDist < ASW_BUZZER_PITCH_DIST1 )
		{
			// recalculate pitch.
			int iPitch1, iPitch2;
			float flDistFactor;

			flDistFactor = MIN( 1.0, 1 - flEnemyDist / ASW_BUZZER_PITCH_DIST1 ); 
			iPitch1 = ASW_BUZZER_MIN_PITCH1 + ( ( ASW_BUZZER_MAX_PITCH1 - ASW_BUZZER_MIN_PITCH1 ) * flDistFactor); 

			// NOTE: ASW_BUZZER_PITCH_DIST2 must be < ASW_BUZZER_PITCH_DIST1
			flDistFactor = MIN( 1.0, 1 - flEnemyDist / ASW_BUZZER_PITCH_DIST2 ); 
			iPitch2 = ASW_BUZZER_MIN_PITCH2 + ( ( ASW_BUZZER_MAX_PITCH2 - ASW_BUZZER_MIN_PITCH2 ) * flDistFactor); 

			m_nEnginePitch1 = iPitch1;
			m_flEnginePitch1Time = gpGlobals->curtime + 0.1f;

			m_bDirtyPitch = true;
		}
		else if( m_bDirtyPitch )
		{
			m_nEnginePitch1 = ASW_BUZZER_MIN_PITCH1;
			m_flEnginePitch1Time = gpGlobals->curtime + 0.1f;
			m_bDirtyPitch = false;
		}
	}
	// If no enemy just play low sound
	else if( IsAlive() && m_bDirtyPitch )
	{
		m_nEnginePitch1 = ASW_BUZZER_MIN_PITCH1;
		m_flEnginePitch1Time = gpGlobals->curtime + 0.1f;

		m_bDirtyPitch = false;
	}

	if (IsOnFire())
		m_nEnginePitch1 = ASW_BUZZER_MAX_PITCH1;

	// Play special engine every once in a while
	if (gpGlobals->curtime > m_flNextEngineSoundTime && flEnemyDist < 48)
	{
		m_flNextEngineSoundTime	= gpGlobals->curtime + random->RandomFloat( 3.0, 10.0 );

		//EmitSound( "NPC_Manhack.EngineNoise" );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CASW_Buzzer::MoveExecute_Alive(float flInterval)
{
	PhysicsCheckWaterTransition();

	Vector	vCurrentVelocity = GetCurrentVelocity();

	// FIXME: move this
	VPhysicsGetObject()->Wake();

	if( m_fEnginePowerScale < GetMaxEnginePower() && gpGlobals->curtime > m_flWaterSuspendTime )
	{
		// Power is low, and we're no longer stuck in water, so bring power up.
		m_fEnginePowerScale += 0.05;
	}

	// ----------------------------------------------------------------------------------------
	// Add time-coherent noise to the current velocity so that it never looks bolted in place.
	// ----------------------------------------------------------------------------------------
	float noiseScale = 7.0f;

	if ( (CBaseEntity*)GetEnemy() )
	{
		float flDist = (GetAbsOrigin() - GetEnemy()->GetAbsOrigin()).Length2D();

		if( flDist < ASW_BUZZER_CHARGE_MIN_DIST )
		{
			// Less noise up close.
			noiseScale = 2.0;
		}

		if ( IsInEffectiveTargetZone( GetEnemy() ) && flDist < ASW_BUZZER_CHARGE_MIN_DIST && gpGlobals->curtime > m_flNextBurstTime )
		{
			Vector vecCurrentDir = GetCurrentVelocity();
			VectorNormalize( vecCurrentDir );

			Vector vecToEnemy = ( GetEnemy()->WorldSpaceCenter() - WorldSpaceCenter() );
			VectorNormalize( vecToEnemy );

			float flDot = DotProduct( vecCurrentDir, vecToEnemy );

			if ( flDot > 0.75 )
			{				
				Vector offsetDir = ( vecToEnemy - vecCurrentDir );
				VectorNormalize( offsetDir );

				Vector offsetSpeed = GetCurrentVelocity() * flDot;

				//FIXME: This code sucks -- jdw

				offsetDir.z = 0.0f;
				m_vForceVelocity += ( offsetDir * ( offsetSpeed.Length2D() * 0.25f ) );

				// Commit to the attack- no steering for about a second
				StartBurst( vecToEnemy );
			}
		}
		
		if ( gpGlobals->curtime > m_flBurstDuration )
		{
			ShowHostile( false );
		}
	}

	// ----------------------------------------------------------------------------------------
	// Add in any forced velocity
	// ----------------------------------------------------------------------------------------
	SetCurrentVelocity( vCurrentVelocity + m_vForceVelocity );
	m_vForceVelocity = vec3_origin;

	if( GetEnemy() )
	{
		// If hacked and no enemy, don't drift!
		AddNoiseToVelocity( noiseScale );
	}

	LimitSpeed( 200, ManhackMaxSpeed() );

	if( m_flWaterSuspendTime > gpGlobals->curtime )
	{ 
		if( UTIL_PointContents( GetAbsOrigin(),(CONTENTS_WATER|CONTENTS_SLIME) ) & (CONTENTS_WATER|CONTENTS_SLIME) )
		{
			// Ooops, we're submerged somehow. Move upwards until our origin is out of the water.
			m_vCurrentVelocity.z = 20.0;
		}
		else
		{
			// Skimming the surface. Forbid any movement on Z
			m_vCurrentVelocity.z = 0.0;
		}
	}
	else if( GetWaterLevel() > 0 )
	{
		// Allow the buzzer to lift off, but not to go deeper.
		m_vCurrentVelocity.z = MAX( m_vCurrentVelocity.z, 0 );
	}

	CheckCollisions(flInterval);	

	QAngle angles = GetLocalAngles();

	// ------------------------------------------
	//  Stalling
	// ------------------------------------------
	if (gpGlobals->curtime < m_flEngineStallTime)
	{
		/*
		// If I'm stalled add random noise
		angles.x += -20+(random->RandomInt(-10,10));
		angles.z += -20+(random->RandomInt(0,40));

		TurnHeadRandomly(flInterval);
		*/
	}
	else
	{
		// Make frame rate independent
		float	iRate	 = 0.5;
		float timeToUse = flInterval;
		while (timeToUse > 0)
		{
			m_vCurrentBanking.x = (iRate * m_vCurrentBanking.x) + (1 - iRate)*(m_vTargetBanking.x);
			m_vCurrentBanking.z = (iRate * m_vCurrentBanking.z) + (1 - iRate)*(m_vTargetBanking.z);
			timeToUse =- 0.1;
		}
		angles.x = m_vCurrentBanking.x;
		angles.z = m_vCurrentBanking.z;
		angles.y = 0;

#if 0
		// Using our steering if we're not otherwise affecting our panels
		if ( m_flEngineStallTime < gpGlobals->curtime && m_flBurstDuration < gpGlobals->curtime )
		{
			Vector delta( 10 * AngleDiff( m_vTargetBanking.x, m_vCurrentBanking.x ), -10 * AngleDiff( m_vTargetBanking.z, m_vCurrentBanking.z ), 0 );
			//Vector delta( 3 * AngleNormalize( m_vCurrentBanking.x ), -4 * AngleNormalize( m_vCurrentBanking.z ), 0 );
			VectorYawRotate( delta, -m_fHeadYaw, delta );

			// DevMsg("%.0f %.0f\n", delta.x, delta.y )
		}
#endif
	}

	// SetLocalAngles( angles );

	if( m_lifeState != LIFE_DEAD )
	{
		PlayFlySound();
		// WalkMove( GetCurrentVelocity() * flInterval, MASK_NPCSOLID );
	}

//	 NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + Vector(0, 0, -10 ), 0, 255, 0, true, 0.1);
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CASW_Buzzer::FlapWings(float flInterval)
{
	if ( IsFlyingActivity( GetActivity() ) )
	{
		// Blades may only ramp up while the engine is running
		if ( m_flEngineStallTime < gpGlobals->curtime )
		{
			if (m_flWingFlapSpeed < 10)
			{
				m_flWingFlapSpeed = m_flWingFlapSpeed * 2 + 1;
			}
			else
			{
				// accelerate engine
				m_flWingFlapSpeed = m_flWingFlapSpeed + 80 * flInterval;
			}
		}

		if (m_flWingFlapSpeed > 100)
		{
			m_flWingFlapSpeed = 100;
		}

		// blend through blades, blades+blur, blur
		if (m_flWingFlapSpeed < 20)
		{
			SetBodygroup( ASW_BUZZER_BODYGROUP_BLADE, ASW_BUZZER_BODYGROUP_ON );
			SetBodygroup( ASW_BUZZER_BODYGROUP_BLUR, ASW_BUZZER_BODYGROUP_OFF );
		}
		else if (m_flWingFlapSpeed < 40)
		{
			SetBodygroup( ASW_BUZZER_BODYGROUP_BLADE, ASW_BUZZER_BODYGROUP_ON );
			SetBodygroup( ASW_BUZZER_BODYGROUP_BLUR, ASW_BUZZER_BODYGROUP_ON );
		}
		else
		{
			SetBodygroup( ASW_BUZZER_BODYGROUP_BLADE, ASW_BUZZER_BODYGROUP_OFF );
			SetBodygroup( ASW_BUZZER_BODYGROUP_BLUR, ASW_BUZZER_BODYGROUP_ON );
		}

		m_flPlaybackRate = m_flWingFlapSpeed / 100.0;
	}
	else
	{
		m_flWingFlapSpeed = 0.0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Smokes and sparks, exploding periodically. Eventually it goes away.
//-----------------------------------------------------------------------------
void CASW_Buzzer::MoveExecute_Dead(float flInterval)
{
	if ( m_fSuicideTime == 0.0f )
	{
		m_fSuicideTime = gpGlobals->curtime + 1.5;
	}
	else if ( gpGlobals->curtime > m_fSuicideTime )
	{
		CTakeDamageInfo info( NULL, NULL, Vector(0,0,0), GetAbsOrigin(), GetHealth() * 2, DMG_ACID );
		Event_Killed( info );
	}

	if( GetWaterLevel() > 0 )
	{
		// No movement if sinking in water.
		return;
	}

	Vector newVelocity = GetCurrentVelocity();

	// accelerate faster and faster when dying
	newVelocity = newVelocity + (newVelocity * 1.5 * flInterval );

	// Lose lift
	newVelocity.z -= 0.2*flInterval*(sv_gravity.GetFloat());

	// ----------------------------------------------------------------------------------------
	// Add in any forced velocity
	// ----------------------------------------------------------------------------------------
	newVelocity += m_vForceVelocity;
	SetCurrentVelocity( newVelocity );
	m_vForceVelocity = vec3_origin;


	// Lots of noise!! Out of control!
	AddNoiseToVelocity( 5.0 );


	// ----------------------
	// Limit overall speed
	// ----------------------
	LimitSpeed( -1, ASW_BUZZER_MAX_SPEED * 2.0 );

	QAngle angles = GetLocalAngles();

	// ------------------------------------------
	// If I'm dying, add random banking noise
	// ------------------------------------------
	angles.x += -20+(random->RandomInt(0,40));
	angles.z += -20+(random->RandomInt(0,40));

	CheckCollisions(flInterval);
	PlayFlySound();

	// SetLocalAngles( angles );

	WalkMove( GetCurrentVelocity() * flInterval,MASK_NPCSOLID );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Buzzer::Precache(void)
{
	//
	// Model.
	//
	PrecacheModel( ASW_BUZZER_MODEL );
	PropBreakablePrecacheAll( MAKE_STRING(ASW_BUZZER_MODEL) );
		
	PrecacheScriptSound( "ASW_Buzzer.Attack" );
	PrecacheScriptSound( "ASW_Buzzer.Death" );
	PrecacheScriptSound( "ASW_Buzzer.Pain" );
	PrecacheScriptSound( "ASW_Buzzer.Idle" );
	PrecacheScriptSound( "ASW_Buzzer.OnFire" );

	PrecacheParticleSystem( "buzzer_trail" );
	PrecacheParticleSystem( "buzzer_death" );

	// Sounds used on Client:
	//PrecacheScriptSound( "NPC_Manhack.EngineSound1" );
	//PrecacheScriptSound( "NPC_Manhack.EngineSound2"  );
	//PrecacheScriptSound( "NPC_Manhack.BladeSound" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CASW_Buzzer::GatherEnemyConditions( CBaseEntity *pEnemy )
{
	// The Manhack "regroups" when its in attack range but to
	// far above or below its enemy.  Set the start attack
	// condition if we are far enough away from the enemy
	// or at the correct height

	// Don't bother with Z if the enemy is in a vehicle
	float fl2DDist = 60.0f;
	float flZDist = 12.0f;

	if ( GetEnemy()->IsPlayer() && assert_cast< CBasePlayer * >(GetEnemy())->IsInAVehicle() )
	{
		flZDist = 24.0f;
	}

	if ((GetAbsOrigin() - pEnemy->GetAbsOrigin()).Length2D() > fl2DDist) 
	{
		SetCondition(COND_ASW_BUZZER_START_ATTACK);
	}
	else
	{
		float targetZ	= pEnemy->EyePosition().z;
		if (fabs(GetAbsOrigin().z - targetZ) < flZDist)
		{
			SetCondition(COND_ASW_BUZZER_START_ATTACK);
		}
	}
	BaseClass::GatherEnemyConditions(pEnemy);
}


//-----------------------------------------------------------------------------
// Purpose: For innate melee attack
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CASW_Buzzer::MeleeAttack1Conditions( float flDot, float flDist )
{
	if ( GetEnemy() == NULL )
		return COND_NONE;

	//TODO: We could also decide if we want to back up here
	if ( m_flNextBurstTime > gpGlobals->curtime )
		return COND_NONE;

	float flMaxDist = 45;
	float flMinDist = 24;
	bool bEnemyInVehicle = GetEnemy()->IsPlayer() && assert_cast< CBasePlayer * >(GetEnemy())->IsInAVehicle();
	if ( GetEnemy()->IsPlayer() && assert_cast< CBasePlayer * >(GetEnemy())->IsInAVehicle() )
	{
		flMinDist = 0;
		flMaxDist = 200.0f;
	}

	if (flDist > flMaxDist)
	{
		return COND_TOO_FAR_TO_ATTACK;
	}

	if (flDist < flMinDist)
	{
		return COND_TOO_CLOSE_TO_ATTACK;
	}

	// Check our current velocity and speed, if it's too far off, we need to settle

	// Don't bother with Z if the enemy is in a vehicle
	if ( bEnemyInVehicle )
	{
		return COND_CAN_MELEE_ATTACK1;
	}

	// Assume the this check is in regards to my current enemy
	// for the Manhacks spetial condition
	float deltaZ = GetAbsOrigin().z - GetEnemy()->EyePosition().z;
	if ( (deltaZ > 12.0f) || (deltaZ < -24.0f) )
	{
		return COND_TOO_CLOSE_TO_ATTACK;
	}

	return COND_CAN_MELEE_ATTACK1;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTask - 
//-----------------------------------------------------------------------------
void CASW_Buzzer::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		// Override this task so we go for the enemy at eye level
	case TASK_ASW_BUZZER_HOVER:
		{
			break;
		}

	// If my enemy has moved significantly, update my path
	case TASK_WAIT_FOR_MOVEMENT:
		{
			CBaseEntity *pEnemy = GetEnemy();
			if (pEnemy &&
				(GetCurSchedule()->GetId() == SCHED_CHASE_ENEMY) && 
				GetNavigator()->IsGoalActive() )
			{
				Vector vecEnemyPosition;
				vecEnemyPosition = pEnemy->EyePosition();
				if ( GetNavigator()->GetGoalPos().DistToSqr(vecEnemyPosition) > 40 * 40 )
				{
					GetNavigator()->UpdateGoalPos( vecEnemyPosition );
				}
			}				
			BaseClass::RunTask(pTask);
			break;
		}

	case TASK_ASW_BUZZER_MOVEAT_SAVEPOSITION:
		{
			// do the movement thingy

//			NDebugOverlay::Line( GetAbsOrigin(), m_vSavePosition, 0, 255, 0, true, 0.1);

			Vector dir = (m_vSavePosition - GetAbsOrigin());
			float dist = VectorNormalize( dir );
			float t = m_fSwarmMoveTime - gpGlobals->curtime;

			if (t < 0.1)
			{
				if (dist > 256)
				{
					TaskFail( FAIL_NO_ROUTE );
				}
				else
				{
					TaskComplete();
				}
			}
			else if (dist < 64)
			{
				m_vSwarmMoveTarget = GetAbsOrigin() - Vector( -dir.y, dir.x, 0 ) * 4;
			}
			else
			{
				m_vSwarmMoveTarget = GetAbsOrigin() + dir * 10;
			}
			break;
		}

	default:
		{
			BaseClass::RunTask(pTask);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Buzzer::Spawn(void)
{
	Precache();

	SetModel( ASW_BUZZER_MODEL );
	SetHullType(HULL_TINY_CENTERED); 
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	
	SetMoveType( MOVETYPE_VPHYSICS );

	SetHealthByDifficultyLevel();
	SetViewOffset( Vector(0, 0, 10) );		// Position of the eyes relative to NPC's origin.
	m_flFieldOfView		= VIEW_FIELD_FULL;
	m_NPCState			= NPC_STATE_NONE;

	if ( m_spawnflags & SF_ASW_BUZZER_USE_AIR_NODES)
	{
		SetNavType(NAV_FLY);
	}
	else
	{
		SetNavType(NAV_GROUND);
	}

	ChangeFaction( FACTION_ALIENS );
		 
	AddEFlags( EFL_NO_DISSOLVE | EFL_NO_MEGAPHYSCANNON_RAGDOLL );
	SetBloodColor( DONT_BLEED );
	SetCurrentVelocity( vec3_origin );
	m_vForceVelocity.Init();
	m_vCurrentBanking.Init();
	m_vTargetBanking.Init();

	m_flNextBurstTime	= gpGlobals->curtime;

	CapabilitiesAdd( bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_MOVE_FLY | bits_CAP_SQUAD );

	m_flNextEngineSoundTime		= gpGlobals->curtime;
	m_flWaterSuspendTime		= gpGlobals->curtime;
	m_flEngineStallTime			= gpGlobals->curtime;
	m_fForceMoveTime			= gpGlobals->curtime;
	m_vForceMoveTarget			= vec3_origin;
	m_fSwarmMoveTime			= gpGlobals->curtime;
	m_vSwarmMoveTarget			= vec3_origin;

	// Set the noise mod to huge numbers right now, in case this buzzer starts out waiting for a script
	// for instance, we don't want him to bob whilst he's waiting for a script. This allows designers
	// to 'hide' buzzers in small places. (sjb)
	SetNoiseMod( ASW_BUZZER_NOISEMOD_HIDE, ASW_BUZZER_NOISEMOD_HIDE, ASW_BUZZER_NOISEMOD_HIDE );

	// Start out with full power! 
	m_fEnginePowerScale = GetMaxEnginePower();
	
	// find panels
	m_iPanel1 = LookupPoseParameter( "Panel1" );
	m_iPanel2 = LookupPoseParameter( "Panel2" );
	m_iPanel3 = LookupPoseParameter( "Panel3" );
	m_iPanel4 = LookupPoseParameter( "Panel4" );

	m_fHeadYaw			= 0;
	m_pMoanSound = NULL;

	NPCInit();

	// Manhacks are designed to slam into things, so don't take much damage from it!
	SetImpactEnergyScale( 0.001 );

	// Manhacks get 30 seconds worth of free knowledge.
	GetEnemies()->SetFreeKnowledgeDuration( 30.0 );
	
	// makes us collide with everything, but the player can walk through us (needed since the player has no vphys shadow to stop us going into him)
	SetCollisionGroup( ASW_COLLISION_GROUP_BUZZER );

	m_bHeld = false;
	StopLoitering();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Buzzer::StartEye( void )
{

}

//-----------------------------------------------------------------------------

void CASW_Buzzer::Activate()
{
	BaseClass::Activate();

	if ( IsAlive() )
	{
		StartEye();
	}
}

void CASW_Buzzer::NPCInit()
{
	BaseClass::NPCInit();

	// set default alien swarm sight/sense distances
	SetDistSwarmSense(576.0f);
	SetDistLook( 768.0f );
	m_flDistTooFar = 1500.0f;	// seems this is used as an early out for checking attack conditions or not, also for LOS?

	// if flagged, alien can see twice as far
	if ( HasSpawnFlags( SF_NPC_LONG_RANGE ) )
	{		
		SetDistSwarmSense(1152.0f);
		SetDistLook( 1536.0f );
		m_flDistTooFar = 2000.0f;
	}

	CASW_GameStats.Event_AlienSpawned( this );

	m_LagCompensation.Init(this);
	SetDistSwarmSense(576.0f);
}

void CASW_Buzzer::NPCThink( void )
{
	BaseClass::NPCThink();

	// stop electro stunning if we're slowed
	if (m_bElectroStunned && m_flElectroStunSlowMoveTime < gpGlobals->curtime)
		m_bElectroStunned = false;

	if (gpGlobals->maxClients > 1)
		m_LagCompensation.StorePositionHistory();
}

void CASW_Buzzer::SetDistSwarmSense( float flDistSense )
{
	if (!GetASWSenses())
		return;
	GetASWSenses()->SetDistSwarmSense( flDistSense );
}

//-----------------------------------------------------------------------------
// Purpose: Get the engine sound started. Unless we're not supposed to have it on yet!
//-----------------------------------------------------------------------------
void CASW_Buzzer::PostNPCInit( void )
{
	// SetAbsVelocity( vec3_origin ); 
	BladesInit();
}

void CASW_Buzzer::BladesInit()
{	
	bool engineSound = (m_spawnflags & SF_NPC_GAG) ? false : true;
	StartEngine( engineSound );
	SetActivity( ACT_FLY );
}


//-----------------------------------------------------------------------------
// Crank up the engine!
//-----------------------------------------------------------------------------
void CASW_Buzzer::StartEngine( bool fStartSound )
{
	if( fStartSound )
	{
		SoundInit();
	}

	// Make the blade appear.
	SetBodygroup( 1, 1 );

	// Pop up a little if falling fast!
	Vector vecVelocity;
	GetVelocity( &vecVelocity, NULL );

	// Under powered flight now.
	// SetMoveType( MOVETYPE_STEP );
	// SetGravity( ASW_BUZZER_GRAVITY );
	AddFlag( FL_FLY );
}


//-----------------------------------------------------------------------------
// Purpose: Start the buzzer's engine sound.
//-----------------------------------------------------------------------------
void CASW_Buzzer::SoundInit( void )
{
	m_nEnginePitch1 = ASW_BUZZER_MIN_PITCH1;
	m_flEnginePitch1Time = gpGlobals->curtime;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Buzzer::StopLoopingSounds(void)
{
	BaseClass::StopLoopingSounds();
	m_nEnginePitch1 = 0;
	m_flEnginePitch1Time = gpGlobals->curtime;
	CSoundEnvelopeController::GetController().SoundDestroy( m_pMoanSound );
	m_pMoanSound = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTask - 
//-----------------------------------------------------------------------------
void CASW_Buzzer::StartTask( const Task_t *pTask )
{
	switch (pTask->iTask)
	{	
	case TASK_ASW_BUZZER_HOVER:
		break;
	case TASK_ASW_BUZZER_BUILD_PATH_TO_ORDER:
		{
			CBaseEntity *pGoalEnt = m_AlienOrderObject;	
			m_bFailedMoveTo = false;
			if (pGoalEnt)
			{
				// if our target is a path corner, do normal AI following of that route
				bool bIsFlying = (GetMoveType() == MOVETYPE_FLY) || (GetMoveType() == MOVETYPE_FLYGRAVITY);
				if (pGoalEnt->ClassMatches("path_corner"))
				{
					SetGoalEnt(pGoalEnt);
					
					AI_NavGoal_t goal( GOALTYPE_PATHCORNER, pGoalEnt->GetAbsOrigin(),
					   bIsFlying ? ACT_FLY : ACT_WALK,
					   AIN_DEF_TOLERANCE, AIN_YAW_TO_DEST);

					SetState( NPC_STATE_IDLE );
					if ( !GetNavigator()->SetGoal( goal ) )
					{
						DevWarning( 2, "Can't Create Route!\n" );
						m_bFailedMoveTo = true;
					}				
					else
					{
						TaskComplete();
					}
				}
				else
				{
					// HACKHACK: Call through TranslateNavGoal to fixup this goal position
					// UNDONE: Remove this and have NPCs that need this functionality fix up paths in the 
					// movement system instead of when they are specified.
					AI_NavGoal_t goal(pGoalEnt->GetAbsOrigin(), bIsFlying ? ACT_FLY : ACT_WALK, AIN_DEF_TOLERANCE, AIN_YAW_TO_DEST);
					TranslateNavGoal( pGoalEnt, goal.dest );
					
					if (!GetNavigator()->SetGoal( goal ))
					{
						DevWarning( 2, "Can't Create Route!\n" );
						m_bFailedMoveTo = true;
					}	
					else
					{
						TaskComplete();
					}
				}
			}
			if (!pGoalEnt || m_bFailedMoveTo)
			{
				//m_AlienOrders = AOT_SpreadThenHibernate;	// make sure our orders are set correctly for this action (may be incorrect if a MoveTo order fails)
				if (pTask->flTaskData <= 0)		// check the taskdata to see if we should do a short spread movement when failing to build a path
				{
					// random nearby position
					if ( GetNavigator()->SetWanderGoal( 90, 200 ) )
					{
						TaskComplete();
					}
					else
					{
						// if we couldn't go for a full path
						if ( GetNavigator()->SetRandomGoal( 150.0f ) )
						{
							TaskComplete();
						}
						TaskFail(FAIL_NO_ROUTE);
					}
				}
				else
				{
					TaskFail(FAIL_NO_ROUTE);
				}
			}
		}
		break;

	case TASK_MOVE_TO_TARGET_RANGE:
	case TASK_GET_PATH_TO_GOAL:
	case TASK_GET_PATH_TO_ENEMY_LKP:
	case TASK_GET_PATH_TO_PLAYER:
		{
			BaseClass::StartTask( pTask );
			/*
			// FIXME: why were these tasks considered bad?
			_asm
			{
				int	3;
				int 5;
			}
			*/
		}
		break;

	case TASK_FACE_IDEAL:
		{
			// this shouldn't ever happen, but if it does, don't choke
			TaskComplete();
		}
		break;

	case TASK_GET_PATH_TO_ENEMY:
		{
			if (IsUnreachable(GetEnemy()))
			{
				TaskFail(FAIL_NO_ROUTE);
				return;
			}

			CBaseEntity *pEnemy = GetEnemy();

			if ( pEnemy == NULL )
			{
				TaskFail(FAIL_NO_ENEMY);
				return;
			}
						
			if ( GetNavigator()->SetGoal( GOALTYPE_ENEMY ) )
			{
				TaskComplete();
			}
			else
			{
				// no way to get there =( 
				DevWarning( 2, "GetPathToEnemy failed!!\n" );
				RememberUnreachable(GetEnemy());
				TaskFail(FAIL_NO_ROUTE);
			}
			break;
		}
		break;

	case TASK_GET_PATH_TO_TARGET:
		// DevMsg("TARGET\n");
		BaseClass::StartTask( pTask );
		break;

	case TASK_ASW_BUZZER_FIND_SQUAD_CENTER:
		{
			if (!m_pSquad)
			{
				m_vSavePosition = GetAbsOrigin();
				TaskComplete();
				break;
			}

			// calc center of squad
			int count = 0;
			m_vSavePosition = Vector( 0, 0, 0 );

			// give attacking members more influence
			AISquadIter_t iter;
			for (CAI_BaseNPC *pSquadMember = m_pSquad->GetFirstMember( &iter ); pSquadMember; pSquadMember = m_pSquad->GetNextMember( &iter ) )
			{
				if (pSquadMember->HasStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ))
				{
					m_vSavePosition += pSquadMember->GetAbsOrigin() * 10;
					count += 10;
				}
				else
				{
					m_vSavePosition += pSquadMember->GetAbsOrigin();
					count++;
				}
			}

			// pull towards enemy
			if (GetEnemy() != NULL)
			{
				m_vSavePosition += GetEnemyLKP() * 4;
				count += 4;
			}

			Assert( count != 0 );
			m_vSavePosition = m_vSavePosition * (1.0 / count);

			TaskComplete();
		}
		break;

	case TASK_ASW_BUZZER_FIND_SQUAD_MEMBER:
		{
			if (m_pSquad)
			{
				CAI_BaseNPC *pSquadMember = m_pSquad->GetAnyMember();
				m_vSavePosition = pSquadMember->GetAbsOrigin();

				// find attacking members
				AISquadIter_t iter;
				for (pSquadMember = m_pSquad->GetFirstMember( &iter ); pSquadMember; pSquadMember = m_pSquad->GetNextMember( &iter ) )
				{
					// are they attacking?
					if (pSquadMember->HasStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ))
					{
						m_vSavePosition = pSquadMember->GetAbsOrigin();
						break;
					}
					// do they have a goal?
					if (pSquadMember->GetNavigator()->IsGoalActive())
					{
						m_vSavePosition = pSquadMember->GetAbsOrigin();
						break;
					}
				}
			}
			else
			{
				m_vSavePosition = GetAbsOrigin();
			}

			TaskComplete();
		}
		break;

	case TASK_ASW_BUZZER_MOVEAT_SAVEPOSITION:
		{
			trace_t tr;
			AI_TraceLine( GetAbsOrigin(), m_vSavePosition, MASK_NPCWORLDSTATIC, this, COLLISION_GROUP_NONE, &tr );
			if (tr.DidHitWorld())
			{
				TaskFail( FAIL_NO_ROUTE );
			}
			else
			{
				m_fSwarmMoveTime = gpGlobals->curtime + RandomFloat( pTask->flTaskData * 0.8, pTask->flTaskData * 1.2 );
			}
		}
		break;

	default:
		BaseClass::StartTask(pTask);
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose:  This is a generic function (to be implemented by sub-classes) to
//			 handle specific interactions between different types of characters
//			 (For example the barnacle grabbing an NPC)
// Input  :  Constant for the type of interaction
// Output :	 true  - if sub-class has a response for the interaction
//			 false - if sub-class has no response
//-----------------------------------------------------------------------------
bool CASW_Buzzer::HandleInteraction(int interactionType, void* data, CBaseCombatCharacter* sourceEnt)
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CASW_Buzzer::ManhackMaxSpeed( void )
{
	if( m_flWaterSuspendTime > gpGlobals->curtime 
			|| m_flElectroStunSlowMoveTime > gpGlobals->curtime )
	{
		// Slower in water!
		return ASW_BUZZER_MAX_SPEED * 0.1f;
	}

	if( m_fHurtSlowMoveTime > gpGlobals->curtime )
	{
		return ASW_BUZZER_MAX_SPEED * 0.5f;
	}

	return ASW_BUZZER_MAX_SPEED;
}



//-----------------------------------------------------------------------------
// Purpose: 
// Output :
//-----------------------------------------------------------------------------
void CASW_Buzzer::ClampMotorForces( Vector &linear, AngularImpulse &angular )
{
	float scale = m_flWingFlapSpeed / 100.0;

	// Msg("%.0f %.0f %.0f\n", linear.x, linear.y, linear.z );

	float fscale = 3000 * scale;

	if ( m_flEngineStallTime > gpGlobals->curtime )
	{
		linear.x = 0.0f;
		linear.y = 0.0f;
		linear.z = clamp( linear.z, -fscale, fscale < 1200 ? 1200 : fscale );
	}
	else
	{
		// limit reaction forces
		linear.x = clamp( linear.x, -fscale, fscale );
		linear.y = clamp( linear.y, -fscale, fscale );
		linear.z = clamp( linear.z, -fscale, fscale < 1200 ? 1200 : fscale );
	}

	angular.x *= scale;
	angular.y *= scale;
	angular.z *= scale;
}

//-----------------------------------------------------------------------------
// Purpose: Tests whether we're above the target's feet but also below their top
// Input  : *pTarget - who we're testing against
//-----------------------------------------------------------------------------
bool CASW_Buzzer::IsInEffectiveTargetZone( CBaseEntity *pTarget )
{
	Vector	vecMaxPos, vecMinPos;
	float	ourHeight = WorldSpaceCenter().z;

	// If the enemy is in a vehicle, we need to get those bounds
	if ( pTarget && pTarget->IsPlayer() && assert_cast< CBasePlayer * >(pTarget)->IsInAVehicle() )
	{
		CBaseEntity *pVehicle = assert_cast< CBasePlayer * >(pTarget)->GetVehicleEntity();
		pVehicle->CollisionProp()->NormalizedToWorldSpace( Vector(0.0f,0.0f,1.0f), &vecMaxPos );
		pVehicle->CollisionProp()->NormalizedToWorldSpace( Vector(0.0f,0.0f,0.0f), &vecMinPos );
	
		if ( ourHeight > vecMinPos.z && ourHeight < vecMaxPos.z )
			return true;

		return false;
	}
	
	// Get the enemies top and bottom point
	pTarget->CollisionProp()->NormalizedToWorldSpace( Vector(0.0f,0.0f,1.0f), &vecMaxPos );
	pTarget->CollisionProp()->NormalizedToWorldSpace( Vector(0.0f,0.0f,0.0f), &vecMinPos );

	// See if we're within that range
	if ( ourHeight > vecMinPos.z && ourHeight < vecMaxPos.z )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEnemy - 
//			&chasePosition - 
//			&tolerance - 
//-----------------------------------------------------------------------------
void CASW_Buzzer::TranslateNavGoal( CBaseEntity *pEnemy, Vector &chasePosition )
{
	if ( pEnemy && pEnemy->IsPlayer() && assert_cast< CBasePlayer * >(pEnemy)->IsInAVehicle() )
	{
		Vector vecNewPos;
		CBaseEntity *pVehicle = assert_cast< CBasePlayer * >(pEnemy)->GetVehicleEntity();
		pVehicle->CollisionProp()->NormalizedToWorldSpace( Vector(0.5,0.5,0.5f), &vecNewPos );
		chasePosition.z = vecNewPos.z;
	}
	else
	{
		Vector vecTarget;
		pEnemy->CollisionProp()->NormalizedToCollisionSpace( Vector(0,0,0.75f), &vecTarget );
		chasePosition.z += vecTarget.z;
	}
}

float CASW_Buzzer::GetDefaultNavGoalTolerance()
{
	return GetHullWidth();
}

//-----------------------------------------------------------------------------
// Freezes this NPC in place for a period of time.
//-----------------------------------------------------------------------------
void CASW_Buzzer::Freeze( float flFreezeAmount, CBaseEntity *pFreezer, Ray_t *pFreezeRay ) 
{
	BaseClass::Freeze( flFreezeAmount, pFreezer, pFreezeRay );
	
	if ( GetMoveType() != MOVETYPE_NONE && GetFrozenAmount() > 0.0f )
	{
		// Freeze makes us hate life!
		m_flFrozen = 1.0f;
		m_flFrozenThawRate = 0.0f;

		m_iHealth = 0;
	}
}

bool CASW_Buzzer::ShouldBecomeStatue( void )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Input that disables the buzzer's swarm behavior
//-----------------------------------------------------------------------------
void CASW_Buzzer::InputDisableSwarm( inputdata_t &inputdata )
{
	m_bDoSwarmBehavior = false;
}


void CASW_Buzzer::StartLoitering( const Vector &vecLoiterPosition )
{
	//Msg("Start Loitering\n");

	m_vTargetBanking = vec3_origin;
	m_vecLoiterPosition = GetAbsOrigin();
	m_vForceVelocity = vec3_origin;
	SetCurrentVelocity( vec3_origin );
}

// buzzers have better acceleration on harder difficulty levels
float CASW_Buzzer::GetMaxEnginePower()
{	
	if (m_bElectroStunned)
		return 0.5f;

	if (ASWGameRules())
	{
		int nSkillLevel = ASWGameRules()->GetSkillLevel();
		nSkillLevel = clamp<int>( nSkillLevel, 1, 4 );
		return nSkillLevel + 0.1f;
	}
	return 2.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hostile - 
//-----------------------------------------------------------------------------
void CASW_Buzzer::ShowHostile( bool hostile /*= true*/)
{
	if ( m_bShowingHostile == hostile )
		return;

	m_bShowingHostile = hostile;

	if ( hostile )
	{
		EmitSound( "ASW_Buzzer.Attack" );
	}
	else
	{
		//EmitSound( "NPC_Manhack.ChargeEnd" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Buzzer::StartBurst( const Vector &vecDirection )
{
	if ( m_flBurstDuration > gpGlobals->curtime )
		return;

	ShowHostile();

	// Don't burst attack again for a couple seconds
	m_flNextBurstTime = gpGlobals->curtime + 2.0;
	m_flBurstDuration = gpGlobals->curtime + 1.0;
	
	// Save off where we were going towards and for how long
	m_vecBurstDirection = vecDirection;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Buzzer::StopBurst( bool bInterruptSchedule /*= false*/ )
{
	if ( m_flBurstDuration < gpGlobals->curtime )
		return;

	ShowHostile( false );

	// Stop our burst timers
	m_flNextBurstTime = gpGlobals->curtime + 2.0f; //FIXME: Skill level based
	m_flBurstDuration = gpGlobals->curtime - 0.1f;

	if ( bInterruptSchedule )
	{
		// We need to rethink our current schedule
		ClearSchedule( "Stopping burst" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CASW_Buzzer::CreateVPhysics( void )
{
	return BaseClass::CreateVPhysics();
}


void CASW_Buzzer::SetSpawner(CASW_Base_Spawner* spawner)
{
	m_hSpawner = spawner;
}

// Updates our memory about the enemies we Swarm Sensed
// todo: add various swarm sense conditions?
void CASW_Buzzer::OnSwarmSensed(int iDistance)
{
	// DON'T let visibility information from last frame sit around!
	//static int conditionsToClear[] =
	//{
		//COND_SEE_HATE,
		//COND_SEE_DISLIKE,
		//COND_SEE_ENEMY,
		//COND_SEE_FEAR,
		//COND_SEE_NEMESIS,
		//COND_SEE_PLAYER,
	//};

	//ClearConditions( conditionsToClear, ARRAYSIZE( conditionsToClear ) );

	AISightIter_t iter;
	CBaseEntity *pSenseEnt;

	pSenseEnt = GetASWSenses()->GetFirstSwarmSenseEntity( &iter );

	while( pSenseEnt )
	{
		if ( pSenseEnt->IsPlayer() )
		{
			// if we see a client, remember that (mostly for scripted AI)
			//SetCondition(COND_SEE_PLAYER);
		}

		Disposition_t relation = IRelationType( pSenseEnt );

		// the looker will want to consider this entity
		// don't check anything else about an entity that can't be seen, or an entity that you don't care about.
		if ( relation != D_NU )
		{
			if ( pSenseEnt == GetEnemy() )
			{
				// we know this ent is visible, so if it also happens to be our enemy, store that now.
				//SetCondition(COND_SEE_ENEMY);
			}

			// don't add the Enemy's relationship to the conditions. We only want to worry about conditions when
			// we see npcs other than the Enemy.
			switch ( relation )
			{
			case D_HT:
				{
					int priority = IRelationPriority( pSenseEnt );
					if (priority < 0)
					{
						//SetCondition(COND_SEE_DISLIKE);
					}
					else if (priority > 10)
					{
						//SetCondition(COND_SEE_NEMESIS);
					}
					else
					{
						//SetCondition(COND_SEE_HATE);
					}
					UpdateEnemyMemory(pSenseEnt,pSenseEnt->GetAbsOrigin());
					break;

				}
			case D_FR:
				UpdateEnemyMemory(pSenseEnt,pSenseEnt->GetAbsOrigin());
				//SetCondition(COND_SEE_FEAR);
				break;
			case D_LI:
			case D_NU:
				break;
			default:
				DevWarning( 2, "%s can't assess %s\n", GetClassname(), pSenseEnt->GetClassname() );
				break;
			}
		}

		pSenseEnt = GetASWSenses()->GetNextSwarmSenseEntity( &iter );
	}
}

// create our custom senses class
CAI_Senses* CASW_Buzzer::CreateSenses()
{
	CAI_Senses *pSenses = new CASW_AI_Senses;
	pSenses->SetOuter( this );
	return pSenses;
}

CASW_AI_Senses* CASW_Buzzer::GetASWSenses()
{
	return dynamic_cast<CASW_AI_Senses*>(GetSenses());
}

// set orders for our alien
//   select schedule should activate the appropriate orders
void CASW_Buzzer::SetAlienOrders(AlienOrder_t Orders, Vector vecOrderSpot, CBaseEntity* pOrderObject)
{
	m_AlienOrders = Orders;
	m_vecAlienOrderSpot = vecOrderSpot;	// unused currently
	m_AlienOrderObject = pOrderObject;

	if (Orders == AOT_None)
	{
		ClearAlienOrders();
		return;
	}

	ForceDecisionThink();	// todo: stagger the decision time if at start of mission?

	//Msg("Buzzer recieved orders\n");
}

AlienOrder_t CASW_Buzzer::GetAlienOrders()
{
	return m_AlienOrders;
}

void CASW_Buzzer::ClearAlienOrders()
{
	//Msg("Buzzer orders cleared\n");
	m_AlienOrders = AOT_None;
	m_vecAlienOrderSpot = vec3_origin;
	m_AlienOrderObject = NULL;
	m_bIgnoreMarines = false;
	m_bFailedMoveTo = false;
}

void CASW_Buzzer::GatherConditions()
{
	BaseClass::GatherConditions();

	if( IsLoitering() && GetEnemy() )
	{
		StopLoitering();
	}

	if (m_bIgnoreMarines)	// todo: 'proper' way to ignore conditions?  (using SetIgnoreConditions on receiving orders doesn't work)
	{
		ClearCondition(COND_CAN_MELEE_ATTACK1);
		ClearCondition(COND_CAN_MELEE_ATTACK2);
		ClearCondition(COND_CAN_RANGE_ATTACK1);
		ClearCondition(COND_CAN_RANGE_ATTACK2);
		ClearCondition(COND_ENEMY_DEAD);
		ClearCondition(COND_HEAR_BULLET_IMPACT);
		ClearCondition(COND_HEAR_COMBAT);
		ClearCondition(COND_HEAR_DANGER);
		ClearCondition(COND_HEAR_PHYSICS_DANGER);
		ClearCondition(COND_NEW_ENEMY);
		ClearCondition(COND_PROVOKED);
		ClearCondition(COND_SEE_ENEMY);
		ClearCondition(COND_SEE_FEAR);
		ClearCondition(COND_SMELL);
		ClearCondition(COND_HEAVY_DAMAGE);
		ClearCondition(COND_LIGHT_DAMAGE);
		ClearCondition(COND_RECEIVED_ORDERS);
	}

	if (HasCondition(COND_NEW_ENEMY)
		&& m_AlienOrders != AOT_MoveToIgnoringMarines)	// if we're not ignoring marines, finish our orders once we spot an enemy
		ClearAlienOrders();
}

// if we arrive at our destination, clear our orders
void CASW_Buzzer::OnMovementComplete()
{
	bool bClearOrders = true;
	if (m_AlienOrders != AOT_None)
	{
		//Msg("Buzzer orders complete\n");
		if (m_AlienOrders == AOT_SpreadThenHibernate && m_bFailedMoveTo)
		{
			// should try to repath?
			// or go to a schedule where we wait X seconds, then try to repath?
		}
		// check if we finished trying to chase a marine, but don't have an enemy yet
		//		this means the marine probably moved somewhere else and we need to chase after him again
		if (m_AlienOrders == AOT_MoveToNearestMarine && !GetEnemy())
		{
			float marine_distance;
			CBaseEntity *pMarine = UTIL_ASW_NearestMarine(GetAbsOrigin(), marine_distance);
			if (pMarine)
			{
				// don't clear orders, this'll make the alien's selectschedule do the marine chase again
				bClearOrders = false;
			}
		}
	}

	if (bClearOrders)
		ClearAlienOrders();
	

	BaseClass::OnMovementComplete();
}

void CASW_Buzzer::IgnoreMarines(bool bIgnoreMarines)
{
	static int g_GeneralConditions[] = 
	{
		COND_CAN_MELEE_ATTACK1,
		COND_CAN_MELEE_ATTACK2,
		COND_CAN_RANGE_ATTACK1,
		COND_CAN_RANGE_ATTACK2,
		COND_ENEMY_DEAD,
		COND_HEAR_BULLET_IMPACT,
		COND_HEAR_COMBAT,
		COND_HEAR_DANGER,
		COND_HEAR_PHYSICS_DANGER,
		COND_NEW_ENEMY,
		COND_PROVOKED,
		COND_SEE_ENEMY,
		COND_SEE_FEAR,
		COND_SMELL,
	};

	static int g_DamageConditions[] = 
	{
		COND_HEAVY_DAMAGE,
		COND_LIGHT_DAMAGE,
		COND_RECEIVED_ORDERS,
	};

	ClearIgnoreConditions( g_GeneralConditions, ARRAYSIZE(g_GeneralConditions) );
	ClearIgnoreConditions( g_DamageConditions, ARRAYSIZE(g_DamageConditions) );

	if (bIgnoreMarines)
	{
		SetIgnoreConditions( g_GeneralConditions, ARRAYSIZE(g_GeneralConditions) );
		SetIgnoreConditions( g_DamageConditions, ARRAYSIZE(g_DamageConditions) );	
		m_bIgnoreMarines = true;
	}
}

int CASW_Buzzer::SelectAlienOrdersSchedule()
{
	int schedule = 0;
	// check if we have any alien orders to follow
	switch (m_AlienOrders)
	{
	case AOT_MoveToIgnoringMarines:
		{
			IgnoreMarines(true);
			schedule = SCHED_ASW_BUZZER_ORDER_MOVE;			
		}
		break;
	case AOT_SpreadThenHibernate:
	case AOT_MoveTo:
		{
			IgnoreMarines(false);
			schedule = SCHED_ASW_BUZZER_ORDER_MOVE;			
		}
		break;
	case AOT_MoveToNearestMarine:
		{
			float marine_distance;
			// refresh our order target, in case the one passed in by the orders is no longer the nearest
			m_AlienOrderObject = UTIL_ASW_NearestMarine( GetAbsOrigin(), marine_distance );
			IgnoreMarines(false);
			schedule = SCHED_ASW_BUZZER_ORDER_MOVE;			
		}
		break;
	case AOT_None:
	default:
		{
			// nothing
		}
	}
	return schedule;
}

int CASW_Buzzer::SelectSchedule( void )
{
	// see if our alien orders want to schedule something
	int order_schedule = SelectAlienOrdersSchedule();
	if (order_schedule > 0)
	{
		//Msg("SelectSchedule picked alien orders\n");
		return order_schedule;
	}

	return BaseClass::SelectSchedule();
}

void CASW_Buzzer::Ignite( float flFlameLifetime, bool bNPCOnly, float flSize, bool bCalledByLevelDesigner )
{
	return;		// use ASW_Ignite instead
}
void CASW_Buzzer::ASW_Ignite( float flFlameLifetime, float flSize, CBaseEntity *pAttacker, CBaseEntity *pDamagingWeapon /*= NULL */ )
{
	if (AllowedToIgnite())
	{
		if( IsOnFire() )
			return;

		AddFlag( FL_ONFIRE );
		m_bOnFire = true;
		if (ASWBurning())
			ASWBurning()->BurnEntity(this, pAttacker, flFlameLifetime, 0.4f, 5.0f * 0.4f, pDamagingWeapon );	// 5 dps, applied every 0.4 seconds

		m_OnIgnite.FireOutput( this, this );

		RemoveSpawnFlags( SF_NPC_GAG );
		
		MoanSound( envBuzzerMoanIgnited, ARRAYSIZE( envBuzzerMoanIgnited ) );

		if( m_pMoanSound )
		{
			CSoundEnvelopeController::GetController().SoundChangePitch( m_pMoanSound, 120, 1.0 );
			CSoundEnvelopeController::GetController().SoundChangeVolume( m_pMoanSound, 1, 1.0 );
		}
	}
}

void CASW_Buzzer::Extinguish()
{
	m_bOnFire = false;
	if( m_pMoanSound )
	{
		CSoundEnvelopeController::GetController().SoundChangeVolume( m_pMoanSound, 0, 2.0 );
		CSoundEnvelopeController::GetController().SoundChangePitch( m_pMoanSound, 100, 2.0 );		
	}

	if (ASWBurning())
		ASWBurning()->Extinguish(this);
	RemoveFlag( FL_ONFIRE );
}

void CASW_Buzzer::MoanSound( envelopePoint_t *pEnvelope, int iEnvelopeSize )
{
	if( HasSpawnFlags( SF_NPC_GAG ) )
	{
		// Not yet!
		return;
	}

	if( !m_pMoanSound )
	{
		// Don't set this up until the code calls for it.
		const char *pszSound = "ASW_Buzzer.OnFire"; //GetMoanSound( m_iMoanSound );
		m_flMoanPitch = random->RandomInt( 95, 105 );

		CPASAttenuationFilter filter( this );
		m_pMoanSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), CHAN_STATIC, pszSound, ATTN_NORM );

		CSoundEnvelopeController::GetController().Play( m_pMoanSound, 1.0, m_flMoanPitch );
	}

	envDefaultBuzzerMoanVolumeFast[ 1 ].durationMin = 0.1f;
	envDefaultBuzzerMoanVolumeFast[ 1 ].durationMax = 0.4f;

	if( random->RandomInt( 1, 2 ) == 1 )
	{
		IdleSound();
	}

	float duration = CSoundEnvelopeController::GetController().SoundPlayEnvelope( m_pMoanSound, SOUNDCTRL_CHANGE_VOLUME, pEnvelope, iEnvelopeSize );

	//float flPitch = random->RandomInt( m_flMoanPitch + 0, m_flMoanPitch  );
	float flPitch = m_flMoanPitch;
	CSoundEnvelopeController::GetController().SoundChangePitch( m_pMoanSound, flPitch, 0.3 );

	m_flNextMoanSound = gpGlobals->curtime + duration + 9999;
}


void CASW_Buzzer::SetHealthByDifficultyLevel()
{	
	SetHealth(ASWGameRules()->ModifyAlienHealthBySkillLevel(sk_asw_buzzer_health.GetFloat()));		
}

void CASW_Buzzer::ElectroStun( float flStunTime )
{
	if (m_flElectroStunSlowMoveTime < gpGlobals->curtime + flStunTime)
		m_flElectroStunSlowMoveTime = gpGlobals->curtime + flStunTime;

	m_bElectroStunned = true;

	if ( ASWGameResource() )
	{
		ASWGameResource()->m_iElectroStunnedAliens++;
	}

	// can't jump after being elecrostunned
	CapabilitiesRemove( bits_CAP_MOVE_JUMP );
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------
AI_BEGIN_CUSTOM_NPC( npc_buzzer, CASW_Buzzer )

	DECLARE_TASK( TASK_ASW_BUZZER_HOVER );
	DECLARE_TASK( TASK_ASW_BUZZER_FIND_SQUAD_CENTER );
	DECLARE_TASK( TASK_ASW_BUZZER_FIND_SQUAD_MEMBER );
	DECLARE_TASK( TASK_ASW_BUZZER_MOVEAT_SAVEPOSITION );
	DECLARE_TASK(TASK_ASW_BUZZER_BUILD_PATH_TO_ORDER);

	DECLARE_CONDITION( COND_ASW_BUZZER_START_ATTACK );

//=========================================================
// > SCHED_ASW_BUZZER_ATTACK_HOVER
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_ASW_BUZZER_ATTACK_HOVER,

	"	Tasks"
	"		TASK_SET_ACTIVITY		ACTIVITY:ACT_FLY"
	"		TASK_ASW_BUZZER_HOVER		0"
	"	"
	"	Interrupts"
	"		COND_TOO_FAR_TO_ATTACK"
	"		COND_TOO_CLOSE_TO_ATTACK"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_ENEMY_OCCLUDED"
);

//=========================================================
// > SCHED_ASW_BUZZER_REGROUP
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_ASW_BUZZER_REGROUP,

	"	Tasks"
	"		TASK_STOP_MOVING							0"
	"		TASK_SET_TOLERANCE_DISTANCE					24"
	"		TASK_STORE_ENEMY_POSITION_IN_SAVEPOSITION	0"
	"		TASK_FIND_BACKAWAY_FROM_SAVEPOSITION		0"
	"		TASK_RUN_PATH								0"
	"		TASK_WAIT_FOR_MOVEMENT						0"
	"	"
	"	Interrupts"
	"		COND_ASW_BUZZER_START_ATTACK"
	"		COND_NEW_ENEMY"
	"		COND_CAN_MELEE_ATTACK1"
);



//=========================================================
// > SCHED_ASW_BUZZER_SWARM_IDLE
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_ASW_BUZZER_SWARM_IDLE,

	"	Tasks"
	"		TASK_STOP_MOVING							0"
	"		TASK_SET_FAIL_SCHEDULE						SCHEDULE:SCHED_ASW_BUZZER_SWARM_FAILURE"
	"		TASK_ASW_BUZZER_FIND_SQUAD_CENTER				0"
	"		TASK_ASW_BUZZER_MOVEAT_SAVEPOSITION			5"
	"	"
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_SEE_ENEMY"
	"		COND_SEE_FEAR"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_SMELL"
	"		COND_PROVOKED"
	"		COND_GIVE_WAY"
	"		COND_HEAR_PLAYER"
	"		COND_HEAR_DANGER"
	"		COND_HEAR_COMBAT"
	"		COND_HEAR_BULLET_IMPACT"
);


DEFINE_SCHEDULE
(
	SCHED_ASW_BUZZER_SWARM,

	"	Tasks"
	"		TASK_STOP_MOVING							0"
	"		TASK_SET_FAIL_SCHEDULE						SCHEDULE:SCHED_ASW_BUZZER_SWARM_FAILURE"
	"		TASK_ASW_BUZZER_FIND_SQUAD_CENTER				0"
	"		TASK_ASW_BUZZER_MOVEAT_SAVEPOSITION			1"
	"	"
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_CAN_MELEE_ATTACK1"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
);

DEFINE_SCHEDULE
(
	SCHED_ASW_BUZZER_SWARM_FAILURE,

	"	Tasks"
	"		TASK_STOP_MOVING							0"
	"		TASK_WAIT									2"
	"		TASK_WAIT_RANDOM							2"
	"		TASK_ASW_BUZZER_FIND_SQUAD_MEMBER				0"
	"		TASK_GET_PATH_TO_SAVEPOSITION				0"
	"		TASK_WAIT_FOR_MOVEMENT						0"
	"	"
	"	Interrupts"
	"		COND_SEE_ENEMY"
	"		COND_NEW_ENEMY"
);

DEFINE_SCHEDULE
(
	SCHED_ASW_BUZZER_ORDER_MOVE,

	"	Tasks"
	"		TASK_ASW_BUZZER_BUILD_PATH_TO_ORDER	0"
	"		TASK_WALK_PATH			9999"
	"		TASK_WAIT_FOR_MOVEMENT	0"		// 0 is spread if fail to build path
	"		TASK_WAIT_PVS			0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_SEE_ENEMY"	// in deference to scripted schedule where the enemy was slammed, thus no COND_NEW_ENEMY
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
	"		COND_SMELL"
	"		COND_PROVOKED"
	"		COND_HEAR_COMBAT"
	"		COND_HEAR_BULLET_IMPACT"
)

AI_END_CUSTOM_NPC()


