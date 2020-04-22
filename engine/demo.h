//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef DEMO_H
#define DEMO_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"

#include "net.h"
#include "demofile/demoformat.h"

class CUtlBuffer;
class CDemoFile;
class ServerClass;

abstract_class IDemoRecorder 
{
public:
	~IDemoRecorder() {}

	virtual CDemoFile *GetDemoFile() = 0;
	virtual int		GetRecordingTick( void ) = 0;

	virtual void	StartRecording( const char *filename, bool bContinuously ) = 0;
	virtual void	SetSignonState( int state ) = 0;
	virtual bool	IsRecording( void ) = 0;
	virtual void	PauseRecording( void ) = 0;
	virtual void	ResumeRecording( void ) = 0;
	virtual void	StopRecording( void ) = 0;
	
	virtual void	RecordCommand( const char *cmdstring ) = 0;  // record a console command
	virtual void	RecordUserInput( int cmdnumber ) = 0;  // record a user input command
	virtual void	RecordMessages( bf_read &data, int bits ) = 0; // add messages to current packet
	virtual void	RecordPacket( void ) = 0; // packet finished, write all recorded stuff to file
	virtual void	RecordServerClasses( ServerClass *pClasses ) = 0; // packet finished, write all recorded stuff to file
	virtual void	RecordStringTables() = 0; 

	virtual void	ResetDemoInterpolation() = 0;
};

abstract_class IDemoPlayer
{
public:
	virtual ~IDemoPlayer() {};

	virtual CDemoFile *GetDemoFile() = 0;
	virtual int		GetPlaybackStartTick( void ) = 0;
	virtual int		GetPlaybackTick( void ) = 0;
	virtual int		GetTotalTicks( void ) = 0;
	
	virtual bool	StartPlayback( const char *filename, bool bAsTimeDemo ) = 0;

	virtual bool	IsPlayingBack( void ) = 0; // true if demo loaded and playing back
	virtual bool	IsPlaybackPaused( void ) = 0; // true if playback paused
	virtual bool	IsPlayingTimeDemo( void ) = 0; // true if playing back in timedemo mode
	virtual bool	IsSkipping( void ) = 0; // true, if demo player skiiping trough packets
	virtual bool	CanSkipBackwards( void ) = 0; // true if demoplayer can skip backwards

	virtual void	SetPlaybackTimeScale( float timescale ) = 0; // sets playback timescale
	virtual float	GetPlaybackTimeScale( void ) = 0; // get playback timescale

	virtual void	PausePlayback( float seconds ) = 0; // pause playback n seconds, -1 = forever
	virtual void	SkipToTick( int tick, bool bRelative, bool bPause ) = 0; // goto a specific tick, 0 = start, -1 = end
	virtual void	SetEndTick( int tick ) = 0;	// set end tick
	virtual void	ResumePlayback( void ) = 0; // resume playback
	virtual void	StopPlayback( void ) = 0;	// stop playback, close file
	virtual void	InterpolateViewpoint() = 0; // override viewpoint
	virtual netpacket_t *ReadPacket( void ) = 0; // read packet from demo file

	virtual void	ResetDemoInterpolation() = 0;
	virtual int		GetProtocolVersion() = 0;

	virtual bool	ShouldLoopDemos() = 0;		// if we're in "startdemos" - should we loop?
	virtual void	OnLastDemoInLoopPlayed() = 0;	// Last demo of "startdemos" just completed

	virtual bool	IsLoading( void ) = 0; // true if demo is currently loading
};

extern IDemoPlayer *demoplayer;	// reference to current demo player
extern IDemoRecorder *demorecorder; // reference to current demo recorder

#endif // DEMO_H
