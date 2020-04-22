//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A stationary gun that players can man
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_obj_manned_plasmagun.h"
#include "ammodef.h"
#include "plasmaprojectile.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "tf_gamerules.h"

#define MANNED_PLASMAGUN_MINS			Vector(-20, -20, 0)
#define MANNED_PLASMAGUN_MAXS			Vector( 20,  20, 55)
#define MANNED_PLASMAGUN_ALIEN_MODEL	"models/objects/obj_manned_plasmagun.mdl"
#define MANNED_PLASMAGUN_HUMAN_MODEL	"models/objects/human_obj_manned_plasmagun.mdl"

#define MANNED_PLASMAGUN_RECHARGE_TIME	0.2
#define MANNED_PLASMAGUN_IDLE_RECHARGE_TIME	0.1

#define MANNED_PLASMAGUN_IDLE_TIME	2.0

#if !defined( CLIENT_DLL )
BEGIN_DATADESC( CObjectMannedPlasmagun )

	DEFINE_THINKFUNC( RechargeThink ),

END_DATADESC()
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( ObjectMannedPlasmagun, DT_ObjectMannedPlasmagun )

BEGIN_NETWORK_TABLE( CObjectMannedPlasmagun, DT_ObjectMannedPlasmagun )
#if !defined( CLIENT_DLL )
	SendPropTime( SENDINFO( m_flNextIdleTime ) ),
	SendPropInt( SENDINFO( m_bFiringLeft ), 1, SPROP_UNSIGNED ),

	SendPropInt( SENDINFO( m_nNextThinkTick ) ),
#else
	RecvPropTime( RECVINFO( m_flNextIdleTime ) ),
	RecvPropInt( RECVINFO( m_bFiringLeft ) ),

	RecvPropInt		( RECVINFO( m_nNextThinkTick ) ),

#endif
END_NETWORK_TABLE()


