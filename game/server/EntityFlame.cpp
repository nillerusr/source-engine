//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Flame entity to be attached to target entity. Serves two purposes:
//
//			1) An entity that can be placed by a level designer and triggered
//			   to ignite a target entity.
//
//			2) An entity that can be created at runtime to ignite a target entity.
//
//===========================================================================//

#include "cbase.h"
#include "EntityFlame.h"
#include "ai_basenpc.h"
#ifdef INFESTED_DLL
#include "asw_fire.h"
#else
#include "fire.h"
#endif
#include "shareddefs.h"
#include "ai_link.h"
#include "ai_node.h"
#include "ai_network.h"
#include "ai_localnavigator.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_DATADESC( CEntityFlame )

	DEFINE_FIELD( m_flLifetime, FIELD_TIME ),
	DEFINE_FIELD( m_flSize, FIELD_FLOAT ),
	DEFINE_FIELD( m_hEntAttached, FIELD_EHANDLE ),
	DEFINE_FIELD( m_iDangerSound, FIELD_INTEGER ),
	DEFINE_FIELD( m_bCheapEffect, FIELD_BOOLEAN ),
	// DEFINE_FIELD( m_bPlayingSound, FIELD_BOOLEAN ),
	// DEFINE_FIELD( m_DangerLinks, CUtlVector< CAI_Link* > ),
	
	DEFINE_FUNCTION( FlameThink ),

END_DATADESC()


IMPLEMENT_SERVERCLASS_ST( CEntityFlame, DT_EntityFlame )
	SendPropEHandle( SENDINFO( m_hEntAttached ) ),
	SendPropBool( SENDINFO( m_bCheapEffect ) ),
END_SEND_TABLE()

#ifndef INFESTED_DLL
LINK_ENTITY_TO_CLASS( entityflame, CEntityFlame );
#endif
PRECACHE_REGISTER(entityflame);


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEntityFlame::CEntityFlame( void )
{
	m_flSize			= 0.0f;
	m_flLifetime		= gpGlobals->curtime;
	m_bPlayingSound		= false;
	m_iDangerSound		= SOUNDLIST_EMPTY;
	m_bCheapEffect		= false;
	m_hObstacle			= OBSTACLE_INVALID;
}

void CEntityFlame::UpdateOnRemove()
{
	// Sometimes the entity I'm burning gets destroyed by other means,
	// which kills me. Make sure to stop the burning sound.
	if ( m_bPlayingSound )
	{
		EmitSound( "General.StopBurning" );
		m_bPlayingSound = false;
	}

	if ( m_iDangerSound != SOUNDLIST_EMPTY )
	{
		CSoundEnt::FreeSound( m_iDangerSound );
		m_iDangerSound = SOUNDLIST_EMPTY;
	}

	int nCount = m_DangerLinks.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CAI_Link *pLink = m_DangerLinks[i];
		--pLink->m_nDangerCount;
	}
	m_DangerLinks.RemoveAll();

	if ( m_hObstacle != OBSTACLE_INVALID )
	{
		CAI_LocalNavigator::RemoveGlobalObstacle( m_hObstacle );
		m_hObstacle = OBSTACLE_INVALID;
	}

	BaseClass::UpdateOnRemove();
}

void CEntityFlame::Precache()
{
	BaseClass::Precache();

	PrecacheParticleSystem( "burning_character" );
	PrecacheParticleSystem( "burning_gib_01" );

	PrecacheScriptSound( "General.StopBurning" );
	PrecacheScriptSound( "General.BurningFlesh" );
	PrecacheScriptSound( "General.BurningObject" );
}

void CEntityFlame::Spawn()
{
	BaseClass::Spawn();
	m_flLifetime = gpGlobals->curtime;

	SetThink( &CEntityFlame::FlameThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
	//Send to the client even though we don't have a model
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );


}
							    

//-----------------------------------------------------------------------------
// Since we don't save/load danger links, we need to reacquire them here
//-----------------------------------------------------------------------------
void CEntityFlame::Activate()
{
	BaseClass::Activate();


}


void CEntityFlame::UseCheapEffect( bool bCheap )
{
	m_bCheapEffect = bCheap;
}


