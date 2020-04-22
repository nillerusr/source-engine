//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "tf_weapon_mechanical_arm.h"
#include "in_buttons.h"

#if !defined( CLIENT_DLL )
#include "tf_player.h"
#include "tf_gamestats.h"
#include "ilagcompensationmanager.h"
#include "particle_parse.h"
#include "tf_fx.h"
#include "tf_weapon_grenade_pipebomb.h"
#include "tf_team.h"
#include "tf_passtime_logic.h"
#else
#include "c_tf_player.h"
#endif


//=============================================================================
//
// tables.
//

IMPLEMENT_NETWORKCLASS_ALIASED( TFMechanicalArm, DT_TFMechanicalArm )

BEGIN_NETWORK_TABLE( CTFMechanicalArm, DT_TFMechanicalArm )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFMechanicalArm )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_mechanical_arm, CTFMechanicalArm );
PRECACHE_WEAPON_REGISTER( tf_weapon_mechanical_arm );

#define		AMMO_BASE_PROJECTILE_SHOCK		10
#define		AMMO_PER_PROJECTILE_SHOCK		5

//=============================================================================
//
// CTFMechanicalArm
//

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFMechanicalArm::CTFMechanicalArm()
{
#ifdef CLIENT_DLL
	m_pParticleBeamEffect = NULL;
	m_pParticleBeamSpark = NULL;
	m_pEffectOwner = NULL;
#endif // CLIENT_DLL
}

