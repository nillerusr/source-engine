//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Base class for object upgrading objects
//
//=============================================================================//
#include "cbase.h"
#include "baseobject_shared.h"
#include "tf_obj_basedrivergun_shared.h"
#include "basetfvehicle.h"

IMPLEMENT_NETWORKCLASS_ALIASED( BaseObjectDriverGun, DT_BaseObjectDriverGun )

BEGIN_NETWORK_TABLE( CBaseObjectDriverGun, DT_BaseObjectDriverGun )
#if !defined( CLIENT_DLL )
	SendPropVector( SENDINFO(m_vecGunAngles), -1, SPROP_COORD ),
#else
	RecvPropVector( RECVINFO(m_vecGunAngles) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CBaseObjectDriverGun )
	DEFINE_PRED_FIELD_TOL( m_vecGunAngles, FIELD_VECTOR, FTYPEDESC_INSENDTABLE, 1.0f ),
END_PREDICTION_DATA()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseObjectDriverGun::CBaseObjectDriverGun()
{
	m_vecGunAngles = QAngle(0,0,0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObjectDriverGun::Spawn()
{
	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObjectDriverGun::FinishedBuilding( void )
{
#if !defined( CLIENT_DLL )
	BaseClass::FinishedBuilding();

	CBaseTFVehicle *pVehicle = dynamic_cast<CBaseTFVehicle*>(GetParentObject());
	Assert( pVehicle );

	pVehicle->SetDriverGun( this );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObjectDriverGun::SetTargetAngles( const QAngle &vecAngles )
{
	m_vecGunAngles = vecAngles;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const QAngle &CBaseObjectDriverGun::GetCurrentAngles( void )
{
	return m_vecGunAngles.Get();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector CBaseObjectDriverGun::GetFireOrigin( void )
{
	return GetAbsOrigin();
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseObjectDriverGun::ShouldPredict( void )
{
	CBaseTFVehicle *pVehicle = dynamic_cast<CBaseTFVehicle*>(GetParentObject());
	if ( pVehicle && pVehicle->GetDriverPlayer() == C_BasePlayer::GetLocalPlayer() )
		return true;

	return BaseClass::ShouldPredict();
}
#endif