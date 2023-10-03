//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#if !defined EVENTLOG_H
#define EVENTLOG_H

#ifdef _WIN32
#pragma once
#endif

#include "GameEventListener.h"
#include <igamesystem.h>

class CEventLog : public CGameEventListener, public CBaseGameSystem
{
	
public:
	CEventLog();
	virtual ~CEventLog();

public: // IGameEventListener Interface
	
	virtual void FireGameEvent( IGameEvent * event );
	
public: // CBaseGameSystem overrides

	virtual bool Init();
	//virtual void Shutdown() {}

	virtual void FormatPlayer( CBaseEntity *ent, char *str, int len ) const;
	const char *FormatPlayer( CBaseEntity *ent ) const;

	enum
	{
		PLAYER_LOGINFO_SIZE = 256,
	};

protected:

	virtual bool PrintEvent( IGameEvent * event );
	virtual bool PrintGameEvent( IGameEvent * event );
	virtual bool PrintPlayerEvent( IGameEvent * event );
	virtual bool PrintTeamEvent( IGameEvent * event );
	virtual bool PrintOtherEvent( IGameEvent * event );
};

extern CEventLog *GameLogSystem();

#endif // EVENTLOG_H
