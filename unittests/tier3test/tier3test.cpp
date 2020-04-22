//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Unit test program for tier3 level libraries testing
//
// $NoKeywords: $
//=============================================================================//

#include "unitlib/unitlib.h"
#include "filesystem.h"
#include "tier3/tier3.h"
#include "mathlib/mathlib.h"


//-----------------------------------------------------------------------------
// Used to connect/disconnect the DLL
//-----------------------------------------------------------------------------
class CTier3TestAppSystem : public CTier3AppSystem< IAppSystem >
{
	typedef CTier3AppSystem< IAppSystem > BaseClass;

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

USE_UNITTEST_APPSYSTEM( CTier3TestAppSystem )
