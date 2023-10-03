//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "ai_basenpc.h"
#include "animation.h"
#include "basecombatweapon.h"
#include "player.h"			// For gEvilImpulse101 / CBasePlayer
#include "gamerules.h"		// For g_pGameRules
#include <KeyValues.h>
#include "ammodef.h"
#include "baseviewmodel.h"
#include "in_buttons.h"
#include "soundent.h"
#include "weapon_parse.h"
#include "game.h"
#include "engine/IEngineSound.h"
#include "sendproxy.h"
#include "tier1/strtools.h"
#include "vphysics/constraints.h"
#include "npcevent.h"
#include "igamesystem.h"
#include "collisionutils.h"
#include "iservervehicle.h"
#include "func_break.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern int	gEvilImpulse101;		// In Player.h

// -----------------------------------------
//	Sprite Index info
// -----------------------------------------
int		g_sModelIndexLaser;			// holds the index for the laser beam
const char	*g_pModelNameLaser = "sprites/laserbeam.vmt";
int		g_sModelIndexLaserDot;		// holds the index for the laser beam dot
int		g_sModelIndexFireball;		// holds the index for the fireball
int		g_sModelIndexSmoke;			// holds the index for the smoke cloud
int		g_sModelIndexWExplosion;	// holds the index for the underwater explosion
int		g_sModelIndexBubbles;		// holds the index for the bubbles model
int		g_sModelIndexBloodDrop;		// holds the sprite index for the initial blood
int		g_sModelIndexBloodSpray;	// holds the sprite index for splattered blood


ConVar weapon_showproficiency( "weapon_showproficiency", "0" );
extern ConVar ai_debug_shoot_positions;

//-----------------------------------------------------------------------------
// Purpose: Precache global weapon resources
//-----------------------------------------------------------------------------
PRECACHE_REGISTER_BEGIN( GLOBAL, WeaponResources )


	PRECACHE_INDEX( MODEL, "sprites/zerogxplode.vmt", g_sModelIndexFireball )
	PRECACHE_INDEX( MODEL, "sprites/steam1.vmt", g_sModelIndexSmoke )
	PRECACHE_INDEX( MODEL, "sprites/bubble.vmt", g_sModelIndexBubbles )
	PRECACHE_INDEX( MODEL, "sprites/laserbeam.vmt", g_sModelIndexLaser )
	PRECACHE( PARTICLE_SYSTEM, "blood_impact_red_01" )
	PRECACHE( PARTICLE_SYSTEM, "blood_impact_green_01" )
	PRECACHE( PARTICLE_SYSTEM, "blood_impact_yellow_01" )
	PRECACHE( MODEL, "models/weapons/w_bullet.mdl" )
	PRECACHE( MODEL, "effects/bubble.vmt" )


	PRECACHE( GAMESOUND, "BaseCombatWeapon.WeaponDrop" )
	PRECACHE( GAMESOUND, "BaseCombatWeapon.WeaponMaterialize" )

PRECACHE_REGISTER_END()

void W_Precache(void)
{
	PrecacheFileWeaponInfoDatabase( filesystem, g_pGameRules->GetEncryptionKey() );
}

//-----------------------------------------------------------------------------
// Purpose: Transmit weapon data
//-----------------------------------------------------------------------------
int CBaseCombatWeapon::UpdateTransmitState( void)
{
	// If the weapon is being carried by a CBaseCombatCharacter, let the combat character do the logic
	// about whether or not to transmit it.
	if ( GetOwner() )
	{	
		return SetTransmitState( FL_EDICT_PVSCHECK );
	}
	else
	{
		// If it's just lying around, then use CBaseEntity's visibility test to see if it should be sent.
		return BaseClass::UpdateTransmitState();
	}
}


