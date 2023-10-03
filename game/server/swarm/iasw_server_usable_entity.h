#ifndef _INCLUDED_ASW_SERVER_USABLE_ENTITY_H
#define _INCLUDED_ASW_SERVER_USABLE_ENTITY_H

class CASW_Marine;
class CASW_Player;

abstract_class IASW_Server_Usable_Entity
{
public:	
	virtual CBaseEntity* GetEntity() = 0;
	virtual bool IsUsable(CBaseEntity *pUser) = 0;
	virtual bool RequirementsMet(CBaseEntity *pUser) = 0;
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType ) = 0;
	virtual void MarineStartedUsing(CASW_Marine* pMarine) = 0;
	virtual void MarineStoppedUsing(CASW_Marine* pMarine) = 0;
	virtual void MarineUsing(CASW_Marine* pMarine, float fDeltaTime) = 0;
	virtual bool NeedsLOSCheck() = 0;
};

#endif // _INCLUDED_ASW_SERVER_USABLE_ENTITY_H