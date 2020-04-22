//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "replay/performance.h"
#include "replay/iclientreplaycontext.h"
#include "replay/ireplayperformancemanager.h"
#include "replay/replayutils.h"
#include "KeyValues.h"
#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

extern IClientReplayContext *g_pClientReplayContext;

//----------------------------------------------------------------------------------------

CReplayPerformance::CReplayPerformance( CReplay *pReplay )
:	m_pReplay( pReplay ),
	m_nTickIn( -1 ),
	m_nTickOut( -1 )
{
	Assert( pReplay );
	m_szBaseFilename[ 0 ] = '\0';
	m_wszTitle[0] = L'\0';
}

CReplayPerformance::CReplayPerformance( const CReplayPerformance *pPerformance )
{
	Copy( pPerformance );
}

void CReplayPerformance::Read( KeyValues *pIn )
{
	SetFilename( pIn->GetString( "filename" ) );
	m_nTickIn = pIn->GetInt( "tick_in", -1 );
	m_nTickOut = pIn->GetInt( "tick_out", -1 );
	V_wcsncpy( m_wszTitle, pIn->GetWString( "title" ), sizeof( m_wszTitle ) );
}

void CReplayPerformance::Write( KeyValues *pOut )
{
	pOut->SetString( "filename", m_szBaseFilename );
	pOut->SetInt( "tick_in", m_nTickIn );
	pOut->SetInt( "tick_out", m_nTickOut );
	pOut->SetWString( "title", m_wszTitle );
}

void CReplayPerformance::Copy( const CReplayPerformance *pSrc )
{
	V_wcsncpy( m_wszTitle, pSrc->m_wszTitle, sizeof( m_wszTitle ) );
	V_strcpy( m_szBaseFilename, pSrc->m_szBaseFilename );

	m_pReplay = pSrc->m_pReplay;

	CopyTicks( pSrc );
}

void CReplayPerformance::CopyTicks( const CReplayPerformance *pSrc )
{
	m_nTickIn = pSrc->m_nTickIn;
	m_nTickOut = pSrc->m_nTickOut;
}

void CReplayPerformance::SetFilename( const char *pFilename )
{
	V_strcpy( m_szBaseFilename, pFilename );
}

const char *CReplayPerformance::GetFullPerformanceFilename()
{
	return Replay_va( "%s%s", g_pClientReplayContext->GetPerformanceManager()->GetFullPath(), m_szBaseFilename );
}

void CReplayPerformance::AutoNameIfHasNoTitle( const char *pMapName )
{
	if ( !m_wszTitle[ 0 ] )
	{
		Replay_GetAutoName( m_wszTitle, sizeof( m_wszTitle ), pMapName );
	}
}

void CReplayPerformance::SetTitle( const wchar_t *pTitle )
{
	V_wcsncpy( m_wszTitle, pTitle, sizeof( m_wszTitle ) );
}

CReplayPerformance *CReplayPerformance::MakeCopy() const
{
	return new CReplayPerformance( this );
}

//----------------------------------------------------------------------------------------
