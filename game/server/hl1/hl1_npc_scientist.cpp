//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Bullseyes act as targets for other NPC's to attack and to trigger
//			events 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#include	"cbase.h"
#include	"ai_default.h"
#include	"ai_task.h"
#include	"ai_schedule.h"
#include	"ai_node.h"
#include	"ai_hull.h"
#include	"ai_hint.h"
#include	"ai_memory.h"
#include	"ai_route.h"
#include	"ai_motor.h"
#include	"hl1_npc_scientist.h"
#include	"soundent.h"
#include	"game.h"
#include	"npcevent.h"
#include	"entitylist.h"
#include	"activitylist.h"
#include	"animation.h"
#include	"engine/IEngineSound.h"
#include	"ai_navigator.h"
#include	"ai_behavior_follow.h"
#include	"AI_Criteria.h"
#include	"SoundEmitterSystem/isoundemittersystembase.h"

#define SC_PLFEAR	"SC_PLFEAR"
#define SC_FEAR		"SC_FEAR"
#define SC_HEAL		"SC_HEAL"
#define SC_SCREAM	"SC_SCREAM"
#define SC_POK		"SC_POK"

ConVar	sk_scientist_health( "sk_scientist_health","20");
ConVar	sk_scientist_heal( "sk_scientist_heal","25");

#define		NUM_SCIENTIST_HEADS		4 // four heads available for scientist model
enum { HEAD_GLASSES = 0, HEAD_EINSTEIN = 1, HEAD_LUTHER = 2, HEAD_SLICK = 3 };


int ACT_EXCITED;

//=========================================================
// Makes it fast to check barnacle classnames in 
// IsValidEnemy()
//=========================================================
string_t	s_iszBarnacleClassname;

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		SCIENTIST_AE_HEAL		( 1 )
#define		SCIENTIST_AE_NEEDLEON	( 2 )
#define		SCIENTIST_AE_NEEDLEOFF	( 3 )

//=======================================================
// Scientist
//=======================================================

LINK_ENTITY_TO_CLASS( monster_scientist, CNPC_Scientist );

//IMPLEMENT_SERVERCLASS_ST( CNPC_Scientist, DT_NPC_Scientist )
//END_SEND_TABLE()


