// Our Swarm Parasite - jumps and infests people

#include "cbase.h"
#include "asw_parasite.h"
#include "asw_marine.h"
#include "te_effect_dispatch.h"
#include "npc_bullseye.h"
#include "npcevent.h"
#include "asw_marine.h"
#include "asw_marine_speech.h"
#include "asw_util_shared.h"
#include "asw_spawner.h"
#include "asw_gamerules.h"
#include "asw_colonist.h"
#include "soundenvelope.h"
#include "asw_player.h"
#include "asw_achievements.h"
#include "asw_fx_shared.h"
#include "asw_marine_resource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	SWARM_PARASITE_MODEL	 "models/aliens/parasite/parasite.mdl"

const int ASW_PARASITE_MIN_JUMP_DIST = 48;
const int ASW_PARASITE_MAX_JUMP_DIST = 170;

#define PARASITE_IGNORE_WORLD_COLLISION_TIME 0.5

ConVar asw_parasite_defanged_damage( "asw_parasite_defanged_damage", "15.0", FCVAR_CHEAT, "Damage per hit from defanged parasites");
ConVar asw_parasite_speedboost( "asw_parasite_speedboost", "1.0", FCVAR_CHEAT, "boost speed for the parasites" );
ConVar asw_infest_angle("asw_infest_angle", "0", 0, "Angle adjustment for parasite infestation attachment");
ConVar asw_infest_pitch("asw_infest_pitch", "-45", 0, "Angle adjustment for parasite infestation attachment");
ConVar asw_parasite_inside("asw_parasite_inside", "0", FCVAR_NONE, "If set, parasites will burrow into their victims rather than staying attached");
extern ConVar asw_debug_alien_damage;
extern ConVar sv_gravity;

int ACT_ASW_EGG_IDLE;

float CASW_Parasite::s_fNextSpottedChatterTime = 0;
float CASW_Parasite::s_fLastHarvesiteAttackSound = 0;
static const char *s_pParasiteAnimThink = "ParasiteAnimThink";

#define ASW_HARVESITE_ATTACK_SOUND_INTERVAL 1.5f

CASW_Parasite::CASW_Parasite( void )// : CASW_Alien()
{
	m_bMidJump = false;
	m_bCommittedToJump = false;
	m_hEgg = NULL;
	m_hMother = NULL;
	m_pszAlienModelName = SWARM_PARASITE_MODEL;
	m_nAlienCollisionGroup = ASW_COLLISION_GROUP_ALIEN;
	m_bNeverRagdoll = true;
}

LINK_ENTITY_TO_CLASS( asw_parasite, CASW_Parasite );
LINK_ENTITY_TO_CLASS( asw_parasite_defanged, CASW_Parasite );

IMPLEMENT_SERVERCLASS_ST( CASW_Parasite, DT_ASW_Parasite )
	SendPropBool(SENDINFO(m_bStartIdleSound)),
	SendPropBool(SENDINFO(m_bDoEggIdle)),
	SendPropBool(SENDINFO(m_bInfesting)),
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Parasite )
	DEFINE_FIELD( m_bCommittedToJump, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bMidJump, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vecCommittedJumpPos, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_flNextNPCThink, FIELD_TIME ),
	DEFINE_FIELD( m_flIgnoreWorldCollisionTime, FIELD_TIME ),
	DEFINE_FIELD( m_bStartIdleSound, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bInfesting, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hEgg, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bJumpFromEgg, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flEggJumpDistance, FIELD_FLOAT ),
	DEFINE_FIELD( m_bDefanged, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fSuicideTime, FIELD_FLOAT ),
	DEFINE_THINKFUNC( LeapThink ),
	DEFINE_THINKFUNC( InfestThink ),
	DEFINE_ENTITYFUNC( LeapTouch ),
	DEFINE_ENTITYFUNC( NormalTouch ),
END_DATADESC()

enum
{
	SCHED_PARASITE_RANGE_ATTACK1 = LAST_ASW_ALIEN_SHARED_SCHEDULE,
	SCHED_PARASITE_JUMP_FROM_EGG = LAST_ASW_ALIEN_SHARED_SCHEDULE+1,
};

enum
{
	TASK_PARASITE_JUMP_FROM_EGG = LAST_ASW_ALIEN_SHARED_TASK,
};

int AE_PARASITE_INFEST_SPURT;
int AE_PARASITE_INFEST;

int AE_HEADCRAB_JUMPATTACK;

