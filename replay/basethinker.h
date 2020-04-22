//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef BASETHINKER_H
#define BASETHINKER_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "ithinker.h"

//----------------------------------------------------------------------------------------

//
// Adds/removes itself from think manager and implements a default ShouldThink().
//
class CBaseThinker : public IThinker
{
public:
					CBaseThinker();
	virtual			~CBaseThinker();

protected:
	virtual void	Think();
	virtual bool	ShouldThink() const;
	virtual void	PostThink();

	// Derived classes must implement this.
	// Return 0 to think every frame.
	// Return -1 to never think.
	virtual float	GetNextThinkTime() const = 0;

private:
	float			m_flNextThinkTime;
};

//----------------------------------------------------------------------------------------

#endif // BASETHINKER_H
