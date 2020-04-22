//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "sv_recordingsessionblock.h"
#include "qlimits.h"
#include "sv_fileservercleanup.h"
#include "sv_replaycontext.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

CServerRecordingSessionBlock::CServerRecordingSessionBlock( IReplayContext *pContext )
:	CBaseRecordingSessionBlock( pContext ),
	m_nWriteStatus( WRITESTATUS_INVALID ),
	m_pPublisher( NULL )
{
}

bool CServerRecordingSessionBlock::Read( KeyValues *pIn )
{
	if ( !BaseClass::Read( pIn ) )
		return false;

	m_nWriteStatus = (WriteStatus_t)pIn->GetInt( "write_status", (int)WRITESTATUS_INVALID );	Assert( m_nWriteStatus != WRITESTATUS_INVALID );
	V_strcpy_safe( m_szFullFilename, pIn->GetString( "filename" ) );			Assert( V_strlen( m_szFullFilename ) > 0 );

	return true;
}

void CServerRecordingSessionBlock::Write( KeyValues *pOut )
{
	BaseClass::Write( pOut );

	pOut->SetInt( "write_status", (int)m_nWriteStatus );	Assert( m_nWriteStatus != WRITESTATUS_INVALID );
	pOut->SetString( "filename", m_szFullFilename );
}

void CServerRecordingSessionBlock::OnDelete()
{
	BaseClass::OnDelete();

	SV_GetFileserverCleaner()->MarkFileForDelete( V_UnqualifiedFileName( m_szFullFilename ) );
}

//----------------------------------------------------------------------------------------