//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_Scientist )
	DEFINE_FIELD( m_flFearTime, FIELD_TIME ),
	DEFINE_FIELD( m_flHealTime, FIELD_TIME ),
	DEFINE_FIELD( m_flPainTime, FIELD_TIME ),

	DEFINE_THINKFUNC( SUB_LVFadeOut ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CNPC_Scientist::Precache( void )
{
	PrecacheModel( "models/scientist.mdl" );

	PrecacheScriptSound( "Scientist.Pain" );

	TalkInit();
	
	BaseClass::Precache();
}

void CNPC_Scientist::ModifyOrAppendCriteria( AI_CriteriaSet& criteriaSet )
{
	BaseClass::ModifyOrAppendCriteria( criteriaSet );

	bool predisaster = FBitSet( m_spawnflags, SF_NPC_PREDISASTER ) ? true : false;

	criteriaSet.AppendCriteria( "disaster", predisaster ? "[disaster::pre]" : "[disaster::post]" );
}

// Init talk data
void CNPC_Scientist::TalkInit()
{
	
	BaseClass::TalkInit();

	// scientist will try to talk to friends in this order:

	m_szFriends[0] = "monster_scientist";
	m_szFriends[1] = "monster_sitting_scientist";
	m_szFriends[2] = "monster_barney";

	// get voice for head
	switch (m_nBody % 3)
	{
	default:
	case HEAD_GLASSES:	GetExpresser()->SetVoicePitch( 105 );	break;	//glasses
	case HEAD_EINSTEIN: GetExpresser()->SetVoicePitch( 100 );	break;	//einstein
	case HEAD_LUTHER:	GetExpresser()->SetVoicePitch( 95 );	break;	//luther
	case HEAD_SLICK:	GetExpresser()->SetVoicePitch( 100 );	break;//slick
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CNPC_Scientist::Spawn( void )
{

	//Select the body first if it's going to be random cause we set his voice pitch in Precache.
	if ( m_nBody == -1 )
		 m_nBody = random->RandomInt( 0, NUM_SCIENTIST_HEADS-1 );// pick a head, any head
	

	SetRenderColor( 255, 255, 255, 255 );
	
	Precache();

	SetModel( "models/scientist.mdl" );

	SetHullType(HULL_HUMAN);
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	m_bloodColor		= BLOOD_COLOR_RED;
	ClearEffects();
	m_iHealth			= sk_scientist_health.GetFloat();
	m_flFieldOfView		= VIEW_FIELD_WIDE;
	m_NPCState			= NPC_STATE_NONE;

	CapabilitiesClear();
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_OPEN_DOORS | bits_CAP_AUTO_DOORS | bits_CAP_USE );
	CapabilitiesAdd( bits_CAP_TURN_HEAD | bits_CAP_ANIMATEDFACE );

	// White hands
	m_nSkin = 0;

	
	// Luther is black, make his hands black
	if ( m_nBody == HEAD_LUTHER )
		 m_nSkin = 1;
	
	NPCInit();

	SetUse( &CNPC_Scientist::FollowerUse );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Scientist::Activate()
{
	s_iszBarnacleClassname = FindPooledString( "monster_barnacle" );
	BaseClass::Activate();
}


//-----------------------------------------------------------------------------
// Purpose: 
//
//
// Output : 
//-----------------------------------------------------------------------------
Class_T	CNPC_Scientist::Classify( void )
{
	return	CLASS_HUMAN_PASSIVE;
}

int CNPC_Scientist::GetSoundInterests ( void )
{
	return	SOUND_WORLD	|
			SOUND_COMBAT	|
			SOUND_DANGER	|
			SOUND_PLAYER;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CNPC_Scientist::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{		
	case SCIENTIST_AE_HEAL:		// Heal my target (if within range)
		Heal();
		break;
	case SCIENTIST_AE_NEEDLEON:
	{
		int oldBody = m_nBody;
		m_nBody = (oldBody % NUM_SCIENTIST_HEADS) + NUM_SCIENTIST_HEADS * 1;
	}
		break;
	case SCIENTIST_AE_NEEDLEOFF:
	{
		int oldBody = m_nBody;
		m_nBody = (oldBody % NUM_SCIENTIST_HEADS) + NUM_SCIENTIST_HEADS * 0;
	}
		break;

	default:
		BaseClass::HandleAnimEvent( pEvent );
		break;
	}
}

void CNPC_Scientist::DeclineFollowing( void )
{
	if ( CanSpeakAfterMyself() )
	{
		Speak( SC_POK );
	}
}

bool CNPC_Scientist::CanBecomeRagdoll( void )
{
	if ( UTIL_IsLowViolence() )
	{
		return false;
	}

	return BaseClass::CanBecomeRagdoll();
}

bool CNPC_Scientist::ShouldGib( const CTakeDamageInfo &info )
{
	if ( UTIL_IsLowViolence() )
	{
		return false;
	}

	return BaseClass::ShouldGib( info );
}

void CNPC_Scientist::SUB_StartLVFadeOut( float delay, bool notSolid )
{
	SetThink( &CNPC_Scientist::SUB_LVFadeOut );
	SetNextThink( gpGlobals->curtime + delay );
	SetRenderColorA( 255 );
	m_nRenderMode = kRenderNormal;

	if ( notSolid )
	{
		AddSolidFlags( FSOLID_NOT_SOLID );
		SetLocalAngularVelocity( vec3_angle );
	}
}

void CNPC_Scientist::SUB_LVFadeOut( void  )
{
	if( VPhysicsGetObject() )
	{
		if( VPhysicsGetObject()->GetGameFlags() & FVPHYSICS_PLAYER_HELD || GetEFlags() & EFL_IS_BEING_LIFTED_BY_BARNACLE )
		{
			// Try again in a few seconds.
			SetNextThink( gpGlobals->curtime + 5 );
			SetRenderColorA( 255 );
			return;
		}
	}

	float dt = gpGlobals->frametime;
	if ( dt > 0.1f )
	{
		dt = 0.1f;
	}
	m_nRenderMode = kRenderTransTexture;
	int speed = MAX(3,256*dt); // fade out over 3 seconds
	SetRenderColorA( UTIL_Approach( 0, m_clrRender->a, speed ) );
	NetworkStateChanged();

	if ( m_clrRender->a == 0 )
	{
		UTIL_Remove(this);
	}
	else
	{
		SetNextThink( gpGlobals->curtime );
	}
}

void CNPC_Scientist::Scream( void )
{
	if ( IsOkToSpeak() )
	{
		GetExpresser()->BlockSpeechUntil( gpGlobals->curtime + 10 );
		SetSpeechTarget( GetEnemy() );
		Speak( SC_SCREAM );
	}
}

Activity CNPC_Scientist::GetStoppedActivity( void )
{ 
	if ( GetEnemy() != NULL ) 
		return (Activity)ACT_EXCITED;
	
	return BaseClass::GetStoppedActivity();
}

float CNPC_Scientist::MaxYawSpeed( void )
{
	switch( GetActivity() )
	{
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		return 160;
		break;
	case ACT_RUN:
		return 160;
		break;
	default:
		return 60;
		break;
	}
}

void CNPC_Scientist::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_SAY_HEAL:

		GetExpresser()->BlockSpeechUntil( gpGlobals->curtime + 2 );
		SetSpeechTarget( GetTarget() );
		Speak( SC_HEAL );

		TaskComplete();
		break;
	 
	case TASK_SCREAM:
		Scream();
		TaskComplete();
		break;

	case TASK_RANDOM_SCREAM:
		if ( random->RandomFloat( 0, 1 ) < pTask->flTaskData )
			Scream();
		TaskComplete();
		break;

	case TASK_SAY_FEAR:
		if ( IsOkToSpeak() )
		{
			GetExpresser()->BlockSpeechUntil( gpGlobals->curtime + 2 );
			SetSpeechTarget( GetEnemy() );
			if ( GetEnemy() && GetEnemy()->IsPlayer() )
				Speak( SC_PLFEAR );
			else
				Speak( SC_FEAR );
		}
		TaskComplete();
		break;

	case TASK_HEAL:
		SetIdealActivity( ACT_MELEE_ATTACK1 );
		break;

	case TASK_RUN_PATH_SCARED:
		GetNavigator()->SetMovementActivity( ACT_RUN_SCARED );
		break;

	case TASK_MOVE_TO_TARGET_RANGE_SCARED:
		{
			if ( GetTarget() == NULL)
			{
				TaskFail(FAIL_NO_TARGET);
			}
			else if ( (GetTarget()->GetAbsOrigin() - GetAbsOrigin()).Length() < 1 )
			{
				TaskComplete();
			}
		}
		break;

	default:
		BaseClass::StartTask( pTask );
		break;
	}
}

void CNPC_Scientist::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_RUN_PATH_SCARED:
		if ( !IsMoving() )
			TaskComplete();
		if ( random->RandomInt(0,31) < 8 )
			Scream();
		break;

	case TASK_MOVE_TO_TARGET_RANGE_SCARED:
		{
			float distance;

			if ( GetTarget() == NULL )
			{
				TaskFail(FAIL_NO_TARGET);
			}
			else
			{
				distance = ( GetNavigator()->GetPath()->ActualGoalPosition() - GetAbsOrigin() ).Length2D();
				// Re-evaluate when you think your finished, or the target has moved too far
				if ( (distance < pTask->flTaskData) || (GetNavigator()->GetPath()->ActualGoalPosition() - GetTarget()->GetAbsOrigin()).Length() > pTask->flTaskData * 0.5 )
				{
					GetNavigator()->GetPath()->ResetGoalPosition(GetTarget()->GetAbsOrigin());
					distance = ( GetNavigator()->GetPath()->ActualGoalPosition() - GetAbsOrigin() ).Length2D();
//					GetNavigator()->GetPath()->Find();
					GetNavigator()->SetGoal( GOALTYPE_TARGETENT );
				}

				// Set the appropriate activity based on an overlapping range
				// overlap the range to prevent oscillation
				// BUGBUG: this is checking linear distance (ie. through walls) and not path distance or even visibility
				if ( distance < pTask->flTaskData )
				{
					TaskComplete();
					GetNavigator()->GetPath()->Clear();		// Stop moving
				}
				else
				{
					if ( distance < 190 && GetNavigator()->GetMovementActivity() != ACT_WALK_SCARED )
						GetNavigator()->SetMovementActivity( ACT_WALK_SCARED );
					else if ( distance >= 270 && GetNavigator()->GetMovementActivity() != ACT_RUN_SCARED )
						GetNavigator()->SetMovementActivity( ACT_RUN_SCARED );
				}
			}
		}
		break;

	case TASK_HEAL:
		if ( IsSequenceFinished() )
		{
			TaskComplete();
		}
		else
		{
			if ( TargetDistance() > 90 )
				TaskComplete();

			if ( GetTarget() )
				 GetMotor()->SetIdealYaw( UTIL_VecToYaw( GetTarget()->GetAbsOrigin() - GetAbsOrigin() ) );

			//GetMotor()->SetYawSpeed( m_YawSpeed );
		}
		break;
	default:
		BaseClass::RunTask( pTask );
		break;
	}
}

