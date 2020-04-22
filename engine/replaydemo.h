//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef REPLAYDEMO_H
#define REPLAYDEMO_H
#ifdef _WIN32
#pragma once
#endif

#include <filesystem.h>

#include "demo.h"
#include "demofile.h"

#define TMP_REPLAY_FILENAME		".replay.tmp"

class CReplayFrame;
class CReplayServer;
class CRecordingSessionBlock;

class CReplayDemoRecorder : public IDemoRecorder
{
public:
	CReplayDemoRecorder( CReplayServer* pServer );
	virtual ~CReplayDemoRecorder();

	CDemoFile *GetDemoFile();
	int		GetRecordingTick();

private:
	friend class CReplayServer;		// Calls to Start/StopRecording() should be funneled through CReplayServer

	void	StartRecording();
	void	StopRecording();

	const char *GetDemoFilename();

public:
	void	SetSignonState( int state ) {}
	bool	IsRecording();
	void	PauseRecording() {}
	void	ResumeRecording() {}

	void	RecordCommand( const char *cmdstring );
	void	RecordUserInput( int cmdnumber ) {} ;  // not need by Replay recorder
	void	RecordMessages( bf_read &data, int bits );
	void	RecordPacket(); 
	void	RecordServerClasses( ServerClass *pClasses );
	void	RecordStringTables(); 

	void	ResetDemoInterpolation() {}

	void	WriteFrame( CReplayFrame *pFrame );
	void	CloseFile();

	void	WriteServerInfo();
	int		WriteSignonData();  // write all necessary signon data and returns written bytes
	void	WriteMessages( unsigned char cmd, bf_write &message );
	int		GetMaxAckTickCount();

	void	GetUniqueDemoFilename( char* pOut, int nLength );

	const char *GetRecordingFilename();

public:
	CDemoFile		m_DemoFile;

	// TODO: I believe this can be removed
	char			m_szDumpFilename[MAX_OSPATH];	// The name of the file name to which we are currently (or will eventually)
													// write the demo, depending on cvar, "replay_record_directly_to_disk"
	bool			m_bIsRecording;
	bool			m_bWrittenFirstFullUpdate;
	int				m_nFrameCount;
	int				m_nStartTick;
	int				m_SequenceInfo;
	int				m_nDeltaTick;	
	int				m_nSignonTick;
	bf_write		m_MessageData; // temp buffer for all network messages
	CReplayServer	*m_pReplayServer;

private:
	void	StartRecording( const char *pFilename, bool bContinuously );
};



#endif // REPLAYDEMO_H
