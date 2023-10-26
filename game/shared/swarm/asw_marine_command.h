//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef MARINE_COMMAND_H
#define MARINE_COMMAND_H
#pragma once


#include "edict.h"
#include "usercmd.h"


class IMoveHelper;
class CMoveData;
class CBasePlayer;

// Marine movement
class CMarineMove
{
public:
	DECLARE_CLASS_NOBASE( CMarineMove );
	
	// Construction/destruction
					CMarineMove( void );
	virtual			~CMarineMove( void ) {}
	
	// Prepare for running movement
	virtual void	SetupMarineMove( const CBasePlayer *player, CBaseEntity *marine, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );

	// Finish movement
	virtual void	FinishMarineMove( const CBasePlayer *player, CBaseEntity *marine, CUserCmd *ucmd, CMoveData *move );
};


//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
CMarineMove *MarineMove();


#endif // MARINE_COMMAND_H
