//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "../eventlog.h"
#include <KeyValues.h>

class CTF2EventLog : public CEventLog
{
private:
	typedef CEventLog BaseClass;

public:
	virtual ~CTF2EventLog() {};

public:
	bool PrintEvent( KeyValues * event )	// override virtual function
	{
		if ( BaseClass::PrintEvent( event ) )
		{
			return true;
		}
	
		if ( Q_strcmp(event->GetName(), "tf2_") == 0 )
		{
			return PrintTF2Event( event );
		}

		return false;
	}

protected:

	bool PrintTF2Event( KeyValues * event )	// print Mod specific logs
	{
	//	const char * name = event->GetName() + Q_strlen("tf2_"); // remove prefix

		return false;
	}

};

CTF2EventLog g_TF2EventLog;

//-----------------------------------------------------------------------------
// Singleton access
//-----------------------------------------------------------------------------
IGameSystem* GameLogSystem()
{
	return &g_TF2EventLog;
}

