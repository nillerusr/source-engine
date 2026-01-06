//=============================================================================//
//
// Purpose: 	Bad Cop's humble friend, a pacifist turret who could open doors for some reason.
// 
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "npc_wilson.h"
#include "Sprite.h"
#include "ai_senses.h"
#include "sceneentity.h"
#include "props.h"
#include "particle_parse.h"
#include "eventqueue.h"
#include "ez2_player.h"
#include "te_effect_dispatch.h"
#include "iservervehicle.h"
#include "basegrenade_shared.h"
#include "ai_interactions.h"
#include "grenade_hopwire.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

extern int ACT_FLOOR_TURRET_OPEN;
extern int ACT_FLOOR_TURRET_CLOSE;
extern int ACT_FLOOR_TURRET_OPEN_IDLE;
extern int ACT_FLOOR_TURRET_CLOSED_IDLE;

extern ConVar	g_debug_turret;

// Interactions
int g_interactionArbeitScannerStart		= 0;
int g_interactionArbeitScannerEnd		= 0;

extern int	g_interactionAPCConstrain;
extern int	g_interactionAPCUnconstrain;
extern int	g_interactionAPCBreak;

// Global pointer to Will-E for fast lookups
CEntityClassList<CNPC_Wilson> g_WillieList;
template <> CNPC_Wilson *CEntityClassList<CNPC_Wilson>::m_pClassList = NULL;
CNPC_Wilson *CNPC_Wilson::GetWilson( void )
{
	return g_WillieList.m_pClassList;
}

CNPC_Wilson *CNPC_Wilson::GetBestWilson( float &flBestDistSqr, const Vector *vecOrigin )
{
	CNPC_Wilson *pBestWilson = NULL;

	CNPC_Wilson *pWilson = CNPC_Wilson::GetWilson();
	for (; pWilson; pWilson = pWilson->m_pNext)
	{
		if (!pWilson->HasCondition(COND_IN_PVS))
			continue;

		if (vecOrigin)
		{
			float flDistSqr = (pWilson->GetAbsOrigin() - *vecOrigin).LengthSqr();
			if ( flDistSqr < flBestDistSqr )
			{
				pBestWilson = pWilson;
				flBestDistSqr = flDistSqr;
			}
		}
		else
		{
			// No entity to search from
			return pWilson;
		}
	}

	return pBestWilson;
}

ConVar npc_wilson_depressing_death("npc_wilson_depressing_death", "0", FCVAR_NONE, "Makes Will-E shut down and die into an empty husk rather than explode.");

ConVar npc_wilson_clearance_speed_threshold( "npc_wilson_clearance_speed_threshold", "250.0", FCVAR_NONE, "The speed at which Will-E starts to think he's gonna get knocked off if approaching a surface." );
ConVar npc_wilson_clearance_debug( "npc_wilson_clearance_debug", "0", FCVAR_NONE, "Debugs Will-E's low clearance detection." );

static const char *g_DamageZapContext = "DamageZapEffect";
static const char *g_AutoSetLocatorContext = "AutoSetLocator";

#define WILSON_MODEL "models/will_e.mdl"
#define DAMAGED_MODEL "models/will_e_damaged.mdl"

#define FLOOR_TURRET_GLOW_SPRITE	"sprites/glow1.vmt"

#define MIN_IDLE_SPEECH 7.5f
#define MAX_IDLE_SPEECH 12.5f

#define WILSON_MAX_TIPPED_HEIGHT 2048

// The amount of damage needed for Will-E to have a tesla effect
#define WILSON_TESLA_DAMAGE_THRESHOLD 15.0f

