//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:			The Medic's Medikit weapon
//					
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetfplayer_shared.h"
#include "in_buttons.h"
#include "weapon_combatshield.h"
#include "engine/IEngineSound.h"
#include "grenade_base_empable.h"
#include "basetfvehicle.h"
#include "tf_gamerules.h"

//#define REPAIR_GUN_DISABLES_GRENADES	// Uncomment if you want the repair gun to disable grenades.

#if defined( CLIENT_DLL )
#include "particles_simple.h"
#else
#include "ndebugoverlay.h"
#endif

#include "weapon_repairgun.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Buff ranges
ConVar weapon_repairgun_target_range( "weapon_repairgun_target_range", "450", FCVAR_REPLICATED, "The farthest away you can be for the repair gun to initially lock onto a player." );
ConVar weapon_repairgun_stick_range( "weapon_repairgun_stick_range", "512", FCVAR_REPLICATED, "How far away the repair gun can stay locked onto someone." );
ConVar weapon_repairgun_rate( "weapon_repairgun_rate", "12", FCVAR_REPLICATED, "Health healed per second by the repair gun." );
ConVar weapon_repairgun_damage_modifier( "weapon_repairgun_damage_modifier", "1.5", FCVAR_REPLICATED, "Scales the damage a player does while being healed with the repair gun." );
ConVar weapon_repairgun_debug( "weapon_repairgun_debug", "0", FCVAR_REPLICATED, "Show debugging info for the repair gun." );
ConVar weapon_repairgun_construction_rate( "weapon_repairgun_construction_rate", "10", FCVAR_REPLICATED, "Constructing object health healed per second by the repair gun." );

LINK_ENTITY_TO_CLASS( weapon_repairgun, CWeaponRepairGun );

PRECACHE_WEAPON_REGISTER( weapon_repairgun );

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponRepairGun, DT_WeaponRepairGun )

BEGIN_NETWORK_TABLE( CWeaponRepairGun, DT_WeaponRepairGun )
#if !defined( CLIENT_DLL )
	SendPropInt( SENDINFO( m_bHealing ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bAttacking ), 1, SPROP_UNSIGNED ),
	SendPropEHandle( SENDINFO( m_hHealingTarget ) ),
#else
	RecvPropInt( RECVINFO( m_bAttacking ) ),
	RecvPropInt( RECVINFO( m_bHealing ) ),
	RecvPropEHandle( RECVINFO(m_hHealingTarget) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponRepairGun  )

	DEFINE_PRED_FIELD( m_bHealing, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bAttacking, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_hHealingTarget, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),

	DEFINE_FIELD( m_flHealEffectLifetime, FIELD_FLOAT ),

//	DEFINE_PRED_FIELD( m_pEmitter, FIELD_POINTER ),
//	DEFINE_PRED_FIELD( m_hParticleMaterial, FIELD_???,  ),
//	DEFINE_PRED_FIELD( m_PathParticleEvent, FIELD_???,  ),
//	DEFINE_PRED_FIELD( m_bPlayingSound, FIELD_BOOLEAN ),

END_PREDICTION_DATA()

#define PARTICLE_PATH_VEL				140.0
#define NUM_PATH_PARTICLES_PER_SEC		600.0f
#define NUM_TARGET_PARTICLES_PER_SEC	720.0f

#define NUM_REPAIRGUN_PATH_POINTS		8

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponRepairGun::CWeaponRepairGun( void )
{
	m_flHealEffectLifetime = 0;

	m_bHealing = false;
	m_bAttacking = false;

	m_flNextBuzzTime = 0;

#if defined( CLIENT_DLL )
	m_pEmitter = NULL;
	m_hParticleMaterial = INVALID_MATERIAL_HANDLE;
	m_PathParticleEvent.Init( NUM_PATH_PARTICLES_PER_SEC );
	m_bPlayingSound = false;
#endif

	SetPredictionEligible( true );
}