CTFMechanicalArm::~CTFMechanicalArm()
{
#ifdef CLIENT_DLL
	if ( m_pEffectOwner )
	{
		if ( m_pParticleBeamEffect )
		{
			m_pEffectOwner->ParticleProp()->StopEmissionAndDestroyImmediately( m_pParticleBeamEffect );
			m_pParticleBeamEffect = NULL;
		}

		if ( m_pParticleBeamSpark )
		{
			m_pEffectOwner->ParticleProp()->StopEmissionAndDestroyImmediately( m_pParticleBeamSpark );
			m_pParticleBeamSpark = NULL;
		}

		m_pEffectOwner = NULL;
	}
#endif // CLIENT_DLL
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFMechanicalArm::Precache()
{
	BaseClass::Precache();

	PrecacheParticleSystem( "dxhr_arm_muzzleflash" );
	PrecacheParticleSystem( "dxhr_arm_muzzleflash2" );
	PrecacheParticleSystem( "dxhr_arm_impact" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFMechanicalArm::ShockAttack( void )
{
	CTFPlayer *pOwner = ToTFPlayer( GetPlayerOwner() );
	if ( !pOwner )
		return false;

	if ( pOwner->GetWaterLevel() == WL_Eyes )
		return false;

	// Enough ammo to shock at least one target?
	if ( pOwner->GetAmmoCount( m_iPrimaryAmmoType ) < ( AMMO_BASE_PROJECTILE_SHOCK + AMMO_PER_PROJECTILE_SHOCK ) )
		return false;

#ifdef GAME_DLL
	if ( pOwner->m_Shared.IsStealthed() )
	{
		pOwner->RemoveInvisibility();
	}

	lagcompensation->StartLagCompensation( pOwner, pOwner->GetCurrentCommand() );

	Vector vecEye = pOwner->EyePosition();
	Vector vecForward, vecRight, vecUp;
	AngleVectors( pOwner->EyeAngles(), &vecForward, &vecRight, &vecUp );
	Vector vecSize = Vector( 128, 128, 64 );
	float flMaxElement = 0.0f;
	for ( int i = 0; i < 3; ++i )
	{
		flMaxElement = MAX( flMaxElement, vecSize[i] );
	}
	Vector vecCenter = vecEye + vecForward * flMaxElement;

	CUtlVector< CBaseEntity* > vecShockTargets;

	// Get a list of entities in the box defined by vecSize at VecCenter.
	// We will then try to deflect everything in the box.
	const int maxCollectedEntities = 64;
	CBaseEntity	*pObjects[ maxCollectedEntities ];
	int count = UTIL_EntitiesInBox( pObjects, maxCollectedEntities, vecCenter - vecSize, vecCenter + vecSize, FL_GRENADE | FL_CLIENT | FL_FAKECLIENT );
	for ( int i = 0; i < count; i++ )
	{
		if ( IsValidVictim( pOwner, pObjects[i] ) )
		{
			vecShockTargets.AddToTail( pObjects[i] );
		}
	}

	// Remove the base cost for attempting to fire, regardless of what we hit
	pOwner->RemoveAmmo( AMMO_BASE_PROJECTILE_SHOCK, m_iPrimaryAmmoType );

	// We attacked by didn't hit anything
	if ( vecShockTargets.Count() == 0 )
	{
		lagcompensation->FinishLagCompensation( pOwner );
		return true;
	}

	// Horribly hacked muzzle position
	Vector vForward, vRight, vUp;
	AngleVectors( pOwner->EyeAngles(), &vForward, &vRight, &vUp );

	Vector vStart = pOwner->EyePosition()
		+ (vForward * 40.0f)
		+ (vRight * 15.0f)
		+ (vUp * -10.0f);

	FOR_EACH_VEC( vecShockTargets, i  )
	{
		if ( pOwner->GetAmmoCount( m_iPrimaryAmmoType ) < AMMO_PER_PROJECTILE_SHOCK )
			break;

		CBaseEntity* pTarget = vecShockTargets[i];
		Vector vecTarget = pTarget->WorldSpaceCenter();

		ShockVictim( pOwner, pTarget );
		pOwner->RemoveAmmo( AMMO_PER_PROJECTILE_SHOCK, m_iPrimaryAmmoType );

		// Play an effect where the target is			
		CPVSFilter filter( vecTarget );
		const char *shootsound = GetShootSound( SPECIAL3 );
		if ( shootsound && *shootsound )
		{
			EmitSound( shootsound );
		}

		// play the electrical effect from the gun to the pipes
		te_tf_particle_effects_control_point_t controlPoint = { PATTACH_WORLDORIGIN, vecTarget };
 		TE_TFParticleEffectComplex( filter, 0.0f, "dxhr_arm_muzzleflash", vStart, QAngle( 0, 0, 0 ), NULL, &controlPoint, pTarget, PATTACH_CUSTOMORIGIN );
	}

	lagcompensation->FinishLagCompensation( pOwner );
#endif

	return true;
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
bool CTFMechanicalArm::IsValidVictim( CTFPlayer *pOwner, CBaseEntity *pTarget )
{
	if ( pTarget == NULL )
		return false;

	if ( pTarget == pOwner )
		return false;

	if ( pTarget->IsPlayer() && pTarget->GetTeamNumber() == TEAM_SPECTATOR )
		return false;

	if ( pTarget->GetTeamNumber() == pOwner->GetTeamNumber() )
		return false;

	if ( pTarget->IsPlayer() && !pTarget->IsAlive() )
		return false;

	if ( !pTarget->IsDeflectable() && !FClassnameIs( pTarget, "prop_physics" ) )
		return false;

	if ( pOwner->FVisible( pTarget, MASK_SOLID_BRUSHONLY ) == false )
		return false;

	if ( g_pPasstimeLogic && ( g_pPasstimeLogic->GetBall() == pTarget ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
void CTFMechanicalArm::ShockVictim( CTFPlayer *pOwner, CBaseEntity *pTarget )
{
	// Projectile
	if ( !pTarget->IsPlayer() )
	{
		pTarget->SetThink( &BaseClass::SUB_Remove );
		pTarget->SetNextThink( gpGlobals->curtime );
		pTarget->SetTouch( NULL );
		pTarget->AddEffects( EF_NODRAW );
		pTarget->RemoveFlag( FL_GRENADE );
	}
	
	// deal damage
	CTakeDamageInfo info;
	info.SetDamageType( DMG_SHOCK );
	info.SetAttacker( pOwner );
	info.SetInflictor( this );
	info.SetWeapon( this );
	info.SetDamage( 20 );
	info.SetDamagePosition( pTarget->WorldSpaceCenter() );
	pTarget->TakeDamage( info );

	// Achievement
	CTFGrenadePipebombProjectile *pPipebomb = dynamic_cast<CTFGrenadePipebombProjectile*>( pTarget );
	if ( pPipebomb && pPipebomb->HasStickyEffects() )
	{
		// If we are near a building, award achievement progress.
		CTFTeam *pTeam = pOwner->GetTFTeam();
		if ( pTeam )
		{
			for ( int j = 0; j < pTeam->GetNumObjects(); j++ )
			{
				CBaseObject *pTemp = pTeam->GetObject( j );
				if ( pTemp && ( pTemp->ObjectType() != OBJ_ATTACHMENT_SAPPER ) )
				{
					if ( ( pTemp->GetAbsOrigin().DistTo( pPipebomb->GetAbsOrigin() ) < 100 ) &&
						( pTemp->FVisible( pPipebomb, MASK_SOLID_BRUSHONLY ) ) )
					{
						pOwner->AwardAchievement( ACHIEVEMENT_TF_ENGINEER_DESTROY_STICKIES, 1 );
						break; // Only one award per sticky.
					}
				}
			}
		}
	}
}
#endif

//-----------------------------------------------------------------------------
void CTFMechanicalArm::SecondaryAttack( void )
{
	CTFPlayer *pOwner = ToTFPlayer( GetPlayerOwner() );
	if ( !pOwner )
		return;

	// Are we capable of firing again?
	if ( m_flNextSecondaryAttack > gpGlobals->curtime )
		return;

	if ( m_iPrimaryAmmoType == TF_AMMO_METAL )
	{
		if ( ( GetAmmoPerShot() > GetOwner()->GetAmmoCount( m_iPrimaryAmmoType ) ) ||
			( pOwner->GetWaterLevel() == WL_Eyes ) )
		{
			WeaponSound( EMPTY );
			m_flNextSecondaryAttack = gpGlobals->curtime + 0.67f;
			return;
		}
	}

	if ( !CanAttack() )
		return;

	if ( ShockAttack() )
	{
		WeaponSound( SPECIAL3 );
	}
	else
	{
		WeaponSound( EMPTY );
	}

#ifdef CLIENT_DLL
	// Play an effect on the client so they have some kind of visual feedback that something happened
	int iParticleAttachment = LookupAttachment( "muzzle" );
	CNewParticleEffect* pEffect = ParticleProp()->Create( "dxhr_sniper_fizzle", PATTACH_POINT_FOLLOW, iParticleAttachment );
	ParticleProp()->AddControlPoint( pEffect, 1, this, PATTACH_POINT_FOLLOW, "muzzle" );
#endif

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	pOwner->SetAnimation( PLAYER_ATTACK1 );

	m_flNextPrimaryAttack = gpGlobals->curtime + 0.67f;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.67f;
}

#ifdef CLIENT_DLL
void CTFMechanicalArm::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	UpdateParticleBeam();
}

//-----------------------------------------------------------------------------
void CTFMechanicalArm::StopParticleBeam( void )
{
	if ( !m_pEffectOwner )
		return;

	// Different owners, kill the old effect
	if ( m_pParticleBeamEffect )
	{
		m_pEffectOwner->ParticleProp()->StopEmissionAndDestroyImmediately( m_pParticleBeamEffect );
		m_pParticleBeamEffect = NULL;
	}

	if ( m_pParticleBeamSpark )
	{
		m_pEffectOwner->ParticleProp()->StopEmissionAndDestroyImmediately( m_pParticleBeamSpark );
		m_pParticleBeamSpark = NULL;
	}

	m_pEffectOwner = NULL;
}

//-----------------------------------------------------------------------------
void CTFMechanicalArm::UpdateParticleBeam()
{
	// Update Particle
	// If we are attacking, update the particle (make it render)
	CTFPlayer *pFiringPlayer = ToTFPlayer( GetOwnerEntity() );
	if ( !pFiringPlayer )
	{
		StopParticleBeam();
		return;
	}

	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	C_BaseEntity *pEffectOwner = this;
	if ( pLocalPlayer == pFiringPlayer )
	{
		pEffectOwner = pLocalPlayer->GetRenderedWeaponModel();
		if ( !pEffectOwner )
		{
			StopParticleBeam();
			return;
		}
	}

	if ( m_pEffectOwner && m_pEffectOwner != pEffectOwner )
	{
		StopParticleBeam();
		return;
	}

	if ( m_flNextSecondaryAttack > gpGlobals->curtime )
	{
		StopParticleBeam();
		return;
	}

	m_pEffectOwner = pEffectOwner;

	// Constantly perform the shock attack and update control points if attack is down and we've already fired
	if ( pFiringPlayer 
		&& pFiringPlayer->m_nButtons & IN_ATTACK 
		&& pFiringPlayer->GetActiveWeapon() == this 
		&& pFiringPlayer->GetWaterLevel() != WL_Eyes
		&& pFiringPlayer->m_flNextAttack < gpGlobals->curtime )
	{
		trace_t tr;
		Vector vecAiming;
		pFiringPlayer->EyeVectors( &vecAiming );
		Vector vecEnd = pFiringPlayer->EyePosition() + vecAiming * 256.0f;
		UTIL_TraceLine( pFiringPlayer->EyePosition(), vecEnd, ( MASK_SHOT & ~CONTENTS_HITBOX ), pFiringPlayer, DMG_GENERIC, &tr );

		// Line laser
		if ( !m_pParticleBeamEffect )
		{
			const char *pszEffectName = "dxhr_arm_muzzleflash2";
			m_pParticleBeamEffect = pEffectOwner->ParticleProp()->Create( pszEffectName, PATTACH_POINT_FOLLOW, "muzzle" );
		}
		if ( m_pParticleBeamEffect )
		{
			Vector vEndPos = tr.endpos - pFiringPlayer->GetAbsOrigin();
			pEffectOwner->ParticleProp()->AddControlPoint( m_pParticleBeamEffect, 1, pFiringPlayer, PATTACH_ABSORIGIN_FOLLOW, NULL, vEndPos );
		}

		// Spark
		if ( !m_pParticleBeamSpark && tr.m_pEnt && tr.m_pEnt->IsPlayer( ) )
		{
			m_pParticleBeamSpark = pEffectOwner->ParticleProp( )->Create( "dxhr_arm_impact", PATTACH_ABSORIGIN_FOLLOW );
		}
		else if ( m_pParticleBeamSpark && (!tr.m_pEnt || !tr.m_pEnt->IsPlayer() ) )
		{
			m_pEffectOwner->ParticleProp()->StopEmissionAndDestroyImmediately( m_pParticleBeamSpark );
			m_pParticleBeamSpark = NULL;
		}

		if ( m_pParticleBeamSpark )
		{
			Vector vEndPos = tr.endpos - pFiringPlayer->GetAbsOrigin();
			pEffectOwner->ParticleProp( )->AddControlPoint( m_pParticleBeamSpark, 1, pFiringPlayer, PATTACH_ABSORIGIN_FOLLOW, NULL, vEndPos );
		}
	}
	else if ( m_pEffectOwner )
	{
		StopParticleBeam( );
	}
}
#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
void CTFMechanicalArm::PrimaryAttack()
{
	CTFPlayer *pOwner = ToTFPlayer( GetPlayerOwner() );
	if ( !pOwner )
		return;

	float flFireDelay = ApplyFireDelay( m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_flTimeFireDelay );

	if ( m_iPrimaryAmmoType == TF_AMMO_METAL )
	{
		if ( ( GetAmmoPerShot() > GetOwner()->GetAmmoCount( m_iPrimaryAmmoType ) ) ||
			( pOwner->GetWaterLevel( ) == WL_Eyes ) )
		{
			WeaponSound( EMPTY );
			m_flNextPrimaryAttack = gpGlobals->curtime + flFireDelay;
			return;
		}
	}

	// Are we capable of firing again?
	if ( m_flNextPrimaryAttack > gpGlobals->curtime )
		return;

	// Get the player owning the weapon.
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

	if ( !CanAttack() )
		return;

#ifdef GAME_DLL
	CTF_GameStats.Event_PlayerFiredWeapon( pPlayer, false );

	int iAmmoPerShot = 0;
	CALL_ATTRIB_HOOK_INT( iAmmoPerShot, mod_ammo_per_shot );
	pOwner->RemoveAmmo( iAmmoPerShot, m_iPrimaryAmmoType );
	
	//int nAmmoToTake = bShocked ? 0 : GetAmmoPerShot();
	//pOwner->RemoveAmmo( nAmmoToTake, m_iPrimaryAmmoType );

	FireProjectile( pPlayer );
#endif

#ifdef CLIENT_DLL
	// Play an effect on the client so they have some kind of visual feedback that something happened
	int iParticleAttachment = LookupAttachment( "muzzle" );
	CNewParticleEffect* pEffect = ParticleProp()->Create( "dxhr_sniper_fizzle", PATTACH_POINT_FOLLOW, iParticleAttachment );
	ParticleProp()->AddControlPoint( pEffect, 1, this,  PATTACH_POINT_FOLLOW, "muzzle" );
#endif

	WeaponSound( SINGLE );

	// Set the weapon mode.
	m_iWeaponMode = TF_WEAPON_PRIMARY_MODE;

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	// Set next attack times.
	m_flNextPrimaryAttack = gpGlobals->curtime + flFireDelay;

	SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );
}

//-----------------------------------------------------------------------------
int CTFMechanicalArm::GetAmmoPerShot( void )
{
	// Used by normal fire code, we only decrement ammo on ticks which uses an Attr
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFMechanicalArm::UpdateBodygroups( CBaseCombatCharacter* pOwner, int iState )
{
	if ( !pOwner )
		return false;

	iState = pOwner->GetActiveWeapon() == this;

	bool res = BaseClass::UpdateBodygroups( pOwner, iState );

	CTFPlayer *pTFOwner = ToTFPlayer( pOwner );
	if ( pTFOwner )
	{
		CBaseViewModel *pVM = pTFOwner->GetViewModel();
		if ( pVM )
		{
			pVM->SetBodygroup( 1, iState );
		}
	}

	return res;
}