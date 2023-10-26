#ifndef _DEFINED_C_ASW_PICKUP_H
#define _DEFINED_C_ASW_PICKUP_H

#include "iasw_client_usable_entity.h"
#include "glow_outline_effect.h"

class C_ASW_Pickup : public C_BaseAnimating, public IASW_Client_Usable_Entity
{
	DECLARE_CLASS( C_ASW_Pickup, C_BaseAnimating );
	DECLARE_CLIENTCLASS();
public:
	C_ASW_Pickup();	
	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void ClientThink();
	virtual void GetUseIconText( wchar_t *unicode, int unicodeBufferSizeInBytes );
	virtual int GetUseIconTextureID();

	virtual C_BaseEntity* GetEntity() { return this; }
	virtual bool IsUsable(C_BaseEntity *pUser);
	virtual bool GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser);
	virtual void CustomPaint(int ix, int iy, int alpha, vgui::Panel *pUseIcon) { }
	virtual bool ShouldPaintBoxAround() { return true; }
	virtual bool AllowedToPickup(C_ASW_Marine *pMarine) { return true; }
	virtual bool NeedsLOSCheck() { return true; }
	virtual void InitPickup() { }

protected:
	C_ASW_Pickup( const C_ASW_Pickup & ); // not defined, not accessible
	
	char m_szUseIconText[32];

	static void LoadUseIconTexture(const char* szTextureName, int &textureID);
	static void LoadUseIconTextures();

	static bool s_bLoadedUseIconTextures;
	static int s_nUseIconTake;
	static int s_nUseIconTakeRifleAmmo;
	static int s_nUseIconTakeAutogunAmmo;
	static int s_nUseIconTakeShotgunAmmo;
	static int s_nUseIconTakeVindicatorAmmo;
	static int s_nUseIconTakeFlamerAmmo;
	static int s_nUseIconTakePistolAmmo;
	static int s_nUseIconTakeMiningLaserAmmo;
	static int s_nUseIconTakeRailgunAmmo;
	static int s_nUseIconTakePDWAmmo;

	static int s_nUseIconTakePowerup_FireB;
	static int s_nUseIconTakePowerup_FreezeB;
	static int s_nUseIconTakePowerup_ExplodeB;
	static int s_nUseIconTakePowerup_ElectricB;
	static int s_nUseIconTakePowerup_ChemicalB;
	static int s_nUseIconTakePowerup_Speed;

	CGlowObject m_GlowObject;
};

#endif /* _DEFINED_C_ASW_PICKUP_H */