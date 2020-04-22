//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "thinkmanager.h"
#include "ithinker.h"
#include "replay/ienginereplay.h"
#include "replay_dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

extern IEngineReplay *g_pEngine;

//----------------------------------------------------------------------------------------

void CThinkManager::AddThinker( IThinker *pThinker )
{
	Assert( m_lstManagers.Find( pThinker ) == m_lstManagers.InvalidIndex() );
	m_lstManagers.AddToTail( pThinker );
}

void CThinkManager::RemoveThinker( IThinker *pThinker )
{
	int it = m_lstManagers.Find( pThinker );		Assert( it != m_lstManagers.InvalidIndex() );
	m_lstManagers.Remove( it );
}

void CThinkManager::Think()
{
	FOR_EACH_LL( m_lstManagers, i )
	{
		IThinker *pCurThinker = m_lstManagers[ i ];
		if ( !pCurThinker->ShouldThink() )
			continue;

		pCurThinker->Think();
		pCurThinker->PostThink();
	}
}

//----------------------------------------------------------------------------------------

static CThinkManager s_ThinkManager;
IThinkManager *g_pThinkManager = &s_ThinkManager;

//----------------------------------------------------------------------------------------