int CNPC_Scientist::OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo )
{

	if ( inputInfo.GetInflictor() && inputInfo.GetInflictor()->GetFlags() & FL_CLIENT )
	{
		Remember( bits_MEMORY_PROVOKED );
		StopFollowing();
	}

	// make sure friends talk about it if player hurts scientist...
	return BaseClass::OnTakeDamage_Alive( inputInfo );
}

void CNPC_Scientist::Event_Killed( const CTakeDamageInfo &info )
{
	SetUse( NULL );	
	BaseClass::Event_Killed( info );

	if ( UTIL_IsLowViolence() )
	{
		SUB_StartLVFadeOut( 0.0f );
	}
}

bool CNPC_Scientist::CanHeal( void )
{ 
	CBaseEntity *pTarget = GetFollowTarget();

	if ( pTarget == NULL )
		 return false;

	if ( pTarget->IsPlayer() == false )
		 return false;

	if ( (m_flHealTime > gpGlobals->curtime) || (pTarget->m_iHealth > (pTarget->m_iMaxHealth * 0.5)) )
		return false;

	return true;
}

//=========================================================
// PainSound
//=========================================================
void CNPC_Scientist::PainSound ( const CTakeDamageInfo &info )
{
	if (gpGlobals->curtime < m_flPainTime )
		return;
	
	m_flPainTime = gpGlobals->curtime + random->RandomFloat( 0.5, 0.75 );

	CPASAttenuationFilter filter( this );

	CSoundParameters params;
	if ( GetParametersForSound( "Scientist.Pain", params, NULL ) )
	{
		EmitSound_t ep( params );
		params.pitch = GetExpresser()->GetVoicePitch();

		EmitSound( filter, entindex(), ep );
	}
}

