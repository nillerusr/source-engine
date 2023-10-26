#ifndef _DEFINED_ASW_USE_AREA_H
#define _DEFINED_ASW_USE_AREA_H

#include "triggers.h"
#include "asw_shareddefs.h"
#include "iasw_server_usable_entity.h"

class CASW_Player;

DECLARE_AUTO_LIST( IASW_Use_Area_List );

class CASW_Use_Area : public CTriggerMultiple, public IASW_Server_Usable_Entity, public IASW_Use_Area_List
{
	DECLARE_CLASS( CASW_Use_Area, CTriggerMultiple );
public:
	CASW_Use_Area();
	void Spawn( void );

	virtual int				ShouldTransmit( const CCheckTransmitInfo *pInfo );
	virtual int				UpdateTransmitState();

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	string_t m_iUseTargetName;	// the name of the door area's door; linked into m_pParent during Activate()
	CNetworkHandle( CBaseEntity, m_hUseTarget );	// the door we're attached to
	CNetworkVar( bool, m_bUseAreaEnabled );
	int m_nPlayersRequired;

	COutputEvent m_OnRequirementFailed;

	virtual Class_T Classify() { return CLASS_HACKED_ROLLERMINE; }

	// IASW_Server_Usable_Entity implementation
	virtual CBaseEntity* GetEntity() { return this; }
	virtual bool IsUsable(CBaseEntity *pUser);
	virtual bool RequirementsMet(CBaseEntity *pUser);
	// fill in by subclasses
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType ) { }
	virtual void MarineStartedUsing(CASW_Marine* pMarine) { }
	virtual void MarineStoppedUsing(CASW_Marine* pMarine) { }
	virtual void MarineUsing(CASW_Marine* pMarine, float fDeltaTime) { } 
	virtual void ActivateMultiTrigger(CBaseEntity *pActivator);
	virtual bool NeedsLOSCheck() { return false; }

	virtual void UpdateWaitingForInput() {}
	virtual void UpdatePanelSkin() {}

	// override these and do our own enabling/disabling
	virtual void InputEnable( inputdata_t &inputdata );
	virtual void InputDisable( inputdata_t &inputdata );
	virtual void InputToggle( inputdata_t &inputdata );

	string_t m_szPanelPropName;
	bool m_bMultiplePanelProps;
	virtual CBaseEntity* GetProp() { return m_hPanelProp; }
	// link to the prop representing us
	CNetworkHandle( CBaseEntity, m_hPanelProp );
};

#endif /* _DEFINED_ASW_USE_AREA_H */