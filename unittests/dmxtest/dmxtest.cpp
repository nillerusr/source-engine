//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Unit test program for DMX testing
//
// $NoKeywords: $
//=============================================================================//

#include "unitlib/unitlib.h"
#include "filesystem.h"
#include "datamodel/idatamodel.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "tier3/tier3dm.h"


//-----------------------------------------------------------------------------
// Used to connect/disconnect the DLL
//-----------------------------------------------------------------------------
class CDmxTestAppSystem : public CTier3DmAppSystem< IAppSystem >
{
	typedef CTier3DmAppSystem< IAppSystem > BaseClass;

public:
	CDmxTestAppSystem()
	{
	}

	virtual bool Connect( CreateInterfaceFn factory ) 
	{
		if ( !BaseClass::Connect( factory ) )
			return false;

		if ( !g_pFullFileSystem || !g_pDataModel || !g_pDmElementFramework )
			return false;

		return true; 
	}

	virtual InitReturnVal_t Init()
	{
		MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );
		return BaseClass::Init();
	}
};

USE_UNITTEST_APPSYSTEM( CDmxTestAppSystem )