//=========================================================
// DeathSound 
//=========================================================
void CNPC_Scientist::DeathSound( const CTakeDamageInfo &info )
{
	PainSound( info );
}


void CNPC_Scientist::Heal( void )
{
	if ( !CanHeal() )
		  return;

	Vector target = GetFollowTarget()->GetAbsOrigin() - GetAbsOrigin();
	if ( target.Length() > 100 )
		return;

	GetTarget()->TakeHealth( sk_scientist_heal.GetFloat(), DMG_GENERIC );
	// Don't heal again for 1 minute
	m_flHealTime = gpGlobals->curtime + 60;
}

int CNPC_Scientist::TranslateSchedule( int scheduleType )
{
	switch( scheduleType )
	{
	// Hook these to make a looping schedule
	case SCHED_TARGET_FACE:
		{
			int baseType;

			// call base class default so that scientist will talk
			// when 'used' 
			baseType = BaseClass::TranslateSchedule( scheduleType );

			if (baseType == SCHED_IDLE_STAND)
				return SCHED_TARGET_FACE;	// override this for different target face behavior
			else
				return baseType;
		}
		break;

	case SCHED_TARGET_CHASE:
		return SCHED_SCI_FOLLOWTARGET;
		break;

	case SCHED_IDLE_STAND:
		{
			int baseType;
				
			baseType = BaseClass::TranslateSchedule( scheduleType );

			if (baseType == SCHED_IDLE_STAND)
				return SCHED_SCI_IDLESTAND;	// override this for different target face behavior
			else
				return baseType;
		}
		break;
	}

	return BaseClass::TranslateSchedule( scheduleType );
}

Activity CNPC_Scientist::NPC_TranslateActivity( Activity newActivity )
{
	if ( GetFollowTarget() && GetEnemy() )
	{
		CBaseEntity *pEnemy = GetEnemy();

		int relationship = D_NU;

		// Nothing scary, just me and the player
		if ( pEnemy != NULL )
			 relationship = IRelationType( pEnemy );

		if ( relationship == D_HT || relationship == D_FR )
		{
			if ( newActivity == ACT_WALK )
				 return ACT_WALK_SCARED;
			else if ( newActivity == ACT_RUN )
				 return ACT_RUN_SCARED;
		}
	}

	return BaseClass::NPC_TranslateActivity( newActivity );
}

