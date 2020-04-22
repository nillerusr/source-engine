//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "c_basefourwheelvehicle.h"
#include "tf_movedata.h"
#include "ObjectControlPanel.h"
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include "vgui_rotation_slider.h"
#include <vgui/ISurface.h>
#include "vgui_basepanel.h"
#include "ground_line.h"
#include "hud_minimap.h"
#include "vgui_bitmapimage.h"
#include "view.h"
#include "c_basetempentity.h"
#include "particles_simple.h"
#include "in_buttons.h"


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_VehicleTank : public C_BaseTFFourWheelVehicle
{
	DECLARE_CLASS( C_VehicleTank, C_BaseTFFourWheelVehicle );
public:
	DECLARE_CLIENTCLASS();

	C_VehicleTank();


	virtual void ClientThink();

	virtual void OnItemPostFrame( CBaseTFPlayer *pDriver );

// C_BaseEntity overrides.
public:
	virtual void GetBoneControllers(float controllers[MAXSTUDIOBONECTRLS]);

private:
	C_VehicleTank( const C_VehicleTank & );		// not defined, not accessible

	float m_flClientYaw;
	float m_flClientPitch;

	// Sent by the server. This is what we render with.
	float m_flTurretYaw;
	float m_flTurretPitch;
};


IMPLEMENT_CLIENTCLASS_DT( C_VehicleTank, DT_VehicleTank, CVehicleTank )
	RecvPropFloat( RECVINFO( m_flTurretYaw ) ),
	RecvPropFloat( RECVINFO( m_flTurretPitch ) )
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

C_VehicleTank::C_VehicleTank()
{
	m_flClientYaw = 0;
	m_flClientPitch = 0;
	m_flTurretYaw = 0;
	m_flTurretPitch = 0;
}


void C_VehicleTank::ClientThink()
{
	if ( GetPassenger( VEHICLE_ROLE_DRIVER ) == C_BasePlayer::GetLocalPlayer() )
	{
		// Cast a ray out of the view to see where the player is looking.
		trace_t trace;
		UTIL_TraceLine( 
			MainViewOrigin(), 
			MainViewOrigin() + MainViewForward() * 100000,
			MASK_OPAQUE, NULL,
			COLLISION_GROUP_NONE,
			&trace );

		if ( trace.fraction < 1 )
		{
			// Figure out what angles our turret needs to be at in order to hit the target.

			Vector vFireOrigin;
			QAngle dummy;
			GetAttachment( LookupAttachment( "barrel" ), vFireOrigin, dummy );

			// Get a direction vector that points at the target.
			Vector vTo = trace.endpos - vFireOrigin;

			// Transform it into the tank's local space.
			matrix3x4_t tankToWorld;
			AngleMatrix( GetAbsAngles(), tankToWorld );
			
			Vector vLocalTo;
			VectorITransform( vTo, tankToWorld, vLocalTo );

			// Now figure out what the angles are in local space.
			QAngle localAngles;
			VectorAngles( vLocalTo, localAngles );

			// Make the angles adhere to the definition in CVehicleTank's header.
			m_flClientYaw = localAngles[YAW] - 90;
			m_flClientPitch = anglemod( -localAngles[PITCH] );


			char cmd[512];
			Q_snprintf( cmd, sizeof( cmd ), "TurretAngles %.2f %.2f", m_flClientYaw, m_flClientPitch );
			SendClientCommand( cmd );
		}
	}

	BaseClass::ClientThink();
}


void C_VehicleTank::GetBoneControllers( float controllers[MAXSTUDIOBONECTRLS])
{
	BaseClass::GetBoneControllers( controllers );

	controllers[0] = anglemod( m_flTurretYaw ) / 360.0;
	controllers[1] = anglemod( m_flTurretPitch ) / 360.0;
}

//-----------------------------------------------------------------------------
// Here's where we deal with weapons
//-----------------------------------------------------------------------------
void C_VehicleTank::OnItemPostFrame( CBaseTFPlayer *pDriver )
{
	if ( GetPassengerRole(pDriver) != VEHICLE_ROLE_DRIVER )
		return;

	if ( pDriver->m_nButtons & IN_ATTACK )
	{
	}
	else if ( pDriver->m_nButtons & (IN_ATTACK2 | IN_SPEED) )
	{
		BaseClass::OnItemPostFrame( pDriver );
	}
}

