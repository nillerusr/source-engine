//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// sv_log.h
// Server logging functions

#ifndef SV_LOG_H
#define SV_LOG_H
#pragma once

#include <igameevents.h>
#include "netadr.h"

class CLog : public IGameEventListener2
{
public:
	CLog();
	virtual ~CLog();

public: // IGameEventListener Interface
	
	void FireGameEvent( IGameEvent *event );

public: 

	bool IsActive( void );	// true if logging is "on"
	void SetLoggingState( bool state );	// set the logging state to true (on) or false (off)
	
	bool UsingLogAddress( void );
	bool AddLogAddress(netadr_t addr);
	bool DelLogAddress(netadr_t addr);
	void DelAllLogAddress( void );
	void ListLogAddress( void );
	
	netadr_t GetLogAddress();
	
	void Open( void );  // opens logging file
	void Close( void );	// closes logging file
	void Flush( void ); // flushes the log file to disk

	void Init( void );
	void Reset( void );	// reset all logging streams
	void Shutdown( void );
	
	void Printf( PRINTF_FORMAT_STRING const char *fmt, ... ) FMTFUNCTION( 2, 3 );	// log a line to log file
	void Print( const char * text );
	void PrintServerVars( void ); // prints server vars to log file

	void RunFrame();

private:

	bool m_bActive;		// true if we're currently logging

	CUtlVector< netadr_t >	m_LogAddresses;		// Server frag log stream is sent to the address(es) in this list
	FileHandle_t			m_hLogFile;			// File where frag log is put.
	CUtlString				m_LogFilename;		// Name of our logfile.
	double					m_flLastLogFlush;
	bool					m_bFlushLog;
};

extern CLog g_Log;

#endif // SV_LOG_H
