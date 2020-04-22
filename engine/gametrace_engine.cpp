//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "gametrace.h"
#include "server.h"
#include "eiface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void CGameTrace::SetEdict( edict_t *pEdict )
{
	m_pEnt = serverGameEnts->EdictToBaseEntity( pEdict );
}


edict_t* CGameTrace::GetEdict() const
{
	return serverGameEnts->BaseEntityToEdict( m_pEnt );
}

