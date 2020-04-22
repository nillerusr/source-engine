//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "replay/replaytime.h"
#include "KeyValues.h"
#include <time.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

CReplayTime::CReplayTime()
:	m_fDate( 0 ),
	m_fTime( 0 )
{
}

void CReplayTime::InitDateAndTimeToNow()
{
	tm now;
	VCRHook_LocalTime( &now );
	SetDate( now.tm_mday, now.tm_mon + 1, now.tm_year + 1900 );
	SetTime( now.tm_hour, now.tm_min, now.tm_sec );
}

void CReplayTime::SetDate( int nDay, int nMonth, int nYear )
{
	Assert( nDay >= 1 && nDay <= 31 );
	Assert( nMonth >= 1 && nMonth <= 12 );
	Assert( nYear >= 2009 && nYear <= 2136 );

	m_fDate  = nDay - 1;
	m_fDate |= ( ( nMonth - 1 ) << 5 );
	m_fDate |= ( ( nYear - 2009 ) << 9 );

#ifdef _DEBUG
	int nDbgDay, nDbgMonth, nDbgYear;
	GetDate( nDbgDay, nDbgMonth, nDbgYear );
	Assert( nDay == nDbgDay );
	Assert( nMonth == nDbgMonth );
	Assert( nYear == nDbgYear );
#endif
}

void CReplayTime::GetDate( int &nDay, int &nMonth, int &nYear ) const
{
	nDay   = 1 + ( m_fDate & 0x1F );				// Bits 0-4 for day
	nMonth = 1 + ( ( m_fDate >> 5 ) & 0x0F );		// Bits 5-8 for month
	nYear  = 2009 + ( ( m_fDate >> 9 ) & 0x7F );	// Bits 9-15 for year

	Assert( nDay >= 1 && nDay <= 31 );
	Assert( nMonth >= 1 && nMonth <= 12 );
	Assert( nYear >= 2009 && nYear <= 2136 );
}

void CReplayTime::SetTime( int nHour, int nMin, int nSec )
{
	Assert( nHour >= 0 && nHour <= 23 );
	Assert( nMin  >= 0 && nMin  <= 59 );
	Assert( nSec  >= 0 && nSec  <= 59 );
	
	m_fTime  = nHour;
	m_fTime |= ( ( nMin ) << 5 ); 
	m_fTime |= ( ( nSec ) << 11 );

#ifdef _DEBUG
	int nDbgHour, nDbgMin, nDbgSec;
	GetTime( nDbgHour, nDbgMin, nDbgSec );
	Assert( nHour == nDbgHour );
	Assert( nMin == nDbgMin );
	Assert( nSec == nDbgSec );
#endif
}

void CReplayTime::GetTime( int &nHour, int &nMin, int &nSec ) const
{
	nHour = m_fTime & 0x1F;				// Bits  0-4 for hour
	nMin  = ( m_fTime >> 5 ) & 0x3F;		// Bits  5-10 for min
	nSec  = ( m_fTime >> 11 ) & 0x3F;		// Bits  11-16 for sec

	Assert( nHour >= 0 && nHour <= 23 );
	Assert( nMin  >= 0 && nMin  <= 59 );
	Assert( nSec  >= 0 && nSec  <= 59 );
}

void CReplayTime::Read( KeyValues *pIn )
{
	m_fDate = pIn->GetInt( "date" );
	m_fTime = pIn->GetInt( "time" );
}

void CReplayTime::Write( KeyValues *pOut )
{
	pOut->SetInt( "date", m_fDate );
	pOut->SetInt( "time", m_fTime );
}

/*static*/ const wchar_t *CReplayTime::GetLocalizedMonth( vgui::ILocalize *pLocalize, int nMonth )
{
	char szMonthKey[32];	// Get localized month

	V_snprintf( szMonthKey, sizeof( szMonthKey ), "#Month_%i", nMonth );
	wchar_t *pResult = pLocalize->Find( szMonthKey );

	return pResult ? pResult : L"";
}

/*static*/ const wchar_t *CReplayTime::GetLocalizedDay( vgui::ILocalize *pLocalize, int nDay )
{
	char szDay[8];		// Convert day to wide
	static wchar_t s_wDay[8];

	V_snprintf( szDay, sizeof( szDay ), "%i", nDay );
	pLocalize->ConvertANSIToUnicode( szDay, s_wDay, sizeof( s_wDay ) );

	return s_wDay;
}

/*static*/ const wchar_t *CReplayTime::GetLocalizedYear( vgui::ILocalize *pLocalize, int nYear )
{
	char szYear[8];		// Convert year to wide
	static wchar_t s_wYear[8];

	V_snprintf( szYear, sizeof( szYear ), "%i", nYear );
	pLocalize->ConvertANSIToUnicode( szYear, s_wYear, sizeof( s_wYear ) );

	return s_wYear;
}

/*static*/ const wchar_t *CReplayTime::GetLocalizedTime( vgui::ILocalize *pLocalize, int nHour, int nMin, int nSec )
{
	char szTime[16];	// Convert time to wide
	static wchar_t s_wTime[16];
	V_snprintf( szTime, sizeof( szTime ), "%i:%02i %s", nHour % 12, nMin, nHour < 12 ? "AM" : "PM" );
	pLocalize->ConvertANSIToUnicode( szTime, s_wTime, sizeof( s_wTime ) );

	return s_wTime;
}

