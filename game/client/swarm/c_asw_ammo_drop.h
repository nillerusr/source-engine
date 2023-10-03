#ifndef C_ASW_AMMO_DROP_H
#define C_ASW_AMMO_DROP_H

#include "iasw_client_usable_entity.h"
#include "glow_outline_effect.h"

#include <vgui/vgui.h>

class C_ASW_Marine;
class C_ASW_Weapon;

class C_ASW_Ammo_Drop : public C_BaseAnimating, public IASW_Client_Usable_Entity
{
public:
	DECLARE_CLASS( C_ASW_Ammo_Drop, C_BaseAnimating );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					C_ASW_Ammo_Drop();
	virtual			~C_ASW_Ammo_Drop();

	bool ShouldDraw();

	virtual void OnDataChanged( DataUpdateType_t updateType );
	void ClientThink();

	virtual int DrawModel( int flags, const RenderableInstance_t &instance );

	int GetAmmoDropIconTextureID();	
 	static bool s_bLoadedUseActionIcons;
 	static int s_nUseActionIconTextureID;		

	CNetworkVar(int, m_iAmmoUnitsRemaining);

	int GetAmmoUnitCost( int iAmmoType );
	int GetAmmoUnitsRemaining() { return m_iAmmoUnitsRemaining; }
	C_ASW_Weapon* GetAmmoUseUnits( C_ASW_Marine *pMarine );
	bool AllowedToPickup( C_ASW_Marine *pMarine );

	// IASW_Client_Usable_Entity
	virtual C_BaseEntity* GetEntity() { return this; }
	virtual bool IsUsable( C_BaseEntity *pUser );
	virtual bool GetUseAction( ASWUseAction &action, C_ASW_Marine *pUser );
	virtual void CustomPaint( int ix, int iy, int alpha, vgui::Panel *pUseIcon );
	virtual bool ShouldPaintBoxAround() { return true; }
	virtual bool NeedsLOSCheck() { return true; }

	static vgui::HFont s_hAmmoFont;

	CGlowObject m_GlowObject;
	
private:
	C_ASW_Ammo_Drop( const C_ASW_Ammo_Drop & ); // not defined, not accessible
	bool m_bEnoughAmmo;
};

extern CUtlVector<C_ASW_Ammo_Drop*>	g_AmmoDrops;

#endif /* C_ASW_AMMO_DROP_H */