//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "../EventLog.h"

class CHL1EventLog : public CEventLog
{
private:
	typedef CEventLog BaseClass;

public:
	virtual ~CHL1EventLog() {};

public:
	bool PrintEvent( IGameEvent * event )	// override virtual function
	{
		if ( BaseClass::PrintEvent( event ) )
		{
			return true;
		}
	
		if ( Q_strcmp(event->GetName(), "hl1_") == 0 )
		{
			return PrintHL1Event( event );
		}

		return false;
	}

protected:

	bool PrintHL1Event( IGameEvent * event )	// print Mod specific logs
	{
	//	const char * name = event->GetName() + Q_strlen("hl1_"); // remove prefix

		return false;
	}

};

CHL1EventLog g_HL1EventLog;

//-----------------------------------------------------------------------------
// Singleton access
//-----------------------------------------------------------------------------
IGameSystem* GameLogSystem()
{
	return &g_HL1EventLog;
}