void CASW_Parasite::Spawn( void )
{	
	SetHullType(HULL_TINY);

	BaseClass::Spawn();

	SetModel( SWARM_PARASITE_MODEL);

	if (FClassnameIs(this, "asw_parasite_defanged"))
	{
		m_bDefanged = true;
		m_iHealth	= ASWGameRules()->ModifyAlienHealthBySkillLevel(10);
		SetBodygroup( 0, 1 );
		m_fSuicideTime = gpGlobals->curtime + 60;
	}
	else
	{
		m_bDefanged = false;
		m_iHealth	= ASWGameRules()->ModifyAlienHealthBySkillLevel(25);
		SetBodygroup( 0, 0 );
		m_fSuicideTime = 0;
	}

	SetMoveType( MOVETYPE_STEP );
	SetHullType(HULL_TINY);
	SetCollisionGroup( ASW_COLLISION_GROUP_PARASITE );
	SetViewOffset( Vector(6, 0, 11) ) ;		// Position of the eyes relative to NPC's origin.

	m_NPCState	= NPC_STATE_NONE;

	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_RANGE_ATTACK1 );

	m_bInfesting = false;
	
}

void CASW_Parasite::Event_Killed( const CTakeDamageInfo &info )
{
	if (GetMother())
		GetMother()->ChildAlienKilled(this);

	BaseClass::Event_Killed( info );

	if ( !m_bDefanged && !m_bDoEggIdle.Get() && ( info.GetDamageType() & DMG_CLUB ) && info.GetAttacker() && info.GetAttacker()->Classify() == CLASS_ASW_MARINE )
	{
		CASW_Marine *pMarine = static_cast<CASW_Marine*>( info.GetAttacker() );
		if ( pMarine && pMarine->IsInhabited() && pMarine->GetCommander() )
		{
			pMarine->GetCommander()->AwardAchievement( ACHIEVEMENT_ASW_MELEE_PARASITE );
			if ( pMarine->GetMarineResource() )
			{
				pMarine->GetMarineResource()->m_bMeleeParasiteKill = true;
			}
		}
	}
}

CASW_Parasite::~CASW_Parasite()
{
	StopLoopingSounds();
	if (GetEgg())
	{
		GetEgg()->ParasiteDied(this);
	}
}

void CASW_Parasite::Precache( void )
{	
	
	PrecacheModel( SWARM_PARASITE_MODEL );

	PrecacheScriptSound("ASW_Parasite.Death");
	PrecacheScriptSound("ASW_Parasite.Attack");
	PrecacheScriptSound("ASW_Parasite.Idle");
	PrecacheScriptSound("ASW_Parasite.Pain");
	PrecacheScriptSound("ASW_Parasite.Attack");

	BaseClass::Precache();
}

void CASW_Parasite::RunAnimation()
{
	StudioFrameAdvance();
	SetContextThink( &CASW_Parasite::RunAnimation, gpGlobals->curtime + 0.1f, s_pParasiteAnimThink );
} 

float CASW_Parasite::GetIdealSpeed() const
{	
	// Hack to get rid of const needed to use the BaseAnimating calls.
	CASW_Parasite *pNPC = const_cast<CASW_Parasite*>( this );
	pNPC->UpdatePlaybackRate();
	return BaseClass::GetIdealSpeed() * m_flPlaybackRate;
}


float CASW_Parasite::GetIdealAccel( ) const
{
	return GetIdealSpeed() * 1.5f;
}

float CASW_Parasite::MaxYawSpeed( void )
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

void CASW_Parasite::AlertSound()
{
	IdleSound();
}

// defanged leaptouch sound
void CASW_Parasite::BiteSound( void )
{
	EmitSound( "ASW_Parasite.Attack" );
}

void CASW_Parasite::PainSound( const CTakeDamageInfo &info )
{
	if (gpGlobals->curtime > m_fNextPainSound)
	{
		if (m_bDefanged)
			EmitSound("ASW_Parasite.Pain");
		else
			EmitSound("ASW_Parasite.Pain");
		m_fNextPainSound = gpGlobals->curtime + 0.5f;
	}
}

void CASW_Parasite::DeathSound( const CTakeDamageInfo &info )
{
	EmitSound( "ASW_Parasite.Death" );
}

void CASW_Parasite::AttackSound()
{
	if (m_bDefanged)
	{
		// since we have a lot of these, force a delay between playing sounds
		if (gpGlobals->curtime > s_fLastHarvesiteAttackSound + ASW_HARVESITE_ATTACK_SOUND_INTERVAL)
		{
			EmitSound("ASW_Parasite.Attack");
			s_fLastHarvesiteAttackSound = gpGlobals->curtime;
		}
	}
	else
		EmitSound("ASW_Parasite.Attack");
}

void CASW_Parasite::IdleSound()
{
	if (!m_bDefanged)	// defanged parasites don't make the idle sounds
		m_bStartIdleSound = true;
}

void CASW_Parasite::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();
	
	if (( m_NPCState == NPC_STATE_COMBAT ) && ( random->RandomFloat( 0, 5 ) < 0.1 ))
	{
		IdleSound();
	}
}

//-----------------------------------------------------------------------------
// Purpose: For innate melee attack
// Input  :
// Output :
//-----------------------------------------------------------------------------
float CASW_Parasite::InnateRange1MinRange( void )
{
	return ASW_PARASITE_MIN_JUMP_DIST;
}

