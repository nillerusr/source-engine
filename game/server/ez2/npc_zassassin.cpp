//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Gonome. Big scary-ass headcrab zombie, based heavily on the Bullsquid.+
//				+++Repurposed into the Zombie Assassin
//				++Can now Jump and Climb like the fast zombie
//
// 			Originally, the Gonome / Zombie Assassin was created by Sergeant Stacker
// 			and provided to the EZ2 team. 
//			It has been heavily modified for its role as the boss of EZ2 chapter 3. 
//			It now inherits from a base class, CNPC_BasePredator			
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "game.h"
#include "ai_default.h"
#include "ai_schedule.h"
#include "ai_hull.h"
#include "ai_route.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_senses.h"
#include "npcevent.h"
#include "animation.h"
#include "npc_zassassin.h"
#include "gib.h"
#include "soundent.h"
#include "ndebugoverlay.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
//#include "hl1_grenade_spit.h"
#include "util.h"
#include "shake.h"
#include "movevars_shared.h"
#include "decals.h"
#include "hl2_shareddefs.h"
#include "hl2_gamerules.h"
#include "ammodef.h"

#include "player.h"
#include "ai_network.h"
#include "ai_navigator.h"
#include "ai_motor.h"
#include "ai_default.h"
#include "ai_schedule.h"
#include "ai_hull.h"
#include "ai_node.h"
#include "ai_memory.h"
#include "ai_senses.h"
#include "bitstring.h"
#include "EntityFlame.h"
#include "hl2_shareddefs.h"
#include "npcevent.h"
#include "activitylist.h"
#include "entitylist.h"
#include "gib.h"
#include "soundenvelope.h"
#include "ndebugoverlay.h"
#include "rope.h"
#include "rope_shared.h"
#include "igamesystem.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "props.h"
#include "hl2_gamerules.h"
#include "weapon_physcannon.h"
#include "ammodef.h"
#include "vehicle_base.h"
#include "ai_squad.h"
#include "ez2_player.h" // Needed to dispatch response from here - probably should be moved elsewhere

#define ZOMBIE_BURN_TIME		10  // If ignited, burn for this many seconds
#define ZOMBIE_BURN_TIME_NOISE	2   // Give or take this many seconds.
#define MAX_SPIT_DISTANCE		512 // Maximum range of range attack 1 - was 784, adjusting to 512

ConVar sk_zombie_assassin_health ( "sk_zombie_assassin_health", "1000" );
ConVar sk_zombie_assassin_boss_health("sk_zombie_assassin_boss_health", "2000");
ConVar sk_zombie_assassin_dmg_bite ( "sk_zombie_assassin_dmg_bite", "25" );
ConVar sk_zombie_assassin_dmg_whip ( "sk_zombie_assassin_dmg_whip", "35" );
ConVar sk_zombie_assassin_dmg_spit ( "sk_zombie_assassin_dmg_spit", "15" );
ConVar sk_zombie_assassin_spawn_time( "sk_zombie_assassin_spawn_time", "5.0" );
ConVar sk_zombie_assassin_look_dist( "sk_zombie_assassin_look_dist", "1024.0" );
ConVar sk_zombie_assassin_eatincombat_percent( "sk_zombie_assassin_eatincombat_percent", "1.0", FCVAR_NONE, "Below what percentage of health should gonomes eat during combat?" );
ConVar sk_zombie_assassin_armsout_dist( "sk_zombie_assassin_armsout_dist", "350.0", FCVAR_NONE, "How far away to begin reaching out to the enemy" );

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_GONOME_CHASE_ENEMY = LAST_SHARED_SCHEDULE_PREDATOR + 1,
	SCHED_GONOME_RANGE_ATTACK1, // Like standard range attack, but with no interrupts for damage
	SCHED_GONOME_SCHED_RUN_FROM_ENEMY, // Like standard run from enemy, but looks for a point to run to 512 units away
	SCHED_GONOME_FRUSTRATION, // Almost caught enemy, but they escaped, so throw a fit
	SCHED_GONOME_FLANK_ENEMY, // Try to go around to attack the enemy

	TASK_GONOME_GET_FLANK_PATH = LAST_SHARED_PREDATOR_TASK + 1, // Try a number of flanking methods
};

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		GONOME_AE_TAILWHIP	( 4 )
#define		GONOME_AE_PLAYSOUND ( 1011 )

int		GONOME_AE_LAND;
int		GONOME_AE_WALL_POUND;

LINK_ENTITY_TO_CLASS( monster_gonome, CNPC_Gonome );
LINK_ENTITY_TO_CLASS( npc_zassassin, CNPC_Gonome ); //For Zombie Assassin version -Stacker

//=========================================================
// Gonome's spit projectile
//=========================================================
class CGonomeSpit : public CBaseEntity
{
	DECLARE_CLASS( CGonomeSpit, CBaseEntity );
public:
	void Spawn( void );
	void Precache( void );

	static void Shoot( CBaseEntity *pOwner, int nGonomeSpitSprite, CSprite * pSprite, Vector vecStart, Vector vecVelocity );
	void Touch( CBaseEntity *pOther );
	void Animate( void );

	int m_nGonomeSpitSprite;

	bool m_bGoo;

	DECLARE_DATADESC();

	void SetSprite( CBaseEntity *pSprite )
	{
		m_hSprite = pSprite;	
	}

	CBaseEntity *GetSprite( void )
	{
		return m_hSprite.Get();
	}

private:
	EHANDLE m_hSprite;


};

LINK_ENTITY_TO_CLASS( squidspit, CGonomeSpit );

BEGIN_DATADESC( CGonomeSpit )
	DEFINE_FIELD( m_nGonomeSpitSprite, FIELD_INTEGER ),
	DEFINE_FIELD( m_bGoo, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hSprite, FIELD_EHANDLE ),
END_DATADESC()


void CGonomeSpit::Precache( void )
{
	PrecacheScriptSound( "NPC_BigMomma.SpitTouch1" );
	PrecacheScriptSound( "NPC_BigMomma.SpitHit1" );
	PrecacheScriptSound( "NPC_BigMomma.SpitHit2" );

	PrecacheScriptSound( "Zombie.AttackHit" );
	PrecacheScriptSound( "Zombie.AttackMiss" );
}