void CBaseCombatWeapon::Operator_FrameUpdate( CBaseCombatCharacter *pOperator )
{
	StudioFrameAdvance( ); // animate

	if ( IsSequenceFinished() )
	{
		if ( SequenceLoops() )
		{
			// animation does loop, which means we're playing subtle idle. Might need to fidget.
			int iSequence = SelectWeightedSequence( GetActivity() );
			if ( iSequence != ACTIVITY_NOT_AVAILABLE )
			{
				ResetSequence( iSequence );	// Set to new anim (if it's there)
			}
		}
#if 0
		else
		{
			// animation that just ended doesn't loop! That means we just finished a fidget
			// and should return to our heaviest weighted idle (the subtle one)
			SelectHeaviestSequence( GetActivity() );
		}
#endif
	}

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner == NULL )
		return;

	CBaseViewModel *vm = pOwner->GetViewModel( m_nViewModelIndex );
	if ( vm == NULL )
		return;

	// HACK: Player weapon and view model often use the same mdl, which results
	// in duplicate anim events.  For now, let the view model handle the events
	// if they're the same, which is the preferred behavior in general.
	CStudioHdr *w_hdr = GetModelPtr();
	CStudioHdr *v_hdr = vm->GetModelPtr();
	if ( w_hdr->GetRenderHdr() != v_hdr->GetRenderHdr() )
	{
		// Animation events are passed back to the weapon's owner/operator
		DispatchAnimEvents( pOperator );
	}

	// Update and dispatch the viewmodel events
	if ( vm != NULL )
	{
		vm->StudioFrameAdvance();
		vm->DispatchAnimEvents( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CBaseCombatWeapon::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	int nEvent = pEvent->Event();
	
	if ( (pEvent->type & AE_TYPE_NEWEVENTSYSTEM) && (pEvent->type & AE_TYPE_SERVER) )
	{
		if ( nEvent == AE_NPC_WEAPON_FIRE )
		{
			bool bSecondary = (atoi( pEvent->options ) != 0);
			Operator_ForceNPCFire( pOperator, bSecondary );
			return;
		}
		else if ( nEvent == AE_WPN_PLAYWPNSOUND )
		{
			int iSnd = GetWeaponSoundFromString(pEvent->options);
			if ( iSnd != -1 )
			{
				WeaponSound( (WeaponSound_t)iSnd );
			}
		}
	}

	DevWarning( 2, "Unhandled animation event %d from %s --> %s\n", nEvent, pOperator->GetClassname(), GetClassname() );
}

// NOTE: This should never be called when a character is operating the weapon.  Animation events should be
// routed through the character, and then back into CharacterAnimEvent() 
void CBaseCombatWeapon::HandleAnimEvent( animevent_t *pEvent )
{
	//If the player is receiving this message, pass it through
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner != NULL )
	{
		Operator_HandleAnimEvent( pEvent, pOwner );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Make the weapon visible and tangible
//-----------------------------------------------------------------------------
CBaseEntity* CBaseCombatWeapon::Respawn( void )
{
	// make a copy of this weapon that is invisible and inaccessible to players (no touch function). The weapon spawn/respawn code
	// will decide when to make the weapon visible and touchable.
	CBaseEntity *pNewWeapon = CBaseEntity::Create( GetClassname(), g_pGameRules->VecWeaponRespawnSpot( this ), GetLocalAngles(), GetOwnerEntity() );

	if ( pNewWeapon )
	{
		pNewWeapon->AddEffects( EF_NODRAW );// invisible for now
		pNewWeapon->SetTouch( NULL );// no touch
		pNewWeapon->SetThink( &CBaseCombatWeapon::AttemptToMaterialize );

		UTIL_DropToFloor( this, MASK_SOLID );

		// not a typo! We want to know when the weapon the player just picked up should respawn! This new entity we created is the replacement,
		// but when it should respawn is based on conditions belonging to the weapon that was taken.
		pNewWeapon->SetNextThink( gpGlobals->curtime + g_pGameRules->FlWeaponRespawnTime( this ) );
	}
	else
	{
		Warning("Respawn failed to create %s!\n", GetClassname() );
	}

	return pNewWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: Weapons ignore other weapons when LOS tracing
//-----------------------------------------------------------------------------
class CWeaponLOSFilter : public CTraceFilterSkipTwoEntities
{
	DECLARE_CLASS( CWeaponLOSFilter, CTraceFilterSkipTwoEntities );
public:
	CWeaponLOSFilter( IHandleEntity *pHandleEntity, IHandleEntity *pHandleEntity2, int collisionGroup ) :
	  CTraceFilterSkipTwoEntities( pHandleEntity, pHandleEntity2, collisionGroup ), m_pVehicle( NULL )
	{
		// If the tracing entity is in a vehicle, then ignore it
		if ( pHandleEntity != NULL )
		{
			CBaseCombatCharacter *pBCC = ((CBaseEntity *)pHandleEntity)->MyCombatCharacterPointer();
			if ( pBCC != NULL )
			{
				m_pVehicle = pBCC->GetVehicleEntity();
			}
		}
	}
	virtual bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{
		CBaseEntity *pEntity = (CBaseEntity *)pServerEntity;

		if ( pEntity->GetCollisionGroup() == COLLISION_GROUP_WEAPON )
			return false;

		// Don't collide with the tracing entity's vehicle (if it exists)
		if ( pServerEntity == m_pVehicle )
			return false;

		if ( pEntity->GetHealth() > 0 )
		{
			CBreakable *pBreakable = dynamic_cast<CBreakable *>(pEntity);
			if ( pBreakable  && pBreakable->IsBreakable() && pBreakable->GetMaterialType() == matGlass)
			{
				return false;
			}
		}

		return BaseClass::ShouldHitEntity( pServerEntity, contentsMask );
	}

private:
	CBaseEntity *m_pVehicle;
};

//-----------------------------------------------------------------------------
// Purpose: Check the weapon LOS for an owner at an arbitrary position
//			If bSetConditions is true, LOS related conditions will also be set
//-----------------------------------------------------------------------------
bool CBaseCombatWeapon::WeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions )
{
	// --------------------
	// Check for occlusion
	// --------------------
	CAI_BaseNPC* npcOwner = m_hOwner.Get()->MyNPCPointer();

	// Find its relative shoot position
	Vector vecRelativeShootPosition;
	VectorSubtract( npcOwner->Weapon_ShootPosition(), npcOwner->GetAbsOrigin(), vecRelativeShootPosition );
	Vector barrelPos = ownerPos + vecRelativeShootPosition;

	// FIXME: If we're in a vehicle, we need some sort of way to handle shooting out of them

	// Use the custom LOS trace filter
	CWeaponLOSFilter traceFilter( m_hOwner.Get(), npcOwner->GetEnemy(), COLLISION_GROUP_BREAKABLE_GLASS );
	trace_t tr;
	UTIL_TraceLine( barrelPos, targetPos, MASK_SHOT, &traceFilter, &tr );

	// See if we completed the trace without interruption
	if ( tr.fraction == 1.0 )
	{
		if ( ai_debug_shoot_positions.GetBool() )
		{
			NDebugOverlay::Line( barrelPos, targetPos, 0, 255, 0, false, 1.0 );
		}

		return true;
	}

	CBaseEntity	*pHitEnt = tr.m_pEnt;

	CBasePlayer *pEnemyPlayer = ToBasePlayer( npcOwner->GetEnemy() );

	// is player in a vehicle? if so, verify vehicle is target and return if so (so npc shoots at vehicle)
	if ( pEnemyPlayer && pEnemyPlayer->IsInAVehicle() )
	{
		// Ok, player in vehicle, check if vehicle is target we're looking at, fire if it is
		// Also, check to see if the owner of the entity is the vehicle, in which case it's valid too.
		// This catches vehicles that use bone followers.
		CBaseEntity	*pVehicle  = pEnemyPlayer->GetVehicle()->GetVehicleEnt();
		if ( pHitEnt == pVehicle || pHitEnt->GetOwnerEntity() == pVehicle )
			return true;
	}

	// Hitting our enemy is a success case
	if ( pHitEnt == npcOwner->GetEnemy() )
	{
		if ( ai_debug_shoot_positions.GetBool() )
		{
			NDebugOverlay::Line( barrelPos, targetPos, 0, 255, 0, false, 1.0 );
		}

		return true;
	}

	// If a vehicle is blocking the view, grab its driver and use that as the combat character
	CBaseCombatCharacter *pBCC;
	IServerVehicle *pVehicle = pHitEnt->GetServerVehicle();
	if ( pVehicle )
	{
		pBCC = pVehicle->GetPassenger( );
	}
	else
	{
		pBCC = ToBaseCombatCharacter( pHitEnt );
	}

	if ( pBCC ) 
	{
		if ( npcOwner->IRelationType( pBCC ) == D_HT )
			return true;

		if ( bSetConditions )
		{
			npcOwner->SetCondition( COND_WEAPON_BLOCKED_BY_FRIEND );
		}
	}
	else if ( bSetConditions )
	{
		npcOwner->SetCondition( COND_WEAPON_SIGHT_OCCLUDED );
		npcOwner->SetEnemyOccluder( pHitEnt );

		if( ai_debug_shoot_positions.GetBool() )
		{
			NDebugOverlay::Line( tr.startpos, tr.endpos, 255, 0, 0, false, 1.0 );
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Base class always returns not bits
//-----------------------------------------------------------------------------
int CBaseCombatWeapon::WeaponRangeAttack1Condition( float flDot, float flDist )
{
 	if ( UsesPrimaryAmmo() && !HasPrimaryAmmo() )
 	{
 		return COND_NO_PRIMARY_AMMO;
 	}
 	else if ( flDist < m_fMinRange1) 
 	{
 		return COND_TOO_CLOSE_TO_ATTACK;
 	}
 	else if (flDist > m_fMaxRange1) 
 	{
 		return COND_TOO_FAR_TO_ATTACK;
 	}
 	else if (flDot < 0.5) 	// UNDONE: Why check this here? Isn't the AI checking this already?
 	{
 		return COND_NOT_FACING_ATTACK;
 	}

 	return COND_CAN_RANGE_ATTACK1;
}

//-----------------------------------------------------------------------------
// Purpose: Base class always returns not bits
//-----------------------------------------------------------------------------
int CBaseCombatWeapon::WeaponRangeAttack2Condition( float flDot, float flDist )
{
	// currently disabled
	return COND_NONE;

	if ( m_bReloadsSingly )
	{
		if (m_iClip2 <=0)
		{
			return COND_NO_SECONDARY_AMMO;
		}
		else if ( flDist < m_fMinRange2) 
		{
			return COND_TOO_CLOSE_TO_ATTACK;
		}
		else if (flDist > m_fMaxRange2) 
		{
			return COND_TOO_FAR_TO_ATTACK;
		}
		else if (flDot < 0.5) 
		{
			return COND_NOT_FACING_ATTACK;
		}
		return COND_CAN_RANGE_ATTACK2;
	}

	return COND_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: Base class always returns not bits
//-----------------------------------------------------------------------------
int CBaseCombatWeapon::WeaponMeleeAttack1Condition( float flDot, float flDist )
{
	return COND_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: Base class always returns not bits
//-----------------------------------------------------------------------------
int CBaseCombatWeapon::WeaponMeleeAttack2Condition( float flDot, float flDist )
{
	return COND_NONE;
}

//====================================================================================
// WEAPON DROPPING / DESTRUCTION
//====================================================================================
void CBaseCombatWeapon::Delete( void )
{
	SetTouch( NULL );
	// FIXME: why doesn't this just remove itself now?
	SetThink(&CBaseCombatWeapon::SUB_Remove);
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CBaseCombatWeapon::DestroyItem( void )
{
	CBaseCombatCharacter *pOwner = m_hOwner.Get();

	if ( pOwner )
	{
		// if attached to a player, remove. 
		pOwner->RemovePlayerItem( this );
	}

	Kill( );
}

void CBaseCombatWeapon::Kill( void )
{
	SetTouch( NULL );
	// FIXME: why doesn't this just remove itself now?
	// FIXME: how is this different than Delete(), and why do they have the same code in them?
	SetThink(&CBaseCombatWeapon::SUB_Remove);
	SetNextThink( gpGlobals->curtime + 0.1f );
}

//====================================================================================
// FALL TO GROUND
//====================================================================================
//-----------------------------------------------------------------------------
// Purpose: Setup for the fall
//-----------------------------------------------------------------------------
void CBaseCombatWeapon::FallInit( void )
{
	SetModel( GetWorldModel() );
	VPhysicsDestroyObject();

	if ( !VPhysicsInitNormal( SOLID_BBOX, GetSolidFlags() | FSOLID_TRIGGER, false ) )
	{
		SetMoveType( MOVETYPE_FLYGRAVITY );
		SetSolid( SOLID_BBOX );
		AddSolidFlags( FSOLID_TRIGGER );
	}
	else
	{
#if !defined( CLIENT_DLL )
		// Constrained start?
		if ( HasSpawnFlags( SF_WEAPON_START_CONSTRAINED ) )
		{
			//Constrain the weapon in place
			IPhysicsObject *pReferenceObject, *pAttachedObject;
			
			pReferenceObject = g_PhysWorldObject;
			pAttachedObject = VPhysicsGetObject();

			if ( pReferenceObject && pAttachedObject )
			{
				constraint_fixedparams_t fixed;
				fixed.Defaults();
				fixed.InitWithCurrentObjectState( pReferenceObject, pAttachedObject );
				
				fixed.constraint.forceLimit	= lbs2kg( 10000 );
				fixed.constraint.torqueLimit = lbs2kg( 10000 );

				m_pConstraint = physenv->CreateFixedConstraint( pReferenceObject, pAttachedObject, NULL, fixed );

				m_pConstraint->SetGameData( (void *) this );
			}
		}
#endif //CLIENT_DLL
	}	

	SetPickupTouch();
	
	SetThink( &CBaseCombatWeapon::FallThink );

	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: Items that have just spawned run this think to catch them when 
//			they hit the ground. Once we're sure that the object is grounded, 
//			we change its solid type to trigger and set it in a large box that 
//			helps the player get it.
//-----------------------------------------------------------------------------
void CBaseCombatWeapon::FallThink ( void )
{
	SetNextThink( gpGlobals->curtime + 0.1f );

	bool shouldMaterialize = false;
	IPhysicsObject *pPhysics = VPhysicsGetObject();
	if ( pPhysics )
	{
		shouldMaterialize = pPhysics->IsAsleep();
	}
	else
	{
		shouldMaterialize = (GetFlags() & FL_ONGROUND) ? true : false;
	}

	if ( shouldMaterialize )
	{
		// clatter if we have an owner (i.e., dropped by someone)
		// don't clatter if the gun is waiting to respawn (if it's waiting, it is invisible!)
		if ( GetOwnerEntity() )
		{
			EmitSound( "BaseCombatWeapon.WeaponDrop" );
		}
		Materialize(); 
	}
}

//====================================================================================
// WEAPON SPAWNING
//====================================================================================
//-----------------------------------------------------------------------------
// Purpose: Make a weapon visible and tangible
//-----------------------------------------------------------------------------// 
void CBaseCombatWeapon::Materialize( void )
{
	if ( IsEffectActive( EF_NODRAW ) )
	{
		// changing from invisible state to visible.
		EmitSound( "BaseCombatWeapon.WeaponMaterialize" );
		
		RemoveEffects( EF_NODRAW );
		DoMuzzleFlash();
	}
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_TRIGGER );

	SetPickupTouch();

	SetThink (NULL);
}

//-----------------------------------------------------------------------------
// Purpose: See if the game rules will let this weapon respawn
//-----------------------------------------------------------------------------
void CBaseCombatWeapon::AttemptToMaterialize( void )
{
	float time = g_pGameRules->FlWeaponTryRespawn( this );

	if ( time == 0 )
	{
		Materialize();
		return;
	}

	SetNextThink( gpGlobals->curtime + time );
}

//-----------------------------------------------------------------------------
// Purpose: Weapon has been picked up, should it respawn?
//-----------------------------------------------------------------------------
void CBaseCombatWeapon::CheckRespawn( void )
{
	switch ( g_pGameRules->WeaponShouldRespawn( this ) )
	{
	case GR_WEAPON_RESPAWN_YES:
		Respawn();
		break;
	case GR_WEAPON_RESPAWN_NO:
		return;
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CBaseCombatWeapon::ObjectCaps( void )
{ 
	int caps = BaseClass::ObjectCaps();
	if ( !IsFollowingEntity() && !HasSpawnFlags(SF_WEAPON_NO_PLAYER_PICKUP) )
	{
		caps |= FCAP_IMPULSE_USE;
	}

	return caps;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseCombatWeapon::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = ToBasePlayer( pActivator );
	
	if ( pPlayer )
	{
		m_OnPlayerUse.FireOutput( pActivator, pCaller );

		//
		// Bump the weapon to try equipping it before picking it up physically. This is
		// important in a few spots in the game where the player could potentially +use pickup
		// and then THROW AWAY a vital weapon, rendering them unable to continue the game.
		//
		if ( pPlayer->BumpWeapon( this ) )
		{
			OnPickedUp( pPlayer );
		}
		else
		{
			pPlayer->PickupObject( this );
		}
	}
}

void CBaseCombatWeapon::MakeWeaponNameFromEntity( CBaseEntity *pOther )
{
	// If I have a name, make my weapon match it with "_weapon" appended
	if ( pOther->GetEntityName() != NULL_STRING )
	{
		const char *pMarineName = STRING( pOther->GetEntityName() );
		const char *pError = UTIL_VarArgs( "%s_weapon", pMarineName );
		string_t pooledName = AllocPooledString( pError );
		SetName( pooledName );
	}
}
