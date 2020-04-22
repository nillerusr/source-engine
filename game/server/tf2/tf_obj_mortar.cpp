//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Indirect's mortar object
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "tf_team.h"
#include "basecombatweapon.h"
#include "tf_obj.h"
#include "tf_obj_mortar.h"
#include "techtree.h"
#include "tf_shareddefs.h"
#include "weapon_mortar.h"
#include "vstdlib/random.h"
#include "movevars_shared.h"
#include "mortar_round.h"


LINK_ENTITY_TO_CLASS(obj_mortar, CObjectMortar);
PRECACHE_REGISTER(obj_mortar);

IMPLEMENT_SERVERCLASS_ST(CObjectMortar, DT_ObjectMortar)
	SendPropInt( SENDINFO( m_iRoundType ), 8, SPROP_UNSIGNED, 0 ),
	SendPropArray( SendPropInt( SENDINFO_ARRAY(m_iMortarRounds), 7, 0 ), m_iMortarRounds ),
END_SEND_TABLE();

BEGIN_DATADESC( CObjectMortar )

	DEFINE_THINKFUNC( ReloadingThink ),

END_DATADESC()

// Mortar size
#define MORTAR_MINS		Vector(-16, -16, 0)
#define MORTAR_MAXS		Vector( 16,  16, 64)

