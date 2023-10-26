#include "cbase.h"
#include "asw_alien.h"
#include "asw_ai_senses.h"
#include "asw_game_resource.h"
#include "asw_gamerules.h"
#include "asw_mission_manager.h"
#include "asw_marine_resource.h"
#include "asw_marine.h"
#include "asw_trace_filter_melee.h"
#include "asw_spawner.h"
#include "ai_waypoint.h"
#include "ai_moveprobe.h"
#include "asw_fx_shared.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "EntityFlame.h"
#include "asw_util_shared.h"
#include "asw_burning.h"
#include "asw_door.h"
#include "asw_gamestats.h"
#include "asw_pickup_money.h"
#include "asw_director.h"
#include "asw_physics_prop_statue.h"
#include "te_effect_dispatch.h"
#include "asw_ai_behavior.h"
#include "asw_ai_behavior_combat_stun.h"
#include "asw_ai_behavior_sleep.h"
#include "asw_ai_behavior_flinch.h"
#include "asw_missile_round_shared.h"
#include "datacache/imdlcache.h"
#include "asw_tesla_trap.h"
#include "sendprop_priorities.h"
#include "asw_spawn_manager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_debug_alien_damage;
extern ConVar ai_show_hull_attacks;
extern ConVar ai_efficiency_override;
extern ConVar ai_frametime_limit;
extern ConVar ai_use_think_optimizations;
extern ConVar ai_use_efficiency;
extern ConVar showhitlocation;
extern ConVar asw_stun_grenade_time;
extern ConVar asw_drone_zig_zagging;
extern ConVar asw_draw_awake_ai;
extern ConVar asw_alien_debug_death_style;
// asw - how much extra damage to do to burning aliens
ConVar asw_fire_alien_damage_scale("asw_fire_alien_damage_scale", "3.0", FCVAR_CHEAT );
ConVar asw_alien_speed_scale_easy("asw_alien_speed_scale_easy", "0");
ConVar asw_alien_speed_scale_normal("asw_alien_speed_scale_normal", "0");
ConVar asw_alien_speed_scale_hard("asw_alien_speed_scale_hard", "0");
ConVar asw_alien_speed_scale_insane("asw_alien_speed_scale_insane", "0");
ConVar asw_alien_hurt_speed( "asw_alien_hurt_speed", "0.5", FCVAR_CHEAT, "Fraction of speed to use when the alien is hurt after being shot" );
ConVar asw_alien_stunned_speed( "asw_alien_stunned_speed", "0.3", FCVAR_CHEAT, "Fraction of speed to use when the alien is electrostunned" );
ConVar asw_drop_money("asw_drop_money", "1", FCVAR_CHEAT, "Do aliens drop money?");
ConVar asw_alien_money_chance("asw_alien_money_chance", "1.0", FCVAR_CHEAT, "Chance of base aliens dropping money");
ConVar asw_drone_hurl_chance( "asw_drone_hurl_chance", "0.66666666666666", FCVAR_NONE, "Chance that an alien killed by explosives will hurl towards the camera." );
ConVar asw_drone_hurl_interval( "asw_drone_hurl_interval", "10", FCVAR_NONE, "Minimum number of seconds that must pass between alien bodies flung at camera." );
ConVar asw_alien_break_chance ( "asw_alien_break_chance", "0.5", FCVAR_NONE, "chance the alien will break into ragdoll pieces instead of gib");
ConVar asw_alien_fancy_death_chance ( "asw_alien_fancy_death_chance", "0.5", FCVAR_NONE, "If a drone doesn't instagib, this is the chance the alien plays a death anim before ragdolling" );
ConVar asw_debug_npcs( "asw_debug_npcs", "0", FCVAR_CHEAT, "Enables debug overlays for various NPCs" );
ConVar asw_alien_burn_duration( "asw_alien_burn_duration", "5.0f", FCVAR_CHEAT, "Alien burn time" );
extern ConVar asw_money;

// soft alien collision
ConVar asw_springcol( "asw_springcol", "1", FCVAR_CHEAT, "Use soft alien collision" );
ConVar asw_springcol_push_cap( "asw_springcol_push_cap", "33.0", FCVAR_CHEAT, "Cap on the total push vector" );
ConVar asw_springcol_core( "asw_springcol_core", "0.33", FCVAR_CHEAT, "Fraction of the alien's pushaway radius that is a solid capped core" );
ConVar asw_springcol_radius( "asw_springcol_radius", "50.0", FCVAR_CHEAT, "Radius of the alien's pushaway cylinder" );
ConVar asw_springcol_force_scale( "asw_springcol_force_scale", "3.0", FCVAR_CHEAT, "Multiplier for each individual push force" );
ConVar asw_springcol_debug( "asw_springcol_debug", "0", FCVAR_CHEAT, "Display the direction of the pushaway vector. Set to entity index or -1 to show all." );

float CASW_Alien::sm_flLastHurlTime = 0;

int ACT_MELEE_ATTACK1_HIT;
int ACT_MELEE_ATTACK2_HIT;
int AE_ALIEN_MELEE_HIT;

int ACT_ALIEN_FLINCH_SMALL;
int ACT_ALIEN_FLINCH_MEDIUM;
int ACT_ALIEN_FLINCH_BIG;
int ACT_ALIEN_FLINCH_GESTURE;
int ACT_DEATH_FIRE;
int ACT_DEATH_ELEC;
int ACT_DIE_FANCY;

int ACT_BURROW_IDLE;
int ACT_BURROW_OUT;

LINK_ENTITY_TO_CLASS( asw_alien, CASW_Alien );

