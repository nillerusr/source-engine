//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"		
#include "ai_basenpc.h"
#include "ai_senses.h"
#include "ai_memory.h"
#include "engine/IEngineSound.h"
#include "Sprite.h"
#include "IEffects.h"						
#include "prop_portal_shared.h"	
#include "te.h"	
#include "te_effect_dispatch.h"
#include "soundenvelope.h"			// for looping sound effects
#include "portal_gamerules.h"		// for difficulty settings
#include "weapon_rpg.h"
#include "explode.h"
#include "smoke_trail.h"			// smoke trailers on the rocket
#include "physics_bone_follower.h"	// For bone follower manager
#include "physicsshadowclone.h"		// For translating hit entities shadow clones to real ent

//#include "ndebugoverlay.h" 

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	ROCKET_TURRET_RANGE		8192
#define ROCKET_TURRET_EMITER_OFFSET 0.0
#define ROCKET_TURRET_THINK_RATE 0.05
#define ROCKET_TURRET_DEATH_EFFECT_TIME 1.5f
#define ROCKET_TURRET_LOCKON_TIME 2.0f
#define ROCKET_TURRET_HALF_LOCKON_TIME 1.0f
#define ROCKET_TURRET_QUARTER_LOCKON_TIME 0.5f
#define ROCKET_TURRET_ROCKET_FIRE_COOLDOWN_TIME 4.0f

// For search thinks
#define MAX_DIVERGENCE_X 30.0f
#define MAX_DIVERGENCE_Y 15.0f

#define ROCKET_TURRET_DECAL_NAME "decals/scorchfade"
#define ROCKET_TURRET_MODEL_NAME "models/props_bts/rocket_sentry.mdl"
#define ROCKET_TURRET_PROJECTILE_NAME "models/props_bts/rocket.mdl"

#define	ROCKET_TURRET_SOUND_LOCKING "NPC_RocketTurret.LockingBeep"
#define	ROCKET_TURRET_SOUND_LOCKED "NPC_FloorTurret.LockedBeep"

#define ROCKET_PROJECTILE_FIRE_SOUND "NPC_FloorTurret.RocketFire"
#define ROCKET_PROJECTILE_LOOPING_SOUND "NPC_FloorTurret.RocketFlyLoop"
#define ROCKET_PROJECTILE_DEFAULT_LIFE 20.0

//Spawnflags
#define SF_ROCKET_TURRET_START_INACTIVE		0x00000001

// These bones have physics shadows
const char *pRocketTurretFollowerBoneNames[] =
{
	"Root",
	"Base",
	"Arm_1",
	"Arm_2",
	"Arm_3",
	"Arm_4",
	"Rot_LR",
	"Rot_UD",
	"Gun_casing",
	"Gun_Barrel_01",
	"gun_barrel_02",
	"loader",
	"missle_01",
	"missle_02",
	"panel",
};


class CNPC_RocketTurret : public CAI_BaseNPC
{
	DECLARE_CLASS( CNPC_RocketTurret, CAI_BaseNPC );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

public:

	CNPC_RocketTurret( void );
	~CNPC_RocketTurret( void );

	void			Precache( void );
	void			Spawn( void );
	virtual void	Activate( void );

	virtual ITraceFilter*	GetBeamTraceFilter( void );
	void	UpdateOnRemove( void );
	bool	CreateVPhysics( void );

	// Think functions
	void	SearchThink( void );			// Lost Target, spaz out
	void	FollowThink( void );			// Found target, chase it
	void	LockingThink( void );			// Charge up effects
	void	FiringThink( void );			// Currently has rocket out
	void	DyingThink( void );				// Overloading, blowing up
	void	DeathThink( void );				// Destroyed, sparking
	void	OpeningThink ( void );			// Finish open/close animation before using pose params
	void	ClosingThink ( void );	

	// Inputs
	void	InputToggle( inputdata_t &inputdata );
	void	InputEnable( inputdata_t &inputdata );
	void	InputDisable( inputdata_t &inputdata );
	void	InputSetTarget( inputdata_t &inputdata );
	void	InputDestroy( inputdata_t &inputdata );

	void	RocketDied( void );				// After rocket hits something and self-destructs (or times out)
	Class_T	Classify( void ) 
	{
		if( m_bEnabled ) 
			return CLASS_COMBINE;

		return CLASS_NONE;
	}

	bool	FVisible( CBaseEntity *pEntity, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = NULL );

	Vector	EyeOffset( Activity nActivity ) 
	{
		return vec3_origin;
	}

	Vector	EyePosition( void )
	{
		Vector vMuzzlePos;
		GetAttachment( m_iMuzzleAttachment, vMuzzlePos, NULL, NULL, NULL );
		return vMuzzlePos;
	}

protected:

	bool	PreThink( void );
	void	Toggle( void );
	void	Enable( void );
	void	Disable( void );
	void	SetTarget( CBaseEntity* pTarget );
	void	Destroy ( void );

	float	UpdateFacing( void );
	void	UpdateAimPoint( void );
	void	FireRocket( void );
	void	UpdateSkin( int nSkin );
	void	UpdateMuzzleMatrix ( void );

	bool	TestLOS( const Vector& vAimPoint );
	bool	TestPortalsForLOS( Vector* pOutVec, bool bConsiderNonPortalAimPoint );
	bool	FindAimPointThroughPortal( const CProp_Portal* pPortal, Vector* pVecOut );
	void	SyncPoseToAimAngles ( void );
	void	LaserOn ( void );
	void	LaserOff ( void );

	bool	m_bEnabled;
	bool	m_bHasSightOfEnemy;

	QAngle	m_vecGoalAngles;
	CNetworkVar( QAngle, m_vecCurrentAngles );
	QAngle  m_vecAnglesToEnemy;

	enum
	{
		ROCKET_SKIN_IDLE=0,
		ROCKET_SKIN_LOCKING,
		ROCKET_SKIN_LOCKED,

		ROCKET_SKIN_COUNT,
	};

