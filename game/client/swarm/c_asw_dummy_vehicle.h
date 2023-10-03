#ifndef _INCLUDED_C_ASW_DUMMY_VEHICLE_H
#define _INCLUDED_C_ASW_DUMMY_VEHICLE_H

#include "iasw_client_vehicle.h"

class C_ASW_Dummy_Vehicle : public C_BaseAnimating, public IASW_Client_Vehicle
{
public:
	DECLARE_CLASS( C_ASW_Dummy_Vehicle, C_BaseAnimating );
	DECLARE_CLIENTCLASS();

					C_ASW_Dummy_Vehicle();
	virtual			~C_ASW_Dummy_Vehicle();

	bool MarineInVehicle();

	// implement our asw vehicle interface
	virtual int ASWGetNumPassengers() { return 0; }		// todo: implement
	virtual C_ASW_Marine* ASWGetDriver();
	virtual C_ASW_Marine* ASWGetPassenger(int i) { return NULL; }	// todo: implement
	CNetworkHandle(C_ASW_Marine, m_hDriver);
	// implement client vehicle interface
	virtual bool ValidUseTarget() { return true; }
	virtual int GetDriveIconTexture();
	virtual int GetRideIconTexture();
	virtual const char* GetDriveIconText();
	virtual const char* GetRideIconText();
	virtual C_BaseEntity* GetEntity() { return this; }
	static bool s_bLoadedRideIconTexture;
	static int s_nRideIconTextureID;
	static bool s_bLoadedDriveIconTexture;
	static int s_nDriveIconTextureID;
	// no clientside prediction with this kind of vehicle	
	virtual void SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move ) { }
	virtual void ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMoveData ) { }
	virtual void ASWStartEngine() { }
	virtual void ASWStopEngine() { }

	virtual bool ShouldDraw();
	virtual ShadowType_t	ShadowCastType();
	virtual void ClientThink();

	virtual bool IsUsable(C_BaseEntity *pUser);
	virtual bool GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser);
	virtual void CustomPaint(int ix, int iy, int alpha, vgui::Panel *pUseIcon) { }
	virtual bool ShouldPaintBoxAround() { return (ASWGetDriver() == NULL); }

private:
	C_ASW_Dummy_Vehicle( const C_ASW_Dummy_Vehicle & ); // not defined, not accessible
};

#endif /* _INCLUDED_C_ASW_DUMMY_VEHICLE_H */