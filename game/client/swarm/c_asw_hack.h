#ifndef _DEFINED_C_ASW_HACK
#define _DEFINED_C_ASW_HACK

#include "c_asw_marine_resource.h"

class CUserCmd;
class C_ASW_Marine;
class C_ASW_Player;
namespace vgui { 
	class Panel;
};

class C_ASW_Hack : public C_BaseEntity
{
public:
	C_ASW_Hack();

	DECLARE_CLASS( C_ASW_Hack, C_BaseEntity );
	DECLARE_CLIENTCLASS();	

	C_ASW_Marine_Resource* GetHackerMarineResource();
	C_BaseEntity* GetHackTarget();

	virtual void ASWPostThink(C_ASW_Player *pPlayer, C_ASW_Marine *pMarine,  CUserCmd *ucmd, float fDeltaTime) { }
	virtual void ReverseTumbler(int i, C_ASW_Marine *pMarine) { }
	virtual void FrameDeleted(vgui::Panel *pPanel) { }
	virtual bool CanOverrideHack() { return false; }
	virtual float GetTumblerProgress() { return 0; }
	virtual C_BasePlayer *GetPredictionOwner( void );
	virtual void PostDataUpdate( DataUpdateType_t updateType );
	
	CNetworkHandle (C_ASW_Marine_Resource, m_hHackerMarineResource); 	// marine info of the marine hacking
	CNetworkHandle (C_BaseEntity, m_hHackTarget);
	CNetworkVar(int, m_iShowOption);
private:
	C_ASW_Hack( const C_ASW_Hack & ); // not defined, not accessible
};

#endif /* _DEFINED_C_ASW_HACK */