BEGIN_DATADESC(CNPC_Wilson)

	DEFINE_FIELD( m_bBlinkState,	FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bTipped,	FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flCarryTime, FIELD_TIME ),
	DEFINE_FIELD( m_bUseCarryAngles, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flPlayerDropTime, FIELD_TIME ),
	DEFINE_FIELD( m_flTeslaStopTime, FIELD_TIME ),

	DEFINE_FIELD( m_iEyeAttachment,	FIELD_INTEGER ),
	//DEFINE_FIELD( m_iMuzzleAttachment,	FIELD_INTEGER ),
	DEFINE_FIELD( m_iEyeState,		FIELD_INTEGER ),
	DEFINE_FIELD( m_hEyeGlow,		FIELD_EHANDLE ),
	DEFINE_FIELD( m_pMotionController,FIELD_EHANDLE),

	DEFINE_FIELD( m_hPhysicsAttacker, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flLastPhysicsInfluenceTime, FIELD_TIME ),

	DEFINE_FIELD( m_hAttachedVehicle, FIELD_EHANDLE ),

	DEFINE_FIELD( m_fNextFidgetSpeechTime, FIELD_TIME ),
	DEFINE_FIELD( m_bPlayerLeftPVS, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flPlayerNearbyTime, FIELD_TIME ),

	DEFINE_KEYFIELD( m_bStatic, FIELD_BOOLEAN, "static" ),

	DEFINE_KEYFIELD( m_bDamaged, FIELD_BOOLEAN, "damaged" ),

	DEFINE_KEYFIELD( m_bDead, FIELD_BOOLEAN, "dead" ),

	DEFINE_INPUT( m_bOmniscient, FIELD_BOOLEAN, "SetOmniscient" ), // TODO: Replace with "Omnipresent" when saves can be changed
	DEFINE_KEYFIELD( m_iszCameraTargets, FIELD_STRING, "CameraTargets" ),

	DEFINE_KEYFIELD( m_bSeeThroughPlayer, FIELD_BOOLEAN, "SeeThroughPlayer" ),

	DEFINE_INPUT( m_bCanBeEnemy, FIELD_BOOLEAN, "SetCanBeEnemy" ),
	
	DEFINE_INPUT( m_bAutoSetLocator, FIELD_BOOLEAN, "AutoSetLocator" ),

	DEFINE_KEYFIELD( m_bEyeLightEnabled, FIELD_BOOLEAN, "EyeLightEnabled" ),
	//DEFINE_FIELD( m_iEyeLightBrightness, FIELD_INTEGER ), // SetEyeGlow()'s call in Activate() means we don't need to save this

	DEFINE_USEFUNC( Use ),

	DEFINE_INPUTFUNC( FIELD_VOID, "SelfDestruct", InputSelfDestruct ),

	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOnEyeLight", InputTurnOnEyeLight ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOffEyeLight", InputTurnOffEyeLight ),

	DEFINE_INPUTFUNC( FIELD_VOID, "EnableMotion", InputEnableMotion ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DisableMotion", InputDisableMotion ),

	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOnDamagedMode", InputTurnOnDamagedMode ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOffDamagedMode", InputTurnOffDamagedMode ),

	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOnDeadMode", InputTurnOnDeadMode ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOffDeadMode", InputTurnOffDeadMode ),

	DEFINE_INPUTFUNC( FIELD_STRING, "SetCameraTargets", InputSetCameraTargets ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ClearCameraTargets", InputClearCameraTargets ),

	DEFINE_INPUTFUNC( FIELD_VOID, "EnableSeeThroughPlayer", InputEnableSeeThroughPlayer ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DisableSeeThroughPlayer", InputDisableSeeThroughPlayer ),

	DEFINE_INPUTFUNC( FIELD_VOID, "EnableOmnipresence", InputEnableOmnipresence ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DisableOmnipresence", InputDisableOmnipresence ),

	DEFINE_THINKFUNC( TeslaThink ),

	DEFINE_OUTPUT( m_OnTipped, "OnTipped" ),
	DEFINE_OUTPUT( m_OnPlayerUse, "OnPlayerUse" ),
	DEFINE_OUTPUT( m_OnPhysGunPickup, "OnPhysGunPickup" ),
	DEFINE_OUTPUT( m_OnPhysGunDrop, "OnPhysGunDrop" ),
	DEFINE_OUTPUT( m_OnDestroyed, "OnDestroyed" ),

END_DATADESC()

BEGIN_ENT_SCRIPTDESC( CNPC_Wilson, CAI_BaseActor, "E:Z2's Wilson, the pacifist turret." )

	DEFINE_SCRIPTFUNC_NAMED( ScriptIsBeingCarriedByPlayer, "IsBeingCarriedByPlayer", "Returns true if Wilson is being carried by the player." )
	DEFINE_SCRIPTFUNC_NAMED( ScriptWasJustDroppedByPlayer, "WasJustDroppedByPlayer", "Returns true if Wilson was just dropped by the player." )
	DEFINE_SCRIPTFUNC( IsHeldByPhyscannon, "Returns true if Wilson is being held by the gravity gun." )

	DEFINE_SCRIPTFUNC( OnSide, "Returns true if Wilson is on his side." )

	DEFINE_SCRIPTFUNC( InLowGravity, "Returns true if Wilson is in low gravity." )

END_SCRIPTDESC();

IMPLEMENT_SERVERCLASS_ST( CNPC_Wilson, DT_NPC_Wilson )
	SendPropBool( SENDINFO( m_bEyeLightEnabled ) ),
	SendPropInt( SENDINFO( m_iEyeLightBrightness ) ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( npc_wilson, CNPC_Wilson );

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CNPC_Wilson::CNPC_Wilson( void ) : 
	m_hEyeGlow( NULL ),
	m_flCarryTime( -1.0f ),
	m_flPlayerDropTime( 0.0f ),
	m_bTipped( false ),
	m_pMotionController( NULL )
{
	g_WillieList.Insert(this);

	// This will be overridden by the keyvalue,
	// but npc_create should automatically spawn Will-E with this
	m_SquadName = GetPlayerSquadName();

	AddSpawnFlags( SF_NPC_ALWAYSTHINK );

	m_bEyeLightEnabled = true;

	// So we don't think we haven't seen the player in a while
	// when in fact we were just spawned
	// (it's funny, but still)
	m_flLastSawPlayerTime = -1.0f;

	m_flPlayerNearbyTime = -1.0f;
}

CNPC_Wilson::~CNPC_Wilson( void )
{
	g_WillieList.Remove(this);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::Precache()
{
	if (GetModelName() == NULL_STRING && m_bDamaged)
		SetModelName(AllocPooledString(DAMAGED_MODEL));
	else if (GetModelName() == NULL_STRING)
		SetModelName(AllocPooledString(WILSON_MODEL));

	PrecacheModel(STRING(GetModelName()));
	PrecacheModel( FLOOR_TURRET_GLOW_SPRITE );

	// Hope that the original floor turret allows us to use them
	//ADD_CUSTOM_ACTIVITY( CNPC_FloorTurret, ACT_FLOOR_TURRET_OPEN );
	//ADD_CUSTOM_ACTIVITY( CNPC_FloorTurret, ACT_FLOOR_TURRET_CLOSE );
	//ADD_CUSTOM_ACTIVITY( CNPC_FloorTurret, ACT_FLOOR_TURRET_CLOSED_IDLE );
	//ADD_CUSTOM_ACTIVITY( CNPC_FloorTurret, ACT_FLOOR_TURRET_OPEN_IDLE );
	//ADD_CUSTOM_ACTIVITY( CNPC_FloorTurret, ACT_FLOOR_TURRET_FIRE );

	//PrecacheScriptSound( "NPC_FloorTurret.Retire" );
	//PrecacheScriptSound( "NPC_FloorTurret.Deploy" );
	//PrecacheScriptSound( "NPC_FloorTurret.Move" );
	//PrecacheScriptSound( "NPC_FloorTurret.Activate" );
	//PrecacheScriptSound( "NPC_FloorTurret.Alert" );
	//PrecacheScriptSound( "NPC_FloorTurret.Die" );
	//PrecacheScriptSound( "NPC_FloorTurret.Retract");
	//PrecacheScriptSound( "NPC_FloorTurret.Alarm");
	//PrecacheScriptSound( "NPC_FloorTurret.Ping");
	//PrecacheScriptSound( "NPC_FloorTurret.DryFire");
	PrecacheScriptSound( "NPC_Wilson.Destruct" );

	PrecacheScriptSound( "RagdollBoogie.Zap" );

	if (!m_bStatic)
	{
		PrecacheParticleSystem( "explosion_turret_break" );
	}

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::Spawn()
{
	Precache();

	SetModel(STRING(GetModelName()));

	BaseClass::Spawn();

	// Wilson has to like Bad Cop for his AI to work.
	AddClassRelationship( CLASS_PLAYER, D_LI, 0 );

	AddEFlags( EFL_NO_DISSOLVE );

	// Will-E has no "head turning", but he does have gestures, which counts as an animated face.
	CapabilitiesAdd( /*bits_CAP_TURN_HEAD |*/ bits_CAP_ANIMATEDFACE );

	CapabilitiesAdd( bits_CAP_SQUAD );

	// Add to the player's squad if we have no squad name
	if (!GetSquad())
		AddToSquad( GetPlayerSquadName() );

	//NPCInit();

	SetBlocksLOS( false );

	m_HackedGunPos	= Vector( 0, 0, 12.75 );
	SetViewOffset( EyeOffset( ACT_IDLE ) );
	m_flFieldOfView	= 0.4f; // 60 degrees
	m_takedamage	= DAMAGE_EVENTS_ONLY;
	m_iHealth		= 100;
	m_iMaxHealth	= 100;

	SetPoseParameter( m_poseAim_Yaw, 0 );
	SetPoseParameter( m_poseAim_Pitch, 0 );

	m_iszIdleExpression = MAKE_STRING("scenes/npc/wilson/expression_idle.vcd");
	m_iszAlertExpression = MAKE_STRING("scenes/npc/wilson/expression_alert.vcd");
	m_iszCombatExpression = MAKE_STRING("scenes/npc/wilson/expression_combat.vcd");
	m_iszDeathExpression = MAKE_STRING("scenes/npc/wilson/expression_dead.vcd");

	//m_iMuzzleAttachment = LookupAttachment( "eyes" );
	m_iEyeAttachment = LookupAttachment( "light" );

	if (m_bDead)
	{
		SetEyeState( TURRET_EYE_DEAD );
		AddContext( "classname:none" );
	}
	else
	{
		SetEyeState( TURRET_EYE_DORMANT );
	}

	if (m_bDamaged)
	{
		AddContext( "wilson_damaged:1" );

		// Get a tesla effect on our hitboxes permanently
		SetContextThink( &CNPC_Wilson::TeslaThink, gpGlobals->curtime, g_DamageZapContext );
		m_flTeslaStopTime = FLT_MAX;
	}
	
	if (m_bAutoSetLocator)
	{
		if (UTIL_GetLocalPlayer())
		{
			// Set the locator target now
			AutoSetLocatorThink();
		}
		else
		{
			// Wait for the player to spawn
			SetContextThink( &CNPC_Wilson::AutoSetLocatorThink, gpGlobals->curtime, g_AutoSetLocatorContext );
		}
	}

	SetUse( &CNPC_Wilson::Use );

	// Don't allow us to skip animation setup because our attachments are critical to us!
	SetBoneCacheFlags( BCF_NO_ANIMATION_SKIP );

	Activate();

	//SetState(NPC_STATE_IDLE);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::Activate( void )
{
	BaseClass::Activate();

	// Force the eye state to the current state so that our glows are recreated after transitions
	SetEyeState( m_iEyeState );

	if ( !m_pMotionController && !m_bStatic )
	{
		// Create the motion controller
		m_pMotionController = CTurretTipController::CreateTipController( this );

		// Enable the controller
		if ( m_pMotionController != NULL )
		{
			m_pMotionController->Enable();
		}
	}

	RefreshCameraTargets();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Wilson::KeyValue( const char *szKeyName, const char *szValue )
{
	bool bResult = BaseClass::KeyValue( szKeyName, szValue );

	if( !bResult )
	{
		// Temporary until we can change saves and rename m_bOmniscient
		if (FStrEq( szKeyName, "Omnipresent" ))
		{
			m_bOmniscient = atoi(szValue) != 0 ? true : false;
			return true;
		}
	}

	return bResult;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::NPCInit( void )
{
	BaseClass::NPCInit();

	if (m_bOmniscient)
	{
		// Infinite look distance needed to see through distant cameras
		SetDistLook( 9999999.0f );
		m_flDistTooFar = 9999999.0f;
	}
}

//-----------------------------------------------------------------------------

bool CNPC_Wilson::CreateVPhysics( void )
{
	//Spawn our physics hull
	IPhysicsObject *pPhys = VPhysicsInitNormal( SOLID_VPHYSICS, 0, false );
	if ( pPhys == NULL )
	{
		DevMsg( "%s unable to spawn physics object!\n", GetDebugName() );
		return false;
	}

	if ( m_bStatic )
	{
		pPhys->EnableMotion( false );
		PhysSetGameFlags(pPhys, FVPHYSICS_NO_PLAYER_PICKUP);
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
unsigned int CNPC_Wilson::PhysicsSolidMaskForEntity( void ) const
{
	// Wilson should ignore NPC clips
	return BaseClass::PhysicsSolidMaskForEntity() & ~CONTENTS_MONSTERCLIP;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::UpdateOnRemove( void )
{
	if ( m_pMotionController != NULL )
	{
		UTIL_Remove( m_pMotionController );
		m_pMotionController = NULL;
	}

	if ( m_hEyeGlow != NULL )
	{
		UTIL_Remove( m_hEyeGlow );
		m_hEyeGlow = NULL;
	}

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::AutoSetLocatorThink()
{
	if (UTIL_GetLocalPlayer())
	{
		CHL2_Player *pPlayer = assert_cast<CHL2_Player*>( UTIL_GetLocalPlayer() );
		{
			// Note that this will override any existing locator target entity
			pPlayer->SetLocatorTargetEntity( this );
		}

		SetNextThink( TICK_NEVER_THINK, g_AutoSetLocatorContext );
	}
	else
	{
		SetNextThink( gpGlobals->curtime + 0.2f, g_AutoSetLocatorContext );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::Touch( CBaseEntity *pOther )
{
	// Do this dodgy trick to avoid TestPlayerPushing in CAI_PlayerAlly.
	CAI_BaseActor::Touch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_OnPlayerUse.FireOutput( pActivator, this );

	CBasePlayer *pPlayer = ToBasePlayer( pActivator );
	if ( pPlayer )
	{
		if (!m_bStatic)
			pPlayer->PickupObject( this, false );

		SpeakIfAllowed( TLK_USE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
//-----------------------------------------------------------------------------
int CNPC_Wilson::OnTakeDamage( const CTakeDamageInfo &info )
{
	CTakeDamageInfo	newInfo = info;

	if ( info.GetDamageType() & (DMG_SLASH|DMG_CLUB) )
	{
		// Take extra force from melee hits
		newInfo.ScaleDamageForce( 2.0f );
		
		// Disable our upright controller for some time
		if ( m_pMotionController != NULL )
		{
			m_pMotionController->Suspend( 2.0f );
		}
	}
	else if ( info.GetDamageType() & DMG_BLAST )
	{
		newInfo.ScaleDamageForce( 2.0f );
	}
	else if ( (info.GetDamageType() & DMG_BULLET) && !(info.GetDamageType() & DMG_BUCKSHOT) )
	{
		// Bullets, but not buckshot, do extra push
		newInfo.ScaleDamageForce( 2.5f );
	}

	// Manually apply vphysics because AI_BaseNPC takedamage doesn't call back to CBaseEntity OnTakeDamage
	VPhysicsTakeDamage( newInfo );

	// Respond to damage
	bool bTriggerHurt = info.GetInflictor() && info.GetInflictor()->ClassMatches("trigger_hurt");
	if ( !(info.GetDamageType() & DMG_RADIATION) || bTriggerHurt )
	{
		AI_CriteriaSet modifiers;
		ModifyOrAppendDamageCriteria(modifiers, info);
		modifiers.AppendCriteria("hitgroup", UTIL_VarArgs("%i", LastHitGroup()));
		SpeakIfAllowed( TLK_WOUND, modifiers );
	}

	if (bTriggerHurt || (m_hDamageFilter && static_cast<CBaseFilter*>(m_hDamageFilter.Get())->PassesDamageFilter(info)))
	{
		// Slowly take damage from trigger_hurts! (or anyone who passes our damage filter if we have one)
		m_iHealth -= info.GetDamage();

		// See this method's declaration comment for more info
		SetLastDamageTime(gpGlobals->curtime);

		if (m_iHealth <= 0)
		{
			Event_Killed( info );
			return 0;
		}
	}

	if (info.GetDamageType() & (DMG_SHOCK | DMG_ENERGYBEAM) && info.GetDamage() >= WILSON_TESLA_DAMAGE_THRESHOLD)
	{
		// Get a tesla effect on our hitboxes for a little while.
		SetContextThink( &CNPC_Wilson::TeslaThink, gpGlobals->curtime, g_DamageZapContext );
		m_flTeslaStopTime = gpGlobals->curtime + 2.0f;
	}

	return BaseClass::OnTakeDamage(info);
}

//-----------------------------------------------------------------------------
// Purpose: Make Will-E look like he's about to explode.
//-----------------------------------------------------------------------------
void CNPC_Wilson::TeslaThink()
{
	// Don't zap if dead
	if (m_bDead)
	{
		m_flTeslaStopTime = gpGlobals->curtime;
		return;
	}

	CEffectData data;
	float flNextTeslaThinkTime;
	data.m_nEntIndex = entindex();
	data.m_flMagnitude = 3;
	data.m_flScale = 0.5f;
	DispatchEffect( "TeslaHitboxes", data );
	EmitSound( "RagdollBoogie.Zap" );

	// Damaged Wilson has a much slower rate of zaps
	if (m_bDamaged)
	{
		flNextTeslaThinkTime = gpGlobals->curtime + random->RandomFloat( 5.0f, 10.0f );
	}
	else
	{
		flNextTeslaThinkTime = gpGlobals->curtime + random->RandomFloat( 0.1f, 0.3f );
	}

	if ( gpGlobals->curtime < m_flTeslaStopTime )
	{
		SetContextThink( &CNPC_Wilson::TeslaThink, flNextTeslaThinkTime, g_DamageZapContext );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
//-----------------------------------------------------------------------------
void CNPC_Wilson::Event_Killed( const CTakeDamageInfo &info )
{
	if (info.GetAttacker())
	{
		info.GetAttacker()->Event_KilledOther(this, info);
	}

	if ( CBasePlayer *pPlayer = UTIL_GetLocalPlayer() )
	{
		pPlayer->Event_NPCKilled(this, info);
	}

	SetState( NPC_STATE_DEAD );

	// WILSOOOOON!!!
	m_OnDestroyed.FireOutput( info.GetAttacker(), this );

	if (!npc_wilson_depressing_death.GetBool())
	{
		// Explode
		Vector vecUp;
		GetVectors( NULL, NULL, &vecUp );
		Vector vecOrigin = WorldSpaceCenter() + ( vecUp * 12.0f );

		// Our effect
		DispatchParticleEffect( "explosion_turret_break", vecOrigin, GetAbsAngles() );

		// Ka-boom!
		RadiusDamage( CTakeDamageInfo( this, info.GetAttacker(), 25.0f, DMG_BLAST ), vecOrigin, (10*12), CLASS_NONE, this );

		EmitSound( "NPC_Wilson.Destruct" );

		breakablepropparams_t params( GetAbsOrigin(), GetAbsAngles(), vec3_origin, RandomAngularImpulse( -800.0f, 800.0f ) );
		params.impactEnergyScale = 1.0f;
		params.defCollisionGroup = COLLISION_GROUP_INTERACTIVE;

		// no damage/damage force? set a burst of 100 for some movement
		params.defBurstScale = 100;
		PropBreakableCreateAll( GetModelIndex(), VPhysicsGetObject(), params, this, -1, true );

		// Throw out some small chunks too obscure the explosion even more
		CPVSFilter filter( vecOrigin );
		for ( int i = 0; i < 4; i++ )
		{
			Vector gibVelocity = RandomVector(-100,100);
			int iModelIndex = modelinfo->GetModelIndex( g_PropDataSystem.GetRandomChunkModel( "MetalChunks" ) );	
			te->BreakModel( filter, 0.0, vecOrigin, GetAbsAngles(), Vector(40,40,40), gibVelocity, iModelIndex, 150, 4, 2.5, BREAK_METAL );
		}

		// We're done!
		UTIL_Remove( this );
	}
	else
	{
		// Shut down
		SetEyeState( TURRET_EYE_DEAD );
		DeathSound( info );

		m_lifeState = LIFE_DEAD;
		SetThink( NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Will-E got involved in a bloody accident!
//-----------------------------------------------------------------------------
void CNPC_Wilson::Event_KilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info )
{
	if (!IsAlive())
		return;

	AI_CriteriaSet modifiers;

	SetSpeechTarget( FindSpeechTarget( AIST_PLAYERS ) );

	ModifyOrAppendDamageCriteria(modifiers, info);
	ModifyOrAppendEnemyCriteria(modifiers, pVictim);

	SpeakIfAllowed(TLK_ENEMY_DEAD, modifiers);
}

//-----------------------------------------------------------------------------
// Purpose: Whether this should return carry angles
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Wilson::HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer )
{
	return m_bUseCarryAngles;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const QAngle
//-----------------------------------------------------------------------------
QAngle CNPC_Wilson::PreferredCarryAngles( void )
{
	// FIXME: Embed this into the class
	static QAngle g_prefAngles;

	Vector vecUserForward;
	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	pPlayer->EyeVectors( &vecUserForward );

	g_prefAngles.Init();

	// Always look at the player as if you're saying "HELOOOOOO"
	g_prefAngles.y = 180.0f;
	
	return g_prefAngles;
}

//-----------------------------------------------------------------------------
// Purpose: Determines whether the turret is upright enough to function
// Output : Returns true if the turret is tipped over
//-----------------------------------------------------------------------------
inline bool CNPC_Wilson::OnSide( void )
{
	Vector	up;
	GetVectors( NULL, NULL, &up );

	return ( DotProduct( up, Vector(0,0,1) ) < 0.40f );
}

//------------------------------------------------------------------------------
// Do we have a physics attacker?
//------------------------------------------------------------------------------
CBasePlayer *CNPC_Wilson::HasPhysicsAttacker( float dt )
{
	// If the player is holding me now, or I've been recently thrown
	// then return a pointer to that player
	if ( IsHeldByPhyscannon( ) || (gpGlobals->curtime - dt <= m_flLastPhysicsInfluenceTime) )
	{
		return m_hPhysicsAttacker;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::NPCThink()
{
	BaseClass::NPCThink();

	if (HasCondition(COND_IN_PVS) && (!m_bOmniscient || HasCondition(COND_SEE_PLAYER)))
	{
		// Needed for choreography reasons
		AimGun();

		// m_flLastSawPlayerTime is a base variable that wasn't designed for this, but it isn't used for anything other than speech,
		// so we're good on re-using it here.
		// 
		// If we haven't left the player's PVS since we last actually saw them, still consider it as "seeing" the player.
		// This is so the player doesn't just stand behind Will-E for a few minutes and surprise him by going back in front of him.
		if (!m_bPlayerLeftPVS)
			m_flLastSawPlayerTime = gpGlobals->curtime;

		if (GetExpression() && IsRunningScriptedSceneWithFlexAndNotPaused(this, false, GetExpression()))
			ClearExpression();
		else if (!GetExpression())
			PlayExpressionForState(GetState());
	}
	else
	{
		if (!m_bPlayerLeftPVS)
			m_bPlayerLeftPVS = true;
	}

	// See if we've tipped, but only do this if we're not being carried.
	// This is based on CNPC_FloorTurret::PreThink()'s tip detection functionality.
	if ( !IsBeingCarriedByPlayer() && !m_bStatic )
	{
		if ( OnSide() == false )
		{
			// TODO: NPC knock-over interaction handling?

			// Only un-tip if we're not in low gravity
			if (m_bTipped && !InLowGravity())
			{
				// Enable the tip controller
				m_pMotionController->Enable( true );

				m_bTipped = false;
			}
		}
		else
		{
			// Checking m_bTipped is how we figure out whether this is our first think since we've been tipped
			if (m_bTipped == false)
			{
				if (!GetExpresser()->IsSpeaking())
				{
					AI_CriteriaSet modifiers;

					// Measure our distance between ourselves and the ground.
					trace_t tr;
					AI_TraceLine(WorldSpaceCenter(), WorldSpaceCenter() - Vector(0, 0, WILSON_MAX_TIPPED_HEIGHT), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

					modifiers.AppendCriteria( "distancetoground", UTIL_VarArgs("%f", (tr.fraction * WILSON_MAX_TIPPED_HEIGHT)) );

					Speak( TLK_TIPPED, modifiers );
				}

				// Might want to get a real activator... (e.g. a last physics attacker)
				m_OnTipped.FireOutput( AI_GetSinglePlayer(), this );

				// Disable the tip controller
				m_pMotionController->Enable( false );

				m_bTipped = true;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Wilson::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();

	// Simple health regeneration.
	// The one in CAI_PlayerAlly::PrescheduleThink() woudn't work because Wilson
	// doesn't take normal damage and therefore wouldn't be able to use TakeHealth().
	if( GetHealth() < GetMaxHealth() && gpGlobals->curtime - GetLastDamageTime() > 2.0f )
	{
		switch (g_pGameRules->GetSkillLevel())
		{
		case SKILL_EASY:		m_iHealth += 3; break;
		default:				m_iHealth += 2; break;
		case SKILL_HARD:		m_iHealth += 1; break;
		}

		if (m_iHealth > m_iMaxHealth)
			m_iHealth = m_iMaxHealth;
	}

	if (m_hAttachedVehicle && m_flNextClearanceCheck < gpGlobals->curtime)
	{
		Vector vecForward = EyeDirection3D();
		Vector velocity = m_hAttachedVehicle->GetSmoothedVelocity();

		// Make sure we can see the direction we're going in
		if (velocity.LengthSqr() >= Square(npc_wilson_clearance_speed_threshold.GetFloat()) && vecForward.Dot(velocity) > 0.0f)
		{
			Vector vecOrigin = GetAbsOrigin();

			CTraceFilterSkipTwoEntities pFilter(this, m_hAttachedVehicle, COLLISION_GROUP_NONE);
			trace_t tr;
			UTIL_TraceHull( vecOrigin, vecOrigin + velocity, GetHullMins(), GetHullMaxs(), MASK_SOLID, &pFilter, &tr );
			if (tr.fraction != 1.0f)
			{
				// We need to see how "head-on" to the surface we are
				// (copied from passenger behavior code)
				float impactDot = DotProduct( tr.plane.normal, vecForward );

				// Don't warn over grazing blows or slopes
				if ( impactDot < -0.8f && tr.plane.normal.z < 0.75f )
				{
					AI_CriteriaSet modifiers;

					// Check if the APC is gonna hit it too
					trace_t tr2;
					vecOrigin = m_hAttachedVehicle->WorldSpaceCenter() + (velocity);
					UTIL_TraceLine( vecOrigin, vecOrigin + velocity, MASK_SOLID, &pFilter, &tr2 );
					if (tr2.fraction != 1.0f)
						modifiers.AppendCriteria( "complete_crash", "1" );

					// We're gonna get knocked off!
					SpeakIfAllowed( TLK_APC_LOW_CLEARANCE, modifiers );

					if (npc_wilson_clearance_debug.GetBool())
					{
						// Red for collision
						NDebugOverlay::BoxAngles( tr.endpos, GetHullMins(), GetHullMaxs(), GetAbsAngles(), 255, 0, 0, 128, 10.0f );

						NDebugOverlay::Axis( tr2.endpos, GetAbsAngles(), 5.0f, true, 10.0f );
					}
				}
				else if (npc_wilson_clearance_debug.GetBool())
				{
					// Gray for ignore
					NDebugOverlay::BoxAngles( tr.endpos, GetHullMins(), GetHullMaxs(), GetAbsAngles(), 64, 64, 64, 128, 5.0f );
				}
			}
			else if (npc_wilson_clearance_debug.GetBool())
			{
				// Green for clear
				NDebugOverlay::BoxAngles( tr.endpos, GetHullMins(), GetHullMaxs(), GetAbsAngles(), 0, 255, 0, 128, 2.0f );
			}
		}

		m_flNextClearanceCheck = gpGlobals->curtime + 1.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::GatherConditions( void )
{
	// Will-E doesn't use sensing code, but COND_SEE_PLAYER is important for CAI_PlayerAlly stuff.
	//CBasePlayer *pLocalPlayer = AI_GetSinglePlayer();
	//if (pLocalPlayer && FInViewCone(pLocalPlayer->EyePosition()) && FVisible(pLocalPlayer))
	//{
	//	Assert( GetSenses()->HasSensingFlags(SENSING_FLAGS_DONT_LOOK) );
	//
	//	SetCondition( COND_SEE_PLAYER );
	//}

	BaseClass::GatherConditions();

	// Handle speech AI. Don't do AI speech if we're in scripts unless permitted by the EnableSpeakWhileScripting input.
	if ( m_NPCState == NPC_STATE_IDLE || m_NPCState == NPC_STATE_ALERT || m_NPCState == NPC_STATE_COMBAT ||
		( ( m_NPCState == NPC_STATE_SCRIPT ) && CanSpeakWhileScripting() ) )
	{
		DoCustomSpeechAI();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::GatherEnemyConditions( CBaseEntity *pEnemy )
{
	BaseClass::GatherEnemyConditions( pEnemy );

	if ( GetLastEnemyTime() == 0 || gpGlobals->curtime - GetLastEnemyTime() > 30 )
	{
		if ( HasCondition( COND_SEE_ENEMY ) && (WorldSpaceCenter() - pEnemy->WorldSpaceCenter()).LengthSqr() <= Square(384.0f) && pEnemy->Classify() != CLASS_BULLSEYE )
		{
			AI_CriteriaSet modifiers;

			CBasePlayer *pPlayer = AI_GetSinglePlayer();
			if ( pPlayer )
			{
				bool bEnemyInPlayerCone = static_cast<CEZ2_Player*>(pPlayer)->FInTrueViewCone( pEnemy->WorldSpaceCenter() );
				bool bEnemyVisibleToPlayer = pPlayer->FVisible( pEnemy );
				bool bInPlayerCone = static_cast<CEZ2_Player*>(pPlayer)->FInTrueViewCone( WorldSpaceCenter() );
				bool bSeePlayer = pPlayer->IsInAVehicle() ? FVisible( pPlayer->GetVehicleEntity() ) : FVisible( pPlayer );

				// If I can see the player, and the player would see this enemy if they turned around,
				// call it out.
				// This code was ported from CNPC_PlayerCompanion.
				if ( !bEnemyInPlayerCone && bSeePlayer && bEnemyVisibleToPlayer )
				{
					Vector2D vecPlayerView = pPlayer->EyeDirection2D().AsVector2D();
					Vector2D vecToEnemy = ( pEnemy->GetAbsOrigin() - pPlayer->GetAbsOrigin() ).AsVector2D();
					Vector2DNormalize( vecToEnemy );
				
					if ( DotProduct2D(vecPlayerView, vecToEnemy) <= -0.75 )
					{
						modifiers.AppendCriteria("dangerloc", "behind");
						SpeakIfAllowed( TLK_WATCHOUT, modifiers );
						return;
					}
				}

				// If neither us or the enemy are visible to the player,
				// and the enemy is either attacking nothing or chasing the player,
				// call it out.
				else if ( ( (!bInPlayerCone || !bSeePlayer) && (!bEnemyInPlayerCone || !bEnemyVisibleToPlayer) ) || pPlayer->GetFlags() & FL_NOTARGET )
				{
					if ( !pEnemy->GetEnemy() || pEnemy->GetEnemy() == pPlayer )
						modifiers.AppendCriteria("alert_player", "1");
				}
			}

			SetSpeechTarget( pEnemy );

			SpeakIfAllowed(TLK_STARTCOMBAT, modifiers);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Wilson::TranslateSchedule( int scheduleType )
{
	switch( scheduleType )
	{
	case SCHED_ALERT_STAND:
		return SCHED_WILSON_ALERT_STAND;
		break;
	}

	return scheduleType;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::OnSeeEntity( CBaseEntity *pEntity )
{
	BaseClass::OnSeeEntity(pEntity);

	if ( pEntity->IsPlayer() )
	{
		if ( pEntity->IsEFlagSet(EFL_IS_BEING_LIFTED_BY_BARNACLE) )
		{
			SetSpeechTarget( pEntity );
			SpeakIfAllowed( TLK_ALLY_IN_BARNACLE );
		}
		else if (gpGlobals->curtime - m_flLastSawPlayerTime > 60.0f && m_flLastSawPlayerTime != -1.0f /*&& m_flCarryTime < m_flLastSawPlayerTime*/)
		{
			// Oh, hey, Bad Cop! Long time no see!
			// Needs to be Speak() to override any currently running speech
			SetSpeechTarget( pEntity );
			Speak( TLK_FOUNDPLAYER );
			m_flPlayerNearbyTime = -1.0f;
		}

		m_bPlayerLeftPVS = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::OnFriendDamaged( CBaseCombatCharacter *pSquadmate, CBaseEntity *pAttackerEnt )
{
	BaseClass::OnFriendDamaged( pSquadmate, pAttackerEnt );

	CAI_BaseNPC *pAttacker = pAttackerEnt->MyNPCPointer();
	if ( pAttacker )
	{
		CBasePlayer *pPlayer = AI_GetSinglePlayer();
		if ( pPlayer && ( pPlayer->GetAbsOrigin().AsVector2D() - GetAbsOrigin().AsVector2D() ).LengthSqr() < Square( 25*12 ) && IsAllowedToSpeak( TLK_WATCHOUT ) )
		{
			if ( !pPlayer->FInViewCone( pAttacker ) )
			{
				Vector2D vPlayerDir = pPlayer->EyeDirection2D().AsVector2D();
				Vector2D vEnemyDir = pAttacker->EyePosition().AsVector2D() - pPlayer->EyePosition().AsVector2D();
				vEnemyDir.NormalizeInPlace();
				float dot = vPlayerDir.Dot( vEnemyDir );
				if ( dot < 0 )
					Speak( TLK_WATCHOUT, "dangerloc:behind" );
				else if ( ( pPlayer->GetAbsOrigin().AsVector2D() - pAttacker->GetAbsOrigin().AsVector2D() ).LengthSqr() > Square( 40*12 ) )
					Speak( TLK_WATCHOUT, "dangerloc:far" );
			}
			else if ( pAttacker->GetAbsOrigin().z - pPlayer->GetAbsOrigin().z > 128 )
			{
				Speak( TLK_WATCHOUT, "dangerloc:above" );
			}
			else if ( pAttacker->GetHullType() <= HULL_TINY && ( pPlayer->GetAbsOrigin().AsVector2D() - pAttacker->GetAbsOrigin().AsVector2D() ).LengthSqr() > Square( 100*12 ) )
			{
				Speak( TLK_WATCHOUT, "dangerloc:far" );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Wilson::IsValidEnemy( CBaseEntity *pEnemy )
{
	if (!BaseClass::IsValidEnemy( pEnemy ))
		return false;

	// HACKHACK: Just completely ignore bullseyes for now
	if (pEnemy->Classify() == CLASS_BULLSEYE)
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Wilson::CanBeAnEnemyOf( CBaseEntity *pEnemy )
{ 
	// Don't be anyone's enemy unless it's needed for something
	if (!m_bCanBeEnemy)
	{
		return false;
	}

	return BaseClass::CanBeAnEnemyOf(pEnemy);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Disposition_t CNPC_Wilson::IRelationType( CBaseEntity *pTarget )
{
	// hackhack - Wilson keeps telling the beast on Bad Cop.
	// For now, Wilson and zombie assassins like each other
	if (FClassnameIs( pTarget, "npc_zassassin" ))
	{
		return D_LI;
	}

	if (!pTarget)
		return D_NU;

	Disposition_t base = BaseClass::IRelationType(pTarget);

	// If we're in a squad and our base relationship is default, use the squad's relationships
	if (GetSquad() && base == GetDefaultRelationshipDisposition(pTarget->Classify()))
	{
		if (MAKE_STRING(m_pSquad->GetName()) == GetPlayerSquadName() && UTIL_GetLocalPlayer())
		{
			// Get the player's disposition
			CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
			if (pPlayer && pTarget != pPlayer)
				base = pPlayer->IRelationType(pTarget);
		}
		else if (!GetSquad()->IsLeader(this))
		{
			// Get the squad leader's disposition
			CAI_BaseNPC *pLeader = GetSquad()->GetLeader();
			if (pLeader)
				base = pLeader->IRelationType(pTarget);
		}
	}

	return base;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Wilson::IsSilentSquadMember() const
{
	// For now, always be silent.
	return true;
}

void CNPC_Wilson::SetDamaged( bool bDamaged )
{
	if (m_bDamaged == bDamaged)
		return;

	m_bDamaged = bDamaged;

	if (m_bDamaged)
	{
		SetModelName( AllocPooledString( DAMAGED_MODEL ) );
		SetModel( STRING( GetModelName() ) );
		AddContext( "wilson_damaged:1" );

		// Get a tesla effect on our hitboxes permanently
		SetContextThink( &CNPC_Wilson::TeslaThink, gpGlobals->curtime, g_DamageZapContext );
		m_flTeslaStopTime = FLT_MAX;
	}
	else
	{
		SetModelName( AllocPooledString( WILSON_MODEL ) );
		SetModel( STRING( GetModelName() ) );
		RemoveContext( "wilson_damaged:1" );
		m_flTeslaStopTime = gpGlobals->curtime;
	}

	// Reset physics
	CreateVPhysics();
}

void CNPC_Wilson::SetPlayingDead( bool bPlayingDead )
{
	if (m_bDead == bPlayingDead)
		return;

	m_bDead = bPlayingDead;

	if (m_bDead)
	{
		// No longer trigger any responses
		AddContext( "classname:none" );
		SetEyeState( TURRET_EYE_DEAD );
		m_flTeslaStopTime = gpGlobals->curtime;
	}
	else
	{
		RemoveContext( "classname:none" );
		SetEyeState( TURRET_EYE_DORMANT );
		m_flTeslaStopTime = gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::AimGun()
{
	// Don't aim at enemies anymore. This instead just calls RelaxAim() to relax aim for the choreogrpahy.
	//if (GetEnemy() && !IsRunningScriptedScene(this) && !OnSide())
	//{
	//	Vector vecShootOrigin;
	//
	//	vecShootOrigin = Weapon_ShootPosition();
	//	Vector vecShootDir = GetShootEnemyDir( vecShootOrigin, false );
	//
	//	SetAim( vecShootDir );
	//}
	//else
	{
		RelaxAim();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Wilson::FInViewCone( const Vector &vecSpot )
{
	if ( m_bSeeThroughPlayer )
	{
		// Bad Cop has special eye
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		if ( pPlayer && pPlayer->FInViewCone( vecSpot ) )
			return true;
	}

	if ( m_hCameraTargets.Count() > 0 )
	{
		FOR_EACH_VEC( m_hCameraTargets, i )
		{
			if ( !m_hCameraTargets[i]->IsEnabled() )
				continue;

			if ( m_hCameraTargets[i]->FInViewCone( vecSpot ) )
				return true;
		}

		// None of the cameras saw it
		// If we're omniscient, then our actual POV is irrelevant
		if (m_bOmniscient)
			return false;
	}

	return BaseClass::FInViewCone( vecSpot );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Wilson::FVisible( CBaseEntity *pEntity, int traceMask, CBaseEntity **ppBlocker )
{
	if ( m_bSeeThroughPlayer )
	{
		// Bad Cop has special eye
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		if ( pEntity == pPlayer || (pPlayer && pPlayer->FVisible( pEntity )) )
			return true;
	}

	if ( m_hCameraTargets.Count() > 0 )
	{
		FOR_EACH_VEC( m_hCameraTargets, i )
		{
			if ( !m_hCameraTargets[i]->IsEnabled() )
				continue;

			// Note that this will not use the visibility cache normally used by CBaseCombatCharacters (which is acceptable)
			if ( m_hCameraTargets[i]->FVisible( pEntity, traceMask, ppBlocker ) )
				return true;
		}

		// None of the cameras saw it
		// If we're omniscient, then our actual POV is irrelevant
		if (m_bOmniscient)
			return false;
	}

	return BaseClass::FVisible( pEntity, traceMask, ppBlocker );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Wilson::FVisible( const Vector &vecTarget, int traceMask, CBaseEntity **ppBlocker )
{
	if ( m_bSeeThroughPlayer )
	{
		// Bad Cop has special eye
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		if ( pPlayer && pPlayer->FVisible( vecTarget ) )
			return true;
	}

	if ( m_hCameraTargets.Count() > 0 )
	{
		FOR_EACH_VEC( m_hCameraTargets, i )
		{
			if ( !m_hCameraTargets[i]->IsEnabled() )
				continue;

			if ( m_hCameraTargets[i]->FVisible( vecTarget, traceMask, ppBlocker ) )
				return true;
		}

		// None of the cameras saw it
		// If we're omniscient, then our actual POV is irrelevant
		if (m_bOmniscient)
			return false;
	}

	return BaseClass::FVisible( vecTarget, traceMask, ppBlocker );
}

//-----------------------------------------------------------------------------
// Purpose: Gets the first camera which can see the target
//-----------------------------------------------------------------------------
CWilsonCamera *CNPC_Wilson::GetCameraForTarget( CBaseEntity *pTarget )
{
	FOR_EACH_VEC( m_hCameraTargets, i )
	{
		if ( !m_hCameraTargets[i]->IsEnabled() )
			continue;

		if ( m_hCameraTargets[i]->FInViewCone( pTarget->WorldSpaceCenter() ) && m_hCameraTargets[i]->FVisible( pTarget ) )
			return m_hCameraTargets[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this NPC can hear the specified sound
//-----------------------------------------------------------------------------
bool CNPC_Wilson::QueryHearSound( CSound *pSound )
{
	CBaseEntity *pOwner = pSound->m_hOwner;
	if ( pOwner )
	{
		// Check if this is a vehicle
		IServerVehicle *pVehicle = pSound->m_hOwner->GetServerVehicle();
		if ( pVehicle )
		{
			// Do not hear sounds from the vehicle we're attached to.
			if ( pOwner == m_hAttachedVehicle )
				return false;

			// For the next code, check the vehicle's driver instead of the vehicle itself.
			pOwner = pVehicle->GetPassenger();
		}

		// We shouldn't hear sounds emitted by entities/players we like.
		if ( IRelationType(pOwner) == D_LI )
			return false;
	}

	return BaseClass::QueryHearSound( pSound );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::HandlePrescheduleIdleSpeech( void )
{
	AISpeechSelection_t selection;
	if ( DoIdleSpeechAI( &selection, GetState() ) )
	{
		SetSpeechTarget( selection.hSpeechTarget );
#ifdef NEW_RESPONSE_SYSTEM
		SpeakDispatchResponse( selection.concept.c_str(), selection.Response );
#else
		SpeakDispatchResponse( selection.concept.c_str(), selection.pResponse );
#endif
		SetNextIdleSpeechTime( gpGlobals->curtime + RandomFloat( 10,20 ) );
	}
	else
	{
		SetNextIdleSpeechTime( gpGlobals->curtime + RandomFloat( 5,10 ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Wilson::DoCustomSpeechAI()
{
	if (HasCondition( COND_HEAR_DANGER ))
	{
		CSound *pSound;
		pSound = GetBestSound( SOUND_DANGER );

		ASSERT( pSound != NULL );

		if ( pSound && (pSound->m_iType & SOUND_DANGER) && !pSound->m_hOwner || IRelationType(pSound->m_hOwner) != D_LI )
		{
			CBaseEntity *pOwner = pSound->m_hOwner;
			AI_CriteriaSet modifiers;
			if (pOwner)
			{
				if (pSound->SoundChannel() == SOUNDENT_CHANNEL_ZOMBINE_GRENADE)
				{
					// Pretend the owner is a grenade (the zombine will be the enemy)
					modifiers.AppendCriteria( "sound_owner", "npc_grenade_frag" );
				}
				else
				{
					modifiers.AppendCriteria( "sound_owner", pSound->m_hOwner->GetClassname() );
				}

				CBaseGrenade *pGrenade = dynamic_cast<CBaseGrenade*>(pOwner);
				if (pGrenade)
					pOwner = pGrenade->GetThrower();

				ModifyOrAppendEnemyCriteria( modifiers, pOwner );
			}

			if ( !(pSound->SoundContext() & (SOUND_CONTEXT_MORTAR|SOUND_CONTEXT_FROM_SNIPER)) || IsOkToCombatSpeak() )
				SpeakIfAllowed( TLK_DANGER, modifiers );
		}
	}

	// The player *must* exist in the level (and the current session must be >2 seconds old) for this speech to be possible
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if (pPlayer && gpGlobals->curtime > 2.0f)
	{
		bool bShouldSpeakGoodbye = (m_flPlayerNearbyTime != -1.0f && gpGlobals->curtime - m_flPlayerNearbyTime >= 30.0f);
		if ( HasCondition(COND_IN_PVS) )
		{
			if ( HasCondition( COND_TALKER_PLAYER_DEAD ) && FInViewCone(pPlayer) && !GetExpresser()->SpokeConcept(TLK_PLDEAD) )
			{
				if ( SpeakIfAllowed( TLK_PLDEAD ) )
					return true;
			}

			// If we're in a vehicle, use the vehicle's origin instead
			Vector vecSearchOrigin = m_hAttachedVehicle ? m_hAttachedVehicle->GetAbsOrigin() : GetAbsOrigin();
			float flDistSqr = (vecSearchOrigin - pPlayer->GetAbsOrigin()).LengthSqr();
			if (flDistSqr >= Square( 300 ))
			{
				m_flPlayerNearbyTime = -1.0f;
				if ( bShouldSpeakGoodbye && SpeakIfAllowed( TLK_GOODBYE, true ) )
					return true;
			}
			else if (m_flPlayerNearbyTime == -1.0f)
			{
				m_flPlayerNearbyTime = gpGlobals->curtime;
			}
		}
		else
		{
			m_flPlayerNearbyTime = -1.0f;
			if ( bShouldSpeakGoodbye && SpeakIfAllowed( TLK_GOODBYE, true ) )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Wilson::DoIdleSpeechAI( AISpeechSelection_t *pSelection, int iState )
{
	// From SelectAlertSpeech()
	CBasePlayer *pTarget = assert_cast<CBasePlayer *>(FindSpeechTarget( AIST_PLAYERS | AIST_FACING_TARGET | AIST_ANY_QUALIFIED ));
	if ( pTarget )
	{
		SetSpeechTarget(pTarget);

		// IsValidSpeechTarget() already verified the player is alive
		float flHealthPerc = ((float)pTarget->m_iHealth / (float)pTarget->m_iMaxHealth);
		if ( flHealthPerc < 1.0 )
		{
			if ( SelectSpeechResponse( TLK_PLHURT, NULL, pTarget, pSelection ) )
				return true;
		}

		if (iState == NPC_STATE_IDLE)
		{
			// Occasionally remind players about things they've neglected.
			{
				// Check for usage based on ammo counts and contexts set in CEZ2_Player.
				if (pTarget->GetAmmoCount("XenGrenade") > 0 && !pTarget->HasContext("xen_grenade_thrown", "1"))
				{
					if ( SelectSpeechResponse( TLK_REMIND_PLAYER, "subject:weapon_hopwire", pTarget, pSelection ) )
						return true;
				}
				else if (pTarget->GetAmmoCount("AR2AltFire") > 0 && !pTarget->HasContext("displacer_used", "1"))
				{
					if ( SelectSpeechResponse( TLK_REMIND_PLAYER, "subject:weapon_displacer_pistol", pTarget, pSelection ) )
						return true;
				}
			}

			// Citizens, etc. only told the player to reload when they themselves were reloading.
			// That can't apply to Will-E and we don't want him to say this in the middle of combat,
			// so we only have Will-E comment on the player's ammo when he's idle.
			CBaseCombatWeapon *pWeapon = pTarget->GetActiveWeapon();
			if( pWeapon && pWeapon->UsesClipsForAmmo1() && 
				pWeapon->Clip1() <= ( pWeapon->GetMaxClip1() * .5 ) &&
				pTarget->GetAmmoCount( pWeapon->GetPrimaryAmmoType() ) )
			{
				if ( SelectSpeechResponse( TLK_PLRELOAD, NULL, pTarget, pSelection ) )
					return true;
			}

			// From SelectIdleSpeech()
			// Player allies normally reduce how much this is spoken while moving,
			// but Will-E is supposed to be moved, so that doesn't apply here.
			if ( ShouldSpeakRandom( TLK_IDLE, 10 ) && SelectSpeechResponse( TLK_IDLE, NULL, pTarget, pSelection ) )
				return true;
		}
	}
	else if ( gpGlobals->curtime - m_flLastSawPlayerTime >= 60.0f )
	{
		// Getting lonely?
		if ( ShouldSpeakRandom( TLK_DARKNESS_LOSTPLAYER, 10 ) && SelectSpeechResponse( TLK_DARKNESS_LOSTPLAYER, NULL, pTarget, pSelection ) )
			return true;
	}

	// Fidget handling
	if (m_fNextFidgetSpeechTime < gpGlobals->curtime)
	{
		// Try again later (set it before we return)
		m_fNextFidgetSpeechTime = gpGlobals->curtime + random->RandomFloat(MIN_IDLE_SPEECH, MAX_IDLE_SPEECH);

		//SpeakIfAllowed(TLK_WILLE_FIDGET);
		if ( HasCondition(COND_IN_PVS) && SelectSpeechResponse( TLK_FIDGET, NULL, NULL, pSelection ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Make us explode
//-----------------------------------------------------------------------------
void CNPC_Wilson::InputSelfDestruct( inputdata_t &inputdata )
{
	const CTakeDamageInfo info(this, inputdata.pActivator, NULL, DMG_GENERIC);
	Event_Killed(info);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason )
{
	m_hPhysicsAttacker = pPhysGunUser;
	m_flLastPhysicsInfluenceTime = gpGlobals->curtime;

	if (gpGlobals->curtime - m_flLastSawPlayerTime > 1.0f)
	{
		// Pretend we're seeing the player
		OnSeeEntity( pPhysGunUser );
		m_flLastSawPlayerTime = gpGlobals->curtime;
	}

	if ( reason != PUNTED_BY_CANNON )
	{
		m_flCarryTime = gpGlobals->curtime;
		m_OnPhysGunPickup.FireOutput( pPhysGunUser, this );

		// We want to use preferred carry angles if we're not nicely upright
		Vector vecToTurret = pPhysGunUser->GetAbsOrigin() - GetAbsOrigin();
		vecToTurret.z = 0;
		VectorNormalize( vecToTurret );

		// We want to use preferred carry angles if we're not nicely upright
		Vector	forward, up;
		GetVectors( &forward, NULL, &up );

		bool bUpright = DotProduct( up, Vector(0,0,1) ) > 0.9f;
		bool bInFront = DotProduct( vecToTurret, forward ) > 0.10f;

		// Correct our angles only if we're not upright or we're mostly in front of the turret
		m_bUseCarryAngles = ( bUpright == false || bInFront );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason )
{
	m_hPhysicsAttacker = pPhysGunUser;
	m_flLastPhysicsInfluenceTime = gpGlobals->curtime;

	m_flCarryTime = -1.0f;
	m_bUseCarryAngles = false;
	m_OnPhysGunDrop.FireOutput( pPhysGunUser, this );

	m_flPlayerDropTime = gpGlobals->curtime + 2.0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Wilson::OnAttemptPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason )
{
	return true;
}

extern int g_interactionCombineBash;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Wilson::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter *sourceEnt)
{
	// Change eye while being scanned
	if ( interactionType == g_interactionArbeitScannerStart )
	{
		AI_CriteriaSet modifiers;
		if (data)
		{
			CArbeitScanner *pScanner = assert_cast<CArbeitScanner*>(static_cast<CBaseEntity*>(data));
			if (pScanner)
				pScanner->AppendContextToCriteria( modifiers );
		}

		SetEyeState(TURRET_EYE_SEEKING_TARGET);
		SpeakIfAllowed( TLK_SCAN_START, modifiers );
		return true;
	}
	if ( interactionType == g_interactionArbeitScannerEnd )
	{
		AI_CriteriaSet modifiers;
		if (data)
		{
			CArbeitScanner *pScanner = assert_cast<CArbeitScanner*>(static_cast<CBaseEntity*>(data));
			if (pScanner)
				pScanner->AppendContextToCriteria( modifiers );
		}

		SetEyeState(TURRET_EYE_DORMANT);
		SpeakIfAllowed( TLK_SCAN_END, modifiers );
		return true;
	}


	if ( interactionType == g_interactionXenGrenadeConsume )
	{
		if (true)
		{
			// "Don't be sucked up" case, which is default

			// Invoke effects similar to that of being lifted by a barnacle.
			AddEFlags( EFL_IS_BEING_LIFTED_BY_BARNACLE );
			AddSolidFlags( FSOLID_NOT_SOLID );
			AddEffects( EF_NODRAW );

			if ( m_hEyeGlow )
			{
				m_hEyeGlow->AddEffects( EF_NODRAW );
			}

			IPhysicsObject *pPhys = VPhysicsGetObject();
			if ( pPhys )
			{
				pPhys->EnableMotion( false );
			}

			// Turn off projected texture
			m_bEyeLightEnabled = false;

			// Return true to indicate we shouldn't be sucked up
			return true;
		}
		else
		{
			// "Let suck" case, which could be random or something

			// Maybe have the grenade-thrower as an activator, passed through *data
			// Also, WILSOOOOON!!!
			m_OnDestroyed.FireOutput(this, this);

			// Return false to indicate we should still be sucked up
			return false;
		}
	}

	if ( interactionType == g_interactionXenGrenadeRelease )
	{
		DisplacementInfo_t *info = static_cast<DisplacementInfo_t*>(data);
		if (!info)
			return false;

		Teleport( info->vecTargetPos, &info->pDisplacer->GetAbsAngles(), NULL );

		RemoveEFlags( EFL_IS_BEING_LIFTED_BY_BARNACLE );
		RemoveSolidFlags( FSOLID_NOT_SOLID );
		RemoveEffects( EF_NODRAW );

		if ( m_hEyeGlow )
		{
			m_hEyeGlow->RemoveEffects( EF_NODRAW );
		}

		IPhysicsObject *pPhys = VPhysicsGetObject();
		if ( pPhys )
		{
			pPhys->EnableMotion( true );
			pPhys->Wake();
		}

		// Pretend we spawned from a recipe
		info->pSink->SpawnEffects( this );

		SpeakIfAllowed( TLK_XEN_GRENADE_RELEASE );

		// Turn on projected texture
		m_bEyeLightEnabled = true;

		return true;
	}


	if ( interactionType == g_interactionAPCConstrain )
	{
		m_hAttachedVehicle = (CBaseEntity*)data;
		return false;
	}

	if ( interactionType == g_interactionAPCUnconstrain )
	{
		m_hAttachedVehicle = NULL;
		return false;
	}

	if ( interactionType == g_interactionAPCBreak )
	{
		SpeakIfAllowed( TLK_APC_EJECTED );

		m_hAttachedVehicle = NULL;
		return false;
	}


	if ( interactionType == g_interactionCombineBash )
	{
		// Get knocked away
		Vector forward, up;
		AngleVectors( sourceEnt->GetLocalAngles(), &forward, NULL, &up );
		ApplyAbsVelocityImpulse( forward * 100 + up * 50 );
		CTakeDamageInfo info( sourceEnt, sourceEnt, 30, DMG_CLUB );
		CalculateMeleeDamageForce( &info, forward, GetAbsOrigin() );
		TakeDamage( info );

		EmitSound( "NPC_Combine.WeaponBash" );
		return true;
	}

	return BaseClass::HandleInteraction( interactionType, data, sourceEnt );
}

// Was 256, should be lower so eye is visible
#define WILSON_EYE_GLOW_DEFAULT_BRIGHT 72
#define WILSON_EYE_GLOW_SCAN_BRIGHT 64

#define WILSON_EYE_GLOW_DEFAULT_COLOR 255, 0, 0

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::SetEyeState( eyeState_t state )
{
	// Must have a valid eye to affect
	if ( !m_hEyeGlow )
	{
		// Create our eye sprite
		m_hEyeGlow = CSprite::SpriteCreate( FLOOR_TURRET_GLOW_SPRITE, GetLocalOrigin(), false );
		if ( !m_hEyeGlow )
			return;

		m_hEyeGlow->SetTransparency( kRenderWorldGlow, 255, 0, 0, WILSON_EYE_GLOW_DEFAULT_BRIGHT, kRenderFxNoDissipation );
		m_hEyeGlow->SetAttachment( this, m_iEyeAttachment );
	}

	m_iEyeState = state;

	//Set the state
	switch( state )
	{
	default:
	case TURRET_EYE_DORMANT: // Default, based on the env_sprite in Wilson's temporary physbox prefab.
		PlayExpressionForState( GetState() );
		m_nSkin = 0;
		m_hEyeGlow->SetColor( WILSON_EYE_GLOW_DEFAULT_COLOR );
		m_hEyeGlow->SetBrightness( WILSON_EYE_GLOW_DEFAULT_BRIGHT, 0.5f );
		m_hEyeGlow->SetScale( 0.3f, 0.5f );
		m_iEyeLightBrightness = 255;
		break;

	case TURRET_EYE_SEEKING_TARGET: // Now used for when an Arbeit scanner is scanning Wilson.
		SetExpression( "scenes/npc/wilson/expression_scanning.vcd" );
		m_nSkin = 2;
		m_hEyeGlow->SetColor( WILSON_EYE_GLOW_DEFAULT_COLOR );
		m_hEyeGlow->SetBrightness( WILSON_EYE_GLOW_SCAN_BRIGHT, 0.25f );
		m_iEyeLightBrightness = 128;

		// Blink state stuff was removed, see npc_turret_floor for original

		break;

	case TURRET_EYE_DEAD: //Fade out slowly
		m_nSkin = 1;
		m_hEyeGlow->SetColor( 255, 0, 0 );
		m_hEyeGlow->SetScale( 0.1f, 3.0f );
		m_hEyeGlow->SetBrightness( 0, 3.0f );
		m_iEyeLightBrightness = 0;
		break;
	}

	// For the projected light on the client
	m_iEyeLightBrightness = m_hEyeGlow->GetBrightness();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::ModifyOrAppendCriteria(AI_CriteriaSet& set)
{
    set.AppendCriteria("being_held", m_flCarryTime != -1 ? UTIL_VarArgs("%f", gpGlobals->curtime - m_flCarryTime) : "-1");

	if (m_hPhysicsAttacker)
	{
		set.AppendCriteria("physics_attacker", m_hPhysicsAttacker->GetClassname());
		set.AppendCriteria("last_physics_attack", UTIL_VarArgs("%f", gpGlobals->curtime - m_flLastPhysicsInfluenceTime));
	}
	else
	{
		set.AppendCriteria("physics_attacker", "0");
	}

	set.AppendCriteria("in_vehicle", m_hAttachedVehicle ? m_hAttachedVehicle->GetClassname() : "0");

	// Assume that if we're not using carrying angles, Will-E is not looking at the player.
    set.AppendCriteria("facing_player", m_bUseCarryAngles ? "1" : "0");

	set.AppendCriteria("on_side", OnSide() ? "1" : "0");

	set.AppendCriteria( "speed", UTIL_VarArgs( "%.3f", GetSmoothedVelocity().Length() ) );

    BaseClass::ModifyOrAppendCriteria(set);

	if (GetSpeechTarget() && GetSpeechTarget()->IsPlayer())
	{
		CEZ2_Player *pPlayer = static_cast<CEZ2_Player*>(GetSpeechTarget());
		if (pPlayer)
		{
			// Ensures we don't respond to when Bad Cop has no "real" squad
			CAI_BaseNPC *pRepresentative = pPlayer->GetSquadCommandRepresentative();
			if (pPlayer && pRepresentative && pRepresentative->IsSilentCommandable())
				set.AppendCriteria("only_silent_commandable", "1");

			if (pPlayer->GetState() == NPC_STATE_COMBAT)
			{
				// Adopt the player's combat criteria
				pPlayer->ModifyOrAppendAICombatCriteria(set);
			}
		}
	}

	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if (pPlayer)
	{
		set.AppendCriteria( "wilson_distance", CFmtStrN<32>( "%f.3", (GetAbsOrigin() - pPlayer->GetAbsOrigin()).Length() ) );

		if (pPlayer->IsInAVehicle())
		{
			// Override playerspeed if in vehicle
			if (pPlayer->GetVehicleEntity() && pPlayer->GetVehicleEntity()->VPhysicsGetObject())
			{
				Vector velocity;
				pPlayer->GetVehicleEntity()->VPhysicsGetObject()->GetVelocity( &velocity, NULL );
				set.AppendCriteria( "playerspeed_vehicle", CFmtStrN<32>( "%.3f", velocity.Length() ) );
			}

			set.AppendCriteria( "player_in_vehicle", pPlayer->GetVehicleEntity()->GetClassname() );
		}
		else
		{
			set.AppendCriteria( "player_in_vehicle", "0" );
		}

		// Record whether the player is speaking
		if (pPlayer->GetExpresser() && pPlayer->GetExpresser()->IsSpeaking())
		{
			set.AppendCriteria( "player_speaking", "1" );
		}
		else
		{
			set.AppendCriteria( "player_speaking", "0" );
		}
	}
	else
	{
		set.AppendCriteria( "wilson_distance", "99999999999" );
		set.AppendCriteria( "player_in_vehicle", "0" );
	}

	if (m_hCameraTargets.Count() > 0 && GetSpeechTarget())
	{
		// Check which camera we're looking at our speech target through
		CWilsonCamera *pCamera = GetCameraForTarget( GetSpeechTarget() );
		if (pCamera)
		{
			set.AppendCriteria( "camera", STRING(pCamera->GetEntityName()) );
			pCamera->AppendContextToCriteria( set, "camera_" );
		}
		else
			set.AppendCriteria( "camera", "0" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Appends damage criteria
//-----------------------------------------------------------------------------
void CNPC_Wilson::ModifyOrAppendDamageCriteria(AI_CriteriaSet & set, const CTakeDamageInfo & info)
{
	set.AppendCriteria("damage", UTIL_VarArgs("%i", (int)info.GetDamage()));
	set.AppendCriteria("damage_type", UTIL_VarArgs("%i", info.GetDamageType()));
}

//-----------------------------------------------------------------------------
// Purpose: Check if the given concept can be spoken and then speak it
//-----------------------------------------------------------------------------
bool CNPC_Wilson::SpeakIfAllowed(AIConcept_t concept, AI_CriteriaSet& modifiers, bool bRespondingToPlayer, char *pszOutResponseChosen, size_t bufsize)
{
	// This might not be what bRespondingToPlayer was originally for, but it lets Will-E speak a lot more.
    if (!IsAllowedToSpeak(concept, HasCondition(COND_SEE_PLAYER)))
        return false;

    return Speak(concept, modifiers, pszOutResponseChosen, bufsize);
}

//-----------------------------------------------------------------------------
// Purpose: Alternate method signature for SpeakIfAllowed allowing no criteriaset parameter 
//-----------------------------------------------------------------------------
bool CNPC_Wilson::SpeakIfAllowed(AIConcept_t concept, bool bRespondingToPlayer, char *pszOutResponseChosen, size_t bufsize)
{
    AI_CriteriaSet set;
	return SpeakIfAllowed(concept, set, bRespondingToPlayer, pszOutResponseChosen, bufsize);
}

//------------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Wilson::IsOkToSpeak( ConceptCategory_t category, bool fRespondingToPlayer )
{
	// if not alive, certainly don't speak
	if ( !IsAlive() )
		return false;

	if ( m_spawnflags & SF_NPC_GAG )
		return false;

	// Don't speak if playing a script.
	if ( ( m_NPCState == NPC_STATE_SCRIPT ) && !CanSpeakWhileScripting() )
		return false;

	if ( IsInAScript() && !CanSpeakWhileScripting() )
		return false;

	if ( category == SPEECH_IDLE )
	{
		if ( GetState() != NPC_STATE_IDLE && GetState() != NPC_STATE_ALERT )
			return false;
		if ( GetSpeechFilter() && GetSpeechFilter()->GetIdleModifier() < 0.001 )
			return false;
	}

	if ( category != SPEECH_PRIORITY )
	{
		// if someone else is talking, don't speak
		if ( !GetExpresser()->SemaphoreIsAvailable( this ) )
			return false;

		if ( fRespondingToPlayer )
		{
			if ( !GetExpresser()->CanSpeakAfterMyself() )
				return false;
		}
		else
		{
			if ( !GetExpresser()->CanSpeak() )
				return false;
		}
	}

	if ( fRespondingToPlayer )
	{
		// If we're responding to the player, don't respond if the scene has speech in it
		if ( IsRunningScriptedSceneWithSpeechAndNotPaused( this, false ) )
		{
			return false;
		}
	}
	else
	{
		// If we're not responding to the player, don't talk if running a logic_choreo
		if ( IsRunningScriptedSceneAndNotPaused( this, false ) )
		{
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::PostSpeakDispatchResponse( AIConcept_t concept, AI_Response *response )
{
	CBaseEntity *pTarget = GetSpeechTarget();
	if (pTarget /*&& pTarget->IsPlayer()*/ && pTarget->IsAlive()) // IsValidSpeechTarget(0, pPlayer)
	{
		// Notify the target so they could respond
		variant_t variant;
		variant.SetString(AllocPooledString(concept));

		char szResponse[64] = { "<null>" };
		char szRule[64] = { "<null>" };
		if (response)
		{
			response->GetName( szResponse, sizeof( szResponse ) );
			response->GetRule( szRule, sizeof( szRule ) );
		}

		float flDuration = (GetExpresser()->GetTimeSpeechComplete() - gpGlobals->curtime);
		pTarget->AddContext("speechtarget_response", szResponse, (gpGlobals->curtime + flDuration + 1.0f));
		pTarget->AddContext("speechtarget_rule", szRule, (gpGlobals->curtime + flDuration + 1.0f));

		// Delay is now based off of predelay
		g_EventQueue.AddEvent(pTarget, "AnswerConcept", variant, (GetTimeSpeechComplete() - gpGlobals->curtime) /*+ RandomFloat(0.25f, 0.5f)*/, this, this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Wilson::GetGameTextSpeechParams( hudtextparms_t &params )
{
	params.channel = 3;
	params.x = -1;
	params.y = 0.6;
	params.effect = 0;

	params.r1 = 66;
	params.g1 = 255;
	params.b1 = 199;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::RefreshCameraTargets()
{
	m_hCameraTargets.RemoveAll();

	if (m_iszCameraTargets == NULL_STRING)
		return;

	CBaseEntity *pCamera = gEntList.FindEntityGeneric( NULL, STRING( m_iszCameraTargets ), this, this, this );
	while (pCamera)
	{
		if (!FClassnameIs( pCamera, "point_wilson_camera" ))
		{
			DevWarning( "%s: \"%s\" (%s) is not a point_wilson_camera\n", GetDebugName(), pCamera->GetDebugName(), pCamera->GetClassname() );
			continue;
		}

		m_hCameraTargets.AddToTail( static_cast<CWilsonCamera*>(pCamera) );

		pCamera = gEntList.FindEntityGeneric( pCamera, STRING( m_iszCameraTargets ), this, this, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const Vector &CNPC_Wilson::GetSpeechTargetSearchOrigin()
{
	if ( m_bOmniscient )
	{
		// It doesn't matter where we are, so search from the player's origin
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		if (pPlayer)
			return pPlayer->GetAbsOrigin();
	}

	return BaseClass::GetSpeechTargetSearchOrigin();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_Expresser *CNPC_Wilson::CreateExpresser(void)
{
    m_pExpresser = new CAI_Expresser(this);
    if (!m_pExpresser)
        return NULL;

    m_pExpresser->Connect(this);
    return m_pExpresser;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Wilson::PostConstructor(const char *szClassname)
{
    BaseClass::PostConstructor(szClassname);
    CreateExpresser();
}

//-----------------------------------------------------------------------------
// Purpose: Enable physics motion and collision response (on by default)
//-----------------------------------------------------------------------------
void CNPC_Wilson::InputEnableMotion( inputdata_t &inputdata )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	if ( pPhysicsObject != NULL )
	{
		pPhysicsObject->EnableMotion( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Disable any physics motion or collision response
//-----------------------------------------------------------------------------
void CNPC_Wilson::InputDisableMotion( inputdata_t &inputdata )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	if ( pPhysicsObject != NULL )
	{
		pPhysicsObject->EnableMotion( false );
	}
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------
AI_BEGIN_CUSTOM_NPC( npc_wilson, CNPC_Wilson )

	DECLARE_INTERACTION( g_interactionArbeitScannerStart );
	DECLARE_INTERACTION( g_interactionArbeitScannerEnd );

	DEFINE_SCHEDULE
	(
		SCHED_WILSON_ALERT_STAND,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_FACE_REASONABLE		0"
		"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
		"		TASK_WAIT					5" // Don't wait very long
		"		TASK_SUGGEST_STATE			STATE:IDLE"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_COMBAT"		// sound flags
		"		COND_HEAR_DANGER"
		"		COND_HEAR_BULLET_IMPACT"
		"		COND_IDLE_INTERRUPT"
	);

AI_END_CUSTOM_NPC()

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

BEGIN_DATADESC(CArbeitScanner)

	DEFINE_KEYFIELD( m_bDisabled,	FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_KEYFIELD( m_flCooldown,	FIELD_FLOAT, "Cooldown" ),

	DEFINE_KEYFIELD( m_flOuterRadius,	FIELD_FLOAT, "OuterRadius" ),
	DEFINE_KEYFIELD( m_flInnerRadius,	FIELD_FLOAT, "InnerRadius" ),

	DEFINE_KEYFIELD( m_iszScanSound, FIELD_SOUNDNAME, "ScanSound" ),
	DEFINE_KEYFIELD( m_iszScanDeploySound, FIELD_SOUNDNAME, "ScanDeploySound" ),
	DEFINE_KEYFIELD( m_iszScanDoneSound, FIELD_SOUNDNAME, "ScanDoneSound" ),
	DEFINE_KEYFIELD( m_iszScanRejectSound, FIELD_SOUNDNAME, "ScanRejectSound" ),

	DEFINE_KEYFIELD( m_flScanTime,	FIELD_FLOAT, "ScanTime" ),
	DEFINE_FIELD( m_flScanEndTime, FIELD_TIME ),

	DEFINE_FIELD( m_hScanning, FIELD_EHANDLE ),
	DEFINE_FIELD( m_pSprite, FIELD_CLASSPTR ),

	DEFINE_KEYFIELD( m_iszScanFilter, FIELD_STRING, "ScanFilter" ),
	DEFINE_FIELD( m_hScanFilter, FIELD_EHANDLE ),

	DEFINE_FIELD( m_iScanAttachment,	FIELD_INTEGER ),

	DEFINE_KEYFIELD( m_bWaitForScene, FIELD_BOOLEAN, "WaitForScene" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "FinishScan", InputFinishScan ),
	DEFINE_INPUTFUNC( FIELD_EHANDLE, "ForceScanNPC", InputForceScanNPC ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetOuterRadius", InputSetOuterRadius ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetInnerRadius", InputSetInnerRadius ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetAuthorizationFilter", InputSetDamageFilter ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetScanFilter", InputSetScanFilter ),

	DEFINE_THINKFUNC( IdleThink ),
	DEFINE_THINKFUNC( AwaitScanThink ),
	DEFINE_THINKFUNC( WaitForReturnThink ),
	DEFINE_THINKFUNC( ScanThink ),

	DEFINE_OUTPUT( m_OnScanDone, "OnScanDone" ),
	DEFINE_OUTPUT( m_OnScanReject, "OnScanReject" ),

	DEFINE_OUTPUT( m_OnScanStart, "OnScanStart" ),
	DEFINE_OUTPUT( m_OnScanInterrupt, "OnScanInterrupt" ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( prop_wilson_scanner, CArbeitScanner );

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CArbeitScanner::CArbeitScanner( void )
{
	m_flCooldown = -1;

	m_flInnerRadius = 64.0f;
	m_flOuterRadius = 128.0f;

	m_flScanTime = 2.0f;

	m_hScanning = NULL;
	m_pSprite = NULL;
	m_flScanEndTime = 0.0f;
	m_iScanAttachment = 0;
	m_bWaitForScene = false;
}

CArbeitScanner::~CArbeitScanner( void )
{
	// It's very important to make sure the scanner doesn't try to dispatch any responses during the destructor
	// We pass "false" to CleanupScan so that it will not dispatch an interaction to the scanned entity
	CleanupScan(false);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CArbeitScanner::Precache( void )
{
	if (GetModelName() == NULL_STRING)
		SetModelName( AllocPooledString("models/props_lab/monitor02.mdl") );

	PrecacheModel( STRING(GetModelName()) );

	PrecacheScriptSound( STRING(m_iszScanSound) );
	PrecacheScriptSound( STRING(m_iszScanDeploySound) );
	PrecacheScriptSound( STRING(m_iszScanDoneSound) );
	PrecacheScriptSound( STRING(m_iszScanRejectSound) );
	PrecacheScriptSound( "AI_BaseNPC.SentenceStop" );

	if (m_iszScanFilter != NULL_STRING)
	{
		CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, m_iszScanFilter, this );
		if (pEntity)
			m_hScanFilter = dynamic_cast<CBaseFilter*>(pEntity);
	}

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CArbeitScanner::Spawn( void )
{
	Precache();

	SetModel( STRING(GetModelName()) );

	BaseClass::Spawn();

	// Our view offset should correspond to our scan attachment
	m_iScanAttachment = LookupAttachment( "eyes" );
	Vector vecOrigin;
	GetAttachment( m_iScanAttachment, vecOrigin );
	SetViewOffset( vecOrigin - GetAbsOrigin() );

	if (!m_bDisabled)
	{
		SetThink( &CArbeitScanner::IdleThink );
		SetNextThink( gpGlobals->curtime );
	}
	else
	{
		SetThink( NULL );
		SetNextThink( TICK_NEVER_THINK );
		SetScanState( SCAN_OFF );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CArbeitScanner::IsScannable( CAI_BaseNPC *pNPC )
{
	if (m_hScanFilter && m_hScanFilter->PassesFilter( this, pNPC ))
		return true;

	if (STRING(m_target)[0] != '\0')
		return pNPC->ClassMatches( m_target );

	// If no filter or specific target, use CLASS_ARBEIT_TECH
	return pNPC->Classify() == CLASS_ARBEIT_TECH;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CArbeitScanner::InputEnable( inputdata_t &inputdata )
{
	// Only reset to IdleThink if we're disabled or "done"
	if (GetNextThink() == TICK_NEVER_THINK || GetScanState() == SCAN_DONE || GetScanState() == SCAN_REJECT)
	{
		SetThink( &CArbeitScanner::IdleThink );
		SetNextThink( gpGlobals->curtime );
		SetScanState( SCAN_IDLE );
	}
}

void CArbeitScanner::InputDisable( inputdata_t &inputdata )
{
	// Clean up any scans
	CleanupScan(true);
	SetScanState( SCAN_OFF );

	SetThink( NULL );
	SetNextThink( TICK_NEVER_THINK );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CArbeitScanner::InputFinishScan( inputdata_t &inputdata )
{
	if (m_hScanning)
	{
		m_flScanEndTime = gpGlobals->curtime;
		FinishScan();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CArbeitScanner::InputForceScanNPC( inputdata_t &inputdata )
{
	if (inputdata.value.Entity() && inputdata.value.Entity()->IsNPC())
	{
		// Begin scanning this NPC!
		m_hScanning = inputdata.value.Entity()->MyNPCPointer();
		SetThink( &CArbeitScanner::ScanThink );
		SetNextThink( gpGlobals->curtime );
	}
	else
	{
		Warning("%s ForceScanNPC: Entity not valid or not a NPC\n", GetDebugName());
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CArbeitScanner::InputSetInnerRadius( inputdata_t &inputdata )
{
	m_flInnerRadius = inputdata.value.Float();
}

void CArbeitScanner::InputSetOuterRadius( inputdata_t &inputdata )
{
	m_flOuterRadius = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CArbeitScanner::InputSetScanFilter( inputdata_t &inputdata )
{
	if (inputdata.value.StringID() != NULL_STRING)
	{
		m_iszScanFilter = inputdata.value.StringID();
		CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, m_iszScanFilter, this );
		m_hScanFilter = dynamic_cast<CBaseFilter*>(pEntity);
	}
	else
	{
		m_hScanFilter = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_BaseNPC *CArbeitScanner::FindMatchingNPC( float flRadiusSqr )
{
	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
	int nAIs = g_AI_Manager.NumAIs();

	Vector vecForward;
	Vector vecOrigin;
	GetAttachment( m_iScanAttachment, vecOrigin, &vecForward );

	for ( int i = 0; i < nAIs; i++ )
	{
		CAI_BaseNPC *pNPC = ppAIs[ i ];

		if( IsScannable(pNPC) && pNPC->IsAlive() )
		{
			Vector vecToNPC = (vecOrigin - pNPC->GetAbsOrigin());
			float flDist = vecToNPC.LengthSqr();

			if( flDist < flRadiusSqr )
			{
				// Now do a visibility test.
				if ( IsInScannablePosition(pNPC, vecToNPC, vecForward) )
				{
					// They passed!
					return pNPC;
				}
			}
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CArbeitScanner::IdleThink()
{
	if( !UTIL_FindClientInPVS(edict()) || (CAI_BaseNPC::m_nDebugBits & bits_debugDisableAI) )
	{
		// If we're not in the PVS or AI is disabled, sleep!
		SetNextThink( gpGlobals->curtime + 1.0 );
		return;
	}

	SetNextThink( gpGlobals->curtime + 0.25 );
	StudioFrameAdvance();

	// First, look through the NPCs that match our criteria and find out if any of them are in our outer radius.
	CAI_BaseNPC *pNPC = FindMatchingNPC( m_flOuterRadius * m_flOuterRadius );
	if (pNPC)
	{
		// Got one! Wait for them to enter the inner radius.
		SetScanState( SCAN_WAITING );
		EmitSound( STRING(m_iszScanDeploySound) );
		SetThink( &CArbeitScanner::AwaitScanThink );
		SetNextThink( gpGlobals->curtime + 1.0f );
	}
	else if (GetScanState() != SCAN_IDLE)
	{
		SetScanState( SCAN_IDLE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CArbeitScanner::AwaitScanThink()
{
	// I know it's kind of weird to look through the NPCs again instead of just storing them or even using FindMatchingNPC, 
	// but I prefer this over having to keep track of every matching entity that gets nearby or doing the same function again.
	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
	int nAIs = g_AI_Manager.NumAIs();

	Vector vecForward;
	Vector vecOrigin;
	GetAttachment( m_iScanAttachment, vecOrigin, &vecForward );

	// Makes sure there's still someone in the outer radius
	bool bNPCAvailable = false;

	for ( int i = 0; i < nAIs; i++ )
	{
		CAI_BaseNPC *pNPC = ppAIs[ i ];

		if( IsScannable(pNPC) && pNPC->IsAlive() )
		{
			Vector vecToNPC = (vecOrigin - pNPC->GetAbsOrigin());
			float flDist = vecToNPC.LengthSqr();

			// This time, make sure they're in the inner radius
			if( flDist < (m_flInnerRadius * m_flInnerRadius) )
			{
				// Now do a visibility test.
				if ( IsInScannablePosition(pNPC, vecToNPC, vecForward) )
				{
					// Begin scanning this NPC!
					m_hScanning = pNPC;
					SetThink( &CArbeitScanner::ScanThink );
					SetNextThink( gpGlobals->curtime );
					return;
				}
			}

			// Okay, nobody in the inner radius yet. Make sure there's someone in the outer radius though.
			else if ( flDist < (m_flOuterRadius * m_flOuterRadius) )
			{
				if ( IsInScannablePosition(pNPC, vecToNPC, vecForward) )
				{
					// We still have someone in the outer radius.
					bNPCAvailable = true;
				}
			}
		}
	}

	if (!bNPCAvailable)
	{
		// Nobody's within scanning distance anymore! Wait a few seconds before going back.
		SetThink( &CArbeitScanner::WaitForReturnThink );
		SetNextThink( gpGlobals->curtime + 1.5f );
	}

	SetNextThink( gpGlobals->curtime + 0.25f );
	StudioFrameAdvance();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CArbeitScanner::WaitForReturnThink()
{
	// Is anyone there?
	CAI_BaseNPC *pNPC = FindMatchingNPC( m_flOuterRadius * m_flOuterRadius );
	if (pNPC)
	{
		// Ah, they've returned!
		SetScanState( SCAN_WAITING );
		SetThink( &CArbeitScanner::AwaitScanThink );
		SetNextThink( gpGlobals->curtime );
	}
	else
	{
		// Go back to idling I guess.
		SetScanState( SCAN_IDLE );
		SetThink( &CArbeitScanner::IdleThink );
		SetNextThink( gpGlobals->curtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CArbeitScanner::ScanThink()
{
	Vector vecOrigin;
	GetAttachment( m_iScanAttachment, vecOrigin );

	if (!HasSpawnFlags(SF_ARBEIT_SCANNER_STAY_SCANNING))
	{
		if (!m_hScanning || !m_hScanning->IsAlive() || ((vecOrigin - m_hScanning->GetAbsOrigin()).LengthSqr() > (m_flInnerRadius * m_flInnerRadius)))
		{
			// Must've been killed or moved too far away while we were scanning.
			// Use WaitForReturn to test whether anyone else is in our radius.
			m_OnScanInterrupt.FireOutput( m_hScanning, this );

			EmitSound( "AI_BaseNPC.SentenceStop" );

			CleanupScan(true);
			SetThink( &CArbeitScanner::WaitForReturnThink );
			SetNextThink( gpGlobals->curtime );
			return;
		}
	}

	if ( m_bWaitForScene && m_flScanEndTime <= gpGlobals->curtime )
	{
		if ( IsRunningScriptedSceneWithSpeechAndNotPaused( m_hScanning ) )
		{
			m_flScanEndTime  += 1.0f;
		}
	}

	// Scanning...
	if (!m_pSprite)
	{
		// Okay, start our scanning effect.
		SetScanState( SCAN_SCANNING );
		EmitSound( STRING(m_iszScanSound) );
		m_OnScanStart.FireOutput( m_hScanning, this );

		m_pSprite = CSprite::SpriteCreate("sprites/glow1.vmt", GetLocalOrigin(), false);
		if (m_pSprite)
		{
			m_pSprite->SetTransparency( kRenderWorldGlow, 200, 240, 255, 255, kRenderFxNoDissipation );
			m_pSprite->SetScale(0.2f, 1.5f);
			m_pSprite->SetAttachment( this, m_iScanAttachment );
		}

		m_hScanning->DispatchInteraction(g_interactionArbeitScannerStart, this, NULL);

		// Check if we should scan indefinitely
		if (m_flScanTime == -1.0f)
			m_flScanEndTime = FLT_MAX;
		else
			m_flScanEndTime = gpGlobals->curtime + m_flScanTime;
	}
	else
	{
		// Check progress...
		if (m_flScanEndTime <= gpGlobals->curtime)
		{
			// Scan is finished!
			FinishScan();

			return;
		}
	}

	SetNextThink(gpGlobals->curtime + 0.1f);
	StudioFrameAdvance();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CArbeitScanner::FinishScan()
{
	bool bPassScan = CanPassScan(m_hScanning);
	if (bPassScan)
	{
		SetScanState( SCAN_DONE );
		EmitSound( STRING(m_iszScanDoneSound) );
		m_OnScanDone.FireOutput(m_hScanning, this);
	}
	else
	{
		SetScanState( SCAN_REJECT );
		EmitSound( STRING(m_iszScanRejectSound) );
		m_OnScanReject.FireOutput(m_hScanning, this);
	}

	CleanupScan(true);

	if (m_flCooldown != -1)
	{
		SetThink( &CArbeitScanner::IdleThink );
		SetNextThink( gpGlobals->curtime + m_flCooldown );
	}
	else
	{
		SetThink( NULL );
		SetNextThink( TICK_NEVER_THINK );
	}

	return bPassScan;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CArbeitScanner::CleanupScan(bool dispatchInteraction)
{
	if (m_hScanning)
	{
		if( dispatchInteraction )
			m_hScanning->DispatchInteraction(g_interactionArbeitScannerEnd, this, NULL);
		
		m_hScanning = NULL;
	}

	if (m_pSprite)
	{
		UTIL_Remove(m_pSprite);
		m_pSprite = NULL;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

BEGIN_DATADESC( CWilsonCamera )

	DEFINE_KEYFIELD( m_bDisabled,		FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_KEYFIELD( m_flFieldOfView,	FIELD_FLOAT, "FieldOfView" ),
	DEFINE_KEYFIELD( m_flLookDistSqr,		FIELD_FLOAT, "LookDist" ),
	DEFINE_KEYFIELD( m_b3DViewCone,		FIELD_BOOLEAN, "Use3DViewCone" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetFOV", InputSetFOV ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetLookDist", InputSetLookDist ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( point_wilson_camera, CWilsonCamera );

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CWilsonCamera::CWilsonCamera( void )
{
	m_bDisabled = false;
	m_flFieldOfView = 90.0f;
	m_flLookDistSqr = 1024.0f;
	m_b3DViewCone = false;
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CWilsonCamera::Spawn( void )
{
	BaseClass::Spawn();

	// Make FOV and look dist ready for visibility calculations
	m_flFieldOfView = cos( DEG2RAD(m_flFieldOfView/2) );
	m_flLookDistSqr *= m_flLookDistSqr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWilsonCamera::FInViewCone( const Vector &vecSpot )
{
	Vector los = ( vecSpot - EyePosition() );

	if (los.LengthSqr() > m_flLookDistSqr)
		return false;

	Vector facingDir;
	GetVectors( &facingDir, NULL, NULL );

	if (m_b3DViewCone)
	{
		// do this in 2D
		los.z = 0;
		VectorNormalize( los );

		facingDir.z = 0;
		facingDir.AsVector2D().NormalizeInPlace();
	}

	float flDot = DotProduct( los, facingDir );

	if ( flDot > m_flFieldOfView )
		return true;

	return false;
}