int CNPC_Scientist::SelectSchedule( void )
{
	if( m_NPCState == NPC_STATE_PRONE )
	{
		// Immediately call up to the talker code. Barnacle death is priority schedule.
		return BaseClass::SelectSchedule();
	}

	// so we don't keep calling through the EHANDLE stuff
	CBaseEntity *pEnemy = GetEnemy();

	if ( GetFollowTarget() )
	{
		// so we don't keep calling through the EHANDLE stuff
		CBaseEntity *pEnemy = GetEnemy();

		int relationship = D_NU;

		// Nothing scary, just me and the player
		if ( pEnemy != NULL )
			 relationship = IRelationType( pEnemy );

		if ( relationship != D_HT && relationship != D_FR )
		{
			// If I'm already close enough to my target
			if ( TargetDistance() <= 128 )
			{
				if ( CanHeal() )	// Heal opportunistically
				{
					SetTarget( GetFollowTarget() );
					return SCHED_SCI_HEAL;
				}
			}
		}
	}
	else if ( HasCondition( COND_PLAYER_PUSHING ) && !(GetSpawnFlags() & SF_NPC_PREDISASTER ) )
	{		// Player wants me to move
			return SCHED_HL1TALKER_FOLLOW_MOVE_AWAY;
	}

	if ( BehaviorSelectSchedule() )
	{
		return BaseClass::SelectSchedule();
	}

	
	
	if ( HasCondition( COND_HEAR_DANGER ) && m_NPCState != NPC_STATE_PRONE )
	{
		CSound *pSound;
		pSound = GetBestSound();

		if ( pSound && pSound->IsSoundType(SOUND_DANGER) )
			return SCHED_TAKE_COVER_FROM_BEST_SOUND;
	}

	switch( m_NPCState )
	{

	case NPC_STATE_ALERT:	
	case NPC_STATE_IDLE:

		if ( pEnemy )
		{
			if ( HasCondition( COND_SEE_ENEMY ) )
				m_flFearTime = gpGlobals->curtime;
			else if ( DisregardEnemy( pEnemy ) )		// After 15 seconds of being hidden, return to alert
			{
				SetEnemy( NULL );
				pEnemy = NULL;
			}
		}

		if ( HasCondition( COND_LIGHT_DAMAGE ) ||  HasCondition( COND_HEAVY_DAMAGE ) )
		{
			// flinch if hurt
			return SCHED_SMALL_FLINCH;
		}
	
		// Cower when you hear something scary
		if ( HasCondition( COND_HEAR_DANGER ) || HasCondition( COND_HEAR_COMBAT ) ) 
		{
			CSound *pSound;
			pSound = GetBestSound();
		
			if ( pSound )
			{
				if ( pSound->IsSoundType(SOUND_DANGER | SOUND_COMBAT) )
				{
					if ( gpGlobals->curtime - m_flFearTime > 3 )	// Only cower every 3 seconds or so
					{
						m_flFearTime = gpGlobals->curtime;		// Update last fear
						return SCHED_SCI_STARTLE;	// This will just duck for a second
					}
				}
			}
		}
		
		if ( GetFollowTarget() )
		{
			if ( !GetFollowTarget()->IsAlive() )
			{
				// UNDONE: Comment about the recently dead player here?
				StopFollowing();
				break;
			}
			
			int relationship = D_NU;

			// Nothing scary, just me and the player
			if ( pEnemy != NULL )
				 relationship = IRelationType( pEnemy );
			
			if ( relationship != D_HT )
			{
				return SCHED_TARGET_FACE;	// Just face and follow.
			}
			else	// UNDONE: When afraid, scientist won't move out of your way.  Keep This?  If not, write move away scared
			{
				if ( HasCondition( COND_NEW_ENEMY ) ) // I just saw something new and scary, react
					return SCHED_SCI_FEAR;					// React to something scary
				return SCHED_SCI_FACETARGETSCARED;	// face and follow, but I'm scared!
			}
		}

		// try to say something about smells
		TrySmellTalk();
		break;


	case NPC_STATE_COMBAT:

		if ( HasCondition( COND_NEW_ENEMY ) )
			return SCHED_SCI_FEAR;					// Point and scream!
		if ( HasCondition( COND_SEE_ENEMY ) )
			return SCHED_SCI_COVER;		// Take Cover
		
		if ( HasCondition( COND_HEAR_COMBAT ) || HasCondition( COND_HEAR_DANGER ) )
			 return SCHED_TAKE_COVER_FROM_BEST_SOUND;	// Cower and panic from the scary sound!
	
		return SCHED_SCI_COVER;			// Run & Cower
		break;
	}

	return BaseClass::SelectSchedule();
}

NPC_STATE CNPC_Scientist::SelectIdealState ( void )
{
	switch ( m_NPCState )
	{
	case NPC_STATE_ALERT:
	case NPC_STATE_IDLE:
		if ( HasCondition( COND_NEW_ENEMY ) )
		{
			if ( GetFollowTarget() && GetEnemy() )
			{
				int relationship = IRelationType( GetEnemy() );
				if ( relationship != D_FR || relationship != D_HT && ( !HasCondition( COND_LIGHT_DAMAGE  ) || !HasCondition( COND_HEAVY_DAMAGE ) ) )
				{
					// Don't go to combat if you're following the player
					return NPC_STATE_ALERT;
				}
				StopFollowing();
			}
		}
		else if ( HasCondition( COND_LIGHT_DAMAGE  ) || HasCondition( COND_HEAVY_DAMAGE ) ) 
		{
			// Stop following if you take damage
			if ( GetFollowTarget() )
				 StopFollowing();
		}
		break;

	case NPC_STATE_COMBAT:
		{
			CBaseEntity *pEnemy = GetEnemy();
			if ( pEnemy != NULL )
			{
				if ( DisregardEnemy( pEnemy ) )		// After 15 seconds of being hidden, return to alert
				{
					// Strip enemy when going to alert
					SetEnemy( NULL );
					return NPC_STATE_ALERT;
				}
				// Follow if only scared a little
				if ( GetFollowTarget() )
				{
					return NPC_STATE_ALERT;
				}

				if ( HasCondition( COND_SEE_ENEMY ) )
				{
					m_flFearTime = gpGlobals->curtime;
					return NPC_STATE_COMBAT;
				}

			}
		}
		break;
	}

	return BaseClass::SelectIdealState();
}

int CNPC_Scientist::FriendNumber( int arrayNumber )
{
	static int array[3] = { 1, 2, 0 };
	if ( arrayNumber < 3 )
		return array[ arrayNumber ];
	return arrayNumber;
}

float CNPC_Scientist::TargetDistance( void )
{
	CBaseEntity *pFollowTarget = GetFollowTarget();

	// If we lose the player, or he dies, return a really large distance
	if ( pFollowTarget == NULL || !pFollowTarget->IsAlive() )
		return 1e6;

	return (pFollowTarget->WorldSpaceCenter() - WorldSpaceCenter()).Length();
}

