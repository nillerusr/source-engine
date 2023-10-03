#ifndef _INCLUDED_ASW_CLIENT_VEHICLE_H
#define _INCLUDED_ASW_CLIENT_VEHICLE_H
#ifdef _WIN32
#pragma once
#endif

class C_ASW_Marine;
class C_BasePlayer;
class CUserCmd;
class IMoveHelper;
class CMoveData;

#include "iasw_client_usable_entity.h"

//-----------------------------------------------------------------------------
// Interface for asw vehicles to implement on the clientside
//-----------------------------------------------------------------------------
abstract_class IASW_Client_Vehicle : public IASW_Client_Usable_Entity
{
public:
	virtual int ASWGetNumPassengers() = 0;
	virtual C_ASW_Marine* ASWGetDriver() = 0;
	virtual C_ASW_Marine* ASWGetPassenger(int i) = 0;
	virtual int GetDriveIconTexture() = 0;
	virtual int GetRideIconTexture() = 0;
	virtual const char* GetDriveIconText() = 0;
	virtual const char* GetRideIconText() = 0;
	virtual bool ValidUseTarget() = 0;
	virtual void ASWStartEngine() = 0;
	virtual void ASWStopEngine() = 0;
	virtual void SetupMove( C_BasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move ) = 0;
	virtual void ProcessMovement( C_BasePlayer *pPlayer, CMoveData *pMoveData ) = 0;

	// IASW_Client_Usable_Entity
	virtual C_BaseEntity* GetEntity() = 0;
	virtual bool IsUsable(C_BaseEntity *pUser) = 0;
	virtual bool GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser) = 0;
	virtual void CustomPaint(int ix, int iy, int alpha, vgui::Panel *pUseIcon) = 0;
	virtual bool ShouldPaintBoxAround() = 0;
	virtual bool NeedsLOSCheck() { return false; }
};

#endif // _INCLUDED_ASW_CLIENT_VEHICLE_H
