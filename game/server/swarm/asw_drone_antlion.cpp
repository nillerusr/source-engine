//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose:		Antlion - nasty bug
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "ai_hint.h"
#include "ai_squad.h"
#include "ai_moveprobe.h"
#include "ai_route.h"
#include "npcevent.h"
#include "gib.h"
#include "entitylist.h"
#include "ndebugoverlay.h"
#include "antlion_dust.h"
#include "engine/IEngineSound.h"
#include "globalstate.h"
#include "movevars_shared.h"
#include "te_effect_dispatch.h"
#include "vehicle_base.h"
#include "mapentities.h"
#include "antlion_maker.h"
#include "npc_antlion.h"
#include "decals.h"
#include "asw_drone_antlion.h"
#include "asw_player.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	SWARM_DRONE_MODEL	"models/swarm/drone/Drone.mdl"

// a test ASW drone, based on the antlion



CASW_Drone_Antlion::CASW_Drone_Antlion( void ) : CNPC_Antlion()
{
	
}

LINK_ENTITY_TO_CLASS( asw_drone_antlion, CASW_Drone_Antlion );

BEGIN_DATADESC( CASW_Drone_Antlion )

END_DATADESC()
/*
IMPLEMENT_SERVERCLASS_ST( CASW_Drone_Antlion, DT_ASW_Drone_Antlion )
	SendPropExclude( "DT_BaseAnimating", "m_flPoseParameter" ),
	SendPropExclude( "DT_BaseAnimating", "m_flPlaybackRate" ),	
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
	SendPropExclude( "DT_BaseAnimating", "m_nNewSequenceParity" ),
	SendPropExclude( "DT_BaseAnimating", "m_nResetEventsParity" ),
	SendPropExclude( "DT_BaseEntity", "m_angRotation" ),
	SendPropExclude( "DT_BaseAnimatingOverlay", "overlay_vars" ),
	
	// cs_playeranimstate and clientside animation takes care of these on the client
	SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),	
	SendPropExclude( "DT_AnimTimeMustBeFirst" , "m_flAnimTime" ),
END_SEND_TABLE()
*/

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Drone_Antlion::Spawn( void )
{
	SetModel( SWARM_DRONE_MODEL );
	m_nSkin = 1;
	Precache();

	BaseClass::Spawn();

	SetModel( SWARM_DRONE_MODEL );

	SetHullType(HULL_MEDIUMBIG);
	//SetHullSizeNormal();

	//SetCollisionBounds(Vector(-26,-26,0), Vector(26,26,72));
	//UTIL_SetSize(this, Vector(-26,-26,0), Vector(26,26,72));

	//UseClientSideAnimation();
	m_nSkin = 1;
}


void CASW_Drone_Antlion::Precache( void )
{
	PrecacheModel( SWARM_DRONE_MODEL );

	BaseClass::Precache();
}

// disable pouncing for the moment
int CASW_Drone_Antlion::MeleeAttack2Conditions( float flDot, float flDist )
{
	return 0;
}

// give our aliens 360 degree vision
bool CASW_Drone_Antlion::FInViewCone( const Vector &vecSpot )
{
	return true;
}

// always gib
bool CASW_Drone_Antlion::ShouldGib( const CTakeDamageInfo &info )
{
	return true;
}

float CASW_Drone_Antlion::GetIdealSpeed() const
{
	//return 0;
	//return BaseClass::GetIdealSpeed();
	return 300;
	/*
	float baseSpeed = BaseClass::GetIdealSpeed();
	const CAI_BaseNPC *base = static_cast<CNPC_Antlion*>(this);
	switch ( base->GetActivity() )
	{				
	case ACT_WALK:
		baseSpeed = 150.0f;
		break;

	case ACT_RUN:
		baseSpeed = 300.0f;
		break;
	
	default:
	case ACT_IDLE:
		baseSpeed = 0.0f;
		break;
	}
	
	return baseSpeed * asw_drone_antlion_speedboost.GetFloat();	
*/
}


float CASW_Drone_Antlion::MaxYawSpeed( void )
{
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

float CASW_Drone_Antlion::GetSequenceGroundSpeed( int iSequence )
{
	return BaseClass::GetSequenceGroundSpeed(iSequence);
	/*
	float t = SequenceDuration( iSequence );

	if (t > 0)
	{
		float move_dist = GetSequenceMoveDist( iSequence );
		if (move_dist == 0)
		{
			if (iSequence == 2)
				return 150;
			else if (iSequence == 3)
				return 300;
			else if (iSequence == 6)
				return 300;
			else if (iSequence == 7)
				return 300;
		}
		return move_dist / t;
	}
	else
	{
		return 0;
	}*/
}

// player (and player controlled marines) always avoid drones
bool CASW_Drone_Antlion::ShouldPlayerAvoid( void )
{
	return true;
}

void CASW_Drone_Antlion::NPCThink()
{
	CASW_Player *pPlayer;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex( i ));
		if (pPlayer)
			pPlayer->MoveMarineToPredictedPosition();
	}

	BaseClass::NPCThink();

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pPlayer = dynamic_cast<CASW_Player*>(UTIL_PlayerByIndex( i ));
		if (pPlayer)
			pPlayer->RestoreMarinePosition();
	}
}