/*static*/ const wchar_t *CReplayTime::GetLocalizedDate( vgui::ILocalize *pLocalize, const CReplayTime &t,
														 bool bForceFullFormat/*=false*/ )
{
	int nHour, nMin, nSec;
	int nDay, nMonth, nYear;
	t.GetTime( nHour, nMin, nSec );
	t.GetDate( nDay, nMonth, nYear );
	return GetLocalizedDate( pLocalize, nDay, nMonth, nYear, &nHour, &nMin, &nSec, bForceFullFormat );
}

/*static*/ const wchar_t *CReplayTime::GetLocalizedDate( vgui::ILocalize *pLocalize, int nDay, int nMonth, int nYear,
														 int *pHour/*=NULL*/, int *pMin/*=NULL*/, int *pSec/*=NULL*/,
														 bool bForceFullFormat/*=false*/ )
{
	static wchar_t s_wBuf[256];

	// Is this collection for replays from today?
	time_t today;
	time( &today );
	tm *pNowTime = localtime( &today );
	bool bToday = ( pNowTime->tm_mday == nDay ) && ( pNowTime->tm_mon + 1 == nMonth ) && ( 1900 + pNowTime->tm_year == nYear );

	// Yesterday?
	time_t yesterday = today - time_t( 86400 );
	tm *pYesterdayTime = localtime( &yesterday );
	bool bYesterday = ( pYesterdayTime->tm_mday == nDay ) && ( pYesterdayTime->tm_mon + 1 == nMonth ) && ( 1900 + pYesterdayTime->tm_year == nYear );

	const wchar_t *pMonth = GetLocalizedMonth( pLocalize, nMonth );
	const wchar_t *pDay = GetLocalizedDay( pLocalize, nDay );
	const wchar_t *pYear = GetLocalizedYear( pLocalize, nYear );
	const wchar_t *pToday = pLocalize->Find( "#Replay_Today" );
	const wchar_t *pYesterday = pLocalize->Find( "#Replay_Yesterday" );

	bool bTime = pHour && pMin && pSec;

	// Include time in formatted string?
	if ( bTime )
	{
		const wchar_t *pTime = GetLocalizedTime( pLocalize, *pHour, *pMin, *pSec );

		if ( bForceFullFormat || ( !bToday && !bYesterday ) )
		{
			pLocalize->ConstructString(
				s_wBuf,
				sizeof( s_wBuf ),
				pLocalize->Find( "#Replay_DateAndTime" ),
				4,
				pMonth, pDay, pYear, pTime
			);
		}
		else
		{
			pLocalize->ConstructString(
				s_wBuf,
				sizeof( s_wBuf ),
				pLocalize->Find( "#Replay_SingleWordDateAndTime" ),
				2,
				bToday ? pToday : pYesterday,
				pTime
			);
		}
	}
	else
	{
		if ( !bToday && !bYesterday )
		{
			pLocalize->ConstructString(
				s_wBuf,
				sizeof( s_wBuf ),
				pLocalize->Find( "#Replay_Date" ),
				3,
				pMonth, pDay, pYear
			);
		}
		else
		{
			V_wcsncpy( s_wBuf, bToday ? pToday : pYesterday, sizeof( s_wBuf ) );
		}
	}

	return s_wBuf;
}

/*static*/ const char *CReplayTime::FormatTimeString( int nSecs )
{
	static int nWhichStr = 0;
	static const int nNumStrings = 2;
	static const int nStrLen = 32;
	static char s_szResult[nNumStrings][nStrLen];

	char *pResult = s_szResult[ nWhichStr ];

	int nSeconds = nSecs % 60;
	int nMins = nSecs / 60;
	int nHours = nMins / 60;
	nMins %= 60;

	if ( nHours > 0 )
	{
		V_snprintf( pResult, nStrLen, "%i:%02i:%02i", nHours, nMins, nSeconds );
	}
	else
	{
		V_snprintf( pResult, nStrLen, "%02i:%02i", nMins, nSeconds );
	}

	nWhichStr = ( nWhichStr + 1 ) % nNumStrings;

	return pResult;
}

/*static*/ const char *CReplayTime::FormatPreciseTimeString( float flSecs )
{
	static int nWhichStr = 0;
	static const int nNumStrings = 2;
	static const int nStrLen = 32;
	static char s_szResult[nNumStrings][nStrLen];

	char *pResult = s_szResult[ nWhichStr ];

	int nSecs = (int)flSecs;
	int nMins = ( nSecs % 3600 ) / 60;
	int nSeconds = nSecs % 60;
	int nMilliseconds = (flSecs - (float)nSecs) * 10.0f;

	V_snprintf( pResult, nStrLen, "%02i:%02i:%02i", nMins, nSeconds, nMilliseconds );

	nWhichStr = ( nWhichStr + 1 ) % nNumStrings;

	return pResult;
}

//----------------------------------------------------------------------------------------
