//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Unit test program for testing of tier1 libraries
//
// $NoKeywords: $
//=============================================================================//

#include "unitlib/unitlib.h"
#include "tier1/tier1.h"
#include "mathlib/mathlib.h"
#include "tier1/convar.h"
#include "vstdlib/iprocessutils.h"


//-----------------------------------------------------------------------------
// Used to connect/disconnect the DLL
//-----------------------------------------------------------------------------
class CTier1TestAppSystem : public CTier1AppSystem< IAppSystem >
{
	typedef CTier1AppSystem< IAppSystem > BaseClass;

public:
	virtual bool Connect( CreateInterfaceFn factory ) 
	{
		if ( !BaseClass::Connect( factory ) )
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

	virtual void Shutdown()
	{
		BaseClass::Shutdown();
	}
};

USE_UNITTEST_APPSYSTEM( CTier1TestAppSystem )
