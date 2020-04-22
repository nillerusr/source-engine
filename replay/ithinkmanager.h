//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef ITHINKMANAGER_H
#define ITHINKMANAGER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "interface.h"

//----------------------------------------------------------------------------------------

class IThinker;

//----------------------------------------------------------------------------------------

class IThinkManager : public IBaseInterface
{
public:
	virtual void		AddThinker( IThinker *pThinker ) = 0;
	virtual void		RemoveThinker( IThinker *pThinker ) = 0;

	virtual void		Think() = 0;
};

//----------------------------------------------------------------------------------------

#endif // ITHINKMANAGER_H
