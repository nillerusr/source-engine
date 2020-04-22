//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "C_BaseFourWheelVehicle.h"

IMPLEMENT_CLIENTCLASS_DT(C_BaseTFFourWheelVehicle, DT_BaseTFFourWheelVehicle, CBaseTFFourWheelVehicle)
	RecvPropFloat( RECVINFO( m_flDeployFinishTime ) ),
	RecvPropInt( RECVINFO( m_eDeployMode ) ),
	RecvPropInt( RECVINFO( m_bBoostUpgrade ) ),
	RecvPropInt( RECVINFO( m_nBoostTimeLeft ) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_BaseTFFourWheelVehicle::C_BaseTFFourWheelVehicle()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float C_BaseTFFourWheelVehicle::GetDeployFinishTime() const
{
	return m_flDeployFinishTime;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
VehicleModeDeploy_e C_BaseTFFourWheelVehicle::GetVehicleModeDeploy() const
{
	return m_eDeployMode;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseTFFourWheelVehicle::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		// Start thinking (Baseclass stops it)
		ClientThinkList()->SetNextClientThink( GetClientHandle(), CLIENT_THINK_ALWAYS );
	}
}

//-----------------------------------------------------------------------------
// Restricts the view within a range of the center...
//-----------------------------------------------------------------------------
void C_BaseTFFourWheelVehicle::RestrictView( int nRole, float flMinYaw, float flMaxYaw, QAngle &vecViewAngles )
{
	Assert( nRole >= 0 );
	Vector vehicleEyeOrigin;
	QAngle vehicleEyeAngles;
	GetRoleViewPosition( nRole, &vehicleEyeOrigin, &vehicleEyeAngles ); 

	// Confine the view to the appropriate yaw range...
	float flCenterYaw = vehicleEyeAngles[YAW];

	// View angles are dealt with in absolute terms here...
	float flAngleDiff = AngleDiff( vecViewAngles[YAW], flCenterYaw );

	// Here, we must clamp to the cone...
	if (flAngleDiff < flMinYaw)
		vecViewAngles[YAW] = anglemod(flCenterYaw + flMinYaw);
	else if (flAngleDiff > flMaxYaw)
		vecViewAngles[YAW] = anglemod(flCenterYaw + flMaxYaw);
}


//-----------------------------------------------------------------------------
// Clamps the view angles while driving the vehicle 
//-----------------------------------------------------------------------------
void C_BaseTFFourWheelVehicle::UpdateViewAngles( C_BasePlayer *pLocalPlayer, CUserCmd *pCmd )
{
	int nRole = GetPassengerRole( pLocalPlayer );
	if ( nRole != VEHICLE_ROLE_DRIVER )
	{
		RestrictView( nRole, -90, 90, pCmd->viewangles );
	}
}

