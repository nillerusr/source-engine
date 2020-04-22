//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef IDOWNLOADSYSTEM_H
#define IDOWNLOADSYSTEM_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "interface.h"

#if defined( WIN32 ) && !defined( _X360 )	// DWORD
#include "winlite.h"
#include <windows.h>
#else
#include "platform.h"
#endif

//----------------------------------------------------------------------------------------

#define INTERFACEVERSION_DOWNLOADSYSTEM		"DownloadSystem001"

//----------------------------------------------------------------------------------------

struct RequestContext_t;

//----------------------------------------------------------------------------------------

class IDownloadSystem : public IBaseInterface
{
public:
	virtual DWORD CreateDownloadThread( RequestContext_t *pContext ) = 0;
};

//----------------------------------------------------------------------------------------

#endif // IDOWNLOADSYSTEM_H