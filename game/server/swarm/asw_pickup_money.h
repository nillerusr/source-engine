#ifndef _INCLUDED_ASW_PICKUP_MONEY_H
#define _INCLUDED_ASW_PICKUP_MONEY_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_pickup.h"

class CSprite;
class CSpriteTrail;

class CASW_Pickup_Money : public CASW_Pickup
{
	DECLARE_CLASS( CASW_Pickup_Money, CASW_Pickup );
	DECLARE_SERVERCLASS();
public:
	CASW_Pickup_Money(void);
	virtual ~CASW_Pickup_Money(void);

	virtual void Spawn();
	virtual void Precache();
	virtual void ItemTouch( CBaseEntity *pOther );
	virtual void CreateEffects();

	// money isn't picked up using E
	virtual bool AllowedToPickup(CASW_Marine *pMarine) { return false; }
	virtual bool IsUsable(CBaseEntity *pUser) { return false; }

	CHandle<CSprite>		m_pMainGlow;
	CHandle<CSpriteTrail>	m_pGlowTrail;
};

#endif // _INCLUDED_ASW_PICKUP_MONEY_H