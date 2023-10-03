#ifndef _INCLUDED_ASW_DUMMY_VEHICLE_H
#define _INCLUDED_ASW_DUMMY_VEHICLE_H
#pragma once

#include "iasw_vehicle.h"

class CASW_Dummy_Vehicle : public CBaseAnimating, public IASW_Vehicle
{
public:
	DECLARE_CLASS( CASW_Dummy_Vehicle, CBaseAnimating );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache()
	{
		// Prevents a warning
		SelectModel( );
		BaseClass::Precache();
	}

	CASW_Dummy_Vehicle();
	virtual ~CASW_Dummy_Vehicle();
	void	Spawn( void );
	void	SelectModel();
	virtual int				ShouldTransmit( const CCheckTransmitInfo *pInfo );
	int UpdateTransmitState();
	void SetNormalizedPoseParameter(int iParam, float fValue);

	// implement our asw vehicle interface
	virtual int ASWGetNumPassengers() { return 0; }		// todo: implement
	virtual void ASWSetDriver(CASW_Marine* pDriver) { m_hDriver = pDriver; }
	virtual CASW_Marine* ASWGetDriver();
	virtual CASW_Marine* ASWGetPassenger(int i) { return NULL; }	// todo: implement
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );
	virtual CBaseEntity* GetEntity() { return this; }
	CNetworkHandle(CASW_Marine, m_hDriver);
	virtual void ASWStartEngine() { } // todo: start making noises and stuff?
	virtual void ASWStopEngine() { } // todo: stop making noises and stuff?
	// no serverside driving with this kind of vehicle
	virtual void SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move ) { }
	virtual void ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMoveData ) { }

	// IASW_Server_Usable_Entity
	virtual bool IsUsable(CBaseEntity *pUser);	
	virtual void MarineStartedUsing(CASW_Marine* pMarine) { } 
	virtual void MarineStoppedUsing(CASW_Marine* pMarine) { } 
	virtual void MarineUsing(CASW_Marine* pMarine, float fDeltaTime) { }
	virtual bool NeedsLOSCheck() { return false; }
};


#endif /* _INCLUDED_ASW_DUMMY_VEHICLE_H */