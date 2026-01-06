//=============================================================================//
//
// Purpose: A prototype portal gun left behind by Aperture at Arbeit 1
//
//	For Bad Cop's purposes, it's either a stun weapon, a puzzle solving tool,
// or a bag of holding.
//
// The first shot fires a 'blue portal' which consumes an NPC or object.
// Firing again shoots the 'red portal' which spits that object back out.
//
// Holding onto an object for more than 5 seconds will cause it to spawn
// back into its original position.
//
// Author: 1upD
//
//=============================================================================//

#include "cbase.h"
#include "baseentity.h"
#include "npc_wilson.h"
#include "te_effect_dispatch.h"
#include "beam_shared.h"
#include "weapon_displacer_pistol.h"
#include "ez2_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_SERVERCLASS_ST( CDisplacerPistol, DT_DisplacerPistol )
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_displacer_pistol, CDisplacerPistol );
PRECACHE_WEAPON_REGISTER( weapon_displacer_pistol );

BEGIN_DATADESC( CDisplacerPistol )
	DEFINE_FIELD( m_hDisplacedEntity, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hTargetPosition, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hLastDisplacePosition, FIELD_EHANDLE ),

	DEFINE_THINKFUNC( ReleaseThink ),
END_DATADESC()

ConVar sk_displacer_pistol_duration( "sk_displacer_pistol_duration", "5" );

// These are defined in npc_wilson.
// I don't know if there's a way to make it base-level since Xen grenades aren't NPCs.
extern int g_interactionXenGrenadeConsume;
extern int g_interactionXenGrenadeRelease;

//-----------------------------------------------------------------------------
// Purpose: Precache
//-----------------------------------------------------------------------------
void CDisplacerPistol::Precache( void )
{
	BaseClass::Precache();

	PrecacheParticleSystem( "red_spawn" );
	PrecacheParticleSystem( "blue_spawn" );
}

