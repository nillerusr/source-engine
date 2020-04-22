//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "tf_weapon_throwable.h"
#include "tf_gamerules.h"
#include "in_buttons.h"
#include "basetypes.h"
#include "tf_weaponbase_gun.h"
#include "effect_dispatch_data.h"

// Client specific.
#ifdef CLIENT_DLL
#include "c_tf_player.h"
// Server specific.
#else
#include "tf_player.h"
#include "tf_fx.h"
#include "te_effect_dispatch.h"
#include "bone_setup.h"
#include "tf_target_dummy.h"
#endif


// Base
// Launcher
IMPLEMENT_NETWORKCLASS_ALIASED( TFThrowable, DT_TFWeaponThrowable )
BEGIN_NETWORK_TABLE( CTFThrowable, DT_TFWeaponThrowable )
#ifdef CLIENT_DLL
RecvPropFloat( RECVINFO( m_flChargeBeginTime ) ),
#else
SendPropFloat( SENDINFO( m_flChargeBeginTime ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFThrowable )
END_PREDICTION_DATA()

//LINK_ENTITY_TO_CLASS( tf_weapon_throwable, CTFThrowable );
//PRECACHE_WEAPON_REGISTER( tf_weapon_throwable );

#ifdef STAGING_ONLY
CREATE_SIMPLE_WEAPON_TABLE( TFThrowablePrimary, tf_weapon_throwable_primary )
CREATE_SIMPLE_WEAPON_TABLE( TFThrowableSecondary, tf_weapon_throwable_secondary )
CREATE_SIMPLE_WEAPON_TABLE( TFThrowableMelee, tf_weapon_throwable_melee )
CREATE_SIMPLE_WEAPON_TABLE( TFThrowableUtility, tf_weapon_throwable_utility )
#endif

// Projectile
IMPLEMENT_NETWORKCLASS_ALIASED( TFProjectile_Throwable, DT_TFProjectile_Throwable )
BEGIN_NETWORK_TABLE( CTFProjectile_Throwable, DT_TFProjectile_Throwable )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( tf_projectile_throwable, CTFProjectile_Throwable );
PRECACHE_WEAPON_REGISTER( tf_projectile_throwable );

// Projectile Repel
IMPLEMENT_NETWORKCLASS_ALIASED( TFProjectile_ThrowableRepel, DT_TFProjectile_ThrowableRepel )
BEGIN_NETWORK_TABLE( CTFProjectile_ThrowableRepel, DT_TFProjectile_ThrowableRepel )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( tf_projectile_throwable_repel, CTFProjectile_ThrowableRepel );
PRECACHE_WEAPON_REGISTER( tf_projectile_throwable_repel );

// Projectile Brick
IMPLEMENT_NETWORKCLASS_ALIASED( TFProjectile_ThrowableBrick, DT_TFProjectile_ThrowableBrick )
BEGIN_NETWORK_TABLE( CTFProjectile_ThrowableBrick, DT_TFProjectile_ThrowableBrick )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( tf_projectile_throwable_brick, CTFProjectile_ThrowableBrick );
PRECACHE_WEAPON_REGISTER( tf_projectile_throwable_brick );

// Projectile Bread Monster
IMPLEMENT_NETWORKCLASS_ALIASED( TFProjectile_ThrowableBreadMonster, DT_TFProjectile_ThrowableBreadMonster )
BEGIN_NETWORK_TABLE( CTFProjectile_ThrowableBreadMonster, DT_TFProjectile_ThrowableBreadMonster )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( tf_projectile_throwable_breadmonster, CTFProjectile_ThrowableBreadMonster );
PRECACHE_WEAPON_REGISTER( tf_projectile_throwable_breadmonster );

#ifdef STAGING_ONLY
// Projectile Target Dummy
IMPLEMENT_NETWORKCLASS_ALIASED( TFProjectile_ThrowableTargetDummy, DT_TFProjectile_ThrowableTargetDummy )
BEGIN_NETWORK_TABLE( CTFProjectile_ConcGrenade, DT_TFProjectile_ThrowableTargetDummy )
END_NETWORK_TABLE()
LINK_ENTITY_TO_CLASS( tf_projectile_target_dummy, CTFProjectile_ThrowableTargetDummy );
PRECACHE_WEAPON_REGISTER( tf_projectile_target_dummy );

// Projectile Concussion Grenade
IMPLEMENT_NETWORKCLASS_ALIASED( TFProjectile_ConcGrenade, DT_TFProjectile_ConcGrenade )
BEGIN_NETWORK_TABLE( CTFProjectile_ConcGrenade, DT_TFProjectile_ConcGrenade )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( tf_projectile_grenade_concussion, CTFProjectile_ConcGrenade );
PRECACHE_WEAPON_REGISTER( tf_projectile_grenade_concussion );

// Projectile Teleport Grenade
IMPLEMENT_NETWORKCLASS_ALIASED( TFProjectile_TeleportGrenade, DT_TFProjectile_TeleportGrenade )
BEGIN_NETWORK_TABLE( CTFProjectile_TeleportGrenade, DT_TFProjectile_TeleportGrenade )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( tf_projectile_grenade_teleport, CTFProjectile_TeleportGrenade );
PRECACHE_WEAPON_REGISTER( tf_projectile_grenade_teleport );

// Projectile Chain Grenade
IMPLEMENT_NETWORKCLASS_ALIASED( TFProjectile_GravityGrenade, DT_TFProjectile_GravityGrenade )
BEGIN_NETWORK_TABLE( CTFProjectile_GravityGrenade, DT_TFProjectile_GravityGrenade )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( tf_projectile_grenade_gravity, CTFProjectile_GravityGrenade );
PRECACHE_WEAPON_REGISTER( tf_projectile_grenade_gravity );

// Projectile Chain Grenade
IMPLEMENT_NETWORKCLASS_ALIASED( TFProjectile_ThrowingKnife, DT_TFProjectile_ThrowingKnife )
BEGIN_NETWORK_TABLE( CTFProjectile_ThrowingKnife, DT_TFProjectile_ThrowingKnife )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( tf_projectile_throwing_knife, CTFProjectile_ThrowingKnife );
PRECACHE_WEAPON_REGISTER( tf_projectile_throwing_knife );

// Projectile Smoke Grenade
IMPLEMENT_NETWORKCLASS_ALIASED( TFProjectile_SmokeGrenade, DT_TFProjectile_SmokeGrenade )
BEGIN_NETWORK_TABLE( CTFProjectile_SmokeGrenade, DT_TFProjectile_SmokeGrenade )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( tf_projectile_grenade_smoke, CTFProjectile_SmokeGrenade );
PRECACHE_WEAPON_REGISTER( tf_projectile_grenade_smoke );
#endif // STAGING_ONLY

#define TF_GRENADE_TIMER			"Weapon_Grenade.Timer"
#define TF_GRENADE_CHARGE			"Weapon_LooseCannon.Charge"

//****************************************************************************
// Throwable Weapon
//****************************************************************************
CTFThrowable::CTFThrowable( void )
{
	m_flChargeBeginTime = -1.0f;
}

//-----------------------------------------------------------------------------
void CTFThrowable::Precache()
{
	BaseClass::Precache();

	PrecacheModel( g_pszArrowModels[MODEL_BREAD_MONSTER] );
	PrecacheModel( g_pszArrowModels[MODEL_THROWING_KNIFE] );
#ifdef STAGING_ONLY
	PrecacheScriptSound( "Weapon_Grenade_Concussion.Explode" );
	PrecacheScriptSound( "Weapon_Grenade_Teleport.Explode" );
	PrecacheScriptSound( TF_GRENADE_TIMER );
#endif // STAGING_ONLY
	PrecacheScriptSound( TF_GRENADE_CHARGE );
	
	PrecacheScriptSound( "Weapon_bm_throwable.throw" );
	PrecacheScriptSound( "Weapon_bm_throwable.smash" );

	PrecacheParticleSystem( "grenade_smoke_cycle" );
	PrecacheParticleSystem( "blood_bread_biting" );
}

//-----------------------------------------------------------------------------
float CTFThrowable::InternalGetEffectBarRechargeTime( void )
{
	float flRechargeTime = 0;
	CALL_ATTRIB_HOOK_FLOAT( flRechargeTime, throwable_recharge_time );
	if ( flRechargeTime )
		return flRechargeTime;
	return 10.0f; // default
}

//-----------------------------------------------------------------------------
float CTFThrowable::GetDetonationTime()
{
	float flDetonationTime = 0;
	CALL_ATTRIB_HOOK_FLOAT( flDetonationTime, throwable_detonation_time );
	if ( flDetonationTime )
		return flDetonationTime;
	return 5.0f; // default 
}

//-----------------------------------------------------------------------------
void CTFThrowable::PrimaryAttack( void )
{
	if ( !CanCharge() )
	{
		// Fire
		BaseClass::PrimaryAttack();
		return;
	}

	if ( m_flChargeBeginTime > 0 )
		return;
	
	// Do all the Checks and start a charged (primed) attack
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

	// Check for ammunition.
	if ( pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) < 1 )
		return;

	// Are we capable of firing again?
	if ( m_flNextPrimaryAttack > gpGlobals->curtime )
		return;

	if ( pPlayer->GetWaterLevel() == WL_Eyes )
		return;

	if ( !CanAttack() )
		return;

	if ( m_flChargeBeginTime <= 0 )
	{
		// Set the weapon mode.
		m_iWeaponMode = TF_WEAPON_PRIMARY_MODE;
		SendWeaponAnim( ACT_VM_PULLBACK );	// TODO : Anim!
#ifdef GAME_DLL
		// save that we had the attack button down
		m_flChargeBeginTime = gpGlobals->curtime;
#endif // GAME_LL
		
#ifdef CLIENT_DLL
		if ( pPlayer == C_BasePlayer::GetLocalPlayer() )
		{
			int iCanBeCharged = 0;
			CALL_ATTRIB_HOOK_INT( iCanBeCharged, is_throwable_chargeable );
			if ( iCanBeCharged )
			{
				EmitSound( TF_GRENADE_CHARGE );
			}
			else 
			{
				EmitSound( TF_GRENADE_TIMER );
			}
		}
#endif // CLIENT_DLL
	}
}

