//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

// cs_simple_hostage.cpp
// Simple CS1.6 level hostage
// Author: Michael S. Booth and Matt Boone, July 2004

#include "cbase.h"
#include "cs_simple_hostage.h"
#include "cs_player.h"
#include "cs_gamerules.h"
#include "game.h"
#include "bot.h"
#include <KeyValues.h>
#include "obstacle_pushaway.h"
#include "props_shared.h"

//=============================================================================
// HPE_BEGIN
//=============================================================================
// [dwenger] Necessary for stats tracking
#include "cs_gamestats.h"

//[tj] Necessary for fast rescue achievement
#include "cs_achievement_constants.h"
//=============================================================================
// HPE_END
//=============================================================================

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define HOSTAGE_THINK_INTERVAL	0.1f

#define DrawLine( from, to, duration, red, green, blue )		NDebugOverlay::Line( from, to, red, green, blue, true, 0.1f )
#define HOSTAGE_PUSHAWAY_THINK_CONTEXT	"HostagePushawayThink"

#define HOSTAGE_BBOX_VEC_MIN	Vector( -13, -13, 0 )
#define HOSTAGE_BBOX_VEC_MAX	Vector( 13, 13, 72 )


ConVar mp_hostagepenalty( "mp_hostagepenalty", "13", FCVAR_NOTIFY, "Terrorist are kicked for killing too much hostages" );
ConVar hostage_debug( "hostage_debug", "0", FCVAR_CHEAT, "Show hostage AI debug information" );

extern ConVar sv_pushaway_force;
extern ConVar sv_pushaway_min_player_speed;
extern ConVar sv_pushaway_max_force;

// We need hostage-specific pushaway cvars because the hostage doesn't have the same friction etc as players
ConVar sv_pushaway_hostage_force( "sv_pushaway_hostage_force", "20000", FCVAR_REPLICATED | FCVAR_CHEAT, "How hard the hostage is pushed away from physics objects (falls off with inverse square of distance)." );
ConVar sv_pushaway_max_hostage_force( "sv_pushaway_max_hostage_force", "1000", FCVAR_REPLICATED | FCVAR_CHEAT, "Maximum of how hard the hostage is pushed away from physics objects." );

const int NumHostageModels = 4;
static const char *HostageModel[NumHostageModels] = 
{
	"models/Characters/Hostage_01.mdl",
	"models/Characters/Hostage_02.mdl",
	"models/Characters/hostage_03.mdl",
	"models/Characters/hostage_04.mdl",
};

Vector DropToGround( CBaseEntity *pMainEnt, const Vector &vPos, const Vector &vMins, const Vector &vMaxs );

//-----------------------------------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( hostage_entity, CHostage );


//-----------------------------------------------------------------------------------------------------
BEGIN_DATADESC( CHostage )

	DEFINE_INPUTFUNC( FIELD_VOID, "OnRescueZoneTouch", HostageRescueZoneTouch ),

	DEFINE_USEFUNC( HostageUse ), 
	DEFINE_THINKFUNC( HostageThink ),

END_DATADESC()


//-----------------------------------------------------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST( CHostage, DT_CHostage )
	SendPropExclude( "DT_BaseAnimating", "m_flPoseParameter" ),
	SendPropExclude( "DT_BaseAnimating", "m_flPlaybackRate" ),	
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
	SendPropExclude( "DT_BaseAnimating", "m_nNewSequenceParity" ),
	SendPropExclude( "DT_BaseAnimating", "m_nResetEventsParity" ),
	SendPropExclude( "DT_BaseAnimatingOverlay", "overlay_vars" ),
	
	// cs_playeranimstate and clientside animation takes care of these on the client
	SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),	
	SendPropExclude( "DT_AnimTimeMustBeFirst" , "m_flAnimTime" ),

	SendPropBool( SENDINFO(m_isRescued) ),
	SendPropInt( SENDINFO(m_iHealth), 10 ),
	SendPropInt( SENDINFO(m_iMaxHealth), 10 ),
	SendPropInt( SENDINFO(m_lifeState), 3, SPROP_UNSIGNED ),

	SendPropEHandle( SENDINFO(m_leader) ),

END_SEND_TABLE()


//-----------------------------------------------------------------------------------------------------
CUtlVector< CHostage * > g_Hostages;
static CountdownTimer announceTimer;		// used to stop "hostage rescued" announcements from stepping on each other


//-----------------------------------------------------------------------------------------------------
CHostage::CHostage()
{
	g_Hostages.AddToTail( this );
	m_PlayerAnimState = CreateHostageAnimState( this, this, LEGANIM_8WAY, false );
	UseClientSideAnimation();
	SetBloodColor( BLOOD_COLOR_RED );
}

//-----------------------------------------------------------------------------------------------------
CHostage::~CHostage()
{
	g_Hostages.FindAndRemove( this );
	m_PlayerAnimState->Release();
}

