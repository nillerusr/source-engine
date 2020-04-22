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
#include	"ai_senses.h"
#include	"soundent.h"
#include	"game.h"
#include	"npcevent.h"
#include	"entitylist.h"
#include	"activitylist.h"
#include	"animation.h"
#include	"basecombatweapon.h"
#include	"IEffects.h"
#include	"vstdlib/random.h"
#include	"engine/IEngineSound.h"
#include	"ammodef.h"
#include	"hl1_ai_basenpc.h"
#include	"studio.h"			//hitbox parsing
#include	"collisionutils.h"	//ComputeSeparatingPlane

#include	"physics_bone_follower.h"	//For BoneFollowerManager

#define ACT_T_IDLE		1010
Activity ACT_1010;
Activity ACT_1011;
Activity ACT_1012;
Activity ACT_1013;

#define ACT_T_TAP		1020
Activity ACT_1020;
Activity ACT_1021;
Activity ACT_1022;
Activity ACT_1023;

#define ACT_T_STRIKE	1030
Activity ACT_1030;
Activity ACT_1031;
Activity ACT_1032;
Activity ACT_1033;

#define ACT_T_REARIDLE	1040
Activity ACT_1040;
Activity ACT_1041;
Activity ACT_1042;
Activity ACT_1043;
Activity ACT_1044;

class CNPC_Tentacle : public CHL1BaseNPC
{
	DECLARE_CLASS( CNPC_Tentacle, CHL1BaseNPC );
public:

	CNPC_Tentacle();

	void Spawn( );
	void Precache( );
	bool KeyValue( const char *szKeyName, const char *szValue );

	bool QueryHearSound( CSound *pSound ) { return true; } // Tentacle isn't picky.

	
	int Level( float dz );
	int MyLevel( void );
	float MyHeight( void );