//-----------------------------------------------------------------------------
// Purpose: Override to fill the player's ammo when first picked up
// Kind of a hackhack
//-----------------------------------------------------------------------------
void CDisplacerPistol::Equip( CBaseCombatCharacter * pOwner )
{
	BaseClass::Equip( pOwner );
	if ( pOwner->IsPlayer() && m_bFirstDraw )
	{
		CBasePlayer * pPlayer = ToBasePlayer( GetOwner() );
		pPlayer->GiveAmmo( 5, "AR2AltFire", true );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Main attack
//-----------------------------------------------------------------------------
void CDisplacerPistol::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if (!pPlayer)
	{
		return;
	}

	// Trace a line from the weapon
	trace_t * pTrace = new trace_t();	
	Vector vecStart, vecDir;
	vecStart = pPlayer->EyePosition();
	vecDir = pPlayer->EyeDirection3D();

	QAngle targetAngle = QAngle( 0, GetAbsAngles().y, 0 );

	// No entity has been displaced - fire a blue portal
	if ( m_hDisplacedEntity == NULL )
	{
		UTIL_TraceLine( vecStart + vecDir, vecStart + vecDir * 8192, MASK_SHOT_PORTAL, this, COLLISION_GROUP_NONE, pTrace );

		// Move the target position DIRECTLY to the end of the trace
		MoveTargetPosition( pTrace->endpos, targetAngle );

		BluePortalEffects();
		// Try to displace the entity
		if ( DispaceEntity( pTrace->m_pEnt ) )
		{
			MoveTargetPosition( m_hDisplacedEntity->WorldSpaceCenter(), m_hDisplacedEntity->GetAbsAngles() );
			MoveLastDisplacedPosition( m_hDisplacedEntity->WorldSpaceCenter(), m_hDisplacedEntity->GetAbsAngles() );
			DevMsg( "Displacer pistol sucessfully displaced an entity\n" );
			m_hDisplacedEntity->EmitSound( "Weapon_DisplacerPistol.DisplaceCatch" );
			DispatchParticleEffect( "blue_spawn", m_hDisplacedEntity->WorldSpaceCenter(), m_hDisplacedEntity->GetAbsAngles(), this );
			WeaponSound( SINGLE );
			SendWeaponAnim( ACT_VM_PRIMARYATTACK );
			pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );
			// TODO Fire an output when an entity is "displaced"
			// m_hDisplacedEntity->OnDisplace();
			SetContextThink( &CDisplacerPistol::ReleaseThink, gpGlobals->curtime + sk_displacer_pistol_duration.GetFloat(), "releaseThink" );
		}
		// Failed - play an effect
		else
		{
			DevMsg( "Displacer pistol failed to displace entity\n" );
			WeaponSound( EMPTY );
			SendWeaponAnim( ACT_VM_DRYFIRE );
		}
	}
	// An entity has been displaced - fire a red portal
	else
	{
		UTIL_TraceLine( vecStart + vecDir, vecStart + vecDir * 8192, MASK_SOLID, this, COLLISION_GROUP_NONE, pTrace );

		// Move the target position OFFSET from the end of the trace, normal to the surface
		if ( pTrace->DidHitWorld() )
		{
			// TODO We need a better metric to determine how far to offset the release point!
			Vector normal = pTrace->plane.normal * 32.0f;
			MoveTargetPosition( pTrace->endpos + normal, targetAngle );
		}
		else
		{
			MoveTargetPosition( pTrace->endpos, targetAngle );
		}

		RedPortalEffects();
		// Try to release the entity
		if ( ReleaseEntity( pTrace->m_pEnt ) )
		{
			DevMsg( "Displacer pistol sucessfully released an entity\n" );

			DispatchParticleEffect( "red_spawn", m_hTargetPosition->WorldSpaceCenter(), m_hTargetPosition->GetAbsAngles(), this );
			SendWeaponAnim( ACT_VM_PRIMARYATTACK );
			WeaponSound( WPN_DOUBLE );
			// TODO Fire an output when an entity is "released"
			// m_hDisplacedEntity->OnUnDisplace();
		}
		// Failed - play an effect
		else
		{
			MoveTargetPosition( m_hLastDisplacePosition->GetAbsOrigin(), m_hLastDisplacePosition->GetAbsAngles() );
			DevMsg( "Displacer pistol failed to release entity\n" );
			WeaponSound( EMPTY );
			SendWeaponAnim( ACT_VM_DRYFIRE );
		}
	}

	pPlayer->m_flNextAttack = gpGlobals->curtime + 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Secondary attack - does nothing at the moment
//-----------------------------------------------------------------------------
void CDisplacerPistol::SecondaryAttack( void )
{

}

//-----------------------------------------------------------------------------
// Purpose: Handle fire on empty - overridden so that the pistol can shoot
// if a target is actively displaced
//-----------------------------------------------------------------------------
void CDisplacerPistol::HandleFireOnEmpty()
{
	if ( m_hDisplacedEntity != NULL )
	{
		PrimaryAttack();
	}
	else
	{
		BaseClass::HandleFireOnEmpty();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the weapon currently has ammo or doesn't need ammo
// Overridden because if there is an actively displaced entity, the pistol
// does not need ammo
// Output :
//-----------------------------------------------------------------------------
bool CDisplacerPistol::HasAnyAmmo( void )
{
	if ( m_hDisplacedEntity != NULL )
	{
		return true;
	}

	return BaseClass::HasAnyAmmo();
}

bool CDisplacerPistol::DispaceEntity( CBaseEntity * pEnt )
{
	// If we hit something, try to displace it
	if ( pEnt )
	{
		if ( pEnt->IsDisplacementImpossible() )
		{
			return false;
		}

		DisplacementInfo_t dinfo( this, this, &m_hTargetPosition->GetAbsOrigin(), &m_hTargetPosition->GetAbsAngles() );
		if (pEnt->DispatchInteraction( g_interactionXenGrenadeConsume, &dinfo, GetOwner() ))
		{
			// Do not remove
			m_hDisplacedEntity = pEnt;
			return true;
		}

		IPhysicsObject *pPhysicsObject = pEnt->VPhysicsGetObject();
		if (pPhysicsObject != NULL)
		{
			// "Wake" the physics object
			// Both motion disabled and sleeping objects have motion disabled
			// We need to make sure this is actually force-disabled and not just sleeping
			pPhysicsObject->Wake();

			if ( pPhysicsObject->IsMotionEnabled() == false || pPhysicsObject->IsStatic() )
			{
				DevMsg( "Displacer pistol can't displace physics object %s because it has motion disabled!\n", pEnt->GetDebugName() );
				return false;
			}
		}

		if (GetOwner())
		{
			// Notify the E:Z2 player
			CEZ2_Player *pEZ2Player = assert_cast<CEZ2_Player*>(GetOwner());
			if (pEZ2Player)
			{
				pEZ2Player->Event_DisplacerPistolDisplace( this, pEnt );
			}
		}

		if ( pEnt->IsNPC() )
		{
			CAI_BaseNPC * pNPC = pEnt->MyNPCPointer();
	
			Assert( pNPC != NULL );

			// Invoke effects similar to that of being lifted by a barnacle.
			pNPC->AddEFlags( EFL_IS_BEING_LIFTED_BY_BARNACLE );
			pNPC->KillSprites( 0.0f );
			pNPC->SetSleepState( AISS_IGNORE_INPUT );
			pNPC->Sleep();
		}

		// Make all children nodraw
		CBaseEntity *pChild = pEnt->FirstMoveChild();
		while ( pChild )
		{
			pChild->AddEffects( EF_NODRAW );
			pChild = pChild->NextMovePeer();
		}

		pEnt->AddSolidFlags( FSOLID_NOT_SOLID );
		pEnt->AddEffects( EF_NODRAW );
		m_hDisplacedEntity = pEnt;
		m_hDisplacedEntity->SetNextThink( TICK_NEVER_THINK );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Context think "timer" for when entities automatically rematerialize
// TODO Add some complicated effects handling to this think!
//-----------------------------------------------------------------------------
void CDisplacerPistol::ReleaseThink()
{
	DevMsg( "Time's up! %s releasing target\n", GetDebugName() );
	if ( ReleaseEntity( NULL ) )
	{
		DispatchParticleEffect( "red_spawn", m_hTargetPosition->WorldSpaceCenter(), m_hTargetPosition->GetAbsAngles(), this );
	}
	else
	{
		SetContextThink( &CDisplacerPistol::ReleaseThink, gpGlobals->curtime + sk_displacer_pistol_duration.GetFloat(), "releaseThink" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Release the last displaced entity
// Takes a different entity pointer as a parameter for the entity that the 
// displaced entity might switch places with.
// If pCollidedEntity is NULL, it will try to find the nearest entity
//-----------------------------------------------------------------------------
bool CDisplacerPistol::ReleaseEntity( CBaseEntity * pCollidedEntity )
{
	Assert( m_hTargetPosition != NULL );

	// If we have something, try to release it
	if ( m_hDisplacedEntity )
	{
		// First try the interaction
		DisplacementInfo_t dinfo( this, this, &m_hTargetPosition->GetAbsOrigin(), &m_hTargetPosition->GetAbsAngles() );
		if (m_hDisplacedEntity->DispatchInteraction( g_interactionXenGrenadeRelease, &dinfo, GetOwner() ))
		{
			m_hDisplacedEntity->EmitSound( "Weapon_DisplacerPistol.DisplaceRelease" );
			m_hDisplacedEntity = NULL;
			return true;
		}

		m_hDisplacedEntity->Teleport( &m_hTargetPosition->GetAbsOrigin(), &m_hTargetPosition->GetAbsAngles(), NULL );

		// If there is no collided entity set, look at our location and try to see if we can find a damageable entity
		if (pCollidedEntity == NULL)
		{
			CBaseEntity * pList[8];
			int numEnts = UTIL_EntitiesAtPoint( pList, 8, m_hDisplacedEntity->GetAbsOrigin(), 0 );

			for (int i = 0; i < numEnts; i++)
			{
				if (!(pList[i]->GetSolidFlags() & FSOLID_NOT_SOLID) && pList[i]->GetMaxHealth() > 0)
				{
					pCollidedEntity = pList[i];
					break;
				}
			}
		}

		if ( m_hDisplacedEntity->IsNPC() )
		{
			CAI_BaseNPC * pNPC = m_hDisplacedEntity->MyNPCPointer();

			Vector vUpBit = pNPC->GetAbsOrigin();
			vUpBit.z += 1;
			trace_t tr;

			// We filter out the collided entity so that they can telefrag - our entity should take precedence
			CTraceFilterSkipTwoEntities traceFilter( pNPC, pCollidedEntity, COLLISION_GROUP_NONE );
			ITraceFilter * pFilter = &traceFilter;

			AI_TraceHull( pNPC->GetAbsOrigin(), vUpBit, pNPC->GetHullMins(), pNPC->GetHullMaxs(),
				MASK_NPCSOLID, pFilter, &tr );
			if (tr.startsolid || (tr.fraction < 1.0))
			{
				DevMsg( "Displacer pistol can't release %s. Bad Position!\n", pNPC->GetDebugName() );
				return false;
			}

			pNPC->Wake();
			pNPC->MyNPCPointer()->StartEye();
		
			m_hDisplacedEntity->RemoveEFlags( EFL_IS_BEING_LIFTED_BY_BARNACLE );
		}

		// Make all children draw
		CBaseEntity *pChild = m_hDisplacedEntity->FirstMoveChild();
		while ( pChild )
		{
			pChild->RemoveEffects( EF_NODRAW );
			pChild = pChild->NextMovePeer();
		}

		m_hDisplacedEntity->RemoveSolidFlags( FSOLID_NOT_SOLID );
		m_hDisplacedEntity->RemoveEffects( EF_NODRAW );

		MoveTargetPosition( m_hDisplacedEntity->WorldSpaceCenter(), m_hDisplacedEntity->GetAbsAngles() );
		m_hDisplacedEntity->EmitSound( "Weapon_DisplacerPistol.DisplaceRelease" );

		IPhysicsObject *pPhysicsObject = m_hDisplacedEntity->VPhysicsGetObject();
		if (pPhysicsObject != NULL)
		{
			// "Wake" the physics object
			// Make sure it doesn't spawn frozen!
			pPhysicsObject->Wake();
		}

		if (GetOwner())
		{
			// Notify the E:Z2 player
			CEZ2_Player *pEZ2Player = assert_cast<CEZ2_Player*>(GetOwner());
			if (pEZ2Player)
			{
				pEZ2Player->Event_DisplacerPistolRelease( this, m_hDisplacedEntity, pCollidedEntity );
			}
		}

		// Deal damage to any entity we might have collided with
		bool bTelefragged = Telefrag( pCollidedEntity, m_hDisplacedEntity );
		// If the collided entity didn't die, the displaced entity damages itself
		if ( !bTelefragged )
		{
			Telefrag( m_hDisplacedEntity, pCollidedEntity );
		}

		// Reactivate thinking
		m_hDisplacedEntity->SetNextThink( gpGlobals->curtime );
		m_hDisplacedEntity = NULL;

		// Cancel the context think
		SetContextThink( NULL, 0.0f, "releaseThink" );

		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Two objects teleported into the same space
// Deal damage to an entity
//-----------------------------------------------------------------------------
bool CDisplacerPistol::Telefrag( CBaseEntity * pVictim, CBaseEntity * pInflictor )
{
	// Considered success if no entity telefragged
	if ( pVictim == NULL )
	{
		return true;
	}

	// If the victim is the world, do no damage and consider success
	if ( pVictim->IsWorld() )
	{
		return true;
	}

	// TODO - Let's add an interaction for telefragging so that "boss" NPCs like advisors and clone cop
	// can have special handling when you teleport something into them

	DevMsg( "Telefragging %s\n", pVictim->GetDebugName() );

	// TODO - Calculate damage based the mass of both objects
	float flDamage = 100.0f;
	CTakeDamageInfo info( pInflictor, GetOwnerEntity(), flDamage, DMG_CRUSH | DMG_ALWAYSGIB, 0 );
	pVictim->TakeDamage( info );
	return pVictim->GetHealth() < 0;

}

void CDisplacerPistol::BluePortalEffects()
{
	PortalEffects( "sprites/lgtning_noz.vmt" );
}

void CDisplacerPistol::RedPortalEffects()
{
	PortalEffects( "sprites/orangelight1.vmt" );
}

void CDisplacerPistol::PortalEffects( const char * pSpriteName)
{
	Assert( m_hTargetPosition );
	if (m_hTargetPosition == NULL)
		return;

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if (pOwner == NULL)
		return;

	Vector	endpos = m_hTargetPosition->GetAbsOrigin();

	// Check to store off our view model index
	CBeam *pBeam = CBeam::BeamCreate( pSpriteName, 8 );

	if ( pBeam != NULL )
	{
		pBeam->PointEntInit( endpos, this );
		pBeam->SetEndAttachment( 1 );
		pBeam->SetWidth( 6.4 );
		pBeam->SetEndWidth( 12.8 );
		pBeam->SetBrightness( 255 );
		pBeam->SetColor( 255, 255, 255 );
		pBeam->LiveForTime( 0.1f );
		pBeam->RelinkBeam();
		pBeam->SetNoise( 2 );
	}

	Vector	shotDir = (endpos - pOwner->Weapon_ShootPosition());
	VectorNormalize( shotDir );

	//End hit
	//FIXME: Probably too big
	CPVSFilter filter( endpos );
	te->GaussExplosion( filter, 0.0f, endpos - (shotDir * 4.0f), RandomVector( -1.0f, 1.0f ), 0 );

	// Light up the environment
	pOwner->DoMuzzleFlash();

	// TODO add a sprite on the end of the muzzle too
}

void CDisplacerPistol::MoveTargetPosition( const Vector & vecOrigin, const QAngle & vecAngles )
{
	m_hTargetPosition = MovePosition( m_hTargetPosition, vecOrigin, vecAngles );
}

void CDisplacerPistol::MoveLastDisplacedPosition( const Vector & vecOrigin, const QAngle & vecAngles )
{
	m_hLastDisplacePosition = MovePosition( m_hLastDisplacePosition, vecOrigin, vecAngles );
}

CBaseEntity * CDisplacerPistol::MovePosition( CBaseEntity * pTarget, const Vector & vecOrigin, const QAngle & vecAngles )
{
	if (pTarget == NULL)
	{
		return Create( "info_target", vecOrigin, vecAngles, GetOwner() );
	}
	else
	{
		pTarget->Teleport( &vecOrigin, &vecAngles, &vec3_origin );
		return pTarget;
	}
}