CWeaponCSBase* CHostage::CSAnim_GetActiveWeapon()
{
	return NULL;
}

bool CHostage::CSAnim_CanMove()
{
	return true;
}

//-----------------------------------------------------------------------------------------------------
void CHostage::Spawn( void )
{
	Precache();

	// round-robin through the hostage models
	static int index = 0;
	int whichModel = index % NumHostageModels;
	++index;

	SetModel( HostageModel[ whichModel ] );


	SetHullType( HULL_HUMAN );

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	SetCollisionGroup( COLLISION_GROUP_PLAYER );

	SetGravity( 1.0 );

	m_iHealth = 100;	
	m_iMaxHealth = m_iHealth;
	m_takedamage = DAMAGE_YES;

	InitBoneControllers( ); 

	// we must set this, because its zero by default thus putting their eyes in their feet
	SetViewOffset( Vector( 0, 0, 60 ) );


	// set up think callback
	SetNextThink( gpGlobals->curtime + HOSTAGE_THINK_INTERVAL );
	SetThink( &CHostage::HostageThink );

	SetContextThink( &CHostage::PushawayThink, gpGlobals->curtime + PUSHAWAY_THINK_INTERVAL, HOSTAGE_PUSHAWAY_THINK_CONTEXT );

	SetUse( &CHostage::HostageUse );

	m_leader = NULL;
	m_reuseTimer.Invalidate();
	m_hasBeenUsed = false;

	m_isRescued = false;

	m_vel = Vector( 0, 0, 0 );
	m_accel = Vector( 0, 0, 0 );

	m_path.Invalidate();
	m_repathTimer.Invalidate();

	m_pathFollower.Reset();
	m_pathFollower.SetPath( &m_path );
	m_pathFollower.SetImprov( this );

	m_lastKnownArea = NULL;

	// Need to make sure the hostages are on the ground when they spawn
	Vector GroundPos = DropToGround( this, GetAbsOrigin(), HOSTAGE_BBOX_VEC_MIN, HOSTAGE_BBOX_VEC_MAX );
	SetAbsOrigin( GroundPos );

	if (TheNavMesh)
	{
		Vector pos = GetAbsOrigin();
		m_lastKnownArea = TheNavMesh->GetNearestNavArea( pos );
	}

	m_isCrouching = false;
	m_isRunning = true;
	m_jumpTimer.Invalidate();
	m_inhibitObstacleAvoidanceTimer.Invalidate();

	m_isWaitingForLeader = false;
	
	m_isAdjusted = false;

	m_lastLeaderID = 0;

	announceTimer.Invalidate();
	m_disappearTime = 0.0f;
}

//-----------------------------------------------------------------------------------------------------
void CHostage::Precache()
{
	for ( int i=0; i<NumHostageModels; ++i )
	{
		PrecacheModel( HostageModel[i] );
	}

	PrecacheScriptSound( "Hostage.StartFollowCT" );
	PrecacheScriptSound( "Hostage.StopFollowCT" );
	PrecacheScriptSound( "Hostage.Pain" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------------------------------
int CHostage::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	float actualDamage = info.GetDamage();

	// say something
	EmitSound( "Hostage.Pain" );

	CCSPlayer *player = ToCSPlayer( info.GetAttacker() );

	if (player)
	{
        //=============================================================================
        // HPE_BEGIN
        // [dwenger] Track which player injured the hostage
        //=============================================================================

        player->SetInjuredAHostage(true);
		CSGameRules()->HostageInjured();

        //=============================================================================
        // HPE_END
        //=============================================================================


		if ( !( player->m_iDisplayHistoryBits & DHF_HOSTAGE_INJURED ) )
		{
			player->HintMessage( "#Hint_careful_around_hostages", FALSE );
			player->m_iDisplayHistoryBits |= DHF_HOSTAGE_INJURED;
		}

		IGameEvent *event = gameeventmanager->CreateEvent( "hostage_hurt" );
		if ( event )
		{
			event->SetInt( "userid", player->GetUserID() );
			event->SetInt( "hostage", entindex() );
			event->SetInt( "priority", 5 );

			gameeventmanager->FireEvent( event );
		}

		player->AddAccount( -((int)actualDamage * 20)  );
	}

	return BaseClass::OnTakeDamage_Alive( info );
}

//-----------------------------------------------------------------------------------------------------
/**
 * Modify damage the hostage takes by hitgroup
 */
float CHostage::GetModifiedDamage( float flDamage, int nHitGroup )
{
	switch ( nHitGroup )
	{
	case HITGROUP_GENERIC:	flDamage *=	1.75;	break;
	case HITGROUP_HEAD:		flDamage *=	2.5;	break;
	case HITGROUP_CHEST:	flDamage *=	1.5;	break;
	case HITGROUP_STOMACH:	flDamage *=	1.75;	break;
	case HITGROUP_LEFTARM:	flDamage *=	0.75;	break;
	case HITGROUP_RIGHTARM:	flDamage *=	0.75;	break;
	case HITGROUP_LEFTLEG:	flDamage *=	0.6;	break;
	case HITGROUP_RIGHTLEG:	flDamage *=	0.6;	break;
	default:				flDamage *=	1.5;	break;
	} 

	return flDamage;
}

//-----------------------------------------------------------------------------------------------------
void CHostage::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	CTakeDamageInfo scaledInfo = info;
	scaledInfo.SetDamage( GetModifiedDamage( info.GetDamage(), ptr->hitgroup ) );
	BaseClass::TraceAttack( scaledInfo, vecDir, ptr, pAccumulator );
}

