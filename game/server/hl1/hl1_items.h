//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HL1_ITEMS_H
#define HL1_ITEMS_H
#ifdef _WIN32
#pragma once
#endif


#include "items.h"

class CHL1Item : public CItem
{
public:
	DECLARE_CLASS( CHL1Item, CItem );

	void Spawn( void );
	void Activate( void );
};


#endif // HL1_ITEMS_H
