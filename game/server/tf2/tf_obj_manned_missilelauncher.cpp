//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A stationary gun that players can man
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_obj_manned_plasmagun.h"
#include "tf_obj_manned_plasmagun_shared.h"
#include "tf_team.h"
#include "tf_obj.h"
#include "sendproxy.h"
#include "in_buttons.h"
#include "tf_player.h"
#include "ammodef.h"
#include "engine/IEngineSound.h"
#include "tf_gamerules.h"
#include "plasmaprojectile.h"
#include "tf_movedata.h"
#include "VGuiScreen.h"
#include "weapon_grenade_rocket.h"
#include "tf_obj_manned_missilelauncher.h"

#define MANNED_MISSILELAUNCHER_CLIP_COUNT			3

#define MANNED_MISSILELAUNCHER_MINS					Vector(-20, -20, 0)
#define MANNED_MISSILELAUNCHER_MAXS					Vector( 20,  20, 55)
#define MANNED_MISSILELAUNCHER_ALIEN_MODEL			"models/objects/obj_manned_plasmagun.mdl"
#define MANNED_MISSILELAUNCHER_HUMAN_MODEL			"models/objects/human_obj_manned_rocketlauncher.mdl"

#define MANNED_MISSILELAUNCHER_RECHARGE_TIME		1.5

#define MANNED_MISSILELAUNCHER_REFIRE_TIME			1.25

BEGIN_DATADESC( CObjectMannedMissileLauncher )
	DEFINE_THINKFUNC( MissileRechargeThink ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CObjectMannedMissileLauncher, DT_ObjectMannedMissileLauncher)
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS(obj_manned_missilelauncher, CObjectMannedMissileLauncher);
PRECACHE_REGISTER(obj_manned_missilelauncher);