void CWeaponRepairGun::Precache()
{
	BaseClass::Precache();
	PrecacheScriptSound( "WeaponRepairGun.NoTarget" );
	PrecacheScriptSound( "WeaponRepairGun.Healing" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponRepairGun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	RemoveHealingTarget();
	m_bAttacking = false;

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponRepairGun::GetTargetRange( void )
{
	return weapon_repairgun_target_range.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponRepairGun::GetStickRange( void )
{
	return weapon_repairgun_stick_range.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponRepairGun::GetHealRate( void )
{
	return weapon_repairgun_rate.GetFloat();
}

// Now make sure there isn't something other than team players in the way.
class CRepairFilter : public CTraceFilterSimple
{
public:
	CRepairFilter( CBaseEntity *pShooter ) : CTraceFilterSimple( pShooter, TFCOLLISION_GROUP_WEAPON )
	{
		m_pShooter = pShooter;
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		// If it hit an edict the isn't the target and is on our team, then the ray is blocked.
		CBaseEntity *pEnt = static_cast<CBaseEntity*>(pHandleEntity);

		// Ignore collisions with the shooter
		if ( pEnt == m_pShooter )
			return false;
		
		// You can't heal a vehicle you are sitting in.

		if( ((CBaseTFPlayer*)m_pShooter)->IsInAVehicle() )
		{

			CBaseEntity* pVehicle = ((CBaseTFPlayer*)m_pShooter)->GetVehicle()->GetVehicleEnt();		
			if( pVehicle == pEnt )
			{
				return false;
			}
		}

#ifdef REPAIR_GUN_DISABLES_GRENADES 

			// Repairgun can also disable enemy grenades
			CBaseEMPableGrenade *pGrenade = dynamic_cast<CBaseEMPableGrenade*>(pEnt);

			// Ignore collisions with teammates, or friendly grenades
			if ( !pGrenade )
			{
				if ( pEnt->GetTeam() == m_pShooter->GetTeam() )
					return false;
			}
#else
			if ( pEnt->GetTeam() == m_pShooter->GetTeam() )
				return false;
#endif

		return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
	}

	CBaseEntity	*m_pShooter;
};

//-----------------------------------------------------------------------------
// Purpose: Vehicle checking to see if we should switch targets from a player 
//			to a vehicle, or vice versa.
//-----------------------------------------------------------------------------
CBaseEntity *CWeaponRepairGun::CheckVehicleTargets( CBaseEntity *pCurHealing )
{
	// Unable to switch to/from players?
	if ( !TargetsPlayers() )
		return pCurHealing;

	CBaseTFVehicle *pTargetVehicle = NULL;

	// If we're a fully healed player sitting in a vehicle, see if the vehicle needs healing instead
	if ( pCurHealing->IsPlayer() && pCurHealing->GetHealth() >= pCurHealing->GetMaxHealth() )
	{
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)pCurHealing;
		if ( !pPlayer->IsInAVehicle() )
			return pCurHealing;

		pTargetVehicle = (CBaseTFVehicle*)(pPlayer->GetVehicle()->GetVehicleEnt());
		if ( pTargetVehicle->GetHealth() < pTargetVehicle->GetMaxHealth() )
			return pTargetVehicle;
	}
	else
	{
		// If the entity is a vehicle, and it's fully healed, heal any players in it instead
		pTargetVehicle = dynamic_cast<CBaseTFVehicle *>(pCurHealing);
	}

	// Is the vehicle fully healed?
	if ( pTargetVehicle && pTargetVehicle->GetHealth() >= pTargetVehicle->GetMaxHealth() )
	{
		CBaseTFPlayer *pUnhurtPlayer = NULL;
		CBaseTFPlayer *pHurtPlayer = NULL;

		// Go through all players in the vehicle
		int iMax = pTargetVehicle->GetMaxPassengerCount();
		for ( int i = 0; i < iMax; i++ )
		{
			CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)pTargetVehicle->GetPassenger(i);
			if ( pPlayer )
			{
				if ( pPlayer->GetHealth() < pPlayer->GetMaxHealth() )
				{
					pUnhurtPlayer = pPlayer;
				}
				else
				{
					pHurtPlayer = pPlayer;
				}
			}
		}

		// Heal hurt players first
		if ( pHurtPlayer )
			return pHurtPlayer;

		if ( pUnhurtPlayer )
			return pUnhurtPlayer;
	}

	return pCurHealing;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to a healable target
//-----------------------------------------------------------------------------
CBaseEntity *CWeaponRepairGun::GetTargetToHeal( CBaseEntity *pCurHealing )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return NULL;

	Vector vecSrc = pOwner->Weapon_ShootPosition( );
	

	// If we're already healing someone, stick onto them as long as possible.
	// Even if we can't heal them at the moment, lock onto them until they release the buttom.
	CBaseEntity* pTarget = pCurHealing;
	CBaseEntity* pVehicle = NULL;	// Vehicle the owner is in, or NULL.

	// You can't heal a vehicle you are sitting in, make sure we aren't healing a vehicle right after we've gotten in.
	if( ((CBaseTFPlayer*)pOwner)->IsInAVehicle() )
	{

		pVehicle = ((CBaseTFPlayer*)pOwner)->GetVehicle()->GetVehicleEnt();		
		if( pVehicle == pTarget )
		{
			pTarget = NULL;
		}
	}

	if ( pTarget && pTarget->IsAlive() && (pTarget->GetTeam() == pOwner->GetTeam()) )
	{
		// Make sure the guy didn't go out of range.
		Vector vecTargetPoint = pTarget->WorldSpaceCenter();
		Vector vecPoint;

		// If it's brush built, use absmins/absmaxs
		pTarget->CollisionProp()->CalcNearestPoint( vecSrc, &vecPoint );

#ifndef CLIENT_DLL
		//NDebugOverlay::Box( vecPoint, Vector(-2,-2,-2), Vector(2,2,2), 255,0,0, 8, 0.1 );
#endif

		float flDistance = (vecPoint - vecSrc).Length();
		if ( flDistance < GetStickRange() )
		{
			trace_t tr;
			CRepairFilter drainFilter( pOwner );
			UTIL_TraceLine( vecSrc, vecTargetPoint, MASK_SHOT, &drainFilter, &tr );
			
			if (( tr.fraction == 1.0f) || (tr.m_pEnt == pTarget))
				return CheckVehicleTargets( pTarget );
		}

		// Return null so we can't heal this player but m_hHealingPlayer stays set to them.
		return NULL;
	}
	else
	{	
		// Ok, try to find a new player to heal.
		// Get the target point and location
		Vector vecAiming;
		pOwner->EyeVectors( &vecAiming );

		// Find a player in range of this player, and make sure they're healable.
		Vector vecEnd = vecSrc + vecAiming * GetTargetRange();
		trace_t tr;

		// Use WeaponTraceLine so shields are tested...
		TFGameRules()->WeaponTraceLine( vecSrc, vecEnd, (MASK_SHOT & ~CONTENTS_HITBOX), pOwner, DMG_PROBE, &tr );

#ifndef CLIENT_DLL
		//NDebugOverlay::Box( vecSrc, Vector(-2,-2,-2), Vector(2,2,2), 192,192,0, 8, 10 );
		//NDebugOverlay::Box( vecEnd, Vector(-2,-2,-2), Vector(2,2,2), 0,255,0, 8, 10 );
		//NDebugOverlay::Box( tr.endpos, Vector(-2,-2,-2), Vector(2,2,2), 0,0,255, 8, 10 );
#endif
		
		if ( tr.fraction != 1.0 )
		{
			CBaseEntity *pEntity = tr.m_pEnt;
			if ( pEntity )
			{
				// Repairgun can also disable enemy grenades
#ifdef REPAIR_GUN_DISABLES_GRENADES 

				CBaseEMPableGrenade *pGrenade = dynamic_cast<CBaseEMPableGrenade*>(pEntity);
				if ( pGrenade && !pGrenade->InSameTeam( pOwner ) )
					return pGrenade;
#endif

				// Only target players if I'm allowed to
				if ( !TargetsPlayers() && pEntity->IsPlayer() )
					return NULL;

				// You can't heal a vehicle you are sitting in.
				if ( pVehicle && ( pVehicle == pEntity ) )
					return NULL;

				if ( (pEntity != pOwner) && pEntity->IsAlive() && pEntity->CanBePoweredUp() )
				{
					// Target needs to be on the same team
					if ( pEntity->InSameTeam( pOwner ) )
						return CheckVehicleTargets( pEntity );
				}
			}
		}

		if ( weapon_repairgun_debug.GetBool() )
		{
			ClientPrint( pOwner, HUD_PRINTCENTER, "REPAIRGUN: no target found\n" );
		}

		return NULL;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Overloaded to handle the hold-down healing
//-----------------------------------------------------------------------------
void CWeaponRepairGun::ItemPostFrame( void )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return;

#if !defined( CLIENT_DLL )
	if ( AppliesModifier() )
	{
		m_DamageModifier.SetModifier( weapon_repairgun_damage_modifier.GetFloat() );
	}
#endif

	// Try to start healing
	m_bAttacking = false;
	if ( (pOwner->m_nButtons & IN_ATTACK) && GetShieldState() == SS_DOWN )
	{
		PrimaryAttack();
		m_bAttacking = true;
	}
	else if ( GetCurHealingTarget() )
	{
		// Detach from the player if they release the attack button.
		RemoveHealingTarget();
	}

	// Prevent shield post frame if we're not ready to attack, or we're healing
	AllowShieldPostFrame( !m_bAttacking );

	WeaponIdle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponRepairGun::RemoveHealingTarget()
{
	// Stop the welding animation
	if ( m_bHealing )
	{
		SendWeaponAnim( ACT_FIRE_END );
	}

	m_hHealingTarget = NULL;
#if !defined( CLIENT_DLL )
	m_DamageModifier.RemoveModifier();
#endif
	m_bHealing = false;

	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( pOwner )
	{
		pOwner->SetIDEnt( NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponRepairGun::ComputeEMPFireState( void )
{
	if ( IsOwnerEMPed() )
	{
		// If we've just been EMPed, remove the heal target
		if ( GetCurHealingTarget() )
		{
			RemoveHealingTarget();
		}
		return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Attempt to heal any player within range of the medikit
//-----------------------------------------------------------------------------
void CWeaponRepairGun::PrimaryAttack( void )
{
	CBaseTFPlayer *pOwner = ToBaseTFPlayer( GetOwner() );
	if ( !pOwner )
		return;

	// Can't fire if we've been EMPed.
	if ( !ComputeEMPFireState() )
		return;

#if !defined( CLIENT_DLL )
	// Find a Player to buff with hitscan
	CBaseEntity *pCurHealing = GetCurHealingTarget();
	CBaseEntity *pTarget = GetTargetToHeal( pCurHealing );
	if ( pTarget )
	{
		// Start the welding animation
		if ( !m_bHealing )
		{
			SendWeaponAnim( ACT_FIRE_START );
		}

		// Tell the client who we're trying to heal.
		m_bHealing = true;
		m_hHealingTarget = pTarget;

		// Repairgun needs to EMP grenades, heal everything else
#ifdef REPAIR_GUN_DISABLES_GRENADES 

		CBaseEMPableGrenade *pGrenade = dynamic_cast<CBaseEMPableGrenade*>(pTarget);
		if ( pGrenade )
		{
			pTarget->TakeEMPDamage( 1.0 );
		}
		else
#endif
		{
			// Can't bring things back from the dead
			if ( pTarget->IsAlive() )
			{
				float flBoostAmount = GetHealRate() * gpGlobals->frametime;
				// If it's an object, and it's constructing, use the construction heal rate
				if ( pTarget->Classify() == CLASS_MILITARY )
				{
					CBaseObject *pObject = dynamic_cast<CBaseObject*>(pTarget);
					if ( pObject && pObject->IsBuilding() )
					{
						flBoostAmount = weapon_repairgun_construction_rate.GetFloat() * gpGlobals->frametime;
					}
					if ( pObject && pObject->IsDying() )
					{
						flBoostAmount = 0;
					}
				}

				// If we're not succeeding, remove the damage modifier
				if ( !pTarget->AttemptToPowerup( POWERUP_BOOST, 1.0, flBoostAmount, pOwner, AppliesModifier() ? &m_DamageModifier : NULL ) )
				{
					m_DamageModifier.RemoveModifier();
				}

				// Force the player's ID target to the heal target
				pOwner->SetIDEnt( pTarget );
			}
		}
	}
	else
	{
		RemoveHealingTarget();
	}
#endif

	CheckRemoveDisguise();
}

void CWeaponRepairGun::PlayAttackAnimation( int activity )
{
	SendWeaponAnim( activity );
}

//-----------------------------------------------------------------------------
// Purpose: Idle tests to see if we're facing a valid target for the medikit
//			If so, move into the "heal-able" animation. 
//			Otherwise, move into the "not-heal-able" animation.
//-----------------------------------------------------------------------------
void CWeaponRepairGun::WeaponIdle( void )
{
	if ( HasWeaponIdleTimeElapsed() )
	{
		// Loop the welding animation
		if ( m_bHealing )
		{
			SendWeaponAnim( ACT_FIRE_LOOP );
		}
		else
		{
			// select an idle animation
			SendWeaponAnim( ACT_VM_IDLE );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: The player holding this weapon has just gained new technology.
//			Check to see if it affects the medikit
//-----------------------------------------------------------------------------
void CWeaponRepairGun::GainedNewTechnology( CBaseTechnology *pTechnology )
{
}


#if defined( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponRepairGun::StopRepairSound( bool bStopHealingSound, bool bStopNoTargetSound )
{
	if ( bStopHealingSound )
	{
		StopSound( "WeaponRepairGun.Healing" );
	}

	if ( bStopNoTargetSound )
	{
		StopSound( "WeaponRepairGun.NoTarget" );
	}
}


void C_WeaponRepairGun::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	// Stop emitting particles if we're going dormant.
	if ( state == SHOULDTRANSMIT_END )
	{
		ClientThinkList()->SetNextClientThink( GetClientHandle(), CLIENT_THINK_NEVER );
	}

	BaseClass::NotifyShouldTransmit( state );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void C_WeaponRepairGun::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		m_pEmitter = CSimpleEmitter::Create( "C_WeaponRepairGun" );
		
		m_hParticleMaterial = m_pEmitter->GetPMaterial( "sprites/chargeball" );
	}

	// Think?
	if ( m_bHealing && m_hHealingTarget.Get())
	{
		ClientThinkList()->SetNextClientThink( GetClientHandle(), CLIENT_THINK_ALWAYS );
	}
	else
	{
		ClientThinkList()->SetNextClientThink( GetClientHandle(), CLIENT_THINK_NEVER );
		m_bPlayingSound = false;
		StopRepairSound( true, false );

		// Are they holding the attack button but not healing anyone? Give feedback.
		if ( IsActiveByLocalPlayer() && GetOwner() && GetOwner()->IsAlive() && m_bAttacking && GetOwner() == C_BasePlayer::GetLocalPlayer() )
		{
			if ( gpGlobals->curtime >= m_flNextBuzzTime )
			{
				CLocalPlayerFilter filter;
				EmitSound( filter, entindex(), "WeaponRepairGun.NoTarget" );
				m_flNextBuzzTime = gpGlobals->curtime + 0.5f;	// only buzz every so often.
			}
		}
		else
		{
			StopRepairSound( false, true );	// Stop the "no target" sound.
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_WeaponRepairGun::ClientThink()
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	if ( m_hHealingTarget == NULL )
	{
		return;
	}

	// Don't show it while the player is dead. Ideally, we'd respond to m_bHealing in OnDataChanged,
	// but it stops sending the weapon when it's holstered, and it gets holstered when the player dies.
	C_BasePlayer *pFiringPlayer = dynamic_cast< C_BasePlayer* >( GetOwner() );
	if ( !pFiringPlayer || pFiringPlayer->IsPlayerDead() )
	{
		ClientThinkList()->SetNextClientThink( GetClientHandle(), CLIENT_THINK_NEVER );
		m_bPlayingSound = false;
		StopRepairSound();
		return;
	}

	if ( !m_bPlayingSound )
	{
		m_bPlayingSound = true;
		CLocalPlayerFilter filter;
		EmitSound( filter, entindex(), "WeaponRepairGun.Healing" );
	}

	Vector points[NUM_REPAIRGUN_PATH_POINTS];

	// First generate a sequence of points so we can parameterize the (curvy) path from the
	// tip of the gun to the target.
	Vector vForward, vOrigin;
	QAngle vAngles;
	GetShootPosition( vOrigin, vAngles );

	AngleVectors( vAngles, &vForward );

	Vector vecTargetOrg = m_hHealingTarget->WorldSpaceCenter();
	
	Vector vDirTo = vecTargetOrg - vOrigin;
	float flDistanceTo = vDirTo.Length();
	vDirTo /= flDistanceTo;

	float flBendDist = flDistanceTo * 3;
	const Vector  A = vOrigin - vForward * flBendDist;
	const Vector &B = vOrigin;
	const Vector &C = vecTargetOrg;
	const Vector  D = vecTargetOrg - vForward * flBendDist;
	
	for ( int i=0; i < NUM_REPAIRGUN_PATH_POINTS; i++ )
	{
		Catmull_Rom_Spline( A, B, C, D, (float)i / (NUM_REPAIRGUN_PATH_POINTS-1), points[i] );
	}

	// Add random short-lived particles from the gun tip to the target.
	m_pEmitter->SetSortOrigin( (vecTargetOrg + pPlayer->GetAbsOrigin()) * 0.5f );

	float flCur = gpGlobals->frametime;
	while ( m_PathParticleEvent.NextEvent( flCur ) )
	{
		float t = RandomFloat( 0, 1 );
		int iPrev = (int)( t * (NUM_REPAIRGUN_PATH_POINTS - 1.001) );
		float tPrev = (float)iPrev / (NUM_REPAIRGUN_PATH_POINTS - 1);
		float tNext = (float)(iPrev+1) / (NUM_REPAIRGUN_PATH_POINTS - 1);
		Assert( tNext <= NUM_REPAIRGUN_PATH_POINTS-1 );

		Vector vPos;
		VectorLerp( points[iPrev], points[iPrev+1], (t-tPrev) / (tNext - tPrev), vPos );
		vPos += RandomVector( -3, 3 );
		
		SimpleParticle *pParticle = m_pEmitter->AddSimpleParticle( m_hParticleMaterial, vPos );
		if ( pParticle )
		{
			// Move the points along the path.
			pParticle->m_vecVelocity = points[iPrev+1] - points[iPrev];
			VectorNormalize( pParticle->m_vecVelocity );
			pParticle->m_vecVelocity *= PARTICLE_PATH_VEL;

			pParticle->m_flRoll = 0;
			pParticle->m_flRollDelta = 0;
			pParticle->m_flDieTime = 0.2f;
			pParticle->m_flLifetime = 0;
			pParticle->m_uchColor[1] = 200; 
			pParticle->m_uchColor[0] = pParticle->m_uchColor[2] = RandomInt( 0, 128 );
			pParticle->m_uchStartAlpha = 255;
			pParticle->m_uchEndAlpha = 0;
			pParticle->m_uchStartSize = 5;
			pParticle->m_uchEndSize = 3;
			pParticle->m_iFlags = 0;
		}
	}
}

#endif