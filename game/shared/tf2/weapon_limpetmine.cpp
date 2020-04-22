//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetfplayer_shared.h"
#include "in_buttons.h"
#include "tf_shareddefs.h"
#include "basegrenade_shared.h"
#include "basetfcombatweapon_shared.h"
#include "weapon_limpetmine.h"
#include "IEffects.h"
#include "engine/IEngineSound.h"
#include "weapon_grenade_rocket.h"
#include "beam_shared.h"
#include "tf_gamerules.h"

#if defined( CLIENT_DLL )
#else
#include "grenade_limpetmine.h"
#include "tf_obj_sentrygun.h"
#include "ai_network.h"
#include "tf_team.h"
#endif

// Damage CVars
ConVar	weapon_laserdesignator_range( "weapon_laserdesignator_range","2048", FCVAR_REPLICATED, "Laser Designator maximum range" );
ConVar	weapon_limpetmine_max_deployed( "weapon_limpetmine_max_deployed","0", FCVAR_REPLICATED, "Maximum number of Limpet Mines that can be deployed at a single time" );
ConVar	weapon_limpetmine_max_distance_off_trace( "weapon_limpetmine_max_distance_off_trace","25", FCVAR_REPLICATED, "Maximum distance off of the trace that a limpet can be." );

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponLimpetmine, DT_WeaponLimpetmine )

BEGIN_NETWORK_TABLE( CWeaponLimpetmine, DT_WeaponLimpetmine )
#if !defined( CLIENT_DLL )
	SendPropInt( SENDINFO( m_iDeployedLimpets ), 5, SPROP_UNSIGNED ),
	SendPropEHandle( SENDINFO( m_hDesignatedEntity ) ),
#else
	RecvPropInt( RECVINFO( m_iDeployedLimpets ) ),
	RecvPropEHandle( RECVINFO(m_hDesignatedEntity) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponLimpetmine  )

	DEFINE_PRED_FIELD( m_iDeployedLimpets, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_hDesignatedEntity, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),

