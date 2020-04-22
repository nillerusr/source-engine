//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "sv_recordingsession.h"
#include "sv_recordingsessionmanager.h"
#include "sv_replaycontext.h"
#include "sv_filepublish.h"
#include "sv_recordingsessionblock.h"
#include "vstdlib/jobthread.h"
#include "fmtstr.h"
#include "sv_fileservercleanup.h"
#include <time.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

#ifdef _DEBUG
ConVar replay_simulate_expired_sessions( "replay_simulate_expired_sessions", "0", FCVAR_DONTRECORD,
	"Simulate expired replay session data - the value of this cvar should be between 0 and 100 and is a probability - any cleanup done (via end of round cleanup or explicit replay_docleanup) will use this value to determine whether data is expired.  E.g, use a value of 100 to delete all sessions, or 50 for a 50 chance of a given session being considered expired.",
	true, 0.0f, true, 100.0f );
#endif

//----------------------------------------------------------------------------------------

CServerRecordingSession::CServerRecordingSession( IReplayContext *pContext )
:	CBaseRecordingSession( pContext ),
	m_bReplaysRequested( false ),
	m_nLifeSpan( 0 )
{
}

CServerRecordingSession::~CServerRecordingSession()
{
}

bool CServerRecordingSession::Read( KeyValues *pIn )
{
	if ( !BaseClass::Read( pIn ) )
		return false;

	m_nLifeSpan = pIn->GetInt( "lifespan", 0 );

	KeyValues *pRecordTimeSubKey = pIn->FindKey( "record_time" );
	if ( pRecordTimeSubKey )
	{
		m_RecordTime.Read( pRecordTimeSubKey );
	}

	return true;
}

void CServerRecordingSession::Write( KeyValues *pOut )
{
	BaseClass::Write( pOut );

	pOut->SetInt( "lifespan", m_nLifeSpan );

	KeyValues *pRecordTime = new KeyValues( "record_time" );
	pOut->AddSubKey( pRecordTime );
	m_RecordTime.Write( pRecordTime );
}

void CServerRecordingSession::OnDelete()
{
	BaseClass::OnDelete();

	SV_GetFileserverCleaner()->MarkFileForDelete( GetFilename() );
}

void CServerRecordingSession::SetLocked( bool bLocked )
{
	BaseClass::SetLocked( bLocked );

	// Propagate to contained blocks
	FOR_EACH_VEC( m_vecBlocks, i )
	{
		m_vecBlocks[ i ]->SetLocked( bLocked );
	}
}

void CServerRecordingSession::PopulateWithRecordingData( int nCurrentRecordingStartTick )
{
	BaseClass::PopulateWithRecordingData( nCurrentRecordingStartTick );

	// Create a new session name
	m_strName = SV_GetRecordingSessionManager()->GetNewSessionName();

	// Cache current date/time and life-span
	extern ConVar replay_data_lifespan;
	m_nLifeSpan = replay_data_lifespan.GetInt() * 24 * 3600;
	m_RecordTime.InitDateAndTimeToNow();
}

bool CServerRecordingSession::ShouldDitchSession() const
{
	return BaseClass::ShouldDitchSession() || !m_bReplaysRequested;
}

#ifdef _DEBUG
void CServerRecordingSession::VerifyLocks()
{
	const bool bLocked = IsLocked();
	FOR_EACH_VEC( m_vecBlocks, i )
	{
		AssertMsg( m_vecBlocks[ i ]->IsLocked() == bLocked, "Parent/child locks out of sync.  The block probably needs to inherit the parent's lock value on creation." );
	}
}
#endif

double CServerRecordingSession::GetSecondsToExpiration() const
{
	tm recordtime_tm;
	V_memset( &recordtime_tm, 0, sizeof( recordtime_tm ) );

	int nDay, nMonth, nYear;
	m_RecordTime.GetDate( nDay, nMonth, nYear );
	recordtime_tm.tm_mday = nDay;
	recordtime_tm.tm_mon = nMonth - 1;
	recordtime_tm.tm_year = nYear - 1900;

	int nHour, nMin, nSec;
	m_RecordTime.GetTime( nHour, nMin, nSec );
	recordtime_tm.tm_hour = nHour;
	recordtime_tm.tm_min = nMin;
	recordtime_tm.tm_sec = nSec;

	time_t recordtime = mktime( &recordtime_tm );

	time_t nowtime;
	time( &nowtime );

	double delta = m_nLifeSpan - difftime( nowtime, recordtime );

#ifdef DBGFLAG_ASSERT
	tm *pTest = localtime( &recordtime );
	Assert( recordtime_tm.tm_mday == pTest->tm_mday );
	Assert( recordtime_tm.tm_mon == pTest->tm_mon );
	Assert( recordtime_tm.tm_year == pTest->tm_year );
	Assert( recordtime_tm.tm_hour == pTest->tm_hour );
	Assert( recordtime_tm.tm_min == pTest->tm_min );
	Assert( recordtime_tm.tm_sec == pTest->tm_sec );
#endif

	return delta;
}

bool CServerRecordingSession::SessionExpired() const
{
#ifdef _DEBUG
	if ( ( 1+rand()%100 ) <= replay_simulate_expired_sessions.GetInt() )
		return true;
#endif

	return GetSecondsToExpiration() <= 0.0;
}

//----------------------------------------------------------------------------------------