//-----------------------------------------------------------------------------
// Purpose: Creates a flame and attaches it to a target entity.
// Input  : pTarget - 
//-----------------------------------------------------------------------------
CEntityFlame *CEntityFlame::Create( CBaseEntity *pTarget, float flLifetime, float flSize /*= 0.0f*/, bool bUseHitboxes /*= true*/ )
{
	CEntityFlame *pFlame = (CEntityFlame *)CreateEntityByName( "entityflame" );

	if ( pFlame == NULL )
		return NULL;

	if ( flSize <= 0.0f )
	{
		float xSize = pTarget->CollisionProp()->OBBMaxs().x - pTarget->CollisionProp()->OBBMins().x;
		float ySize = pTarget->CollisionProp()->OBBMaxs().y - pTarget->CollisionProp()->OBBMins().y;
		flSize = ( xSize + ySize ) * 0.5f;
		if ( flSize < 16.0f )
		{
			flSize = 16.0f;
		}
	}

	if ( flLifetime <= 0.0f )
	{
		flLifetime = 2.0f;
	}

	pFlame->m_flSize = flSize;
	pFlame->Spawn();
	UTIL_SetOrigin( pFlame, pTarget->GetAbsOrigin() );
	pFlame->AttachToEntity( pTarget );
	pFlame->SetLifetime( flLifetime );
	pFlame->Activate();

	return pFlame;
}


