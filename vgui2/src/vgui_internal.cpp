//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core implementation of vgui
//
// $NoKeywords: $
//=============================================================================//

#include "vgui_internal.h"

#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui/IPanel.h>
#include "filesystem.h"
#include <vstdlib/IKeyValuesSystem.h>

#include <stdio.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace vgui
{

ISurface *g_pSurface = NULL;
IPanel *g_pIPanel = NULL;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void *InitializeInterface( char const *interfaceName, CreateInterfaceFn *factoryList, int numFactories )
{
	void *retval;

	for ( int i = 0; i < numFactories; i++ )
	{
		CreateInterfaceFn factory = factoryList[ i ];
		if ( !factory )
			continue;

		retval = factory( interfaceName, NULL );
		if ( retval )
			return retval;
	}

	// No provider for requested interface!!!
	// assert( !"No provider for requested interface!!!" );

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool VGui_InternalLoadInterfaces( CreateInterfaceFn *factoryList, int numFactories )
{
	// loads all the interfaces
	g_pSurface = (ISurface *)InitializeInterface(VGUI_SURFACE_INTERFACE_VERSION, factoryList, numFactories );
//	g_pKeyValues = (IKeyValues *)InitializeInterface(KEYVALUES_INTERFACE_VERSION, factoryList, numFactories );
	g_pIPanel = (IPanel *)InitializeInterface(VGUI_PANEL_INTERFACE_VERSION, factoryList, numFactories );

	if (g_pSurface && /*g_pKeyValues &&*/ g_pIPanel)
		return true;

	return false;
}

} // namespace vgui