//-----------------------------------------------------------------------------------------------------
/**
 * Check for hostage-killer abuse
 */
void CHostage::CheckForHostageAbuse( CCSPlayer *player )
{
	int hostageKillLimit = mp_hostagepenalty.GetInt();

	if (hostageKillLimit > 0)
	{
		player->m_iHostagesKilled++;

		if ( player->m_iHostagesKilled == hostageKillLimit - 1 )
		{
			player->HintMessage( "#Hint_removed_for_next_hostage_killed", TRUE );
		}
		else if ( player->m_iHostagesKilled >= hostageKillLimit )
		{
			Msg( "Kicking client \"%s\" for killing too many hostages\n", player->GetPlayerName() );
			engine->ServerCommand( UTIL_VarArgs( "kickid %d \"For killing too many hostages\"\n", player->GetUserID() ) );
		}
	}
}


//-----------------------------------------------------------------------------------------------------
/**
 * Hostage was killed
 */
void CHostage::Event_Killed( const CTakeDamageInfo &info )
{
	// tell the game logic that we've died
	CSGameRules()->CheckWinConditions();

	//=============================================================================
	// HPE_BEGIN:
	// [tj] Let the game know that a hostage has been killed
	//=============================================================================
	CSGameRules()->HostageKilled();
	//=============================================================================
	// HPE_END
	//=============================================================================

	CCSPlayer *attacker = ToCSPlayer( info.GetAttacker() );

	if (attacker)
	{
		if ( !( attacker->m_iDisplayHistoryBits & DHF_HOSTAGE_KILLED ) )
		{
			attacker->HintMessage( "#Hint_lost_money", FALSE );
			attacker->m_iDisplayHistoryBits |= DHF_HOSTAGE_KILLED;
		}

		// monetary penalty for killing the hostage
		attacker->AddAccount( -( 500 + ((int)info.GetDamage() * 20) ) );

		// check for hostage-killer abuse
		if (attacker->GetTeamNumber() == TEAM_TERRORIST)
		{
			CheckForHostageAbuse( attacker );
		}
	}

	m_lastLeaderID = 0;

	SetUse( NULL );	
	BaseClass::Event_Killed( info );

	IGameEvent *event = gameeventmanager->CreateEvent("hostage_killed");
	if ( event )
	{
		event->SetInt( "userid", (attacker)?attacker->GetUserID():0 );
		event->SetInt( "hostage", entindex() );
		event->SetInt( "priority", 6 );
		gameeventmanager->FireEvent( event );
	}
}


//-----------------------------------------------------------------------------------------------------
/**
 * Invoked when a Hostage touches a Rescue Zone
 */
