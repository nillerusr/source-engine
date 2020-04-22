//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#include "filesystem.h"
#include "dedicated.h"
#include <stdio.h>
#include <stdlib.h>
#include "interface.h"
#include <string.h>
#include <malloc.h>
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "tier0/dbg.h"
#include "../filesystem/basefilesystem.h"
#include "appframework/AppFramework.h"
#include "tier2/tier2.h"


extern IFileSystem *g_pFileSystem;
extern IBaseFileSystem *g_pBaseFileSystem;

// implement our own special factory that we don't export outside of the DLL, to stop
// people being able to get a pointer to a FILESYSTEM_INTERFACE_VERSION stdio interface
void* FileSystemFactory(const char *pName, int *pReturnCode)
{
	{
		if ( !Q_stricmp(pName, FILESYSTEM_INTERFACE_VERSION ) )
		{
			if ( pReturnCode )
			{
				*pReturnCode = IFACE_OK;
			}
			return g_pFileSystem;
		}
		if ( !Q_stricmp(pName, BASEFILESYSTEM_INTERFACE_VERSION ) )
		{
			if ( pReturnCode )
			{
				*pReturnCode = IFACE_OK;
			}
			return g_pBaseFileSystem;
		}
	}

	if ( pReturnCode )
	{
		*pReturnCode = IFACE_FAILED;
	}
	return NULL;
}
