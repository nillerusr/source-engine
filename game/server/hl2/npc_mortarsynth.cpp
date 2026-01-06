//======= Copyright © 2022, Obsidian Conflict Team, All rights reserved. =======//
//
// Purpose: Mortar Synth NPC
//
//==============================================================================//

#include "cbase.h"
#include "npcevent.h"

#include "ai_interactions.h"
#include "ai_hint.h"
#include "ai_moveprobe.h"
#include "ai_squad.h"
#include "ai_motor.h"
#include "beam_shared.h"
#include "globalstate.h"
#include "soundent.h"
#include "gib.h"
#include "IEffects.h"
#include "ai_route.h"
#include "npc_mortarsynth.h"
#include "prop_combine_ball.h"
#include "explode.h"
#include "te_effect_dispatch.h"
#include "particle_parse.h"
#include "grenade_energy.h"
#ifdef EZ2
#include "props.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	sk_mortarsynth_health( "sk_mortarsynth_health","0");
ConVar	sk_mortarsynth_mortar_rate( "sk_mortarsynth_mortar_rate","1.8");
ConVar	sk_mortarsynth_repulse_rate( "sk_mortarsynth_repulse_rate","1.0");
#ifdef EZ2
ConVar	sk_mortarsynth_dmg_melee( "sk_mortarsynth_dmg_melee", "15" );
ConVar	sk_mortarsynth_dmg_repulser( "sk_mortarsynth_dmg_repulser", "10" );
#endif

extern ConVar sk_energy_grenade_radius;

#define MORTARSYNTH_FIRE		( 1 )
#ifdef EZ2
#define MORTARSYNTH_MELEE		( 2 )

#define MORTARSYNTH_MELEE_RANGE		64.0f

#define	MORTARSYNTH_MAX_SPEED		500
#define	MORTARSYNTH_ACCEL			500
#define	MORTARSYNTH_ACCEL_TURN		250

// Think contexts
static const char *MORTARSYNTH_BLEED_THINK = "MortarSynthBleed";
#endif

//-----------------------------------------------------------------------------
// Mortarsynth activities
//-----------------------------------------------------------------------------
Activity ACT_MSYNTH_FLINCH_BACK;
Activity ACT_MSYNTH_FLINCH_FRONT;
Activity ACT_MSYNTH_FLINCH_LEFT;
Activity ACT_MSYNTH_FLINCH_RIGHT;
Activity ACT_MSYNTH_FLINCH_BIG_BACK;
Activity ACT_MSYNTH_FLINCH_BIG_FRONT;
Activity ACT_MSYNTH_FLINCH_BIG_LEFT;
Activity ACT_MSYNTH_FLINCH_BIG_RIGHT;
#ifdef EZ2
Activity ACT_MSYNTH_ALERT;
Activity ACT_MSYNTH_PANIC;
#endif

BEGIN_DATADESC( CNPC_Mortarsynth )

	DEFINE_EMBEDDED( m_KilledInfo ),

	DEFINE_FIELD( m_fNextFlySoundTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flNextRepulse,		FIELD_TIME ),
	DEFINE_FIELD( m_pSmokeTrail,			FIELD_CLASSPTR ),
	DEFINE_FIELD( m_flNextFlinch,			FIELD_TIME ),

#ifdef EZ2
	DEFINE_FIELD( m_pPropulsionTrail, FIELD_EHANDLE ),

	DEFINE_FIELD( m_bIsBleeding,		FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bDidBoostEffect,		FIELD_BOOLEAN ),

	DEFINE_THINKFUNC( BleedThink ),
#endif

END_DATADESC()