IMPLEMENT_SERVERCLASS_ST(CASW_Alien, DT_ASW_Alien)
	SendPropExclude ( "DT_BaseEntity", "m_vecOrigin" ),
	SendPropVectorXY( SENDINFO( m_vecOrigin ), 				 CELL_BASEENTITY_ORIGIN_CELL_BITS, SPROP_CELL_COORD_LOWPRECISION | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, CBaseEntity::SendProxy_CellOriginXY, SENDPROP_NONLOCALPLAYER_ORIGINXY_PRIORITY ),
	SendPropFloat   ( SENDINFO_VECTORELEM( m_vecOrigin, 2 ), CELL_BASEENTITY_ORIGIN_CELL_BITS, SPROP_CELL_COORD_LOWPRECISION | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, CBaseEntity::SendProxy_CellOriginZ, SENDPROP_NONLOCALPLAYER_ORIGINZ_PRIORITY ),

	SendPropExclude( "DT_BaseEntity", "m_angRotation" ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angRotation, 0), 10, SPROP_CHANGES_OFTEN, CBaseEntity::SendProxy_AnglesX ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angRotation, 1), 10, SPROP_CHANGES_OFTEN, CBaseEntity::SendProxy_AnglesY ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angRotation, 2), 10, SPROP_CHANGES_OFTEN, CBaseEntity::SendProxy_AnglesZ ),

	// test
	//SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),
	SendPropBool( SENDINFO(m_bElectroStunned) ),// not using ElectroStunned
	//SendPropBool( SENDINFO(m_bElectroShockSmall) ),
	//SendPropBool( SENDINFO(m_bElectroShockBig) ),
	SendPropBool( SENDINFO(m_bOnFire) ),
	SendPropInt( SENDINFO(m_nDeathStyle), CASW_Alien::kDEATHSTYLE_NUM_TRANSMIT_BITS , SPROP_UNSIGNED ),
	SendPropInt		(SENDINFO(m_iHealth), ASW_ALIEN_HEALTH_BITS ),
	//SendPropBool(SENDINFO(m_bGibber)),
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Alien )
	DEFINE_KEYFIELD( m_bVisibleWhenAsleep, FIELD_BOOLEAN, "visiblewhenasleep" ),
	DEFINE_KEYFIELD( m_iMoveCloneName, FIELD_STRING, "MoveClone" ),
	DEFINE_KEYFIELD( m_bStartBurrowed,		FIELD_BOOLEAN,	"startburrowed" ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"BreakWaitForScript", InputBreakWaitForScript ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetMoveClone", InputSetMoveClone ),
	DEFINE_FIELD( m_flBurrowTime, FIELD_TIME ),
	DEFINE_FIELD(m_bIgnoreMarines, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bFailedMoveTo, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bElectroStunned, FIELD_BOOLEAN),
	
	//DEFINE_FIELD(m_bPerformingZigZag, FIELD_BOOLEAN),	// don't store this, let the zig zag be cleared each time	
	//DEFINE_FIELD(m_bRunAtChasingPathEnds, FIELD_BOOLEAN), // no need to store currently, it's always true form constructor
	DEFINE_FIELD(m_fNextPainSound, FIELD_FLOAT),
	DEFINE_FIELD(m_fNextStunSound, FIELD_FLOAT),
	DEFINE_FIELD(m_fHurtSlowMoveTime, FIELD_TIME),
	//								m_ActBusyBehavior (auto saved by AI)	
	DEFINE_FIELD( m_hSpawner, FIELD_EHANDLE ),
	DEFINE_FIELD( m_AlienOrders, FIELD_INTEGER ),
	DEFINE_FIELD( m_vecAlienOrderSpot, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_AlienOrderObject, FIELD_EHANDLE ),
	DEFINE_FIELD( m_fLastSleepCheckTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_bOnFire, FIELD_BOOLEAN ),	
	DEFINE_FIELD( m_iNumASWOrderRetries, FIELD_INTEGER ),
	DEFINE_FIELD( m_flFreezeResistance, FIELD_FLOAT ),
	DEFINE_FIELD( m_flFrozenTime, FIELD_TIME ),

	// soft alien collision
	DEFINE_FIELD( m_vecLastPushAwayOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecLastPush, FIELD_VECTOR ),
	DEFINE_FIELD( m_bPushed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bHoldoutAlien, FIELD_BOOLEAN ),
END_DATADESC()

IMPLEMENT_AUTO_LIST( IAlienAutoList );

#define ASW_ALIEN_SLEEP_CHECK_INTERVAL 1.0f

CASW_Alien::CASW_Alien( void ) :
	m_BehaviorParms( DefLessFunc( const CUtlSymbol ) )
{
	m_bRegisteredAsAwake = false;
	m_pszAlienModelName = NULL;
	m_bRunAtChasingPathEnds = true;	
	m_bPerformingZigZag = false;
	m_fNextPainSound = 0;
	m_fNextStunSound = 0;

	m_fHurtSlowMoveTime = 0;
	m_hSpawner = NULL;
	m_AlienOrders = AOT_None;
	m_vecAlienOrderSpot = vec3_origin;
	m_AlienOrderObject = NULL;
	m_bIgnoreMarines = false;
	m_fLastSleepCheckTime = 0;
	m_bVisibleWhenAsleep = false;
	m_fLastMarineCanSeeTime = -100;
	m_bLastMarineCanSee = false;
	//m_bGibber = false;
	m_bTimeToRagdoll = false;
	m_iDeadBodyGroup = 1;
	m_bNeverRagdoll = false;
	m_bNeverInstagib = false;
	m_nDeathStyle = kDIE_RAGDOLLFADE;
	m_flBaseThawRate = 0.5f;
	m_flFrozenTime = 0.0f;

	m_UnburrowActivity = (Activity) ACT_BURROW_OUT;
	m_UnburrowIdleActivity = (Activity) ACT_BURROW_IDLE;
	m_iszUnburrowActivityName = NULL_STRING;
	m_iszUnburrowIdleActivityName = NULL_STRING;

	m_pPreviousBehavior = NULL;
	m_vecLastPush.Init();
	m_bBehaviorParameterChanged = false;

	m_nVolleyType = -1;
	m_flRangeAttackStartTime = 0.0f;
	m_flRangeAttackLastUpdateTime = 0.0f;
	m_vecRangeAttackTargetPosition.Init();
	m_bHoldoutAlien = false;

	m_nAlienCollisionGroup = COLLISION_GROUP_NPC;

	meleeAttack1.Init( 0.0f, 64.0f, 0.7f, false );
	meleeAttack2.Init( 0.0f, 64.0f, 0.7f, false );
	rangeAttack1.Init( 64.0f, 786.0f, 0.5f, false );
	rangeAttack2.Init( 64.0f, 512.0f, 0.5f, false );
}

CASW_Alien::~CASW_Alien()
{
	for( int i = 0; i < m_Behaviors.Count(); i++ )
	{
		if ( m_Behaviors[ i ]->IsAllocated() )
		{
			delete m_Behaviors[ i ];
			m_Behaviors.Remove( i );
			i--;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Alien::Spawn()
{
	if ( asw_debug_npcs.GetBool() )
	{
		m_debugOverlays |= OVERLAY_NPC_ROUTE_BIT | OVERLAY_BBOX_BIT | OVERLAY_PIVOT_BIT | OVERLAY_TASK_TEXT_BIT | OVERLAY_TEXT_BIT;
	}

	ChangeFaction( FACTION_ALIENS );

	// get pointers to behaviors
	GetBehavior( &m_pFlinchBehavior );
	// Precache.
	Precache();
	SetModel( m_pszAlienModelName );

	SetHullSizeNormal();

	// Base spawn.
	BaseClass::Spawn();

	// By default NPCs are always solid and cannot be stood upon.
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetBloodColor( BLOOD_COLOR_GREEN );

	SetCollisionGroup( m_nAlienCollisionGroup );

	SetNavType( NAV_GROUND );
	SetMoveType( MOVETYPE_STEP );

	// Set the initial NPC state.
	m_NPCState	= NPC_STATE_NONE;
	m_vecLastPushAwayOrigin = GetAbsOrigin();
	m_fFancyDeathChance = RandomFloat();

	if ( m_SquadName != NULL_STRING )
	{
		CapabilitiesAdd( bits_CAP_SQUAD );																			 
	}

	if ( HasSpawnFlags( SF_ANTLION_USE_GROUNDCHECKS ) == false )
	{
		CapabilitiesAdd( bits_CAP_SKIP_NAV_GROUND_CHECK );
	}
	NPCInit();
	m_flDistTooFar = 9999999.0f;
	LookupBurrowActivities();

	//See if we're supposed to start burrowed
	if ( m_bStartBurrowed )
	{		
		AddEffects( EF_NODRAW );
		AddEffects( EF_NOSHADOW );
		AddFlag( FL_NOTARGET );
		m_spawnflags |= SF_NPC_GAG;

		AddSolidFlags( FSOLID_NOT_SOLID );
		m_takedamage	= DAMAGE_NO;
		m_vecUnburrowEndPoint = GetAbsOrigin();

		// set alien to fly and not collide for the duration of his unburrow
		AddFlag( FL_FLY );
		SetGroundEntity( NULL );
		SetCollisionGroup( COLLISION_GROUP_NPC_SCRIPTED );

		// offset alien's position by the motion in his unburrow sequence
		Activity unburrowActivity = m_UnburrowActivity;
		int nSeq = SelectWeightedSequence( unburrowActivity );
		if ( nSeq != -1 )
		{
			Vector vecLocalDelta;
			GetSequenceLinearMotion( nSeq, &vecLocalDelta );

			// Transform the sequence delta
			matrix3x4_t fRotateMatrix;
			AngleMatrix( GetLocalAngles(), fRotateMatrix );
			Vector vecDelta;
			VectorRotate( vecLocalDelta, fRotateMatrix, vecDelta );

			Vector vecNewPos = GetAbsOrigin() - vecDelta;
			Teleport( &vecNewPos, &GetAbsAngles(), &vec3_origin );
		}

		SetState( NPC_STATE_IDLE );
		SetActivity( m_UnburrowIdleActivity );
		SetSchedule( SCHED_BURROW_WAIT ); // todo: a schedule where they don't crawl out right away?
	}

	if ( m_iMoveCloneName != NULL_STRING )
	{
		SetMoveClone( m_iMoveCloneName, NULL );
	}
}

void CASW_Alien::Precache()
{

	BaseClass::Precache();


	PrecacheModel( m_pszAlienModelName );
	
	//pre-cache any models used by particle gib effects
	int modelIndex = modelinfo->GetModelIndex( m_pszAlienModelName );
	const model_t *model = modelinfo->GetModel( modelIndex );
	if ( model )
	{
		KeyValues *modelKeyValues = new KeyValues( "" );
		if ( modelKeyValues->LoadFromBuffer( m_ModelName.ToCStr(), modelinfo->GetModelKeyValueText( model ) ) )
		{

			KeyValues *pkvMeshParticleEffect = modelKeyValues->FindKey( "MeshParticles" );
			if ( pkvMeshParticleEffect )
			{

				for ( KeyValues *pSingleEffect = pkvMeshParticleEffect->GetFirstSubKey(); pSingleEffect; pSingleEffect = pSingleEffect->GetNextKey() )
				{
					const char *pszModelName = pSingleEffect->GetString( "modelName", "" );
					if( pszModelName )
					{
						PrecacheModel( pszModelName );
					}
				}
			}
		}
		modelKeyValues->deleteThis();
	}


	PrecacheParticleSystem( "drone_death" );	// death
	PrecacheParticleSystem( "drone_shot" );		// shot
	PrecacheParticleSystem( "freeze_statue_shatter" );
}

// Updates our memory about the enemies we Swarm Sensed
// todo: add various swarm sense conditions?
void CASW_Alien::OnSwarmSensed(int iDistance)
{
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
CAI_Senses* CASW_Alien::CreateSenses()
{
	CAI_Senses *pSenses = new CASW_AI_Senses;
	pSenses->SetOuter( this );
	return pSenses;
}

CASW_AI_Senses* CASW_Alien::GetASWSenses()
{
	return dynamic_cast<CASW_AI_Senses*>(GetSenses());
}

bool CASW_Alien::QuerySeeEntity( CBaseEntity *pEntity, bool bOnlyHateOrFearIfNPC )
{
	if ( !BaseClass::QuerySeeEntity( pEntity, bOnlyHateOrFearIfNPC ) ) 
		return false;

	if ( pEntity && pEntity->Classify() == CLASS_ASW_BAIT )
	{
		return GetAbsOrigin().DistToSqr( pEntity->GetAbsOrigin() ) < 90000.0f;	// only see bait within 300 units
	}
	return true;
}

// wake the alien up when a marine gets nearby
void CASW_Alien::UpdateSleepState(bool bInPVS)
{
	if ( GetSleepState() > AISS_AWAKE )		// alien is asleep, check for marines getting near to wake us up
	{
		// wake up if we have a script to run
		if (m_hCine != NULL && GetSleepState() > AISS_AWAKE )
			Wake();

		bInPVS = MarineCanSee(384, 0.1f);
		if ( bInPVS )
			SetCondition( COND_IN_PVS );
		else
			ClearCondition( COND_IN_PVS );

		if ( GetSleepState() > AISS_AWAKE && GetSleepState() != AISS_WAITING_FOR_INPUT )
		{
			if (bInPVS)
			{
				if (asw_draw_awake_ai.GetBool())
					NDebugOverlay::EntityText(entindex(), 1, "WAKING", 60, 255,255,0,255);
				Wake();
			}
		}
	}
	else	// alien is awake, check for going back to ZZZ again :)
	{		
		bool bHasOrders = (m_AlienOrders != AOT_None);
					
		// Don't let an NPC sleep if they're running a script!
		if( !ShouldAlwaysThink() && !bHasOrders && !IsInAScript() && m_NPCState != NPC_STATE_SCRIPT )
		{			
			if( m_fLastSleepCheckTime < gpGlobals->curtime + ASW_ALIEN_SLEEP_CHECK_INTERVAL )
			{
				//if (!GetEnemy() && !MarineNearby(1024.0f) )
				if (!GetEnemy() && !MarineCanSee(384, 2.0f) )
				{
					SetSleepState( AISS_WAITING_FOR_PVS );

					if (asw_draw_awake_ai.GetBool())
						NDebugOverlay::EntityText(entindex(), 1, "SLEEPING", 600, 255,255,0,255);

					Sleep();
					if (m_bVisibleWhenAsleep)
						RemoveEffects( EF_NODRAW );
				}
				m_fLastSleepCheckTime = gpGlobals->curtime;
			}
		}
	}
	if ( GetSleepState() == AISS_AWAKE )
	{
		if ( !m_bRegisteredAsAwake )
		{
			ASWSpawnManager()->OnAlienWokeUp( this );
			m_bRegisteredAsAwake = true;
		}
	}
	else
	{
		if ( m_bRegisteredAsAwake )
		{
			ASWSpawnManager()->OnAlienSleeping( this );
			m_bRegisteredAsAwake = false;
		}
	}
}

void CASW_Alien::UpdateOnRemove()
{
	if ( m_bRegisteredAsAwake )
	{
		m_bRegisteredAsAwake = false;
		ASWSpawnManager()->OnAlienSleeping( this );
	}
	BaseClass::UpdateOnRemove();
}

void CASW_Alien::SetDistSwarmSense( float flDistSense )
{
	if (!GetASWSenses())
		return;
	GetASWSenses()->SetDistSwarmSense( flDistSense );
}

void CASW_Alien::NPCInit()
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
	SetCollisionBounds( GetHullMins(), GetHullMaxs() );

	CASW_GameStats.Event_AlienSpawned( this );
	
	m_LagCompensation.Init(this);
}

void CASW_Alien::CallBehaviorThink()
{
	CAI_ASW_Behavior *pCurrent = GetPrimaryASWBehavior();
	if ( pCurrent )
	{
		pCurrent->BehaviorThink();
	}
}

void CASW_Alien::NPCThink( void )
{
	BaseClass::NPCThink();

	if ( asw_springcol.GetBool() && CanBePushedAway() )
	{
		PerformPushaway();
	}
	// stop electro stunning if we're slowed
	if ( m_bElectroStunned && m_lifeState != LIFE_DYING )
	{
		if ( m_fHurtSlowMoveTime < gpGlobals->curtime )
		{
			m_bElectroStunned = false;
		}
		else
		{
			if ( gpGlobals->curtime >= m_fNextStunSound )
			{
				m_fNextStunSound = gpGlobals->curtime + RandomFloat( 0.2f, 0.5f );

				EmitSound( "ASW_Tesla_Laser.Damage" );
			}
		}
	}

	if (gpGlobals->maxClients > 1)
		m_LagCompensation.StorePositionHistory();
	// Update range attack
	if ( m_nVolleyType >= 0 )
		UpdateRangedAttack();

	UpdateThawRate();

	m_flLastThinkTime = gpGlobals->curtime;
}

bool CASW_Alien::MarineNearby(float radius, bool bCheck3D)
{
	// find the closest marine
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return false;
	
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource* pMarineResource = pGameResource->GetMarineResource(i);
		if (!pMarineResource)
			continue;

		CASW_Marine* pMarine = pMarineResource->GetMarineEntity();
		if (!pMarine)
			continue;
		
		Vector diff = pMarine->GetAbsOrigin() - GetAbsOrigin();
		float dist = bCheck3D ? diff.Length() : diff.Length2D();

		if (dist < radius)
		{
			return true;
		}
	}
	return false;
}

CBaseEntity *CASW_Alien::CheckTraceHullAttack( float flDist, const Vector &mins, const Vector &maxs, float flDamage, int iDmgType, float forceScale, bool bDamageAnyNPC )
{
	// If only a length is given assume we want to trace in our facing direction
	Vector forward;
	AngleVectors( GetAbsAngles(), &forward );
	Vector vStart = GetAbsOrigin();

	// The ideal place to start the trace is in the center of the attacker's bounding box.
	// however, we need to make sure there's enough clearance. Some of the smaller monsters aren't 
	// as big as the hull we try to trace with. (SJB)
	float flVerticalOffset = WorldAlignSize().z * 0.5;

	if( flVerticalOffset < maxs.z )
	{
		// There isn't enough room to trace this hull, it's going to drag the ground.
		// so make the vertical offset just enough to clear the ground.
		flVerticalOffset = maxs.z + 1.0;
	}

	vStart.z += flVerticalOffset;
	Vector vEnd = vStart + (forward * flDist );
	return CheckTraceHullAttack( vStart, vEnd, mins, maxs, flDamage, iDmgType, forceScale, bDamageAnyNPC );
}

// alien has been hit by a melee attack
void CASW_Alien::MeleeBleed(CTakeDamageInfo* info)
{
	if ( info->GetDamage() >= 1.0 && !(info->GetDamageType() & DMG_SHOCK )
		&& !(info->GetDamageType() & DMG_BURN ))
	{
		Vector vecDir = -info->GetDamageForce();
		vecDir.NormalizeInPlace();
		UTIL_ASW_DroneBleed( info->GetDamagePosition() + m_LagCompensation.GetLagCompensationOffset(), vecDir, 4 );
	}
}


//-----------------------------------------------------------------------------
// Freezes this NPC in place for a period of time.
//-----------------------------------------------------------------------------
void CASW_Alien::Freeze( float flFreezeAmount, CBaseEntity *pFreezer, Ray_t *pFreezeRay ) 
{
	if ( flFreezeAmount <= 0.0f )
	{
		SetCondition(COND_NPC_FREEZE);
		SetMoveType(MOVETYPE_NONE);
		SetGravity(0);
		SetLocalAngularVelocity(vec3_angle);
		SetAbsVelocity( vec3_origin );
		return;
	}

	if ( flFreezeAmount > 1.0f )
	{
		float flFreezeDuration = flFreezeAmount - 1.0f;

		// if freezing permanently, then reduce freeze duration by freeze resistance
		flFreezeDuration *= ( 1.0f - m_flFreezeResistance );

		BaseClass::Freeze( 1.0f, pFreezer, pFreezeRay );			// make alien fully frozen

		m_flFrozenTime = gpGlobals->curtime + flFreezeDuration;
	}
	else
	{
		// if doing a partial freeze, then freeze resistance reduces that
		flFreezeAmount *= ( 1.0f - m_flFreezeResistance );

		BaseClass::Freeze( flFreezeAmount, pFreezer, pFreezeRay );
	}

	UpdateThawRate();
}

bool CASW_Alien::ShouldBecomeStatue()
{
	// Prevents parent classes from turning this NPC into a statue
	// We handle that ourselves if the alien dies while iced up
	return false;
}

void CASW_Alien::UpdateThawRate()
{
	if ( m_flFrozenTime > gpGlobals->curtime )
	{
		m_flFrozenThawRate = 0.0f;
	}
	else
	{
		m_flFrozenThawRate = m_flBaseThawRate * (1.5f - m_flFrozen);
	}
}

//------------------------------------------------------------------------------
// Purpose :	start and end trace position, amount 
//				of damage to do, and damage type. Returns a pointer to
//				the damaged entity in case the NPC wishes to do
//				other stuff to the victim (punchangle, etc)
//
//				Used for many contact-range melee attacks. Bites, claws, etc.
// Input   :
// Output  :
//------------------------------------------------------------------------------
CBaseEntity *CASW_Alien::CheckTraceHullAttack( const Vector &vStart, const Vector &vEnd, const Vector &mins, const Vector &maxs, float flDamage, int iDmgType, float flForceScale, bool bDamageAnyNPC )
{
	// Handy debuging tool to visualize HullAttack trace
	if ( ai_show_hull_attacks.GetBool() )
	{
		float length	 = (vEnd - vStart ).Length();
		Vector direction = (vEnd - vStart );
		VectorNormalize( direction );
		Vector hullMaxs = maxs;
		hullMaxs.x = length + hullMaxs.x;
		NDebugOverlay::BoxDirection(vStart, mins, hullMaxs, direction, 100,255,255,20,1.0);
		NDebugOverlay::BoxDirection(vStart, mins, maxs, direction, 255,0,0,20,1.0);
	}

	CTakeDamageInfo	dmgInfo( this, this, flDamage, DMG_SLASH );
	
	CASW_Trace_Filter_Melee traceFilter( this, COLLISION_GROUP_NONE, this, bDamageAnyNPC );

	Ray_t ray;
	ray.Init( vStart, vEnd, mins, maxs );

	trace_t tr;
	enginetrace->TraceRay( ray, MASK_SHOT_HULL, &traceFilter, &tr );

	Vector vecAttackerCenter = WorldSpaceCenter();
	
	for ( int i = 0; i < traceFilter.m_nNumHits; i++ )
	{
		trace_t *tr = &traceFilter.m_HitTraces[i];
		CBaseEntity *pHitEntity = tr->m_pEnt;

		Vector attackDir = pHitEntity->WorldSpaceCenter() - vecAttackerCenter;
		VectorNormalize( attackDir );
		CalculateMeleeDamageForce( &dmgInfo, attackDir, vecAttackerCenter, flForceScale );
		tr->m_pEnt->DispatchTraceAttack( dmgInfo, tr->endpos - vecAttackerCenter, tr );
#ifdef GAME_DLL
		ApplyMultiDamage();
#endif
	}
		
	CBaseEntity *pEntity = traceFilter.m_pHit;

	return pEntity;
}


//-----------------------------------------------------------------------------
// Purpose:
// NOTE: I removed the ON_GROUND check for Alien Swarm.  Do we need to put it 
//	back?
//-----------------------------------------------------------------------------
int	CASW_Alien::MeleeAttack1Conditions( float flDot, float flDist )
{
	// Should we even check this condition?
	if ( !meleeAttack1.m_bCheck )
		return COND_NONE;

	// Check range.
	if ( flDist < meleeAttack1.m_flMinDist )
		return COND_TOO_CLOSE_TO_ATTACK;

	if ( flDist > meleeAttack1.m_flMaxDist )
		return COND_TOO_FAR_TO_ATTACK;

	// Check facing.
	if ( meleeAttack1.m_flDotAngle != COMBAT_COND_NO_FACING_CHECK )
	{
		if ( flDot < meleeAttack1.m_flDotAngle )
			return COND_NOT_FACING_ATTACK;
	}

	return COND_CAN_MELEE_ATTACK1;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CASW_Alien::MeleeAttack2Conditions( float flDot, float flDist )
{
	// Should we even check this condition?
	if ( !meleeAttack2.m_bCheck )
		return COND_NONE;

	// Check ranges.
	if ( flDist < meleeAttack2.m_flMinDist )
		return COND_TOO_CLOSE_TO_ATTACK;

	if ( flDist > meleeAttack2.m_flMaxDist )
		return COND_TOO_FAR_TO_ATTACK;

	// Check facing.
	if ( meleeAttack1.m_flDotAngle != COMBAT_COND_NO_FACING_CHECK )
	{
		if ( flDot < meleeAttack2.m_flDotAngle )
			return COND_NOT_FACING_ATTACK;
	}

	return COND_CAN_MELEE_ATTACK2;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CASW_Alien::RangeAttack1Conditions ( float flDot, float flDist )
{
	// Should we even check this condition?
	if ( !rangeAttack1.m_bCheck )
		return COND_NONE;

	// Check ranges.
	if ( flDist < rangeAttack1.m_flMinDist )
		return COND_TOO_CLOSE_TO_ATTACK;

	if ( flDist > rangeAttack1.m_flMaxDist )
		return COND_TOO_FAR_TO_ATTACK;

	// Check facing.
	if ( meleeAttack1.m_flDotAngle != COMBAT_COND_NO_FACING_CHECK )
	{
		if ( flDot < rangeAttack1.m_flDotAngle )
			return COND_NOT_FACING_ATTACK;
	}

	return COND_CAN_RANGE_ATTACK1;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CASW_Alien::RangeAttack2Conditions ( float flDot, float flDist )
{
	// Should we even check this condition?
	if ( !rangeAttack2.m_bCheck )
		return COND_NONE;

	// Check ranges.
	if ( flDist < rangeAttack2.m_flMinDist )
		return COND_TOO_CLOSE_TO_ATTACK;

	if ( flDist > rangeAttack2.m_flMaxDist )
		return COND_TOO_FAR_TO_ATTACK;

	// Check facing.
	if ( meleeAttack1.m_flDotAngle != COMBAT_COND_NO_FACING_CHECK )
	{
		if ( flDot < rangeAttack2.m_flDotAngle )
			return COND_NOT_FACING_ATTACK;
	}

	return COND_CAN_RANGE_ATTACK2;
}

// give our aliens 360 degree vision
bool CASW_Alien::FInViewCone( const Vector &vecSpot )
{
	return true;
}

// always gib
bool CASW_Alien::ShouldGib( const CTakeDamageInfo &info )
{
	return true;
}

// player (and player controlled marines) always avoid drones
bool CASW_Alien::ShouldPlayerAvoid( void )
{
	return true;
}

// catching on fire
int CASW_Alien::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	int result = 0;	

	// scale burning damage up
	//if (dynamic_cast<CEntityFlame*>(info.GetAttacker()))
	if (dynamic_cast<CASW_Burning*>(info.GetInflictor()))
	{
		CTakeDamageInfo newDamage = info;		
		newDamage.ScaleDamage(asw_fire_alien_damage_scale.GetFloat());
		if (asw_debug_alien_damage.GetBool())
		{
			Msg("%d %s hurt by %f dmg (scaled up by asw_fire_alien_damage_scale)\n", entindex(), GetClassname(), newDamage.GetDamage());
		}
		result = BaseClass::OnTakeDamage_Alive(newDamage);
	}
	else
	{
		if (asw_debug_alien_damage.GetBool())
		{
			Msg("%d %s hurt by %f dmg\n", entindex(), GetClassname(), info.GetDamage());
		}
		result = BaseClass::OnTakeDamage_Alive(info);
	}

	// if we take fire damage, catch on fire
	if ( result > 0 && ( info.GetDamageType() & DMG_BURN ) )
	{
		ASW_Ignite( asw_alien_burn_duration.GetFloat(), 0, info.GetAttacker(), info.GetWeapon() );
	}

	// make the alien move slower for 0.5 seconds
	if (info.GetDamageType() & DMG_SHOCK)
	{
		ElectroStun( asw_stun_grenade_time.GetFloat() );		

		m_fNoDamageDecal = true;
	}
	else
	{
		if (m_fHurtSlowMoveTime < gpGlobals->curtime + 0.5f)
			m_fHurtSlowMoveTime = gpGlobals->curtime + 0.5f;
	}

	CASW_Marine* pMarine = NULL;
	if ( info.GetAttacker() && info.GetAttacker()->Classify() == CLASS_ASW_MARINE )
	{
		pMarine = static_cast<CASW_Marine*>( info.GetAttacker() );
	}
	if (pMarine)
		pMarine->HurtAlien(this, info);

	// Notify gamestats of the damage
	CASW_GameStats.Event_AlienTookDamage( this, info );
		
	if ( m_RecentDamage.Count() == ASW_NUM_RECENT_DAMAGE )
	{
		m_RecentDamage.RemoveAtHead();
	}
	m_RecentDamage.Insert( info );
	if ( m_pFlinchBehavior )
	{
		m_pFlinchBehavior->OnOuterTakeDamage( info );
	}

	return result;
}

// ===================
// schedule/task stuff
// ====================

void CASW_Alien::StartTask( const Task_t *pTask )
{
	//int task = pTask->iTask;
	switch ( pTask->iTask )
	{
	case TASK_BURROW_WAIT:
		if ( pTask->flTaskData == 1.0f )
		{
			//Set our next burrow time
			if (m_flBurrowTime == 0)
				m_flBurrowTime = gpGlobals->curtime;
			else
				m_flBurrowTime = gpGlobals->curtime + random->RandomFloat( 1,  6);
		}
		break;

	case TASK_UNBURROW:
		Unburrow();
		break;
	case TASK_CHECK_FOR_UNBORROW:
		//if ( ValidBurrowPoint( GetAbsOrigin() ) )
		{
			m_spawnflags &= ~SF_NPC_GAG;
			RemoveSolidFlags( FSOLID_NOT_SOLID );
			TaskComplete();
		}
		break;
	case TASK_SET_UNBURROW_IDLE_ACTIVITY:
		{
			SetIdealActivity( m_UnburrowIdleActivity );
		}
		break;
	case TASK_ASW_WAIT_FOR_ORDER_MOVE:
		{
			if ( IsMovementFrozen() )
			{
				TaskFail(FAIL_FROZEN);
				break;
			}

			if (GetNavigator()->GetGoalType() == GOALTYPE_NONE)
			{
				TaskComplete();
				GetNavigator()->ClearGoal();		// Clear residual state
			}
			else if (!GetNavigator()->IsGoalActive())
			{
				SetIdealActivity( GetStoppedActivity() );
			}
			else
			{
				// Check validity of goal type
				ValidateNavGoal();
			}
		}
		break;
	case TASK_ASW_ORDER_RETRY_WAIT:	
		//Msg("TASK_ASW_ORDER_RETRY_WAIT %d tries. doorblocked=%d\n", m_iNumASWOrderRetries, m_bBlockedByOpeningDoor);		
		m_iNumASWOrderRetries++;
		if (m_iNumASWOrderRetries > 5)
		{
			TaskFail("Failed to find route to order target\n");			
		}
		else
		{			
			SetWait(MIN(float(m_iNumASWOrderRetries) / 2.0f, 12.0f));	// wait a bit longer in between each try
		}		
		break;
	case TASK_ASW_REPORT_BLOCKING_ENT:
		Msg("Last blocking ent was %d:%s\n",
			GetNavigator()->GetBlockingEntity(),
			GetNavigator()->GetBlockingEntity() ? GetNavigator()->GetBlockingEntity()->GetClassname() : "Unknown");
		TaskComplete();
		break;
	case TASK_ASW_ALIEN_ZIGZAG:
		m_bPerformingZigZag = false;
		break;
	// override the default TASK_GET_PATH_TO_ENEMY to make our alien run at the end of the path
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
				if (m_bRunAtChasingPathEnds)
				{
					//Msg("Setting arrival speed to %f\n", GetIdealSpeed());
					//GetNavigator()->SetArrivalSpeed(GetIdealSpeed()); // asw test
					GetNavigator()->SetArrivalSpeed(300.0f); // asw test
				}
				TaskComplete();
			}
			else
			{
				// no way to get there =( 
				DevWarning( 2, "GetPathToEnemy failed!!\n" );
				RememberUnreachable(GetEnemy());
				TaskFail(FAIL_NO_ROUTE);
			}
			return;
		}
	case TASK_GET_PATH_TO_ENEMY_LKP:
		{
			CBaseEntity *pEnemy = GetEnemy();
			if (!pEnemy || IsUnreachable(pEnemy))
			{
				TaskFail(FAIL_NO_ROUTE);
				return;
			}
			AI_NavGoal_t goal( GetEnemyLKP() );

			TranslateNavGoal( pEnemy, goal.dest );

			if ( GetNavigator()->SetGoal( goal, AIN_CLEAR_TARGET ) )
			{
				if (m_bRunAtChasingPathEnds)
				{
					//Msg("Setting arrival speed to %f\n", GetIdealSpeed());
					//GetNavigator()->SetArrivalSpeed(GetIdealSpeed()); // asw test
					GetNavigator()->SetArrivalSpeed(300.0f); // asw test
				}
				TaskComplete();
			}
			else
			{
				// no way to get there =(
				DevWarning( 2, "GetPathToEnemyLKP failed!!\n" );
				RememberUnreachable(GetEnemy());
				TaskFail(FAIL_NO_ROUTE);
			}
			break;
		}
	case TASK_ASW_SPREAD_THEN_HIBERNATE:
		{
			// This task really uses 2 parameters, so we have to extract
			// them from a single integer. To send both parameters, the
			// formula is MIN_DIST * 10000 + MAX_DIST
			{
				int iMinDist, iMaxDist;

				iMinDist = 90;
				iMaxDist = 200;

				// try to find a simple vector goal
				if ( GetNavigator()->SetWanderGoal( iMinDist, iMaxDist ) )
					TaskComplete();
				else
				{
					// if we couldn't go for a full path
					if ( GetNavigator()->SetRandomGoal( 150.0f ) )
						TaskComplete();
					else
						TaskFail(FAIL_NO_REACHABLE_NODE);
				}
			}
		}
		break;
	case TASK_ASW_BUILD_PATH_TO_ORDER:
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

	default:
		{
			BaseClass::StartTask(pTask);
			break;
		}
	}
}


bool CASW_Alien::IsCurTaskContinuousMove()
{
	const Task_t* pTask = GetTask();
	if( !pTask || pTask->iTask == TASK_ASW_WAIT_FOR_ORDER_MOVE )
		return true;

	return BaseClass::IsCurTaskContinuousMove();
}

void CASW_Alien::RunTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_BURROW_WAIT:
		//See if enough time has passed
		if ( m_flBurrowTime < gpGlobals->curtime )
		{
			TaskComplete();
		}
		break;
	case TASK_ASW_WAIT_FOR_ORDER_MOVE:
		{			
			if ( IsMovementFrozen() )
			{
				TaskFail(FAIL_FROZEN);
				break;
			}

			bool fTimeExpired = ( pTask->flTaskData != 0 && pTask->flTaskData < gpGlobals->curtime - GetTimeTaskStarted() );

			if (fTimeExpired || GetNavigator()->GetGoalType() == GOALTYPE_NONE)
			{
				TaskComplete();
				GetNavigator()->StopMoving();		// Stop moving
			}
			else if (!GetNavigator()->IsGoalActive())
			{
				SetIdealActivity( GetStoppedActivity() );
			}
			else
			{
				// Check validity of goal type
				ValidateNavGoal();

				if ( m_AlienOrderObject.Get() )
				{
					const Vector &vecGoalPos = m_AlienOrderObject->GetAbsOrigin();
					if ( ( GetNavigator()->GetGoalPos() - vecGoalPos ).LengthSqr() > Square( 60 ) )
					{
						if ( GetNavigator()->GetNavType() != NAV_JUMP )
						{
							if ( !GetNavigator()->UpdateGoalPos( vecGoalPos ) )
							{
								TaskFail( FAIL_NO_ROUTE );
							}
						}
					}
				}
			}
			break;
		}

	case TASK_CHECK_FOR_UNBORROW:
		//Must wait for our next check time
		if ( m_flBurrowTime > gpGlobals->curtime )
			return;

		//See if we can pop up
		// todo: see if a marine is near?
		//if ( ValidBurrowPoint( GetAbsOrigin() ) )
		{
			m_spawnflags &= ~SF_NPC_GAG;			
			RemoveSolidFlags( FSOLID_NOT_SOLID );

			TaskComplete();
			return;
		}

		//Try again in a couple of seconds
		m_flBurrowTime = gpGlobals->curtime + random->RandomFloat( 0.5f, 1.0f );
		break;
	case TASK_UNBURROW:
		{
			AutoMovement();
			if ( IsActivityFinished() )
			{
				RemoveFlag( FL_FLY );
				CheckForBlockingTraps();
				SetCollisionGroup( m_nAlienCollisionGroup );
				TaskComplete();
			}
			break;
		}
	case TASK_SET_UNBURROW_IDLE_ACTIVITY:
		{
			AutoMovement();
			if ( IsActivityFinished() )
			{
				TaskComplete();
			}
			break;
		}
	case TASK_ASW_ORDER_RETRY_WAIT:
		{
			if ( IsWaitFinished() )
			{
				TaskComplete();
			}
			break;
		}
	case TASK_ASW_ALIEN_ZIGZAG:
		{
			if ( IsMovementFrozen() )
			{
				TaskFail(FAIL_FROZEN);
				break;
			}

			if (GetNavigator()->GetGoalType() == GOALTYPE_NONE)
			{
				m_bPerformingZigZag = false;
				TaskComplete();
				GetNavigator()->StopMoving();		// Stop moving
			}
			else if (!GetNavigator()->IsGoalActive())
			{
				SetIdealActivity( GetStoppedActivity() );
			}
			else if (ValidateNavGoal())
			{
				SetIdealActivity( GetNavigator()->GetMovementActivity() );

				// if we've just spotted our enemy, recompute our path, to try and get a single local path to add a zigzag to
				/*
				if (HasCondition( COND_SEE_ENEMY ) && !m_bLastSeeEnemy)
				{
					if ( !GetNavigator()->RefindPathToGoal( false ) )
					{	
						TaskFail( FAIL_NO_ROUTE );
						return;
					}
				}
				m_bLastSeeEnemy = HasCondition( COND_SEE_ENEMY );

				if (GetNavigator()->CurWaypointIsGoal())
				{
					m_bPerformingZigZag = false;
					AddZigZagToPath();
				}*/
			}
			break;
		}
	default:
		{
			BaseClass::RunTask( pTask );
		}
	}
}


#define ZIG_ZAG_SIZE 1500	// was 3600

// don't change our chase goal while we're on a zigzag detour
bool CASW_Alien::ShouldUpdateEnemyPos()
{
	if (GetTask() && GetTask()->iTask == TASK_ASW_ALIEN_ZIGZAG
		//&& (GetNavigator()->GetCurWaypointFlags() & bits_WP_TO_DETOUR) )
		&& m_bPerformingZigZag)
		return false;

	// don't repath if we're heading to a point that shouldn't be simplified
	if (asw_drone_zig_zagging.GetBool() && GetNavigator()->GetCurWaypointFlags() & bits_WP_DONT_SIMPLIFY)
	{
		return false;
	}

	//Msg("updating enemy pos %f\n", gpGlobals->curtime);
	return true;
}



float CASW_Alien::GetGoalRepathTolerance( CBaseEntity *pGoalEnt, GoalType_t type, const Vector &curGoal, const Vector &curTargetPos )
{
	if (!ShouldUpdateEnemyPos())
		return 10000.0f;	// allow huge tolerance to prevent repath

	

	//return BaseClass::GetGoalRepathTolerance(pGoalEnt, type, curGoal, curTargetPos);
	float distToGoal = ( GetAbsOrigin() - curTargetPos ).Length() - GetNavigator()->GetArrivalDistance();
	float distMoved1Sec = GetSmoothedVelocity().Length();
	float result = 120;  // FIXME: why 120?

	if (distToGoal <= 100)
		return GetMotor()->MinStoppingDist(0.0f);
	
	if (distMoved1Sec > 0.0)
	{
		float t = distToGoal / distMoved1Sec;

		result = clamp( 120 * t, 0, 120 );
		// Msg("t %.2f : d %.0f  (%.0f)\n", t, result, distMoved1Sec );
	}
		
	if ( !pGoalEnt->IsPlayer() )
		result *= 1.20;
		
	return result;
}

void CASW_Alien::AddZigZagToPath(void) 
{
	// If already on a detour don't add a zigzag
	if (GetNavigator()->GetCurWaypointFlags() & bits_WP_TO_DETOUR)
	{
		return;
	}
	
	// If enemy isn't facing me or occluded, don't add a zigzag
	//if (HasCondition(COND_ENEMY_OCCLUDED) || !HasCondition ( COND_ENEMY_FACING_ME ))
	//{
		//return;
	//}

	Vector waypointPos = GetNavigator()->GetCurWaypointPos();
	Vector waypointDir = (waypointPos - GetAbsOrigin());

	// If the distance to the next node is greater than ZIG_ZAG_SIZE
	// then add a random zig/zag to the path
	if (waypointDir.LengthSqr() > ZIG_ZAG_SIZE)
	{
		// Pick a random distance for the zigzag (less that sqrt(ZIG_ZAG_SIZE)		
		float fZigZigDistance = random->RandomFloat( 100, 200 ); //30, 60 );
		int iZigZagSide = random->RandomInt(0,1);
		
		
		Msg("adding a zig zag of %f units\n", fZigZigDistance);		

		// Get me a vector orthogonal to the direction of motion
		VectorNormalize( waypointDir );
		Vector vDirUp(0,0,1);
		Vector vDir;
		CrossProduct( waypointDir, vDirUp, vDir);

		// Pick a random direction (left/right) for the zigzag
		if (iZigZagSide)
		{
			vDir = -1 * vDir;
		}

		// Get zigzag position in direction of target waypoint
		Vector zigZagPos = GetAbsOrigin() + waypointDir * 200;

		// Now offset 
		zigZagPos = zigZagPos + (vDir * fZigZigDistance);

		// Now make sure that we can still get to the zigzag position and the waypoint
		AIMoveTrace_t moveTrace1, moveTrace2;
		GetMoveProbe()->MoveLimit( NAV_GROUND, GetAbsOrigin(), zigZagPos, MASK_NPCSOLID, NULL, &moveTrace1);
		GetMoveProbe()->MoveLimit( NAV_GROUND, zigZagPos, waypointPos, MASK_NPCSOLID, NULL, &moveTrace2);
		if ( !IsMoveBlocked( moveTrace1 ) && !IsMoveBlocked( moveTrace2 ) )
		{
			m_bPerformingZigZag = true;
			GetNavigator()->PrependWaypoint( zigZagPos, NAV_GROUND, bits_WP_TO_DETOUR );
		}
	}
}

int CASW_Alien::TranslateSchedule( int scheduleType )
{
	if (scheduleType == SCHED_CHASE_ENEMY)
		return SCHED_ASW_ALIEN_CHASE_ENEMY;

	int i = BaseClass::TranslateSchedule(scheduleType);

	if ( i == SCHED_MELEE_ATTACK1 )
	{
		if ( ShouldStopBeforeMeleeAttack() )
		{
			return SCHED_ASW_ALIEN_SLOW_MELEE_ATTACK1;
		}
		return SCHED_ASW_ALIEN_MELEE_ATTACK1;
	}

	return i;
}

// pushes aliens out of each other
void CASW_Alien::PerformPushaway()
{
	VPROF("CASW_Alien::PerformPushaway");
	SetupPushawayVector();

#ifdef MOVEPROBE_PUSHAWAY
	AIMoveTrace_t moveTrace;
	unsigned testFlags = AITGM_IGNORE_FLOOR;
	GetMoveProbe()->TestGroundMove( GetLocalOrigin(), GetAbsOrigin() + m_vecLastPush, GetAITraceMask(), testFlags, &moveTrace );
	UTIL_SetOrigin( this, moveTrace.vEndPosition, true );
#else
	// check if we can safely move through this push
	Vector dest = GetAbsOrigin() + m_vecLastPush;
	trace_t tr;
	AI_TraceHull( GetAbsOrigin(), dest, WorldAlignMins(), WorldAlignMaxs(), GetAITraceMask(), this, GetCollisionGroup(), &tr );
	if ( !tr.startsolid && (tr.fraction == 1.0) )
	{
		// all was clear, move into new position
		UTIL_SetOrigin(this, dest);
	}
#endif

	// if we're close to stopping, zero our velocity
	if ( GetMotor()->GetCurSpeed() < 1.0f )
	{
		GetMotor()->SetMoveVel( Vector( 0,0,0 ) );
	}
	// if we were pushed, slow us down a bit
	else if ( m_bPushed )
	{
		float scale_vel = 1.0f - ( m_vecLastPush.Length2D() / asw_springcol_push_cap.GetFloat() );
		GetMotor()->SetMoveVel( GetMotor()->GetCurVel() * scale_vel );
	}
}

float CASW_Alien::GetSpringColRadius()
{
	return asw_springcol_radius.GetFloat();
}

void CASW_Alien::SetupPushawayVector()
{
	CASW_Alien *pOtherAlien;
	Vector vecPush;
	vecPush.Init();
	bool m_bPushed = false;
	float flSpringColRadius = GetSpringColRadius();
	// go through all aliens

	int iAliens = IAlienAutoList::AutoList().Count();
	for ( int i = 0; i < iAliens; i++ )
	{
		pOtherAlien = static_cast< CASW_Alien* >( IAlienAutoList::AutoList()[ i ] );
		if ( pOtherAlien != this && !( Classify() == CLASS_ASW_HARVESTER && pOtherAlien->Classify() != CLASS_ASW_PARASITE )
				&& !( Classify() == CLASS_ASW_PARASITE && pOtherAlien->Classify() != CLASS_ASW_HARVESTER ) )
		{
			Vector diff = m_vecLastPushAwayOrigin - pOtherAlien->GetAbsOrigin();
			float dist = diff.Length();
			VectorNormalize( diff );
			// TODO: Customize springcolradius per alien type
			if ( dist < flSpringColRadius )		// if alien is too close, add a force from him to our push vector
			{
				dist -= flSpringColRadius * asw_springcol_core.GetFloat();
				if (dist < 1)
					dist = 1;
				float push_amount = flSpringColRadius / dist;
				vecPush += push_amount * diff * asw_springcol_force_scale.GetFloat();
				m_bPushed = true;
			}
		}
	}

	// cap the push vector
	float over_length = vecPush.Length() / asw_springcol_push_cap.GetFloat();
	if (over_length > 1)
		vecPush /= over_length;

	// push out of players with equal force to total push from other drones
	int iMaxMarines = ASWGameResource()->GetMaxMarineResources();
	for ( int i = 0; i < iMaxMarines; i++ )
	{
		CASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( i );
		if ( !pMR )
			continue;

		CASW_Marine *pMarine = pMR->GetMarineEntity();
		if ( !pMarine )
			continue;

		Vector diff = m_vecLastPushAwayOrigin - pMarine->GetAbsOrigin();
		float dist = diff.Length();
		VectorNormalize( diff );
		if ( dist < flSpringColRadius )		// if drone is too close, add a force from him to our push vector
		{
			dist -= flSpringColRadius * asw_springcol_core.GetFloat();
			if ( dist < 1 )
			{
				dist = 1;
			}
			float push_amount = flSpringColRadius / dist;
			vecPush += push_amount * diff * asw_springcol_force_scale.GetFloat();
			m_bPushed = true;
		}
	}
	// push away from tesla traps
	int nTraps = g_aTeslaTraps.Count();
	for ( int i = 0; i < nTraps; i++ )
	{
		Vector diff = m_vecLastPushAwayOrigin - g_aTeslaTraps[i]->GetAbsOrigin();
		float dist = diff.Length();
		VectorNormalize( diff );
		if ( dist < flSpringColRadius )		// if drone is too close, add a force from him to our push vector
		{
			dist -= flSpringColRadius * asw_springcol_core.GetFloat();
			if ( dist < 1 )
			{
				dist = 1;
			}
			float push_amount = flSpringColRadius / dist;
			vecPush += push_amount * diff * asw_springcol_force_scale.GetFloat();
			m_bPushed = true;
		}
	}
	// cap the push vector
	over_length = vecPush.Length() / asw_springcol_push_cap.GetFloat();
	if ( over_length > 1 )
	{
		vecPush /= over_length;
	}

	// smooth the push vector
	m_vecLastPush = (m_vecLastPush * 2.0f + vecPush) / 3.0f;
	//Msg("%d Pushaway vector size %f\n", entindex(), m_vecLastPush.Length2D());
	if ( asw_springcol_debug.GetInt() == -1 ||
		asw_springcol_debug.GetInt() == entindex() )
	{
		float flYaw = UTIL_VecToYaw( m_vecLastPush );
		NDebugOverlay::YawArrow( GetAbsOrigin() + Vector( 0, 0, 24 ), flYaw, 64, 8, 255, 255, 0, 0, true, 0.1f );
	}
	m_vecLastPushAwayOrigin = GetAbsOrigin();
}

bool CASW_Alien::CanBePushedAway()
{
	// no pushing away while burrowed
	if ( IsCurSchedule( SCHED_BURROW_WAIT, false ) || IsCurSchedule( SCHED_WAIT_FOR_CLEAR_UNBORROW, false )
		|| IsCurSchedule( SCHED_BURROW_OUT, false ) )
		return false;

	// don't push away aliens that have low sight ('asleep')
	return !IsCurSchedule( CAI_ASW_SleepBehavior::SCHED_SLEEP_UNBURROW );
}

AI_BEGIN_CUSTOM_NPC( asw_alien, CASW_Alien )
	DECLARE_CONDITION( COND_ASW_BEGIN_COMBAT_STUN )
	DECLARE_CONDITION( COND_ASW_FLINCH )

	DECLARE_TASK( TASK_ASW_ALIEN_ZIGZAG )
	DECLARE_TASK( TASK_ASW_SPREAD_THEN_HIBERNATE )
	DECLARE_TASK( TASK_ASW_BUILD_PATH_TO_ORDER )
	DECLARE_TASK( TASK_ASW_ORDER_RETRY_WAIT )
	DECLARE_TASK( TASK_ASW_REPORT_BLOCKING_ENT )
	DECLARE_TASK( TASK_UNBURROW )
	DECLARE_TASK( TASK_CHECK_FOR_UNBORROW )
	DECLARE_TASK( TASK_BURROW_WAIT )
	DECLARE_TASK( TASK_SET_UNBURROW_IDLE_ACTIVITY )
	DECLARE_TASK( TASK_ASW_WAIT_FOR_ORDER_MOVE )

	DECLARE_ACTIVITY( ACT_MELEE_ATTACK1_HIT )
	DECLARE_ACTIVITY( ACT_MELEE_ATTACK2_HIT )
	DECLARE_ACTIVITY( ACT_ALIEN_FLINCH_SMALL )
	DECLARE_ACTIVITY( ACT_ALIEN_FLINCH_MEDIUM )
	DECLARE_ACTIVITY( ACT_ALIEN_FLINCH_BIG )
	DECLARE_ACTIVITY( ACT_ALIEN_FLINCH_GESTURE )
   	DECLARE_ACTIVITY( ACT_DEATH_FIRE )
	DECLARE_ACTIVITY( ACT_DEATH_ELEC )
	DECLARE_ACTIVITY( ACT_DIE_FANCY )
	DECLARE_ACTIVITY( ACT_BURROW_OUT )
	DECLARE_ACTIVITY( ACT_BURROW_IDLE )

	DECLARE_ANIMEVENT( AE_ALIEN_MELEE_HIT )

	//=========================================================
	// > SCHED_ASW_ALIEN_CHASE_ENEMY
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ASW_ALIEN_CHASE_ENEMY,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY_FAILED"
		//"		TASK_SET_TOLERANCE_DISTANCE		24"
		"		TASK_GET_PATH_TO_ENEMY			300"
		"		TASK_RUN_PATH					0"
		//"		TASK_ASW_ALIEN_ZIGZAG			0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_TOO_CLOSE_TO_ATTACK"
		"		COND_TASK_FAILED"
		"		COND_HEAR_DANGER"
	)

	// Same as base melee attack1, minus the TASK_STOP_MOVING
	DEFINE_SCHEDULE
	(
		SCHED_ASW_ALIEN_MELEE_ATTACK1,

		"	Tasks"

		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_MELEE_ATTACK1		0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_ENEMY_OCCLUDED"
	);
	DEFINE_SCHEDULE
		(
		SCHED_ASW_ALIEN_SLOW_MELEE_ATTACK1,

		"	Tasks"

		"		TASK_STOP_MOVING		1"
		"		TASK_WAIT				0.1"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
		"		TASK_MELEE_ATTACK1		1"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_ENEMY_OCCLUDED"
		);

	DEFINE_SCHEDULE
	(
		SCHED_ASW_SPREAD_THEN_HIBERNATE,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_ASW_SPREAD_THEN_HIBERNATE	0"
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_STOP_MOVING				0"
		"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_IDLE_STAND"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ASW_ORDER_MOVE,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_ASW_RETRY_ORDERS"
		"		TASK_ASW_BUILD_PATH_TO_ORDER	0"
		"		TASK_RUN_PATH			0"
		"		TASK_ASW_WAIT_FOR_ORDER_MOVE	0"		// 0 is spread if fail to build path
		//"		TASK_WAIT_PVS			0"
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

	DEFINE_SCHEDULE
	(
		SCHED_ASW_RETRY_ORDERS,

		"	Tasks"
		//"		TASK_ASW_REPORT_BLOCKING_ENT	0"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_ASW_SPREAD_THEN_HIBERNATE"
		"		TASK_ASW_ORDER_RETRY_WAIT		1"	// this will fail the schedule if we've retried too many times
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_ASW_RETRY_ORDERS"
		"		TASK_ASW_BUILD_PATH_TO_ORDER	1"		// 1 is no spread if fail to build path
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
	DEFINE_SCHEDULE
	(
		SCHED_BURROW_WAIT,		
		"	Tasks"		
		"		TASK_SET_UNBURROW_IDLE_ACTIVITY	0"
		"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_BURROW_WAIT"
		"		TASK_BURROW_WAIT			1"
		"		TASK_SET_SCHEDULE					SCHEDULE:SCHED_WAIT_FOR_CLEAR_UNBORROW"
		""
		"	Interrupts"
		"		COND_TASK_FAILED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_WAIT_FOR_CLEAR_UNBORROW,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_BURROW_WAIT"
		"		TASK_CHECK_FOR_UNBORROW		1"
		"		TASK_SET_SCHEDULE					SCHEDULE:SCHED_BURROW_OUT"
		""
		"	Interrupts"
		"		COND_TASK_FAILED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_BURROW_OUT,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_BURROW_WAIT"
		"		TASK_UNBURROW			0"
		""
		"	Interrupts"
		"		COND_TASK_FAILED"
	)

AI_END_CUSTOM_NPC()



// Behaviour stuff to support act busy
bool CASW_Alien::CreateBehaviors()
{
	AddBehavior( &m_ActBusyBehavior );
	
	return BaseClass::CreateBehaviors();
}

int CASW_Alien::SelectAlienOrdersSchedule()
{
	int schedule = 0;
	// check if we have any alien orders to follow
	switch (m_AlienOrders)
	{
	case AOT_MoveToIgnoringMarines:
		{
			IgnoreMarines(true);
			schedule = SCHED_ASW_ORDER_MOVE;			
		}
		break;
	case AOT_SpreadThenHibernate:
	case AOT_MoveTo:
		{
			IgnoreMarines(false);
			schedule = SCHED_ASW_ORDER_MOVE;			
		}
		break;
	case AOT_MoveToNearestMarine:
		{
			float marine_distance;
			// refresh our order target, in case the one passed in by the orders is no longer the nearest
			m_AlienOrderObject = UTIL_ASW_NearestMarine( GetAbsOrigin(), marine_distance );
			IgnoreMarines(false);
			schedule = SCHED_ASW_ORDER_MOVE;			
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

int CASW_Alien::SelectSchedule( void )
{
	// If we're supposed to be burrowed, stay there
	if ( m_bStartBurrowed )
		return SCHED_BURROW_WAIT;

	// see if our alien orders want to schedule something
	int order_schedule = SelectAlienOrdersSchedule();
	if (order_schedule > 0)
	{
		//Msg("SelectSchedule picked alien orders\n");
		return order_schedule;
	}

	if ( !BehaviorSelectSchedule() )
	{
	}

	return BaseClass::SelectSchedule();
}

ConVar asw_drone_death_force_pitch( "asw_drone_death_force_pitch", "-10", FCVAR_CHEAT, "Tilt the angle of death forces on alien ragdolls" );
ConVar asw_drone_death_force( "asw_drone_death_force", "5", FCVAR_CHEAT, "Scale for alien death forces" );

#define DRONE_MASS 90.0f

Vector CASW_Alien::CalcDeathForceVector( const CTakeDamageInfo &info )
{
	if ( asw_debug_alien_damage.GetBool() )
	{
		Msg( "Death force pre = %f\n", info.GetDamageForce().Length() );
	}

	if ( !( info.GetDamageType() & DMG_BLAST || info.GetDamageType() & DMG_BURN ) )
	{
		// normalize force for non-explosive weapons, as they each have different fire rates/forces
		// TODO: Tilt force vector upwards a bit

		float flDesiredForceScale = asw_drone_death_force.GetFloat() * 10000.0f;
		float flMassScale = 1.0f;
		if ( VPhysicsGetObject() )
		{
			flMassScale = 1.0f / ( VPhysicsGetObject()->GetMass() / DRONE_MASS );		// using drone's mass as a baseline
		}

		CBaseEntity *pForce = info.GetInflictor();
		if ( !pForce )
		{
			pForce = info.GetAttacker();
		}
		Vector forceVector = vec3_origin;

		if ( pForce )
		{
			forceVector = GetAbsOrigin() - pForce->GetAbsOrigin();
			forceVector.NormalizeInPlace();

			if ( asw_debug_alien_damage.GetBool() )
			{
				Msg( "Death force post = %f flDesiredForceScale=%f flMassScale=%f\n", (forceVector * flDesiredForceScale * flMassScale).Length(), flDesiredForceScale, flMassScale );
			}

			return forceVector * flDesiredForceScale * flMassScale;
		}
		else
		{
			forceVector.x = random->RandomFloat( -1.0f, 1.0f );
			forceVector.y = random->RandomFloat( -1.0f, 1.0f );
			forceVector.z = 0.0;
			
			if ( asw_debug_alien_damage.GetBool() )
			{
				Msg( "Death force post = %f flDesiredForceScale=%f flMassScale=%f\n", (forceVector * flDesiredForceScale * flMassScale).Length(), flDesiredForceScale, flMassScale );
			}

			return forceVector * (flDesiredForceScale/4) * flMassScale;
		}
	}
	return BaseClass::CalcDeathForceVector( info );
}

void CASW_Alien::BreakAlien( const CTakeDamageInfo &info )
{
	Vector	velocity;
	velocity = CalcDeathForceVector( info )/150;
	if ( velocity == vec3_origin )
		velocity = Vector( RandomFloat( 1, 15 ), RandomFloat( 1, 15 ), 75.0f );
	velocity.z *= 1.25;
	velocity.z += 175.0;
	AngularImpulse	angVelocity = RandomAngularImpulse( -500.0f, 500.0f );
	breakablepropparams_t params( GetAbsOrigin(), GetAbsAngles(), velocity, angVelocity );
	params.impactEnergyScale = 1.0f;
	params.defBurstScale = 100.0f;
	params.defCollisionGroup = COLLISION_GROUP_DEBRIS;
	PropBreakableCreateAll( GetModelIndex(), NULL, params, this, -1, true, true ); 
}

//=========================================================
// GetDeathActivity - determines the best type of death
// anim to play.
//=========================================================
Activity CASW_Alien::GetDeathActivity ( void )
{
	Activity	deathActivity;
	deathActivity = ACT_DIESIMPLE;

	if ( m_nDeathStyle == kDIE_FANCY )
	{
		if ( m_bOnFire )
			deathActivity = ( Activity ) ACT_DEATH_FIRE;
		else if ( m_bElectroStunned )
			deathActivity = ( Activity ) ACT_DEATH_ELEC;	  
		else
			deathActivity = ( Activity ) ACT_DIE_FANCY;
	}

	//BaseClass::GetDeathActivity();
	return deathActivity; 
}


bool CASW_Alien::CanBecomeRagdoll( void ) 
{
	MDLCACHE_CRITICAL_SECTION();

	int ragdollSequence = SelectWeightedSequence( ACT_DIERAGDOLL );
	
	if ( m_nDeathStyle == kDIE_FANCY && !m_bTimeToRagdoll ) 
		return false;

	//Can't cause we don't have a ragdoll sequence.
	if ( ragdollSequence == ACTIVITY_NOT_AVAILABLE )
		return false;

	if ( GetFlags() & FL_TRANSRAGDOLL )
		return false;

	return true;
}

bool CASW_Alien::CanDoFancyDeath()
{	
	if ( m_nDeathStyle == kDIE_BREAKABLE || m_nDeathStyle == kDIE_INSTAGIB )
		return false;

	if ( GetNavType() != NAV_GROUND )
		return false;

	if ( IsCurSchedule( SCHED_BURROW_WAIT, false ) || IsCurSchedule( SCHED_WAIT_FOR_CLEAR_UNBORROW, false ) || IsCurSchedule( SCHED_BURROW_OUT, false ) )
		return false;

	if ( m_hMoveClone.Get() )
		return false;

	if ( m_bOnFire )
	{
		// do i have the custom death activities?
		if ( SelectWeightedSequence ( ( Activity ) ACT_DEATH_FIRE ) == ACTIVITY_NOT_AVAILABLE || RandomFloat() < 0.5f )
			return false;
	}
	else if ( m_bElectroStunned )
	{
		// do i have the custom death activities?
		if ( SelectWeightedSequence ( ( Activity ) ACT_DEATH_ELEC ) == ACTIVITY_NOT_AVAILABLE || RandomFloat() < 0.75f )
			return false;
	}
	// do i have the custom death activities?
	else 
	{
		if ( m_fFancyDeathChance > asw_alien_fancy_death_chance.GetFloat() )
			return false;

		if ( SelectWeightedSequence ( ( Activity ) ACT_DIE_FANCY ) == ACTIVITY_NOT_AVAILABLE  )
			return false;
	}

	return true;
}

extern ConVar asw_fist_ragdoll_chance;

void CASW_Alien::Event_Killed( const CTakeDamageInfo &info )
{
	if (asw_debug_alien_damage.GetBool())
		Msg("%f alien killed\n", gpGlobals->curtime);
	if (ASWGameRules())
	{
		ASWGameRules()->AlienKilled(this, info);
	}

	CASW_GameStats.Event_AlienKilled( this, info );

	if ( ASWDirector() )
		ASWDirector()->Event_AlienKilled( this, info );

	if (m_hSpawner.Get())
		m_hSpawner->AlienKilled(this);

	//bool bRagdollCreated = Dissolve( NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_ELECTRICAL );

	//switch to the dead bodygroup if you are not on fire or electrocuted
	if ( HasDeadBodyGroup() && !m_bOnFire && !m_bElectroStunned )
	{
		SetBodygroup( 0, m_iDeadBodyGroup );
	}

	if ( m_flFrozen >= 0.1f )
	{
		bool bShatter = ( RandomFloat() > 0.01f );
		CreateASWServerStatue( this, COLLISION_GROUP_NONE, info, bShatter );
		BaseClass::Event_Killed( CTakeDamageInfo( info.GetAttacker(), info.GetAttacker(), info.GetDamage(), DMG_GENERIC | DMG_REMOVENORAGDOLL | DMG_PREVENT_PHYSICS_FORCE ) );
		RemoveDeferred();
		return;
	}

	// if we died from an explosion, instagib
	if ( !m_bNeverInstagib && info.GetDamage() > 8.0f && (info.GetDamageType() & DMG_BLAST || info.GetDamageType() & DMG_SONIC) )
	{
		m_nDeathStyle = kDIE_INSTAGIB;
	}
	else if ( !m_bNeverInstagib && !m_bElectroStunned && info.GetDamage() > 20.0f && RandomFloat() > 0.05f ) // if the damage inflicted was a high amount of damage, instagib 95% of the time
	{
		m_nDeathStyle = kDIE_INSTAGIB;
	}
	else  // else see if we can break apart
	{
		// if we can do a fancy death or we don't do a break, we will ragdoll to the split version
		if ( CanDoFancyDeath() )
		{
			m_nDeathStyle = kDIE_FANCY;
		}
		// if our model is set up to break and we aren't on fire or electrocuted, break
		else if ( CanBreak() && RandomFloat() < asw_alien_break_chance.GetFloat() && !m_bElectroStunned && !m_bOnFire )
		{
			m_nDeathStyle = kDIE_BREAKABLE;
			//BreakAlien( info );
		}
		// if we are set to never ragdoll or if we are electrified, but dont have anims or decided not to do them, instagib
		else if ( m_bNeverRagdoll || m_bElectroStunned )
		{
			m_nDeathStyle = kDIE_INSTAGIB;
		}
	}

	if (!ShouldGib(info))
	{
		const unsigned int nDamageTypesThatCauseHurling = DMG_BLAST | DMG_BLAST_SURFACE;
		SetCollisionGroup(ASW_COLLISION_GROUP_PASSABLE);	// don't block marines by dead bodies
		if ( (info.GetDamageType()	&  nDamageTypesThatCauseHurling)							&&
			  gpGlobals->curtime	>  sm_flLastHurlTime + asw_drone_hurl_interval.GetFloat()	&&
			  random->RandomFloat()	<= asw_drone_hurl_chance.GetFloat() )
		{
			m_nDeathStyle = kDIE_HURL;
			sm_flLastHurlTime = gpGlobals->curtime;
		}
		if ( ( info.GetDamageType() & DMG_CLUB ) && info.GetAttacker() && info.GetAttacker()->Classify() == CLASS_ASW_MARINE )
		{
			CASW_Marine *pMarine = assert_cast<CASW_Marine*>( info.GetAttacker() );

			if ( pMarine->HasPowerFist() && RandomFloat() < asw_fist_ragdoll_chance.GetFloat() )
			{
				m_nDeathStyle = kDIE_MELEE_THROW;
			}
		}
	}
	
	DropMoney( info );
	if ( ASWGameRules() )
		ASWGameRules()->DropPowerup( this, info, GetClassname() );

	if ( asw_alien_debug_death_style.GetBool() )
		Msg( "'%s' CASW_Alien::Event_Killed: m_nDeathStyle = %d\n", GetClassname(), m_nDeathStyle );

	BaseClass::Event_Killed(info);
}

void CASW_Alien::SetSpawner(CASW_Base_Spawner* spawner)
{
	m_hSpawner = spawner;
}

void CASW_Alien::InputBreakWaitForScript(inputdata_t &inputdata)
{
	if (IsCurSchedule( SCHED_WAIT_FOR_SCRIPT ))
	{
		SetSchedule(SCHED_IDLE_STAND);
	}
}

// set orders for our alien
//   select schedule should activate the appropriate orders
void CASW_Alien::SetAlienOrders(AlienOrder_t Orders, Vector vecOrderSpot, CBaseEntity* pOrderObject)
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

	//Msg("Drone recieved orders\n");
}

AlienOrder_t CASW_Alien::GetAlienOrders()
{
	return m_AlienOrders;
}

void CASW_Alien::ClearAlienOrders()
{
	//Msg("Drone orders cleared\n");
	m_AlienOrders = AOT_None;
	m_vecAlienOrderSpot = vec3_origin;
	m_AlienOrderObject = NULL;
	m_bIgnoreMarines = false;
	m_bFailedMoveTo = false;
}

void CASW_Alien::GatherConditions()
{
	BaseClass::GatherConditions();

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
	{
		ClearAlienOrders();
	}
}

// if we arrive at our destination, clear our orders
void CASW_Alien::OnMovementComplete()
{
	if ( ShouldClearOrdersOnMovementComplete() )
	{
		ClearAlienOrders();
	}

	BaseClass::OnMovementComplete();
}

bool CASW_Alien::ShouldClearOrdersOnMovementComplete()
{
	if (m_AlienOrders != AOT_None)
	{
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
				return false;
			}
		}
	}

	return true;
};

void CASW_Alien::IgnoreMarines(bool bIgnoreMarines)
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

// we're blocking a fellow alien from spawning, let's move a short distance
void CASW_Alien::MoveAside()
{
	if (!GetEnemy() && !IsMoving())
	{
		// random nearby position
		if ( !GetNavigator()->SetWanderGoal( 90, 200 ) )
		{
			if ( !GetNavigator()->SetRandomGoal( 150.0f ) )
			{
				return;	// couldn't find a wander spot
			}
		}
		//SetSchedule(SCHED_IDLE_WALK);
	}
}

void CASW_Alien::ASW_Ignite( float flFlameLifetime, float flSize, CBaseEntity *pAttacker, CBaseEntity *pDamagingWeapon )
{
	if (AllowedToIgnite())
	{
		if( IsOnFire() )
			return;

		AddFlag( FL_ONFIRE );
		m_bOnFire = true;
		if (ASWBurning())
			ASWBurning()->BurnEntity(this, pAttacker, flFlameLifetime, 0.4f, 2.5f * 0.4f, pDamagingWeapon );	// 2.5 dps, applied every 0.4 seconds

		m_OnIgnite.FireOutput( this, this );
	}
}

void CASW_Alien::Extinguish()
{
	m_bOnFire = false;
	if (ASWBurning())
		ASWBurning()->Extinguish(this);
	RemoveFlag( FL_ONFIRE );
}

bool CASW_Alien::IsMeleeAttacking()
{
	return (GetTask() && (GetTask()->iTask == TASK_MELEE_ATTACK1));
}

bool CASW_Alien::Knockback(Vector vecForce)
{
	/*
	Vector end = GetAbsOrigin() + vecForce;	// todo: divide by mass, etc
	trace_t tr;
	UTIL_TraceEntity( this, GetAbsOrigin(), end, MASK_NPCSOLID, &tr );
	if (tr.allsolid)
		return false;

	if( tr.fraction > 0 )
	{	
		SetAbsOrigin(tr.endpos);
		return true;
	}
	return false;*/
	Vector newVel = GetAbsVelocity();
	SetAbsVelocity(newVel + vecForce);
	SetGroundEntity( NULL );	
	return true;
}

void CASW_Alien::UpdateEfficiency( bool bInPVS )
{
	// Sleeping NPCs always dormant
	if ( GetSleepState() != AISS_AWAKE )
	{
		SetEfficiency( AIE_DORMANT );
		return;
	}

	m_bInChoreo = ( GetState() == NPC_STATE_SCRIPT || IsCurSchedule( SCHED_SCENE_GENERIC, false ) );

#ifndef _RETAIL
	if ( !(ai_use_think_optimizations.GetBool() && ai_use_efficiency.GetBool()) )
	{
		SetEfficiency( AIE_NORMAL );
		SetMoveEfficiency( AIME_NORMAL );
		return;
	}
#endif		

	bool bInVisibilityPVS = ( UTIL_FindClientInVisibilityPVS( edict() ) != NULL );

	//if ( bInPVS && MarineNearby(1024) ) 
	if ( bInPVS && MarineCanSee(384, 1.0f) ) 
	{
		SetMoveEfficiency( AIME_NORMAL );
	}
	else
	{
		SetMoveEfficiency( AIME_EFFICIENT );
	}

	//---------------------------------

	if ( !IsRetail() && ai_efficiency_override.GetInt() > AIE_NORMAL && ai_efficiency_override.GetInt() <= AIE_DORMANT )
	{
		SetEfficiency( (AI_Efficiency_t)ai_efficiency_override.GetInt() );
		return;
	}

	// Some conditions will always force normal
	if ( gpGlobals->curtime - GetLastAttackTime() < .15 )
	{
		SetEfficiency( AIE_NORMAL );
		return;
	}

	bool bFramerateOk = ( gpGlobals->frametime < ai_frametime_limit.GetFloat() );

	if ( IsForceGatherConditionsSet() || 
		 gpGlobals->curtime - GetLastAttackTime() < .2 ||
		 gpGlobals->curtime - m_flLastDamageTime < .2 ||
		 ( GetState() < NPC_STATE_IDLE || GetState() > NPC_STATE_SCRIPT ) ||
		 ( ( bInPVS || bInVisibilityPVS ) && 
		   ( ( GetTask() && !TaskIsRunning() ) ||
			 GetTaskInterrupt() > 0 ||
			 m_bInChoreo ) ) )
	{
		SetEfficiency( ( bFramerateOk ) ? AIE_NORMAL : AIE_EFFICIENT );
		return;
	}

	SetEfficiency( ( bFramerateOk ) ? AIE_EFFICIENT : AIE_VERY_EFFICIENT );
}

void CASW_Alien::OnRestore()
{
	BaseClass::OnRestore();
	m_LagCompensation.Init(this);
}

// checks if a marine can see us
//  caches the results and won't recheck unless the specified interval has passed since the last check
bool CASW_Alien::MarineCanSee(int padding, float interval)
{
	if (gpGlobals->curtime >= m_fLastMarineCanSeeTime + interval)
	{
		bool bCorpseCanSee = false;
		m_bLastMarineCanSee = (UTIL_ASW_AnyMarineCanSee(GetAbsOrigin(), padding, bCorpseCanSee) != NULL) || bCorpseCanSee;
		m_fLastMarineCanSeeTime = gpGlobals->curtime;
	}
	return m_bLastMarineCanSee;
}

void CASW_Alien::SetHealthByDifficultyLevel()
{
	// filled in by subclasses
}

void CASW_Alien::Ignite( float flFlameLifetime, bool bNPCOnly, float flSize, bool bCalledByLevelDesigner )
{
	return;	// use ASW_Ignite instead
}

bool CASW_Alien::ShouldMoveSlow() const
{
	return (gpGlobals->curtime < m_fHurtSlowMoveTime);
}

bool CASW_Alien::ModifyAutoMovement( Vector &vecNewPos )
{
	// melee auto movement on the drones seems way too fast
	float fFactor = 1.0f;
	if ( ShouldMoveSlow() )
	{
		if ( m_bElectroStunned.Get() )
		{
			fFactor *= asw_alien_stunned_speed.GetFloat() * 0.1f;
		}
		else
		{
			fFactor *= asw_alien_hurt_speed.GetFloat() * 0.1f;
		}
		Vector vecRelPos = vecNewPos - GetAbsOrigin();
		vecRelPos *= fFactor;
		vecNewPos = GetAbsOrigin() + vecRelPos;
		return true;
	}
	return false;
}

float CASW_Alien::GetIdealSpeed() const
{
	// if the alien is hurt, move slower	
	if ( ShouldMoveSlow() )
	{
		if ( m_bElectroStunned.Get() )
		{
			return BaseClass::GetIdealSpeed() * asw_alien_stunned_speed.GetFloat();
		}
		else
		{
			return BaseClass::GetIdealSpeed() * asw_alien_hurt_speed.GetFloat();
		}
	}

	return BaseClass::GetIdealSpeed();
}

void CASW_Alien::DropMoney( const CTakeDamageInfo &info )
{
	if ( !asw_drop_money.GetBool() || !asw_money.GetBool() )
		return;

	int iMoneyCount = GetMoneyCount( info );
	for (int i=0;i<iMoneyCount;i++)
	{
		CASW_Pickup_Money* pMoney = (CASW_Pickup_Money *)CreateEntityByName( "asw_pickup_money" );		
		UTIL_SetOrigin( pMoney, WorldSpaceCenter() );
		pMoney->Spawn();
		Vector vel = -info.GetDamageForce();
		vel.NormalizeInPlace();
		vel *= RandomFloat( 30, 50 );
		vel += RandomVector( -20, 20 );
		vel.z = RandomFloat( 30, 50 );
		if ( iMoneyCount >= 3 )	// if we're dropping a lot of money, make it fly up in the air more
		{
			vel.z += RandomFloat( 60, 80 );
		}
		pMoney->SetAbsVelocity( vel );
	}
}

int CASW_Alien::GetMoneyCount( const CTakeDamageInfo &info )
{
	if ( RandomFloat() < asw_alien_money_chance.GetFloat() )
	{
		return 1;
	}
	return 0;
}

void CASW_Alien::ElectroStun( float flStunTime )
{
	if (m_fHurtSlowMoveTime < gpGlobals->curtime + flStunTime)
		m_fHurtSlowMoveTime = gpGlobals->curtime + flStunTime;

	m_bElectroStunned = true;

	if ( ASWGameResource() )
	{
		ASWGameResource()->m_iElectroStunnedAliens++;
	}

	// can't jump after being elecrostunned
	CapabilitiesRemove( bits_CAP_MOVE_JUMP );
}

void CASW_Alien::ForceFlinch( const Vector &vecSrc )
{	
	SetCondition( COND_HEAVY_DAMAGE );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CASW_AlienVolley *CASW_Alien::GetVolley( const char *pszVolleyName )
{
	for( int nIndex = 0; nIndex < m_volleys.Count(); nIndex++ )
	{
		if ( m_volleys[ nIndex ].m_strName == pszVolleyName )
		{
			return &m_volleys[ nIndex ];
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CASW_Alien::GetVolleyIndex( const char *pszVolleyName )
{
	for( int nIndex = 0; nIndex < m_volleys.Count(); nIndex++ )
	{
		if ( m_volleys[ nIndex ].m_strName == pszVolleyName )
		{
			return nIndex;
		}
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CASW_AlienShot *CASW_Alien::GetShot( const char *pszShotName )
{
	for( int nIndex = 0; nIndex < m_shots.Count(); nIndex++ )
	{
		if ( m_shots[ nIndex ].m_strName == pszShotName )
		{
			return &m_shots[ nIndex ];
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CASW_Alien::GetShotIndex( const char *pszShotName )
{
	for( int nIndex = 0; nIndex < m_shots.Count(); nIndex++ )
	{
		if ( m_shots[ nIndex ].m_strName == pszShotName )
		{
			return nIndex;
		}
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Alien::CreateShot( const char *pszShotName, const CASW_AlienShot *pShot )
{
	CASW_AlienShot *pNewShot = GetShot( pszShotName );
	if ( !pNewShot )
	{
		int nIndex = m_shots.AddToTail();
		pNewShot = &m_shots[ nIndex ];
	}

	*pNewShot = *pShot;
	pNewShot->m_strName = pszShotName;

	// Precache resources
	if ( !pShot->m_strModel.IsEmpty() )
		PrecacheModel( pShot->m_strModel );

	if ( !pShot->m_strSound_spawn.IsEmpty() )
		PrecacheScriptSound( pShot->m_strSound_spawn );

	if ( !pShot->m_strSound_hitNPC.IsEmpty() )
		PrecacheScriptSound( pShot->m_strSound_hitNPC );

	if ( !pShot->m_strSound_hitWorld.IsEmpty() )
		PrecacheScriptSound( pShot->m_strSound_hitWorld );

	if ( !pShot->m_strParticles_trail.IsEmpty() )
		PrecacheParticleSystem( pShot->m_strParticles_trail );

	if ( !pShot->m_strParticles_hit.IsEmpty() )
		PrecacheParticleSystem( pShot->m_strParticles_hit );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Alien::CreateVolley( const char *pszVolleyName, const CASW_AlienVolley *pVolley )
{
	CASW_AlienVolley *pNewVolley = GetVolley( pszVolleyName );
	if ( !pNewVolley )
	{
		int nIndex = m_volleys.AddToTail();
		pNewVolley = &m_volleys[ nIndex ];
	}

	*pNewVolley = *pVolley;
	pNewVolley->m_strName = pszVolleyName;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Alien::CalcMissileVelocity( const Vector &vTargetPos, Vector &vVelocity, float flSpeed, float flAngle )
{
	float flRadians = DEG2RAD( flAngle );
	Vector vForward = ( vTargetPos - GetAbsOrigin() );
	vForward.NormalizeInPlace();
	Vector vRight = vForward.Cross( Vector( 0, 0, 1 ) );
	Vector vDir = vForward * cos( flRadians ) + vRight * sin( flRadians );

	vVelocity = vDir * flSpeed;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Alien::UpdateRangedAttack() 
{
	if ( m_nVolleyType < 0 )
		return;

	Vector vecSpawn = GetAbsOrigin() + Vector( 0.0f, 0.0f, 15.0f );
	QAngle vecAngles = QAngle( 0.0f, 0.0f, 0.0f );
	int iAttachment = LookupAttachment( "mouth" );
	if ( iAttachment > 0 )
		GetAttachment( iAttachment, vecSpawn, vecAngles );	

	Vector vForward = ( m_vecRangeAttackTargetPosition- GetAbsOrigin() );
	vForward.NormalizeInPlace();
	Vector vRight = vForward.Cross( Vector( 0, 0, 1 ) );

	int nNumFinished = 0;
	CASW_AlienVolley *pVolley = &m_volleys[ m_nVolleyType ];
	for( int nRound = 0; nRound < pVolley->m_rounds.Count(); nRound++ )
	{
		CASW_AlienVolleyRound &round = pVolley->m_rounds[ nRound ];
		if ( round.m_nShot_type < 0 )
		{
			nNumFinished++;
			continue;
		}

		// has round started yet?
		if ( m_flRangeAttackStartTime + round.m_flTime > gpGlobals->curtime )
			continue;

		// is round finished?
		float flEndTime = m_flRangeAttackStartTime + round.m_flTime + round.m_nNumShots * round.m_flShotDelay;
		if ( ( flEndTime < gpGlobals->curtime ) && ( flEndTime < m_flRangeAttackLastUpdateTime ) )
		{
			nNumFinished++;
			continue;
		}

		for( int i = 0; i < round.m_nNumShots; i++ )
		{
			float flShotTime = m_flRangeAttackStartTime + round.m_flTime + i * round.m_flShotDelay;
			if ( ( flShotTime > m_flRangeAttackLastUpdateTime ) && ( flShotTime <= gpGlobals->curtime ) )
			{
				Vector vVelocity;
				CASW_AlienShot &shot = m_shots[ round.m_nShot_type ];
				float flPercent = ( round.m_nNumShots > 1 ) ? ( ( float )i / ( float )( round.m_nNumShots - 1 ) ) : 0.0f;
				float flRadians = DEG2RAD( Lerp( flPercent, round.m_flStartAngle, round.m_flEndAngle ) );
				Vector vDir = vForward * cos( flRadians ) + vRight * sin( flRadians );
				vVelocity = vDir * round.m_flSpeed;
				Vector vStart = vecSpawn + vRight * round.m_flHorizontalOffset;
				CASW_Missile_Round::Missile_Round_Create( shot, vStart, vecAngles, vVelocity, this );
			}
		}
	}

	if ( nNumFinished >= pVolley->m_rounds.Count() )
	{
		m_nVolleyType = -1;
		return;
	}

	m_flRangeAttackLastUpdateTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Alien::LaunchMissile( const char *pszVolleyName, Vector &vTargetPosition )
{
	m_nVolleyType = GetVolleyIndex( pszVolleyName );
	if ( m_nVolleyType < 0 )
	{
		//FIXME: add warning
		return;
	}

	m_flRangeAttackStartTime = gpGlobals->curtime;
	m_flRangeAttackLastUpdateTime = -1.0f;
	m_vecRangeAttackTargetPosition = vTargetPosition;
	UpdateRangedAttack();
}
void CASW_Alien::AddBehaviorParam( const char *pszParmName, int nDefaultValue )
{
	CUtlSymbol	ParmName = CAI_ASW_Behavior::GetSymbol( pszParmName );

	m_BehaviorParms.Insert( ParmName, nDefaultValue );
}


int CASW_Alien::GetBehaviorParam( CUtlSymbol ParmName )
{
	int nIndex = m_BehaviorParms.Find( ParmName );

	if ( m_BehaviorParms.IsValidIndex( nIndex ) == true )
	{
		return m_BehaviorParms.Element( nIndex );
	}

	return 0;
}


void CASW_Alien::SetBehaviorParam( CUtlSymbol ParmName, int nValue )
{
	if ( ParmName.IsValid() == false )
	{
		return;
	}

	int nIndex = m_BehaviorParms.Find( ParmName );
	if ( m_BehaviorParms.IsValidIndex( nIndex ) == true )
	{
		if ( m_BehaviorParms.Element( nIndex ) != nValue )
		{
			m_BehaviorParms.Element( nIndex ) = nValue;
			m_bBehaviorParameterChanged = true;
		}
	}
	else
	{
		Assert( 0 );
	}
}


void CASW_Alien::OnChangeRunningBehavior( CAI_BehaviorBase *pOldBehavior, CAI_BehaviorBase *pNewBehavior )
{
	BaseClass::OnChangeRunningBehavior( pOldBehavior, pNewBehavior );

	m_pPreviousBehavior = m_pPreviousBehavior;
}

void CASW_Alien::SendBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t Event, int nParm, bool bToAllBehaviors )
{
	if ( bToAllBehaviors )
	{
		for ( int i = 0; i < m_Behaviors.Count(); i++)
		{
			CAI_ASW_Behavior	*pBehavior = static_cast < CAI_ASW_Behavior * >(  m_Behaviors[ i ] );

			pBehavior->HandleBehaviorEvent( pInflictor, Event, nParm );
		}
	}
	else
	{
		CAI_ASW_Behavior	*pBehavior = GetPrimaryASWBehavior();
		if ( pBehavior == NULL )
		{	// we have probably died, try sending this to the last behavior
			pBehavior = static_cast < CAI_ASW_Behavior * >( m_pPreviousBehavior );
		}

		if ( pBehavior != NULL )
		{
			pBehavior->HandleBehaviorEvent( pInflictor, Event, nParm );
		}
		else
		{
			// some how the boomer is coming back with having no primary behavior, which I don't understand.
			// if you see this assert fire, please let RJ know.
			Assert( 0 ); 
		}
	}
}
void CASW_Alien::BuildScheduleTestBits()
{
	CAI_ASW_CombatStunBehavior *pBehavior = NULL;

	// Check for combat stun behavior, or, if we're not using behaviors, we'll fall back to the combat stun schedule in CASW_Alien
	if ( GetBehavior( &pBehavior ) && pBehavior != m_pPrimaryBehavior )
	{
		SetCustomInterruptCondition( COND_ASW_BEGIN_COMBAT_STUN );
	}

	SetCustomInterruptCondition( COND_BEHAVIOR_PARAMETERS_CHANGED );

	if ( m_pFlinchBehavior )
	{
		SetCustomInterruptCondition( COND_ASW_FLINCH );
	}	

	BaseClass::BuildScheduleTestBits();
}
void CASW_Alien::StartTouch( CBaseEntity *pOther )
{
	BaseClass::StartTouch(pOther);

	CAI_ASW_Behavior *pCurrent = GetPrimaryASWBehavior();
	if ( pCurrent )
	{
		pCurrent->StartTouch( pOther );
	}
}
CAI_ASW_Behavior* CASW_Alien::GetPrimaryASWBehavior()
{
	return assert_cast<CAI_ASW_Behavior*>( GetPrimaryBehavior() );
}

void CASW_Alien::HandleAnimEvent( animevent_t *pEvent )
{

	int nEvent = pEvent->Event();
	CTakeDamageInfo info;
	if ( nEvent == AE_NPC_RAGDOLL )
	{
		m_bTimeToRagdoll = true;
		// anim event is telling us to ragdoll.  If we aren't on fire, break instead
		if ( m_nDeathStyle == kDIE_FANCY && !m_bOnFire )
		{   
			m_nDeathStyle = kDIE_BREAKABLE;
			//BreakAlien( info );
		}
		//BecomeRagdollOnClient(GetAbsOrigin());
		//return;
	}

	CAI_ASW_Behavior	*pBehavior = GetPrimaryASWBehavior();
	if ( pBehavior && pBehavior->BehaviorHandleAnimEvent( pEvent ) )
		return;

	BaseClass::HandleAnimEvent( pEvent );
}

int	CASW_Alien::DrawDebugTextOverlays()
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT)
	{
		NDebugOverlay::EntityText( entindex(),text_offset,CFmtStr( "Freeze amt.: %f", m_flFrozen.Get() ),0 );
		text_offset++;
		NDebugOverlay::EntityText( entindex(),text_offset,CFmtStr( "Freeze time: %f", m_flFrozenTime - gpGlobals->curtime ),0 );
		text_offset++;
	}
	return text_offset;
}

void CASW_Alien::InputSetMoveClone( inputdata_t &inputdata )
{
	SetMoveClone( inputdata.value.StringID(), inputdata.pActivator );
}

void CASW_Alien::SetMoveClone( string_t EntityName, CBaseEntity *pActivator )
{
	CBaseEntity *pEnt = gEntList.FindEntityByName( NULL, EntityName, NULL, pActivator );

	if ( EntityName != NULL_STRING && pEnt == NULL )
	{
		Msg( "Entity %s(%s) has bad move clone %s\n", STRING(m_iClassname), GetDebugName(), STRING(EntityName) );
	}
	else
	{
		// make sure there isn't any ambiguity
		if ( gEntList.FindEntityByName( pEnt, EntityName, NULL, pActivator ) )
		{
			Msg( "Entity %s(%s) is ambiguously move cloned to %s, because there is more than one entity by that name.\n", STRING(m_iClassname), GetDebugName(), STRING(EntityName) );
		}
		SetMoveClone( pEnt );
	}
}

void CASW_Alien::SetMoveClone( CBaseEntity *pEnt )
{
	m_hMoveClone = pEnt;

	if ( pEnt )
	{
		matrix3x4_t entityToWorld = EntityToWorldTransform();
		matrix3x4_t otherToWorld = pEnt->EntityToWorldTransform();

		matrix3x4_t temp;
		MatrixInvert( otherToWorld, temp );
		ConcatTransforms( temp, entityToWorld, m_moveCloneOffset );
	}
}

bool CASW_Alien::OverrideMove( float flInterval )
{
	if ( m_hMoveClone.Get() )
	{
		matrix3x4_t otherToWorld = m_hMoveClone->EntityToWorldTransform();
		matrix3x4_t temp;
		ConcatTransforms( otherToWorld, m_moveCloneOffset, temp );
		SetLocalTransform( temp );
		return true;
	}
	return false;
}

void CASW_Alien::ClearBurrowPoint( const Vector &origin )
{
	CBaseEntity *pEntity = NULL;
	float		flDist;
	Vector		vecSpot, vecCenter, vecForce;

	//Cause a ruckus
	//UTIL_ScreenShake( origin, 1.0f, 80.0f, 1.0f, 256.0f, SHAKE_START );

	//Iterate on all entities in the vicinity.
	for ( CEntitySphereQuery sphere( origin, 128 ); ( pEntity = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		//if ( pEntity->m_takedamage != DAMAGE_NO && pEntity->Classify() != CLASS_PLAYER && pEntity->VPhysicsGetObject() )
		if ( pEntity->Classify() != CLASS_PLAYER && pEntity->VPhysicsGetObject() )
		{
			vecSpot	 = pEntity->BodyTarget( origin );
			vecForce = ( vecSpot - origin ) + Vector( 0, 0, 16 );

			// decrease damage for an ent that's farther from the bomb.
			flDist = VectorNormalize( vecForce );

			//float mass = pEntity->VPhysicsGetObject()->GetMass();
			CollisionProp()->RandomPointInBounds( vec3_origin, Vector( 1.0f, 1.0f, 1.0f ), &vecCenter );

			if ( flDist <= 128.0f )
			{
				pEntity->VPhysicsGetObject()->Wake();
				pEntity->VPhysicsGetObject()->ApplyForceOffset( vecForce * 250.0f, vecCenter );
			}
		}
	}
}

void CASW_Alien::Unburrow( void )
{
	m_bStartBurrowed = false;

	ClearBurrowPoint( m_vecUnburrowEndPoint );	// physics blast anything in the way out of the way

	//Become solid again and visible
	m_spawnflags &= ~SF_NPC_GAG;	
	RemoveSolidFlags( FSOLID_NOT_SOLID );
	m_takedamage	= DAMAGE_YES;

	RemoveEffects( EF_NODRAW );
	RemoveFlag( FL_NOTARGET );

	SetIdealActivity( m_UnburrowActivity );

	//If we have an enemy, come out facing them
	/*
	if ( GetEnemy() )
	{
	Vector	dir = GetEnemy()->GetAbsOrigin() - GetAbsOrigin();
	VectorNormalize(dir);

	QAngle angles = GetAbsAngles();
	angles[ YAW ] = UTIL_VecToYaw( dir );
	SetLocalAngles( angles );
	}*/
}

void CASW_Alien::SetUnburrowActivity( string_t iszActivityName )
{
	m_iszUnburrowActivityName = iszActivityName;
}

void CASW_Alien::SetUnburrowIdleActivity( string_t iszActivityName )
{
	m_iszUnburrowIdleActivityName = iszActivityName;
}

void CASW_Alien::LookupBurrowActivities()
{
	if ( m_iszUnburrowActivityName == NULL_STRING )
	{
		m_UnburrowActivity = (Activity) ACT_BURROW_OUT;
	}
	else
	{
		m_UnburrowActivity = (Activity) LookupActivity( STRING( m_iszUnburrowActivityName ) );
		if ( m_UnburrowActivity == ACT_INVALID )
		{
			Warning( "Unknown unburrow activity %s", STRING( m_iszUnburrowActivityName ) );
			if ( m_hSpawner.Get() )
			{
				Warning( "  Spawner is: %d %s at %f %f %f\n", m_hSpawner->entindex(), m_hSpawner->GetDebugName(), VectorExpand( m_hSpawner->GetAbsOrigin() ) );
			}
			m_UnburrowActivity = (Activity) ACT_BURROW_OUT;
		}
	}

	if ( m_iszUnburrowIdleActivityName == NULL_STRING )
	{
		m_UnburrowIdleActivity = (Activity) ACT_BURROW_IDLE;
	}
	else
	{
		m_UnburrowIdleActivity = (Activity) LookupActivity( STRING( m_iszUnburrowIdleActivityName ) );
		if ( m_UnburrowActivity == ACT_INVALID )
		{
			Warning( "Unknown unburrow idle activity %s", STRING( m_iszUnburrowIdleActivityName ) );
			if ( m_hSpawner.Get() )
			{
				Warning( "  Spawner is: %d %s at %f %f %f\n", m_hSpawner->entindex(), m_hSpawner->GetDebugName(), VectorExpand( m_hSpawner->GetAbsOrigin() ) );
			}
			m_UnburrowIdleActivity = (Activity) ACT_BURROW_IDLE;
		}
	}
}

class CASW_Trace_Filter_Disable_Collision_With_Traps : public CTraceFilterEntitiesOnly
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS_NOBASE( CASW_Trace_Filter_Disable_Collision_With_Traps );

	CASW_Trace_Filter_Disable_Collision_With_Traps( IHandleEntity *passentity, int collisionGroup )
		: m_pPassEnt(passentity), m_collisionGroup(collisionGroup)
	{		
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		if ( !StandardFilterRules( pHandleEntity, contentsMask ) )
			return false;

		// Don't test if the game code tells us we should ignore this collision...
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		CBaseEntity *pEntPass = EntityFromEntityHandle( m_pPassEnt );

		// don't hurt ourself
		if ( pEntPass == pEntity )
			return false;

		Class_T entClass = pEntity->Classify();

		if ( entClass == CLASS_ASW_SENTRY_BASE ||
			entClass == CLASS_ASW_TESLA_TRAP 
			)
		{
			PhysDisableEntityCollisions( pEntPass, pEntity );
		}

		return false;	// keep iterating over entities in the trace ray
	}

public:
	IHandleEntity		*m_pPassEnt;
	int					m_collisionGroup;
};

// if the alien is stuck inside any player set traps, this will disable collision between them
void CASW_Alien::CheckForBlockingTraps()
{
	Ray_t ray;
	trace_t tr;
	ray.Init( GetAbsOrigin() + Vector( 0, 0, 1 ), GetAbsOrigin(), GetHullMins(), GetHullMaxs() );

	CASW_Trace_Filter_Disable_Collision_With_Traps traceFilter( this, GetCollisionGroup() );
	enginetrace->TraceRay( ray, PhysicsSolidMaskForEntity(), &traceFilter, &tr );
}

void CASW_Alien::SetDefaultEyeOffset()
{	
	m_vDefaultEyeOffset = vec3_origin;
	m_vDefaultEyeOffset.z = ( WorldAlignMins().z + WorldAlignMaxs().z ) * 0.75f;

	SetViewOffset( m_vDefaultEyeOffset );
}