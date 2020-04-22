//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "basethinker.h"
#include "ithinkmanager.h"
#include "replay/ienginereplay.h"
#include "dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

extern IThinkManager *g_pThinkManager;
extern IEngineReplay *g_pEngine;

//----------------------------------------------------------------------------------------

CBaseThinker::CBaseThinker()
:	m_flNextThinkTime( 0.0f )
{
	g_pThinkManager->AddThinker( this );
}

CBaseThinker::~CBaseThinker()
{
	g_pThinkManager->RemoveThinker( this );
}

void CBaseThinker::Think()
{
	AssertMsg( ShouldThink(), "Thinking before ready - Think() being called explicitly?  Let the think manager call Think()." );
}

bool CBaseThinker::ShouldThink() const
{
	const float flHostTime = g_pEngine->GetHostTime();
	return m_flNextThinkTime >= 0.0f && flHostTime >= m_flNextThinkTime;
}

void CBaseThinker::PostThink()
{
	m_flNextThinkTime = GetNextThinkTime();
}

//----------------------------------------------------------------------------------------