	// Don't allow the tentacle to go across transitions!!!
	virtual int	ObjectCaps( void ) { return CAI_BaseNPC::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	void Start ( void );
	void Cycle ( void );
	void HitTouch( CBaseEntity *pOther );

	void HandleAnimEvent( animevent_t *pEvent );
	float HearingSensitivity( void ) { return 2.0; };

	virtual int OnTakeDamage( const CTakeDamageInfo &info );

	bool CreateVPhysics( void );

	void UpdateOnRemove( void );

	float m_flInitialYaw;
	int m_iGoalAnim;
	int m_iLevel;
	int m_iDir;
	float m_flFramerateAdj;
	float m_flSoundYaw;
	int m_iSoundLevel;
	float m_flSoundTime;
	float m_flSoundRadius;
	int m_iHitDmg;
	float m_flHitTime;

	float m_flTapRadius;

	float m_flNextSong;
	static int g_fFlySound;
	static int g_fSquirmSound;

	float m_flMaxYaw;
	int m_iTapSound;

	Vector m_vecPrevSound;
	float m_flPrevSoundTime;

	float	MaxYawSpeed( void ) { 	return 18.0f;	}

	bool	HeardAnything( void );

	Class_T Classify ( void );
	
	CBoneFollowerManager	m_BoneFollowerManager;

	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
};

// Crane bones that have physics followers
static const char *pTentacleFollowerBoneNames[] =
{
	"Bone08",
	"Bone09"
};

int CNPC_Tentacle::g_fFlySound;
int CNPC_Tentacle::g_fSquirmSound;

LINK_ENTITY_TO_CLASS( monster_tentacle, CNPC_Tentacle );

// stike sounds
#define TE_NONE -1
#define TE_SILO 0
#define TE_DIRT 1
#define TE_WATER 2

// animation sequence aliases 
typedef enum
{
	TENTACLE_ANIM_Pit_Idle,

	TENTACLE_ANIM_rise_to_Temp1,
	TENTACLE_ANIM_Temp1_to_Floor,
	TENTACLE_ANIM_Floor_Idle,
	TENTACLE_ANIM_Floor_Fidget_Pissed,
	TENTACLE_ANIM_Floor_Fidget_SmallRise,
	TENTACLE_ANIM_Floor_Fidget_Wave,
	TENTACLE_ANIM_Floor_Strike,
	TENTACLE_ANIM_Floor_Tap,
	TENTACLE_ANIM_Floor_Rotate,
	TENTACLE_ANIM_Floor_Rear,
	TENTACLE_ANIM_Floor_Rear_Idle,
	TENTACLE_ANIM_Floor_to_Lev1,

	TENTACLE_ANIM_Lev1_Idle,
	TENTACLE_ANIM_Lev1_Fidget_Claw,
	TENTACLE_ANIM_Lev1_Fidget_Shake,
	TENTACLE_ANIM_Lev1_Fidget_Snap,
	TENTACLE_ANIM_Lev1_Strike,
	TENTACLE_ANIM_Lev1_Tap,
	TENTACLE_ANIM_Lev1_Rotate,
	TENTACLE_ANIM_Lev1_Rear,
	TENTACLE_ANIM_Lev1_Rear_Idle,
	TENTACLE_ANIM_Lev1_to_Lev2,

	TENTACLE_ANIM_Lev2_Idle,
	TENTACLE_ANIM_Lev2_Fidget_Shake,
	TENTACLE_ANIM_Lev2_Fidget_Swing,
	TENTACLE_ANIM_Lev2_Fidget_Tut,
	TENTACLE_ANIM_Lev2_Strike,
	TENTACLE_ANIM_Lev2_Tap,
	TENTACLE_ANIM_Lev2_Rotate,
	TENTACLE_ANIM_Lev2_Rear,
	TENTACLE_ANIM_Lev2_Rear_Idle,
	TENTACLE_ANIM_Lev2_to_Lev3,

	TENTACLE_ANIM_Lev3_Idle,
	TENTACLE_ANIM_Lev3_Fidget_Shake,
	TENTACLE_ANIM_Lev3_Fidget_Side,
	TENTACLE_ANIM_Lev3_Fidget_Swipe,
	TENTACLE_ANIM_Lev3_Strike,
	TENTACLE_ANIM_Lev3_Tap,
	TENTACLE_ANIM_Lev3_Rotate,
	TENTACLE_ANIM_Lev3_Rear,
	TENTACLE_ANIM_Lev3_Rear_Idle,

	TENTACLE_ANIM_Lev1_Door_reach,

	TENTACLE_ANIM_Lev3_to_Engine,
	TENTACLE_ANIM_Engine_Idle,
	TENTACLE_ANIM_Engine_Sway,
	TENTACLE_ANIM_Engine_Swat,
	TENTACLE_ANIM_Engine_Bob,
	TENTACLE_ANIM_Engine_Death1,
	TENTACLE_ANIM_Engine_Death2,
	TENTACLE_ANIM_Engine_Death3,

	TENTACLE_ANIM_none
} TENTACLE_ANIM;

BEGIN_DATADESC( CNPC_Tentacle )
	DEFINE_FIELD( m_flInitialYaw, FIELD_FLOAT ),
	DEFINE_FIELD( m_iGoalAnim, FIELD_INTEGER ),
	DEFINE_FIELD( m_iLevel, FIELD_INTEGER ),
	DEFINE_FIELD( m_iDir, FIELD_INTEGER ),
	DEFINE_FIELD( m_flFramerateAdj, FIELD_FLOAT ),
	DEFINE_FIELD( m_flSoundYaw, FIELD_FLOAT ),
	DEFINE_FIELD( m_iSoundLevel, FIELD_INTEGER ),
	DEFINE_FIELD( m_flSoundTime, FIELD_TIME ),
	DEFINE_FIELD( m_flSoundRadius, FIELD_FLOAT ),
	DEFINE_FIELD( m_iHitDmg, FIELD_INTEGER ),
	DEFINE_FIELD( m_flHitTime, FIELD_TIME ),
	DEFINE_FIELD( m_flTapRadius, FIELD_FLOAT ),
	DEFINE_FIELD( m_flNextSong, FIELD_TIME ),
	DEFINE_FIELD( m_iTapSound, FIELD_INTEGER ),
	DEFINE_FIELD( m_flMaxYaw, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecPrevSound, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_flPrevSoundTime, FIELD_TIME ),

	DEFINE_EMBEDDED( m_BoneFollowerManager ),

