//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: System to generate events as specified entities become visible to players.
//			This contains some specific early-outs and culling code, so it's not
//			exactly general purpose yet. (sjb)
//
// $NoKeywords: $
//=====================================================================================//

#include "cbase.h"
#include "cvisibilitymonitor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar debug_visibility_monitor("debug_visibility_monitor", "0", FCVAR_CHEAT );
ConVar vismon_poll_frequency("vismon_poll_frequency", ".5", FCVAR_CHEAT );
ConVar vismon_trace_limit("vismon_trace_limit", "12", FCVAR_CHEAT );

//=============================================================================
// This structure packages up an entity for the visibility monitor.
//=============================================================================
#define NO_VISIBILITY_MEMORY	0
struct visibility_target_t
{
	EHANDLE						entity;
	float						minDistSqr;
	int							memory;// A bit vector that remembers which clients have seen this thing already.
	VisibilityMonitorCallback	pfnCallback;
	VisibilityMonitorEvaluator  pfnEvaluator;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CVisibilityMonitor : public CAutoGameSystemPerFrame
{
public:
	CVisibilityMonitor( char const *name ) : CAutoGameSystemPerFrame( name )
	{
		m_flPollFrequency = 1.0f;
		m_flTimeNextPoll = 0.0f;
	}

	// Methods of IGameSystem
	virtual char const *Name() { return "VisibilityMonitor"; }

	virtual bool Init();
	virtual void FrameUpdatePostEntityThink();
	virtual bool EntityIsVisibleToPlayer( const visibility_target_t &target, CBasePlayer *pPlayer, int *numTraces );
	virtual void Shutdown();

	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity();
	virtual void LevelShutdownPreEntity();

	// Visibility Monitor Methods
	void AddEntity( CBaseEntity *pEntity, float flMinDist, VisibilityMonitorCallback pfnCallback, VisibilityMonitorEvaluator pfnEvaluator );
	void RemoveEntity( CBaseEntity *pEntity );

	bool IsTrackingEntity( CBaseEntity *pEntity );

private:
	CUtlVector<visibility_target_t> m_Entities;
	float							m_flPollFrequency;
	float							m_flTimeNextPoll;

	int								m_iMaxTracesPerThink;
	int								m_iMaxEntitiesPerThink;

	int								m_iStartElement;
};

//=========================================================
// Auto game system instantiation
//=========================================================
CVisibilityMonitor VisibilityMonitor( "CVisibilityMonitor" );

//---------------------------------------------------------
//---------------------------------------------------------
bool CVisibilityMonitor::Init()
{
	return true;
}

//---------------------------------------------------------
// Purpose: See if it is time to poll and do so. 
//
// SOON: We need to load-balance this. 
//---------------------------------------------------------
void CVisibilityMonitor::FrameUpdatePostEntityThink()
{
	if( gpGlobals->curtime < m_flTimeNextPoll )
		return;

	m_flTimeNextPoll = gpGlobals->curtime + vismon_poll_frequency.GetFloat();

	int iDebugging = debug_visibility_monitor.GetInt();

	if( m_Entities.Count() > m_iMaxEntitiesPerThink )
		m_iMaxEntitiesPerThink = m_Entities.Count();

	if( iDebugging > 1 )
	{
		Msg("\nVisMon: Polling now. (Frequency: %f)\n", m_flPollFrequency );
		Msg("VisMon: Time: %f - Tracking %d Entities. (Max:%d)\n", gpGlobals->curtime, m_Entities.Count(), m_iMaxEntitiesPerThink );
	}

	// Cleanup, dump entities that have been removed since we last polled.
	for ( int i = 0 ; i < m_Entities.Count() ; i++ )
	{
		if ( m_Entities[i].entity == NULL )
		{
			m_Entities.FastRemove(i);
			if ( i >= m_Entities.Count() )
			{
				break;
			}
		}
	}

	int numTraces = 0;
	bool bHitTraceLimit = false;

	if( m_iStartElement >= m_Entities.Count() )
	{
		if( iDebugging > 1 )
		{
			Msg("VisMon: RESET\n");
		}

		m_iStartElement = 0;
	}

	if( iDebugging > 1 )
	{
		Msg("VisMon: Starting at element: %d\n", m_iStartElement );
	}

	for( int i = m_iStartElement ; i < m_Entities.Count() ; i++ )
	{
		for( int j = 1 ; j <= gpGlobals->maxClients ; j++ )
		{
			CBasePlayer *pPlayer =UTIL_PlayerByIndex( j );

			if( pPlayer != NULL && pPlayer->IsAlive() && !pPlayer->IsBot() )
			{
				int memoryBit = 1 << j; // The bit that is used to remember whether a given entity has been seen by a given player.

				CBaseEntity *pSeeEntity = m_Entities[ i ].entity.Get();

				if( pSeeEntity == NULL )
				{
					continue;
				}

				if( !(m_Entities[i].memory & memoryBit) )
				{
					// If this player hasn't seen this entity yet, check it.
					if( EntityIsVisibleToPlayer( m_Entities[i], pPlayer, &numTraces ) )
					{
						bool bIgnore = false;

						if( m_Entities[i].pfnEvaluator != NULL && !m_Entities[i].pfnEvaluator( m_Entities[i].entity, pPlayer ) )
						{
							bIgnore = true;
						}

						// See it! Generate our event.
						if( iDebugging > 0 )
						{
							if( bIgnore )
							{
								Msg("VisMon: Player %s IGNORING VISIBILE Entity: %s\n", pPlayer->GetDebugName(), pSeeEntity->GetDebugName() );
								NDebugOverlay::Cross3D( pSeeEntity->WorldSpaceCenter(), 16, 255, 0, 0, false, 10.0f );
							}
							else
							{
								Msg("VisMon: Player %s sees Entity: %s\n", pPlayer->GetDebugName(), pSeeEntity->GetDebugName() );
								NDebugOverlay::Cross3D( pSeeEntity->WorldSpaceCenter(), 16, 0, 255, 0, false, 10.0f );
							}
						}

						if( !bIgnore )
						{
							bool bGenerateEvent = true;

							if( m_Entities[i].pfnCallback != NULL )
							{
								// Make the callback, and let it determine whether to generate the simple event.
								bGenerateEvent = m_Entities[i].pfnCallback( m_Entities[i].entity, pPlayer );
							}

							if( bGenerateEvent )
							{
								// No callback, generate the generic game event.
								IGameEvent * event = gameeventmanager->CreateEvent( "entity_visible" );
								if ( event )
								{
									event->SetInt( "userid", pPlayer->GetUserID() );
									event->SetInt( "subject", pSeeEntity->entindex() );
									event->SetString( "classname", pSeeEntity->GetClassname() );
									event->SetString( "entityname", STRING( pSeeEntity->GetEntityName() ) );
									gameeventmanager->FireEvent( event );
								}
							}

							// Remember that this entity was visible to the player
							m_Entities[i].memory |= memoryBit;
						}
					}
				}
			}
		}

		if( numTraces >= vismon_trace_limit.GetInt() )
		{
			if( iDebugging > 1 )
			{
				Msg("VisMon: MAX Traces. Stopping after element %d\n", i );
			}

			m_iStartElement = i + 1; // Pick up here next think.
			bHitTraceLimit = true;
			break;
		}
	}

	if( !bHitTraceLimit )
	{
		m_iStartElement = 0;
	}

	if( numTraces > m_iMaxTracesPerThink )
		m_iMaxTracesPerThink = numTraces;

	if( iDebugging > 1 )
	{
		Msg("VisMon: %d traces performed during this polling cycle (Max: %d)\n\n", numTraces, m_iMaxTracesPerThink );
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CVisibilityMonitor::EntityIsVisibleToPlayer( const visibility_target_t &target, CBasePlayer *pPlayer, int *numTraces )
{
	CBaseCombatCharacter *pEyeEntity = pPlayer->ActivePlayerCombatCharacter();

	Vector vecTargetOrigin = target.entity->WorldSpaceCenter();
	Vector vecPlayerOrigin = pEyeEntity->EyePosition();

	float flDistSqr = vecPlayerOrigin.DistToSqr( vecTargetOrigin );

	if( flDistSqr <= target.minDistSqr )
	{
		// Increment the counter of traces done during this polling cycle
		*numTraces += 1;

		trace_t tr;
		int mask = MASK_BLOCKLOS_AND_NPCS & ~CONTENTS_BLOCKLOS;
		UTIL_TraceLine( vecPlayerOrigin, vecTargetOrigin, mask, pEyeEntity, COLLISION_GROUP_NONE, &tr );

		if( tr.fraction == 1.0f || tr.m_pEnt == target.entity )
			return true;

		if( debug_visibility_monitor.GetInt() > 1 )
		{
			NDebugOverlay::Line( vecPlayerOrigin, vecTargetOrigin, 255, 0, 0, false, vismon_poll_frequency.GetFloat() );
		}
	}

	return false;
}

//---------------------------------------------------------
//---------------------------------------------------------
void CVisibilityMonitor::Shutdown()
{
}

//---------------------------------------------------------
//---------------------------------------------------------
void CVisibilityMonitor::LevelInitPreEntity()
{
}

//---------------------------------------------------------
//---------------------------------------------------------
void CVisibilityMonitor::LevelInitPostEntity()
{
	m_iMaxTracesPerThink = 0;
	m_iMaxEntitiesPerThink = 0;
	m_iStartElement = 0;
	m_flTimeNextPoll = gpGlobals->curtime + vismon_poll_frequency.GetFloat();
}

//---------------------------------------------------------
//---------------------------------------------------------
void CVisibilityMonitor::LevelShutdownPreEntity()
{
	m_Entities.RemoveAll();
}

//---------------------------------------------------------
//---------------------------------------------------------
void CVisibilityMonitor::AddEntity( CBaseEntity *pEntity, float flMinDist, VisibilityMonitorCallback pfnCallback, VisibilityMonitorEvaluator pfnEvaluator )
{
	Assert( pEntity != NULL );

	if( !IsTrackingEntity( pEntity ) )
	{
		visibility_target_t newTarget;

		newTarget.entity = pEntity;
		newTarget.minDistSqr = Square(flMinDist);
		newTarget.memory = NO_VISIBILITY_MEMORY;
		newTarget.pfnCallback = pfnCallback;
		newTarget.pfnEvaluator = pfnEvaluator;

		m_Entities.AddToTail( newTarget );

		if( debug_visibility_monitor.GetBool() )
		{
			Msg("VisMon: Added Entity: %s (%s)\n", pEntity->GetClassname(), pEntity->GetDebugName() );
		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void CVisibilityMonitor::RemoveEntity( CBaseEntity *pEntity )
{
	Assert( pEntity != NULL );

	for( int i = 0 ; i < m_Entities.Count() ; i++ )
	{
		if( m_Entities[i].entity == pEntity )
		{
			m_Entities.Remove( i );

			if( debug_visibility_monitor.GetBool() )
			{
				Msg("VisMon: Removed Entity: %s (%s)\n", pEntity->GetClassname(), pEntity->GetDebugName() );
			}

			return;
		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CVisibilityMonitor::IsTrackingEntity( CBaseEntity *pEntity )
{
	Assert( pEntity != NULL );

	for( int i = 0 ; i < m_Entities.Count() ; i++ )
	{
		if( m_Entities[i].entity == pEntity )
		{
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------
// Purpose: Adds an entity to the list of entities that 
// the visibility monitor is tracking.
//---------------------------------------------------------
void VisibilityMonitor_AddEntity( CBaseEntity *pEntity, float flMinDist, VisibilityMonitorCallback pfnCallback, VisibilityMonitorEvaluator pfnEvaluator )
{
	VisibilityMonitor.AddEntity( pEntity, flMinDist, pfnCallback, pfnEvaluator );
}

//---------------------------------------------------------
// Purpose: Remove (stop looking for) this entity
//---------------------------------------------------------
void VisibilityMonitor_RemoveEntity( CBaseEntity *pEntity )
{
	VisibilityMonitor.RemoveEntity( pEntity );
}

