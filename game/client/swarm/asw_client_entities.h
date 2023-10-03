//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose : Manages creating clientside entities
//
// $NoKeywords: $
//===========================================================================//

#ifndef _INCLUDED_ASW_CLIENT_ENTITIES_H
#define _INCLUDED_ASW_CLIENT_ENTITIES_H

#ifdef _WIN32
#pragma once
#endif

#include "igamesystem.h"


//------------------------------------------------------------------------------
// Purpose : Manages creating clientside entities
//------------------------------------------------------------------------------

class CASW_Client_Entities : public CAutoGameSystem
{
	// Inherited from IGameSystemPerFrame
public:
	virtual char const *Name() { return "Infested clientside entities"; }

	// Other public methods
public:
	CASW_Client_Entities();
	virtual void LevelInitPostEntity();
	virtual void LevelShutdownPreEntity();
};


#endif // _INCLUDED_ASW_CLIENT_ENTITIES_H