	DEFINE_THINKFUNC( Start ),
	DEFINE_THINKFUNC( Cycle ),
	DEFINE_ENTITYFUNC( HitTouch ),
END_DATADESC()

Class_T CNPC_Tentacle::Classify ( void ) 
{ 
	return CLASS_ALIEN_MONSTER;	
}


CNPC_Tentacle::CNPC_Tentacle()
{
	m_flMaxYaw = 65;
	m_iTapSound = 0;
}

bool CNPC_Tentacle::KeyValue( const char *szKeyName, const char *szValue  )
{
	if ( FStrEq( szKeyName, "sweeparc") )
	{
		 m_flMaxYaw = atof( szValue ) / 2.0;
		 return true;
	}
	else if (FStrEq( szKeyName, "sound"))
	{
		 m_iTapSound = atoi( szValue );
		 return true;
	}
	else
		return BaseClass::KeyValue( szKeyName, szValue );

	return false;
}

//
// Tentacle Spawn
//
void CNPC_Tentacle::Spawn( )
{
	Precache( );

	SetSolid( SOLID_BBOX );

	//Necessary for TestCollision to be called for hitbox ray hits
	AddSolidFlags( FSOLID_CUSTOMRAYTEST );

	SetMoveType( MOVETYPE_NONE );
	ClearEffects();
	m_iHealth			= 75;
	SetSequence( 0 );

	SetModel( "models/tentacle2.mdl" );
	UTIL_SetSize( this, Vector( -32, -32, 0 ), Vector( 32, 32, 64 ) );

	// Use our hitboxes to determine our render bounds
	CollisionProp()->SetSurroundingBoundsType( USE_HITBOXES );

	m_takedamage		= DAMAGE_AIM;
	AddFlag( FL_NPC );
	
	m_bloodColor		= BLOOD_COLOR_GREEN;

	ResetSequenceInfo( );
	m_iDir = 1;

	SetThink( &CNPC_Tentacle::Start );
	SetNextThink( gpGlobals->curtime + 0.2 );

	SetTouch( &CNPC_Tentacle::HitTouch );

	m_flInitialYaw = GetAbsAngles().y;
	GetMotor()->SetIdealYawAndUpdate( m_flInitialYaw );
	
	g_fFlySound = FALSE;
	g_fSquirmSound = FALSE;

	m_iHitDmg = 200;

	if (m_flMaxYaw <= 0)
		m_flMaxYaw = 65;

	m_NPCState = NPC_STATE_IDLE;

	UTIL_SetOrigin( this, GetAbsOrigin() );

	CreateVPhysics();

	AddEffects( EF_NOSHADOW );
}

void CNPC_Tentacle::UpdateOnRemove( void )
{
	m_BoneFollowerManager.DestroyBoneFollowers();
	BaseClass::UpdateOnRemove();
}

void CNPC_Tentacle::Precache( )
{
	PrecacheModel("models/tentacle2.mdl");

	PrecacheScriptSound( "Tentacle.Flies" );
	PrecacheScriptSound( "Tentacle.Squirm" );
	PrecacheScriptSound( "Tentacle.Sing" );
	PrecacheScriptSound( "Tentacle.HitDirt" );
	PrecacheScriptSound( "Tentacle.Swing" );
	PrecacheScriptSound( "Tentacle.Search" );
	PrecacheScriptSound( "Tentacle.Roar" );
	PrecacheScriptSound( "Tentacle.Alert" );

	BaseClass::Precache();
}


int CNPC_Tentacle::Level( float dz )
{
	if (dz < 216)
		return 0;
	if (dz < 408)
		return 1;
	if (dz < 600)
		return 2;
	return 3;
}


float CNPC_Tentacle::MyHeight( )
{
	switch ( MyLevel( ) )
	{
	case 1:
		return 256;
	case 2:
		return 448;
	case 3:
		return 640;
	}
	return 0;
}


int CNPC_Tentacle::MyLevel( )
{
	switch( GetSequence() )
	{
	case TENTACLE_ANIM_Pit_Idle: 
		return -1;

	case TENTACLE_ANIM_rise_to_Temp1:
	case TENTACLE_ANIM_Temp1_to_Floor:
	case TENTACLE_ANIM_Floor_to_Lev1:
		return 0;

	case TENTACLE_ANIM_Floor_Idle:
	case TENTACLE_ANIM_Floor_Fidget_Pissed:
	case TENTACLE_ANIM_Floor_Fidget_SmallRise:
	case TENTACLE_ANIM_Floor_Fidget_Wave:
	case TENTACLE_ANIM_Floor_Strike:
	case TENTACLE_ANIM_Floor_Tap:
	case TENTACLE_ANIM_Floor_Rotate:
	case TENTACLE_ANIM_Floor_Rear:
	case TENTACLE_ANIM_Floor_Rear_Idle:
		return 0;

	case TENTACLE_ANIM_Lev1_Idle:
	case TENTACLE_ANIM_Lev1_Fidget_Claw:
	case TENTACLE_ANIM_Lev1_Fidget_Shake:
	case TENTACLE_ANIM_Lev1_Fidget_Snap:
	case TENTACLE_ANIM_Lev1_Strike:
	case TENTACLE_ANIM_Lev1_Tap:
	case TENTACLE_ANIM_Lev1_Rotate:
	case TENTACLE_ANIM_Lev1_Rear:
	case TENTACLE_ANIM_Lev1_Rear_Idle:
		return 1;

	case TENTACLE_ANIM_Lev1_to_Lev2:
		return 1;

	case TENTACLE_ANIM_Lev2_Idle:
	case TENTACLE_ANIM_Lev2_Fidget_Shake:
	case TENTACLE_ANIM_Lev2_Fidget_Swing:
	case TENTACLE_ANIM_Lev2_Fidget_Tut:
	case TENTACLE_ANIM_Lev2_Strike:
	case TENTACLE_ANIM_Lev2_Tap:
	case TENTACLE_ANIM_Lev2_Rotate:
	case TENTACLE_ANIM_Lev2_Rear:
	case TENTACLE_ANIM_Lev2_Rear_Idle:
		return 2;

	case TENTACLE_ANIM_Lev2_to_Lev3:
		return 2;

	case TENTACLE_ANIM_Lev3_Idle:
	case TENTACLE_ANIM_Lev3_Fidget_Shake:
	case TENTACLE_ANIM_Lev3_Fidget_Side:
	case TENTACLE_ANIM_Lev3_Fidget_Swipe:
	case TENTACLE_ANIM_Lev3_Strike:
	case TENTACLE_ANIM_Lev3_Tap:
	case TENTACLE_ANIM_Lev3_Rotate:
	case TENTACLE_ANIM_Lev3_Rear:
	case TENTACLE_ANIM_Lev3_Rear_Idle:
		return 3;

	case TENTACLE_ANIM_Lev1_Door_reach:
		return -1;
	}
	return -1;
}

void CNPC_Tentacle::Start( void )
{
	SetThink( &CNPC_Tentacle::Cycle );

	CPASAttenuationFilter filter( this );

	if ( !g_fFlySound )
	{
		EmitSound( filter, entindex(), "Tentacle.Flies" );
		g_fFlySound = TRUE;
	}
	else if ( !g_fSquirmSound )
	{
		EmitSound( filter, entindex(), "Tentacle.Squirm" );
		g_fSquirmSound = TRUE;
	}
	
	SetNextThink( gpGlobals->curtime + 0.1 );
}

bool CNPC_Tentacle::HeardAnything( void )
{
	if ( HasCondition( COND_HEAR_DANGER ) || // I remove a bunch of sounds from here on purpose. Talk to me if you're changing this!(sjb)
		 HasCondition( COND_HEAR_COMBAT ) ||
		 HasCondition( COND_HEAR_WORLD )  ||
		 HasCondition( COND_HEAR_PLAYER ) )
		 return true;

	return false;
}

void CNPC_Tentacle::Cycle( void )
{
	//NDebugOverlay::Cross3D( EarPosition(), 32, 255, 0, 0, false, 0.1 );

	// ALERT( at_console, "%s %.2f %d %d\n", STRING( pev->targetname ), pev->origin.z, m_MonsterState, m_IdealMonsterState );
	SetNextThink( gpGlobals->curtime + 0.1 );

	// ALERT( at_console, "%s %d %d %d %f %f\n", STRING( pev->targetname ), pev->sequence, m_iGoalAnim, m_iDir, pev->framerate, pev->health );

	if ( m_NPCState == NPC_STATE_SCRIPT || GetIdealState() == NPC_STATE_SCRIPT)
	{
		SetAbsAngles( QAngle( GetAbsAngles().x, m_flInitialYaw, GetAbsAngles().z ) );
		GetMotor()->SetIdealYaw( m_flInitialYaw );	
		RemoveIgnoredConditions();
		NPCThink( );
		m_iGoalAnim = TENTACLE_ANIM_Pit_Idle;
		return;
	}
	
	StudioFrameAdvance();
	DispatchAnimEvents( this );

	GetMotor()->UpdateYaw( MaxYawSpeed() );
	
	CSound *pSound = NULL;

	GetSenses()->Listen();
	m_BoneFollowerManager.UpdateBoneFollowers(this);

	// Listen will set this if there's something in my sound list
	if ( HeardAnything() )
		 pSound = GetSenses()->GetClosestSound( false, (SOUND_DANGER|SOUND_COMBAT|SOUND_WORLD|SOUND_PLAYER) );
	else
		 pSound = NULL;

	if ( pSound )
	{
		//NDebugOverlay::Line( EarPosition(), pSound->GetSoundOrigin(), 0, 255, 0, false, 0.2 );

		Vector vecDir;
		if ( gpGlobals->curtime - m_flPrevSoundTime < 0.5 )
		{
			float dt = gpGlobals->curtime - m_flPrevSoundTime;
			vecDir = pSound->GetSoundOrigin() + (pSound->GetSoundOrigin() - m_vecPrevSound) / dt - GetAbsOrigin();
		}
		else
		{
			vecDir = pSound->GetSoundOrigin() - GetAbsOrigin();
		}

		m_flPrevSoundTime = gpGlobals->curtime;
		m_vecPrevSound = pSound->GetSoundOrigin();

		m_flSoundYaw = VecToYaw ( vecDir ) - m_flInitialYaw;

		m_iSoundLevel = Level( vecDir.z );

		if (m_flSoundYaw < -180)
			m_flSoundYaw += 360;
		if (m_flSoundYaw > 180)
			m_flSoundYaw -= 360;

		// ALERT( at_console, "sound %d %.0f\n", m_iSoundLevel, m_flSoundYaw );
		if (m_flSoundTime < gpGlobals->curtime)
		{
			// play "I hear new something" sound
			 UTIL_EmitAmbientSound( GetSoundSourceIndex(), GetAbsOrigin() + Vector( 0, 0, MyHeight()), "Tentacle.Alert", 1.0, SNDLVL_GUNFIRE, 0, 100);
		}
		m_flSoundTime = gpGlobals->curtime + random->RandomFloat( 5.0, 10.0 );
	}

	// clip ideal_yaw
	float dy = m_flSoundYaw;
	switch( GetSequence() )
	{
	case TENTACLE_ANIM_Floor_Rear:
	case TENTACLE_ANIM_Floor_Rear_Idle:
	case TENTACLE_ANIM_Lev1_Rear:
	case TENTACLE_ANIM_Lev1_Rear_Idle:
	case TENTACLE_ANIM_Lev2_Rear:
	case TENTACLE_ANIM_Lev2_Rear_Idle:
	case TENTACLE_ANIM_Lev3_Rear:
	case TENTACLE_ANIM_Lev3_Rear_Idle:
		if (dy < 0 && dy > -m_flMaxYaw)
			dy = -m_flMaxYaw;
		if (dy > 0 && dy < m_flMaxYaw)
			dy = m_flMaxYaw;
		break;
	default:
		if (dy < -m_flMaxYaw)
			dy = -m_flMaxYaw;
		if (dy > m_flMaxYaw)
			dy = m_flMaxYaw;
	}
	GetMotor()->SetIdealYaw( m_flInitialYaw + dy );

	if ( IsSequenceFinished() )
	{
		// ALERT( at_console, "%s done %d %d\n", STRING( pev->targetname ), pev->sequence, m_iGoalAnim );
		if ( m_iHealth <= 1)
		{
			m_iGoalAnim = TENTACLE_ANIM_Pit_Idle;

			if ( GetSequence() == TENTACLE_ANIM_Pit_Idle)
			{
				m_iHealth = 75;
			}
		}
		else if ( m_flSoundTime > gpGlobals->curtime )
		{
			if (m_flSoundYaw >= -(m_flMaxYaw + 30) && m_flSoundYaw <= (m_flMaxYaw + 30))
			{
				// strike
				switch ( m_iSoundLevel )
				{
					case 0:
						m_iGoalAnim = SelectWeightedSequence ( ACT_1030 );
						break;
					case 1:
						m_iGoalAnim = SelectWeightedSequence ( ACT_1031 );
						break;
					case 2:
						m_iGoalAnim = SelectWeightedSequence ( ACT_1032 );
						break;
					case 3:
						m_iGoalAnim = SelectWeightedSequence ( ACT_1033 );
						break;
				}
			}
			else if (m_flSoundYaw >= -m_flMaxYaw * 2 && m_flSoundYaw <= m_flMaxYaw * 2) 
			{
				// tap
				switch ( m_iSoundLevel )
				{
					case 0:
						m_iGoalAnim = SelectWeightedSequence ( ACT_1020 );
						break;
					case 1:
						m_iGoalAnim = SelectWeightedSequence ( ACT_1021 );
						break;
					case 2:
						m_iGoalAnim = SelectWeightedSequence ( ACT_1022 );
						break;
					case 3:
						m_iGoalAnim = SelectWeightedSequence ( ACT_1023 );
						break;
				}
			}
			else
			{
				// go into rear idle
				switch ( m_iSoundLevel )
				{
					case 0:
						m_iGoalAnim = SelectWeightedSequence ( ACT_1040 );
						break;
					case 1:
						m_iGoalAnim = SelectWeightedSequence ( ACT_1041 );
						break;
					case 2:
						m_iGoalAnim = SelectWeightedSequence ( ACT_1042 );
						break;
					case 3:
						m_iGoalAnim = SelectWeightedSequence ( ACT_1043 );
						break;
					case 4:
						m_iGoalAnim = SelectWeightedSequence ( ACT_1044 );
						break;
				}
			}
		}
		else if ( GetSequence() == TENTACLE_ANIM_Pit_Idle)
		{
			// stay in pit until hear noise
			m_iGoalAnim = TENTACLE_ANIM_Pit_Idle;
		}
		else if ( GetSequence() == m_iGoalAnim)
		{
			if ( MyLevel() >= 0 && gpGlobals->curtime < m_flSoundTime)
			{
				if ( random->RandomInt(0,9) < m_flSoundTime - gpGlobals->curtime )
				{
					// continue stike
					switch ( m_iSoundLevel )
					{
						case 0:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1030 );
							break;
						case 1:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1031 );
							break;
						case 2:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1032 );
							break;
						case 3:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1033 );
							break;
					}
				}
				else
				{
					// tap
					switch ( m_iSoundLevel )
					{
						case 0:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1020 );
							break;
						case 1:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1021 );
							break;
						case 2:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1022 );
							break;
						case 3:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1023 );
							break;
					}
				}
			}
			else if ( MyLevel( ) < 0 )
			{
				m_iGoalAnim = SelectWeightedSequence( ACT_1010 );
			}
			else
			{
				if (m_flNextSong < gpGlobals->curtime)
				{
					// play "I hear new something" sound
					CPASAttenuationFilter filter( this );
					EmitSound( filter, entindex(), "Tentacle.Sing" );

					m_flNextSong = gpGlobals->curtime + random->RandomFloat( 10, 20 );
				}

				if (random->RandomInt(0,15) == 0)
				{
					// idle on new level
					switch ( random->RandomInt( 0, 3 ) )
					{
						case 0:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1010 );
							break;
						case 1:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1011 );
							break;
						case 2:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1012 );
							break;
						case 3:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1013 );
							break;
					}
				}
				else if ( random->RandomInt( 0, 3 ) == 0 )
				{
					// tap
					switch ( MyLevel() )
					{
						case 0:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1020 );
							break;
						case 1:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1021 );
							break;
						case 2:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1022 );
							break;
						case 3:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1023 );
							break;
					}
				}
				else
				{
					// idle
					switch ( MyLevel() )
					{
						case 0:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1010 );
							break;
						case 1:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1011 );
							break;
						case 2:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1012 );
							break;
						case 3:
							m_iGoalAnim = SelectWeightedSequence ( ACT_1013 );
							break;
					}
				}
			}
			if (m_flSoundYaw < 0)
				m_flSoundYaw += random->RandomFloat( 2, 8 );
			else
				m_flSoundYaw -= random->RandomFloat( 2, 8 );
		}

		SetSequence( FindTransitionSequence( GetSequence(), m_iGoalAnim, &m_iDir ) );
		

		if (m_iDir > 0)
		{
			SetCycle( 0 );
		}
		else
		{
			m_iDir = -1; // just to safe
			SetCycle( 1.0f );
		}

		ResetSequenceInfo( );

		m_flFramerateAdj = random->RandomFloat( -0.2, 0.2 );
		m_flPlaybackRate = m_iDir * 1.0 + m_flFramerateAdj;

		switch( GetSequence() )
		{
		case TENTACLE_ANIM_Floor_Tap:
		case TENTACLE_ANIM_Lev1_Tap:
		case TENTACLE_ANIM_Lev2_Tap:
		case TENTACLE_ANIM_Lev3_Tap:
			{
				Vector vecSrc, v_forward;
				AngleVectors( GetAbsAngles(), &v_forward );

				trace_t tr1, tr2;

				vecSrc = GetAbsOrigin() + Vector( 0, 0, MyHeight() - 4);
				UTIL_TraceLine( vecSrc, vecSrc + v_forward * 512, MASK_NPCSOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr1 );

				vecSrc = GetAbsOrigin() + Vector( 0, 0, MyHeight() + 8);
				UTIL_TraceLine( vecSrc, vecSrc + v_forward * 512, MASK_NPCSOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr2 );

				// ALERT( at_console, "%f %f\n", tr1.flFraction * 512, tr2.flFraction * 512 );

				m_flTapRadius = SetPoseParameter( 0, random->RandomFloat( tr1.fraction * 512, tr2.fraction * 512 ) );
			}
			break;
		default:
			m_flTapRadius = 336; // 400 - 64
			break;
		}
		SetViewOffset( Vector( 0, 0, MyHeight() ) );
		// ALERT( at_console, "seq %d\n", pev->sequence );
	}

	if (m_flPrevSoundTime + 2.0 > gpGlobals->curtime)
	{
		// 1.5 normal speed if hears sounds
		m_flPlaybackRate = m_iDir * 1.5 + m_flFramerateAdj;
	}
	else if (m_flPrevSoundTime + 5.0 > gpGlobals->curtime)
	{
		// slowdown to normal
		m_flPlaybackRate = m_iDir + m_iDir * (5 - (gpGlobals->curtime - m_flPrevSoundTime)) / 2 + m_flFramerateAdj;
	}
}