//-----------------------------------------------------------------------------
// Purpose: Attaches the flame to an entity and moves with it
// Input  : pTarget - target entity to attach to
//-----------------------------------------------------------------------------
void CEntityFlame::AttachToEntity( CBaseEntity *pTarget )
{
	// For networking to the client.
	m_hEntAttached = pTarget;

	if( pTarget->IsNPC() )
	{
		EmitSound( "General.BurningFlesh" );
	}
	else
	{
		EmitSound( "General.BurningObject" );
	}

	m_bPlayingSound = true;

	// So our heat emitter follows the entity around on the server.
	SetParent( pTarget );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : lifetime - 
//-----------------------------------------------------------------------------
void CEntityFlame::SetLifetime( float lifetime )
{
	m_flLifetime = gpGlobals->curtime + lifetime;
}

float CEntityFlame::GetRemainingLife( void ) const
{
	return m_flLifetime - gpGlobals->curtime;
}


//-----------------------------------------------------------------------------
// Purpose: Burn targets around us
//-----------------------------------------------------------------------------
void CEntityFlame::FlameThink( void )
{
	// Assure that this function will be ticked again even if we early-out in the if below.
	SetNextThink( gpGlobals->curtime + FLAME_DAMAGE_INTERVAL );

	if ( !m_hEntAttached.Get() )
	{
		UTIL_Remove( this );
		return;
	}

	if ( m_hEntAttached->GetFlags() & FL_TRANSRAGDOLL )
	{
		SetRenderAlpha( 0 );
		return;
	}

	CAI_BaseNPC *pNPC = m_hEntAttached->MyNPCPointer();
	if ( pNPC && !pNPC->IsAlive() )
	{
		UTIL_Remove( this );
		// Notify the NPC that it's no longer burning!
		pNPC->Extinguish();
		return;
	}

	if ( m_hEntAttached->GetWaterLevel() > 0 )
	{
		Vector mins, maxs;

		mins = m_hEntAttached->WorldSpaceCenter();
		maxs = mins;

		maxs.z = m_hEntAttached->WorldSpaceCenter().z;
		maxs.x += 32;
		maxs.y += 32;
		
		mins.z -= 32;
		mins.x -= 32;
		mins.y -= 32;

		UTIL_Bubbles( mins, maxs, 12 );
	}

	// See if we're done burning, or our attached ent has vanished
	if ( m_flLifetime < gpGlobals->curtime || m_hEntAttached == NULL )
	{
		EmitSound( "General.StopBurning" );
		m_bPlayingSound = false;
		SetThink( &CEntityFlame::SUB_Remove );
		SetNextThink( gpGlobals->curtime + 0.5f );

		// Notify anything we're attached to
		if ( m_hEntAttached )
		{
			CBaseCombatCharacter *pAttachedCC = m_hEntAttached->MyCombatCharacterPointer();

			if( pAttachedCC )
			{
				// Notify the NPC that it's no longer burning!
				pAttachedCC->Extinguish();
			}
		}

		return;
	}

	if ( m_hEntAttached )
	{
		// Do radius damage ignoring the entity I'm attached to. This will harm things around me.
		RadiusDamage( CTakeDamageInfo( this, this, 4.0f, DMG_BURN ), GetAbsOrigin(), m_flSize/2, CLASS_NONE, m_hEntAttached );

		// Directly harm the entity I'm attached to. This is so we can precisely control how much damage the entity
		// that is on fire takes without worrying about the flame's position relative to the bodytarget (which is the
		// distance that the radius damage code uses to determine how much damage to inflict)
		m_hEntAttached->TakeDamage( CTakeDamageInfo( this, this, FLAME_DIRECT_DAMAGE, DMG_BURN | DMG_DIRECT ) );

		if( !m_hEntAttached->IsNPC() && hl2_episodic.GetBool() )
		{
			const float ENTITYFLAME_MOVE_AWAY_DIST = 24.0f;
			// Make a sound near my origin, and up a little higher (in case I'm on the ground, so NPC's still hear it)
			CSoundEnt::InsertSound( SOUND_MOVE_AWAY, GetAbsOrigin(), ENTITYFLAME_MOVE_AWAY_DIST, 0.1f, this, SOUNDENT_CHANNEL_REPEATED_DANGER );
			CSoundEnt::InsertSound( SOUND_MOVE_AWAY, GetAbsOrigin() + Vector( 0, 0, 48.0f ), ENTITYFLAME_MOVE_AWAY_DIST, 0.1f, this, SOUNDENT_CHANNEL_REPEATING );
		}
	}
	else
	{
		RadiusDamage( CTakeDamageInfo( this, this, FLAME_RADIUS_DAMAGE, DMG_BURN ), GetAbsOrigin(), m_flSize/2, CLASS_NONE, NULL );
	}

	FireSystem_AddHeatInRadius( GetAbsOrigin(), m_flSize/2, 2.0f );

}  


//-----------------------------------------------------------------------------
// Igniter
//-----------------------------------------------------------------------------
class CEnvEntityIgniter : public CBaseEntity 
{
public:
	DECLARE_CLASS( CEnvEntityIgniter, CBaseEntity );
	DECLARE_DATADESC();

	virtual void Precache();

protected:
	void InputIgnite( inputdata_t &inputdata );
	float m_flLifetime;
};


BEGIN_DATADESC( CEnvEntityIgniter )

	DEFINE_KEYFIELD( m_flLifetime, FIELD_FLOAT, "lifetime" ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Ignite", InputIgnite ),

END_DATADESC()


LINK_ENTITY_TO_CLASS( env_entity_igniter, CEnvEntityIgniter );


//-----------------------------------------------------------------------------
// Purpose: Ignites entities
//-----------------------------------------------------------------------------
void CEnvEntityIgniter::Precache()
{
	BaseClass::Precache();
	UTIL_PrecacheOther( "entityflame" );
}


//-----------------------------------------------------------------------------
// Purpose: Ignites entities
//-----------------------------------------------------------------------------
void CEnvEntityIgniter::InputIgnite( inputdata_t &inputdata )
{
	if ( m_target == NULL_STRING )
		return;

	CBaseEntity *pTarget = NULL;
	while ( (pTarget = gEntList.FindEntityGeneric(pTarget, STRING(m_target), this, inputdata.pActivator)) != NULL )
	{
		// Combat characters know how to catch themselves on fire.
		CBaseCombatCharacter *pBCC = pTarget->MyCombatCharacterPointer();
		if (pBCC)
		{
			// DVS TODO: consider promoting Ignite to CBaseEntity and doing everything here
			pBCC->Ignite( m_flLifetime );
			continue;
		}

		// Everything else, we handle here.
		CEntityFlame::Create( pTarget, m_flLifetime );
	}
}