//-----------------------------------------------------------------------------
void CTFThrowable::ItemPostFrame( void )
{
	// Get the player owning the weapon.
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

	if ( m_flChargeBeginTime > 0.f && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
	{
		bool bFiredWeapon = false;
		// If we're not holding down the attack button, launch our grenade
		if ( !( pPlayer->m_nButtons & IN_ATTACK ) )
		{
			FireProjectile( pPlayer );
			bFiredWeapon = true;
		}
		// Misfire
		else if ( m_flChargeBeginTime + GetDetonationTime() < gpGlobals->curtime )
		{
			CTFProjectile_Throwable * pThrowable = dynamic_cast<CTFProjectile_Throwable*>( FireProjectile( pPlayer ) );
			if ( pThrowable )
			{
#ifdef GAME_DLL
				pThrowable->Misfire();
#endif // GAME_DLL
			}

			bFiredWeapon = true;
		}

		if ( bFiredWeapon )
		{
			SendWeaponAnim( ACT_VM_PRIMARYATTACK );
			pPlayer->SetAnimation( PLAYER_ATTACK1 );
#ifdef GAME_DLL
			m_flChargeBeginTime = -1.0f; // reset
#endif // GAME_DLL
			// Set next attack times.
			float flFireDelay = m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_flTimeFireDelay;
			m_flNextPrimaryAttack = gpGlobals->curtime + flFireDelay;
			SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );
#ifdef CLIENT_DLL
			int iCanBeCharged = 0;
			CALL_ATTRIB_HOOK_INT( iCanBeCharged, is_throwable_chargeable );
			if ( iCanBeCharged )
			{
				StopSound( TF_GRENADE_CHARGE );
			}
#endif // CLIENT_DLL
		}
	}
	BaseClass::ItemPostFrame();
}