void CHostage::HostageRescueZoneTouch( inputdata_t &inputdata )
{
	if (!m_isRescued)
	{
		m_isRescued = true;
		m_lastLeaderID = 0;

		SetSolid( SOLID_NONE );
		SetSolidFlags( 0 );

		// start fading out
		m_disappearTime = gpGlobals->curtime + 3.0f;

		SetUse( NULL );
		m_takedamage = DAMAGE_NO;

		// give rescuer a cash bonus
		CCSPlayer *player = GetLeader();
		if (player)
		{
			const int rescuerCashBonus = 1000;
			player->AddAccount( rescuerCashBonus );
		}
		
		Idle();

		// tell the bots someone has rescued a hostage
		IGameEvent *event = gameeventmanager->CreateEvent( "hostage_rescued" );
		if ( event )
		{
			event->SetInt( "userid", player ? player->GetUserID() : (-1) );
			event->SetInt( "hostage", entindex() );
			event->SetInt( "site", 0 ); // TODO add site index
			event->SetInt( "priority", 9 );
			gameeventmanager->FireEvent( event );
		}

		// update game rules
		CSGameRules()->m_iHostagesRescued++;

        //=============================================================================
        // HPE_BEGIN
        // [dwenger] Hostage rescue achievement processing
        //=============================================================================

        // Track last rescuer
        if ( CSGameRules()->m_pLastRescuer == NULL )
        {
            // No rescuer yet, so assign one & set rescuer count to 1
            CSGameRules()->m_pLastRescuer = player;
            CSGameRules()->m_iNumRescuers = 1;
        }
        else
        {
            if ( CSGameRules()->m_pLastRescuer != player )
            {
                // Rescuer changed
                CSGameRules()->m_pLastRescuer = player;
                CSGameRules()->m_iNumRescuers++;
            }
        }

		bool roundIsAlreadyOver = (CSGameRules()->m_iRoundWinStatus != WINNER_NONE);

        //=============================================================================
        // HPE_END
        //=============================================================================

		if (CSGameRules()->CheckWinConditions() == false)
		{
			// this hostage didn't win the round, so announce its rescue to everyone
			if (announceTimer.IsElapsed())
			{
				CSGameRules()->BroadcastSound( "Event.HostageRescued" );
			}

			// avoid having the announcer talk over himself
			announceTimer.Start( 2.0f );
		}
        //=============================================================================
        // HPE_BEGIN
        // [dwenger] Awarding of hostage rescue achievement
        //=============================================================================

        else
        {
            //Check hostage rescue achievements
            if ( CSGameRules()->m_iNumRescuers == 1 && !CSGameRules()->WasHostageKilled())
            {
				//check for unrescued hostages
				bool allHostagesRescued = true;				
				CHostage* hostage = NULL;
				int iNumHostages = g_Hostages.Count();				

				for ( int i = 0 ; i < iNumHostages; i++ )
				{
					hostage = g_Hostages[i];

					if ( hostage->m_iHealth > 0 && !hostage->IsRescued() )
					{
						allHostagesRescued = false;
						break;
					}
				}

				if (allHostagesRescued)
				{
					player->AwardAchievement(CSRescueAllHostagesInARound);

					//[tj] fast version                
					if (gpGlobals->curtime - CSGameRules()->GetRoundStartTime() < AchievementConsts::FastHostageRescue_Time)
					{
						if ( player )
						{
							player->AwardAchievement(CSFastHostageRescue);
						}
					}
				}
            }
			//=============================================================================
			// HPE_BEGIN:
			// [menglish] If this rescue ended the round give an mvp to the rescuer
			//=============================================================================

            if ( player && !roundIsAlreadyOver)
            {
			    player->IncrementNumMVPs( CSMVP_HOSTAGERESCUE );
            }

			//=============================================================================
			// HPE_END
			//=============================================================================
        }

        if ( player )
        {
            CCS_GameStats.Event_HostageRescued( player );
        }

        //=============================================================================
        // HPE_END
        //=============================================================================
	}
}


//-----------------------------------------------------------------------------------------------------
/**
 * In contact with "other"
 */
void CHostage::Touch( CBaseEntity *other )
{
	BaseClass::Touch( other );

	// allow players and other hostages to push me around
	if ( ( other->IsPlayer() && other->GetTeamNumber() == TEAM_CT ) || FClassnameIs( other, "hostage_entity" ) )
	{
		// only push in 2D
		Vector to = GetAbsOrigin() - other->GetAbsOrigin();
		to.z = 0.0f;
		to.NormalizeInPlace();
		
		const float pushForce = 500.0f;
		ApplyForce( pushForce * to );
	}
	else if ( m_inhibitDoorTimer.IsElapsed() &&
		( other->ClassMatches( "func_door*" ) || other->ClassMatches( "prop_door*" ) ) )
	{
		m_inhibitDoorTimer.Start( 3.0f );
		other->Use( this, this, USE_TOGGLE, 0.0f );
	}
}


//-----------------------------------------------------------------------------------------------------
/** 
 * Hostage is stuck - attempt to wiggle out
 */
void CHostage::Wiggle( void )
{
	if (m_wiggleTimer.IsElapsed())
	{
		m_wiggleDirection = (NavRelativeDirType)RandomInt( 0, 3 );
		m_wiggleTimer.Start( RandomFloat( 0.3f, 0.5f ) );
	}

	Vector dir, lat;
	AngleVectors( GetAbsAngles(), &dir, &lat, NULL );

	const float speed = 500.0f;

	switch( m_wiggleDirection )
	{
		case LEFT:
			ApplyForce( speed * lat );
			break;

		case RIGHT:
			ApplyForce( -speed * lat );
			break;

		case FORWARD:
			ApplyForce( speed * dir );
			break;

		case BACKWARD:
			ApplyForce( -speed * dir );
			break;
	}

	const float minStuckJumpTime = 0.25f;
	if (m_pathFollower.GetStuckDuration() > minStuckJumpTime)
	{
		Jump();
	}
}


//-----------------------------------------------------------------------------------------------------
/**
 * Do following behavior
 */
