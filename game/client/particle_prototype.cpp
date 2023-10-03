//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


PrototypeEffectLink *g_pPrototypeEffects = 0;

PrototypeEffectLink::PrototypeEffectLink(PrototypeEffectCreateFn fn, const char *pName)
{
	m_CreateFn = fn;
	m_pEffectName = pName;
	m_pNext = g_pPrototypeEffects;
	g_pPrototypeEffects = this;
}


