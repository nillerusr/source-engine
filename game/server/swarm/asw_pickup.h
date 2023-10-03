#ifndef _DEFINED_ASW_PICKUP_H
#define _DEFINED_ASW_PICKUP_H

#include "items.h"
#include "asw_shareddefs.h"
#include "iasw_server_usable_entity.h"

class CASW_Player;
class CASW_Marine;

class CASW_Pickup : public CItem, public IASW_Server_Usable_Entity
{
public:
	DECLARE_CLASS( CASW_Pickup, CItem );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Spawn();

	bool m_bFreezePickup;	// if set, the pickup won't be physically simulated, but will be frozen in place

	virtual bool AllowedToPickup(CASW_Marine *pMarine) { return true; }

	// IASW_Server_Usable_Entity implementation
	virtual CBaseEntity* GetEntity() { return this; }
	virtual bool IsUsable(CBaseEntity *pUser);
	virtual bool RequirementsMet( CBaseEntity *pUser ) { return true; }
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );
	virtual void MarineUsing(CASW_Marine* pMarine, float deltatime) { }
	virtual void MarineStartedUsing(CASW_Marine* pMarine) { }
	virtual void MarineStoppedUsing(CASW_Marine* pMarine) { }
	virtual bool NeedsLOSCheck() { return true; }
};

#endif /* _DEFINED_ASW_PICKUP_H */