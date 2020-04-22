//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A moving vehicle that is used as a battering ram
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_vehicle_mortar.h"
#include "engine/IEngineSound.h"
#include "VGuiScreen.h"
#include "ammodef.h"
#include "in_buttons.h"
#include "vehicle_mortar_shared.h"
#include "movevars_shared.h"
#include "mortar_round.h"


// Waits this long after each shot before they can fire.
#define MORTAR_FIRE_DELAY		2


#define MORTAR_MINS				Vector(-30, -50, -10)
#define MORTAR_MAXS				Vector( 30,  50, 55)
#define MORTAR_MODEL			"models/objects/vehicle_mortar.mdl"
#define MORTAR_SCREEN_NAME		"screen_vehicle_mortar"  

#define ELEVATION_INTERVAL 0.3

const char *g_pMortarThinkContextName = "MortarThinkContext";
const char *g_pMortarNextFireContextName = "MortarNextFireContext";



IMPLEMENT_SERVERCLASS_ST(CVehicleMortar, DT_VehicleMortar)
	SendPropFloat( SENDINFO( m_flMortarYaw ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flMortarPitch ), 0, SPROP_NOSCALE ),
	SendPropBool( SENDINFO( m_bAllowedToFire ) )
END_SEND_TABLE();


LINK_ENTITY_TO_CLASS(vehicle_mortar, CVehicleMortar);
PRECACHE_REGISTER(vehicle_mortar);