// CVars
ConVar	obj_manned_missilelauncher_health( "obj_manned_missilelauncher_health","100", FCVAR_NONE, "Manned Missile Launcher health" );
ConVar	obj_manned_missilelauncher_range_def( "obj_manned_missilelauncher_range_def","1100", FCVAR_NONE, "Defensive Manned Missile Launcher range" );
ConVar	obj_manned_missilelauncher_range_off( "obj_manned_missilelauncher_range_off","900", FCVAR_NONE, "Offensive Manned Missile Launcher range" );
ConVar	obj_manned_missilelauncher_damage( "obj_manned_missilelauncher_damage","150", FCVAR_NONE, "Manned Missile Launcher damage" );
ConVar	obj_manned_missilelauncher_radius( "obj_manned_missilelauncher_radius","128", FCVAR_NONE, "Manned Missile Launcher explosive radius" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectMannedMissileLauncher::CObjectMannedMissileLauncher()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectMannedMissileLauncher::Precache()
{
	PrecacheModel( MANNED_MISSILELAUNCHER_ALIEN_MODEL );
	PrecacheModel( MANNED_MISSILELAUNCHER_HUMAN_MODEL );

	PrecacheScriptSound( "ObjectMannedMissileLauncher.Fire" );
	PrecacheScriptSound( "ObjectMannedMissileLauncher.Reload" );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectMannedMissileLauncher::Spawn()
{
	m_iHealth = obj_manned_missilelauncher_health.GetInt();
	BaseClass::Spawn();

	SetSolid( SOLID_BBOX );
	UTIL_SetSize(this, MANNED_MISSILELAUNCHER_MINS, MANNED_MISSILELAUNCHER_MAXS);

	SetThink( MissileRechargeThink );
	SetNextThink( gpGlobals->curtime + MANNED_MISSILELAUNCHER_RECHARGE_TIME );

	SetType( OBJ_MANNED_MISSILELAUNCHER );
	m_nAmmoType = GetAmmoDef()->Index( "Rockets" );
	m_nAmmoCount = m_nMaxAmmoCount = MANNED_MISSILELAUNCHER_CLIP_COUNT;
}

//-----------------------------------------------------------------------------
// Purpose: Finished the build
//-----------------------------------------------------------------------------
void CObjectMannedMissileLauncher::FinishedBuilding( void )
{
	BaseClass::FinishedBuilding();

	CalculateMaxRange( obj_manned_missilelauncher_range_def.GetFloat(), obj_manned_missilelauncher_range_off.GetFloat() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectMannedMissileLauncher::SetupTeamModel( void )
{
	// FIXME: When adding in build animations here, make sure C_ObjectBaseMannedGun::OnDataChanged
	// does the right thing on the client!!
	if ( GetTeamNumber() == TEAM_HUMANS )
	{
		SetMovementStyle( MOVEMENT_STYLE_BARREL_PIVOT );
		SetModel( MANNED_MISSILELAUNCHER_HUMAN_MODEL );
	}
	else
	{
		SetMovementStyle( MOVEMENT_STYLE_STANDARD );
		SetModel( MANNED_MISSILELAUNCHER_ALIEN_MODEL );
	}

	// Call this to get all the attachment points happy
	OnModelSelected();
}

//-----------------------------------------------------------------------------
// Recharge think...
//-----------------------------------------------------------------------------
void CObjectMannedMissileLauncher::MissileRechargeThink( void )
{
	// Prevent manned guns from deteriorating
	ResetDeteriorationTime();
	SetNextThink( gpGlobals->curtime + (HasPowerup(POWERUP_EMP) ? (MANNED_MISSILELAUNCHER_RECHARGE_TIME * 1.5) : MANNED_MISSILELAUNCHER_RECHARGE_TIME ) );

	// I can't do anything if I'm not active
	if ( !ShouldBeActive() )
		return;

	if (m_nAmmoCount < m_nMaxAmmoCount)
	{
		m_nAmmoCount += 1;

		EmitSound( "ObjectMannedMissileLauncher.Reload" );

		// Push fire out
		m_flNextAttack = gpGlobals->curtime + 0.3;
	}
	else
	{
		// No need to think when it's full
		SetNextThink( gpGlobals->curtime + 5.0f );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Missile Launcher fire
//-----------------------------------------------------------------------------
void CObjectMannedMissileLauncher::Fire( )
{
	if ( m_flNextAttack > gpGlobals->curtime )
		return;
	if ( !m_nAmmoCount )
		return;

	// Push recharge out
	SetNextThink( gpGlobals->curtime + (HasPowerup(POWERUP_EMP) ? (MANNED_MISSILELAUNCHER_RECHARGE_TIME * 1.5) : MANNED_MISSILELAUNCHER_RECHARGE_TIME ) );

	// We have to flush the bone cache because it's possible that only the bone controllers
	// have changed since the bonecache was generated, and bone controllers aren't checked.
	InvalidateBoneCache();

	QAngle vecAng;
	Vector vecSrc, vecAim;
	GetAttachment( m_nBarrelAttachment, vecSrc, vecAng );

	// Get the distance to the target
	AngleVectors( vecAng, &vecAim );

	// Create the rocket.
	CWeaponGrenadeRocket *pRocket = CWeaponGrenadeRocket::Create( vecSrc, vecAim, m_flMaxRange, this );
	if ( pRocket )
	{
		pRocket->SetRealOwner( GetDriverPlayer() );
		pRocket->SetDamage( obj_manned_missilelauncher_damage.GetFloat() );
		pRocket->SetDamageRadius( obj_manned_missilelauncher_radius.GetFloat() );
	}

	SetActivity( ACT_VM_PRIMARYATTACK );

	EmitSound( "ObjectMannedMissileLauncher.Fire" );
	// SetSentryAnim( TFTURRET_ANIM_FIRE );
	DoMuzzleFlash();

	m_nAmmoCount -= 1;

	// Slow fire rate while EMPed
	m_flNextAttack = gpGlobals->curtime + ( HasPowerup(POWERUP_EMP) ? MANNED_MISSILELAUNCHER_REFIRE_TIME * 2 : MANNED_MISSILELAUNCHER_REFIRE_TIME );
}

#if defined( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void CObjectMannedMissileLauncher::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	bool teamchanged = GetTeamNumber() != m_nPreviousTeam;

	if ( teamchanged ||
		updateType == DATA_UPDATE_CREATED )
	{
		C_BaseAnimating::AllowBoneAccess( true, false );
		SetupTeamModel();
		C_BaseAnimating::AllowBoneAccess( false, false );
	}
}

void CObjectMannedMissileLauncher::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate( updateType );

	m_nPreviousTeam = GetTeamNumber();
}

#endif