void CGonomeSpit:: Spawn( void )
{
	Precache();

	SetMoveType ( MOVETYPE_FLY );
	SetClassname( "squidspit" );
	
	SetSolid( SOLID_BBOX );

	m_nRenderMode = kRenderTransAlpha;
	SetRenderColorA( 255 );
	SetModel( "" );

	if ( m_bGoo )
	{
		SetRenderColor( 255, 255, 255, 255 );
	}
	else
	{
		SetRenderColor( 150, 0, 0, 255 );
	}
	
	UTIL_SetSize( this, Vector( 0, 0, 0), Vector(0, 0, 0) );

	SetCollisionGroup( HL2COLLISION_GROUP_SPIT );
}

void CGonomeSpit::Shoot( CBaseEntity *pOwner, int nGonomeSpitSprite, CSprite * pSprite, Vector vecStart, Vector vecVelocity )
{
	CGonomeSpit *pSpit = CREATE_ENTITY( CGonomeSpit, "squidspit" );
	pSpit->m_nGonomeSpitSprite = nGonomeSpitSprite;
	pSpit->SetOwnerEntity( pOwner );
	pSpit->m_bGoo = (pSpit->GetOwnerEntity() && pSpit->GetOwnerEntity()->IsNPC()) ? pSpit->GetOwnerEntity()->MyNPCPointer()->m_tEzVariant == EZ_VARIANT_RAD : false;
	pSpit->Spawn();
	
	UTIL_SetOrigin( pSpit, vecStart );
	pSpit->SetAbsVelocity( vecVelocity );

	pSpit->SetSprite( pSprite );

	if ( pSprite )
	{
		pSprite->SetAttachment( pSpit, 0 );
		pSprite->SetOwnerEntity( pSpit );

		pSprite->SetScale( 0.75 );
		pSprite->SetTransparency( pSpit->m_nRenderMode, pSpit->m_clrRender->r, pSpit->m_clrRender->g, pSpit->m_clrRender->b, pSpit->m_clrRender->a, pSpit->m_nRenderFX );
	}


	CPVSFilter filter( vecStart );

	VectorNormalize( vecVelocity );
	te->SpriteSpray( filter, 0.0, &vecStart , &vecVelocity, pSpit->m_nGonomeSpitSprite, 210, 25, 15 );
}

void CGonomeSpit::Touch ( CBaseEntity *pOther )
{
	trace_t tr;
	int		iPitch;

	if ( pOther->GetSolidFlags() & FSOLID_TRIGGER )
		 return;

	if ( pOther->GetCollisionGroup() == HL2COLLISION_GROUP_SPIT)
	{
		return;
	}

	// splat sound
	iPitch = random->RandomFloat( 90, 110 );

	EmitSound( "NPC_BigMomma.SpitTouch1" );

	switch ( random->RandomInt( 0, 1 ) )
	{
	case 0:
		EmitSound( "NPC_BigMomma.SpitHit1" );
		break;
	case 1:
		EmitSound( "NPC_BigMomma.SpitHit2" );
		break;
	}

	if ( !pOther->m_takedamage )
	{
		// make a splat on the wall
		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + GetAbsVelocity() * 10, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
		UTIL_DecalTrace(&tr, m_bGoo ? "GlowSplat" : "Blood" ); //BeerSplash

		// make some flecks
		CPVSFilter filter( tr.endpos );

		te->SpriteSpray( filter, 0.0,	&tr.endpos, &tr.plane.normal, m_nGonomeSpitSprite, 30, 8, 5 );

	}
	else
	{
		CTakeDamageInfo info( this, GetOwnerEntity(), sk_zombie_assassin_dmg_spit.GetFloat(), m_bGoo ? DMG_RADIATION : DMG_ACID );
		CalculateBulletDamageForce( &info, GetAmmoDef()->Index("9mmRound"), GetAbsVelocity(), GetAbsOrigin() );
		pOther->TakeDamage( info );
	}

	UTIL_Remove( m_hSprite );
	UTIL_Remove( this );
}


BEGIN_DATADESC(CNPC_Gonome)
	DEFINE_FIELD(m_flBurnDamage, FIELD_FLOAT),
	DEFINE_FIELD(m_flBurnDamageResetTime, FIELD_TIME),
	DEFINE_FIELD(m_nGonomeSpitSprite, FIELD_INTEGER),

	DEFINE_INPUTFUNC( FIELD_VOID, "GoHome", InputGoHome ),
	DEFINE_INPUTFUNC( FIELD_VOID, "GoHomeInstant", InputGoHomeInstant ),

	DEFINE_OUTPUT( m_OnBeastHome, "OnBeastHome" ),
	DEFINE_OUTPUT( m_OnBeastLeaveHome, "OnBeastLeaveHome" ),
END_DATADESC()

//=========================================================
// Spawn
//=========================================================
void CNPC_Gonome::Spawn()
{
	Precache( );
	
	SetModel( STRING( GetModelName() ) );

	SetHullType(HULL_HUMAN);
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );

	if (m_tEzVariant == EZ_VARIANT_RAD)
	{
		SetBloodColor( BLOOD_COLOR_BLUE );
	}
	else
	{
		SetBloodColor( BLOOD_COLOR_YELLOW );
	}
	
	SetRenderColor( 255, 255, 255, 255 );
	
	m_iMaxHealth = IsBoss() ? sk_zombie_assassin_boss_health.GetFloat() : sk_zombie_assassin_health.GetFloat();
	m_iHealth = m_iMaxHealth;
	m_flFieldOfView		= 0.2;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_NPCState			= NPC_STATE_NONE;
	
	CapabilitiesClear();
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_RANGE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK2 );
	CapabilitiesAdd( bits_CAP_MOVE_JUMP | bits_CAP_MOVE_CLIMB );
	CapabilitiesAdd( bits_CAP_OPEN_DOORS | bits_CAP_AUTO_DOORS | bits_CAP_DOORS_GROUP );
	CapabilitiesAdd( bits_CAP_AIM_GUN ); // Technically aiming body; see CNPC_BasePredator::AimGun()
	
	m_fCanThreatDisplay	= TRUE;
	m_flNextSpitTime = gpGlobals->curtime;

	BaseClass::Spawn();

	NPCInit();

	m_flDistTooFar		= MAX_SPIT_DISTANCE;

	// Separate convar for gonome look distance to make them "blind"
	SetDistLook( sk_zombie_assassin_look_dist.GetFloat() );
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_Gonome::Precache()
{
	BaseClass::Precache();
	
	if (GetModelName() == NULL_STRING)
	{
		switch ( m_tEzVariant )
		{
		case EZ_VARIANT_XEN:
			SetModelName( AllocPooledString( "models/xonome.mdl" ) );
			break;
		case EZ_VARIANT_RAD:
			SetModelName( AllocPooledString( "models/glownome.mdl" ) );
			break;
		default:
			SetModelName( AllocPooledString( "models/gonome.mdl" ) );
			break;
		}
	}
	PrecacheModel( STRING( GetModelName() ) );
	


	if ( m_tEzVariant == EZ_VARIANT_RAD )
	{
		PrecacheParticleSystem( "blood_impact_blue_01" );
		m_nGonomeSpitSprite = PrecacheModel( "sprites/glownomespit.vmt" );// spit projectile.
		PrecacheMaterial( "cable/goocable.vmt" );
	}
	else
	{
		m_nGonomeSpitSprite = PrecacheModel( "sprites/gonomespit.vmt" );// spit projectile.
	}

	PrecacheScriptSound( "Gonome.Idle" );
	PrecacheScriptSound( "Gonome.Pain" );
	PrecacheScriptSound( "Gonome.Alert" );
	PrecacheScriptSound( "Gonome.Die" );
	PrecacheScriptSound( "Gonome.Attack" );
	PrecacheScriptSound( "Gonome.Bite" );
	PrecacheScriptSound( "Gonome.Growl" );
	PrecacheScriptSound( "Gonome.FoundEnemy");
	PrecacheScriptSound( "Gonome.RetreatMode");
	PrecacheScriptSound( "Gonome.BerserkMode");
	PrecacheScriptSound( "Gonome.RunFootstepLeft" );
	PrecacheScriptSound( "Gonome.RunFootstepRight" );
	PrecacheScriptSound( "Gonome.FootstepLeft" );
	PrecacheScriptSound( "Gonome.FootstepRight" );
	PrecacheScriptSound( "Gonome.JumpLand" );
	PrecacheScriptSound( "Gonome.Eat" );
	PrecacheScriptSound( "Gonome.BeginSpawnCrab" );
	PrecacheScriptSound( "Gonome.EndSpawnCrab" );

	// Placeholder gib and soundscript
	PrecacheParticleSystem( "glownome_explode" );
	PrecacheScriptSound( "npc_zassassin.kickburst" );
}