void CHostage::UpdateFollowing( float deltaT )
{
	if ( !IsFollowingSomeone() && m_lastLeaderID != 0 )
	{
		// emit hostage_stops_following event
		IGameEvent *event = gameeventmanager->CreateEvent( "hostage_stops_following" );
		if ( event )
		{
			event->SetInt( "userid", m_lastLeaderID );
			event->SetInt( "hostage", entindex() );
			event->SetInt( "priority", 6 );
			gameeventmanager->FireEvent( event );
		}

		m_lastLeaderID = 0;
	}

	// if we have a leader, follow him
	CCSPlayer *leader = GetLeader();
	if (leader)
	{
		// if leader is dead, stop following him
		if (!leader->IsAlive())
		{
			Idle();
			return;
		}

		// if leader has moved, repath
		if (m_path.IsValid())
		{
			Vector pathError = leader->GetAbsOrigin() - m_path.GetEndpoint();
			
			const float repathRange = 100.0f;
			if (pathError.IsLengthGreaterThan( repathRange ))
			{
				m_path.Invalidate();
			}
		}


		// build a path to our leader
		if (!m_path.IsValid() && m_repathTimer.IsElapsed())
		{
			const float repathInterval = 0.5f;
			m_repathTimer.Start( repathInterval );

			Vector from = GetAbsOrigin();
			Vector to = leader->GetAbsOrigin();
			HostagePathCost pathCost;

			m_path.Compute( from, to, pathCost );
			m_pathFollower.Reset();
		}


		// if our rescuer is too far away, give up
		const float giveUpRange = 2000.0f;
		const float maxPathLength = 4000.0f;
		Vector toLeader = leader->GetAbsOrigin() - GetAbsOrigin();
		if (toLeader.IsLengthGreaterThan( giveUpRange ) || (m_path.IsValid() && m_path.GetLength() > maxPathLength))
		{
			if ( hostage_debug.GetInt() < 2 )
			{
				Idle();
			}
			return;
		}

		
		// don't crowd the leader
		if (m_isWaitingForLeader)
		{
			// we are close to our leader and waiting for him to move
			const float waitRange = 150.0f;
			if (toLeader.IsLengthGreaterThan( waitRange ))
			{
				// leader has moved away - follow him
				m_isWaitingForLeader = false;
			}

			// face the leader
			//FaceTowards( leader->GetAbsOrigin(), deltaT );
		}
		else
		{
			// we are far from our leader, and need to check if we're close enough to wait
			const float nearRange = 125.0f;

			if (toLeader.IsLengthLessThan( nearRange ))
			{
				// we are close to the leader - wait for him to move
				m_isWaitingForLeader = true;
			}
		}

		if (!m_isWaitingForLeader)
		{
			// move along path towards the leader
			m_pathFollower.Update( deltaT, m_inhibitObstacleAvoidanceTimer.IsElapsed() );

			if (hostage_debug.GetBool())
			{
				m_pathFollower.Debug( true );
			}

			if (m_pathFollower.IsStuck())
			{
				Wiggle();
			}

			if (hostage_debug.GetBool())
			{
				m_path.Draw();
			}
		}
	}
}



//-----------------------------------------------------------------------------------------------------
void CHostage::AvoidPhysicsProps( void )
{
	if ( m_lifeState == LIFE_DEAD )
		return;

	CBaseEntity *props[512];
	int nEnts = GetPushawayEnts( this, props, ARRAYSIZE( props ), 0.0f, PARTITION_ENGINE_SOLID_EDICTS );

	for ( int i=0; i < nEnts; i++ )
	{
		// Don't respond to this entity on the client unless it has PHYSICS_MULTIPLAYER_FULL set.
		IMultiplayerPhysics *pInterface = dynamic_cast<IMultiplayerPhysics*>( props[i] );
		if ( pInterface && pInterface->GetMultiplayerPhysicsMode() != PHYSICS_MULTIPLAYER_SOLID )
			continue;

		const float minMass = 10.0f; // minimum mass that can push a player back
		const float maxMass = 30.0f; // cap at a decently large value
		float mass = maxMass;
		if ( pInterface )
		{
			mass = pInterface->GetMass();
		}
		mass = MIN( mass, maxMass );
		mass -= minMass;
		mass = MAX( mass, 0 );
		mass /= (maxMass-minMass); // bring into a 0..1 range

		// Push away from the collision point. The closer our center is to the collision point,
		// the harder we push away.
		Vector vPushAway = (WorldSpaceCenter() - props[i]->WorldSpaceCenter());
		float flDist = VectorNormalize( vPushAway );
		flDist = MAX( flDist, 1 );

		float flForce = sv_pushaway_hostage_force.GetFloat() / flDist * mass;
		flForce = MIN( flForce, sv_pushaway_max_hostage_force.GetFloat() );
		vPushAway *= flForce;

		ApplyForce( vPushAway );
	}

	//
	// Handle step and ledge "step-up" movement here, before m_accel is zero'd
	//
	if ( !m_accel.IsZero() )
	{
		trace_t trace;
		Vector start = GetAbsOrigin();
		Vector forward = m_accel;
		forward.NormalizeInPlace();
		UTIL_TraceEntity( this, start, start + forward, MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER, &trace );
		if ( !trace.startsolid && trace.fraction < 1.0f && trace.plane.normal.z < 0.7f )
		{
			float groundFraction = trace.fraction;
			start.z += StepHeight;
			UTIL_TraceEntity( this, start, start + forward, MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER, &trace );
			if ( !trace.startsolid && trace.fraction > groundFraction )
			{
				SetAbsOrigin( start );
			}
		}
	}
}