	Vector	m_vecDirToEnemy;
	float	m_flDistToEnemy;

	float	m_flTimeSpentDying;
	float	m_flTimeLocking;			// Period spent locking on to target
	float	m_flTimeLastFired;			// Cooldown time between attacks

	float	m_flTimeSpentPaused;		// for search think's movements
	float	m_flPauseLength;			
	float	m_flTotalDivergenceX;
	float	m_flTotalDivergenceY;

	matrix3x4_t m_muzzleToWorld;
	int		m_muzzleToWorldTick;

	int		m_iPosePitch;
	int		m_iPoseYaw;

	// Contained Bone Follower manager
	CBoneFollowerManager m_BoneFollowerManager;

	// Model indices for effects
	CNetworkVar( int, m_iLaserState );
	CNetworkVar( int, m_nSiteHalo );

	// Target indicator sprite info
	int		m_iMuzzleAttachment;
	int		m_iLightAttachment;

	COutputEvent m_OnFoundTarget;
	COutputEvent m_OnLostTarget;

	CTraceFilterSkipTwoEntities		m_filterBeams;

	EHANDLE m_hCurRocket;

};

//Datatable
BEGIN_DATADESC( CNPC_RocketTurret )

	DEFINE_FIELD( m_bEnabled,					FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vecGoalAngles,				FIELD_VECTOR ),
	DEFINE_FIELD( m_vecCurrentAngles,			FIELD_VECTOR ),
	DEFINE_FIELD( m_bHasSightOfEnemy,			FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vecAnglesToEnemy,			FIELD_VECTOR ),
	DEFINE_FIELD( m_vecDirToEnemy,				FIELD_VECTOR ),
	DEFINE_FIELD( m_flDistToEnemy,				FIELD_FLOAT ),
	DEFINE_FIELD( m_flTimeSpentDying,			FIELD_FLOAT ),
	DEFINE_FIELD( m_flTimeLocking,				FIELD_FLOAT ),
	DEFINE_FIELD( m_flTimeLastFired,			FIELD_FLOAT ),
	DEFINE_FIELD( m_iLaserState,				FIELD_INTEGER ),
	DEFINE_FIELD( m_nSiteHalo,					FIELD_INTEGER ),
	DEFINE_FIELD( m_flTimeSpentPaused,			FIELD_FLOAT ),
	DEFINE_FIELD( m_flPauseLength,				FIELD_FLOAT ),
	DEFINE_FIELD( m_flTotalDivergenceX,			FIELD_FLOAT ),
	DEFINE_FIELD( m_flTotalDivergenceY,			FIELD_FLOAT ),
	DEFINE_FIELD( m_iPosePitch,					FIELD_INTEGER ),
	DEFINE_FIELD( m_iPoseYaw,					FIELD_INTEGER ),
	DEFINE_FIELD( m_hCurRocket,					FIELD_EHANDLE ),
	DEFINE_FIELD( m_iMuzzleAttachment,			FIELD_INTEGER ),
	DEFINE_FIELD( m_iLightAttachment,			FIELD_INTEGER ),
	DEFINE_FIELD( m_muzzleToWorldTick,			FIELD_INTEGER ),
	DEFINE_FIELD( m_muzzleToWorld,				FIELD_MATRIX3X4_WORLDSPACE ),
//	DEFINE_FIELD( m_filterBeams, CTraceFilterSkipTwoEntities ),

	DEFINE_EMBEDDED( m_BoneFollowerManager ),

	DEFINE_THINKFUNC( SearchThink ),
	DEFINE_THINKFUNC( FollowThink ),
	DEFINE_THINKFUNC( LockingThink ),
	DEFINE_THINKFUNC( FiringThink ),
	DEFINE_THINKFUNC( DyingThink ),
	DEFINE_THINKFUNC( DeathThink ),
	DEFINE_THINKFUNC( OpeningThink ),
	DEFINE_THINKFUNC( ClosingThink ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID,		"Toggle", InputToggle ),
	DEFINE_INPUTFUNC( FIELD_VOID,		"Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID,		"Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_STRING,		"SetTarget", InputSetTarget ),
	DEFINE_INPUTFUNC( FIELD_VOID,		"Destroy", InputDestroy ),

	DEFINE_OUTPUT( m_OnFoundTarget,		"OnFoundTarget" ),
	DEFINE_OUTPUT( m_OnLostTarget,		"OnLostTarget" ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CNPC_RocketTurret, DT_NPC_RocketTurret)

	SendPropInt( SENDINFO( m_iLaserState ), 2 ),
	SendPropInt( SENDINFO( m_nSiteHalo ) ),
	SendPropVector( SENDINFO( m_vecCurrentAngles ) ), 

END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( npc_rocket_turret, CNPC_RocketTurret );



// Projectile class for this weapon, a rocket
class CRocket_Turret_Projectile : public CMissile
{
	DECLARE_CLASS( CRocket_Turret_Projectile, CMissile );
	DECLARE_DATADESC();
public:
	void Precache( void );
	void Spawn( void );

	virtual void NotifyLauncherOnDeath( void );
	virtual void NotifySystemEvent( CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params );
	virtual void SetLauncher( EHANDLE hLauncher );
	virtual void CreateSmokeTrail( void );		// overloaded from base

	virtual void MissileTouch( CBaseEntity *pOther );

	EHANDLE			m_hLauncher;
	CSoundPatch		*m_pAmbientSound;
protected:
	virtual void DoExplosion( void );
	virtual void CreateSounds( void );
	virtual void StopLoopingSounds ( void );

};

