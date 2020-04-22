//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "mdllib_common.h"

//-----------------------------------------------------------------------------
// Global instance
//-----------------------------------------------------------------------------
CMdlLib mdllib;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CMdlLib, IMdlLib, MDLLIB_INTERFACE_VERSION, mdllib );




//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CMdlLib::~CMdlLib()
{
}


//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CMdlLib::Connect( CreateInterfaceFn factory )
{
// 	g_pFileSystem = (IFileSystem*)factory( FILESYSTEM_INTERFACE_VERSION, NULL );
// 	return ( g_pFileSystem != NULL );
	return true;
}

void CMdlLib::Disconnect()
{
//	g_pFileSystem = NULL;
	return;
}
	

//-----------------------------------------------------------------------------
// Purpose: Startup
//-----------------------------------------------------------------------------
InitReturnVal_t CMdlLib::Init()
{
	return INIT_OK;
}


//-----------------------------------------------------------------------------
// Purpose: Cleanup
//-----------------------------------------------------------------------------
void CMdlLib::Shutdown()
{
	return;
}

//-----------------------------------------------------------------------------
// Query interface
//-----------------------------------------------------------------------------
void *CMdlLib::QueryInterface( const char *pInterfaceName )
{
	return Sys_GetFactoryThis()( pInterfaceName, NULL );
}