float CASW_Parasite::InnateRange1MaxRange( void )
{
	return ASW_PARASITE_MAX_JUMP_DIST;
}

int CASW_Parasite::RangeAttack1Conditions( float flDot, float flDist )
{
	if ( gpGlobals->curtime < m_flNextAttack )
		return 0;

	if ( ( GetFlags() & FL_ONGROUND ) == false )
		return 0;

	// This code stops lots of headcrabs swarming you and blocking you
	// whilst jumping up and down in your face over and over. It forces
	// them to back up a bit. If this causes problems, consider using it
	// for the fast headcrabs only, rather than just removing it.(sjb)
	if ( flDist < ASW_PARASITE_MIN_JUMP_DIST )
		return COND_TOO_CLOSE_TO_ATTACK;

	if ( flDist > ASW_PARASITE_MAX_JUMP_DIST )
		return COND_TOO_FAR_TO_ATTACK;

	// Make sure the way is clear!
	CBaseEntity *pEnemy = GetEnemy();
	if( pEnemy )
	{
		bool bEnemyIsBullseye = ( dynamic_cast<CNPC_Bullseye *>(pEnemy) != NULL );

		trace_t tr;
		AI_TraceLine( EyePosition(), pEnemy->EyePosition(), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

		if ( tr.m_pEnt != GetEnemy() )
		{
			if ( !bEnemyIsBullseye || tr.m_pEnt != NULL )
				return COND_NONE;
		}

		if( GetEnemy()->EyePosition().z - 36.0f > GetAbsOrigin().z )
		{
			// Only run this test if trying to jump at a player who is higher up than me, else this 
			// code will always prevent a headcrab from jumping down at an enemy, and sometimes prevent it
			// jumping just slightly up at an enemy.
			Vector vStartHullTrace = GetAbsOrigin();
			vStartHullTrace.z += 1.0;

			Vector vEndHullTrace = GetEnemy()->EyePosition() - GetAbsOrigin();
			vEndHullTrace.NormalizeInPlace();
			vEndHullTrace *= 8.0;
			vEndHullTrace += GetAbsOrigin();

			AI_TraceHull( vStartHullTrace, vEndHullTrace,GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, this, GetCollisionGroup(), &tr );

			if ( tr.m_pEnt != NULL && tr.m_pEnt != GetEnemy() )
			{
				return COND_TOO_CLOSE_TO_ATTACK;
			}
		}
	}

	return COND_CAN_RANGE_ATTACK1;
}


void CASW_Parasite::HandleAnimEvent( animevent_t *pEvent )
{
	int nEvent = pEvent->Event();

	if ( nEvent == AE_HEADCRAB_JUMPATTACK )
	{
		// Ignore if we're in mid air
		if ( m_bMidJump )
			return;

		CBaseEntity *pEnemy = GetEnemy();
			
		if ( pEnemy )
		{
			if ( m_bCommittedToJump )
			{
				JumpAttack( false, m_vecCommittedJumpPos );
			}
			else
			{
				// Jump at my enemy's eyes.
				JumpAttack( false, pEnemy->EyePosition() );
			}

			m_bCommittedToJump = false;
			
		}
		else
		{
			// Jump hop, don't care where.
			JumpAttack( true );
		}

		return;
	}
	else if ( nEvent == AE_PARASITE_INFEST_SPURT)
	{
		// spurt some blood from our front claws
		Vector vecBloodPos;
		if( GetAttachment( "leftclaw", vecBloodPos ) )
			UTIL_ASW_BloodDrips( vecBloodPos, Vector(1,0,0), BLOOD_COLOR_RED, 1 );
		if( GetAttachment( "rightclaw", vecBloodPos ) )
			UTIL_ASW_BloodDrips( vecBloodPos, Vector(1,0,0), BLOOD_COLOR_RED, 1 );
		return;
	}
	else if ( nEvent == AE_PARASITE_INFEST)
	{
		// we're done infesting, make ourselves vanish
		FinishedInfesting();
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

bool CASW_Parasite::ShouldGib( const CTakeDamageInfo &info )
{
	return false;	
}

bool CASW_Parasite::CorpseGib( const CTakeDamageInfo &info )
{

	CEffectData	data;
	
	data.m_vOrigin = WorldSpaceCenter();
	data.m_vNormal = data.m_vOrigin - info.GetDamagePosition();
	VectorNormalize( data.m_vNormal );
	
	data.m_flScale = RemapVal( m_iHealth, 0, -500, 1, 3 );
	data.m_flScale = clamp( data.m_flScale, 1, 3 );
	data.m_fFlags = IsOnFire() ? ASW_GIBFLAG_ON_FIRE : 0;

	if (m_bDefanged)
		DispatchEffect( "HarvesiteGib", data );
	else
		DispatchEffect( "ParasiteGib", data );

	return true;
}

void CASW_Parasite::BuildScheduleTestBits( void )
{
	//Don't allow any modifications when scripted
	if ( m_NPCState == NPC_STATE_SCRIPT )
		return;

	//Make sure we interrupt a run schedule if we can jump
	if ( IsCurSchedule(SCHED_CHASE_ENEMY) )
	{
		SetCustomInterruptCondition( COND_ENEMY_UNREACHABLE );
	}

	//Interrupt any schedule unless already fleeing, burrowing, burrowed, or unburrowing.
	if( GetFlags() & FL_ONGROUND )
	{
		if ( GetEnemy() == NULL )
		{
			SetCustomInterruptCondition( COND_HEAR_PHYSICS_DANGER );
		}					
	}
}

void CASW_Parasite::GatherEnemyConditions( CBaseEntity *pEnemy )
{
	// Do the base class
	BaseClass::GatherEnemyConditions( pEnemy );

	// If we're not already too far away, check again
	//TODO: Check to make sure we don't already have a condition set that removes the need for this
	if ( HasCondition( COND_ENEMY_UNREACHABLE ) == false )
	{
		Vector	predPosition;
		UTIL_PredictedPosition( GetEnemy(), 1.0f, &predPosition );

		Vector	predDir = ( predPosition - GetAbsOrigin() );
		float	predLength = VectorNormalize( predDir );

		// See if we'll be outside our effective target range
		if ( predLength > 2000 ) // m_flEludeDistance
		{
			Vector	predVelDir = ( predPosition - GetEnemy()->GetAbsOrigin() );
			float	predSpeed  = VectorNormalize( predVelDir );

			// See if the enemy is moving mostly away from us
			if ( ( predSpeed > 512.0f ) && ( DotProduct( predVelDir, predDir ) > 0.0f ) )
			{
				// Mark the enemy as eluded and burrow away
				ClearEnemyMemory();
				SetEnemy( NULL );
				SetIdealState( NPC_STATE_ALERT );
				SetCondition( COND_ENEMY_UNREACHABLE );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Does a jump attack at the given position.
// Input  : bRandomJump - Just hop in a random direction.
//			vecPos - Position to jump at, ignored if bRandom is set to true.
//			bThrown - 
//-----------------------------------------------------------------------------
void CASW_Parasite::JumpAttack( bool bRandomJump, const Vector &vecPos, bool bThrown )
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
		AngleVectors( GetLocalAngles(), &forward, NULL, &up );
		vecJumpVel = Vector( forward.x, forward.y, up.z ) * 350;
	}

	AttackSound();
	Leap( vecJumpVel );
}

void CASW_Parasite::Leap( const Vector &vecVel )
{
	SetTouch( &CASW_Parasite::LeapTouch );

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
	SetThink( &CASW_Parasite::LeapThink );
	SetNextThink( gpGlobals->curtime );
}

void CASW_Parasite::LeapThink( void )
{
	if (gpGlobals->curtime > m_flNextNPCThink)
	{
		NPCThink();
		m_flNextNPCThink = gpGlobals->curtime + 0.1;
	}

	if( GetFlags() & FL_ONGROUND )
	{
		SetThink( &CASW_Parasite::CallNPCThink );
		SetNextThink( gpGlobals->curtime + 0.1 );
		return;
	}

	SetNextThink( gpGlobals->curtime );
}

static const char *s_pStartInfestContext = "StartInfestContext";

void CASW_Parasite::NormalTouch( CBaseEntity* pOther )
{
	if ( !m_bDefanged && !m_hPrepareToInfest.Get() && pOther && ( pOther->Classify() == CLASS_ASW_COLONIST || pOther->Classify() == CLASS_ASW_MARINE ) )
	{
		SetCollisionGroup( ASW_COLLISION_GROUP_BUZZER );		// stop collisions with the marine/colonist

		if ( !CheckInfestTarget( pOther ) )
		{
			// Hop away in a random direction!
			JumpAttack( true );
			return;
		}
		// infest after a delay equal to the default interpolation time for aliens.  This stops the parasite teleporting to its target immediately.
		m_hPrepareToInfest = pOther;
		SetContextThink( &CASW_Parasite::StartInfestation, gpGlobals->curtime + 0.2f, s_pStartInfestContext );
	}
}

bool CASW_Parasite::CheckInfestTarget( CBaseEntity *pOther )
{
	CASW_Marine* pMarine = CASW_Marine::AsMarine( pOther );
	if ( pOther )
	{
		// if marine has electrified armour on, that protects him from infestation
		if ( pMarine->IsElectrifiedArmorActive() )
		{
			CTakeDamageInfo info( NULL, NULL, Vector(0,0,0), GetAbsOrigin(), GetHealth() * 2, DMG_SHOCK );
			TakeDamage(info);
			return false;
		}

		if ( pMarine->m_takedamage == DAMAGE_NO )
		{
			// We're in the death cam... no fair infesting there
			return false;
		}

		if ( IsOnFire() )
		{
			// don't actually infest if we're on fire, since we'll die very shortly
			return false;
		}

		if ( pMarine->m_iJumpJetting.Get() != 0 )
		{
			// marine is in the middle of a jump jet or blink, don't infest him
			return false;
		}
		return true;
	}
	else if ( pOther->Classify() == CLASS_ASW_COLONIST )
	{
		return !IsOnFire();
	}
	return false;
}

void CASW_Parasite::StartInfestation()
{
	CASW_Marine* pMarine = CASW_Marine::AsMarine( m_hPrepareToInfest.Get() );
	if ( pMarine )
	{
		InfestMarine( pMarine );
	}
	else
	{
		CASW_Colonist *pColonist = dynamic_cast<CASW_Colonist*>( m_hPrepareToInfest.Get() );
		if ( pColonist )
		{
			InfestColonist( pColonist );
		}
	}
}

void CASW_Parasite::InfestThink( void )
{	
	SetNextThink( gpGlobals->curtime + 0.1f );

	if ( !GetModelPtr() )
		return;
	
	StudioFrameAdvance();

	DispatchAnimEvents( this );

	CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(GetParent());
	if ( !pMarine || !pMarine->IsInfested() || pMarine->IsEffectActive( EF_NODRAW ) )
	{
		FinishedInfesting();
	}
}

void CASW_Parasite::InfestMarine(CASW_Marine* pMarine)
{
	if ( !pMarine )	
		return;

	pMarine->BecomeInfested(this);

	// attach
	int attachment = pMarine->LookupAttachment( "chest" );
	if ( attachment )
	{
		SetSolid( SOLID_NONE );
		SetMoveType( MOVETYPE_NONE );
		QAngle current(0,0,0);

		Vector diff = pMarine->GetAbsOrigin() - GetAbsOrigin();
		float angle = UTIL_VecToYaw(diff);
		angle -= pMarine->GetAbsAngles()[YAW];	// get the diff between our angle from the marine and the marine's facing;
		
		current = GetAbsAngles();

		Vector vAttachmentPos;
		pMarine->GetAttachment( attachment, vAttachmentPos );

		// Make sure it's near the chest attachement before parenting
		Teleport( &vAttachmentPos, &vec3_angle, &vec3_origin );
		
		SetParent( pMarine, attachment );

		float flRaise = RandomFloat( 15.0f, 18.0f );
		float flForward = RandomFloat( -3.0f, 0.0f );
		float flSide = RandomFloat( 1.75f, 3.0f ) * ( RandomInt( 0, 1 ) == 0 ? 1.0f : -1.0f );

		if ( asw_debug_alien_damage.GetBool() )
		{
			Msg( "INFEST: flRaise = %f flForward = %f flSide = %f yaw = %f\n", flRaise, flForward, flSide, angle + asw_infest_angle.GetFloat() );
		}
		SetLocalOrigin( Vector( flForward, flSide, flRaise ) );
		SetLocalAngles( QAngle( asw_infest_pitch.GetFloat(), angle + asw_infest_angle.GetFloat(), 0 ) );
		// play our infesting anim
		if ( asw_parasite_inside.GetBool() )
		{
			SetActivity(ACT_RANGE_ATTACK2);
		}
		else
		{
			int iInfestAttack = LookupSequence("Infest_attack");
			if (GetSequence() != iInfestAttack)
			{
				ResetSequence(iInfestAttack);
			}
		}
		
		AddFlag( FL_NOTARGET );
		SetThink( &CASW_Parasite::InfestThink );
		SetTouch( NULL );
		m_bInfesting = true;		
	}
	else
	{
		FinishedInfesting();
	}		
}

void CASW_Parasite::InfestColonist(CASW_Colonist* pColonist)
{
	if (m_bDefanged || !pColonist)	// no infesting if we've been defanged
		return;

	if (!IsOnFire())	// don't actually infest if we're on fire, since we'll die very shortly
		pColonist->BecomeInfested(this);

	// attach
	int attachment = pColonist->LookupAttachment( "chest" );
	if ( attachment )
	{
		//SetAbsAngles( GetOwnerEntity()->GetAbsAngles() );
		SetSolid( SOLID_NONE );
		SetMoveType( MOVETYPE_NONE );
		QAngle current(0,0,0);

		Vector diff = pColonist->GetAbsOrigin() - GetAbsOrigin();
		float angle = UTIL_VecToYaw(diff);
		angle -= pColonist->GetAbsAngles()[YAW];	// get the diff between our angle from the marine and the marine's facing;
		
		current = GetAbsAngles();
		
		SetParent( pColonist, attachment );
				Vector vecPosition;
		float fRaise = random->RandomFloat(0,20);
		
		SetLocalOrigin( Vector( -fRaise * 0.2f, 0, fRaise ) );
		SetLocalAngles( QAngle( 0, angle + asw_infest_angle.GetFloat(), 0 ) );
		// play our infesting anim
		if ( asw_parasite_inside.GetBool() )
		{
			SetActivity(ACT_RANGE_ATTACK2);
		}
		else
		{
			int iInfestAttack = LookupSequence("Infest_attack");
			if (GetSequence() != iInfestAttack)
			{
				ResetSequence(iInfestAttack);
			}
		}
		// don't do anymore thinking - need to think still to animate?
		AddFlag( FL_NOTARGET );
		SetThink( &CASW_Parasite::InfestThink );
		SetTouch( NULL );
		m_bInfesting = true;		
	}
	else
	{
		FinishedInfesting();
	}		
}

// we're done clawing our way in, remove the AI
void CASW_Parasite::FinishedInfesting()
{
	StopLoopingSounds();

	// notify everything that needs to know about our death
	if (ASWGameRules())
	{
		CTakeDamageInfo info;
		ASWGameRules()->AlienKilled(this, info);
	}	

	if (m_hSpawner.Get())
		m_hSpawner->AlienKilled(this);

	if (GetMother())
		GetMother()->ChildAlienKilled(this);

	UTIL_Remove( this );
	SetThink( NULL ); //We're going away, so don't think anymore.
	SetTouch( NULL );
}

void CASW_Parasite::SetEgg(CASW_Egg* pEgg)
{
	m_hEgg = pEgg;
}

CASW_Egg* CASW_Parasite::GetEgg()
{	
	return dynamic_cast<CASW_Egg*>(m_hEgg.Get());
}	

//-----------------------------------------------------------------------------
// Purpose: LeapTouch - this is the headcrab's touch function when it is in the air.
// Input  : *pOther - 
//-----------------------------------------------------------------------------

void CASW_Parasite::LeapTouch( CBaseEntity *pOther )
{
	m_bMidJump = false;	

	if ( IRelationType( pOther ) == D_HT )
	{
		if (m_bDefanged)
		{
			if ( pOther->m_takedamage != DAMAGE_NO )
			{
				BiteSound();
				TouchDamage( pOther );
				//ClearSchedule( "About to gib self" );
				// gib us
				CTakeDamageInfo info(NULL, NULL, Vector(0,0,0), GetAbsOrigin(), GetHealth() * 2,
						DMG_ACID);
				TakeDamage(info);
				SetSchedule( SCHED_DIE );
				return;
			}
			else
			{
				//ImpactSound();
			}
		}
		// Don't hit if back on ground
		//if ( !( GetFlags() & FL_ONGROUND ) && m_bDefanged)	// if we're defanged, don't infest, just do some combat damage
		//{
	 		
		//}
		//else
		//{
			//ImpactSound();
		//}
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
	}

	// make sure we're solid
	RemoveSolidFlags( FSOLID_NOT_SOLID );
	// Shut off the touch function.
	SetTouch( &CASW_Parasite::NormalTouch );
	SetThink ( &CASW_Parasite::CallNPCThink );

	SetCollisionGroup( ASW_COLLISION_GROUP_PARASITE );

	// if we hit a marine, infest him and go away
	NormalTouch( pOther );
}

int CASW_Parasite::CalcDamageInfo( CTakeDamageInfo *pInfo )
{
	Assert(ASWGameRules());
	pInfo->Set( this, this, asw_parasite_defanged_damage.GetFloat(), DMG_ACID );
	CalculateMeleeDamageForce( pInfo, GetAbsVelocity(), GetAbsOrigin() );
	return pInfo->GetDamage();
}

//-----------------------------------------------------------------------------
// Purpose: Deal the damage from the defanged parasite's touch attack.
//-----------------------------------------------------------------------------
void CASW_Parasite::TouchDamage( CBaseEntity *pOther )
{
	CTakeDamageInfo info;
	CalcDamageInfo( &info );
	int damage = ASWGameRules()->ModifyAlienDamageBySkillLevel(info.GetDamage());
	info.SetDamage(damage);
	pOther->TakeDamage( info  );
	EmitSound("ASWFire.AcidBurn");
	CEffectData	data;			
	data.m_vOrigin = GetAbsOrigin();
	data.m_nOtherEntIndex = pOther->entindex();
	DispatchEffect( "ASWAcidBurn", data );
}

bool CASW_Parasite::HasHeadroom()
{
	trace_t tr;
	UTIL_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, 1 ), MASK_NPCSOLID, this, GetCollisionGroup(), &tr );

#if 0
	if( tr.fraction == 1.0f )
	{
		Msg("Headroom\n");
	}
	else
	{
		Msg("NO Headroom\n");
	}
#endif

	return (tr.fraction == 1.0);
}

void CASW_Parasite::StartTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
		/*
		case TASK_STOP_MOVING:
		{
			if (m_bDoEggIdle)
			{
				SetIdealActivity( (Activity) ACT_ASW_EGG_IDLE );
				TaskComplete();
				return;
			}
			if ( ( GetNavigator()->IsGoalSet() && GetNavigator()->IsGoalActive() ) || GetNavType() == NAV_JUMP )
			{
				DbgNavMsg( this, "Start TASK_STOP_MOVING\n" );
				if ( pTask->flTaskData == 1 )
				{
					DbgNavMsg( this, "Initiating stopping path\n" );
					GetNavigator()->StopMoving( false );
				}
				else
				{
					GetNavigator()->ClearGoal();
				}

				// E3 Hack
				if (LookupPoseParameter( "move_yaw") >= 0)
				{
					SetPoseParameter( "move_yaw", 0 );
				}
			}
			else
			{
				if ( pTask->flTaskData == 1 && GetNavigator()->SetGoalFromStoppingPath() )
				{
					DbgNavMsg( this, "Start TASK_STOP_MOVING\n" );
					DbgNavMsg( this, "Initiating stopping path\n" );
				}
				else
				{
					GetNavigator()->ClearGoal();
					if (m_bDoEggIdle)
						SetIdealActivity( (Activity) ACT_ASW_EGG_IDLE );
					else
						SetIdealActivity( GetStoppedActivity() );

					TaskComplete();
				}
			}
		}
		*/
		case TASK_PARASITE_JUMP_FROM_EGG:
		{
			DoJumpFromEgg();
			break;
		}
		case TASK_RANGE_ATTACK1:
		{
			SetIdealActivity( ACT_RANGE_ATTACK1 );
			break;
		}
		default:
		{
			BaseClass::StartTask( pTask );
		}
	}
}

void CASW_Parasite::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_RANGE_ATTACK1:
		case TASK_RANGE_ATTACK2:
		{
			if ( IsActivityFinished() )
			{
				TaskComplete();
				m_bMidJump = false;
				SetTouch( NULL );
				SetThink( &CASW_Parasite::CallNPCThink );
				SetIdealActivity( ACT_IDLE );
			}
			break;
		}
		case TASK_PARASITE_JUMP_FROM_EGG:
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

void CASW_Parasite::UpdatePlaybackRate()
{
	if ( GetActivity() != ACT_RUN )
	{
		m_flPlaybackRate = 1.0f;
		return;
	}

	float boost = asw_parasite_speedboost.GetFloat();
	switch (ASWGameRules()->GetSkillLevel())
	{
		case 5: boost *= asw_alien_speed_scale_insane.GetFloat(); break;
		case 4: boost *= asw_alien_speed_scale_insane.GetFloat(); break;
		case 3: boost *= asw_alien_speed_scale_hard.GetFloat(); break;
		case 2: boost *= asw_alien_speed_scale_normal.GetFloat(); break;
		default: boost *= asw_alien_speed_scale_easy.GetFloat(); break;
	}
	m_flPlaybackRate = boost;
}

int CASW_Parasite::TranslateSchedule( int scheduleType )
{
	switch( scheduleType )
	{
		case SCHED_RANGE_ATTACK1:
			return SCHED_PARASITE_RANGE_ATTACK1;
	}

	return BaseClass::TranslateSchedule( scheduleType );
}

int CASW_Parasite::SelectSchedule()
{
	if ( m_bJumpFromEgg )
	{
		m_bJumpFromEgg = false;
		return SCHED_PARASITE_JUMP_FROM_EGG;
	}

	return BaseClass::SelectSchedule();
}

void CASW_Parasite::SetJumpFromEgg(bool b, float flJumpDistance)
{
	m_bJumpFromEgg = true;
	m_flEggJumpDistance = flJumpDistance;
}

void CASW_Parasite::DoJumpFromEgg()
{
	SetContextThink( NULL, gpGlobals->curtime, s_pParasiteAnimThink );
	SetParent( NULL );
	SetAbsOrigin( GetAbsOrigin() + Vector( 0, 0, 30 ) );	// TODO: position parasite at where his 'idle in egg' animation has him.  This has to be some distance off the ground, else the jump will immediately end.
	Vector dir = vec3_origin;
	AngleVectors( GetAbsAngles(), &dir );
	//Vector vecJumpPos = GetAbsOrigin()+ Vector(19,0,60)+ dir * m_flEggJumpDistance;
	Vector vecJumpPos = GetAbsOrigin() + dir * m_flEggJumpDistance;

	SetActivity( ACT_RANGE_ATTACK1 );
	StudioFrameAdvanceManual( 0.0 );
	SetParent( NULL );
	RemoveFlag( FL_FLY );
	AddEffects( EF_NOINTERP );
	m_bDoEggIdle = false;

	GetMotor()->SetIdealYaw( GetAbsAngles().y );

	JumpAttack( false, vecJumpPos, false );
}

void CASW_Parasite::IdleInEgg(bool b)
{
	if (b)
	{
		SetActivity((Activity) ACT_ASW_EGG_IDLE);
		SetIdealActivity((Activity) ACT_ASW_EGG_IDLE);
		SetContextThink( &CASW_Parasite::RunAnimation, gpGlobals->curtime + 0.1f, s_pParasiteAnimThink );
	}
	m_bDoEggIdle = b;
}

Activity CASW_Parasite::TranslateActivity( Activity baseAct, Activity *pIdealWeaponActivity )
{
	Activity translated = BaseClass::TranslateActivity(baseAct, pIdealWeaponActivity);
/*
	if (translated == ACT_IDLE && m_bDoEggIdle)
	{
		Msg("Translated idle to egg idle\n");
		return (Activity) ACT_ASW_EGG_IDLE;
	}
	else if (translated == ACT_IDLE)
	{
		Msg("Go an act idle, but not translating it as we're not set to do an egg idle\n");
	}
*/
	return translated;
}

void CASW_Parasite::SetMother(CASW_Alien* spawner)
{
	m_hMother = spawner;
}

CASW_Alien* CASW_Parasite::GetMother()
{
	return dynamic_cast<CASW_Alien*>(m_hMother.Get());
}

int CASW_Parasite::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	int result = 0;

	// scale damage up while in the air
	if (m_bMidJump)
	{
		CTakeDamageInfo newDamage = info;		
		newDamage.ScaleDamage(10.0f);
		result = BaseClass::OnTakeDamage_Alive(newDamage);
	}
	else
	{
		result = BaseClass::OnTakeDamage_Alive(info);
	}

	return result;
}