//=========================================================
// SetupGlobalModelData
//=========================================================
void CNPC_Gonome::SetupGlobalModelData()
{
	BaseClass::SetupGlobalModelData();

	m_poseArmsOut = LookupPoseParameter( "arms_out" );
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CNPC_Gonome::CreateBehaviors()
{
	AddBehavior( &m_BeastBehavior );

	return BaseClass::CreateBehaviors();
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this NPC can hear the specified sound
//-----------------------------------------------------------------------------
bool CNPC_Gonome::QueryHearSound( CSound *pSound )
{
	// Don't smell dead headcrabs
	if ( pSound->SoundContext() & SOUND_CONTEXT_EXCLUDE_ZOMBIE )
		return false;

	return BaseClass::QueryHearSound( pSound );
}

int CNPC_Gonome::TranslateSchedule( int scheduleType )
{	
	switch	( scheduleType )
	{
		case SCHED_CHASE_ENEMY:

			if (RandomInt( 0, 1 ) == 1 && HasCondition(COND_LIGHT_DAMAGE) && GetMaxHealth() != 0 && GetAbsOrigin().AsVector2D().DistToSqr( GetEnemyLKP().AsVector2D() ) > Square(256.0f))
			{
				return SCHED_GONOME_FLANK_ENEMY;
			}

			return SCHED_GONOME_CHASE_ENEMY;
			break;
		// 1upD - Gonome specific range attack cannot be interrupted by damage
		case SCHED_RANGE_ATTACK1:
			if (m_tBossState == BOSS_STATE_BERSERK) 
			{
				return SCHED_COMBAT_FACE; // Berserk gonomes cannot use a ranged attack
			}
			return SCHED_GONOME_RANGE_ATTACK1;
			break;
		case SCHED_RUN_FROM_ENEMY:
			if (HasCondition( COND_HAVE_ENEMY_LOS ))
			{
				return SCHED_GONOME_SCHED_RUN_FROM_ENEMY;
			}
			break;
	}

	return BaseClass::TranslateSchedule( scheduleType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Gonome::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	int base = BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );

	if ( failedSchedule == SCHED_GONOME_CHASE_ENEMY && GetEnemy() )
	{
		Vector2D vecOrigin = GetAbsOrigin().AsVector2D();
		Vector vecEnemyPos = GetEnemies()->LastSeenPosition( GetEnemy() );
		float flDistSqr = vecOrigin.DistToSqr( vecEnemyPos.AsVector2D() );
		if ( flDistSqr <= Square(224.0f) )
		{
			CHintCriteria hintCriteria;
			hintCriteria.SetHintType( HINT_BEAST_FRUSTRATION );
			hintCriteria.SetFlag( bits_HINT_NODE_NEAREST | bits_HINT_NODE_CLEAR );
			hintCriteria.AddIncludePosition( vecEnemyPos, 96.0f );
			CAI_Hint *pHint = CAI_HintManager::FindHint( this, hintCriteria );

			if (pHint)
			{
				// Dag nabbit!
				SetHintNode( pHint );
				pHint->Lock( this );
				return SCHED_GONOME_FRUSTRATION;
			}
		}
	}

	return base;
}

//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_Gonome::BuildScheduleTestBits()
{
	BaseClass::BuildScheduleTestBits();

	// Beast behavior must be able to interrupt our schedules
	if (!m_BeastBehavior.IsRunning())
	{
		SetCustomInterruptCondition( COND_PROVOKED );
	}

	if ( IsCurSchedule( SCHED_CHASE_ENEMY, true ) )
	{
		// Only interrupt by smell if we should eat in combat
		if ( ShouldEatInCombat() )
		{
			SetCustomInterruptCondition( COND_SMELL );
		}
	}
}

//=========================================================
// Translate missing activities to custom ones
//=========================================================
// Shared activities from base predator
extern int ACT_EAT;
extern int ACT_EXCITED;
extern int ACT_DETECT_SCENT;
extern int ACT_INSPECT_FLOOR;

Activity CNPC_Gonome::NPC_TranslateActivity( Activity eNewActivity )
{
	if (eNewActivity == ACT_EAT)
	{
		return (Activity) ACT_VICTORY_DANCE;
	}
	else if (eNewActivity == ACT_EXCITED)
	{
		return (Activity) ACT_HOP;
	}
	else if (eNewActivity == ACT_DETECT_SCENT)
	{
		return (Activity) ACT_HOP;
	}
	else if (eNewActivity == ACT_INSPECT_FLOOR)
	{
		return (Activity) ACT_IDLE;
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}

//-----------------------------------------------------------------------------
// Purpose: Indicates this monster's place in the relationship table.
// Output : 
//-----------------------------------------------------------------------------
Class_T	CNPC_Gonome::Classify( void )
{
	return CLASS_ZOMBIE; 
}

extern int g_interactionBadCopKick;

//-----------------------------------------------------------------------------
// Purpose: Override to handle player kicks - zassassins are immune
//-----------------------------------------------------------------------------
bool CNPC_Gonome::HandleInteraction( int interactionType, void *data, CBaseCombatCharacter *sourceEnt )
{
	if ( interactionType == g_interactionBadCopKick )
	{
		// If this is a glownome, explode blue goo
		if ( m_tEzVariant == EZ_VARIANT_RAD )
		{
			CTakeDamageInfo info( this, this, 30, DMG_BLAST_SURFACE | DMG_RADIATION );
			RadiusDamage( info, GetAbsOrigin(), 128.0f, CLASS_NONE, this );
			DispatchParticleEffect( "glownome_explode", WorldSpaceCenter(), GetAbsAngles() );
			EmitSound( "npc_zassassin.kickburst" );
			DropGooPuddle( CTakeDamageInfo() );
		}

		// What did you expect was going to happen?
		UpdateEnemyMemory( sourceEnt, sourceEnt->GetAbsOrigin(), sourceEnt );
		return true;
	}

	return BaseClass::HandleInteraction( interactionType, data, sourceEnt );
}

//-----------------------------------------------------------------------------
// Purpose: Override to like Wilson 
//-----------------------------------------------------------------------------
Disposition_t CNPC_Gonome::IRelationType( CBaseEntity *pTarget )
{
	// hackhack - Wilson keeps telling the beast on Bad Cop.
	// For now, Wilson and zombie assassins like each other
	if ( FClassnameIs( pTarget, "npc_wilson" ) )
	{
		return D_LI;
	}

	return BaseClass::IRelationType( pTarget );
}

bool CNPC_Gonome::IsJumpLegal(const Vector &startPos, const Vector &apex, const Vector &endPos) const
{
	const float MAX_JUMP_RISE		= 400.0f;
	const float MAX_JUMP_DISTANCE	= 1024.0f; // Was 800
	const float MAX_JUMP_DROP		= 2048.0f;

	if ( BaseClass::IsJumpLegal( startPos, apex, endPos, MAX_JUMP_RISE, MAX_JUMP_DROP, MAX_JUMP_DISTANCE ) )
	{
		// Hang onto the jump distance. The AI is going to want it.
		m_flJumpDist = (startPos - endPos).Length();

		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Gonome::OnChangeActivity( Activity eNewActivity )
{
	BaseClass::OnChangeActivity( eNewActivity );

	if ( /*(GetActivity() == ACT_RUN || 
		 GetActivity() == ACT_RUN_AIM ||
		 GetActivity() == ACT_WALK) &&*/
		eNewActivity != ACT_IDLE )
	{
		// If we're no longer in the same movement activity, stop the range attack
		if (IsPlayingGesture( ACT_GESTURE_RANGE_ATTACK1 ))
			RemoveGesture( ACT_GESTURE_RANGE_ATTACK1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Gonome::OnStateChange( NPC_STATE OldState, NPC_STATE NewState )
{
	BaseClass::OnStateChange( OldState, NewState );

	if ( NewState == NPC_STATE_SCRIPT )
	{
		// Cancel range attack when entering a script
		if (IsPlayingGesture( ACT_GESTURE_RANGE_ATTACK1 ))
			RemoveGesture( ACT_GESTURE_RANGE_ATTACK1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: turn in the direction of movement
// Output :
//-----------------------------------------------------------------------------
bool CNPC_Gonome::OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval )
{
	if (!HasPoseParameter( GetSequence(), m_poseMove_Yaw ))
	{
		return BaseClass::OverrideMoveFacing( move, flInterval );
	}

	if (IsPlayingGesture( ACT_GESTURE_RANGE_ATTACK1 ))
	{
		return BaseClass::OverrideMoveFacing( move, flInterval );
	}

	if (IsCurSchedule( SCHED_GONOME_FLANK_ENEMY, false ) && GetEnemy())
	{
		// Always face enemy when flanking
		GetMotor()->SetIdealYawToTargetAndUpdate( GetEnemyLKP() );
		return true;
	}

	// required movement direction
	float flMoveYaw = UTIL_VecToYaw( move.dir );
	float idealYaw = UTIL_AngleMod( flMoveYaw );

	if (GetEnemy())
	{
		float flEDist = UTIL_DistApprox2D( WorldSpaceCenter(), GetEnemy()->WorldSpaceCenter() );

		if (flEDist < 512.0)
		{
			float flEYaw = UTIL_VecToYaw( GetEnemy()->WorldSpaceCenter() - WorldSpaceCenter() );

			if (flEDist < 128.0)
			{
				idealYaw = flEYaw;
			}
			else
			{
				idealYaw = flMoveYaw + UTIL_AngleDiff( flEYaw, flMoveYaw ) * (2 - flEDist / 128.0);
			}

			//DevMsg("was %.0f now %.0f\n", flMoveYaw, idealYaw );
		}
	}

	GetMotor()->SetIdealYawAndUpdate( idealYaw );

	// find movement direction to compensate for not being turned far enough
	float fSequenceMoveYaw = GetSequenceMoveYaw( GetSequence() );
	float flDiff = UTIL_AngleDiff( flMoveYaw, GetLocalAngles().y + fSequenceMoveYaw );
	SetPoseParameter( m_poseMove_Yaw, GetPoseParameter( m_poseMove_Yaw ) + flDiff );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the maximum yaw speed based on the monster's current activity.
//-----------------------------------------------------------------------------
float CNPC_Gonome::MaxYawSpeed( void )
{
	if (IsMoving() && HasPoseParameter( GetSequence(), m_poseMove_Yaw ))
	{
		// Make sure we can turn quickly when throwing
		if ( IsPlayingGesture(ACT_GESTURE_RANGE_ATTACK1) )
			return 30;

		return( 15 );
	}
	else
	{
		switch( GetActivity() )
		{
		case ACT_TURN_LEFT:
		case ACT_TURN_RIGHT:
			return 100;
			break;
		case ACT_RUN:
			return 15;
			break;
		case ACT_WALK:
		case ACT_IDLE:
			return 25;
			break;
		case ACT_RANGE_ATTACK1:
		case ACT_RANGE_ATTACK2:
		case ACT_MELEE_ATTACK1:
		case ACT_MELEE_ATTACK2:
			return 120;
		default:
			return 90;
			break;
		}
	}
}

//=========================================================
// IdleSound 
//=========================================================
#define GONOME_ATTN_IDLE	(float)1.5
void CNPC_Gonome::IdleSound( void )
{
	CPASAttenuationFilter filter( this, GONOME_ATTN_IDLE );
	EmitSound( filter, entindex(), "Gonome.Idle" );	
}

//=========================================================
// AttackSound
//=========================================================
void CNPC_Gonome::AttackSound( void )
{
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Gonome.Attack" );	
}

//=========================================================
// GrowlSound
//=========================================================
void CNPC_Gonome::GrowlSound( void )
{
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Gonome.Growl" );
}

//=========================================================
//  RetreatModeSound
//=========================================================
void CNPC_Gonome::RetreatModeSound(void)
{
	CPASAttenuationFilter filter(this);
	EmitSound(filter, entindex(), "Gonome.RetreatMode");
}

//=========================================================
//  BerserkModeSound
//=========================================================
void CNPC_Gonome::BerserkModeSound(void)
{
	CPASAttenuationFilter filter(this);
	EmitSound(filter, entindex(), "Gonome.BerserkMode");
}

//=========================================================
//  EatSound
//=========================================================
void CNPC_Gonome::EatSound( void )
{
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Gonome.Eat" );
}

//=========================================================
//  BeginSpawnSound
//=========================================================
void CNPC_Gonome::BeginSpawnSound( void )
{
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Gonome.BeginSpawnCrab" );
}

//=========================================================
//  EndSpawnSound
//=========================================================
void CNPC_Gonome::EndSpawnSound( void )
{
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Gonome.EndSpawnCrab" );
}

bool CNPC_Gonome::ShouldIgnite( const CTakeDamageInfo &info )
{
 	if ( IsOnFire() )
	{
		// Already burning!
		return false;
	}

	if ( info.GetDamageType() & DMG_BURN )
	{
		//
		// If we take more than ten percent of our health in burn damage within a five
		// second interval, we should catch on fire.
		//
		m_flBurnDamage += info.GetDamage();
		m_flBurnDamageResetTime = gpGlobals->curtime + 5;

		if ( m_flBurnDamage >= m_iMaxHealth * 0.1 )
		{
			Ignite(100.0f);
			return true;
		}
	}

	return false;
}

void CNPC_Gonome::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();

	if ( ( m_flBurnDamageResetTime ) && ( gpGlobals->curtime >= m_flBurnDamageResetTime ) )
	{
		m_flBurnDamage = 0;
	}

	if ( GetEnemy() && HasCondition( COND_SEE_ENEMY ) )
	{
		float flValue = (GetEnemy()->GetAbsOrigin() - GetAbsOrigin()).LengthSqr() / Square( sk_zombie_assassin_armsout_dist.GetFloat() );
		SetPoseParameter( m_poseArmsOut, EdgeLimitPoseParameter( m_poseArmsOut, flValue ) );
	}
	else
	{
		SetPoseParameter( m_poseArmsOut, 1.0f );
	}
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CNPC_Gonome::HandleAnimEvent( animevent_t *pEvent )
{
	if (pEvent->type & AE_TYPE_NEWEVENTSYSTEM)
	{
		if ( pEvent->event == GONOME_AE_LAND )
		{
			trace_t tr;
			UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector(0,0,16), MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &tr );
			if( tr.fraction < 1.0 && tr.m_pEnt )
			{
				surfacedata_t *psurf = physprops->GetSurfaceData( tr.surface.surfaceProps );
				if ( psurf )
				{
					// Shake
					UTIL_ScreenShake( tr.endpos, 10, 150.0, 0.5f, 512, SHAKE_START );

					// Material Sound
					EmitSound_t params;
					//params.m_pSoundName = physprops->GetString( psurf->sounds.impactHard );
					//
					//CPASAttenuationFilter filter( this, params.m_pSoundName );
					//
					//params.m_bWarnOnDirectWaveReference = true;
					//params.m_flVolume = 0.5f;
					//EmitSound( filter, entindex(), params );

					// Land Sound
					params.m_pSoundName = pEvent->options;
					params.m_flVolume = 1.0f;
					CPASAttenuationFilter filter2( this, params.m_pSoundName );
					EmitSound( filter2, entindex(), params );
				}
			}
		}
		else if ( pEvent->event == GONOME_AE_WALL_POUND )
		{
			trace_t		tr;
			Vector		forward;

			GetVectors( &forward, NULL, NULL );

			AI_TraceLine( EyePosition(), EyePosition() + forward * 128, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

			if( tr.fraction == 1.0 )
			{
				// Didn't hit anything!
				return;
			}

			if( tr.m_pEnt )
			{
				const surfacedata_t *psurf = physprops->GetSurfaceData( tr.surface.surfaceProps );
				if( psurf )
				{
					EmitSound( physprops->GetString(psurf->sounds.impactHard) );
				}

				// Shake
				UTIL_ScreenShake( tr.endpos, 2, 35.0, 0.75f, 384, SHAKE_START );

				EmitSound( pEvent->options );

				// HACKHACK: If we have a hint node, hijack OnUser4 to tell it we're striking the wall
				// (for any potential decals or particle effects)
				if (GetHintNode())
				{
					GetHintNode()->FireNamedOutput( "OnUser4", variant_t(), this, this );
				}
			}
		}
		else
		{
			BaseClass::HandleAnimEvent( pEvent );
		}

		return;
	}

	switch( pEvent->event )
	{
		case PREDATOR_AE_SPIT:
		{
			if ( GetEnemy() )
			{
				Vector	vecSpitOffset;
				Vector	vecSpitDir;
				Vector  vRight, vUp, vForward;

				AngleVectors ( GetAbsAngles(), &vForward, &vRight, &vUp );

				// !!!HACKHACK - the spot at which the spit originates (in front of the mouth) was measured in 3ds and hardcoded here.
				// we should be able to read the position of bones at runtime for this info.
				vecSpitOffset = ( vRight * 8 + vForward * 60 + vUp * 50 );		
				vecSpitOffset = ( GetAbsOrigin() + vecSpitOffset );
				vecSpitDir = ( ( GetEnemy()->BodyTarget( GetAbsOrigin() ) ) - vecSpitOffset );

				VectorNormalize( vecSpitDir );

				vecSpitDir.x += random->RandomFloat( -0.05, 0.05 );
				vecSpitDir.y += random->RandomFloat( -0.05, 0.05 );
				vecSpitDir.z += random->RandomFloat( -0.05, 0 );
						
				AttackSound();
				if ( m_tEzVariant == EZ_VARIANT_RAD )
				{
					CGonomeSpit::Shoot( this, m_nGonomeSpitSprite, CSprite::SpriteCreate( "sprites/glownomespit.vmt", GetAbsOrigin(), true ), vecSpitOffset, vecSpitDir * 900 );
				}
				else
				{
					CGonomeSpit::Shoot( this, m_nGonomeSpitSprite, CSprite::SpriteCreate( "sprites/gonomespit.vmt", GetAbsOrigin(), true ), vecSpitOffset, vecSpitDir * 900 );
				}
			}
		}
		break;

		case PREDATOR_AE_BITE:
		{
		// SOUND HERE!
			CPASAttenuationFilter filter( this );
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, Vector(-16,-16,-16), Vector(16,16,16), sk_zombie_assassin_dmg_bite.GetFloat(), DMG_SLASH, 1.0f, ShouldMeleeDamageAnyNPC() );
			if ( pHurt )
			{
				EmitSound( filter, entindex(), "Zombie.AttackHit" );

				// If the player is holding this, make sure it's dropped
				Pickup_ForcePlayerToDropThisObject( pHurt );
			}
			else // Play a random attack miss sound
			{
				EmitSound( filter, entindex(), "Zombie.AttackMiss" );
			}

			// Apply a velocity to hit entity if it is a character or if it has a physics movetype
			if ( pHurt && ShouldApplyHitVelocityToTarget( pHurt ) )
			{
				Vector forward, up;
				AngleVectors( GetAbsAngles(), &forward, NULL, &up );

				if (pHurt->IsNPC())
				{
					Vector vecInteractionDir;
					if ( GetNearestInteractionDir( pHurt->MyNPCPointer(), vecInteractionDir ) )
					{
						// Push the target in the direction of the interaction
						pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() + vecInteractionDir );
					}
					else
						pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() - (forward * 100) );
				}
				else
					pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() - (forward * 100) );

				pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() + (up * 100) );
				pHurt->SetGroundEntity( NULL );
			}

		}
		break;

		case GONOME_AE_TAILWHIP:
		{
			CPASAttenuationFilter filter( this );
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, Vector(-16,-16,-16), Vector(16,16,16), sk_zombie_assassin_dmg_whip.GetFloat(), DMG_SLASH | DMG_ALWAYSGIB, 1.0f, ShouldMeleeDamageAnyNPC() );
			if ( pHurt ) 
			{
				EmitSound( filter, entindex(), "Gonome.Bite" );

				// If the player is holding this, make sure it's dropped
				Pickup_ForcePlayerToDropThisObject( pHurt );

				if ( pHurt->GetFlags() & ( FL_NPC | FL_CLIENT ) )
					pHurt->ViewPunch( QAngle( 20, 0, -20 ) );

				if ( pHurt->MyCombatCharacterPointer() || pHurt->GetMoveType() == MOVETYPE_VPHYSICS )
				{
					Vector right, up;
					AngleVectors( GetAbsAngles(), NULL, &right, &up );

					pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() + (right * 200) );
					pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() + (up * 100) );
				}

				// If we were trying to spawn and hit something by accident, defer spawning for a while
				if (m_bReadyToSpawn)
				{
					PredMsg( UTIL_VarArgs( "npc_zassassin '%s' tried to create a headcrab but hit something instead. Waiting %f seconds before retrying.", GetDebugName(), sk_zombie_assassin_spawn_time.GetFloat() ) );
					m_flNextSpawnTime = gpGlobals->curtime + sk_zombie_assassin_spawn_time.GetFloat();
				}
			} 			
			// Reusing this activity / animation event for headcrab spawning
			else if ( m_bReadyToSpawn )
			{
				Vector forward, up, spawnPos;
				AngleVectors( GetAbsAngles(), &forward, NULL, &up );
				spawnPos = (forward * 32) + (up * 64) + GetAbsOrigin();
				EndSpawnSound();
				if ( SpawnNPC( spawnPos ) )
				{
					PredMsg( "npc_zassassin '%s' created a headcrab!" );
				}
				// Wait a while before retrying
				else
				{
					PredMsg( UTIL_VarArgs("npc_zassassin '%s' failed to create a headcrab. Waiting %f seconds before retrying.", GetDebugName(), sk_zombie_assassin_spawn_time.GetFloat()));
					m_flNextSpawnTime = gpGlobals->curtime + sk_zombie_assassin_spawn_time.GetFloat();
				}
			}

		}
		break;

		case PREDATOR_AE_BLINK:
		{
			// close eye. 
			m_nSkin = 1;
		}
		break;

		case PREDATOR_AE_HOP:
		{
			float flGravity = sv_gravity.GetFloat();

			// throw the squid up into the air on this frame.
			if ( GetFlags() & FL_ONGROUND )
			{
				SetGroundEntity( NULL );
			}

			// jump into air for 0.8 (24/30) seconds
			Vector vecVel = GetAbsVelocity();
			vecVel.z += ( 0.625 * flGravity ) * 0.5;
			SetAbsVelocity( vecVel );
		}
		break;

		case PREDATOR_AE_THROW:
			{
				// squid throws its prey IF the prey is a client. 
				CBaseEntity *pHurt = CheckTraceHullAttack( 70, Vector(-16,-16,-16), Vector(16,16,16), 0, 0 );


				if ( pHurt )
				{
					// croonchy bite sound
					CPASAttenuationFilter filter( this );
					EmitSound( filter, entindex(), "Gonome.Bite" );	

					// screeshake transforms the viewmodel as well as the viewangle. No problems with seeing the ends of the viewmodels.
					UTIL_ScreenShake( pHurt->GetAbsOrigin(), 25.0, 1.5, 0.7, 2, SHAKE_START );

					if ( pHurt->IsPlayer() )
					{
						Vector forward, up;
						AngleVectors( GetAbsAngles(), &forward, NULL, &up );
				
						pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() + forward * 300 + up * 300 );
					}
				}
			}
		break;

		default:
			BaseClass::HandleAnimEvent( pEvent );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Gonome::InputGoHome( inputdata_t &inputdata )
{
	if (!m_BeastBehavior.CanSelectSchedule())
	{
		Warning("%s received GoHome input, but beast behavior can't run! (global might be off)\n", GetDebugName());
		return;
	}

	m_BeastBehavior.GoHome();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Gonome::InputGoHomeInstant( inputdata_t &inputdata )
{
	if (!m_BeastBehavior.CanSelectSchedule())
	{
		Warning("%s received GoHomeInstant input, but beast behavior can't run! (global might be off)\n", GetDebugName());
		return;
	}

	m_BeastBehavior.GoHome( true );
}

//=========================================================
// 
//=========================================================
float CNPC_Gonome::GetMaxSpitWaitTime( void )
{
	return IsCurSchedule( SCHED_GONOME_FLANK_ENEMY, false ) ? 3.0f : 5.0f;
}

float CNPC_Gonome::GetMinSpitWaitTime( void )
{
	return 0.5f;
}

//=========================================================
// Damage for bullsquid whip attack
//=========================================================
float CNPC_Gonome::GetWhipDamage( void )
{
	return sk_zombie_assassin_dmg_whip.GetFloat();
}

//=========================================================
// At what percentage health should this NPC seek food?
//=========================================================
float CNPC_Gonome::GetEatInCombatPercentHealth( void )
{
	return sk_zombie_assassin_eatincombat_percent.GetFloat();
}

// Overriden to play a sound when this NPC is killed by Hammer I/O
bool CNPC_Gonome::BecomeRagdollOnClient(const Vector & force)
{
	CTakeDamageInfo info; // Need this to play death sound
	DeathSound(info);

	// Send achievement information
	SendOnKilledGameEvent( info );

	// Hackhack - This is not an elegant way to do this, but we need Bad Cop to comment on the dead glownome
	// TODO - Perhaps we can pass this through the input somehow because the player should be the activator
	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if ( pPlayer )
	{
		CEZ2_Player *pEZ2Player = assert_cast<CEZ2_Player*>(pPlayer);
		pEZ2Player->Event_KilledEnemy( this, info );
	}

	return BaseClass::BecomeRagdollOnClient(force);
}

void CNPC_Gonome::RemoveIgnoredConditions( void )
{
	BaseClass::RemoveIgnoredConditions();

	if ( GetActivity() == ACT_MELEE_ATTACK1 )
	{
		ClearCondition( COND_LIGHT_DAMAGE );
		ClearCondition( COND_HEAVY_DAMAGE );
	}

	if ( GetActivity() == ACT_MELEE_ATTACK2 )
	{
		ClearCondition( COND_LIGHT_DAMAGE );
		ClearCondition( COND_HEAVY_DAMAGE );
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

//=========================================================
// TakeDamage - overridden for gonome so we can keep track
// of how much time has passed since it was last injured
//=========================================================
int CNPC_Gonome::OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo info = inputInfo;

	if( inputInfo.GetDamageType() & DMG_BURN )
	{
		// If a zombie is on fire it only takes damage from the fire that's attached to it. (DMG_DIRECT)
		// This is to stop zombies from burning to death 10x faster when they're standing around
		// 10 fire entities.
		if( IsOnFire() && !(inputInfo.GetDamageType() & DMG_DIRECT) )
		{
			return 0;
		}
		
		Scorch( 8, 50 );
		Ignite( 100.0f );
	}

	if ( ShouldIgnite( info ) )
	{
		Ignite( 100.0f );
	}

	if ( info.GetDamageType() == DMG_BULLET )
	{
		// // Push the Gonome back based on damage received
		// Vector vecDir = GetAbsOrigin() - info.GetInflictor()->WorldSpaceCenter();
		// VectorNormalize( vecDir );
		// float flForce = DamageForce( WorldAlignSize(), info.GetDamage() );
		// SetAbsVelocity( GetAbsVelocity() + vecDir * flForce );
		info.ScaleDamage( 0.25f );
	}

	// If this is a slimy zombie, it does not take damage from radiation
	if ( m_tEzVariant == EZ_VARIANT_RAD && ( info.GetDamageType() & DMG_RADIATION ) ) {
		return 0;
	}

	return BaseClass::OnTakeDamage_Alive ( inputInfo );
}

//=========================================================
// OnFed - Handles eating food and regenerating health
//	Fires the output m_OnFed
//=========================================================
void CNPC_Gonome::OnFed()
{
	// Can produce a crab if capable of spawning one
	m_bReadyToSpawn = m_bSpawningEnabled;
	m_flNextSpawnTime = gpGlobals->curtime + sk_zombie_assassin_spawn_time.GetFloat();

	// Spawn 3 crabs per meal
	m_iTimesFed += 2;

	BaseClass::OnFed();
}

void CNPC_Gonome::Ignite( float flFlameLifetime, bool bNPCOnly, float flSize, bool bCalledByLevelDesigner )
{
	BaseClass::Ignite( flFlameLifetime, bNPCOnly, flSize, bCalledByLevelDesigner );

	// Set the zombie up to burn to death in about ten seconds.
	if ( !IsBoss() )
	{
		SetHealth( MIN( m_iHealth, FLAME_DIRECT_DAMAGE_PER_SEC * (ZOMBIE_BURN_TIME + random->RandomFloat( -ZOMBIE_BURN_TIME_NOISE, ZOMBIE_BURN_TIME_NOISE )) ) );
	}

	Activity activity = GetActivity();
	Activity burningActivity = activity;

	if ( activity == ACT_WALK )
	{
		burningActivity = ACT_WALK_ON_FIRE;
	}
	else if ( activity == ACT_RUN )
	{
		burningActivity = ACT_RUN_ON_FIRE;
	}
	else if ( activity == ACT_IDLE )
	{
		burningActivity = ACT_IDLE_ON_FIRE;
	}

	if( HaveSequenceForActivity(burningActivity) )
	{
		// Make sure we have a sequence for this activity (torsos don't have any, for instance) 
		// to prevent the baseNPC & baseAnimating code from throwing red level errors.
		SetActivity( burningActivity );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create a new baby bullsquid
// Output : True if the new bullsquid is created
//-----------------------------------------------------------------------------
bool CNPC_Gonome::SpawnNPC( const Vector position )
{
	// Try to create entity
	CAI_BaseNPC *pChild = dynamic_cast< CAI_BaseNPC * >(CreateEntityByName( "npc_headcrab" ));
	// Make a second crab to explode
	CAI_BaseNPC *pChild2 = dynamic_cast< CAI_BaseNPC * >(CreateEntityByName( "npc_headcrab" ));
	if (pChild)
	{
		pChild->m_tEzVariant = this->m_tEzVariant;
		pChild->AddSpawnFlags( SF_NPC_FALL_TO_GROUND );
		pChild2->AddSpawnFlags( SF_NPC_FALL_TO_GROUND );
		pChild->Precache();

		DispatchSpawn( pChild );
		DispatchSpawn( pChild2 );


		// Now attempt to drop into the world
		pChild->Teleport( &position, NULL, NULL );

		// Now check that this is a valid location for the new npc to be
		Vector	vUpBit = pChild->GetAbsOrigin();
		vUpBit.z += 1;

		trace_t tr;
		AI_TraceHull( pChild->GetAbsOrigin(), vUpBit, pChild->GetHullMins(), pChild->GetHullMaxs(),
			MASK_NPCSOLID, pChild, COLLISION_GROUP_NONE, &tr );
		if (tr.startsolid || (tr.fraction < 1.0))
		{
			pChild->SUB_Remove();
			pChild2->SUB_Remove();
			DevMsg( "Can't create baby headcrab. Bad Position!\n" );
			return false;
		}

		pChild2->Teleport( &position, NULL, NULL );

		if ( this->GetSquad() != NULL )
		{
			this->GetSquad()->AddToSquad( pChild );
		}
		pChild->Activate();
		pChild2->TakeDamage( CTakeDamageInfo( this, this, pChild2->GetHealth(), DMG_CRUSH | DMG_ALWAYSGIB ) );

		// Decrement feeding counter
		m_iTimesFed--;
		if ( m_iTimesFed <= 0 )
		{
			m_bReadyToSpawn = false;
		}
		else 
		{
			m_flNextSpawnTime = gpGlobals->curtime + 0.25f;
		}

		// Fire output
		variant_t value;
		value.SetEntity( pChild );
		m_OnSpawnNPC.CBaseEntityOutput::FireOutput( value, this, this );

		return true;
	}

	// Couldn't instantiate NPC
	return false;
}

//=========================================================
// Start task - selects the correct activity and performs
// any necessary calculations to start the next task on the
// schedule. 
//
// Overridden for predators to play specific activities
//=========================================================
void CNPC_Gonome::StartTask( const Task_t *pTask )
{
	switch (pTask->iTask)
	{
	case TASK_PREDATOR_SPAWN:
	{
		SetIdealActivity( (Activity)ACT_MELEE_ATTACK1 );
		break;
	}
	case TASK_GONOME_GET_FLANK_PATH:
	{
		// First, try getting a wide arc path
		ChainStartTask( TASK_GET_FLANK_ARC_PATH_TO_ENEMY_LOS, 90.0f );

		if (TaskIsComplete())
			break;

		// If that didn't work, try getting a node within radius
		ChainStartTask( TASK_GET_FLANK_RADIUS_PATH_TO_ENEMY_LOS, 150.0f );

		if (TaskIsComplete())
			break;

		// Finally, just try taking cover
		ChainStartTask( TASK_FIND_NODE_COVER_FROM_ENEMY, 0.0f );

		break;
	}
	default:
	{
		BaseClass::StartTask( pTask );
		break;
	}
	}
}

//=========================================================
// RunTask
//=========================================================
void CNPC_Gonome::RunTask ( const Task_t *pTask )
{
	switch (pTask->iTask)
	{
	case TASK_PREDATOR_SPAWN:
	{
		// If we fall in this case, end the task when the activity ends
		if (IsActivityFinished())
		{
			TaskComplete();
		}
		break;
	}
	case TASK_WAIT_FOR_MOVEMENT:
	{
		if (HasCondition( COND_CAN_RANGE_ATTACK1 ) && GetEnemy())
		{
			// Try throwing while moving
			int iLayer = AddGesture( ACT_GESTURE_RANGE_ATTACK1 );

			AddFacingTarget( GetEnemy(), 1.0f, GetLayerDuration( iLayer ) );
		}
		BaseClass::RunTask( pTask );
		break;
	}
	default:
	{
		BaseClass::RunTask( pTask );
		break;
	}
	}
}


//------------------------------------------------------------------------------
//
// Schedules
//
//------------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( monster_gonome, CNPC_Gonome )

	DECLARE_ANIMEVENT( GONOME_AE_LAND )
	DECLARE_ANIMEVENT( GONOME_AE_WALL_POUND )

	DECLARE_TASK( TASK_GONOME_GET_FLANK_PATH )
		
	//=========================================================
	// > SCHED_GONOME_CHASE_ENEMY
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GONOME_CHASE_ENEMY,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_RUN_RANDOM" // If the path to a target is blocked, run randomly
		"		TASK_GET_PATH_TO_ENEMY			0"
		"		TASK_RUN_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"	"
		"	Interrupts"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		//"		COND_SMELL"
		//"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_TASK_FAILED"
		"		COND_NEW_BOSS_STATE"
	)

	//===============================================
	//	> SCHED_GONOME_RANGE_ATTACK1
	//===============================================
	DEFINE_SCHEDULE
	(
		SCHED_GONOME_RANGE_ATTACK1,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_RANGE_ATTACK1		0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		//"		COND_LIGHT_DAMAGE"
		//"		COND_HEAVY_DAMAGE"
		"		COND_ENEMY_OCCLUDED"
		//"		COND_NO_PRIMARY_AMMO"
		//"		COND_HEAR_DANGER"
		"		COND_WEAPON_BLOCKED_BY_FRIEND"
		"		COND_WEAPON_SIGHT_OCCLUDED"
		"		COND_NEW_BOSS_STATE"
	)

	//===============================================
	//	> SCHED_GONOME_SCHED_RUN_FROM_ENEMY
	//===============================================
	DEFINE_SCHEDULE
	(
		SCHED_GONOME_SCHED_RUN_FROM_ENEMY,
		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE					SCHEDULE:SCHED_RUN_FROM_ENEMY"
		"		TASK_STOP_MOVING						0"
		"		TASK_FIND_FAR_NODE_COVER_FROM_ENEMY		2048" // Run at least 2048 units from the enemy
		"		TASK_RUN_PATH							0"
		"		TASK_WAIT_FOR_MOVEMENT					0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
	)

	DEFINE_SCHEDULE
	( 
		SCHED_GONOME_FRUSTRATION,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_RUN_RANDOM"
		"		TASK_REMEMBER				MEMORY:LOCKED_HINT"
		"		TASK_GET_PATH_TO_HINTNODE	0"
		"		TASK_RUN_PATH				0"
		"		TASK_WAIT_FOR_MOVEMENT		0"
		"		TASK_STOP_MOVING			0"
		"		TASK_PLAY_HINT_ACTIVITY		0"
		"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_RUN_RANDOM"
		""
		"	Interrupts"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
	)
		
	//=========================================================
	// > SCHED_GONOME_FLANK_ENEMY
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_GONOME_FLANK_ENEMY,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_GONOME_CHASE_ENEMY"
		"		TASK_GONOME_GET_FLANK_PATH		0"
		"		TASK_RUN_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"	"
		"	Interrupts"
		//"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		//"		COND_SMELL"
		//"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_TASK_FAILED"
		"		COND_NEW_BOSS_STATE"
	)

AI_END_CUSTOM_NPC()
