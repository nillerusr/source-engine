//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// An RTS!
//=============================================================================

#ifndef LEGION_H
#define LEGION_H

#ifdef _WIN32
#pragma once
#endif

#include "appframework/vguimatsysapp.h"
#include "tier1/convar.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CLegionApp;


//-----------------------------------------------------------------------------
// Singleton interfaces
//-----------------------------------------------------------------------------
extern CLegionApp *g_pApp;


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CLegionApp : public CVguiMatSysApp, public IConCommandBaseAccessor
{
	typedef CVguiMatSysApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit( );
	virtual int Main();
	virtual void PostShutdown( );
	virtual const char *GetAppName() { return "Legion"; }

	// Methods of IConCommandBaseAccessor
	virtual bool RegisterConCommandBase( ConCommandBase *pCommand );
	virtual void UnregisterConCommandBase( ConCommandBase *pCommand );

	// Promote to public
	void AppPumpMessages() { BaseClass::AppPumpMessages(); }

private:
};


#endif // LEGION_H