void CASW_Parasite::UpdateSleepState(bool bInPVS)
{
	if (m_bDoEggIdle)
	{
		int iEggIdle = LookupSequence("Egg_Idle");
		if (GetSequence() != iEggIdle)
		{
			ResetSequence(iEggIdle);
		}
	}
	BaseClass::UpdateSleepState(bInPVS);
}

void CASW_Parasite::SetHealthByDifficultyLevel()
{
	if (FClassnameIs(this, "asw_parasite_defanged"))
	{		
		SetHealth(ASWGameRules()->ModifyAlienHealthBySkillLevel(10));
	}
	else
	{
		SetHealth(ASWGameRules()->ModifyAlienHealthBySkillLevel(30));
	}
}

void CASW_Parasite::NPCThink()
{
	BaseClass::NPCThink();

	if ( GetEfficiency() < AIE_DORMANT && GetSleepState() == AISS_AWAKE 
		&& !m_bDefanged && gpGlobals->curtime > s_fNextSpottedChatterTime && GetEnemy())
	{
		CASW_Marine *pMarine = UTIL_ASW_Marine_Can_Chatter_Spot(this);
		if (pMarine)
		{
			pMarine->GetMarineSpeech()->Chatter(CHATTER_PARASITE);
			s_fNextSpottedChatterTime = gpGlobals->curtime + 30.0f;
		}
		else
			s_fNextSpottedChatterTime = gpGlobals->curtime + 1.0f;
	}
	if (m_bDefanged && m_fSuicideTime < gpGlobals->curtime && GetEnemy() == NULL)
	{
		// suicide!		
		CTakeDamageInfo info(NULL, NULL, Vector(0,0,0), GetAbsOrigin(), GetHealth() * 2,
				DMG_ACID);
		TakeDamage(info);
	}
}