//-----------------------------------------------------------------------------------------------------
/**
 * Push physics objects away from the hostage
 */
void CHostage::PushawayThink( void )
{
	PerformObstaclePushaway( this );
	SetNextThink( gpGlobals->curtime + PUSHAWAY_THINK_INTERVAL, HOSTAGE_PUSHAWAY_THINK_CONTEXT );
}

//-----------------------------------------------------------------------------------------------------
/**
 * @TODO imitate player movement:
 * MoveHelperServer()->SetHost( this );
 * this->PlayerRunCommand( &cmd, MoveHelperServer() );
 */
void CHostage::PhysicsSimulate( void )
{
	BaseClass::PhysicsSimulate();

	SetAbsVelocity( m_vel );
}

//-----------------------------------------------------------------------------------------------------
/**
 * Update Hostage behaviors
 */
void CHostage::HostageThink( void )
{
	if (!m_isAdjusted)
	{
		m_isAdjusted = true;

		// HACK - figure out why the default bbox is 6 units too low
		SetCollisionBounds( HOSTAGE_BBOX_VEC_MIN, HOSTAGE_BBOX_VEC_MAX );
	}

	const float deltaT = HOSTAGE_THINK_INTERVAL;
	SetNextThink( gpGlobals->curtime + deltaT );

	// keep track of which Navigation Area we are in (or were in, if we're "off the mesh" right now)
	CNavArea *area = TheNavMesh->GetNavArea( GetAbsOrigin() );
	if (area != NULL && area != m_lastKnownArea)
	{
		// entered a new nav area
		m_lastKnownArea = area;
	}

	// do leader-following behavior, if necessary
	UpdateFollowing( deltaT );

	AvoidPhysicsProps();

	// update hostage velocity in the XY plane
	const float damping = 2.0f;
	m_vel += deltaT * (m_accel - damping * m_vel);

	// leave Z component untouched
	m_vel.z = GetAbsVelocity().z;

	if ( m_accel.IsZero() && m_vel.AsVector2D().IsZero( 1.0f ) )
	{
		m_vel.x = 0.0f;
		m_vel.y = 0.0f;
	}

	m_accel = Vector( 0, 0, 0 );

	// set animation to idle for now
	StudioFrameAdvance();

	int sequence = SelectWeightedSequence( ACT_IDLE );
	if (GetSequence() != sequence)
	{
		SetSequence( sequence );
	}

	m_PlayerAnimState->Update( GetAbsAngles()[YAW], GetAbsAngles()[PITCH] );


	if ( m_disappearTime && m_disappearTime < gpGlobals->curtime )
	{
		// finished fading - remove us completely
		AddEffects( EF_NODRAW );

		SetSolid( SOLID_NONE );
		SetSolidFlags( 0 );
		m_disappearTime = 0.0f;
	}
}

//-----------------------------------------------------------------------------------------------------
bool CHostage::IsFollowingSomeone( void )
{
	return (m_leader.m_Value != NULL);
}

//-----------------------------------------------------------------------------------------------------
bool CHostage::IsFollowing( const CBaseEntity *entity )
{
	return (m_leader.m_Value == entity);
}

//-----------------------------------------------------------------------------------------------------
bool CHostage::IsValid( void ) const
{
	return (m_iHealth > 0 && !IsRescued());
}

//-----------------------------------------------------------------------------------------------------
bool CHostage::IsRescuable( void ) const
{
	return (m_iHealth > 0 && !IsRescued());
}

//-----------------------------------------------------------------------------------------------------
bool CHostage::IsRescued( void ) const
{
	return m_isRescued;
}

//-----------------------------------------------------------------------------------------------------
bool CHostage::IsOnGround( void ) const
{
	return (GetFlags() & FL_ONGROUND);
}

//-----------------------------------------------------------------------------------------------------
/**
 * Return true if hostage can see position
 */
bool CHostage::IsVisible( const Vector &pos, bool testFOV ) const
{
	trace_t result;
	UTIL_TraceLine( EyePosition(), pos, CONTENTS_SOLID, this, COLLISION_GROUP_NONE, &result );
	return (result.fraction >= 1.0f);
}