ConVar	obj_mortar_health( "obj_mortar_health","200", FCVAR_NONE, "Mortar object health" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectMortar::CObjectMortar()
{
	for ( int i=0; i < m_iMortarRounds.Count(); i++ )
		m_iMortarRounds.Set( i, 0 );
	m_flLastBlastTime = -1;
	UseClientSideAnimation();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectMortar::Spawn()
{
	SetModel( "models/objects/obj_mortar.mdl" );
	
	SetSolid( SOLID_BBOX );
	UTIL_SetSize(this, MORTAR_MINS, MORTAR_MAXS);
	m_takedamage = DAMAGE_YES;
	m_iHealth = obj_mortar_health.GetInt();
	m_iRoundType = MA_SHELL;
	m_iSalvoLeft = MORTAR_SALVO_SIZE;
	m_fObjectFlags |= OF_DONT_PREVENT_BUILD_NEAR_OBJ;

	SetType( OBJ_MORTAR );

	// Fill out the ammo levels
	for ( int i = 0; i < MA_LASTAMMOTYPE; i++ )
	{
		m_iMortarRounds.Set( i, MortarAmmoMax[i] );
	}

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectMortar::Precache()
{
	PrecacheModel( "models/objects/obj_mortar.mdl" );
	PrecacheVGuiScreen( "screen_obj_mortar" );
}

//-----------------------------------------------------------------------------
// Gets info about the control panels
//-----------------------------------------------------------------------------
void CObjectMortar::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "screen_obj_mortar";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectMortar::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Sapper removal
	if ( RemoveEnemyAttachments( pActivator ) )
		return;

	if ( pActivator == GetOwner() )
	{
		int iOldType = m_iRoundType;
		m_iRoundType += 1;

		// Cycle to the next ammo type
		while ( m_iRoundType != iOldType )
		{
			// Hit the end of the round types?
			if ( m_iRoundType == MA_LASTAMMOTYPE )
			{
				m_iRoundType = MA_SHELL;
				break;
			}

			// Does this round type need a technology?
			if ( MortarAmmoTechs[ m_iRoundType ] && MortarAmmoTechs[ m_iRoundType ][0] )
			{
				// Does the player have the technology?
				if ( GetOwner() && GetOwner()->HasNamedTechnology( MortarAmmoTechs[ m_iRoundType ] ) )
				{
					// Do we have ammo?
					if ( m_iMortarRounds[ m_iRoundType ] > 0 )
						break;
				}
			}

			// Go to the next round type
			m_iRoundType += 1;
		}
	}
	else
	{
		// Let other team's technician try to subvert it
		BaseClass::Use( pActivator, pCaller, useType, value );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Fire a round from the mortar
//-----------------------------------------------------------------------------
bool CObjectMortar::FireMortar( float flFiringPower, float flFiringAccuracy, bool bRangeUpgraded, bool bAccuracyUpgraded )
{
	// Are we reloading?
	if ( IsReloading() )
		return false;
	// Do we have any ammo of this type left?
	if ( m_iMortarRounds[ m_iRoundType ] != -1 && m_iMortarRounds[ m_iRoundType ] == 0 )
		return false;

	// Get target distance
	float flDistance;
	if ( bRangeUpgraded )
	{
		flDistance = MORTAR_RANGE_MIN + (flFiringPower * (MORTAR_RANGE_MAX_UPGRADED - MORTAR_RANGE_MIN));
	}
	else
	{
		flDistance = MORTAR_RANGE_MIN + (flFiringPower * (MORTAR_RANGE_MAX_INITIAL - MORTAR_RANGE_MIN));
	}

	// Factor in inaccuracy
	float flInaccuracy;
	if ( bAccuracyUpgraded )
	{
		flInaccuracy = MORTAR_INACCURACY_MAX_UPGRADED * (flFiringAccuracy * 4);	// flFiringAccuracy is a range from -0.25 to 0.25
	}
	else
	{
		flInaccuracy = MORTAR_INACCURACY_MAX_INITIAL * (flFiringAccuracy * 4);	// flFiringAccuracy is a range from -0.25 to 0.25
	}
	flDistance += (flDistance * MORTAR_DIST_INACCURACY) * random->RandomFloat( -flInaccuracy, flInaccuracy );

	Vector forward, right;
	AngleVectors( GetAbsAngles(), &forward, &right, NULL );
	Vector vecTargetOrg = GetAbsOrigin() + (forward * flDistance);
	// Add in sideways inaccuracy
	vecTargetOrg += (right * (flDistance * flInaccuracy) );

	// Trace down from the sky and find the point we're actually going to hit
	trace_t tr;
	Vector vecSky = vecTargetOrg + Vector(0,0,1024);
	UTIL_TraceLine( vecSky, vecTargetOrg, MASK_ALL, this, COLLISION_GROUP_NONE, &tr );
	vecTargetOrg = tr.endpos;

	Vector vecMidPoint = vec3_origin;
	// Start with a low arc, and keep aiming higher until we've got a roughly clear shot
	for (int i = 2048; i <= 4096; i += 1024)
	{
		trace_t tr1;
		trace_t tr2;

		vecMidPoint = Vector(0,0,i) + GetAbsOrigin() + (vecTargetOrg - GetAbsOrigin()) * 0.5;
		UTIL_TraceLine(GetAbsOrigin(), vecMidPoint, MASK_ALL, this, COLLISION_GROUP_NONE, &tr1);
		UTIL_TraceLine(vecMidPoint, vecTargetOrg, MASK_ALL, this, COLLISION_GROUP_NONE, &tr2);

		// Clear shot?
		// We want a clear shot for the first half, and a fairly clear shot on the fall
		if ( tr1.fraction == 1 && tr2.fraction > 0.5 )
			break;
	}

	// How high should we travel to reach the apex
	float distance1 = (vecMidPoint.z - GetAbsOrigin().z);
	float distance2 = (vecMidPoint.z - vecTargetOrg.z);

	// How long will it take to travel this distance
	float flGravity = GetCurrentGravity();
	float time1 = sqrt( distance1 / (0.5 * flGravity) );
	float time2 = sqrt( distance2 / (0.5 * flGravity) );
	if (time1 < 0.1)
		return false;

	// how hard to launch to get there in time.
	Vector vecTargetVel = (vecTargetOrg - GetLocalOrigin()) / (time1 + time2);
	vecTargetVel.z = flGravity * time1;

	// Create the round
	CMortarRound *pRound = CMortarRound::Create( GetLocalOrigin(), vecTargetVel, edict() );
	pRound->ChangeTeam( GetTeamNumber() );
	pRound->SetFallTime( time1 * 0.5 );	// Start a falling sound just a bit before we begin to fall
	pRound->SetRoundType( m_iRoundType );

	// Decrease ammo count
	if ( m_iMortarRounds[ m_iRoundType ] > 0 )
	{
		m_iMortarRounds.Set( m_iRoundType, m_iMortarRounds[m_iRoundType]-1 );
	}

	// Decrease salvo count
	if ( m_iSalvoLeft )
	{
		m_iSalvoLeft--;
		if ( m_iSalvoLeft <= 0 )
		{
			// Time to reload
			StartReloading();
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Start reloading our next salvo
//-----------------------------------------------------------------------------
void CObjectMortar::StartReloading( void )
{
	SetThink( ReloadingThink );
	SetNextThink( gpGlobals->curtime + MORTAR_RELOAD_TIME );
}

//-----------------------------------------------------------------------------
// Purpose: Finish reloading our salvo
//-----------------------------------------------------------------------------
void CObjectMortar::ReloadingThink( void )
{
	SetThink( NULL );

	m_iSalvoLeft = MORTAR_SALVO_SIZE;
}