// can't be seen by AI marines when infesting someone
bool CASW_Parasite::CanBeSeenBy( CAI_BaseNPC *pNPC )
{
	return !m_bInfesting;
}

AI_BEGIN_CUSTOM_NPC( asw_parasite, CASW_Parasite )
	DECLARE_ANIMEVENT( AE_HEADCRAB_JUMPATTACK )
	DECLARE_ANIMEVENT( AE_PARASITE_INFEST_SPURT )
	DECLARE_ANIMEVENT( AE_PARASITE_INFEST )
	DECLARE_TASK( TASK_PARASITE_JUMP_FROM_EGG )
	DECLARE_ACTIVITY( ACT_ASW_EGG_IDLE )

	DEFINE_SCHEDULE
	(
		SCHED_PARASITE_RANGE_ATTACK1,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_FACE_ENEMY				0"
		"		TASK_RANGE_ATTACK1			0"
		"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
		"		TASK_FACE_IDEAL				0"
		"		TASK_WAIT_RANDOM			0.5"
		""
		"	Interrupts"
		"		COND_ENEMY_OCCLUDED"
		"		COND_NO_PRIMARY_AMMO"
	)

	DEFINE_SCHEDULE
	(
		SCHED_PARASITE_JUMP_FROM_EGG,

		"	Tasks"
		"		TASK_PARASITE_JUMP_FROM_EGG			0"
		""
		"	Interrupts"
	)
AI_END_CUSTOM_NPC()
