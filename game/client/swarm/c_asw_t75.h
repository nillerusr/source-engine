#ifndef _INCLUDED_C_ASW_T75_H
#define _INCLUDED_C_ASW_T75_H

#include "c_basecombatcharacter.h"
#include "iasw_client_usable_entity.h"

struct dlight_t;

class C_ASW_T75 : public C_BaseCombatCharacter, public IASW_Client_Usable_Entity
{
public:
	DECLARE_CLASS( C_ASW_T75, C_BaseCombatCharacter );
	DECLARE_CLIENTCLASS();

					C_ASW_T75();
	virtual			~C_ASW_T75();

	bool IsArmed() { return m_bArmed; }
	bool IsInUse() { return m_bIsInUse; }
	float GetArmProgress() { return m_flArmProgress; }

	int GetT75IconTextureID();	
	static bool s_bLoadedArmIcons;
	static int s_nArmIconTextureID;

	// IASW_Client_Usable_Entity
	virtual C_BaseEntity* GetEntity() { return this; }
	virtual bool IsUsable(C_BaseEntity *pUser);
	virtual bool GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser);
	virtual void CustomPaint(int ix, int iy, int alpha, vgui::Panel *pUseIcon);
	virtual bool ShouldPaintBoxAround() { return true; }
	virtual bool NeedsLOSCheck() { return true; }

	CNetworkVar( bool, m_bArmed );
	CNetworkVar( bool, m_bIsInUse );
	CNetworkVar( float, m_flArmProgress );
	CNetworkVar( float, m_flDetonateTime );

private:
	C_ASW_T75( const C_ASW_T75 & ); // not defined, not accessible
};

#endif /* _INCLUDED_C_ASW_T75_H */