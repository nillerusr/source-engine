#ifndef _DEFINED_C_ASW_USE_AREA_H
#define _DEFINED_C_ASW_USE_AREA_H

#include "iasw_client_usable_entity.h"
#include "c_triggers.h"

class C_ASW_Use_Area : public C_BaseTrigger, public IASW_Client_Usable_Entity
{
	DECLARE_CLASS( C_ASW_Use_Area, C_BaseTrigger );
	DECLARE_CLIENTCLASS();
public:
	C_ASW_Use_Area();
	
	C_BaseEntity* GetUseTarget();

	EHANDLE	m_hUseTarget;
	EHANDLE	GetUseTargetHandle() { return m_hUseTarget; }
	CNetworkVar(bool, m_bUseAreaEnabled);

	// IASW_Client_Usable_Entity implementation

	virtual C_BaseEntity* GetEntity() { return this; }
	virtual bool IsUsable(C_BaseEntity *pUser);
	virtual bool GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser);
	virtual void CustomPaint(int ix, int iy, int alpha, vgui::Panel *pUseIcon) { }
	virtual bool ShouldPaintBoxAround() { return true; }
	virtual bool NeedsLOSCheck() { return false; }

	// the entity that should glow when a marine can use this area
	virtual C_BaseEntity* GetGlowEntity() { return NULL; }

	CNetworkHandle( C_BaseEntity, m_hPanelProp );

protected:
	C_ASW_Use_Area( const C_ASW_Use_Area & ); // not defined, not accessible		
};

#endif /* _DEFINED_C_ASW_USE_AREA_H */