void CNPC_Tentacle::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{
	case 1:	// bang 
		{
			Vector vecSrc;
			QAngle angAngles;
			GetAttachment( "0", vecSrc, angAngles );

			// Vector vecSrc = GetAbsOrigin() + m_flTapRadius * Vector( cos( GetAbsAngles().y * (3.14192653 / 180.0) ), sin( GetAbsAngles().y * (M_PI / 180.0) ), 0.0 );

			// vecSrc.z += MyHeight( );

			switch( m_iTapSound )
			{
			case TE_SILO:
				UTIL_EmitAmbientSound( GetSoundSourceIndex(), vecSrc, "Tentacle.HitSilo", 1.0, SNDLVL_GUNFIRE, 0, 100);
				break;
			case TE_NONE:
				break;
			case TE_DIRT:
				UTIL_EmitAmbientSound( GetSoundSourceIndex(), vecSrc, "Tentacle.HitDirt", 1.0, SNDLVL_GUNFIRE, 0, 100);
				break;
			case TE_WATER:
				UTIL_EmitAmbientSound( GetSoundSourceIndex(), vecSrc, "Tentacle.HitWater", 1.0, SNDLVL_GUNFIRE, 0, 100);
				break;
			}
		}
		break;

	case 3: // start killing swing
		m_iHitDmg = 200;
		break;

	case 4: // end killing swing
		m_iHitDmg = 25;
		break;

	case 5: // just "whoosh" sound
		break;

	case 2:	// tap scrape
	case 6: // light tap
		{
			Vector vecSrc = GetAbsOrigin() + m_flTapRadius * Vector( cos( GetAbsAngles().y * (M_PI / 180.0) ), sin( GetAbsAngles().y * (M_PI / 180.0) ), 0.0 );

			vecSrc.z += MyHeight( );

			float flVol = random->RandomFloat( 0.3, 0.5 );

			switch( m_iTapSound )
			{
			case TE_SILO:
				UTIL_EmitAmbientSound( GetSoundSourceIndex(), vecSrc, "Tentacle.HitSilo", flVol, SNDLVL_GUNFIRE, 0, 100);
				break;
			case TE_NONE:
				break;
			case TE_DIRT:
				UTIL_EmitAmbientSound( GetSoundSourceIndex(), vecSrc, "Tentacle.HitDirt", flVol, SNDLVL_GUNFIRE, 0, 100);
				break;
			case TE_WATER:
				UTIL_EmitAmbientSound( GetSoundSourceIndex(), vecSrc, "Tentacle.HitWater", flVol, SNDLVL_GUNFIRE, 0, 100);
				break;
			}
		}
		break;


	case 7: // roar
		UTIL_EmitAmbientSound( GetSoundSourceIndex(), GetAbsOrigin() + Vector( 0, 0, MyHeight()), "Tentacle.Roar", 1.0, SNDLVL_GUNFIRE, 0, 100);
		break;

	case 8: // search
		UTIL_EmitAmbientSound( GetSoundSourceIndex(), GetAbsOrigin() + Vector( 0, 0, MyHeight()), "Tentacle.Search", 1.0, SNDLVL_GUNFIRE, 0, 100);
		break;

	case 9: // swing
		UTIL_EmitAmbientSound( GetSoundSourceIndex(), GetAbsOrigin() + Vector( 0, 0, MyHeight()), "Tentacle.Swing", 1.0, SNDLVL_GUNFIRE, 0, 100);
		break;
	default:
		BaseClass::HandleAnimEvent( pEvent );
	}
}