//-----------------------------------------------------------------------------------------------------
/**
 * Give bonus to CT's for talking to a hostage
 */
void CHostage::GiveCTUseBonus( CCSPlayer *rescuer )
{
	// money to team
	const int teamBonus = 100;
	CSGameRules()->m_iAccountCT += teamBonus;

	// money to rescuer
	const int rescuerBonus = 150;
	rescuer->AddAccount( rescuerBonus );
}

//-----------------------------------------------------------------------------------------------------
/**
 * Stand idle
 */
void CHostage::Idle( void )
{
	m_leader = NULL;
}

//-----------------------------------------------------------------------------------------------------
/**
 * Begin following "leader"
 */
void CHostage::Follow( CCSPlayer *leader )
{
    //=============================================================================
    // HPE_BEGIN
    // [dwenger] Set variable to track whether player is currently rescuing hostages
    //=============================================================================

    if ( leader )
    {
        leader->IncrementNumFollowers();
        leader->SetIsRescuing(true);
    }

    //=============================================================================
    // HPE_END
    //=============================================================================

	m_leader = leader;
	m_isWaitingForLeader = false;
	m_lastLeaderID = (leader) ? leader->GetUserID() : 0;
}


//-----------------------------------------------------------------------------------------------------
/**
 * Return our leader, or NULL
 */
CCSPlayer *CHostage::GetLeader( void ) const
{
	return ToCSPlayer( m_leader.m_Value );
}


//-----------------------------------------------------------------------------------------------------
/**
 * Invoked when a Hostage is "used" by a player
 */
void CHostage::HostageUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Vector to = pActivator->GetAbsOrigin() - GetAbsOrigin();

	// limit use range
	float useRange = 1000.0f;
	if (to.IsLengthGreaterThan( useRange ))
	{
		return;
	}

	// TODO: check line of sight to hostage


	CCSPlayer *user = ToCSPlayer( pActivator );
	if (user == NULL)
	{
		return;
	}

	// only members of the CT team can use hostages (no T's or spectators)
	if (!hostage_debug.GetBool() && user->GetTeamNumber() != TEAM_CT)
	{
		if ( user->GetTeamNumber() == TEAM_TERRORIST )
		{
			if ( !(user->m_iDisplayHistoryBits & DHF_HOSTAGE_CTMOVE) )
			{
				user->m_iDisplayHistoryBits |= DHF_HOSTAGE_CTMOVE;
				user->HintMessage( "#Only_CT_Can_Move_Hostages", false, true );
			}
		}

		return;
	}

	CCSPlayer *leader = GetLeader();
	if( leader && !leader->IsAlive() )
	{
		Idle();
		leader = NULL;
	}

	// throttle how often leader can change
	if (!m_reuseTimer.IsElapsed())
	{
		return;
	}

	// give money bonus to first CT touching this hostage
	if (!m_hasBeenUsed)
	{
		m_hasBeenUsed = true;

		GiveCTUseBonus( user );

		CSGameRules()->HostageTouched();
	}

	// if we are already following the player who used us, stop following
	if (IsFollowing( user ))
	{
		Idle();

		// say something
		EmitSound( "Hostage.StopFollowCT" );
	}
	else
	{
		// if we're already following a CT, ignore new uses
		if (IsFollowingSomeone())
		{
			return;
		}

		// start following
		Follow( user );

		// say something
		EmitSound( "Hostage.StartFollowCT" );

		// emit hostage_follows event
		IGameEvent *event = gameeventmanager->CreateEvent( "hostage_follows" );
		if ( event )
		{
			event->SetInt( "userid", user->GetUserID() );
			event->SetInt( "hostage", entindex() );
			event->SetInt( "priority", 6 );
			gameeventmanager->FireEvent( event );
		}

		if ( !(user->m_iDisplayHistoryBits & DHF_HOSTAGE_USED) )
		{
			user->m_iDisplayHistoryBits |= DHF_HOSTAGE_USED;
			user->HintMessage( "#Hint_lead_hostage_to_rescue_point", false );
		}
	}

	m_reuseTimer.Start( 1.0f );
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Rotate body to face towards "target"
 */
void CHostage::FaceTowards( const Vector &target, float deltaT )
{
	Vector to = target - GetFeet();
	to.z = 0.0f;

	QAngle desiredAngles;
	VectorAngles( to, desiredAngles );

	QAngle angles = GetAbsAngles();

	const float turnSpeed = 250.0f;	
	angles.y = ApproachAngle( desiredAngles.y, angles.y, turnSpeed * deltaT );

	SetAbsAngles( angles );
}

//-----------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------

const Vector &CHostage::GetCentroid( void ) const
{
	static Vector centroid;
	
	centroid = GetFeet();
	centroid.z += HalfHumanHeight;

	return centroid;
}

//-----------------------------------------------------------------------------------------------------
/**
 * Return position of "feet" - point below centroid of improv at feet level
 */
