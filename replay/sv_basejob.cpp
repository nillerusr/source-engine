//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "sv_basejob.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

CBaseJob::CBaseJob( JobPriority_t priority/*=JP_NORMAL*/,
				    ISpewer *pSpewer/*=g_pDefaultSpewer*/ )
:	CJob( priority ),
	CBaseSpewer( pSpewer ),
	m_nError( ERROR_NONE )
{
	m_szError[ 0 ] = '\0';
}

void CBaseJob::SetError( int nError, const char *pError )
{
	m_nError = nError;

	if ( pError )
	{
		V_strcpy( m_szError, pError );
	}
}

//----------------------------------------------------------------------------------------