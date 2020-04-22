//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Unit test program for testing of tier2 libraries
//
// $NoKeywords: $
//=============================================================================//

#include "unitlib/unitlib.h"
#include "filesystem.h"
#include "tier2/tier2.h"
#include "mathlib/mathlib.h"


//-----------------------------------------------------------------------------
// Used to connect/disconnect the DLL
//-----------------------------------------------------------------------------
class CTier2TestAppSystem : public CTier2AppSystem< IAppSystem >
{
	typedef CTier2AppSystem< IAppSystem > BaseClass;

public:
	virtual bool Connect( CreateInterfaceFn factory ) 
	{
		if ( !BaseClass::Connect( factory ) )
			return false;

		if ( !g_pFullFileSystem )
			return false;
		return true; 
	}

	virtual InitReturnVal_t Init()
	{
		MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

		InitReturnVal_t nRetVal = BaseClass::Init();
		if ( nRetVal != INIT_OK )
			return nRetVal;

		return INIT_OK;
	}
};

USE_UNITTEST_APPSYSTEM( CTier2TestAppSystem )
