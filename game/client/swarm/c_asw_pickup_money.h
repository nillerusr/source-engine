#ifndef _INCLUDED_C_ASW_PICKUP_MONEY_H
#define _INCLUDED_C_ASW_PICKUP_MONEY_H

#include "c_asw_pickup.h"

class C_ASW_Pickup_Money : public C_ASW_Pickup
{
	DECLARE_CLASS( C_ASW_Pickup_Money, C_ASW_Pickup );
	DECLARE_CLIENTCLASS();
public:
	C_ASW_Pickup_Money();

	virtual bool GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser) { return false; }
	virtual bool IsUsable(C_BaseEntity *pUser) { return false; }
	virtual bool AllowedToPickup(C_ASW_Marine *pMarine) { return false; }
	virtual bool NeedsLOSCheck() { return false; }

protected:
	C_ASW_Pickup_Money( const C_ASW_Pickup_Money & ); // not defined, not accessible
};

#endif /* _INCLUDED_C_ASW_PICKUP_MONEY_H */