bool CNPC_Scientist::IsValidEnemy( CBaseEntity *pEnemy )
{
	if( pEnemy->m_iClassname == s_iszBarnacleClassname )
	{
		// Scientists ignore barnacles rather than freak out.(sjb)
		return false;
	}

	return BaseClass::IsValidEnemy(pEnemy);
}


//=========================================================
// Dead Scientist PROP
//=========================================================
class CNPC_DeadScientist : public CAI_BaseNPC
{
	DECLARE_CLASS( CNPC_DeadScientist, CAI_BaseNPC );
public:

	void Spawn( void );
	Class_T	Classify ( void ) { return	CLASS_NONE; }

	bool KeyValue( const char *szKeyName, const char *szValue );
	float MaxYawSpeed ( void ) { return 8.0f; }

	int	m_iPose;// which sequence to display	-- temporary, don't need to save
	int m_iDesiredSequence;
	static char *m_szPoses[7];
};


char *CNPC_DeadScientist::m_szPoses[] = { "lying_on_back", "lying_on_stomach", "dead_sitting", "dead_hang", "dead_table1", "dead_table2", "dead_table3" };

bool CNPC_DeadScientist::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "pose" ) )
		m_iPose = atoi( szValue );
	else 
		BaseClass::KeyValue( szKeyName, szValue );

	return true;
}

LINK_ENTITY_TO_CLASS( monster_scientist_dead, CNPC_DeadScientist );

//
// ********** DeadScientist SPAWN **********
//
void CNPC_DeadScientist::Spawn( void )
{
	PrecacheModel("models/scientist.mdl");
	SetModel( "models/scientist.mdl" );
	
	ClearEffects();
	SetSequence( 0 );
	m_bloodColor		= BLOOD_COLOR_RED;

	SetRenderColor( 255, 255, 255, 255 );

	if ( m_nBody == -1 )
	{// -1 chooses a random head
		m_nBody = random->RandomInt( 0, NUM_SCIENTIST_HEADS-1);// pick a head, any head
	}
	// Luther is black, make his hands black
	if ( m_nBody == HEAD_LUTHER )
		m_nSkin = 1;
	else
		m_nSkin = 0;

	SetSequence( LookupSequence( m_szPoses[m_iPose] ) );

	if ( GetSequence() == -1)
	{
		Msg ( "Dead scientist with bad pose\n" );
	}

	m_iHealth			= 0.0;//gSkillData.barneyHealth;

	NPCInitDead();

}


//=========================================================
// Sitting Scientist PROP
//=========================================================


LINK_ENTITY_TO_CLASS( monster_sitting_scientist, CNPC_SittingScientist );

//IMPLEMENT_CUSTOM_AI( monster_sitting_scientist, CNPC_SittingScientist );

//IMPLEMENT_SERVERCLASS_ST( CNPC_SittingScientist, DT_NPC_SittingScientist )
//END_SEND_TABLE()


//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_SittingScientist )
	DEFINE_FIELD( m_iHeadTurn, FIELD_INTEGER ),
	DEFINE_FIELD( m_flResponseDelay, FIELD_FLOAT ),
	//DEFINE_FIELD( m_baseSequence, FIELD_INTEGER ),

	DEFINE_THINKFUNC( SittingThink ),
END_DATADESC()


// animation sequence aliases 
typedef enum
{
SITTING_ANIM_sitlookleft,
SITTING_ANIM_sitlookright,
SITTING_ANIM_sitscared,
SITTING_ANIM_sitting2,
SITTING_ANIM_sitting3
} SITTING_ANIM;


//
// ********** Scientist SPAWN **********
//
void CNPC_SittingScientist::Spawn( )
{
	PrecacheModel("models/scientist.mdl");
	SetModel("models/scientist.mdl");
	Precache();

	InitBoneControllers();

	SetHullType(HULL_HUMAN);
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	m_iHealth			= 50;
	
	m_bloodColor		= BLOOD_COLOR_RED;
	m_flFieldOfView		= VIEW_FIELD_WIDE; // indicates the width of this monster's forward view cone ( as a dotproduct result )

	m_NPCState			= NPC_STATE_NONE;

	CapabilitiesClear();
	CapabilitiesAdd( bits_CAP_TURN_HEAD );

	m_spawnflags |= SF_NPC_PREDISASTER; // predisaster only!

	if ( m_nBody == -1 )
	{// -1 chooses a random head
		m_nBody = random->RandomInt( 0, NUM_SCIENTIST_HEADS-1 );// pick a head, any head
	}
	// Luther is black, make his hands black
	if ( m_nBody == HEAD_LUTHER )
		 m_nBody = 1;
	
	UTIL_DropToFloor( this,MASK_SOLID );

	NPCInit();

	SetThink (&CNPC_SittingScientist::SittingThink);
	SetNextThink( gpGlobals->curtime + 0.1f );

	m_baseSequence = LookupSequence( "sitlookleft" );
	SetSequence( m_baseSequence + random->RandomInt(0,4) );
	ResetSequenceInfo( );
}

