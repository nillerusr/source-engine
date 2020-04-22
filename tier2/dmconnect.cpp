//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A higher level link library for general use in the game and tools.
//
//===========================================================================//

#include <tier2/tier2.h>
#include "datamodel/idatamodel.h"
#include "dmserializers/idmserializers.h"


//-----------------------------------------------------------------------------
// Set up methods related to datamodel interfaces
//-----------------------------------------------------------------------------
bool ConnectDataModel( CreateInterfaceFn factory )
{
	if ( !g_pDataModel->Connect( factory ) )
		return false;

	if ( !g_pDmElementFramework->Connect( factory ) )
		return false;

	if ( !g_pDmSerializers->Connect( factory ) )
		return false;

	return true;
}

InitReturnVal_t InitDataModel()
{
	InitReturnVal_t nRetVal;

	nRetVal = g_pDataModel->Init( );
	if ( nRetVal != INIT_OK )
		return nRetVal;

	nRetVal = g_pDmElementFramework->Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	nRetVal = g_pDmSerializers->Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	return INIT_OK;
}

void ShutdownDataModel()
{
	g_pDmSerializers->Shutdown();
	g_pDmElementFramework->Shutdown();
	g_pDataModel->Shutdown( );
}

void DisconnectDataModel()
{
	g_pDmSerializers->Disconnect();
	g_pDmElementFramework->Disconnect();
	g_pDataModel->Disconnect();
}


