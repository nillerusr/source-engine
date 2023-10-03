//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#if !defined( KBUTTON_H )
#define KBUTTON_H
#ifdef _WIN32
#pragma once
#endif

#include "shareddefs.h"

struct kbutton_t
{
	struct Split_t
	{
		// key nums holding it down
		int		down[ 2 ];		
		// low bit is down state
		int		state;			
	};

	Split_t		&GetPerUser( int nSlot = -1 );

	Split_t		m_PerUser[ MAX_SPLITSCREEN_PLAYERS ];
};

#endif // KBUTTON_H