void CNPC_SittingScientist::Precache( void )
{
	m_baseSequence = LookupSequence( "sitlookleft" );
	TalkInit();
}

int CNPC_SittingScientist::FriendNumber( int arrayNumber )
{
	static int array[3] = { 2, 1, 0 };
	if ( arrayNumber < 3 )
		return array[ arrayNumber ];
	return arrayNumber;
}

//=========================================================
// sit, do stuff
//=========================================================
void CNPC_SittingScientist::SittingThink( void )
{
	CBaseEntity *pent;	

	StudioFrameAdvance( );

	// try to greet player
	//FIXMEFIXME

	//MB - don't greet, done by base talker
	if ( 0 && GetExpresser()->CanSpeakConcept( TLK_HELLO ) )
	{
		pent = FindNearestFriend(true);
		if (pent)
		{
			float yaw = VecToYaw(pent->GetAbsOrigin() - GetAbsOrigin()) - GetAbsAngles().y;

			if (yaw > 180) yaw -= 360;
			if (yaw < -180) yaw += 360;
				
			if (yaw > 0)
				SetSequence( m_baseSequence + SITTING_ANIM_sitlookleft );
			else
				SetSequence ( m_baseSequence + SITTING_ANIM_sitlookright );
		
			ResetSequenceInfo( );
			SetCycle( 0 );
			SetBoneController( 0, 0 );

			GetExpresser()->Speak( TLK_HELLO );
		}
	}
	else if ( IsSequenceFinished() )
	{
		int i = random->RandomInt(0,99);
		m_iHeadTurn = 0;
		
		if (m_flResponseDelay && gpGlobals->curtime > m_flResponseDelay)
		{
			// respond to question
			GetExpresser()->Speak( TLK_QUESTION );
			SetSequence( m_baseSequence + SITTING_ANIM_sitscared );
			m_flResponseDelay = 0;
		}
		else if (i < 30)
		{
			SetSequence( m_baseSequence + SITTING_ANIM_sitting3 );

			// turn towards player or nearest friend and speak

			//FIXME
		/*/	if (!FBitSet(m_nSpeak, bit_saidHelloPlayer))
				pent = FindNearestFriend(TRUE);
			else*/
				pent = FindNamedEntity( "!nearestfriend" );

			if (!FIdleSpeak() || !pent)
			{	
				m_iHeadTurn = random->RandomInt(0,8) * 10 - 40;
				SetSequence( m_baseSequence + SITTING_ANIM_sitting3 );
			}
			else
			{
				// only turn head if we spoke
				float yaw = VecToYaw(pent->GetAbsOrigin() - GetAbsOrigin()) - GetAbsAngles().y;

				if (yaw > 180) yaw -= 360;
				if (yaw < -180) yaw += 360;
				
				if (yaw > 0)
					SetSequence( m_baseSequence + SITTING_ANIM_sitlookleft );
				else
					SetSequence( m_baseSequence + SITTING_ANIM_sitlookright );

				//ALERT(at_console, "sitting speak\n");
			}
		}
		else if (i < 60)
		{
			SetSequence( m_baseSequence + SITTING_ANIM_sitting3 );
			m_iHeadTurn = random->RandomInt(0,8) * 10 - 40;
			if ( random->RandomInt(0,99) < 5)
			{
				//ALERT(at_console, "sitting speak2\n");
				FIdleSpeak();
			}
		}
		else if (i < 80)
		{
			SetSequence( m_baseSequence + SITTING_ANIM_sitting2 );
		}
		else if (i < 100)
		{
			SetSequence( m_baseSequence + SITTING_ANIM_sitscared );
		}

		ResetSequenceInfo( );
		SetCycle( 0 );
		SetBoneController( 0, m_iHeadTurn );
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
}

// prepare sitting scientist to answer a question
void CNPC_SittingScientist::SetAnswerQuestion( CNPCSimpleTalker *pSpeaker )
{
	m_flResponseDelay = gpGlobals->curtime + random->RandomFloat(3, 4);
	SetSpeechTarget( (CNPCSimpleTalker *)pSpeaker );
}

//------------------------------------------------------------------------------
//
// Schedules
//
//------------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( monster_scientist, CNPC_Scientist )

	DECLARE_TASK( TASK_SAY_HEAL )
	DECLARE_TASK( TASK_HEAL )
	DECLARE_TASK( TASK_SAY_FEAR )
	DECLARE_TASK( TASK_RUN_PATH_SCARED )
	DECLARE_TASK( TASK_SCREAM )
	DECLARE_TASK( TASK_RANDOM_SCREAM )
	DECLARE_TASK( TASK_MOVE_TO_TARGET_RANGE_SCARED )
	
	DECLARE_ACTIVITY( ACT_EXCITED )

	//=========================================================
	// > SCHED_SCI_HEAL
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_SCI_HEAL,

		"	Tasks"
		"		TASK_GET_PATH_TO_TARGET				0"
		"		TASK_MOVE_TO_TARGET_RANGE			50"
		"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_SCI_FOLLOWTARGET"
		"		TASK_FACE_IDEAL						0"
		"		TASK_SAY_HEAL						0"
		"		TASK_PLAY_SEQUENCE_FACE_TARGET		ACTIVITY:ACT_ARM"
		"		TASK_HEAL							0"
		"		TASK_PLAY_SEQUENCE_FACE_TARGET		ACTIVITY:ACT_DISARM"
		"	"
		"	Interrupts"
	)

	//=========================================================
	// > SCHED_SCI_FOLLOWTARGET
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_SCI_FOLLOWTARGET,

		"	Tasks"
