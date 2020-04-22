//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dt_stack.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CDatatableStack::CDatatableStack( CSendTablePrecalc *pPrecalc, unsigned char *pStructBase, int objectID )
{
	m_pPrecalc = pPrecalc;

	m_pStructBase = pStructBase;
	m_ObjectID = objectID;
	
	m_iCurProp = 0;
	m_pCurProp = NULL;

	m_bInitted = false;

#ifdef _DEBUG
	memset( m_pProxies, 0xFF, sizeof( m_pProxies ) );
#endif
}


void CDatatableStack::Init( bool bExplicitRoutes )
{
	if ( bExplicitRoutes )
	{
		memset( m_pProxies, 0xFF, sizeof( m_pProxies[0] ) * m_pPrecalc->m_ProxyPaths.Count() );
	}
	else
	{
		// Walk down the tree and call all the datatable proxies as we hit them.
		RecurseAndCallProxies( &m_pPrecalc->m_Root, m_pStructBase );
	}

	m_bInitted = true;
}