const Vector &CHostage::GetFeet( void ) const
{
	static Vector feet;
	
	feet = GetAbsOrigin();

	return feet;
}

//-----------------------------------------------------------------------------------------------------
const Vector &CHostage::GetEyes( void ) const
{
	static Vector eyes;
	
	eyes = EyePosition();

	return eyes;
}

//-----------------------------------------------------------------------------------------------------
/**
 * Return direction of movement
 */
float CHostage::GetMoveAngle( void ) const
{
	return GetAbsAngles().y;
}

//-----------------------------------------------------------------------------------------------------
CNavArea *CHostage::GetLastKnownArea( void ) const
{
	return m_lastKnownArea;
}

//-----------------------------------------------------------------------------------------------------
/**
 * Find "simple" ground height, treating current nav area as part of the floo
 */
bool CHostage::GetSimpleGroundHeightWithFloor( const Vector &pos, float *height, Vector *normal )
{
	if (TheNavMesh->GetSimpleGroundHeight( pos, height, normal ))
	{
		// our current nav area also serves as a ground polygon
		if (m_lastKnownArea && m_lastKnownArea->IsOverlapping( pos ))
		{
			*height = MAX( (*height), m_lastKnownArea->GetZ( pos ) );
		}

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------------------------------
void CHostage::Crouch( void )
{
	m_isCrouching = true;
}

//-----------------------------------------------------------------------------------------------------
/**
 * un-crouch
 */
void CHostage::StandUp( void )
{
	m_isCrouching = false;
}

//-----------------------------------------------------------------------------------------------------
bool CHostage::IsCrouching( void ) const
{
	return m_isCrouching;
}

//-----------------------------------------------------------------------------------------------------
/**
 * Initiate a jump
 */
void CHostage::Jump( void )
{
	if (m_jumpTimer.IsElapsed() && IsOnGround())
	{
		const float minJumpInterval = 0.5f;
		m_jumpTimer.Start( minJumpInterval );

		Vector vel = GetAbsVelocity();
		vel.z += 200.0f;
		SetAbsVelocity( vel );
	}
}


//-----------------------------------------------------------------------------------------------------
bool CHostage::IsJumping( void ) const
{
	return !m_jumpTimer.IsElapsed();
}

//-----------------------------------------------------------------------------------------------------
/**
 * Set movement speed to running
 */
void CHostage::Run( void )
{
	m_isRunning = true;
}


//-----------------------------------------------------------------------------------------------------
/**
 * Set movement speed to walking
 */
void CHostage::Walk( void )
{
	m_isRunning = false;
}


//-----------------------------------------------------------------------------------------------------
bool CHostage::IsRunning( void ) const
{
	return m_isRunning;
}

//-----------------------------------------------------------------------------------------------------
/**
 * Invoked when a ladder is encountered while following a path
 */
void CHostage::StartLadder( const CNavLadder *ladder, NavTraverseType how, const Vector &approachPos, const Vector &departPos )
{
}

//-----------------------------------------------------------------------------------------------------
/**
 * Traverse given ladder
 */
bool CHostage::TraverseLadder( const CNavLadder *ladder, NavTraverseType how, const Vector &approachPos, const Vector &departPos, float deltaT )
{
	return false;
}

//-----------------------------------------------------------------------------------------------------
bool CHostage::IsUsingLadder( void ) const
{
	return false;
}

//-----------------------------------------------------------------------------------------------------
/**
 * Move Hostage directly toward "pathGoal", causing Hostage to track the current path.
 */
void CHostage::TrackPath( const Vector &pathGoal, float deltaT )
{
	// face in the direction of our motion
	FaceTowards( GetAbsOrigin() + 10.0f * m_vel, deltaT );

	if (GetFlags() & FL_ONGROUND)
	{
		// on the ground - move towards pathGoal
		Vector to = pathGoal - GetFeet();
		to.z = 0.0f;
		to.NormalizeInPlace();

		const float speed = 1000.0f;
		ApplyForce( speed * to );
	}
	else
	{
		// in the air - continue forward motion
		Vector to;
		QAngle angles = GetAbsAngles();
		AngleVectors( angles, &to );

		const float airSpeed = 350.0f;
		ApplyForce( airSpeed * to );
	}
}

//-----------------------------------------------------------------------------------------------------
/**
 * Invoked when an improv reaches its MoveTo goal
 */
void CHostage::OnMoveToSuccess( const Vector &goal )
{
}

//-----------------------------------------------------------------------------------------------------
/**
 * Invoked when an improv fails to reach a MoveTo goal
 */
void CHostage::OnMoveToFailure( const Vector &goal, MoveToFailureType reason )
{
}


unsigned int CHostage::PhysicsSolidMaskForEntity() const
{
	return MASK_PLAYERSOLID;
}