void CNPC_Tentacle::HitTouch( CBaseEntity *pOther )
{
	if (m_flHitTime > gpGlobals->curtime)
		return;

	// only look at the ones where the player hit me
	if( pOther == NULL || pOther->GetModelIndex() == GetModelIndex() || ( pOther->GetSolidFlags() & FSOLID_TRIGGER ) )
		 return;

	//Right now the BoneFollower will always be hit in box 0, and 
	//will pass that to us. Make *any* touch by the physics objects a kill
	//as the ragdoll only covers the top portion of the tentacle.
	if ( pOther->m_takedamage )
	{
		CTakeDamageInfo info( this, this, m_iHitDmg, DMG_CLUB );

		Vector vDamageForce = pOther->GetAbsOrigin() - GetAbsOrigin();
		VectorNormalize( vDamageForce );

		CalculateMeleeDamageForce( &info, vDamageForce, pOther->GetAbsOrigin() );
		pOther->TakeDamage( info );

		m_flHitTime = gpGlobals->curtime + 0.5;
	}
}

int CNPC_Tentacle::OnTakeDamage( const CTakeDamageInfo &info )
{
	CTakeDamageInfo i = info;

	//Don't allow the tentacle to die. Instead set health to 1, so we can do our own death and rebirth
	if( (int)i.GetDamage() >= m_iHealth )
	{
		i.SetDamage( 0.0f );
		m_iHealth = 1;
	}

	return BaseClass::OnTakeDamage( i );
}

