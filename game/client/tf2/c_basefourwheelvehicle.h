//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_BASE_FOUR_WHEEL_VEHICLE_H
#define C_BASE_FOUR_WHEEL_VEHICLE_H

#include "basetfvehicle.h"

class C_BasePlayer;
	    
class C_BaseTFFourWheelVehicle : public C_BaseTFVehicle
{
	DECLARE_CLASS( C_BaseTFFourWheelVehicle, C_BaseTFVehicle );
	DECLARE_CLIENTCLASS();

public:

	C_BaseTFFourWheelVehicle();

	float				GetDeployFinishTime() const;
	VehicleModeDeploy_e GetVehicleModeDeploy() const;

	// TF2 vehicles are animated by the server
	virtual bool IsSelfAnimating() { return false; };

	virtual void OnDataChanged( DataUpdateType_t updateType );

// IClientVehicle overrides.
public:
	virtual void UpdateViewAngles( C_BasePlayer *pLocalPlayer, CUserCmd *pCmd );

protected:
	// Restricts the view within a range of the center...
	void RestrictView( int nRole, float flMinYaw, float flMaxYaw, QAngle &vecViewAngles );

private:

	C_BaseTFFourWheelVehicle( const C_BaseTFFourWheelVehicle & ); // not defined, not accessible

private:

	// Used to draw deploy timer on vgui screens.
	float					m_flDeployFinishTime;
	VehicleModeDeploy_e		m_eDeployMode;
};

#endif // C_BASE_FOUR_WHEEL_VEHICLE_H