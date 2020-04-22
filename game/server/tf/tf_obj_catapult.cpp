//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//
//=============================================================================
#include "cbase.h"
#include "tf_obj_catapult.h"
#include "tf_player.h"
#include "mathlib/mathlib.h"
#include "in_buttons.h"

#ifdef STAGING_ONLY

#define CATAPULT_THINK_CONTEXT				"CatapultContext"

#define CATAPULT_MODEL	"models/buildables/teleporter_light.mdl"

const Vector CATAPULT_MINS = Vector( -24, -24, 0 );
const Vector CATAPULT_MAXS = Vector( 24, 24, 12 );

ConVar tf_engineer_catapult_force( "tf_engineer_catapult_force", "1000" );
ConVar tf_engineer_catapult_delay( "tf_engineer_catapult_delay", "0" );


//IMPLEMENT_SERVERCLASS_ST( CObjectCatapult, DT_ObjectCatapult )
//END_SEND_TABLE()

BEGIN_DATADESC( CObjectCatapult )
	DEFINE_THINKFUNC( CatapultThink ),
END_DATADESC()

PRECACHE_REGISTER( obj_catapult );

LINK_ENTITY_TO_CLASS( obj_catapult, CObjectCatapult );


CObjectCatapult::CObjectCatapult()
{
	int iHealth = GetMaxHealthForCurrentLevel();

	SetMaxHealth( iHealth );
	SetHealth( iHealth );
	UseClientSideAnimation();

	SetType( OBJ_CATAPULT );
}


void CObjectCatapult::Spawn()
{
	SetSolid( SOLID_BBOX );

	SetModel( CATAPULT_MODEL );
	int nBodyDir = FindBodygroupByName( "teleporter_direction" );
	if ( nBodyDir != -1 )
	{
		SetBodygroup( nBodyDir, 0 );
	}

	UTIL_SetSize( this, CATAPULT_MINS, CATAPULT_MAXS );

	BaseClass::Spawn();

	// HACK: Spin this building 180.  The temp model is backwards
	RotateBuildAngles();
	RotateBuildAngles();

	UpdateDesiredBuildRotation( 5.f );
}


void CObjectCatapult::Precache()
{
	BaseClass::Precache();

	PrecacheModel( CATAPULT_MODEL );
}


void CObjectCatapult::CatapultThink()
{
	if ( IsCarried() )
		return;

	SetContextThink( &CObjectCatapult::CatapultThink, gpGlobals->curtime + 0.1f, CATAPULT_THINK_CONTEXT );

	const float flJumpDelay = tf_engineer_catapult_delay.GetFloat();
	for ( int i=0; i<m_jumpers.Count(); )
	{
		const Jumper_t& jumper = m_jumpers[i];

		// Cleanup
		if( !jumper.m_hJumper )
		{
			m_jumpers.Remove(i);
			continue;
		}
		
		CTFPlayer *pPlayer = ToTFPlayer( jumper.m_hJumper );
		if ( !pPlayer )
		{
			m_jumpers.Remove( i );
			continue;
		}

		//pPlayer->m_nButtons |= IN_DUCK;
		if ( jumper.flTouchTime + flJumpDelay < gpGlobals->curtime || ( pPlayer->m_nButtons & IN_DUCK ) )
		{
			Launch( jumper.m_hJumper );
			m_jumpers.Remove(i);
		}
		else
		{
			++i;
		}
	}
}


void CObjectCatapult::OnGoActive()
{
	BaseClass::OnGoActive();

	SetContextThink( &CObjectCatapult::CatapultThink, gpGlobals->curtime + 0.1, CATAPULT_THINK_CONTEXT );

	int nBodyDir = FindBodygroupByName( "teleporter_direction" );
	if ( nBodyDir != -1 )
	{
		SetBodygroup( nBodyDir, 1 );
	}

}


bool CObjectCatapult::IsPlacementPosValid( void )
{
	bool bResult = BaseClass::IsPlacementPosValid();

	if ( !bResult )
	{
		return false;
	}

	// m_vecBuildOrigin is the proposed build origin

	// start above the teleporter position
	Vector vecTestPos = m_vecBuildOrigin;
	vecTestPos.z += CATAPULT_MAXS.z;

	// make sure we can fit a player on top in this pos
	trace_t tr;
	UTIL_TraceHull( vecTestPos, vecTestPos, VEC_HULL_MIN, VEC_HULL_MAX, MASK_SOLID | CONTENTS_PLAYERCLIP, this, COLLISION_GROUP_PLAYER_MOVEMENT, &tr );

	return ( tr.fraction >= 1.0 );
}


void CObjectCatapult::StartTouch( CBaseEntity *pOther )
{
	BaseClass::StartTouch( pOther );

	if ( pOther->IsPlayer() )
	{
		int index = m_jumpers.AddToTail();
		Jumper_t& jumper = m_jumpers[index];
		jumper.m_hJumper = pOther;
		jumper.flTouchTime = gpGlobals->curtime;
	}
}


void CObjectCatapult::EndTouch( CBaseEntity *pOther )
{
	BaseClass::EndTouch( pOther );

	for ( int i=0; i<m_jumpers.Count(); ++i )
	{
		if ( m_jumpers[i].m_hJumper == pOther )
		{
			m_jumpers.Remove(i);
			return;
		}
	}
}


void CObjectCatapult::Launch( CBaseEntity* pEnt )
{
	CTFPlayer *pPlayer = ToTFPlayer( pEnt );
	if ( !pPlayer ) 
		return;

	//Vector vForward;
	//QAngle qEyeAngle = pEnt->EyeAngles();
	//AngleVectors( pEnt->EyeAngles(), &vForward );
	//vForward.NormalizeInPlace();
	//vForward.z += 2.0f;
	//vForward.NormalizeInPlace();

	
	//pPlayer->ApplyAirBlastImpulse( tf_engineer_catapult_force.GetFloat() * vForward );
	pPlayer->m_Shared.AddCond( TF_COND_SPEED_BOOST, 5.0f );
}

#endif // STAGING_ONLY