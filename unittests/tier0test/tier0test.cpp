//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Unit test program for testing of tier0 libraries
//
// $NoKeywords: $
//=============================================================================//

#include "unitlib/unitlib.h"
#include "appframework/IAppSystem.h"

//-----------------------------------------------------------------------------
// Used to connect/disconnect the DLL
//-----------------------------------------------------------------------------
class CTier0TestAppSystem : public CTier0AppSystem< IAppSystem >
{
	typedef CTier0AppSystem< IAppSystem > BaseClass;

public:
	virtual bool Connect( CreateInterfaceFn factory ) 
	{
		if ( !BaseClass::Connect( factory ) )
			return false;
		return true;
	}

	virtual InitReturnVal_t Init()
	{
		return INIT_OK;
	}

	virtual void Shutdown()
	{
		BaseClass::Shutdown();
	}
};

USE_UNITTEST_APPSYSTEM( CTier0TestAppSystem )