// ITFChargeUpWeapon
//-----------------------------------------------------------------------------
// Primable is for timed explosions
// Charagable is for things like distance or power increases
// Can't really have both but can have neither
bool CTFThrowable::CanCharge()
{
	int iCanBePrimed = 0;
	CALL_ATTRIB_HOOK_INT( iCanBePrimed, is_throwable_primable );

	int iCanBeCharged = 0;
	CALL_ATTRIB_HOOK_INT( iCanBeCharged, is_throwable_chargeable );

	return iCanBeCharged || iCanBePrimed ;
}

//-----------------------------------------------------------------------------
float CTFThrowable::GetChargeBeginTime( void )
{
	float flDetonateTimeLength = GetDetonationTime();
//	float flModDetonateTimeLength = 0;
	
	int iCanBePrimed = 0;
	CALL_ATTRIB_HOOK_INT( iCanBePrimed, is_throwable_primable );

	// Use reverse logic for primable grenades (Counts down to boom)
	// Full charge since we haven't fired
	if ( iCanBePrimed )
	{
		if ( m_flChargeBeginTime < 0 )
		{
			return gpGlobals->curtime - flDetonateTimeLength;
		}
		return gpGlobals->curtime - Clamp( m_flChargeBeginTime + flDetonateTimeLength - gpGlobals->curtime, 0.f, flDetonateTimeLength );
	}

	return m_flChargeBeginTime;
}

