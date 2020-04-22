//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "replay/screenshot.h"
#include "replay/replayutils.h"
#include "replay/iclientreplaycontext.h"
#include "KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

bool CReplayScreenshot::Read( KeyValues *pIn )
{
	m_nWidth = pIn->GetInt( "w" );
	m_nHeight = pIn->GetInt( "h" );
	V_strcpy_safe( m_szBaseFilename, pIn->GetString( "file", "" ) );

	return true;
}

void CReplayScreenshot::Write( KeyValues *pOut )
{
	pOut->SetInt( "w", m_nWidth );
	pOut->SetInt( "h", m_nHeight );
	pOut->SetString( "file", m_szBaseFilename );
}

const char *CReplayScreenshot::GetSubKeyTitle() const
{
	return m_szBaseFilename;
}

const char *CReplayScreenshot::GetPath() const
{
	extern IClientReplayContext *g_pClientReplayContext;
	return Replay_va( "%s%s%c", g_pClientReplayContext->GetBaseDir(), SUBDIR_SCREENSHOTS, CORRECT_PATH_SEPARATOR );
}

//----------------------------------------------------------------------------------------