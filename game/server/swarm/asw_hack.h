#ifndef _DEFINED_ASW_HACK
#define _DEFINED_ASW_HACK

#include "baseentity.h"
#include "asw_marine_resource.h"

class CASW_Marine;
class CASW_Player;
class CUserCmd;
class CASW_Player;
class CASW_Marine;

class CASW_Hack : public CBaseEntity
{
public:
	CASW_Hack();
	//CASW_Hack(CASW_Player* pHackingPlayer, CASW_Marine* pHackingMarine, CBaseEntity* pHackTarget);

	DECLARE_CLASS( CASW_Hack, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual bool InitHack(CASW_Player* pHackingPlayer, CASW_Marine* pHackingMarine, CBaseEntity* pHackTarget);
	virtual void SelectHackOption(int i) { } 	// the currently hacking marine has chosen option i on this hack

	virtual int	UpdateTransmitState();
	virtual void ASWPostThink(CASW_Player *pPlayer, CASW_Marine *pMarine,  CUserCmd *ucmd, float fDeltaTime) { }
	virtual void ReverseTumbler(int i, CASW_Marine *pMarine) { }

	virtual void MarineStoppedUsing(CASW_Marine* pMarine);
	virtual bool IsDownloadingFiles() { return false; }

	virtual void OnHackComplete() { }

	CBaseEntity* GetHackTarget() { return m_hHackTarget.Get(); }

	CNetworkHandle (CASW_Marine_Resource, m_hHackerMarineResource); 	// marine info of the marine hacking
	CNetworkHandle (CBaseEntity, m_hHackTarget);
	CNetworkVar(int, m_iShowOption);

	CASW_Player* GetHackingPlayer();
	EHANDLE m_hHackingPlayer;
	CASW_Marine* GetHackingMarine();
	EHANDLE m_hHackingMarine;
};

#endif /* _DEFINED_ASW_HACK */