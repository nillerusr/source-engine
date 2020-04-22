//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HOLIDAY_GIFT_H
#define HOLIDAY_GIFT_H
#ifdef _WIN32
#pragma once
#endif


#include "items.h"


class CHolidayGift: public CItem
{
public:
	DECLARE_CLASS( CHolidayGift, CItem );

public:
	
	virtual void Precache();
	virtual void Spawn( void );
	virtual bool MyTouch( CBasePlayer *pBasePlayer );
	virtual void ItemTouch( CBaseEntity *pOther );
	void DropSoundThink( void );

public:

	static CHolidayGift* Create( const Vector &position, const QAngle &angles, const QAngle &eyeAngles, const Vector &velocity, CBaseCombatCharacter *pOwner );
};


#endif // HOLIDAY_GIFT_H
