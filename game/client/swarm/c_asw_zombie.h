#ifndef _INCLUDED_C_ASW_ZOMBIE_H
#define _INCLUDED_C_ASW_ZOMBIE_H

#include "c_ai_basenpc.h"
#include "iasw_client_aim_target.h"

class C_ASW_Zombie : public C_AI_BaseNPC, public IASW_Client_Aim_Target
{
public:
	DECLARE_CLASS( C_ASW_Zombie, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();

					C_ASW_Zombie();
	virtual			~C_ASW_Zombie();

	// aim target interface
	IMPLEMENT_AUTO_LIST_GET();
	virtual float GetRadius() { return 23; }
	virtual bool IsAimTarget() { return true; }
	virtual const Vector& GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming) { return WorldSpaceCenter(); }
	virtual const Vector& GetAimTargetRadiusPos(const Vector &vecFiringSrc) { return WorldSpaceCenter(); }
	
	CNetworkVar(bool, m_bOnFire);
	virtual void UpdateFireEmitters();
	virtual void UpdateOnRemove();

private:
	C_ASW_Zombie( const C_ASW_Zombie & ); // not defined, not accessible
};

#endif /* _INCLUDED_C_ASW_ZOMBIE_H */