//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef SV_RECORDINGSESSION_H
#define SV_RECORDINGSESSION_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "baserecordingsession.h"
#include "basethinker.h"
#include "utlbuffer.h"
#include "sv_filepublish.h"
#include "replay/replaytime.h"
#include "sessioninfoheader.h"

//----------------------------------------------------------------------------------------

class IFilePublisher;

//----------------------------------------------------------------------------------------

class CServerRecordingSession : public CBaseRecordingSession
{
	typedef CBaseRecordingSession BaseClass;
public:
	CServerRecordingSession( IReplayContext *pContext );
	~CServerRecordingSession();

	virtual bool	Read( KeyValues *pIn );
	virtual void	Write( KeyValues *pOut );
	virtual void	OnDelete();
	virtual void	SetLocked( bool bLocked );

	virtual void	PopulateWithRecordingData( int nCurrentRecordingStartTick );

	void			NotifyReplayRequested()			{ m_bReplaysRequested = true; }

	double			GetSecondsToExpiration() const;
	bool			SessionExpired() const;

#ifdef _DEBUG
	void			VerifyLocks();
#endif

private:
	virtual bool	ShouldDitchSession() const;

	bool			m_bReplaysRequested;
	int				m_nLifeSpan;
	CReplayTime		m_RecordTime;
};

//----------------------------------------------------------------------------------------

inline CServerRecordingSession *SV_CastSession( CBaseRecordingSession *pSession )
{
	return static_cast< CServerRecordingSession * >( pSession );
}

//----------------------------------------------------------------------------------------

#endif // SV_RECORDINGSESSION_H