BEGIN_DATADESC( CRocket_Turret_Projectile )

	DEFINE_FIELD( m_hLauncher,		FIELD_EHANDLE ),

	DEFINE_FUNCTION( MissileTouch ),
	DEFINE_SOUNDPATCH( m_pAmbientSound ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( rocket_turret_projectile, CRocket_Turret_Projectile );


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CNPC_RocketTurret::CNPC_RocketTurret( void )
	: m_filterBeams( NULL, NULL, COLLISION_GROUP_DEBRIS )
{
	m_bEnabled			= false;
	m_bHasSightOfEnemy	= false;

	m_vecGoalAngles.Init();
	m_vecAnglesToEnemy.Init();
	m_vecDirToEnemy.Init();

	m_flTimeLastFired = m_flTimeLocking = m_flDistToEnemy = m_flTimeSpentDying	= 0.0f;
	
	m_iLightAttachment = m_iMuzzleAttachment = m_nSiteHalo = 0;

	m_flTimeSpentPaused = m_flPauseLength = m_flTotalDivergenceX = m_flTotalDivergenceY = 0.0f;

	m_hCurRocket = NULL;
}

CNPC_RocketTurret::~CNPC_RocketTurret( void )
{
	
}


//-----------------------------------------------------------------------------
// Purpose: Precache
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::Precache( void )
{
	PrecacheModel("effects/bluelaser1.vmt");
	m_nSiteHalo = PrecacheModel("sprites/light_glow03.vmt");
	
	PrecacheScriptSound ( ROCKET_TURRET_SOUND_LOCKING );
	PrecacheScriptSound ( ROCKET_TURRET_SOUND_LOCKED );
	PrecacheScriptSound ( ROCKET_PROJECTILE_FIRE_SOUND );
	PrecacheScriptSound ( ROCKET_PROJECTILE_LOOPING_SOUND );

	UTIL_PrecacheDecal( ROCKET_TURRET_DECAL_NAME );
	PrecacheModel( ROCKET_TURRET_MODEL_NAME );
	PrecacheModel ( ROCKET_TURRET_PROJECTILE_NAME );

	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose: the entity
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::Spawn( void )
{ 
	Precache();

	BaseClass::Spawn();

	SetViewOffset( vec3_origin );

	AddEFlags( EFL_NO_DISSOLVE );

	SetModel( ROCKET_TURRET_MODEL_NAME );
	SetSolid( SOLID_VPHYSICS );

	m_iMuzzleAttachment = LookupAttachment ( "barrel" );
	m_iLightAttachment = LookupAttachment ( "eye" );

	m_iPosePitch = LookupPoseParameter( "aim_pitch" );
	m_iPoseYaw   = LookupPoseParameter( "aim_yaw" );

	m_vecCurrentAngles = m_vecGoalAngles = GetAbsAngles();

	CreateVPhysics();

	//Set our autostart state
	m_bEnabled	 = ( ( m_spawnflags & SF_ROCKET_TURRET_START_INACTIVE ) == false );

	// Set Locked sprite
	if ( m_bEnabled )
	{
		m_iLaserState = 1;
		SetSequence(LookupSequence("idle"));
	}
	else
	{
		m_iLaserState = 0;
		SetSequence(LookupSequence("inactive"));
	}
	SetCycle(1.0f);
	UpdateSkin( ROCKET_SKIN_IDLE );

	SetPoseParameter( "aim_pitch", 0 );
	SetPoseParameter( "aim_yaw", -180 );

	if ( m_bEnabled )
	{
		SetThink( &CNPC_RocketTurret::FollowThink );
	}
	
	SetNextThink( gpGlobals->curtime + ROCKET_TURRET_THINK_RATE );
}

bool CNPC_RocketTurret::CreateVPhysics( void )
{
	m_BoneFollowerManager.InitBoneFollowers( this, ARRAYSIZE(pRocketTurretFollowerBoneNames), pRocketTurretFollowerBoneNames );
	BaseClass::CreateVPhysics();
	return true;
}

void CNPC_RocketTurret::Activate( void )
{
	m_filterBeams.SetPassEntity( this );
	m_filterBeams.SetPassEntity2( UTIL_GetLocalPlayer() );
	BaseClass::Activate();
}

ITraceFilter* CNPC_RocketTurret::GetBeamTraceFilter( void )
{
	return &m_filterBeams;
}

void CNPC_RocketTurret::UpdateOnRemove( void )
{
	m_BoneFollowerManager.DestroyBoneFollowers();
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_RocketTurret::FVisible( CBaseEntity *pEntity, int traceMask, CBaseEntity **ppBlocker )
{
	CBaseEntity	*pHitEntity = NULL;
	if ( BaseClass::FVisible( pEntity, traceMask, &pHitEntity ) )
		return true;

	if (ppBlocker)
	{
		*ppBlocker = pHitEntity;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void CNPC_RocketTurret::UpdateAimPoint
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::UpdateAimPoint ( void )
{
	//If we've become inactive
	if ( ( m_bEnabled == false ) || ( GetEnemy() == NULL ) )
	{
		SetEnemy( NULL );
		SetNextThink( TICK_NEVER_THINK );
		m_vecGoalAngles = GetAbsAngles();
		return;
	}

	//Get our shot positions
	Vector vecMid = EyePosition();
	Vector vecMidEnemy = GetEnemy()->GetAbsOrigin() + (GetEnemy()->WorldAlignMins() + GetEnemy()->WorldAlignMaxs()) * 0.5f;

	//Calculate dir and dist to enemy
	m_vecDirToEnemy = vecMidEnemy - vecMid;	
	m_flDistToEnemy = VectorNormalize( m_vecDirToEnemy );
	VectorAngles( m_vecDirToEnemy, m_vecAnglesToEnemy );

	bool bEnemyVisible = false;
	if ( !(GetEnemy()->GetFlags() & FL_NOTARGET) )
	{
		bool bEnemyVisibleInWorld = FVisible( GetEnemy() );

		// Test portals in our view as possible ways to view the player
		bool bEnemyVisibleThroughPortal = TestPortalsForLOS( &vecMidEnemy, bEnemyVisibleInWorld );

		bEnemyVisible = bEnemyVisibleInWorld || bEnemyVisibleThroughPortal;
	}

	//Store off our last seen location
	UpdateEnemyMemory( GetEnemy(), vecMidEnemy );

	if ( bEnemyVisible )
	{
		m_vecDirToEnemy = vecMidEnemy - vecMid;
		m_flDistToEnemy = VectorNormalize( m_vecDirToEnemy );
		VectorAngles( m_vecDirToEnemy, m_vecAnglesToEnemy );
	}

	//Current enemy is not visible
	if ( ( bEnemyVisible == false ) || ( m_flDistToEnemy > ROCKET_TURRET_RANGE ) )
	{
		// Had LOS, just lost it
		if ( m_bHasSightOfEnemy )
		{
			m_OnLostTarget.FireOutput( GetEnemy(), this );
		}
		m_bHasSightOfEnemy = false;
	}

	//If we can see our enemy
	if ( bEnemyVisible )
	{
		// Had no LOS, just gained it
		if ( !m_bHasSightOfEnemy )
		{
			m_OnFoundTarget.FireOutput( GetEnemy(), this );
		}
		m_bHasSightOfEnemy = true;
	}
}

bool SignDiffers ( float f1, float f2 )
{
	return !( Sign(f1) == Sign(f2) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::SearchThink()
{
	if ( PreThink() || GetEnemy() == NULL )
		return;

	SetSequence ( LookupSequence( "idle" ) );
	UpdateAimPoint();

	//Update our think time
	SetNextThink( gpGlobals->curtime + ROCKET_TURRET_THINK_RATE );

	// Still can't see enemy, zip around frantically
	if ( !m_bHasSightOfEnemy )
	{
		if ( m_flTimeSpentPaused >= m_flPauseLength )
		{
			float flOffsetX = RandomFloat( -5.0f, 5.0f );
			float flOffsetY = RandomFloat( -5.0f, 5.0f );

			if ( fabs(m_flTotalDivergenceX) <= MAX_DIVERGENCE_X || 
				 SignDiffers( m_flTotalDivergenceX, flOffsetX ) )
			{
				m_flTotalDivergenceX += flOffsetX;
				m_vecGoalAngles.x += flOffsetX;
			}

			if ( fabs(m_flTotalDivergenceY) <= MAX_DIVERGENCE_Y ||
				 SignDiffers( m_flTotalDivergenceY, flOffsetY ) )
			{
				m_flTotalDivergenceY += flOffsetY;
				m_vecGoalAngles.y += flOffsetY;
			}

			// Reset pause timer
			m_flTimeSpentPaused = 0.0f;
			m_flPauseLength = RandomFloat( 0.3f, 2.5f );
		}
		m_flTimeSpentPaused += ROCKET_TURRET_THINK_RATE;
	}
	else
	{
		// Found target, go back to following it
		SetThink( &CNPC_RocketTurret::FollowThink );
		SetNextThink( gpGlobals->curtime + ROCKET_TURRET_THINK_RATE );
	}

	// Move beam towards goal angles
	UpdateFacing();
}

//-----------------------------------------------------------------------------
// Purpose: Allows the turret to fire on targets if they're visible
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::FollowThink( void )
{
	// Default to player as enemy
	if ( GetEnemy() == NULL )
	{
		SetEnemy( UTIL_GetLocalPlayer() );
	}

	SetSequence ( LookupSequence( "idle" ) );
	//Allow descended classes a chance to do something before the think function
	if ( PreThink() || GetEnemy() == NULL )
	{
		return;
	}
	//Update our think time
	SetNextThink( gpGlobals->curtime + ROCKET_TURRET_THINK_RATE );

	UpdateAimPoint();

	m_vecGoalAngles = m_vecAnglesToEnemy;

	// Chase enemy
	if ( !m_bHasSightOfEnemy )
	{
		// Aim at the last known location
		m_vecGoalAngles = m_vecCurrentAngles;

		// Lost sight, move to search think
		SetThink( &CNPC_RocketTurret::SearchThink );
	}

	//Turn to face
	UpdateFacing();

	// If our facing direction hits our enemy, fire the beam
	Ray_t rayDmg;
	Vector vForward;
	AngleVectors( m_vecCurrentAngles, &vForward, NULL, NULL );
	Vector vEndPoint = EyePosition() + vForward*ROCKET_TURRET_RANGE;
	rayDmg.Init( EyePosition(), vEndPoint );
	rayDmg.m_IsRay = true;
	trace_t traceDmg;

	// This version reorients through portals
	CTraceFilterSimple subfilter( this, COLLISION_GROUP_NONE );
	CTraceFilterTranslateClones filter ( &subfilter );
	float flRequiredParameter = 2.0f;
	CProp_Portal* pFirstPortal = UTIL_Portal_FirstAlongRay( rayDmg, flRequiredParameter );
	UTIL_Portal_TraceRay_Bullets( pFirstPortal, rayDmg, MASK_VISIBLE_AND_NPCS, &filter, &traceDmg, false );

	if ( traceDmg.m_pEnt )
	{
		// This thing we're hurting is our enemy
		if ( traceDmg.m_pEnt == GetEnemy() )
		{
			// If we're past the cooldown time, fire another rocket
			if ( (gpGlobals->curtime - m_flTimeLastFired) > ROCKET_TURRET_ROCKET_FIRE_COOLDOWN_TIME )
			{
				SetThink( &CNPC_RocketTurret::LockingThink );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Charge up, prepare to fire and give player time to dodge
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::LockingThink( void )
{
	//Allow descended classes a chance to do something before the think function
	if ( PreThink() )
		return;

	//Turn to face
	UpdateFacing();
	SetNextThink( gpGlobals->curtime + ROCKET_TURRET_THINK_RATE );

    if ( m_flTimeLocking == 0.0f )
	{
		// Play lockon sound
		EmitSound ( ROCKET_TURRET_SOUND_LOCKING );
		EmitSound ( ROCKET_TURRET_SOUND_LOCKING, gpGlobals->curtime + ROCKET_TURRET_QUARTER_LOCKON_TIME );
		EmitSound ( ROCKET_TURRET_SOUND_LOCKED, gpGlobals->curtime + ROCKET_TURRET_HALF_LOCKON_TIME );

		ResetSequence(LookupSequence("load"));

		// Change lockon sprite
		UpdateSkin( ROCKET_SKIN_LOCKING );
	}

	m_flTimeLocking += ROCKET_TURRET_THINK_RATE;

	if ( m_flTimeLocking > ROCKET_TURRET_LOCKON_TIME )
	{
		// Set Locked sprite to 'rocket out' color
		UpdateSkin( ROCKET_SKIN_LOCKED );

		FireRocket();
		SetThink ( &CNPC_RocketTurret::FiringThink );
		m_flTimeLocking = 0.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Charge up, deal damage along our facing direction.
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::FiringThink( void )
{	
	//Allow descended classes a chance to do something before the think function
	if ( PreThink() )
		return;

	SetNextThink( gpGlobals->curtime + ROCKET_TURRET_THINK_RATE );
	CRocket_Turret_Projectile* pRocket = dynamic_cast<CRocket_Turret_Projectile*>(m_hCurRocket.Get());

	if ( pRocket )
	{
		// If this rocket has been out too long, detonate it and launch a new one
		if ( (gpGlobals->curtime - m_flTimeLastFired) > ROCKET_PROJECTILE_DEFAULT_LIFE )
		{
			pRocket->ShotDown();
			m_flTimeLastFired = gpGlobals->curtime;
			SetThink( &CNPC_RocketTurret::FollowThink );
		}
	}
	else
	{
		// Set Locked sprite
		UpdateSkin( ROCKET_SKIN_IDLE );
		// Rocket dead, or never created. Revert to follow think
		m_flTimeLastFired = gpGlobals->curtime;
		SetThink( &CNPC_RocketTurret::FollowThink );
	}
}

void CNPC_RocketTurret::FireRocket ( void )
{
	UTIL_Remove( m_hCurRocket );

	CRocket_Turret_Projectile *pRocket = (CRocket_Turret_Projectile *) CBaseEntity::Create( "rocket_turret_projectile", EyePosition(), m_vecCurrentAngles, this );

	if ( !pRocket )
		return;

	m_hCurRocket = pRocket;

	Vector vForward;
	AngleVectors( m_vecCurrentAngles, &vForward, NULL, NULL );

	m_flTimeLastFired = gpGlobals->curtime;

	EmitSound ( ROCKET_PROJECTILE_FIRE_SOUND );
	ResetSequence(LookupSequence("fire"));

	pRocket->SetThink( NULL );
	pRocket->SetMoveType( MOVETYPE_FLY );

	pRocket->CreateSmokeTrail();

	pRocket->SetModel( ROCKET_TURRET_PROJECTILE_NAME );
	UTIL_SetSize( pRocket, vec3_origin, vec3_origin );

	pRocket->SetAbsVelocity( vForward * 550 );
	pRocket->SetLauncher ( this );
}

void CNPC_RocketTurret::UpdateSkin( int nSkin )
{
	m_nSkin = nSkin;
}


//-----------------------------------------------------------------------------
// Purpose: Rocket destructed, resume search behavior
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::RocketDied( void )
{
	// Set Locked sprite
	UpdateSkin( ROCKET_SKIN_IDLE );

	// Rocket dead, return to follow think
	m_flTimeLastFired = gpGlobals->curtime;
	SetThink( &CNPC_RocketTurret::FollowThink );
}


//-----------------------------------------------------------------------------
// Purpose: Show this rocket turret has overloaded with effects and noise for a period of time
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::DyingThink( void )
{
	// Make the beam graphics freak out a bit
	m_iLaserState = 2;


	UpdateSkin( ROCKET_SKIN_IDLE );

	// If we've freaked out for long enough, be dead
	if ( m_flTimeSpentDying > ROCKET_TURRET_DEATH_EFFECT_TIME )
	{
		Vector vForward;
		AngleVectors( m_vecCurrentAngles, &vForward, NULL, NULL );
		g_pEffects->EnergySplash( EyePosition(), vForward, true );

		m_OnDeath.FireOutput( this, this );
		SetThink( &CNPC_RocketTurret::DeathThink );
		SetNextThink( gpGlobals->curtime + 1.0f );
		m_flTimeSpentDying = 0.0f;
	}

	SetNextThink( gpGlobals->curtime + ROCKET_TURRET_THINK_RATE );
	m_flTimeSpentDying += ROCKET_TURRET_THINK_RATE;
}

//-----------------------------------------------------------------------------
// Purpose: Sparks and fizzes to show it's broken.
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::DeathThink( void )
{
	Vector vForward;
	AngleVectors( m_vecCurrentAngles, &vForward, NULL, NULL );

	m_iLaserState = 0;
	SetEnemy( NULL );

	g_pEffects->Sparks( EyePosition(), 1, 1, &vForward );
	g_pEffects->Smoke( EyePosition(), 0, 6.0f, 20 );

	SetNextThink( gpGlobals->curtime + RandomFloat( 2.0f, 8.0f ) );
}

void CNPC_RocketTurret::UpdateMuzzleMatrix()
{
	if ( gpGlobals->tickcount != m_muzzleToWorldTick )
	{
		m_muzzleToWorldTick = gpGlobals->tickcount;
		GetAttachment( m_iMuzzleAttachment, m_muzzleToWorld );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Avoid aiming/drawing beams while opening and closing
// Input  :  - 
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::OpeningThink()
{
	StudioFrameAdvance();

	// Require these poses for this animation
	QAngle vecNeutralAngles ( 0, 90, 0 );
	m_vecGoalAngles = m_vecCurrentAngles = vecNeutralAngles;
	SyncPoseToAimAngles();

	// Start following player after we're fully opened
	float flCurProgress = GetCycle();
	if ( flCurProgress >= 0.99f )
	{
		
		LaserOn();
		SetThink( &CNPC_RocketTurret::FollowThink );
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: Avoid aiming/drawing beams while opening and closing
// Input  :  - 
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::ClosingThink()
{
	LaserOff();

	// Require these poses for this animation
	QAngle vecNeutralAngles ( 0, 90, 0 );
	m_vecGoalAngles = vecNeutralAngles;

	// Once we're within 10 degrees of the neutral pose, start close animation.
	if ( UpdateFacing() <= 10.0f )
	{
		StudioFrameAdvance();
	}

	SetNextThink( gpGlobals->curtime + ROCKET_TURRET_THINK_RATE );

	// Start following player after we're fully opened
	float flCurProgress = GetCycle();
	if ( flCurProgress >= 0.99f )
	{
		SetThink( NULL );
		SetNextThink( TICK_NEVER_THINK );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void SyncPoseToAimAngles
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::SyncPoseToAimAngles ( void )
{
	QAngle localAngles = TransformAnglesToLocalSpace( m_vecCurrentAngles.Get(), EntityToWorldTransform() );

	// Update pitch
	SetPoseParameter( m_iPosePitch, localAngles.x );

	// Update yaw -- NOTE: This yaw movement is screwy for this model, we must invert the yaw delta and also skew an extra 90 deg to
	// get the 'forward face' of the turret to match up with the look direction. If the model and it's pose parameters change, this will be wrong.
	SetPoseParameter( m_iPoseYaw, AngleNormalize( -localAngles.y - 90 ) );

	InvalidateBoneCache();
}

//-----------------------------------------------------------------------------
// Purpose: Causes the turret to face its desired angles
// Returns distance current and goal angles the angles in degrees.
//-----------------------------------------------------------------------------
float CNPC_RocketTurret::UpdateFacing( void )
{
	Quaternion qtCurrent ( m_vecCurrentAngles.Get() );
	Quaternion qtGoal ( m_vecGoalAngles );
	Quaternion qtOut;

	float flDiff = QuaternionAngleDiff( qtCurrent, qtGoal );

	// 1/10th degree is all the granularity we need, gives rocket player hit box width accuracy at 18k game units.
	if ( flDiff < 0.1 )
		return flDiff;

	// Slerp 5% of the way to goal (distance dependant speed, but torque minimial and no euler wrapping issues).
	QuaternionSlerp( qtCurrent, qtGoal, 0.05, qtOut );

	QAngle vNewAngles;
	QuaternionAngles( qtOut, vNewAngles );

	m_vecCurrentAngles = vNewAngles;

	SyncPoseToAimAngles();

	return flDiff;
}

//-----------------------------------------------------------------------------
// Purpose: Tests if this prop's front point will have direct line of sight to it's target entity once the pose parameters are set to face it
// Input  : vAimPoint - The point to aim at
// Output : Returns true if target is in direct line of sight, false otherwise.
//-----------------------------------------------------------------------------
bool CNPC_RocketTurret::TestLOS( const Vector& vAimPoint )
{
	// Snap to face (for accurate traces)
	QAngle vecOldAngles = m_vecCurrentAngles.m_Value;
	Vector vecToAimPoint = vAimPoint - EyePosition();
	VectorAngles( vecToAimPoint, m_vecCurrentAngles.m_Value );
	SyncPoseToAimAngles();

	Vector vFaceOrigin = EyePosition();
	trace_t trTarget;
	Ray_t ray;
	ray.Init( vFaceOrigin, vAimPoint );
	ray.m_IsRay = true;

	// This aim point does hit target, now make sure there are no blocking objects in the way
	CTraceFilterSimple filter ( this, COLLISION_GROUP_NONE );
	UTIL_Portal_TraceRay( ray, MASK_VISIBLE_AND_NPCS, &filter, &trTarget, false );

	// Set model back to current facing
	m_vecCurrentAngles = vecOldAngles;
	SyncPoseToAimAngles();

	return ( trTarget.m_pEnt == GetEnemy() );
}

//-----------------------------------------------------------------------------
// Purpose: Tests all portals in the turret's vis for possible routes to see it's target point
// Input  : pOutVec - The location to aim at in order to hit the target ent, choosing least rotation if multiple
//			bConsiderNonPortalAimPoint - Output in pOutVec the non portal (direct) aimpoint if it requires the least rotation
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_RocketTurret::TestPortalsForLOS( Vector* pOutVec, bool bConsiderNonPortalAimPoint = false )
{
	// Aim at the target through the world
	CBaseEntity* pTarget = GetEnemy();
	if ( !pTarget )
	{
		return false;
	}
	Vector vAimPoint = pTarget->GetAbsOrigin() + (pTarget->WorldAlignMins() + pTarget->WorldAlignMaxs()) * 0.5f;

	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if( iPortalCount == 0 )
	{
		*pOutVec = vAimPoint;
		return false;
	}

	Vector vCurAim;
	AngleVectors( m_vecCurrentAngles.m_Value, &vCurAim );
	vCurAim.NormalizeInPlace();

	CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
	Vector *portalAimPoints = (Vector *)stackalloc( sizeof( Vector ) * iPortalCount );
	bool *bUsable = (bool *)stackalloc( sizeof( bool ) * iPortalCount );
	float *fPortalDot = (float *)stackalloc( sizeof( float ) * iPortalCount );

	// Test through any active portals: This may be a shorter distance to the target
	for( int i = 0; i != iPortalCount; ++i )
	{
		CProp_Portal *pTempPortal = pPortals[i];

		if( !pTempPortal->m_bActivated ||
			(pTempPortal->m_hLinkedPortal.Get() == NULL) )
		{
			//portalAimPoints[i] = vec3_invalid;
			bUsable[i] = false;
			continue;
		}

		
		bUsable[i] = FindAimPointThroughPortal( pPortals[ i ], &portalAimPoints[ i ] );
		if ( 1 )
		{
			QAngle goalAngles;
			Vector vecToEnemy = portalAimPoints[ i ] - EyePosition();
			vecToEnemy.NormalizeInPlace();

			// This value is for choosing the easiest aim point for the turret to see through.
			// 'Easiest' is the least rotation needed.
			fPortalDot[i] = DotProduct( vecToEnemy, vCurAim );
		}
	}
	

	int iCountPortalsThatSeeTarget = 0;
	
	float fHighestDot = -1.0;
	if ( bConsiderNonPortalAimPoint )
	{
		QAngle enemyRotToFace;
		Vector vecToEnemy = vAimPoint - EyePosition();
		vecToEnemy.NormalizeInPlace();
	
		fHighestDot			= DotProduct( vecToEnemy, vCurAim );
	}

	// Compare aim points, use the closest aim point which has direct LOS
	for( int i = 0; i != iPortalCount; ++i )
	{
		if( bUsable[i] )
		{
			// This aim point has direct LOS
			if ( TestLOS( portalAimPoints[ i ] ) && fHighestDot < fPortalDot[ i ] )
			{
				*pOutVec = portalAimPoints[ i ];
				fHighestDot = fPortalDot[ i ];

				++iCountPortalsThatSeeTarget;
			}
		}
	}

	return (iCountPortalsThatSeeTarget != 0);
}

//-----------------------------------------------------------------------------
// Purpose: Find the center of the target entity as seen through the specified portal
// Input  : pPortal - The portal to look through
// Output : Vector& output point in world space where the target *appears* to be as seen through the portal
//-----------------------------------------------------------------------------
bool CNPC_RocketTurret::FindAimPointThroughPortal( const CProp_Portal* pPortal, Vector* pVecOut )
{ 
	if ( pPortal && pPortal->m_bActivated )
	{
		CProp_Portal* pLinked = pPortal->m_hLinkedPortal.Get(); 
		CBaseEntity*  pTarget = GetEnemy();

		// Require that the portal is facing towards the beam to test through it
		Vector vRocketToPortal, vPortalForward;
		VectorSubtract ( pPortal->GetAbsOrigin(), EyePosition(), vRocketToPortal );
		pPortal->GetVectors( &vPortalForward, NULL, NULL);
		float fDot = DotProduct( vRocketToPortal, vPortalForward );

		// Portal must be facing the turret, and have a linked partner
		if ( fDot < 0.0f && pLinked && pLinked->m_bActivated && pTarget )
		{
			VMatrix matToPortalView = pLinked->m_matrixThisToLinked;
			Vector vTargetAimPoint = pTarget->GetAbsOrigin() + (pTarget->WorldAlignMins() + pTarget->WorldAlignMaxs()) * 0.5f;
			*pVecOut =  matToPortalView * vTargetAimPoint;   
			return true;
		}
	}

	// Bad portal pointer, not linked, no target or otherwise failed
	return false;
}

void CNPC_RocketTurret::LaserOn( void )
{
	// Set Locked sprite
	m_iLaserState = 1;
}

void CNPC_RocketTurret::LaserOff( void )
{
	// Set Locked sprite;
	m_iLaserState = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Allows a generic think function before the others are called
// Input  : state - which state the turret is currently in
//-----------------------------------------------------------------------------
bool CNPC_RocketTurret::PreThink( void )
{
	StudioFrameAdvance();
	CheckPVSCondition();

	//Do not interrupt current think function
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Toggle the turret's state
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::Toggle( void )
{
	//Toggle the state
	if ( m_bEnabled )
	{
		Disable();
	}
	else 
	{
		Enable();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Enable the turret and deploy
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::Enable( void )
{
	if ( m_bEnabled )
		return;

	m_bEnabled = true;
	ResetSequence( LookupSequence("open") );

	SetThink( &CNPC_RocketTurret::OpeningThink );
	SetNextThink( gpGlobals->curtime + 0.05 );
}

//-----------------------------------------------------------------------------
// Purpose: Retire the turret until enabled again
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::Disable( void )
{
	if ( !m_bEnabled )
		return;

	UpdateSkin( ROCKET_SKIN_IDLE );

	m_bEnabled = false;
	ResetSequence(LookupSequence("close"));

	SetThink( &CNPC_RocketTurret::ClosingThink );
	SetNextThink( gpGlobals->curtime + 0.05 );
	SetEnemy( NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the enemy of this rocket
// Input  : pTarget - the enemy to set
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::SetTarget( CBaseEntity* pTarget )
{
	SetEnemy( pTarget );
}

//-----------------------------------------------------------------------------
// Purpose: Explode and set death think
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::Destroy( void )
{
	SetThink( &CNPC_RocketTurret::DyingThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::InputToggle( inputdata_t &inputdata )
{
	Toggle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::InputEnable( inputdata_t &inputdata )
{
	Enable();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::InputDisable( inputdata_t &inputdata )
{
	Disable();
}

void CNPC_RocketTurret::InputSetTarget( inputdata_t &inputdata )
{
	CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, inputdata.value.String(), NULL, NULL );
	SetTarget( pTarget );
}

//-----------------------------------------------------------------------------
// Purpose: Plays some 'death' effects and sets the destroy think
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CNPC_RocketTurret::InputDestroy( inputdata_t &inputdata )
{
	Destroy();
}


//-----------------------------------------------------------------------------
// Projectile methods
//-----------------------------------------------------------------------------
void CRocket_Turret_Projectile::Spawn( void )
{
	Precache();
	BaseClass::Spawn();
	SetTouch ( &CRocket_Turret_Projectile::MissileTouch );
	CreateSounds();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CRocket_Turret_Projectile::MissileTouch( CBaseEntity *pOther )
{
	Assert( pOther );
	Vector vVel = GetAbsVelocity();

	// Touched a launcher, and is heading towards that launcher
	if ( FClassnameIs( pOther, "npc_rocket_turret" ) )
	{
		Dissolve( NULL, gpGlobals->curtime + 0.1f, false, ENTITY_DISSOLVE_NORMAL );
		Vector vBounceVel = Vector( -vVel.x, -vVel.y, 200 );
		SetAbsVelocity (  vBounceVel * 0.1f );
		QAngle vBounceAngles;
		VectorAngles( vBounceVel, vBounceAngles );
		SetAbsAngles ( vBounceAngles );
		SetLocalAngularVelocity ( QAngle ( 180, 90, 45 ) );
		UTIL_Remove ( m_hRocketTrail );

		SetSolid ( SOLID_NONE );

		if( m_hRocketTrail )
		{
			m_hRocketTrail->SetLifetime(0.1f);
			m_hRocketTrail = NULL;
		}

		return;
	}

	// Don't touch triggers (but DO hit weapons)
	if ( pOther->IsSolidFlagSet(FSOLID_TRIGGER|FSOLID_VOLUME_CONTENTS) && pOther->GetCollisionGroup() != COLLISION_GROUP_WEAPON )
		return;

	Explode();
}


void CRocket_Turret_Projectile::Precache( void )
{
	BaseClass::Precache();

	PrecacheScriptSound( ROCKET_PROJECTILE_LOOPING_SOUND );
}

void CRocket_Turret_Projectile::NotifyLauncherOnDeath( void )
{
	CNPC_RocketTurret* pLauncher = (CNPC_RocketTurret*)m_hLauncher.Get();

	if ( pLauncher ) 
	{
		pLauncher->RocketDied();
	}
}

// When teleported (usually by portal)
void CRocket_Turret_Projectile::NotifySystemEvent(CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params )
{
	// On teleport, we record a pointer to the portal we are arriving at
	if ( eventType == NOTIFY_EVENT_TELEPORT )
	{
		// HACK: Clearing the owner allows collisions with launcher.
		// Players have had trouble realizing a launcher's own rockets don't kill it
		// because they didn't ever collide. We do this after a portal teleport so it avoids self-collisions on launch.
		SetOwnerEntity( NULL );

		// Restart smoke trail
		UTIL_Remove( m_hRocketTrail );
		m_hRocketTrail = NULL; // This shouldn't leak cause the pointer has been handed to the delete list
		CreateSmokeTrail();
	}
}

void CRocket_Turret_Projectile::SetLauncher ( EHANDLE hLauncher )
{
	m_hLauncher = hLauncher;
}

void CRocket_Turret_Projectile::DoExplosion( void )
{
	NotifyLauncherOnDeath();

	StopLoopingSounds();

	// Explode
	ExplosionCreate( GetAbsOrigin(), GetAbsAngles(), GetOwnerEntity(), 200, 25, 
		SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_NOSMOKE, 100.0f, this);

	// Hackish: Knock turrets in the area
	CBaseEntity* pTurretIter = NULL;

	while ( (pTurretIter = gEntList.FindEntityByClassnameWithin( pTurretIter, "npc_portal_turret_floor", GetAbsOrigin(), 128 )) != NULL )
	{
		CTakeDamageInfo info( this, this, 200, DMG_BLAST );
		info.SetDamagePosition( GetAbsOrigin() );
		CalculateExplosiveDamageForce( &info, (pTurretIter->GetAbsOrigin() - GetAbsOrigin()), GetAbsOrigin() );

		pTurretIter->VPhysicsTakeDamage( info );
	}	
}

void CRocket_Turret_Projectile::CreateSounds()
{
	if (!m_pAmbientSound)
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		CPASAttenuationFilter filter( this );

		m_pAmbientSound = controller.SoundCreate( filter, entindex(), ROCKET_PROJECTILE_LOOPING_SOUND );
		controller.Play( m_pAmbientSound, 1.0, 100 );
	}
}


void CRocket_Turret_Projectile::StopLoopingSounds()
{
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

	controller.SoundDestroy( m_pAmbientSound );
	m_pAmbientSound = NULL;

	BaseClass::StopLoopingSounds();
}


void CRocket_Turret_Projectile::CreateSmokeTrail( void )
{
	if ( m_hRocketTrail )
		return;

	// Smoke trail.
	if ( (m_hRocketTrail = RocketTrail::CreateRocketTrail()) != NULL )
	{
		m_hRocketTrail->m_Opacity = 0.2f;
		m_hRocketTrail->m_SpawnRate = 100;
		m_hRocketTrail->m_ParticleLifetime = 0.8f;
		m_hRocketTrail->m_StartColor.Init( 0.65f, 0.65f , 0.65f );
		m_hRocketTrail->m_EndColor.Init( 0.0, 0.0, 0.0 );
		m_hRocketTrail->m_StartSize = 8;
		m_hRocketTrail->m_EndSize = 32;
		m_hRocketTrail->m_SpawnRadius = 4;
		m_hRocketTrail->m_MinSpeed = 2;
		m_hRocketTrail->m_MaxSpeed = 16;
		
		m_hRocketTrail->SetLifetime( 999 );
		m_hRocketTrail->FollowEntity( this, "0" );
	}
}






static void fire_rocket_projectile_f( void )
{
	CBasePlayer *pPlayer = (CBasePlayer *)UTIL_GetCommandClient();

	Vector ptEyes, vForward;
	QAngle vLookAng;
	ptEyes = pPlayer->EyePosition();
	pPlayer->EyeVectors( &vForward );
	vLookAng = pPlayer->EyeAngles();

	CRocket_Turret_Projectile *pRocket = (CRocket_Turret_Projectile *) CBaseEntity::Create( "rocket_turret_projectile", ptEyes, vLookAng, pPlayer );

	if ( !pRocket )
		return;

	pRocket->SetThink( NULL );
	pRocket->SetMoveType( MOVETYPE_FLY );

	pRocket->SetModel( ROCKET_TURRET_PROJECTILE_NAME );
	UTIL_SetSize( pRocket, vec3_origin, vec3_origin );

	pRocket->CreateSmokeTrail();

	pRocket->SetAbsVelocity( vForward * 550 );
	pRocket->SetLauncher ( NULL );
}

ConCommand fire_rocket_projectile( "fire_rocket_projectile", fire_rocket_projectile_f, "Fires a rocket turret projectile from the player's eyes for testing.", FCVAR_CHEAT );