//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef ITHINKER_H
#define ITHINKER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "interface.h"

//----------------------------------------------------------------------------------------

class IThinker : public IBaseInterface
{
public:
	virtual void		Think() = 0;
	virtual void		PostThink() = 0;
	virtual bool		ShouldThink() const = 0;
};

//----------------------------------------------------------------------------------------

#endif // ITHINKER_H