// CVars
ConVar	vehicle_mortar_health( "vehicle_mortar_health","800", FCVAR_NONE, "Mortar vehicle health" );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVehicleMortar::CVehicleMortar()
{
	m_flMortarYaw = 0;
	m_flMortarPitch = 0;
	m_bAllowedToFire = true;
	
	UseClientSideAnimation();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVehicleMortar::Precache()
{
	PrecacheModel( MORTAR_MODEL );

	PrecacheVGuiScreen( MORTAR_SCREEN_NAME );

	PrecacheScriptSound( "VehicleMortar.FireSound" );

	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVehicleMortar::Spawn()
{
	SetModel( MORTAR_MODEL );
	
	// This size is used for placement only...
	SetSize(MORTAR_MINS, MORTAR_MAXS);
	m_takedamage = DAMAGE_YES;
	m_iHealth = vehicle_mortar_health.GetInt();

	SetType( OBJ_VEHICLE_MORTAR );
	SetMaxPassengerCount( 1 );

	BaseClass::Spawn();
}


//-----------------------------------------------------------------------------
// Purpose: Gets info about the control panels
//-----------------------------------------------------------------------------
void CVehicleMortar::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = MORTAR_SCREEN_NAME;
}


bool CVehicleMortar::CanGetInVehicle( CBaseTFPlayer *pPlayer )
{
	return ( !InDeployMode() && BaseClass::CanGetInVehicle( pPlayer ) );
}


//-----------------------------------------------------------------------------
// Here's where we deal with weapons
//-----------------------------------------------------------------------------
void CVehicleMortar::OnItemPostFrame( CBaseTFPlayer *pDriver )
{
	// I can't do anything if I'm not active
	if ( !ShouldBeActive() )
		return;

	if ( GetPassengerRole( pDriver ) != VEHICLE_ROLE_DRIVER )
		return;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVehicleMortar::OnFinishedDeploy( void )
{
	BaseClass::OnFinishedDeploy();

	EntityMessageBegin( this, true );
		WRITE_STRING( "OnDeployed" );
	MessageEnd();
	
	m_flMortarYaw = 0;
	m_flMortarPitch = 45;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CVehicleMortar::OnFinishedUnDeploy( void )
{
	BaseClass::OnFinishedUnDeploy();

	// Called when we are deployed.
	EntityMessageBegin( this, true );
		WRITE_STRING( "OnUndeployed" );
	MessageEnd();

	m_flMortarYaw = 0;
	m_flMortarPitch = 0;
}


void CVehicleMortar::SetPassenger( int nRole, CBasePlayer *pEnt )
{
	if ( pEnt )
		ShowVGUIScreen( 0, false );
	else
		ShowVGUIScreen( 0, true );

	BaseClass::SetPassenger( nRole, pEnt );
}

void CVehicleMortar::UpdateElevation( const Vector &vecTargetVel )
{
	QAngle angles;
	VectorAngles( vecTargetVel, angles );
	m_flMortarPitch = anglemod( -angles[PITCH] );

	SetBoneController( 0, m_flMortarYaw );
	SetBoneController( 1, m_flMortarPitch );
}


bool CVehicleMortar::ClientCommand( CBaseTFPlayer *pPlayer, const char *pCmd, ICommandArguments *pArg )
{
	ResetDeteriorationTime();

	if ( !Q_stricmp( pCmd, "Deploy" ) )
	{
		Deploy();
		return true;
	}
	else if ( !Q_stricmp( pCmd, "Undeploy" ) )
	{
		UnDeploy();
	}
	else if ( !Q_stricmp( pCmd, "CancelDeploy" ) )
	{
		CancelDeploy();
		return true;
	}
	else if ( !Q_stricmp( pCmd, "FireMortar" ) )
	{
		if ( pArg->Argc() == 3 )
		{
			FireMortar( atof( pArg->Argv(1) ), atof( pArg->Argv(2) ), false, false );
		}
		return true;
	}
	else if ( !Q_stricmp( pCmd, "MortarYaw" ) )
	{
		if ( pArg->Argc() == 2 )
		{
			m_flMortarYaw = atof( pArg->Argv(1) );
		}
		return true;
	}

	return BaseClass::ClientCommand( pPlayer, pCmd, pArg );
}


bool CVehicleMortar::CalcFireInfo( 
	float flFiringPower, 
	float flFiringAccuracy, 
	bool bRangeUpgraded, 
	bool bAccuracyUpgraded,
	Vector &vStartPt,
	Vector &vecTargetVel,
	float &fallTime
	)
{
	QAngle dummy;
	if ( !GetAttachment( "barrel", vStartPt, dummy ) )
		vStartPt = WorldSpaceCenter();

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

	float flAngle = GetAbsAngles()[YAW] + m_flMortarYaw;
	Vector forward( -sin( DEG2RAD( flAngle ) ), cos( DEG2RAD( flAngle ) ), 0 );
	Vector right( forward.y, -forward.x, 0 );

	Vector vecTargetOrg = vStartPt + (forward * flDistance);
	// Add in sideways inaccuracy
	vecTargetOrg += (right * (flDistance * random->RandomFloat( -flInaccuracy, flInaccuracy )) );

	// Trace down from the sky and find the point we're actually going to hit
	trace_t tr;
	Vector vecSky = vecTargetOrg + Vector(0,0,1024);
	UTIL_TraceLine( vecSky, vecTargetOrg, MASK_ALL, this, COLLISION_GROUP_NONE, &tr );
	vecTargetOrg = tr.endpos;

	Vector vecMidPoint = vec3_origin;
	// Start with a low arc, and keep aiming higher until we've got a roughly clear shot
	for (int i = 512; i <= 4096; i += 512)
	{
		trace_t tr1;
		trace_t tr2;

		vecMidPoint = Vector(0,0,i) + vStartPt + (vecTargetOrg - vStartPt) * 0.5;
		UTIL_TraceLine(vStartPt, vecMidPoint, MASK_ALL, this, COLLISION_GROUP_NONE, &tr1);
		UTIL_TraceLine(vecMidPoint, vecTargetOrg, MASK_ALL, this, COLLISION_GROUP_NONE, &tr2);

		// Clear shot?
		// We want a clear shot for the first half, and a fairly clear shot on the fall
		if ( tr1.fraction == 1 && tr2.fraction > 0.5 )
			break;
	}

	// How high should we travel to reach the apex
	float distance1 = (vecMidPoint.z - vStartPt.z);
	float distance2 = (vecMidPoint.z - vecTargetOrg.z);

	// How long will it take to travel this distance
	float flGravity = GetCurrentGravity();
	float time1 = sqrt( distance1 / (0.5 * flGravity) );
	float time2 = sqrt( distance2 / (0.5 * flGravity) );
	if (time1 < 0.1)
		return false;

	// how hard to launch to get there in time.
	vecTargetVel = (vecTargetOrg - vStartPt) / (time1 + time2);
	vecTargetVel.z = flGravity * time1;

	fallTime = time1 * 0.5;
	return true;
}


void CVehicleMortar::NextFireThink()
{
	// Ok, we can fire again now.
	m_bAllowedToFire = true;
}


bool CVehicleMortar::FireMortar( float flFiringPower, float flFiringAccuracy, bool bRangeUpgraded, bool bAccuracyUpgraded )
{
	SetActivity( ACT_RANGE_ATTACK1 );
	
	// Calculate the shot.
	Vector vStartPt;
	Vector vecTargetVel;
	float fallTime;

	if ( !CalcFireInfo(
		flFiringPower, 
		flFiringAccuracy, 
		bRangeUpgraded, 
		bAccuracyUpgraded,
		vStartPt,
		vecTargetVel,
		fallTime ) )
	{
		return false;
	}

	UpdateElevation( vecTargetVel );

	// Create the round
	CMortarRound *pRound = CMortarRound::Create( vStartPt, vecTargetVel, GetOwner()->edict() );
	pRound->SetLauncher( this );
	pRound->ChangeTeam( GetTeamNumber() );
	pRound->SetFallTime( fallTime );	// Start a falling sound just a bit before we begin to fall
	pRound->SetRoundType( MA_SHELL );

	// BOOM!
	EmitSound( "VehicleMortar.FireSound" );

	// Put in a delay before thinking again.
	m_bAllowedToFire = false;
	SetContextThink( NextFireThink, gpGlobals->curtime + MORTAR_FIRE_DELAY, g_pMortarNextFireContextName );

	return true;
}