BEGIN_PREDICTION_DATA( CObjectMannedPlasmagun  )
	DEFINE_PRED_FIELD_TOL( m_flNextIdleTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD( m_bFiringLeft, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),

	DEFINE_PRED_FIELD( m_nNextThinkTick, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),

//	DEFINE_FIELD( m_nRightBarrelAttachment, FIELD_INTEGER ),
	DEFINE_FIELD( m_nMaxAmmoCount, FIELD_INTEGER ),

END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS(obj_manned_plasmagun, CObjectMannedPlasmagun);
PRECACHE_REGISTER(obj_manned_plasmagun);

// CVars
ConVar	obj_manned_plasmagun_health( "obj_manned_plasmagun_health","100", FCVAR_REPLICATED, "Manned Plasmagun health" );
ConVar	obj_manned_plasmagun_range_def( "obj_manned_plasmagun_range_def","1000", FCVAR_REPLICATED, "Defensive Manned Plasmagun range" );
ConVar	obj_manned_plasmagun_range_off( "obj_manned_plasmagun_range_off","1000", FCVAR_REPLICATED, "Offensive Manned Plasmagun range" );
ConVar	obj_manned_plasmagun_damage( "obj_manned_plasmagun_damage","20", FCVAR_REPLICATED, "Manned Plasmagun damage" );
ConVar	obj_manned_plasmagun_radius( "obj_manned_plasmagun_radius","128", FCVAR_REPLICATED, "Manned Plasmagun explosive radius" );
ConVar	obj_manned_plasmagun_clip( "obj_manned_plasmagun_clip","35", FCVAR_REPLICATED, "Manned Plasmagun's clip size" );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CObjectMannedPlasmagun::CObjectMannedPlasmagun()
{
	m_bFiringLeft = true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectMannedPlasmagun::Precache()
{
	BaseClass::Precache();
	PrecacheModel( MANNED_PLASMAGUN_ALIEN_MODEL );
	PrecacheModel( MANNED_PLASMAGUN_HUMAN_MODEL );

	PrecacheScriptSound( "ObjectMannedPlasmagun.Fire" );

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectMannedPlasmagun::SetupTeamModel( void )
{
	// FIXME: When adding in build animations here, make sure C_ObjectBaseMannedGun::OnDataChanged
	// does the right thing on the client!!
	if ( GetTeamNumber() == TEAM_HUMANS )
	{
		SetMovementStyle( MOVEMENT_STYLE_BARREL_PIVOT );
		SetModel( MANNED_PLASMAGUN_HUMAN_MODEL );
	}
	else
	{
		SetMovementStyle( MOVEMENT_STYLE_STANDARD );
		SetModel( MANNED_PLASMAGUN_ALIEN_MODEL );
	}

	// Call this to get all the attachment points happy
	OnModelSelected();

	// Get our extra barrel
	m_nRightBarrelAttachment = LookupAttachment( "barrelR" );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectMannedPlasmagun::Spawn()
{
	Precache();

	SetSolid( SOLID_BBOX );

	SetSize( MANNED_PLASMAGUN_MINS, MANNED_PLASMAGUN_MAXS );
	SetHealth( obj_manned_plasmagun_health.GetInt() );

	SetNextThink( gpGlobals->curtime + MANNED_PLASMAGUN_IDLE_RECHARGE_TIME );

	SetType( OBJ_MANNED_PLASMAGUN );

	m_nAmmoCount = m_nMaxAmmoCount = obj_manned_plasmagun_clip.GetInt();
	m_flNextAttack = gpGlobals->curtime;
	m_nAmmoType = GetAmmoDef()->Index( "RechargeEnergy" );
	SetThink( RechargeThink );

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: Finished the build
//-----------------------------------------------------------------------------
void CObjectMannedPlasmagun::FinishedBuilding( void )
{
	BaseClass::FinishedBuilding();

	CalculateMaxRange( obj_manned_plasmagun_range_def.GetFloat(), obj_manned_plasmagun_range_off.GetFloat() );
}

//-----------------------------------------------------------------------------
// Recharge think...
//-----------------------------------------------------------------------------
void CObjectMannedPlasmagun::RechargeThink(  )
{
	// Prevent manned guns from deteriorating
	ResetDeteriorationTime();

	float flNextRechargeTime = MANNED_PLASMAGUN_RECHARGE_TIME;
	/*
	ROBIN: Remove idle recharging for now

	if (gpGlobals->curtime >= m_flNextIdleTime)
		flNextRechargeTime = MANNED_PLASMAGUN_IDLE_RECHARGE_TIME; 
	else
		flNextRechargeTime = MANNED_PLASMAGUN_RECHARGE_TIME;
	*/

	// If I'm EMPed, slow the recharge rate down
	if ( HasPowerup(POWERUP_EMP) )
	{
		flNextRechargeTime *= 1.5;
	}
	SetNextThink( gpGlobals->curtime + flNextRechargeTime );

	// I can't do anything if I'm not active
	if ( !ShouldBeActive() )
		return;

	if (m_nAmmoCount < m_nMaxAmmoCount)
	{
		++m_nAmmoCount;
	}
	else
	{
		// No need to think when it's full
		SetNextThink( gpGlobals->curtime + 5.0f );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Plasma sentrygun's fire
//-----------------------------------------------------------------------------
void CObjectMannedPlasmagun::Fire( )
{
	if (m_flNextAttack > gpGlobals->curtime)
		return;

	// Because the plasma sentrygun always thinks it has ammo (see below)
	// we might not have ammo here, in which case we should just abort.
	if ( !m_nAmmoCount )
		return;

	// Make sure we think soon enough in case of firing...
	float flNextRecharge = gpGlobals->curtime + (HasPowerup(POWERUP_EMP) ? MANNED_PLASMAGUN_RECHARGE_TIME * 1.5 : MANNED_PLASMAGUN_RECHARGE_TIME);
	SetNextThink( gpGlobals->curtime + flNextRecharge );

	// We have to flush the bone cache because it's possible that only the bone controllers
	// have changed since the bonecache was generated, and bone controllers aren't checked.
	InvalidateBoneCache();

	QAngle vecAng;
	Vector vecSrc, vecAim;

	// Alternate barrels when firing
	if ( m_bFiringLeft )
	{
		// Aliens permanently fire left barrel because they have no right
		if ( GetTeamNumber() == TEAM_HUMANS )
		{
			m_bFiringLeft = false;
		}
		GetAttachment( m_nBarrelAttachment, vecSrc, vecAng );
		SetActivity( ACT_VM_PRIMARYATTACK );
	}
	else
	{
		m_bFiringLeft = true;
		GetAttachment( m_nRightBarrelAttachment, vecSrc, vecAng );
		SetActivity( ACT_VM_SECONDARYATTACK );
	}

	// Get the distance to the target
	AngleVectors( vecAng, &vecAim, 0, 0 );

	int damageType = GetAmmoDef()->DamageType( m_nAmmoType );
	CBasePlasmaProjectile *pPlasma = CBasePlasmaProjectile::CreatePredicted( vecSrc, vecAim, Vector( 0, 0, 0 ), damageType, GetDriverPlayer() );
	if ( pPlasma )
	{
		pPlasma->SetDamage( obj_manned_plasmagun_damage.GetFloat() );
		pPlasma->m_hOwner = GetDriverPlayer();
		//pPlasma->SetOwnerEntity( this );
		pPlasma->SetMaxRange( m_flMaxRange );
		if ( obj_manned_plasmagun_radius.GetFloat() )
		{
			pPlasma->SetExplosive( obj_manned_plasmagun_radius.GetFloat() );
		}
	}

	CSoundParameters params;
	if ( GetParametersForSound( "ObjectMannedPlasmagun.Fire", params, NULL ) )
	{
		CPASAttenuationFilter filter( this, params.soundlevel );
		if ( IsPredicted() )
		{
			filter.UsePredictionRules();
		}
		EmitSound( filter, entindex(), "ObjectMannedPlasmagun.Fire" );
	}
//	SetSentryAnim( TFTURRET_ANIM_FIRE );
	DoMuzzleFlash();

	--m_nAmmoCount;

	m_flNextIdleTime = gpGlobals->curtime + MANNED_PLASMAGUN_IDLE_TIME;

	// If I'm EMPed, slow the firing rate down
	m_flNextAttack = gpGlobals->curtime + ( HasPowerup(POWERUP_EMP) ? 0.3f : 0.1f );
}

#if defined( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void CObjectMannedPlasmagun::PostDataUpdate( DataUpdateType_t updateType )
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

void CObjectMannedPlasmagun::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate( updateType );

	m_nPreviousTeam = GetTeamNumber();
}

#endif
