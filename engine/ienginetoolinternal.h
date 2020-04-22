//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Methods of IEngineTool visible only inside the engine
//
//=============================================================================

#ifndef IENGINETOOLINTERNAL_H
#define IENGINETOOLINTERNAL_H

#include "toolframework/ienginetool.h"


//-----------------------------------------------------------------------------
// Purpose: Singleton implementation of external tools callback interface
//-----------------------------------------------------------------------------
class IEngineToolInternal : public IEngineTool
{
public:
	virtual void	SetIsInGame( bool bIsInGame ) = 0;
};

extern IEngineToolInternal *g_pEngineToolInternal;


#endif // IENGINETOOLINTERNAL_H

