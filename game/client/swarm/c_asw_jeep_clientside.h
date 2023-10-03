#ifndef _INCLUDED_C_ASW_JEEP_CLIENTSIDE_H
#define _INCLUDED_C_ASW_JEEP_CLIENTSIDE_H
#pragma once

#include "c_asw_jeep.h"

//=============================================================================
//
// Client-side Jeep Class
//
class C_ASW_PropJeep_Clientside : public C_ASW_PropJeep
{
	DECLARE_CLASS( C_ASW_PropJeep_Clientside, C_ASW_PropJeep );

public:

	C_ASW_PropJeep_Clientside();
	virtual ~C_ASW_PropJeep_Clientside();
	virtual void Spawn();	
	bool			Initialize();	
	virtual void ClientThink();
	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual int VPhysicsGetObjectList( IPhysicsObject **pList, int listMax );
	static C_ASW_PropJeep_Clientside *CreateNew(bool bForce = false);

	// asw
	virtual void InitPhysics();
	virtual void SetupMove( C_BasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );
	virtual void ProcessMovement( C_BasePlayer *pPlayer, CMoveData *pMoveData );
	virtual void DriveVehicle( C_BasePlayer *pPlayer, CUserCmd *ucmd );
	virtual void DriveVehicle( float flFrameTime, CUserCmd *ucmd, int iButtonsDown, int iButtonsReleased );	
	virtual void UpdateSteeringAngle();
	virtual void ThinkTick();
	virtual void VPhysicsUpdate( IPhysicsObject *pPhysics );
	virtual bool ShouldPredict() { return true; }
	// asw: our clientside physics
	C_ASW_FourWheelVehiclePhysics		m_VehiclePhysics;

	// implement our asw vehicle interface (pass these on to the dummy)
	virtual int ASWGetNumPassengers();
	virtual C_ASW_Marine* ASWGetDriver();
	virtual C_ASW_Marine* ASWGetPassenger(int i);
	IASW_Client_Vehicle* GetDummy();	// the dummy entity that other players see for our vehicle	
	virtual void ASWStartEngine();
	virtual void ASWStopEngine();		// destroys the vehicle!
	virtual bool ValidUseTarget() { return false; }

	bool m_bInitialisedPhysics;  // asw
	bool bDestroyVehicle;
};

#endif // _INCLUDED_C_ASW_JEEP_CLIENTSIDE_H