//-----------------------------------------------------------------------------
float CTFThrowable::GetChargeMaxTime( void )
{
	return GetDetonationTime();
}
//-----------------------------------------------------------------------------
CBaseEntity *CTFThrowable::FireJar( CTFPlayer *pPlayer )
{
#ifdef GAME_DLL
	return FireProjectileInternal();
#endif
	return NULL;
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
void CTFThrowable::TossJarThink( void )
{
	FireProjectileInternal();
}

//-----------------------------------------------------------------------------
CTFProjectile_Throwable *CTFThrowable::FireProjectileInternal( void )
{
	CTFPlayer *pPlayer = GetTFPlayerOwner();
	if ( !pPlayer )
		return NULL;

	CAttribute_String attrProjectileEntityName;
	GetProjectileEntityName( &attrProjectileEntityName );
	if ( !attrProjectileEntityName.has_value() )
		return NULL;

	Vector vecForward, vecRight, vecUp;
	AngleVectors( pPlayer->EyeAngles(), &vecForward, &vecRight, &vecUp );

	float fRight = 8.f;
	if ( IsViewModelFlipped() )
	{
		fRight *= -1;
	}
	Vector vecSrc = pPlayer->Weapon_ShootPosition();

	// Make spell toss position at the hand
	vecSrc = vecSrc + ( vecUp * -9.0f ) + ( vecRight * 7.0f ) + ( vecForward * 3.0f );

	trace_t trace;
	Vector vecEye = pPlayer->EyePosition();
	CTraceFilterSimple traceFilter( this, COLLISION_GROUP_NONE );
	UTIL_TraceHull( vecEye, vecSrc, -Vector( 8, 8, 8 ), Vector( 8, 8, 8 ), MASK_SOLID_BRUSHONLY, &traceFilter, &trace );

	// If we started in solid, don't let them fire at all
	if ( trace.startsolid )
		return NULL;

	CalcIsAttackCritical();

	// Create the Grenade and Intialize it appropriately
	CTFProjectile_Throwable *pGrenade = static_cast<CTFProjectile_Throwable*>( CBaseEntity::CreateNoSpawn( attrProjectileEntityName.value().c_str(), trace.endpos, pPlayer->EyeAngles(), pPlayer ) );
	if ( pGrenade )
	{
		// Set the pipebomb mode before calling spawn, so the model & associated vphysics get setup properly.
		pGrenade->SetPipebombMode();
		pGrenade->SetLauncher( this );
		pGrenade->SetCritical( IsCurrentAttackACrit() );

		DispatchSpawn( pGrenade );

		// Calculate a charge percentage
		// For now Charge just effects exit velocity
		int iCanBeCharged = 0;
		float flChargePercent = 0;
		float flDetonateTime = GetDetonationTime();
		CALL_ATTRIB_HOOK_INT( iCanBeCharged, is_throwable_chargeable );
		if ( iCanBeCharged )
		{
			flChargePercent = RemapVal( gpGlobals->curtime, m_flChargeBeginTime, m_flChargeBeginTime + flDetonateTime, 0.0f, 1.0f );
		}

		Vector vecVelocity = pGrenade->GetVelocityVector( vecForward, vecRight, vecUp, flChargePercent );
		AngularImpulse angVelocity = pGrenade->GetAngularImpulse();

		pGrenade->InitGrenade( vecVelocity, angVelocity, pPlayer, GetTFWpnData() );
		pGrenade->InitThrowable( flChargePercent );
		pGrenade->ApplyLocalAngularVelocityImpulse( angVelocity );
		
		if ( flDetonateTime > 0 )
		{
			// Check if this has been primed
			int iCanBePrimed = 0;
			CALL_ATTRIB_HOOK_INT( iCanBePrimed, is_throwable_primable );
			if ( m_flChargeBeginTime > 0 && iCanBePrimed > 0 )
			{
				flDetonateTime = ( m_flChargeBeginTime + flDetonateTime - gpGlobals->curtime );
			}
			pGrenade->SetDetonateTimerLength( flDetonateTime );
		}
		pGrenade->m_flFullDamage = 0;

		if ( pGrenade->GetThrowSoundEffect() )
		{
			pGrenade->EmitSound( pGrenade->GetThrowSoundEffect() );
		}
	}

	StartEffectBarRegen();

	return pGrenade;
}
#endif // GAME_DLL

//----------------------------------------------------------------------------------------------------------------------------------------------------------
// Throwable Projectile
//----------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef GAME_DLL
CTFProjectile_Throwable::CTFProjectile_Throwable( void )
{
	m_flChargePercent = 0;
	m_bHit = false;
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
// Get Initial Velocity
Vector CTFProjectile_Throwable::GetVelocityVector( const Vector &vecForward, const Vector &vecRight, const Vector &vecUp, float flCharge )
{
	// Scale the projectile speed up to a maximum of 3000?
	float flSpeed = RemapVal( flCharge, 0, 1.0f, GetProjectileSpeed(), GetProjectileMaxSpeed() );

	return ( ( flSpeed * vecForward ) + 
		( ( random->RandomFloat( -10.0f, 10.0f ) + 200.0f ) * vecUp ) + 
		(   random->RandomFloat( -10.0f, 10.0f ) * vecRight ) );
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void CTFProjectile_Throwable::OnHit( CBaseEntity *pOther ) 
{ 
	if ( m_bHit )
		return;

	if ( ExplodesOnHit() )
	{
		Explode();
	}

	m_bHit = true;
}
//-----------------------------------------------------------------------------
void CTFProjectile_Throwable::Explode()
{
	trace_t		tr;
	Vector		vecSpot;// trace starts here!
	SetThink( NULL );
	vecSpot = GetAbsOrigin() + Vector ( 0 , 0 , 8 );
	UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -32 ), MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, & tr);
	Explode( &tr, GetDamageType() );
}

//-----------------------------------------------------------------------------
void CTFProjectile_Throwable::Explode( trace_t *pTrace, int bitsDamageType )
{
	if ( GetThrower() )
	{
		InitialExplodeEffects( NULL, pTrace );

		// Particle
		const char* pszExplodeEffect = GetExplodeEffectParticle();
		if ( pszExplodeEffect && pszExplodeEffect[0] != '\0' )
		{	
			CPVSFilter filter( GetAbsOrigin() );
			TE_TFParticleEffect( filter, 0.0, pszExplodeEffect, GetAbsOrigin(), vec3_angle );
		}

		// Sounds
		const char* pszSoundEffect = GetExplodeEffectSound();
		if ( pszSoundEffect && pszSoundEffect[0] != '\0' )
		{	
			EmitSound( pszSoundEffect );
		}
	}

	SetContextThink( &CBaseGrenade::SUB_Remove, gpGlobals->curtime, "RemoveThink" );
	SetTouch( NULL );

	AddEffects( EF_NODRAW );
	SetAbsVelocity( vec3_origin );
}

//-----------------------------------------------------------------------------
// THROWABLE REPEL
//-----------------------------------------------------------------------------
void CTFProjectile_ThrowableRepel::OnHit( CBaseEntity *pOther ) 
{ 
	if ( m_bHit )
		return;

	CTFPlayer *pPlayer = dynamic_cast< CTFPlayer*>( pOther );

	if ( pPlayer && !pPlayer->InSameTeam( GetThrower() ) )
	{
		CTraceFilterIgnoreTeammates tracefilter( this, COLLISION_GROUP_NONE, GetTeamNumber() );
		trace_t trace;
		UTIL_TraceLine( GetAbsOrigin(), pPlayer->GetAbsOrigin(), ( MASK_SHOT & ~( CONTENTS_HITBOX ) ), &tracefilter, &trace );

		// Apply AirBlast Force
		Vector vecToTarget;
		vecToTarget = pPlayer->GetAbsOrigin() - this->GetAbsOrigin();
		vecToTarget.z = 0;
		VectorNormalize( vecToTarget );

		// Quick Fix Uber is immune
		if ( pPlayer->m_Shared.InCond( TF_COND_MEGAHEAL )) 
			return;

		float flForce = 300.0f * m_flChargePercent + 350.0f;
		pPlayer->ApplyAirBlastImpulse( vecToTarget * flForce + Vector( 0, 0, flForce ) );
		pPlayer->ApplyPunchImpulseX( RandomInt( -50, -30 ) );

		// Apply Damage to Victim
		CTakeDamageInfo info;
		info.SetAttacker( GetThrower() );
		info.SetInflictor( this ); 
		info.SetWeapon( GetLauncher() );
		info.SetDamage( GetDamage() );
		info.SetDamageCustom( GetCustomDamageType() );
		info.SetDamagePosition( this->GetAbsOrigin() );
		info.SetDamageType( DMG_CLUB | DMG_PREVENT_PHYSICS_FORCE );

		//Vector dir;
		//AngleVectors( GetAbsAngles(), &dir );

		pPlayer->DispatchTraceAttack( info, vecToTarget, &trace );
		ApplyMultiDamage();
	}

	BaseClass::OnHit( pOther );
}
//-----------------------------------------------------------------------------
// THROWABLE BRICK
//-----------------------------------------------------------------------------
void CTFProjectile_ThrowableBrick::OnHit( CBaseEntity *pOther ) 
{ 
	if ( m_bHit )
		return;

	CTFPlayer *pPlayer = dynamic_cast< CTFPlayer*>( pOther );

	if ( pPlayer && !pPlayer->InSameTeam( GetThrower() ) )
	{
		CTraceFilterIgnoreTeammates tracefilter( this, COLLISION_GROUP_NONE, GetTeamNumber() );
		trace_t trace;
		UTIL_TraceLine( GetAbsOrigin(), pPlayer->GetAbsOrigin(), ( MASK_SHOT & ~( CONTENTS_HITBOX ) ), &tracefilter, &trace );

		Vector vecToTarget;
		vecToTarget = pPlayer->WorldSpaceCenter() - this->WorldSpaceCenter();
		VectorNormalize( vecToTarget );

		// Apply Damage to Victim
		CTakeDamageInfo info;
		info.SetAttacker( GetThrower() );
		info.SetInflictor( this ); 
		info.SetWeapon( GetLauncher() );
		info.SetDamage( GetDamage() );
		info.SetDamageCustom( GetCustomDamageType() );
		info.SetDamagePosition( GetAbsOrigin() );
		info.SetDamageType( DMG_CLUB );

		pPlayer->DispatchTraceAttack( info, vecToTarget, &trace );
		pPlayer->ApplyPunchImpulseX( RandomInt( 15, 20 ) );
		ApplyMultiDamage();
	}

	BaseClass::OnHit( pOther );
}
//-----------------------------------------------------------------------------
// THROWABLE BREADMONSTER
//-----------------------------------------------------------------------------
void CTFProjectile_ThrowableBreadMonster::OnHit( CBaseEntity *pOther ) 
{ 
	if ( m_bHit )
		return;

	CTFPlayer *pVictim = dynamic_cast< CTFPlayer*>( pOther );
	CTFPlayer *pOwner = dynamic_cast< CTFPlayer*>( GetThrower() );

	if ( pVictim && pOwner && !pVictim->InSameTeam( pOwner ) )
	{
		m_bHit = true;
		CTraceFilterIgnoreTeammates tracefilter( this, COLLISION_GROUP_NONE, GetTeamNumber() );
		trace_t trace;
		Vector vEndPos = pVictim->WorldSpaceCenter();
		vEndPos.z = WorldSpaceCenter().z + 1.0f;
		UTIL_TraceLine( WorldSpaceCenter(), vEndPos, CONTENTS_HITBOX|CONTENTS_MONSTER|CONTENTS_SOLID, &tracefilter, &trace );

		Vector vecToTarget;
		vecToTarget = pVictim->WorldSpaceCenter() - this->WorldSpaceCenter();
		VectorNormalize( vecToTarget );

		// Apply Damage to Victim
		CTakeDamageInfo info;
		info.SetAttacker( GetThrower() );
		info.SetInflictor( this );
		info.SetWeapon( GetLauncher() );
		info.SetDamage( GetDamage() );
		info.SetDamageCustom( GetCustomDamageType() );
		info.SetDamagePosition( GetAbsOrigin() );
		
		int iDamageType = DMG_CLUB;
		if ( IsCritical() )
		{
			iDamageType |= DMG_CRITICAL;
		}
		info.SetDamageType( iDamageType );

		pVictim->DispatchTraceAttack( info, vecToTarget, &trace );
		pVictim->ApplyPunchImpulseX( RandomInt( 15, 20 ) );
		pVictim->m_Shared.MakeBleed( pOwner, dynamic_cast< CTFWeaponBase * >( GetLauncher() ), 5.0f, 1.0f );
		ApplyMultiDamage();

		// Bread Particle
		CPVSFilter filter( vEndPos );
		TE_TFParticleEffect( filter, 0.0, "blood_bread_biting", vEndPos, vec3_angle );

		// Attach Breadmonster to Victim
		CreateStickyAttachmentToTarget( pOwner, pVictim, &trace );

		BaseClass::Explode();
		return;
	}
	else // its a dud
	{
		BaseClass::Explode();
		return;
	}
	BaseClass::OnHit( pOther );
}

//-----------------------------------------------------------------------------
void CTFProjectile_ThrowableBreadMonster::Detonate() 
{
	SetContextThink( &CBaseGrenade::SUB_Remove, gpGlobals->curtime, "RemoveThink" );
	SetTouch( NULL );

	AddEffects( EF_NODRAW );
	SetAbsVelocity( vec3_origin );
}

//-----------------------------------------------------------------------------
void CTFProjectile_ThrowableBreadMonster::Explode( trace_t *pTrace, int bitsDamageType )
{
	if ( !m_bHit )
	{
		// TODO, Spawn Debris / Flopping BreadInstead
		trace_t tr;
		Vector velDir = m_vCollisionVelocity;
		VectorNormalize( velDir );
		Vector vecSpot = GetAbsOrigin() - velDir * 32;
		UTIL_TraceLine( vecSpot, vecSpot + velDir * 64, MASK_SOLID, this, COLLISION_GROUP_DEBRIS, &tr );
		if ( tr.fraction < 1.0 && tr.surface.flags & SURF_SKY )
		{
			// We hit the skybox, go away soon.
			return;
		}

		// Create a breadmonster in the world
		CEffectData	data;
		data.m_vOrigin = tr.endpos;
		data.m_vNormal = velDir;
		data.m_nEntIndex = 0;
		data.m_nAttachmentIndex = 0;
		data.m_nMaterial = 0;
		data.m_fFlags = TF_PROJECTILE_BREAD_MONSTER;
		data.m_nColor = ( GetTeamNumber() == TF_TEAM_BLUE ) ? 1 : 0;

		DispatchEffect( "TFBoltImpact", data );
	}

	BaseClass::Explode( pTrace, bitsDamageType );
}

#endif // GAME_DLL
//
//#ifdef CLIENT_DLL
//
//static CUtlMap< const char*, CUtlString > s_TeamParticleMap;
//static bool s_TeamParticleMapInited = false;
//
////-----------------------------------------------------------------------------
//const char *CTFProjectile_Throwable::GetTrailParticleName( void )
//{
//	// Check for Particles
//	int iDynamicParticleEffect = 0;
//	CALL_ATTRIB_HOOK_INT_ON_OTHER( GetLauncher(), iDynamicParticleEffect, set_attached_particle );
//	if ( iDynamicParticleEffect > 0 )
//	{
//		// Init Map Once
//		if ( !s_TeamParticleMapInited )
//		{
//			SetDefLessFunc( s_TeamParticleMap );
//			s_TeamParticleMapInited = true;
//		}
//
//		attachedparticlesystem_t *pParticleSystem = GetItemSchema()->GetAttributeControlledParticleSystem( iDynamicParticleEffect );
//		if ( pParticleSystem )
//		{
//			// TF Team Color Particles
//			const char * pName = pParticleSystem->pszSystemName;
//			if ( GetTeamNumber() == TF_TEAM_BLUE && V_stristr( pName, "_teamcolor_red" ))
//			{
//				int index = s_TeamParticleMap.Find( pName );
//				if ( !s_TeamParticleMap.IsValidIndex( index ) )
//				{
//					char pBlue[256];
//					V_StrSubst( pName, "_teamcolor_red", "_teamcolor_blue", pBlue, 256 );
//					CUtlString pBlueString( pBlue );
//					index = s_TeamParticleMap.Insert( pName, pBlueString );
//				}
//				return s_TeamParticleMap[index].String();
//			}
//			else if ( GetTeamNumber() == TF_TEAM_RED && V_stristr( pParticleSystem->pszSystemName, "_teamcolor_blue" ))
//			{
//				// Guard against accidentally giving out the blue team color (support tool)
//				int index = s_TeamParticleMap.Find( pName );
//				if ( !s_TeamParticleMap.IsValidIndex( index ) )
//				{
//					char pRed[256];
//					V_StrSubst( pName, "_teamcolor_blue", "_teamcolor_red", pRed, 256 );
//					CUtlString pRedString( pRed );
//					index = s_TeamParticleMap.Insert( pName, pRedString );
//				}
//				return s_TeamParticleMap[index].String();
//			}
//
//			return pName;
//		}
//	}
//
//	if ( GetTeamNumber() == TF_TEAM_BLUE )
//	{
//		return "trail_basic_blue";
//	}
//	else
//	{
//		return "trail_basic_red";
//	}
//}
//
//#endif // CLIENT_DLL

//----------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef STAGING_ONLY
#ifdef GAME_DLL


void CTFProjectile_ThrowableTargetDummy::Explode()
{
	CTFPlayer *pPlayer = ToTFPlayer( GetThrower() );
	if ( !pPlayer )
		return;

	CTFTargetDummy::Create( GetAbsOrigin(), GetAbsAngles(), pPlayer );
	BaseClass::Explode();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_ConcGrenade::Detonate( )
{
	Explode();
}
//-----------------------------------------------------------------------------
void CTFProjectile_ConcGrenade::Misfire( )
{
	CTFPlayer *pPlayer = ToTFPlayer( GetThrower() );
	if ( pPlayer )
	{
		SetAbsOrigin( pPlayer->GetAbsOrigin() );
	}
	
	Explode();
}
//-----------------------------------------------------------------------------
void CTFProjectile_ConcGrenade::Explode( )
{
	// Apply pushback
	const float flMaxForce = 900.f;
	const float flMaxSelfForce = 800.f;
	const int nMaxEnts = MAX_PLAYERS;
	CBaseEntity	*pObjects[ nMaxEnts ];
	int nCount = UTIL_EntitiesInSphere( pObjects, nMaxEnts, GetAbsOrigin(), GetDamageRadius(), FL_CLIENT );
	CTFPlayer *pThrower = ToTFPlayer( GetThrower() );

	for ( int i = 0; i < nCount; i++ )
	{
		if ( !pObjects[i] )
			continue;

		if ( !pObjects[i]->IsAlive() )
			continue;

		// Only affect the thrower from same team
		if ( InSameTeam( pObjects[i] ) && pObjects[i] != pThrower )
			continue;

		if ( !FVisible( pObjects[i], MASK_OPAQUE ) )
			continue;

		if ( !pObjects[i]->IsPlayer() )
			continue;

		CTFPlayer *pTFPlayer = static_cast< CTFPlayer* >( pObjects[i] );
		if ( !pTFPlayer )
			continue;

		// Conc does more force the further away you are from the blast radius
		Vector vecPushDir = pTFPlayer->GetAbsOrigin() - GetAbsOrigin();
		float flForce = RemapVal( vecPushDir.Length(), 0, GetDamageRadius(), 0, flMaxForce );
 		
 		if ( flForce < 250.0f && pObjects[i] == pThrower ) // Hold case
		{
			AngularImpulse ang;
			pTFPlayer->GetVelocity( &vecPushDir, &ang );
			flForce = flMaxSelfForce;
		}
		VectorNormalize( vecPushDir );
		vecPushDir.z *= 1.5f;
		pTFPlayer->ApplyAirBlastImpulse( vecPushDir * flForce );
		pTFPlayer->ApplyPunchImpulseX( RandomInt( -50, -30 ) );

// 		if (  pObjects[i] == pThrower )
// 		{
// 			// Apply 'Bonk' lines to make target more visible for 2 seconds
// 			pThrower->m_Shared.AddCond( TF_COND_SELF_CONC, 2 );
// 		}
	}

	BaseClass::Explode();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_TeleportGrenade::Spawn( void )
{
	BaseClass::Spawn();

	SetContextThink( &CTFProjectile_TeleportGrenade::RecordPosThink, gpGlobals->curtime + 0.05f, "RecordThink" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_TeleportGrenade::RecordPosThink( void )
{
	m_vecTrailingPos.AddToTail( GetAbsOrigin() );

	// Only retain 5 positions
	if ( m_vecTrailingPos.Count() > 5 )
	{
		m_vecTrailingPos.Remove( 0 );
	}

	SetContextThink( &CTFProjectile_TeleportGrenade::RecordPosThink, gpGlobals->curtime + 0.05f, "RecordThink" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_TeleportGrenade::Explode( trace_t *pTrace, int bitsDamageType )
{
	CTFPlayer *pThrower = ToTFPlayer( GetThrower() );
	if ( !pThrower || !pThrower->IsAlive() )
		return;

	trace_t traceHull;
	CTraceFilterIgnoreTeammates traceFilter( this, COLLISION_GROUP_PLAYER_MOVEMENT, GetTeamNumber() );
	unsigned int nMask = pThrower->GetTeamNumber() == TF_TEAM_RED ? CONTENTS_BLUETEAM : CONTENTS_REDTEAM;
	nMask |= MASK_PLAYERSOLID;

	// Try a few spots
	FOR_EACH_VEC_BACK( m_vecTrailingPos, i )
	{
		// Try positions starting with the current, and moving back in time a bit
		Vector vecStart = m_vecTrailingPos[i];
		UTIL_TraceHull( vecStart, vecStart, VEC_HULL_MIN, VEC_HULL_MAX, nMask, &traceFilter, &traceHull );

		if ( !traceHull.DidHit() )
		{
			// Place a teleport effect where they came from
			const Vector& vecOrigin = pThrower->GetAbsOrigin();
			CPVSFilter pvsFilter( vecOrigin );
			TE_TFParticleEffect( pvsFilter, 0.f, GetExplodeEffectParticle(), vecOrigin, vec3_angle );

			// Move 'em!
			pThrower->Teleport( &vecStart, &pThrower->GetAbsAngles(), NULL );

			// Do a zoom effect
			pThrower->SetFOV( pThrower, 0.f, 0.3f, 120.f );

			// Screen flash
			color32 fadeColor = { 255, 255, 255, 100 };
			UTIL_ScreenFade( pThrower, fadeColor, 0.25f, 0.4f, FFADE_IN );

			if ( TFGameRules() )
			{
				TFGameRules()->HaveAllPlayersSpeakConceptIfAllowed( MP_CONCEPT_PLAYER_SPELL_TELEPORT, ( pThrower->GetTeamNumber() == TF_TEAM_RED ) ? TF_TEAM_BLUE : TF_TEAM_RED );
			}
		}
	}

	BaseClass::Explode( pTrace, bitsDamageType );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_GravityGrenade::Spawn( void )
{
	BaseClass::Spawn();

	m_flStartTime = -1.f;
	m_flNextPulseEffectTime = -1.f;
	m_bHitWorld = false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_GravityGrenade::TrapThink( void )
{
	if ( gpGlobals->curtime > m_flStartTime + 5.f )
	{
		SetContextThink( &CBaseGrenade::SUB_Remove, gpGlobals->curtime, "RemoveThink" );
		SetTouch( NULL );

		AddEffects( EF_NODRAW );
		SetAbsVelocity( vec3_origin );
		return;
	}

	PulseTrap();

	SetContextThink( &CTFProjectile_GravityGrenade::TrapThink, gpGlobals->curtime + 0.15f, "TrapThink" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_GravityGrenade::OnHitWorld( void )
{
	if ( !m_bHitWorld )
	{
		SetDetonateTimerLength( FLT_MAX );

		m_bHitWorld = true;
		m_flStartTime = gpGlobals->curtime;

		AddSolidFlags( FSOLID_TRIGGER );
		SetTouch( NULL );

		SetContextThink( &CTFProjectile_GravityGrenade::TrapThink, gpGlobals->curtime, "TrapThink" );
	}

	BaseClass::OnHitWorld();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_GravityGrenade::PulseTrap( void )
{
	const int nMaxEnts = 32;

	Vector vecPos = GetAbsOrigin();
	CBaseEntity	*pObjects[ nMaxEnts ];
	int nCount = UTIL_EntitiesInSphere( pObjects, nMaxEnts, vecPos, GetDamageRadius(), FL_CLIENT );

	// Iterate through sphere's contents
	for ( int i = 0; i < nCount; i++ )
	{
		CBaseCombatCharacter *pEntity = pObjects[i]->MyCombatCharacterPointer();
		if ( !pEntity )
			continue;

		if ( InSameTeam( pEntity ) )
			continue;

		if ( !FVisible( pEntity, MASK_OPAQUE ) )
			continue;

 		// Draw player toward us
		Vector vecSourcePos = pEntity->GetAbsOrigin();
		Vector vecTargetPos = GetAbsOrigin();
		Vector vecVelocity = ( vecTargetPos - vecSourcePos ) * 2.f;
		vecVelocity.z += 50.f;

		if ( pEntity->GetFlags() & FL_ONGROUND )
		{
			vecVelocity.z += 150.f;
			pEntity->SetGroundEntity( NULL );
			pEntity->SetGroundChangeTime( gpGlobals->curtime + 0.5f );
		}

		pEntity->Teleport( NULL, NULL, &vecVelocity );
	}

	// NDebugOverlay::Sphere( vecPos, GetDamageRadius(), 0, 255, 0, false, 0.35f );

	PulseEffects();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CTFProjectile_GravityGrenade::PulseEffects( void )
{
	if ( gpGlobals->curtime < m_flNextPulseEffectTime )
		return;

	Vector vecOrigin = GetAbsOrigin();
	CPVSFilter filter( vecOrigin );
	TE_TFParticleEffect( filter, 0.f, "Explosion_ShockWave_01", vecOrigin, vec3_angle );
	EmitSound( filter, entindex(), "Weapon_Upgrade.ExplosiveHeadshot" );

	m_flNextPulseEffectTime = gpGlobals->curtime + 1.f;
}

//-----------------------------------------------------------------------------
// THROWABLE KNIFE
//-----------------------------------------------------------------------------
Vector CTFProjectile_ThrowingKnife::GetVelocityVector( const Vector &vecForward, const Vector &vecRight, const Vector &vecUp, float flCharge )
{
	// Scale the projectile speed up to a maximum of 3000?
	float flSpeed = RemapVal( flCharge, 0, 1.0f, GetProjectileSpeed(), GetProjectileMaxSpeed() );

	return ( ( flSpeed * vecForward ) + 
			 ( flSpeed * 0.1 * vecUp ) );
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void CTFProjectile_ThrowingKnife::OnHit( CBaseEntity *pOther )
{
	if ( m_bHit )
		return;

	CTFPlayer *pVictim = dynamic_cast< CTFPlayer*>( pOther );
	CTFPlayer *pOwner = dynamic_cast< CTFPlayer*>( GetThrower() );

	if ( pVictim && pOwner && !pVictim->InSameTeam( pOwner ) )
	{
		CTraceFilterIgnoreTeammates tracefilter( this, COLLISION_GROUP_NONE, GetTeamNumber() );
		trace_t trace;
		UTIL_TraceLine( GetAbsOrigin(), pVictim->GetAbsOrigin(), ( MASK_SHOT & ~( CONTENTS_HITBOX ) ), &tracefilter, &trace );

		Vector entForward; 
		AngleVectors( pVictim->EyeAngles(), &entForward );
		Vector toEnt = pVictim->GetAbsOrigin() - this->GetAbsOrigin();
		toEnt.z = 0;
		entForward.z = 0;
		toEnt.NormalizeInPlace();
		entForward.NormalizeInPlace();

		// Is from behind?
		bool bIsFromBehind = DotProduct( toEnt, entForward ) > 0.7071f;

		// Apply Damage to Victim
		CTakeDamageInfo info;
		info.SetAttacker( GetThrower() );
		info.SetInflictor( this );
		info.SetWeapon( GetLauncher() );
		info.SetDamageCustom( GetCustomDamageType() );
		info.SetDamagePosition( GetAbsOrigin() );
		
		int iDamageType = DMG_CLUB;
		if ( bIsFromBehind )
		{
			iDamageType |= DMG_CRITICAL;
		}
		info.SetDamageType( iDamageType );
		info.SetDamage( bIsFromBehind ? GetBackHitDamage() : GetDamage() );

		pVictim->DispatchTraceAttack( info, toEnt, &trace );
		ApplyMultiDamage();

		CreateStickyAttachmentToTarget( pOwner, pVictim, &trace );

		Explode();
		return;
	}
	else // its a dud, mark as hit and let it roll around
	{
		m_bHit = true;
	}
	BaseClass::OnHit( pOther );
}

//-----------------------------------------------------------------------------
void CTFProjectile_ThrowingKnife::Detonate()
{
	SetContextThink( &CBaseGrenade::SUB_Remove, gpGlobals->curtime, "RemoveThink" );
	SetTouch( NULL );

	AddEffects( EF_NODRAW );
	SetAbsVelocity( vec3_origin );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_SmokeGrenade::Spawn( void )
{
	BaseClass::Spawn();

	m_flStartTime = -1.f;
	m_bHitWorld = false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_SmokeGrenade::OnHitWorld( void )
{
	if ( !m_bHitWorld )
	{
		SetDetonateTimerLength( FLT_MAX );
		// VPhysicsGetObject()->EnableMotion( false );

		m_bHitWorld = true;
		m_flStartTime = gpGlobals->curtime;

		const char *pszSoundEffect = GetExplodeEffectSound();
		if ( pszSoundEffect && pszSoundEffect[0] != '\0' )
		{	
			EmitSound( pszSoundEffect );
		}

		AddSolidFlags( FSOLID_TRIGGER );
		SetTouch( NULL );

		SetContextThink( &CTFProjectile_SmokeGrenade::SmokeThink, gpGlobals->curtime, "SmokeThink" );
	}

	BaseClass::OnHitWorld();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_SmokeGrenade::SmokeThink( void )
{
	if ( gpGlobals->curtime > m_flStartTime + 6.f )
	{
		SetContextThink( &CBaseGrenade::SUB_Remove, gpGlobals->curtime, "RemoveThink" );

		AddEffects( EF_NODRAW );
		SetAbsVelocity( vec3_origin );
		return;
	}

	CPVSFilter filter( GetAbsOrigin() );
	TE_TFParticleEffect( filter, 0.f, "grenade_smoke_cycle", GetAbsOrigin(), vec3_angle );

	const int nMaxEnts = 32;

	Vector vecPos = GetAbsOrigin();
	CBaseEntity	*pObjects[ nMaxEnts ];
	int nCount = UTIL_EntitiesInSphere( pObjects, nMaxEnts, vecPos, GetDamageRadius(), FL_CLIENT );

	// Iterate through sphere's contents
	for ( int i = 0; i < nCount; i++ )
	{
		CBaseCombatCharacter *pEntity = pObjects[i]->MyCombatCharacterPointer();
		if ( !pEntity )
			continue;

		if ( !InSameTeam( pEntity ) )
			continue;

		if ( !FVisible( pEntity, MASK_OPAQUE ) )
			continue;

		if ( !pEntity->IsPlayer() )
			continue;

		CTFPlayer *pTFPlayer = ToTFPlayer( pEntity );
		if ( !pTFPlayer )
			continue;

		pTFPlayer->m_Shared.AddCond( TF_COND_OBSCURED_SMOKE, 0.5f );
	}

	// NDebugOverlay::Sphere( vecPos, GetDamageRadius(), 0, 255, 0, false, 0.35f );

	SetContextThink( &CTFProjectile_SmokeGrenade::SmokeThink, gpGlobals->curtime + 0.3f, "SmokeThink" );
}
#endif // GAME_DLL
#endif // STAGING_ONLY
