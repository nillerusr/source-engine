//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "NPCEvent.h"
#include "tf_basecombatweapon.h"
#include "smoke_trail.h"
#include "tf_player.h"
#include "in_buttons.h"
#include "tf_gamerules.h"
#include "ammodef.h"
#include "IEffects.h"
#include "vstdlib/random.h"
#include "effects.h"
#include "baseviewmodel.h"
#include "basegrenade_shared.h"
#include "grenade_limpetmine.h"
#include "tf_obj_sentrygun.h"
#include "tf_obj_aerial_sentry_station.h"


// Damage CVars
ConVar	weapon_laserdesignator_range( "weapon_laserdesignator_range","1024", FCVAR_NONE, "Laser Designator maximum range" );

// ------------------------------------------------------------------------ //
// CWeaponLaserDesignator
// ------------------------------------------------------------------------ //
class CWeaponLaserDesignator : public CBaseTFCombatWeapon
{
	DECLARE_CLASS( CWeaponLaserDesignator, CBaseTFCombatWeapon );
public:	
	virtual void	Precache( void );
	virtual float	GetFireRate( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual bool	ComputeEMPFireState( void );
	virtual void	ItemPostFrame( void );
	virtual void	PrimaryAttack( void );

	bool	ValidDesignationTarget( CBaseEntity *pEntity );
	bool	TargetIsInLock( CBaseTFPlayer *pPlayer, CBaseEntity *pTarget, float *flMaxDot );

	// Designation
	void	StartDesignating( void );
	void	StopDesignating( void );
	void	UpdateBeam( void );
	void	DesignateSentriesToAttack( CBaseEntity *pEntity );

public:
	// Designation
	bool	m_bDesignating;

	// Beam
	CBeam	*m_pBeam;
};

LINK_ENTITY_TO_CLASS( weapon_laserdesignator, CWeaponLaserDesignator );
PRECACHE_WEAPON_REGISTER(weapon_laserdesignator);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponLaserDesignator::Precache( void )
{
	BaseClass::Precache();
	PrecacheModel( "sprites/laserbeam.vmt" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponLaserDesignator::GetFireRate()
{
	return 0.2;
}

//-----------------------------------------------------------------------------
// Purpose: Stop thinking and holster
//-----------------------------------------------------------------------------
bool CWeaponLaserDesignator::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	StopDesignating();
	return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponLaserDesignator::ComputeEMPFireState( void )
{
	if (IsOwnerEMPed())
	{
		// FIXME: Need a sound
		//UTIL_EmitSound( pPlayer->pev, CHAN_WEAPON, g_pszEMPGatlingFizzle, 1.0, ATTN_NORM );
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponLaserDesignator::ItemPostFrame( void )
{
	CBasePlayer *pPlayer = GetOwner();
	if (pPlayer == NULL)
		return;

	// Are we already welding?
	if ( m_bDesignating )
	{
		if ( pPlayer->m_nButtons & (IN_ATTACK | IN_ATTACK2) )
		{
			UpdateBeam();
		}
		else
		{
			StopDesignating();
		}
	}
	else
	{
		if ( (pPlayer->m_nButtons & IN_ATTACK) && (m_flNextPrimaryAttack <= gpGlobals->curtime) )
		{
			// Start designating
			PrimaryAttack();
			UpdateBeam();
		}
	}

	// Always idle
	WeaponIdle();
}

//-----------------------------------------------------------------------------
// Purpose: Start designating with the laser
//-----------------------------------------------------------------------------
void CWeaponLaserDesignator::StartDesignating( void )
{
	m_bDesignating = true;

	if ( !m_pBeam )
	{
		CBaseTFPlayer *pPlayer = ToBaseTFPlayer( m_hOwner );
		if ( !pPlayer )
			return;
		int iIndex = pPlayer->entindex();

		m_pBeam = CBeam::BeamCreate( "sprites/laserbeam.vmt", 1 );
		m_pBeam->PointEntInit( vec3_origin, iIndex );
		m_pBeam->SetEndAttachment( 1 );
		m_pBeam->SetColor( 255, 32, 32 );
		m_pBeam->SetBrightness( 255 );
		m_pBeam->SetNoise( 0 );
		m_pBeam->SetWidth( 2 );
		m_pBeam->SetEndWidth( 1 );
		m_pBeam->SetStartEntity( ENTINDEX(m_hOwner->edict()) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Stop designating with the laser
//-----------------------------------------------------------------------------
void CWeaponLaserDesignator::StopDesignating( void )
{
	m_bDesignating = false;
	if ( m_pBeam )
	{
		UTIL_Remove( m_pBeam );
		m_pBeam = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponLaserDesignator::PrimaryAttack( void )
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( m_hOwner );
	if ( !pPlayer )
		return;
	
	if ( !ComputeEMPFireState() )
		return;

	WeaponSound(SINGLE);
	PlayAttackAnimation( GetPrimaryAttackActivity() );
	pPlayer->AddEffects( EF_MUZZLEFLASH );

	StartDesignating();
}

//-----------------------------------------------------------------------------
// Purpose: Update the beam position and do designator functions
//-----------------------------------------------------------------------------
void CWeaponLaserDesignator::UpdateBeam( void )
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( m_hOwner );
	if ( !pPlayer )
		return;

	ASSERT( m_pBeam );

	// "Fire" the designator beam
	Vector vecSrc = pPlayer->Weapon_ShootPosition( pPlayer->GetOrigin() );
	Vector vecAiming;
	pPlayer->EyeVectors( &vecAiming );
	Vector vecEnd = vecSrc + vecAiming * weapon_laserdesignator_range.GetFloat();

	trace_t tr;
	TFGameRules()->WeaponTraceLine(vecSrc, vecEnd, MASK_SHOT, pPlayer, DMG_ENERGYBEAM, &tr);

	// Update beam visual
	m_pBeam->SetStartPos( tr.endpos );
	m_pBeam->RelinkBeam();

	// Perform designator functions
	// Did we hit something?
	CBaseEntity *pEntity = CBaseEntity::Instance( tr.u.ent );
	if ( pEntity )
	{
		// TODO: We dynamic_cast the entity we choose twice. Fix it.

		// If we hit a target we don't care about, try and grab the nearest valid target to our crosshair
		if ( !ValidDesignationTarget( pEntity ) )
		{
			pEntity = NULL;

			// Grab the entity nearest the crosshair
			float bestdot = AUTOAIM_20DEGREES;
			float flSize = weapon_laserdesignator_range.GetFloat() * 0.5;
			Vector vecCenter = vecSrc + vecAiming * flSize;

			// Find a target.
			CBaseEntity *pList[100];
			Vector delta( flSize, flSize, flSize );
			int count = UTIL_EntitiesInBox( pList, 100, vecCenter - delta, vecCenter + delta, FL_CLIENT|FL_NPC|FL_OBJECT );
			for ( int i = 0; i < count; i++ )
			{
				CBaseEntity *pTarget = pList[i];
				if ( !ValidDesignationTarget(pTarget) )
					continue;
				if ( TargetIsInLock( pPlayer, pTarget, &bestdot ) == false )
					continue;
				if ( (pTarget->Center() - pPlayer->Center() ).Length() > weapon_laserdesignator_range.GetFloat() )
					continue;

				// Can shoot at this one
				pEntity = pTarget;
			}
		}

		// Couldn't find anything
		if ( !pEntity )
			return;

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
				}
				else 
				{
					// If it's an enemy object, tell my sentryguns to attack it
					if ( !pPlayer->InSameTeam( pEntity ) )
					{
						DesignateSentriesToAttack( pEntity );
					}
				}
			}

			// Is it a limpet mine? If so, detonate it
			CLimpetMine *pLimpet = dynamic_cast<CLimpetMine *>(pEntity);
			if ( pLimpet && pLimpet->IsLive() && pLimpet->m_hOwner->pev == pPlayer->pev )
			{
				pLimpet->Use( pPlayer, pPlayer, USE_ON, 0 );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the specified entity is a valid one for designation
//-----------------------------------------------------------------------------
bool CWeaponLaserDesignator::ValidDesignationTarget( CBaseEntity *pEntity )
{
	// Ignore the world
	if ( pEntity->entindex() == 0 )
		return false;

	// Ignore players on my team
	if ( pEntity->IsPlayer() && pEntity->InSameTeam(m_hOwner) )
		return false;

	// Is it a sentrygun? If so, tell it to toggle it's sunken state.
	if ( pEntity->Classify() == CLASS_MILITARY )
		return true;

	// My limpet mines are valid
	CLimpetMine *pLimpet = dynamic_cast<CLimpetMine *>(pEntity);
	if ( pLimpet && pLimpet->IsLive() && pLimpet->m_hOwner->pev == m_hOwner->pev )
		return true;

	// NPCs are valid
	if ( pEntity->GetFlags() & FL_NPC )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the target entity's close enough to the player's crosshair to lock onto
//-----------------------------------------------------------------------------
bool CWeaponLaserDesignator::TargetIsInLock( CBaseTFPlayer *pPlayer, CBaseEntity *pTarget, float *flMaxDot )
{
	Vector	center = pTarget->Center();
	Vector	dir = center - pPlayer->Center();
	float	length = VectorNormalize( dir );
	Vector forward, right, up;
	pPlayer->EyeVectors( &forward, &right, &up );

	// Make sure it's in front of the player
	if ( DotProduct( dir, forward ) < 0.0f )
		return false;

	float dot = fabs( DotProduct(dir, right ) ) + fabs( DotProduct(dir, up ) );

	// Tweak for distance
	dot *= 1.0f + 4.0f * ( length / MAX_COORD_RANGE );

	// Outside the dot?
	if ( dot > *flMaxDot )
		return false;

	// Open LOS?
	trace_t tr;
	UTIL_TraceLine( pPlayer->Center(), center, MASK_SHOT, edict(), COLLISION_GROUP_NONE, &tr );
	if ( ( tr.fraction != 1.0f ) && ( tr.u.ent != pTarget->edict() ) )
		return false;

	*flMaxDot = dot;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Set the player's sentryguns to all attack the specified target
//-----------------------------------------------------------------------------
void CWeaponLaserDesignator::DesignateSentriesToAttack( CBaseEntity *pEntity )
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( m_hOwner );
	if ( !pPlayer )
		return;

	// Tell all this player's sentryguns
	for ( int i = 0; i < pPlayer->m_aObjects.Size(); i++ )
	{
		CBaseObject *pObj = pPlayer->m_aObjects[i];
		if ( !pObj )
			continue;

		if ( pObj->IsSentrygun() )
		{
			CObjectSentrygun *pSentry = static_cast<CObjectSentrygun *>(pObj);
			pSentry->DesignateTarget( pEntity );
		}

		if ( pObj->GetType() == OBJ_AERIAL_SENTRY_STATION )
		{
			CObjectAerialSentryStation *pSentryStation = static_cast<CObjectAerialSentryStation *>(pObj);
			pSentryStation->DesignateTarget( pEntity );
		}
	}
}