LINK_ENTITY_TO_CLASS(npc_mortarsynth, CNPC_Mortarsynth);


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNPC_Mortarsynth::CNPC_Mortarsynth()
{
	m_flAttackNearDist = sk_energy_grenade_radius.GetFloat() + 50;
	m_flAttackFarDist = 500;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::Spawn(void)
{
	Precache();

	BaseClass::Spawn();

	SetModel( STRING( GetModelName() ) );

	SetHullType( HULL_LARGE_CENTERED ); 
	SetHullSizeNormal();

	SetMoveType( MOVETYPE_STEP );
	
#ifndef EZ2
	SetBloodColor( BLOOD_COLOR_YELLOW );
#endif

	m_iHealth = sk_mortarsynth_health.GetFloat();
	m_iMaxHealth = m_iHealth;

	m_flFieldOfView	= VIEW_FIELD_WIDE;

	m_fNextFlySoundTime = 0;
	m_flNextRepulse = 0;

#ifdef EZ2
	CapabilitiesAdd( bits_CAP_INNATE_RANGE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK2 /*| bits_CAP_MOVE_SHOOT*/ );

	m_nPoseFaceVert = LookupPoseParameter( "flex_vert" );
	m_nPoseFaceHoriz = LookupPoseParameter( "flex_horz" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::Activate(void)
{
#ifdef EZ2
	m_nPoseHeadPitch = LookupPoseParameter( "head_pitch" );
	m_nPoseInjured = LookupPoseParameter( "injured" );
	m_nPosePropulsionYaw = LookupPoseParameter( "propulsion_yaw" );
	m_nPosePropulsionPitch = LookupPoseParameter( "propulsion_pitch" );

	m_nAttachmentBleed = LookupAttachment( "bleed" );
	m_nAttachmentPropulsion = LookupAttachment( "propulsion" );
	m_nAttachmentPropulsionL = LookupAttachment( "propulsion_l" );
	m_nAttachmentPropulsionR = LookupAttachment( "propulsion_r" );
	
	if ( m_bIsBleeding )
	{
		StartSmokeTrail();
	}
#endif

	BaseClass::Activate();
}

#ifdef EZ2
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::StartEye( void )
{
	BaseClass::StartEye();

	StartPropulsionTrail();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::KillSprites( float flDelay )
{
	BaseClass::KillSprites( flDelay );

	StopPropulsionTrail( flDelay );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::StartPropulsionTrail( void )
{
	// UNDONE: Particle trail
	//DispatchParticleEffect( "mortarsynth_propulsion_trail", PATTACH_POINT_FOLLOW, this, m_nAttachmentPropulsion );
	
	if ( m_pPropulsionTrail == NULL )
	{
		// Start up the eye trail
		m_pPropulsionTrail = CSpriteTrail::SpriteTrailCreate( "sprites/bluelaser1.vmt", GetLocalOrigin(), false );

		if (m_nAttachmentPropulsion <= 0)
			m_nAttachmentPropulsion = LookupAttachment( "propulsion" );

		m_pPropulsionTrail->SetAttachment( this, m_nAttachmentPropulsion );
		m_pPropulsionTrail->SetTransparency( kRenderTransAdd, 64, 192, 255, 160, kRenderFxNone );
		m_pPropulsionTrail->SetStartWidth( 12.0f );
		m_pPropulsionTrail->SetLifeTime( 0.25f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::StopPropulsionTrail( float flDelay )
{
	if (m_pPropulsionTrail)
	{
		m_pPropulsionTrail->FadeAndDie( flDelay );
		m_pPropulsionTrail = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
QAngle CNPC_Mortarsynth::BodyAngles()
{
	// Account for bone controllers
	QAngle angles = GetAbsAngles();
	angles.y += m_fHeadYaw;
	angles.x += m_fHeadPitch;

	return angles;
}
#endif

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
int CNPC_Mortarsynth::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	if( m_flNextFlinch < gpGlobals->curtime )
	{
		// Fenix: Horrible hack to get proper light and heavy flinches
		Activity aFlinchFront, aFlinchBack, aFlinchLeft, aFlinchRight;

		if ( IsHeavyDamage( info ) )
		{ 
			aFlinchFront = ACT_MSYNTH_FLINCH_BIG_FRONT;
			aFlinchBack = ACT_MSYNTH_FLINCH_BIG_BACK;
			aFlinchLeft = ACT_MSYNTH_FLINCH_BIG_LEFT;
			aFlinchRight = ACT_MSYNTH_FLINCH_BIG_RIGHT;
		}
		else 
		{
			aFlinchFront = ACT_MSYNTH_FLINCH_FRONT;
			aFlinchBack = ACT_MSYNTH_FLINCH_BACK;
			aFlinchLeft = ACT_MSYNTH_FLINCH_LEFT;
			aFlinchRight = ACT_MSYNTH_FLINCH_RIGHT;
		}

		Vector vFacing = BodyDirection2D();
		Vector vDamageDir = ( info.GetDamagePosition() - WorldSpaceCenter() );

		vDamageDir.z = 0.0f;

		VectorNormalize( vDamageDir );

		Vector vDamageRight, vDamageUp;
		VectorVectors( vDamageDir, vDamageRight, vDamageUp );

		float damageDot = DotProduct( vFacing, vDamageDir );
		float damageRightDot = DotProduct( vFacing, vDamageRight );

		// See if it's in front
		if ( damageDot > 0.0f )
		{
			// See if it's right
			if ( damageRightDot > 0.0f )
				SetActivity( ( Activity ) aFlinchRight );
			else
				SetActivity( ( Activity ) aFlinchLeft );
		}
		else
		{
			// Otherwise it's from behind
			if ( damageRightDot < 0.0f )
				SetActivity( ( Activity ) aFlinchBack );
			else
				SetActivity( ( Activity ) aFlinchFront );
		}
	}

	m_flNextFlinch = gpGlobals->curtime + 6;

	int nBase = BaseClass::OnTakeDamage_Alive( info );

#ifdef EZ2
	// Update injured layer when hurt
	if (nBase == 1 && m_nPoseInjured != -1)
	{
		float flRatio = ((float)GetHealth() / (float)GetMaxHealth());
		SetPoseParameter( m_nPoseInjured, flRatio );
	}
#endif

	return nBase;
}

#ifdef EZ2
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::StartSmokeTrail( void )
{
	m_bIsBleeding = true;

	DispatchParticleEffect( "blood_drip_synth_01", PATTACH_POINT_FOLLOW, this, m_nAttachmentBleed );

	// Emit spurts of our blood
	SetContextThink( &CNPC_Mortarsynth::BleedThink, gpGlobals->curtime + 0.1, MORTARSYNTH_BLEED_THINK );
}

//-----------------------------------------------------------------------------
// Our health is low. Show damage effects.
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::BleedThink()
{
	// Spurt blood from random points on the synth's head.
	Vector vecOrigin;
	QAngle angDir;
	GetAttachment( m_nAttachmentBleed, vecOrigin, angDir );
	
	Vector vecDir = RandomVector( -1, 1 );
	VectorNormalize( vecDir );
	VectorAngles( vecDir, Vector( 0, 0, 1 ), angDir );

	vecDir *= 8.0f;
	DispatchParticleEffect( "blood_spurt_synth_01", vecOrigin + vecDir, angDir );

	SetNextThink( gpGlobals->curtime + random->RandomFloat( 0.6, 1.5 ), MORTARSYNTH_BLEED_THINK );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Mortarsynth::IsHeavyDamage( const CTakeDamageInfo &info )
{
	// Struck by blast
	if ( info.GetDamageType() & DMG_BLAST )
	{
		if ( info.GetDamage() > 25.0f )
			return true;
	}

	// Struck by large object
	if ( info.GetDamageType() & DMG_CRUSH )
	{
		IPhysicsObject *pPhysObject = info.GetInflictor()->VPhysicsGetObject();

		if ( ( pPhysObject != NULL ) && ( pPhysObject->GetGameFlags() & FVPHYSICS_WAS_THROWN ) )
		{
			// Always take hits from a combine ball
			if ( UTIL_IsAR2CombineBall( info.GetInflictor() ) )
				return true;

			// If we're under half health, stop being interrupted by heavy damage
			if ( GetHealth() < (GetMaxHealth() * 0.25) )
				return false;

			// Ignore physics damages that don't do much damage
			if ( info.GetDamage() < 20.0f )
				return false;

			return true;
		}

		return false;
	}

	return false;
}
	
//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CNPC_Mortarsynth::Gib( void )
{
	if ( IsMarkedForDeletion() )
		return;

#ifdef EZ2
	breakablepropparams_t params( GetAbsOrigin(), GetAbsAngles(), vec3_origin, RandomAngularImpulse( -1000.0f, 1000.0f ) );
	params.impactEnergyScale = 1.0f;
	params.defCollisionGroup = COLLISION_GROUP_INTERACTIVE;

	params.defBurstScale = 250;
	PropBreakableCreateAll( GetModelIndex(), VPhysicsGetObject(), params, this, -1, true );
#else
	CGib::SpawnSpecificGibs( this, 1, 1000, 800, "models/gibs/mortarsynth_carapace.mdl" );
	CGib::SpawnSpecificGibs( this, 1, 1000, 800, "models/gibs/mortarsynth_core.mdl" );
	CGib::SpawnSpecificGibs( this, 1, 1000, 800, "models/gibs/mortarsynth_engine.mdl" );
	CGib::SpawnSpecificGibs( this, 1, 1000, 800, "models/gibs/mortarsynth_left_arm.mdl" );
	CGib::SpawnSpecificGibs( this, 1, 1000, 800, "models/gibs/mortarsynth_left_pincer.mdl" );
	CGib::SpawnSpecificGibs( this, 1, 1000, 800, "models/gibs/mortarsynth_right_arm.mdl" );
	CGib::SpawnSpecificGibs( this, 1, 1000, 800, "models/gibs/mortarsynth_right_pincer.mdl" );
	CGib::SpawnSpecificGibs( this, 1, 1000, 800, "models/gibs/mortarsynth_tail.mdl" );
#endif

	BaseClass::Gib();
}

//-----------------------------------------------------------------------------
// Purpose: Overriden from npc_basescanner.cpp, to avoid DiveBomb attack 
// Input  : pInflictor - 
//			pAttacker - 
//			flDamage - 
//			bitsDamageType - 
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::Event_Killed( const CTakeDamageInfo &info )
{
	// Copy off the takedamage info that killed me, since we're not going to call
	// up into the base class's Event_Killed() until we gib. (gibbing is ultimate death)
	m_KilledInfo = info;	

	// Interrupt whatever schedule I'm on
	SetCondition( COND_SCHEDULE_DONE );

	StopLoopingSounds();

	Gib();

#ifdef EZ2
	// Stop all our thinks
	SetContextThink( NULL, 0, MORTARSYNTH_BLEED_THINK );

	StopParticleEffects( this );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Type - 
//-----------------------------------------------------------------------------
Activity CNPC_Mortarsynth::NPC_TranslateActivity( Activity eNewActivity )
{
	Activity actBase = BaseClass::NPC_TranslateActivity( eNewActivity );

#ifdef EZ2
	if ( actBase == ACT_IDLE && ( IsCurSchedule( SCHED_TAKE_COVER_FROM_BEST_SOUND ) || ( IsCurSchedule( SCHED_TAKE_COVER_FROM_ENEMY ) && m_bIsBleeding ) ) )
	{
		// Use panic animation when either taking cover from a sound or taking cover from an enemy while hurt
		actBase = ACT_MSYNTH_PANIC;
	}
#endif

	return actBase;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Type - 
//-----------------------------------------------------------------------------
int CNPC_Mortarsynth::TranslateSchedule( int scheduleType ) 
{
	switch ( scheduleType )
	{
		case SCHED_RANGE_ATTACK1:
			return SCHED_MORTARSYNTH_ATTACK;

#ifdef EZ2
		case SCHED_MELEE_ATTACK1:
			return SCHED_MORTARSYNTH_MELEE_ATTACK1;

		case SCHED_TAKE_COVER_FROM_ENEMY:
			//if (!HasCondition( COND_SEE_ENEMY ))
			//	return SCHED_MORTARSYNTH_HOVER;
			return SCHED_MORTARSYNTH_TAKE_COVER_FROM_ENEMY;

		case SCHED_MORTARSYNTH_HOVER:
			{
				if ( GetEnemy() )
				{
					// Make sure we have enough space
					trace_t	tr;
					AI_TraceHull( GetAbsOrigin(), GetAbsOrigin() + Vector(0, 0, MinGroundDist()), GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

					// If we can't hover, take cover
					if (tr.fraction < 1.0f)
					{
						// Rush in if we're too close and haven't fired recently
						if (EnemyDistance( GetEnemy() ) <= m_flAttackNearDist && gpGlobals->curtime > m_flNextAttack)
						{
							// Make sure we're not blocked first
							AI_TraceHull( GetAbsOrigin(), GetEnemy()->GetAbsOrigin(), GetHullMins(), GetHullMaxs(), MASK_NPCSOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
							if (tr.fraction != 1.0)
								return SCHED_MORTARSYNTH_TAKE_COVER_FROM_ENEMY;

							return SCHED_MORTARSYNTH_MOVE_TO_MELEE;
						}

						return SCHED_MORTARSYNTH_TAKE_COVER_FROM_ENEMY;
					}
				}
			}
			break;

		case SCHED_FLEE_FROM_BEST_SOUND:
			return SCHED_MORTARSYNTH_FLEE_FROM_BEST_SOUND;
#else
		case SCHED_MELEE_ATTACK1:
			return SCHED_MORTARSYNTH_REPEL;
#endif
	}
	return BaseClass::TranslateSchedule(scheduleType);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::BuildScheduleTestBits()
{
#ifdef EZ2
	if (IsCurSchedule( SCHED_TAKE_COVER_FROM_ENEMY, false ))
	{
		SetCustomInterruptCondition( COND_CAN_MELEE_ATTACK1 );
	}
#endif

	BaseClass::BuildScheduleTestBits();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::HandleAnimEvent( animevent_t *pEvent )
{
	if( pEvent->event == MORTARSYNTH_FIRE )
	{
		Vector vShootStart;
		QAngle vShootAng;

		GetAttachment( "mortar", vShootStart, vShootAng);

		CGrenadeEnergy::Shoot( this, vShootStart, m_vecTossVelocity, vShootAng );

		EmitSound( "NPC_Mortarsynth.Shoot" );

		return;
	}
#ifdef EZ2
	if ( pEvent->event == MORTARSYNTH_MELEE )
	{
		// Need to trace based on head controllers
		QAngle angAngles = GetAbsAngles();
		//angAngles.x = m_fHeadPitch;
		angAngles.y = m_fHeadYaw;

		Vector forward, up;
		AngleVectors( angAngles, &forward, NULL, &up );
		Vector vStart = GetAbsOrigin();

		float flDist = MORTARSYNTH_MELEE_RANGE;
		float flVerticalOffset = WorldAlignSize().z * 0.5;
		vStart.z += flVerticalOffset;
		Vector mins = -Vector( 32, 32, 34 );
		Vector maxs = Vector( 32, 32, 34 );

		if (m_fHeadPitch > 0)
		{
			mins.z -= m_fHeadPitch;
			flDist -= m_fHeadPitch * 0.5f;
		}
		else if (m_fHeadPitch < 0)
		{
			maxs.z -= m_fHeadPitch;
			flDist += m_fHeadPitch * 0.5f;
		}

		Vector vEnd = vStart + (forward * flDist );

		CBaseEntity *pHurt = CheckTraceHullAttack( vStart, vEnd, mins, maxs, sk_mortarsynth_dmg_melee.GetInt(), DMG_CLUB );
		if ( pHurt )
		{
			if (pHurt->IsPlayer())
			{
				pHurt->ViewPunch( QAngle( -12, -7, 0 ) );
				pHurt->ApplyAbsVelocityImpulse( forward * 100 + up * 50 );
			}

			EmitSound( "NPC_Mortarsynth.Melee" );
		}
		else
		{
			EmitSound( "NPC_Mortarsynth.Melee_Miss" );
		}

		return;
	}
#endif

	BaseClass::HandleAnimEvent( pEvent );
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CNPC_Mortarsynth::StopLoopingSounds( void )
{
	StopSound( "NPC_Mortarsynth.Hover" );
	StopSound( "NPC_Mortarsynth.HoverAlarm" );
	StopSound( "NPC_Mortarsynth.Shoot" );
	StopSound( "NPC_Mortarsynth.EnergyShoot" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::Precache(void)
{	
	if( !GetModelName() )
		SetModelName( MAKE_STRING( "models/mortarsynth_npc.mdl" ) );

	PrecacheModel( STRING( GetModelName() ) );

#ifdef EZ2
	PropBreakablePrecacheAll( GetModelName() );
#else
	PrecacheModel( "models/gibs/mortarsynth_carapace.mdl" );
	PrecacheModel( "models/gibs/mortarsynth_core.mdl" );
	PrecacheModel( "models/gibs/mortarsynth_engine.mdl" );
	PrecacheModel( "models/gibs/mortarsynth_left_arm.mdl" );
	PrecacheModel( "models/gibs/mortarsynth_left_pincer.mdl" );
	PrecacheModel( "models/gibs/mortarsynth_right_arm.mdl" );
	PrecacheModel( "models/gibs/mortarsynth_right_pincer.mdl" );
	PrecacheModel( "models/gibs/mortarsynth_tail.mdl" );
#endif

	PrecacheScriptSound( "NPC_Mortarsynth.Hover" );
	PrecacheScriptSound( "NPC_Mortarsynth.HoverAlarm" );
	PrecacheScriptSound( "NPC_Mortarsynth.EnergyShoot" );
	PrecacheScriptSound( "NPC_Mortarsynth.Shoot" );

#ifdef EZ2
	PrecacheScriptSound( "NPC_MortarSynth.Alert" );
	PrecacheScriptSound( "NPC_MortarSynth.Die" );
	PrecacheScriptSound( "NPC_MortarSynth.Combat" );
	PrecacheScriptSound( "NPC_MortarSynth.Idle" );
	PrecacheScriptSound( "NPC_MortarSynth.Pain" );
	PrecacheScriptSound( "NPC_MortarSynth.Danger" );
	PrecacheScriptSound( "NPC_MortarSynth.Boost" );

	PrecacheParticleSystem( "blood_impact_synth_01" );
	PrecacheParticleSystem( "blood_impact_synth_01_arc_parent" );
	PrecacheParticleSystem( "blood_spurt_synth_01" );
	PrecacheParticleSystem( "blood_drip_synth_01" );
	PrecacheParticleSystem( "mortarsynth_propulsion_trail" );
	PrecacheParticleSystem( "mortarsynth_propulsion_trail_boost" );
#endif

	PrecacheParticleSystem( "weapon_combine_ion_cannon_d" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTask - 
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		// If my enemy has moved significantly, update my path
		case TASK_WAIT_FOR_MOVEMENT:
		{
			CBaseEntity *pEnemy = GetEnemy();
			if ( pEnemy && IsCurSchedule( SCHED_MORTARSYNTH_CHASE_ENEMY ) && GetNavigator()->IsGoalActive() )
			{
				Vector vecEnemyPosition = pEnemy->EyePosition();
				if ( GetNavigator()->GetGoalPos().DistToSqr( vecEnemyPosition ) > 40 * 40 )
					GetNavigator()->UpdateGoalPos( vecEnemyPosition );
			}

			// Fall through
		}

		case TASK_WAIT:
		{
#ifdef EZ2
			if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) && !IsCurSchedule( SCHED_MORTARSYNTH_CHASE_ENEMY ) )
			{
				// Fire!
				m_flNextAttack = gpGlobals->curtime + sk_mortarsynth_mortar_rate.GetFloat();
				SetLastAttackTime( gpGlobals->curtime );
				AddGesture( ACT_GESTURE_RANGE_ATTACK1 );
			}
#endif
		}

		default:
		{
			BaseClass::RunTask(pTask);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gets the appropriate next schedule based on current condition
//			bits.
//-----------------------------------------------------------------------------
int CNPC_Mortarsynth::SelectSchedule(void)
{
	// -------------------------------
	// If I'm in a script sequence
	// -------------------------------
	if ( m_NPCState == NPC_STATE_SCRIPT )
		return( BaseClass::SelectSchedule() );

	// -------------------------------
	// Flinch
	// -------------------------------
	if ( HasCondition( COND_LIGHT_DAMAGE ) || HasCondition( COND_HEAVY_DAMAGE ) )
	{
#ifndef EZ2
		if ( m_NPCState == NPC_STATE_ALERT )
#endif
		{
			if ( m_iHealth < ( 3 * m_iMaxHealth / 4 ))
				return SCHED_TAKE_COVER_FROM_ENEMY;
		}
	}

#ifdef EZ2
	// -------------------------------
	// New enemy
	// -------------------------------
	if ( HasCondition( COND_NEW_ENEMY ) && gpGlobals->curtime - GetLastAttackTime() > 30.0f && gpGlobals->curtime - GetLastDamageTime() > 5.0f )
	{
		// If we haven't attacked or taken any damage recently, play an alert animation
		return SCHED_MORTARSYNTH_ALERT;
	}

	// -------------------------------
	// Hear danger
	// -------------------------------
	if ( HasCondition(COND_HEAR_DANGER) )
	{
		CSound *pSound;
		pSound = GetBestSound();

		Assert( pSound != NULL );
		if ( pSound)
		{
			if (pSound->m_iType & SOUND_DANGER)
			{
				DangerSound();
				return SCHED_TAKE_COVER_FROM_BEST_SOUND;
			}

			if ( !HasCondition( COND_SEE_ENEMY ) && ( pSound->m_iType & (SOUND_PLAYER | SOUND_COMBAT) ) )
			{
				return SCHED_ALERT_FACE_BESTSOUND;
			}
		}
	}
#endif

	// -------------------------------
	// If I'm directly blocked
	// -------------------------------
	if ( HasCondition( COND_SCANNER_FLY_BLOCKED ) )
	{
		if ( GetTarget() != NULL )
			return SCHED_MORTARSYNTH_CHASE_TARGET;
		else if ( GetEnemy() != NULL )
		{
#ifdef EZ2
			// Melee attack in case the enemy is blocking us
			if (HasCondition( COND_CAN_MELEE_ATTACK1 ))
				return SCHED_MELEE_ATTACK1;

			if ( HasCondition( COND_SEE_ENEMY ) )
			{
				// If we can see the enemy, but we're blocked, retreat
				trace_t tr;
				AI_TraceHull( GetAbsOrigin(), GetEnemy()->GetAbsOrigin(), GetHullMins(), GetHullMaxs(), MASK_NPCSOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
				if ( tr.fraction != 1.0 )
					return SCHED_TAKE_COVER_FROM_ENEMY;
			}

			float flEnemyDist = EnemyDistance( GetEnemy() );
			if (flEnemyDist <= 120.0f && gpGlobals->curtime > m_flNextAttack)
			{
				// Close enough that melee would be better than retreating
				return SCHED_MORTARSYNTH_MOVE_TO_MELEE;
			}
#endif

			return SCHED_MORTARSYNTH_CHASE_ENEMY;
		}
	}

	// ----------------------------------------------------------
	//  If I have an enemy
	// ----------------------------------------------------------
	if ( GetEnemy() != NULL && GetEnemy()->IsAlive() )
	{
		if (GetTarget()	== NULL	)
		{
			// Patrol if the enemy has vanished
			if ( HasCondition( COND_LOST_ENEMY ) )
				return SCHED_MORTARSYNTH_PATROL;

#ifdef EZ2
			// Use the repulse weapon if an enemy is too close to me
			if ( CanRepelEnemy() )
				return SCHED_MORTARSYNTH_REPEL;

			// Melee attack if they're close enough and we can't repulse
			if ( HasCondition( COND_CAN_MELEE_ATTACK1 ) )
				return SCHED_MELEE_ATTACK1;
#else
			// Use the repulse weapon if an enemy is too close to me
			if ( CanRepelEnemy() )
				return SCHED_MELEE_ATTACK1;
#endif

#ifdef EZ2
			if ( !HasCondition( COND_SEE_ENEMY ) )
			{
				// If we can suppress the enemy's last known location, do so
				// NOTE: This essentially reconstructs COND_CAN_RANGE_ATTACK1 because RangeAttack1Conditions() doesn't return that when the NPC can't see the enemy
				if ( !HasCondition( COND_TOO_CLOSE_TO_ATTACK ) && !HasCondition( COND_TOO_FAR_TO_ATTACK ) && gpGlobals->curtime > m_flNextAttack && CanGrenadeEnemy() )
				{
					// Fire!
					m_flNextAttack = gpGlobals->curtime + sk_mortarsynth_mortar_rate.GetFloat();
					SetLastAttackTime( gpGlobals->curtime );
					AddGesture( ACT_GESTURE_RANGE_ATTACK1 );
					return SCHED_MORTARSYNTH_HOVER;
				}

				return SCHED_MORTARSYNTH_ESTABLISH_LINE_OF_FIRE;
			}

			if ( HasCondition( COND_WEAPON_SIGHT_OCCLUDED ) )
				return SCHED_MORTARSYNTH_ESTABLISH_LINE_OF_FIRE;
#else
			// Attack if it's time
			else if ( gpGlobals->curtime >= m_flNextAttack )
			{
				if ( CanGrenadeEnemy() )
					return SCHED_RANGE_ATTACK1;
			}
#endif
		}

		// Otherwise fly in low for attack
		return SCHED_MORTARSYNTH_HOVER;
	}

#ifdef EZ2
	// -------------------------------
	// Hear combat
	// -------------------------------
	if ( HasCondition(COND_HEAR_COMBAT) )
	{
		CSound *pSound;
		pSound = GetBestSound();

		Assert( pSound != NULL );
		if ( pSound)
		{
			if ( ( pSound->m_iType & (SOUND_PLAYER | SOUND_COMBAT) ) )
			{
				return SCHED_ALERT_FACE_BESTSOUND;
			}
		}
	}
#endif

	// Default to patrolling around
	return SCHED_MORTARSYNTH_PATROL;
}

//-----------------------------------------------------------------------------
// Purpose: Gets the appropriate next schedule based on current condition
//			bits.
//-----------------------------------------------------------------------------
int CNPC_Mortarsynth::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
#ifdef EZ2
	switch (failedSchedule)
	{
		case SCHED_FAIL_TAKE_COVER:
			if ( GetEnemy() )
				return SCHED_MORTARSYNTH_MOVE_TO_MELEE;
			break;

		case SCHED_MORTARSYNTH_ESTABLISH_LINE_OF_FIRE:
			return SCHED_MORTARSYNTH_PATROL;
	}
#endif

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

//-----------------------------------------------------------------------------
// Purpose: Called just before we are deleted.
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::UpdateOnRemove( void )
{
	StopLoopingSounds();

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTask - 
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::StartTask( const Task_t *pTask )
{
	switch (pTask->iTask)
	{
	case TASK_ENERGY_REPEL:
		{
			UseRepulserWeapon();
			TaskComplete();
			break;
		}

#ifdef EZ2
	// Skip as done via bone controller
	case TASK_FACE_SAVEPOSITION:
	case TASK_FACE_REASONABLE:
		{
			TaskComplete();
			break;
		}
#endif

	default:
		BaseClass::StartTask(pTask);
		break;
	}
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
float CNPC_Mortarsynth::MinGroundDist( void )
{
	return 72;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : flInterval - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Mortarsynth::OverrideMove( float flInterval )
{
	Vector vMoveTargetPos( 0, 0, 0 );
	CBaseEntity *pMoveTarget = NULL;
		
	if ( !GetNavigator()->IsGoalActive() || ( GetNavigator()->GetCurWaypointFlags() & bits_WP_TO_PATHCORNER ) )
	{
		// Select move target 
		if ( GetTarget() != NULL )
		{
			pMoveTarget = GetTarget();
			// Select move target position
			vMoveTargetPos = GetTarget()->GetAbsOrigin();
		}
		else if ( GetEnemy() != NULL )
		{
			pMoveTarget = GetEnemy();
			// Select move target position
			vMoveTargetPos = GetEnemy()->GetAbsOrigin();
		}
	}
	else
		vMoveTargetPos = GetNavigator()->GetCurWaypointPos();

	ClearCondition( COND_SCANNER_FLY_CLEAR );
	ClearCondition( COND_SCANNER_FLY_BLOCKED );

	// See if we can fly there directly
	if ( pMoveTarget )
	{
		trace_t tr;
		AI_TraceHull( GetAbsOrigin(), vMoveTargetPos, GetHullMins(), GetHullMaxs(), MASK_NPCSOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );

		if ( tr.fraction != 1.0 )
			SetCondition( COND_SCANNER_FLY_CLEAR );	
		else
			SetCondition( COND_SCANNER_FLY_BLOCKED );
	}

	// If I have a route, keep it updated and move toward target
	if ( GetNavigator()->IsGoalActive() )
	{
		// -------------------------------------------------------------
		// If I'm on a path check LOS to my next node, and fail on path
		// if I don't have LOS.  Note this is the only place I do this,
		// so the manhack has to collide before failing on a path
		// -------------------------------------------------------------
		if ( GetNavigator()->IsGoalActive() && !( GetNavigator()->GetPath()->CurWaypointFlags() & bits_WP_TO_PATHCORNER ) )
		{
			AIMoveTrace_t moveTrace;
			GetMoveProbe()->MoveLimit( NAV_FLY, GetAbsOrigin(), GetNavigator()->GetCurWaypointPos(), MASK_NPCSOLID, GetEnemy(), &moveTrace );

			if ( IsMoveBlocked( moveTrace ) )
			{
				TaskFail( FAIL_NO_ROUTE );
				GetNavigator()->ClearGoal();
				return true;
			}
		}

		if ( OverridePathMove( pMoveTarget, flInterval ) )
			return true;
	}	
	// ----------------------------------------------
	//	If attacking
	// ----------------------------------------------
	else if ( pMoveTarget )
	{
		MoveToAttack( flInterval );
	}
	// -----------------------------------------------------------------
	// If I don't have a route, just decelerate
	// -----------------------------------------------------------------
	else if ( !GetNavigator()->IsGoalActive() )
	{
		float myDecay = 9.5;
		Decelerate( flInterval, myDecay );

		// -------------------------------------
		// If I have an enemy, turn to face him
		// -------------------------------------
		if ( GetEnemy() )
			TurnHeadToTarget( flInterval, GetEnemy()->EyePosition() );
	}
	
	MoveExecute_Alive( flInterval );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Accelerates toward a given position.
// Input  : flInterval - Time interval over which to move.
//			vecMoveTarget - Position to move toward.
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::MoveToTarget(float flInterval, const Vector& vecMoveTarget)
{
	if ( flInterval <= 0 )
		return;

	// Look at our inspection target if we have one
#ifdef EZ2
	if ( IsCurSchedule( SCHED_ALERT_FACE_BESTSOUND ) )
	{
		// Look at sound if possible
		CSound *pBestSound = GetBestSound();
		if (pBestSound)
			TurnHeadToTarget( flInterval, pBestSound->GetSoundReactOrigin() );
		else
			TurnHeadToTarget( flInterval, vecMoveTarget );
	}
	else if ( GetEnemy() != NULL && GetTranslatedActivity() != ACT_MSYNTH_PANIC )
#else
	if ( GetEnemy() != NULL )
#endif
		// Otherwise at our enemy
		TurnHeadToTarget( flInterval, GetEnemy()->EyePosition() );
	else
		// Otherwise face our motion direction
		TurnHeadToTarget( flInterval, vecMoveTarget );

	// -------------------------------------
	// Move towards our target
	// -------------------------------------
	float myAccel;
	float myZAccel = 400.0f;
	float myDecay  = 0.15f;

	Vector vecCurrentDir;

	// Get the relationship between my current velocity and the way I want to be going.
	vecCurrentDir = GetCurrentVelocity();
	VectorNormalize( vecCurrentDir );

	Vector targetDir = vecMoveTarget - GetAbsOrigin();
	float flDist = VectorNormalize(targetDir);

	float flDot;
	flDot = DotProduct( targetDir, vecCurrentDir );

#ifdef EZ2
	if( flDot > 0.25 )
		// If my target is in front of me, my flight model is a bit more accurate.
		myAccel = MORTARSYNTH_ACCEL;
	else
		// Have a harder time correcting my course if I'm currently flying away from my target.
		myAccel = MORTARSYNTH_ACCEL_TURN;

	// Move faster when panicking
	if (GetTranslatedActivity() == ACT_MSYNTH_PANIC)
		myAccel *= 2.0f;
	else if ( IsCurSchedule( SCHED_MORTARSYNTH_MOVE_TO_MELEE, false ) )
		myAccel *= 1.5f;
#else
	if( flDot > 0.25 )
		// If my target is in front of me, my flight model is a bit more accurate.
		myAccel = 250;
	else
		// Have a harder time correcting my course if I'm currently flying away from my target.
		myAccel = 128;
#endif

	// Clamp lateral acceleration
	if ( myAccel > flDist / flInterval )
		myAccel = flDist / flInterval;

	// Clamp vertical movement
	if ( myZAccel > flDist / flInterval )
		myZAccel = flDist / flInterval;

	MoveInDirection( flInterval, targetDir, myAccel, myZAccel, myDecay );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flInterval - 
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::MoveToAttack(float flInterval)
{
	if ( GetEnemy() == NULL )
		return;

	if ( flInterval <= 0 )
		return;

	TurnHeadToTarget( flInterval, GetEnemy()->EyePosition() );

	// -------------------------------------
	// Move towards our target
	// -------------------------------------
	float myAccel;
	float myZAccel = 400.0f;
	float myDecay  = 0.15f;

	Vector vecCurrentDir;

	// Get the relationship between my current velocity and the way I want to be going.
	vecCurrentDir = GetCurrentVelocity();
	VectorNormalize( vecCurrentDir );

	Vector targetDir = GetEnemy()->EyePosition() - GetAbsOrigin();
	float flDist = VectorNormalize( targetDir );

	float flDot;
	flDot = DotProduct( targetDir, vecCurrentDir );

#ifdef EZ2
	if( flDot > 0.25 )
		// If my target is in front of me, my flight model is a bit more accurate.
		myAccel = MORTARSYNTH_ACCEL;
	else
		// Have a harder time correcting my course if I'm currently flying away from my target.
		myAccel = MORTARSYNTH_ACCEL_TURN;
#else
	if( flDot > 0.25 )
		// If my target is in front of me, my flight model is a bit more accurate.
		myAccel = 250;
	else
		// Have a harder time correcting my course if I'm currently flying away from my target.
		myAccel = 128;
#endif

	// Clamp lateral acceleration
	if ( myAccel > flDist / flInterval )
		myAccel = flDist / flInterval;

	// Clamp vertical movement
	if ( myZAccel > flDist / flInterval )
		myZAccel = flDist / flInterval;

	Vector idealPos;

#ifdef EZ2
	// Stay in melee range
	if ( IsCurSchedule( SCHED_MORTARSYNTH_MOVE_TO_MELEE, false ) || IsCurSchedule( SCHED_MORTARSYNTH_MELEE_ATTACK1, false ) )
	{
		if ( flDist < MORTARSYNTH_MELEE_RANGE )
			idealPos = -targetDir;
		else if ( flDist > MORTARSYNTH_MELEE_RANGE )
			idealPos = targetDir;
	}
	else
#endif
	{
#ifdef EZ2
		if ( flDist < m_flAttackFarDist )
#else
		if ( flDist < m_flAttackNearDist )
#endif
			idealPos = -targetDir;
		else if ( flDist > m_flAttackFarDist )
			idealPos = targetDir;
	}

	MoveInDirection( flInterval, idealPos, myAccel, myZAccel, myDecay );
}

//-----------------------------------------------------------------------------
// Purpose: Turn head yaw into facing direction. Overriden from ai_basenpc_physicsflyer.cpp
// Input  : flInterval, MoveTarget
// Output :
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::TurnHeadToTarget(float flInterval, const Vector &MoveTarget )
{
#ifdef EZ2
	Vector vecDelta = MoveTarget - GetLocalOrigin();
	VectorNormalize( vecDelta );

	float desYaw = UTIL_AngleDiff( UTIL_VecToYaw( vecDelta ), GetLocalAngles().y );
	float desPitch = UTIL_VecToPitch( vecDelta );
	
	// If I've flipped completely around, reverse angles
	float fYawDiff = m_fHeadYaw - desYaw;
	if ( fYawDiff > 180 )
		m_fHeadYaw -= 360;
	else if ( fYawDiff < -180 )
		m_fHeadYaw += 360;
	
	float fPitchDiff = m_fHeadPitch - desPitch;
	if (fPitchDiff > 180 )
		m_fHeadPitch -= 360;
	else if (fPitchDiff < -180 )
		m_fHeadPitch += 360;

	float iRate = 0.9f;

	// Make frame rate independent
	float timeToUse = flInterval;
	while ( timeToUse > 0 )
	{
		m_fHeadYaw = ( iRate * m_fHeadYaw ) + ( 1-iRate ) * desYaw;
		m_fHeadPitch = ( iRate * m_fHeadPitch ) + ( 1-iRate ) * desPitch;
		timeToUse -= 0.1;
	}

	while( m_fHeadYaw > 360 )
		m_fHeadYaw -= 360.0f;
	while( m_fHeadYaw < 0 )
		m_fHeadYaw += 360.f;

	while( m_fHeadPitch > 360 )
		m_fHeadPitch -= 360.0f;

	SetBoneController( 0, m_fHeadYaw );

	//SetBoneController( 1, m_fHeadPitch );
	SetPoseParameter( m_nPoseHeadPitch, m_fHeadPitch );
#else
	float desYaw = UTIL_AngleDiff( VecToYaw( MoveTarget - GetLocalOrigin() ), GetLocalAngles().y );

	// If I've flipped completely around, reverse angles
	float fYawDiff = m_fHeadYaw - desYaw;
	if ( fYawDiff > 180 )
		m_fHeadYaw -= 360;
	else if ( fYawDiff < -180 )
		m_fHeadYaw += 360;

	float iRate = 0.8;

	// Make frame rate independent
	float timeToUse = flInterval;
	while ( timeToUse > 0 )
	{
		m_fHeadYaw = ( iRate * m_fHeadYaw ) + ( 1-iRate ) * desYaw;
		timeToUse -= 0.1;
	}

	while( m_fHeadYaw > 360 )
		m_fHeadYaw -= 360.0f;

	while( m_fHeadYaw < 0 )
		m_fHeadYaw += 360.f;

	SetBoneController( 0, m_fHeadYaw );
#endif
}

#ifdef EZ2
//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
Vector CNPC_Mortarsynth::VelocityToAvoidObstacles(float flInterval)
{
	// --------------------------------
	//  Avoid banging into stuff
	// --------------------------------
	trace_t tr;
	Vector vTravelDir = m_vCurrentVelocity*flInterval;
	Vector endPos = GetAbsOrigin() + vTravelDir;
	AI_TraceEntity( this, GetAbsOrigin(), endPos, MASK_NPCSOLID|CONTENTS_WATER, &tr );
	if (tr.fraction != 1.0)
	{	
		// Bounce off in normal 
		Vector vBounce = tr.plane.normal * 0.5 * m_vCurrentVelocity.Length();
		return (vBounce);
	}
	
	// --------------------------------
	// Try to remain above the ground.
	// --------------------------------
	float flMinGroundDist = MinGroundDist();
	AI_TraceLine(GetAbsOrigin(), GetAbsOrigin() + Vector(0, 0, -flMinGroundDist), 
		MASK_NPCSOLID_BRUSHONLY|CONTENTS_WATER, this, COLLISION_GROUP_NONE, &tr);
	if (tr.fraction < 1)
	{
		// Clamp velocity
		if (tr.fraction < 0.1)
		{
			tr.fraction = 0.1;
		}

		return Vector(0, 0, 50/tr.fraction);
	}
	
	
	// --------------------------------
	// Try to remain below the ceiling.
	// --------------------------------
	AI_TraceLine(GetAbsOrigin(), GetAbsOrigin() + Vector(0, 0, flMinGroundDist), 
		MASK_NPCSOLID_BRUSHONLY|CONTENTS_WATER, this, COLLISION_GROUP_NONE, &tr);
	if (tr.fraction < 1)
	{
		// Clamp velocity
		if (tr.fraction < 0.1)
		{
			tr.fraction = 0.1;
		}

		return Vector(0, 0, -(100/tr.fraction));
	}

	return vec3_origin;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CNPC_Mortarsynth::GetMaxSpeed()
{
	if (m_flCustomMaxSpeed > 0.0f)
		return m_flCustomMaxSpeed;

	return MORTARSYNTH_MAX_SPEED;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Overriden from npc_basescanner.cpp
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::MoveExecute_Alive( float flInterval )
{
	// Amount of noise to add to flying
	float noiseScale = 3.0f;

	SetCurrentVelocity( GetCurrentVelocity() + VelocityToAvoidObstacles( flInterval ) );

	// Add time-coherent noise to the current velocity so that it never looks bolted in place.
	AddNoiseToVelocity( noiseScale );

	float maxSpeed = GetEnemy() ? ( GetMaxSpeed() * 2.0f ) : GetMaxSpeed();

#ifdef EZ2
	// Move faster when panicking
	if (GetTranslatedActivity() == ACT_MSYNTH_PANIC)
	{
		maxSpeed *= 2.0f;

		// Boost particles
		// TODO: Better place for this?
		if ( !m_bDidBoostEffect )
		{
			DispatchParticleEffect( "mortarsynth_propulsion_trail_boost", PATTACH_POINT_FOLLOW, this, m_nAttachmentPropulsionL );
			DispatchParticleEffect( "mortarsynth_propulsion_trail_boost", PATTACH_POINT_FOLLOW, this, m_nAttachmentPropulsionR );
			EmitSound( "NPC_MortarSynth.Boost" );
			m_bDidBoostEffect = true;
		}
	}
	else if ( IsCurSchedule( SCHED_MORTARSYNTH_MOVE_TO_MELEE, false ) )
		maxSpeed *= 1.5f;

	// Limit fall speed
	LimitSpeed( 0.0f, maxSpeed );
#else
	// Limit fall speed
	LimitSpeed( maxSpeed );
#endif

	QAngle angles = GetLocalAngles();

	// Make frame rate independent
#ifdef EZ2
	float	iRate	 = 0.25;
#else
	float	iRate	 = 0.5;
#endif
	float timeToUse = flInterval;

	while ( timeToUse > 0 )
	{
		// Get the relationship between my current velocity and the way I want to be going.
		Vector vecCurrentDir = GetCurrentVelocity();
		VectorNormalize( vecCurrentDir );

		// calc relative banking targets
		Vector forward, right;
		GetVectors( &forward, &right, NULL );

		Vector m_vTargetBanking;

#ifdef EZ2
		m_vTargetBanking.x = 10 * DotProduct( forward, vecCurrentDir );
		m_vTargetBanking.z = 10 * DotProduct( right, vecCurrentDir );
#else
		m_vTargetBanking.x	= 40 * DotProduct( forward, vecCurrentDir );
		m_vTargetBanking.z	= 40 * DotProduct( right, vecCurrentDir );
#endif

		m_vCurrentBanking.x = ( iRate * m_vCurrentBanking.x ) + ( 1 - iRate )*( m_vTargetBanking.x );
		m_vCurrentBanking.z = ( iRate * m_vCurrentBanking.z ) + ( 1 - iRate )*( m_vTargetBanking.z );
		timeToUse = -0.1;
	}
	angles.x = m_vCurrentBanking.x;
	angles.z = m_vCurrentBanking.z;
	angles.y = 0;

	SetLocalAngles( angles );

	PlayFlySound();

	WalkMove( GetCurrentVelocity() * flInterval, MASK_NPCSOLID );

#ifdef EZ2
	// Update what we're looking at
	UpdateHead( flInterval );

	// Update propulsion pose from banking
	if (m_nPosePropulsionYaw != -1 && m_nPosePropulsionPitch != -1)
	{
		QAngle angBodyAngles = BodyAngles();

		Vector vecForward, vecRight;
		AngleVectors( angBodyAngles, &vecForward, &vecRight, NULL );

		Vector vecCurrentDir = GetCurrentVelocity();
		float flSpeedRatio = vecCurrentDir.LengthSqr() / Square( MORTARSYNTH_MAX_SPEED / 2 );
		VectorNormalize( vecCurrentDir );

		float flYaw = RemapVal( -DotProduct( vecRight, vecCurrentDir ) * flSpeedRatio * 10.0f, -1.0f, 1.0f, -20.0f, 20.0f );
		float flPitch = RemapVal( DotProduct( vecForward, vecCurrentDir ) * flSpeedRatio, -1.0f, 1.0f, -25.0f, 70.0f );

		SetPoseParameter( m_nPosePropulsionYaw, flYaw );
		SetPoseParameter( m_nPosePropulsionPitch, flPitch );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Handles movement towards the last move target. Overriden from npc_basescanner.cpp
// Input  : flInterval - 
//-----------------------------------------------------------------------------
bool CNPC_Mortarsynth::OverridePathMove( CBaseEntity *pMoveTarget, float flInterval )
{
	// Save our last patrolling direction
	Vector lastPatrolDir = GetNavigator()->GetCurWaypointPos() - GetAbsOrigin();

	// Continue on our path
	if ( ProgressFlyPath( flInterval, pMoveTarget, ( MASK_NPCSOLID | CONTENTS_WATER ), !IsCurSchedule( SCHED_MORTARSYNTH_PATROL ) ) == AINPP_COMPLETE )
	{
		if ( IsCurSchedule( SCHED_MORTARSYNTH_PATROL ) )
		{
			m_vLastPatrolDir = lastPatrolDir;
			VectorNormalize( m_vLastPatrolDir );
		}

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::PlayFlySound( void )
{
	if ( IsMarkedForDeletion() )
		return;

	if ( gpGlobals->curtime > m_fNextFlySoundTime )
	{
		EmitSound( "NPC_Mortarsynth.Hover" );

		// If we are hurting, play some scary hover sounds :O
		if ( GetHealth() < ( GetMaxHealth() / 3 ) )
			EmitSound( "NPC_Mortarsynth.HoverAlarm" );

		m_fNextFlySoundTime	= gpGlobals->curtime + 1.0;
	}
}

//---------------------------------------------------------------------------------
// Purpose : Checks if an enemy is within radius, in order to use the repulse wave
// Output : Returns true on success, false on failure.
//---------------------------------------------------------------------------------
bool CNPC_Mortarsynth::CanRepelEnemy()
{
#ifdef EZ2
	if ( !(CapabilitiesGet() & bits_CAP_INNATE_MELEE_ATTACK2) )
		return false;

	// We're specifically trying to melee this enemy
	if ( IsCurSchedule( SCHED_MORTARSYNTH_MOVE_TO_MELEE, false ) )
		return false;
#endif

	if ( gpGlobals->curtime < m_flNextRepulse )
		return false;

	float flDist;
	flDist = ( GetEnemy()->GetAbsOrigin() - GetAbsOrigin() ).Length();

	if( flDist < sk_energy_grenade_radius.GetFloat() + 50 )
		return true;

	return false;
}

//------------------------------------------------------------------------------
// Purpose : Pushes away anything within a radius. Base code from point_push
//------------------------------------------------------------------------------
void CNPC_Mortarsynth::UseRepulserWeapon( void )
{
	CTakeDamageInfo	info;
	
	info.SetAttacker( this );
	info.SetInflictor( this );
#ifdef EZ2
	info.SetDamage( sk_mortarsynth_dmg_repulser.GetFloat() );
	info.SetDamageType( DMG_SONIC );
	info.SetDamagePosition( GetAbsOrigin() );
#else
	info.SetDamage( 10.0f );
	info.SetDamageType( DMG_DISSOLVE );
#endif

	// Get a collection of entities in a radius around us
	CBaseEntity *pEnts[256];
	int numEnts = UTIL_EntitiesInSphere( pEnts, 256, GetAbsOrigin(), m_flAttackNearDist, 0 );
	for ( int i = 0; i < numEnts; i++ )
	{
		// Fenix: Failsafe
		if ( pEnts[i] == NULL )
			continue;

#ifdef EZ2
		// Don't push any friendly NPC
		if ( IRelationType( pEnts[i] ) == D_LI )
			continue;
#else
		// Fenix: Dont push myself or other mortarsynths
		if ( FClassnameIs( pEnts[i], "npc_mortarsynth" ) )
			continue;
#endif

		// Must be solid
		if ( pEnts[i]->IsSolid() == false )
			continue;

		// Cannot be parented (only push parents)
		if ( pEnts[i]->GetMoveParent() != NULL )
			continue;

		// Must be moveable
		if ( pEnts[i]->GetMoveType() != MOVETYPE_VPHYSICS && 
			 pEnts[i]->GetMoveType() != MOVETYPE_WALK && 
			 pEnts[i]->GetMoveType() != MOVETYPE_STEP )
			continue; 

		// Push it along
		PushEntity( pEnts[i], info );
	}

	DispatchParticleEffect( "weapon_combine_ion_cannon_d", GetAbsOrigin(), GetAbsAngles() );

	EmitSound( "NPC_Mortarsynth.EnergyShoot" );

	m_flNextRepulse = gpGlobals->curtime + sk_mortarsynth_repulse_rate.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose : Pushes away anything within a radius. Base code from point_push
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::PushEntity( CBaseEntity *pTarget, CTakeDamageInfo &info )
{
	Vector vecPushDir;
	
	vecPushDir = ( pTarget->BodyTarget( GetAbsOrigin(), false ) - GetAbsOrigin() );
#ifdef EZ2
	VectorNormalize( vecPushDir );
#endif

	switch( pTarget->GetMoveType() )
	{
	case MOVETYPE_NONE:
	case MOVETYPE_PUSH:
	case MOVETYPE_NOCLIP:
		break;

	case MOVETYPE_VPHYSICS:
		{
			IPhysicsObject *pPhys = pTarget->VPhysicsGetObject();
			if ( pPhys )
			{
#ifdef EZ2
				info.SetDamageForce( 10 * 1.0f * 5000.0f * vecPushDir * gpGlobals->frametime );
#else
				// UNDONE: Assume the velocity is for a 100kg object, scale with mass
				pPhys->ApplyForceCenter( 10 * 1.0f * 100.0f * vecPushDir * pPhys->GetMass() * gpGlobals->frametime );
#endif
				return;
			}
		}
		break;

	case MOVETYPE_STEP:
		{
			// NPCs cannot be lifted up properly, they need to move in 2D
			vecPushDir.z = 0.0f;
			
			// NOTE: Falls through!
		}

	default:
		{
			Vector vecPush = ( 10 * vecPushDir * 1.0f );
			if ( pTarget->GetFlags() & FL_BASEVELOCITY )
			{
				vecPush = vecPush + pTarget->GetBaseVelocity();
			}
			if ( vecPush.z > 0 && (pTarget->GetFlags() & FL_ONGROUND) )
			{
				pTarget->SetGroundEntity( NULL );
				Vector origin = pTarget->GetAbsOrigin();
				origin.z += 1.0f;
				pTarget->SetAbsOrigin( origin );
			}

			pTarget->SetBaseVelocity( vecPush );
			pTarget->AddFlag( FL_BASEVELOCITY );

#ifdef EZ2
			info.SetDamageForce( vecPush );
#endif
		}
		break;
	}

	pTarget->TakeDamage( info );
}

//-----------------------------------------------------------------------------
// Purpose : Checks if an enemy exists, in order to attack
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Mortarsynth::CanGrenadeEnemy( bool bUseFreeKnowledge )
{
	CBaseEntity *pEnemy = GetEnemy();

	Assert( pEnemy != NULL );

	if( pEnemy )
	{
		if( bUseFreeKnowledge )
		{
			// throw to where we think they are.
			return CanThrowGrenade( GetEnemies()->LastKnownPosition( pEnemy ) );
		}
		else
		{
			// hafta throw to where we last saw them.
			return CanThrowGrenade( GetEnemies()->LastSeenPosition( pEnemy ) );
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the combine has grenades, hasn't checked lately, and
//			can throw a grenade at the target point.
// Input  : &vecTarget - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Mortarsynth::CanThrowGrenade( const Vector &vecTarget )
{
#ifndef EZ2
	float flDist;
	flDist = ( vecTarget - GetAbsOrigin() ).Length();

	if ( flDist > 2048 || flDist < sk_energy_grenade_radius.GetFloat() + 50 )
	{
		// Too close or too far!
		m_flNextAttack = gpGlobals->curtime + 1; // one full second.
		return false;
	}
#endif

	// ---------------------------------------------------------------------
	// Are any of my squad members near the intended grenade impact area?
	// ---------------------------------------------------------------------
	
#ifdef EZ2
	if ( m_pSquad )
	{
		if ( m_pSquad->SquadMemberInRange( vecTarget, sk_energy_grenade_radius.GetFloat() + 50 ) )
		{
			// crap, I might blow my own guy up. Don't throw a grenade and don't check again for a while.
			//m_flNextAttack = gpGlobals->curtime + 1; // one full second.

			// Tell my squad members to clear out so I can get a grenade in
			CSoundEnt::InsertSound( SOUND_MOVE_AWAY /*| SOUND_CONTEXT_COMBINE_ONLY*/, vecTarget, sk_energy_grenade_radius.GetFloat() + 50, 0.1 );
			//return false;
		}
	}
#else
	// Fenix: Disabled for us, as different NPCs that the mapper may place on the same squad can't interpret the sound warning, so the mortarsynth would hold fire forever
	// Feel free to re-enable if such problem doesn't exist on your mod

	/*if ( m_pSquad )
	{
		if ( m_pSquad->SquadMemberInRange( vecTarget, sk_energy_grenade_radius.GetFloat() + 50 ) )
		{
			// crap, I might blow my own guy up. Don't throw a grenade and don't check again for a while.
			m_flNextAttack = gpGlobals->curtime + 1; // one full second.

			// Tell my squad members to clear out so I can get a grenade in
			CSoundEnt::InsertSound( SOUND_MOVE_AWAY | SOUND_CONTEXT_COMBINE_ONLY, vecTarget, 300, 0.1 );
			return false;
		}
	}*/
#endif

	return CheckCanThrowGrenade( vecTarget );
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the combine can throw a grenade at the specified target point
// Input  : &vecTarget - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Mortarsynth::CheckCanThrowGrenade( const Vector &vecTarget )
{
	// ---------------------------------------------------------------------
	// Check that throw is legal and clear
	// ---------------------------------------------------------------------
	Vector vecGrenadeOrigin = EyePosition();
	Vector vecToss;
	Vector vecMins = -Vector( 4, 4, 4 );
	Vector vecMaxs = Vector( 4, 4, 4 );

#ifdef EZ2
	// Calculate from where I will be in 0.7 seconds
	vecGrenadeOrigin += (GetCurrentVelocity() * 0.7f);
#endif

	// Have to try a high toss. Do I have enough room?
	trace_t tr;
	AI_TraceLine( vecGrenadeOrigin, vecGrenadeOrigin + Vector( 0, 0, 64 ), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

	if( tr.fraction != 1.0 )
		return false;

	vecToss = VecCheckToss( this, vecGrenadeOrigin, vecTarget, -1, 1.0, true, &vecMins, &vecMaxs );

	if ( vecToss != vec3_origin )
	{
		m_vecTossVelocity = vecToss;

#ifndef EZ2
		// don't check again for a while.
		m_flNextAttack = gpGlobals->curtime + sk_mortarsynth_mortar_rate.GetFloat();
#endif
		return true;
	}
	else
	{
		// don't check again for a while.
		m_flNextAttack = gpGlobals->curtime + 1; // one full second.
		return false;
	}
}

#ifdef EZ2
//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Mortarsynth::RangeAttack1Conditions( float flDot, float flDist )
{
	if ( flDist > 2048 )
	{
		return COND_TOO_FAR_TO_ATTACK;
	}
	else if ( flDist < sk_energy_grenade_radius.GetFloat() + 50 )
	{
		return COND_TOO_CLOSE_TO_ATTACK;
	}
	else if ( flDot < 0.5f )
	{
		return COND_NOT_FACING_ATTACK;
	}
	else if ( m_flNextAttack > gpGlobals->curtime )
	{
		return COND_NONE;
	}
	else if ( !CanGrenadeEnemy() )
	{
		return COND_WEAPON_SIGHT_OCCLUDED;
	}

	return COND_CAN_RANGE_ATTACK1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Mortarsynth::MeleeAttack1Conditions( float flDot, float flDist )
{
	// Since we move more quickly when moving to melee, we can start the melee attack from further away
	if (flDist > 72.0f && (!IsCurSchedule(SCHED_MORTARSYNTH_MOVE_TO_MELEE, false) || flDist > 120.0f))
	{
		return COND_NONE; // COND_TOO_FAR_TO_ATTACK;
	}
	//else if (flDot < 0.5f)
	//{
	//	return COND_NONE; // COND_NOT_FACING_ATTACK;
	//}

	return COND_CAN_MELEE_ATTACK1;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Mortarsynth::OnScheduleChange( void )
{
#ifdef EZ2
	// Reset after every schedule change
	m_bDidBoostEffect = false;
#endif

	BaseClass::OnScheduleChange();
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_mortarsynth, CNPC_Mortarsynth )
	DECLARE_TASK( TASK_ENERGY_REPEL )

	DECLARE_ACTIVITY( ACT_MSYNTH_FLINCH_BACK )
	DECLARE_ACTIVITY( ACT_MSYNTH_FLINCH_FRONT )
	DECLARE_ACTIVITY( ACT_MSYNTH_FLINCH_LEFT )
	DECLARE_ACTIVITY( ACT_MSYNTH_FLINCH_RIGHT )
	DECLARE_ACTIVITY( ACT_MSYNTH_FLINCH_BIG_BACK )
	DECLARE_ACTIVITY( ACT_MSYNTH_FLINCH_BIG_FRONT )
	DECLARE_ACTIVITY( ACT_MSYNTH_FLINCH_BIG_LEFT )
	DECLARE_ACTIVITY( ACT_MSYNTH_FLINCH_BIG_RIGHT )
#ifdef EZ2
	DECLARE_ACTIVITY( ACT_MSYNTH_ALERT )
	DECLARE_ACTIVITY( ACT_MSYNTH_PANIC )
#endif

	//=========================================================
	// > SCHED_MORTARSYNTH_PATROL
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_MORTARSYNTH_PATROL,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_FAIL_NOSTOP"
		"		TASK_SET_TOLERANCE_DISTANCE			32"
		"		TASK_SET_ROUTE_SEARCH_TIME			5"	// Spend 5 seconds trying to build a path if stuck
		"		TASK_GET_PATH_TO_RANDOM_NODE		2000"
		"		TASK_RUN_PATH						0"
		"		TASK_WAIT_FOR_MOVEMENT				0"
		""
		"	Interrupts"
		"		COND_GIVE_WAY"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_SEE_FEAR"
		"		COND_HEAR_COMBAT"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_PLAYER"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_PROVOKED"
	)

	//=========================================================
	// > SCHED_MORTARSYNTH_ATTACK
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_MORTARSYNTH_ATTACK,

		"	Tasks"
		"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_RANGE_ATTACK1"
		"		TASK_SET_SCHEDULE					SCHEDULE:SCHED_TAKE_COVER_FROM_ENEMY"
		"		TASK_WAIT							5"
		""
		"	Interrupts"
	)

	//=========================================================
	// > SCHED_MORTARSYNTH_REPEL
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_MORTARSYNTH_REPEL,

		"	Tasks"
		"		TASK_ENERGY_REPEL					0"
		"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_MELEE_ATTACK2"
		"		TASK_WAIT							0.1"
		""
		"	Interrupts"
	)

	//=========================================================
	// > SCHED_MORTARSYNTH_CHASE_ENEMY
	//
	//  Different interrupts than normal chase enemy.  
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_MORTARSYNTH_CHASE_ENEMY,

		"	Tasks"
		"		 TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_MORTARSYNTH_PATROL"
		"		 TASK_SET_TOLERANCE_DISTANCE		120"
		"		 TASK_GET_PATH_TO_ENEMY				0"
		"		 TASK_RUN_PATH						0"
		"		 TASK_WAIT_FOR_MOVEMENT				0"
		""
		""
		"	Interrupts"
		"		COND_SCANNER_FLY_CLEAR"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_LOST_ENEMY"
	)

	//=========================================================
	// > SCHED_MSYNTH_CHASE_TARGET
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_MORTARSYNTH_CHASE_TARGET,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_MORTARSYNTH_HOVER"
		"		TASK_SET_TOLERANCE_DISTANCE			120"
		"		TASK_GET_PATH_TO_TARGET				0"
		"		TASK_RUN_PATH						0"
		"		TASK_WAIT_FOR_MOVEMENT				0"
		""
		"	Interrupts"
		"		COND_SCANNER_FLY_CLEAR"
		"		COND_NEW_ENEMY"
	)

	//=========================================================
	// > SCHED_MORTARSYNTH_HOVER
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_MORTARSYNTH_HOVER,

		"	Tasks"
		"		TASK_WAIT							5"
		""
		"	Interrupts"
		"		COND_GIVE_WAY"
		"		COND_NEW_ENEMY"
		//"		COND_SEE_ENEMY"
		//"		COND_SEE_FEAR"
		//"		COND_HEAR_COMBAT"
		//"		COND_HEAR_DANGER"
		//"		COND_HEAR_PLAYER"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_PROVOKED"
		"		COND_SCANNER_FLY_BLOCKED"
	)

#ifdef EZ2
	//=========================================================
	// > SCHED_MORTARSYNTH_MELEE_ATTACK1
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_MORTARSYNTH_MELEE_ATTACK1,

		"	Tasks"
		//"		TASK_STOP_MOVING		0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_MELEE_ATTACK1		0"
		""
		"	Interrupts"
	)

	//=========================================================
	// > SCHED_MORTARSYNTH_MOVE_TO_MELEE  
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_MORTARSYNTH_MOVE_TO_MELEE,

		"	Tasks"
		"		 TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_MORTARSYNTH_PATROL"
		"		 TASK_WAIT							5"
		//"		 TASK_SET_TOLERANCE_DISTANCE		0"
		//"		 TASK_GET_PATH_TO_ENEMY				0"
		//"		 TASK_RUN_PATH						0"
		//"		 TASK_WAIT_FOR_MOVEMENT				0"
		""
		"	Interrupts"
		"		 COND_NEW_ENEMY"
		"		 COND_ENEMY_DEAD"
		"		 COND_CAN_MELEE_ATTACK1"
		"		 COND_CAN_RANGE_ATTACK1"
	)

	//=========================================================
	// > SCHED_MORTARSYNTH_ESTABLISH_LINE_OF_FIRE  
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_MORTARSYNTH_ESTABLISH_LINE_OF_FIRE,

		"	Tasks"
		"		 TASK_STOP_MOVING				0"
		"		 TASK_GET_CHASE_PATH_TO_ENEMY	300"
		"		 TASK_RUN_PATH					0"
		"		 TASK_WAIT_FOR_MOVEMENT			0"
		""
		"	Interrupts"
		"		 COND_NEW_ENEMY"
		"		 COND_ENEMY_DEAD"
		"		 COND_ENEMY_UNREACHABLE"
		"		 COND_CAN_RANGE_ATTACK1"
		"		 COND_CAN_MELEE_ATTACK1"
		"		 COND_CAN_RANGE_ATTACK2"
		"		 COND_CAN_MELEE_ATTACK2"
		"		 COND_TOO_CLOSE_TO_ATTACK"
		"		 COND_TASK_FAILED"
		"		 COND_LOST_ENEMY"
		"		 COND_HEAR_DANGER"
	)

	//=========================================================
	// > SCHED_MORTARSYNTH_ALERT
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_MORTARSYNTH_ALERT,

		"	Tasks"
		"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_MSYNTH_ALERT"
		""
		"	Interrupts"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_PROVOKED"
	)

	//=========================================================
	// > SCHED_MORTARSYNTH_TAKE_COVER_FROM_ENEMY
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_MORTARSYNTH_TAKE_COVER_FROM_ENEMY,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_FAIL_TAKE_COVER"
		"		TASK_STOP_MOVING				0"
		//"		TASK_WAIT						0.2"
//		"		TASK_SET_TOLERANCE_DISTANCE		24"
		"		TASK_FIND_COVER_FROM_ENEMY		0"
		"		TASK_RUN_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_REMEMBER					MEMORY:INCOVER"
		"		TASK_FACE_ENEMY					0"
		"		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"	// Translated to cover
		"		TASK_WAIT						1"
		""
		"	Interrupts" // Not interrupted by new enemy or danger
		//"		COND_NEW_ENEMY"
		//"		COND_HEAR_DANGER"
	)

	//=========================================================
	// > SCHED_MORTARSYNTH_FLEE_FROM_BEST_SOUND
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_MORTARSYNTH_FLEE_FROM_BEST_SOUND,

		"	Tasks"
		"		 TASK_STORE_BESTSOUND_REACTORIGIN_IN_SAVEPOSITION	0"
		"		 TASK_GET_PATH_AWAY_FROM_BEST_SOUND	200"
		"		 TASK_RUN_PATH						0"
		"		 TASK_STOP_MOVING					0"
		"		 TASK_FACE_SAVEPOSITION				0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
	)
#endif
	
AI_END_CUSTOM_NPC()