bool CNPC_Tentacle::CreateVPhysics( void )
{
	BaseClass::CreateVPhysics();

	IPhysicsObject *pPhysics = VPhysicsGetObject();
	if( pPhysics )
	{
		unsigned short flags = pPhysics->GetCallbackFlags();

		flags |= CALLBACK_GLOBAL_TOUCH;

		pPhysics->SetCallbackFlags( flags );
	}
	m_BoneFollowerManager.InitBoneFollowers( this, ARRAYSIZE(pTentacleFollowerBoneNames), pTentacleFollowerBoneNames );

	return true;
}

//------------------------------------------------------------------------------
//
// Schedules
//
//------------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( monster_tentacle, CNPC_Tentacle )

	DECLARE_ACTIVITY( ACT_1010 )
	DECLARE_ACTIVITY( ACT_1011 )
	DECLARE_ACTIVITY( ACT_1012 )
	DECLARE_ACTIVITY( ACT_1013 )

	DECLARE_ACTIVITY( ACT_1020 )
	DECLARE_ACTIVITY( ACT_1021 )
	DECLARE_ACTIVITY( ACT_1022 )
	DECLARE_ACTIVITY( ACT_1023 )

	DECLARE_ACTIVITY( ACT_1030 )
	DECLARE_ACTIVITY( ACT_1031 )
	DECLARE_ACTIVITY( ACT_1032 )
	DECLARE_ACTIVITY( ACT_1033 )

	DECLARE_ACTIVITY( ACT_1040 )
	DECLARE_ACTIVITY( ACT_1041 )
	DECLARE_ACTIVITY( ACT_1042 )
	DECLARE_ACTIVITY( ACT_1043 )
	DECLARE_ACTIVITY( ACT_1044 )

AI_END_CUSTOM_NPC()
