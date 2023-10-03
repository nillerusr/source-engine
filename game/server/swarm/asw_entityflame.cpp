//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Flame entity to be attached to target entity. Serves two purposes:
//
//			1) An entity that can be placed by a level designer and triggered
//			   to ignite a target entity.
//
//			2) An entity that can be created at runtime to ignite a target entity.
//
//=============================================================================//

#include "cbase.h"
#include "asw_entityflame.h"
#include "asw_fire.h"
#include "shareddefs.h"
#include "ai_basenpc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_DATADESC( CASW_EntityFlame )

	DEFINE_FUNCTION( ASWFlameThink ),

	DEFINE_FIELD( m_fDamageInterval, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDamagePerInterval, FIELD_FLOAT ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( entityflame, CASW_EntityFlame );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CASW_EntityFlame::CASW_EntityFlame( void )
{
	m_fDamageInterval = FLAME_DAMAGE_INTERVAL;
	m_flDamagePerInterval = FLAME_DIRECT_DAMAGE_PER_SEC * FLAME_DAMAGE_INTERVAL;	
}

void CASW_EntityFlame::Spawn( void )
{
	BaseClass::Spawn();

	SetThink( &CASW_EntityFlame::ASWFlameThink );
}

void CASW_EntityFlame::ASWFlameThink( void )
{
	if ( m_hEntAttached )
	{
		if ( m_hEntAttached->GetFlags() & FL_TRANSRAGDOLL )
		{
			SetRenderAlpha( 0 );
			return;
		}

		CAI_BaseNPC *pNPC = m_hEntAttached->MyNPCPointer();
		if ( pNPC && !pNPC->IsAlive() )
		{
			return;
		}

		if( m_hEntAttached->GetWaterLevel() > 0 )
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
	}
	else
	{
		UTIL_Remove( this );
		return;
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
		// Directly harm the entity I'm attached to. This is so we can precisely control how much damage the entity
		// that is on fire takes without worrying about the flame's position relative to the bodytarget (which is the
		// distance that the radius damage code uses to determine how much damage to inflict)

		m_hEntAttached->TakeDamage( CTakeDamageInfo( GetOwnerEntity() ? GetOwnerEntity() : this, this, m_flDamagePerInterval, DMG_BURN | DMG_DIRECT ) );

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
		RadiusDamage( CTakeDamageInfo( GetOwnerEntity() ? GetOwnerEntity() : this, this, FLAME_RADIUS_DAMAGE, DMG_BURN ), GetAbsOrigin(), m_flSize/2, CLASS_NONE, NULL );
	}

	SetNextThink( gpGlobals->curtime + m_fDamageInterval );
}  