END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_limpetmine, CWeaponLimpetmine );
PRECACHE_WEAPON_REGISTER(weapon_limpetmine);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponLimpetmine::CWeaponLimpetmine( void )
{
	SetPredictionEligible( true );

	m_hDesignatedEntity = NULL;
	m_flNextBuzzTime = 0;
	m_iDeployedLimpets = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponLimpetmine::Precache( void )
{
	BaseClass::Precache();

#ifndef CLIENT_DLL
	UTIL_PrecacheOther( "grenade_limpetmine" );
#endif

	PrecacheScriptSound( "WeaponLimpetmine.Deny" );

	PrecacheModel( "sprites/laserbeam.vmt" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponLimpetmine::UpdateOnRemove( void )
{
#ifndef CLIENT_DLL
	RemoveDeployedLimpets();

	if ( m_hLaserDesignation )
	{
		UTIL_Remove( m_hLaserDesignation );
	}
#endif

	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponLimpetmine::CleanupOnActStart( void )
{
#ifndef CLIENT_DLL
	RemoveDeployedLimpets();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponLimpetmine::GetFireRate( void )
{	
	return 0.5; 
}

//-----------------------------------------------------------------------------
// Purpose: Limpetmine considers itself as having ammo at all times, because it contains the designator
//-----------------------------------------------------------------------------
bool CWeaponLimpetmine::HasAnyAmmo( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Stop thinking and holster
//-----------------------------------------------------------------------------
bool CWeaponLimpetmine::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	StopDesignating();
	return BaseClass::Holster(pSwitchingTo);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponLimpetmine::ItemPostFrame( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

#ifndef CLIENT_DLL
	// If we don't have a laser designator yet, create one
	if ( !m_hLaserDesignation )
	{
		m_hLaserDesignation = CEnvLaserDesignation::Create( pPlayer ); 
	}
#endif

	// Is the player trying to designate?
	if ( pPlayer->m_nButtons & IN_ATTACK )
	{
		if ( m_bDesignating )
		{
#ifndef CLIENT_DLL
			ActivateBeam();
#endif
		}
		else if ( m_flNextPrimaryAttack <= gpGlobals->curtime )
		{
			StartDesignating();
#ifndef CLIENT_DLL
			ActivateBeam();
#endif
		}
	}
	else if ( m_bDesignating )
	{
		StopDesignating();
	}
	else if ( (pPlayer->m_nButtons & IN_ATTACK2) && !m_flStartedLaunchingAt && (m_flNextPrimaryAttack <= gpGlobals->curtime) )
	{
		m_flStartedLaunchingAt = gpGlobals->curtime;

		SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	}
	else if ( (pPlayer->m_afButtonReleased & IN_ATTACK2) && (m_flNextPrimaryAttack <= gpGlobals->curtime) && m_flStartedLaunchingAt )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime;
		PrimaryAttack();
		m_flStartedLaunchingAt = 0;
	}

	UpdateBeamTarget();

	// Always idle
	WeaponIdle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponLimpetmine::PrimaryAttack( void )
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ( !pPlayer )
		return;

	// Don't allow anymore limpets to be placed if we've reached the max
	if ( m_iDeployedLimpets >= weapon_limpetmine_max_deployed.GetInt() )
	{
#ifdef CLIENT_DLL
		// We should flash the limpet mine count on the weapon at this point
		CLocalPlayerFilter filter;
		EmitSound( filter, entindex(), "WeaponLimpetmine.Deny" );
		m_flNextBuzzTime = gpGlobals->curtime + 0.5f;	// only buzz every so often.
#endif
		return;
	}

	if ( IsOwnerEMPed() )
		return;

	trace_t tr;
	// Get an aim vector. Don't use GetAimVector() because we don't want autoaiming.
	Vector vecSrc = pPlayer->Weapon_ShootPosition( );
	Vector vecAiming;
	pPlayer->EyeVectors( &vecAiming );

	// Calculate launch velocity (3 seconds for max distance)
	float flThrowTime = MIN( (gpGlobals->curtime - m_flStartedLaunchingAt), 3.0 );
	float flSpeed = 600 + (300 * flThrowTime);
	vecAiming *= flSpeed;

	WeaponSound( WPN_DOUBLE );
#ifndef CLIENT_DLL
	ThrowLimpet( pPlayer, vecSrc, vecAiming );
#endif
	SendWeaponAnim( ACT_VM_SECONDARYATTACK );

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_flNextSecondaryAttack = gpGlobals->curtime + GetFireRate();

	pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );
}

#if !defined( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponLimpetmine::ThrowLimpet( CBasePlayer *pPlayer, Vector vecSrc, Vector vecAiming )
{
	CLimpetMine *pLimpet = CLimpetMine::Create(vecSrc, vecAiming, pPlayer);
	pLimpet->SetLauncher( this );

	// Increase the limpet count
	m_iDeployedLimpets += 1;
}

//-----------------------------------------------------------------------------
// Purpose: Detonates all deployed limpets for this weapon
//-----------------------------------------------------------------------------
void CWeaponLimpetmine::DetonateDeployedLimpets( void )
{
	CBaseEntity *pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "grenade_limpetmine" )) != NULL)
	{
		CLimpetMine* pLimpet = (CLimpetMine*)pEntity;
		if ( pLimpet->IsLive() && pLimpet->GetThrower() == GetOwner() )
		{
			pLimpet->Use( GetOwner(), this, USE_ON, 0 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Quietly removes all deployed limpets for this weapon
//-----------------------------------------------------------------------------
void CWeaponLimpetmine::RemoveDeployedLimpets( void )
{
	CBaseEntity *pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "grenade_limpetmine" )) != NULL)
	{
		CLimpetMine* pLimpet = (CLimpetMine*)pEntity;
		if ( pLimpet->GetThrower() == GetOwner() )
		{
			pLimpet->Use( GetOwner(), this, USE_SET, 0 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponLimpetmine::ActivateBeam( void )
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ( !pPlayer )
		return;

	Assert( m_pBeam );
	if( m_hDesignatedEntity != NULL )
	{
		CBaseEntity* pEntity = m_hDesignatedEntity.Get();

		// If we hit a player, and it's an enemy, tell my sentryguns to attack it
		if ( pEntity->IsPlayer() && !pPlayer->InSameTeam(pEntity) )
		{
			DesignateSentriesToAttack( pEntity );
		}
		else if ( pEntity->GetFlags() & FL_NPC )
		{
			// If it's an enemy NPC, tell my sentryguns to attack it
			if ( !pPlayer->InSameTeam( pEntity ) )
			{
				DesignateSentriesToAttack( pEntity );
			}
		}
		else
		{
			// Is it a sentrygun? If so, tell it to toggle it's sunken state.
			if ( pEntity->Classify() == CLASS_MILITARY )
			{
				CObjectSentrygun *pSentry = dynamic_cast<CObjectSentrygun *>(pEntity);
				if ( pSentry && pSentry->GetBuilder() == pPlayer )
				{
					pSentry->ToggleTurtle();
					return;
				}
				else 
				{
					// If it's an enemy object, tell my sentryguns to attack it
					if ( !pPlayer->InSameTeam( pEntity ) )
					{
						DesignateSentriesToAttack( pEntity );
						return;
					}
				}
			}

			// Is it a limpet mine? If so, detonate it
			CLimpetMine *pLimpet = dynamic_cast<CLimpetMine *>(pEntity);
			if ( pLimpet && pLimpet->IsLive() && pLimpet->GetThrower() == pPlayer )
			{
				pLimpet->Use( pPlayer, this, USE_ON, 0 );
			}

		}
	} 
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the specified entity is a valid one for designation
//-----------------------------------------------------------------------------
bool CWeaponLimpetmine::ValidDesignationTarget( CBaseEntity *pEntity )
{
	if( !pEntity )
		return false;

	// Ignore the world
	if ((!pEntity) || ( pEntity->entindex() == 0 ))
		return false;

	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	Assert(pPlayer);
	if(!pPlayer) 
		return false;

	// Ignore players on my team
	if ( pEntity->IsPlayer() )
	{
		if( pEntity->InSameTeam(GetOwner()) )
			return false;
		else
			return true;
	}

	// can either target my own sentry guns to turtle, or enemy buildings to target for sentries:
	if ( pEntity->Classify() == CLASS_MILITARY )
	{
		// Is it a sentry gun I built?
		if ( dynamic_cast<CObjectSentrygun *>(pEntity) && ((CObjectSentrygun*)pEntity)->GetBuilder() == pPlayer )
		{
			return true;
		}
		// Is it an enemy object?
		else if ( !pPlayer->InSameTeam( pEntity ) )
		{
			return true;
		}
	}

	// My limpet mines are valid
	CLimpetMine *pLimpet = dynamic_cast<CLimpetMine *>(pEntity);
	if ( pLimpet && pLimpet->IsLive() && pLimpet->GetThrower() == GetOwner() )
		return true;

	// NPCs are valid
	if ( pEntity->GetFlags() & FL_NPC )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Set the player's sentryguns to all attack the specified target
//-----------------------------------------------------------------------------
void CWeaponLimpetmine::DesignateSentriesToAttack( CBaseEntity *pEntity )
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ( !pPlayer )
		return;

	// Tell all this player's sentryguns
	for ( int i = 0; i < pPlayer->GetObjectCount(); i++ )
	{
		CBaseObject *pObj = pPlayer->GetObject(i);
		if ( !pObj )
			continue;

		if ( pObj->IsSentrygun() )
		{
			CObjectSentrygun *pSentry = static_cast<CObjectSentrygun *>(pObj);
			pSentry->DesignateTarget( pEntity );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity* CWeaponLimpetmine::GetDesignatedEntity( CBaseTFPlayer *pPlayer )
{
	// Construct the start and end vectors.
	Vector vecSrc = pPlayer->Weapon_ShootPosition( );
	Vector vecAiming;
	pPlayer->EyeVectors( &vecAiming );
	Vector vecEnd = vecSrc + vecAiming * weapon_laserdesignator_range.GetFloat();

	float fBestDistanceSq = FLT_MAX;
	CBaseEntity* pBestEntity = NULL;
	float fMaxDist = weapon_limpetmine_max_distance_off_trace.GetFloat();
	float fMaxDistSq = fMaxDist * fMaxDist;

	// Check all limpets against a cylinder:
	if( CLimpetMine::allLimpets )
	{
		for ( CLimpetMine *pLimpet = CLimpetMine::allLimpets ;pLimpet; pLimpet = pLimpet->nextLimpet)
		{
			// Find the closest limpet to the center of the cylinder:
			if( pLimpet->IsLive() && pLimpet->GetThrower() != NULL && pLimpet->GetThrower() == pPlayer )
			{
				Vector vecNearestPoint = PointOnLineNearestPoint( vecSrc, vecEnd, pLimpet->GetAbsOrigin() );

				float fDistSq = ( pLimpet->GetAbsOrigin() - vecNearestPoint ).LengthSqr();
				
				if (( fDistSq > fMaxDistSq ) || ( fDistSq >= fBestDistanceSq ))
					continue;

				if (TFGameRules()->IsBlockedByEnemyShields( vecSrc, pLimpet->GetAbsOrigin(), GetOwner()->GetTeamNumber() ))
					continue;

				pBestEntity = pLimpet;
				fBestDistanceSq = fDistSq;
			}
		}

		if( pBestEntity )
			return pBestEntity;
	}

	// Check to see if we are designating a combat object:
	for ( int i = 0; i < pPlayer->GetObjectCount(); i++ )
	{
		CBaseObject *pObj = pPlayer->GetObject(i);
		if ( !pObj || !pObj->IsSentrygun() )
			continue;

		Vector vecObjCenter = pObj->WorldSpaceCenter();
		Vector vecNearestPoint = PointOnLineNearestPoint( vecSrc, vecEnd, vecObjCenter );

		float fDistSq = ( vecObjCenter - vecNearestPoint ).LengthSqr();
		
		if (( fDistSq > fMaxDistSq ) || ( fDistSq >= fBestDistanceSq ))
			continue;

		if (TFGameRules()->IsBlockedByEnemyShields( vecSrc, vecObjCenter, GetOwner()->GetTeamNumber() ))
			continue;

		pBestEntity = pObj;
		fBestDistanceSq = fDistSq;
	}

	// Valid or NULL
	return pBestEntity;
}

#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponLimpetmine::WeaponIdle( void )
{
	if ( HasWeaponIdleTimeElapsed() )
	{
		if ( m_bDesignating || m_flStartedLaunchingAt )
		{
			SendWeaponAnim( ACT_VM_PRIMARYATTACK );
		}
		else
		{
			SendWeaponAnim( ACT_VM_IDLE );
		}
		SetWeaponIdleTime( gpGlobals->curtime + 0.2 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Start designating with the laser
//-----------------------------------------------------------------------------
void CWeaponLimpetmine::StartDesignating( void )
{
	m_bDesignating = true;

	// Add myself to the designated target list
	Vector vecOrigin(0,0,0);

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

#ifndef CLIENT_DLL
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ( !pPlayer )
		return;

	if ( !m_pBeam )
	{
		m_pBeam = CBeam::BeamCreate( "sprites/laserbeam.vmt", 5 );

		m_pBeam->PointEntInit( vec3_origin, this );
		m_pBeam->SetEndAttachment( 1 );
		m_pBeam->SetColor( 255, 32, 32 );
		m_pBeam->SetBrightness( 255 );
		m_pBeam->SetNoise( 0 );
		m_pBeam->SetWidth( 0.5 );
		m_pBeam->SetEndWidth( 0.5 );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Stop designating with the laser
//-----------------------------------------------------------------------------
void CWeaponLimpetmine::StopDesignating( void )
{
	SendWeaponAnim( ACT_VM_IDLE );

	m_bDesignating = false;

#ifndef CLIENT_DLL
	if ( m_pBeam )
	{
		UTIL_Remove( m_pBeam );
		m_pBeam = NULL;
	}

	if ( m_hLaserDesignation )
	{
		m_hLaserDesignation->SetActive( false );
	}

	// Remove any designated target:
	m_hDesignatedEntity.Set( NULL );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Update the beam position and do designator functions
//-----------------------------------------------------------------------------
void CWeaponLimpetmine::UpdateBeamTarget()
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ( !pPlayer )
		return;

	// "Fire" the designator beam
	Vector vecSrc = pPlayer->Weapon_ShootPosition( );
	Vector vecAiming;
	pPlayer->EyeVectors( &vecAiming );
	Vector vecEnd = vecSrc + vecAiming * weapon_laserdesignator_range.GetFloat();

	trace_t tr;
	TFGameRules()->WeaponTraceLine(vecSrc, vecEnd, MASK_SHOT, pPlayer, DMG_PROBE, &tr);

	// Only update our designated target point if we hit something
#ifndef CLIENT_DLL
	if ( tr.fraction != 1.0 && m_bDesignating )
	{
		m_hLaserDesignation->SetActive( true );
		m_hLaserDesignation->SetAbsOrigin( tr.endpos );
	}
	else
	{
		m_hLaserDesignation->SetActive( false );
	}

	// Update beam visual
	if( m_pBeam )
	{
		m_pBeam->SetStartPos( tr.endpos );
		m_pBeam->RelinkBeam();
	}

	CBaseEntity* pEntity= NULL;

	// Perform designator functions
	// Did we hit something?
	//pEntity = CBaseEntity::Instance( tr.u.ent );

	// If we hit a target we don't care about, try and grab the nearest valid target to our crosshair
	//if ( !ValidDesignationTarget( pEntity ) )
	{
		// Get the designated entity, ignoring trace information:
		pEntity = GetDesignatedEntity( pPlayer );
		
		if( !ValidDesignationTarget( pEntity ) )
		{
			m_hDesignatedEntity.Set( NULL );
			return;
		}
	}

	// We have a valid entity, light it up.
	m_hDesignatedEntity.Set( pEntity );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponLimpetmine::DecrementLimpets( void )
{
#ifndef CLIENT_DLL
	if ( m_iDeployedLimpets )
	{
		m_iDeployedLimpets -= 1;
	}
#endif
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: Limpet mine's always selectable, because the laser designator can always fire
//-----------------------------------------------------------------------------
bool C_WeaponLimpetmine::CanBeSelected( void )
{
	return true;
}

void C_WeaponLimpetmine::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate( updateType );

	m_hOldDesignatedEntity = m_hDesignatedEntity;
}

void C_WeaponLimpetmine::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	if ( GetOwner() == C_BaseTFPlayer::GetLocalPlayer() )
	{
		// Old target isn't valid anymore, so reset it's render properties
		if ( m_hOldDesignatedEntity.Get() && m_hOldDesignatedEntity != m_hDesignatedEntity )
		{
			m_hOldDesignatedEntity->m_nRenderFX = m_eDesignatedEntityOriginalRenderFx;
			m_hOldDesignatedEntity->SetRenderMode( (RenderMode_t)m_eDesignatedEntityOriginalRenderMode );
		}

		// New target? Set it's render properties
		if ( m_hDesignatedEntity != NULL && m_hOldDesignatedEntity != m_hDesignatedEntity )
		{
			// Store off original info for designated entity
			m_eDesignatedEntityOriginalRenderFx		= m_hDesignatedEntity->m_nRenderFX;
			m_eDesignatedEntityOriginalRenderMode	= m_hDesignatedEntity->GetRenderMode();

			// Set up alpha blended pulsefast renderer
			m_hDesignatedEntity->m_nRenderFX	= kRenderFxPulseFastWider;
			m_hDesignatedEntity->SetRenderMode( kRenderTransAlpha );
		}
	}
}

#endif