//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HLTVDEMO_H
#define HLTVDEMO_H
#ifdef _WIN32
#pragma once
#endif

#include <filesystem.h>

#include "demo.h"
#include "demofile.h"

class CHLTVFrame;

class CHLTVDemoRecorder : public IDemoRecorder
{
public:
	CHLTVDemoRecorder();
	virtual ~CHLTVDemoRecorder();

	CDemoFile *GetDemoFile();
	int		GetRecordingTick( void );

	void	StartRecording( const char *filename, bool bContinuously );
	void	StartAutoRecording();
	void	SetSignonState( int state ) {}; // not need by HLTV recorder
	bool	IsRecording( void );
	void	PauseRecording( void ) {};
	void	ResumeRecording( void ) {};
	void	StopRecording( void );
	
	void	RecordCommand( const char *cmdstring );
	void	RecordUserInput( int cmdnumber ) {} ;  // not need by HLTV recorder
	void	RecordMessages( bf_read &data, int bits );
	void	RecordPacket( void ); 
	void	RecordServerClasses( ServerClass *pClasses );
	void	RecordStringTables(); 

	void	ResetDemoInterpolation( void ) {};

public:
	void	WriteFrame( CHLTVFrame *pFrame );
	void	CloseFile();
	void	Reset();

	void	WriteServerInfo();
	int		WriteSignonData();  // write all necessary signon data and returns written bytes
	void	WriteMessages( unsigned char cmd, bf_write &message );
	int		GetMaxAckTickCount();

public:

	CDemoFile		m_DemoFile;
	bool			m_bIsRecording;
	int				m_nFrameCount;
	float			m_nStartTick;
	int				m_SequenceInfo;
	int				m_nDeltaTick;	
	int				m_nSignonTick;
	bf_write		m_MessageData; // temp buffer for all network messages
};



#endif // HLTVDEMO_H