//		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_SCI_STOPFOLLOWING"
		"		TASK_GET_PATH_TO_TARGET			0"
		"		TASK_MOVE_TO_TARGET_RANGE		128"
		"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_TARGET_FACE"	
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_COMBAT"
	)

	//=========================================================
	// > SCHED_SCI_STOPFOLLOWING
	//=========================================================
//	DEFINE_SCHEDULE
//	(
//		SCHED_SCI_STOPFOLLOWING,
//
//		"	Tasks"
//		"		TASK_TALKER_CANT_FOLLOW			0"
//		"	"
//		"	Interrupts"
//	)

	//=========================================================
	// > SCHED_SCI_FACETARGET
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_SCI_FACETARGET,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_FACE_TARGET			0"
		"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
		"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_SCI_FOLLOWTARGET"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_COMBAT"
		"		COND_GIVE_WAY"
	)

	//=========================================================
	// > SCHED_SCI_COVER
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_SCI_COVER,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_SCI_PANIC"
		"		TASK_STOP_MOVING				0"
		"		TASK_FIND_COVER_FROM_ENEMY		0"
		"		TASK_RUN_PATH_SCARED			0"
		"		TASK_TURN_LEFT					179"
		"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_SCI_HIDE"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
	)

	//=========================================================
	// > SCHED_SCI_HIDE
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_SCI_HIDE,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_SCI_PANIC"
		"		TASK_STOP_MOVING			0"
		"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_CROUCHIDLE"
		"		TASK_SET_ACTIVITY			ACTIVITY:ACT_CROUCHIDLE"
		"		TASK_WAIT_RANDOM			10"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_SEE_HATE"
		"		COND_SEE_FEAR"
		"		COND_SEE_DISLIKE"
		"		COND_HEAR_DANGER"
	)

	//=========================================================
	// > SCHED_SCI_IDLESTAND
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_SCI_IDLESTAND,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
		"		TASK_WAIT					2"
		"		TASK_TALKER_HEADRESET		0"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_SMELL"
		"		COND_PROVOKED"
		"		COND_HEAR_COMBAT"
		"		COND_GIVE_WAY"
	)

	//=========================================================
	// > SCHED_SCI_PANIC
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_SCI_PANIC,

		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_FACE_ENEMY						0"
		"		TASK_SCREAM							0"
		"		TASK_PLAY_SEQUENCE_FACE_ENEMY		ACTIVITY:ACT_EXCITED"
		"		TASK_SET_ACTIVITY					ACTIVITY:ACT_IDLE"
		"	"
		"	Interrupts"
	)

	//=========================================================
	// > SCHED_SCI_FOLLOWSCARED
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_SCI_FOLLOWSCARED,

		"	Tasks"
		"		TASK_GET_PATH_TO_TARGET				0"
		"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_SCI_FOLLOWTARGET"	
		"		TASK_MOVE_TO_TARGET_RANGE_SCARED	128"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
	)

	//=========================================================
	// > SCHED_SCI_FACETARGETSCARED
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_SCI_FACETARGETSCARED,

		"	Tasks"
		"	TASK_FACE_TARGET				0"
		"		TASK_SET_ACTIVITY			ACTIVITY:ACT_CROUCHIDLE"
		"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_SCI_FOLLOWSCARED"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_HEAR_DANGER"
	)

	//=========================================================
	// > SCHED_FEAR
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_SCI_FEAR,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_FACE_ENEMY				0"
		"		TASK_SAY_FEAR				0"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
	)

	//=========================================================
	// > SCHED_SCI_STARTLE
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_SCI_STARTLE,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_SCI_PANIC"
		"		TASK_RANDOM_SCREAM					0.3"
		"		TASK_STOP_MOVING					0"
		"		TASK_PLAY_SEQUENCE_FACE_ENEMY		ACTIVITY:ACT_CROUCH"
		"		TASK_RANDOM_SCREAM					0.1"
		"		TASK_PLAY_SEQUENCE_FACE_ENEMY		ACTIVITY:ACT_CROUCHIDLE"
		"		TASK_WAIT_RANDOM					1"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_SEE_HATE"
		"		COND_SEE_FEAR"
		"		COND_SEE_DISLIKE"
	)

AI_END_CUSTOM_NPC()
