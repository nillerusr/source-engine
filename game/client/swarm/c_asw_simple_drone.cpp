#include "cbase.h"
#include "c_asw_simple_drone.h"
#include "engine/IVDebugOverlay.h"
#include "asw_shareddefs.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Simple_Drone, DT_ASW_Simple_Drone, CASW_Simple_Drone)
	
END_RECV_TABLE()

extern ConVar cl_asw_drone_travel_yaw;
extern ConVar cl_asw_drone_travel_yaw_rate;


namespace
{
	float AI_ClampYaw( float yawSpeedPerSec, float current, float target, float time )
	{
		if (current != target)
		{
			float speed = yawSpeedPerSec * time;
			float move = target - current;

			if (target > current)
			{
				if (move >= 180)
					move = move - 360;
			}
			else
			{
				if (move <= -180)
					move = move + 360;
			}

			if (move > 0)
			{// turning to the npc's left
				if (move > speed)
					move = speed;
			}
			else
			{// turning to the npc's right
				if (move < -speed)
					move = -speed;
			}
			
			return anglemod(current + move);
		}
		
		return target;
	}
}

C_ASW_Simple_Drone::C_ASW_Simple_Drone()
{
	m_flCurrentTravelYaw = -1;
	m_flCurrentTravelSpeed = 0;
}


C_ASW_Simple_Drone::~C_ASW_Simple_Drone()
{

}

// get the full velocity we can run at for the current direction (i.e. relative to our facing)
float C_ASW_Simple_Drone::GetRunSpeed()
{
	return GetSequenceGroundSpeed(LookupSequence("run_idle"));	
}

void C_ASW_Simple_Drone::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		// We want to think every frame.
		SetNextClientThink( CLIENT_THINK_ALWAYS );
		return;
	}
}

void C_ASW_Simple_Drone::ClientThink()
{
	BaseClass::ClientThink();

	if (GetSequence() == LookupSequence("run_idle")
		|| GetSequence() == LookupSequence("lunge_attack"))
		UpdatePoseParams();
}

void C_ASW_Simple_Drone::UpdatePoseParams()
{
	VPROF_BUDGET( "C_ASW_Simple_Drone::UpdatePoseParams", VPROF_BUDGETGROUP_ASW_CLIENT );
	// update pose params based on velocity and our angles

	// calculate the angle difference between our facing and our velocity
	Vector v;
	EstimateAbsVelocity(v);	
	float travel_yaw = anglemod(UTIL_VecToYaw(v));
	float current_yaw = anglemod( GetLocalAngles().y );

	// Draw a green triangle on the ground for the travel yaw
	if (cl_asw_drone_travel_yaw.GetBool())
	{
		float flBaseSize = 10;
		float flHeight = 80;
		Vector vBasePos = GetAbsOrigin() + Vector( 0, 0, 5 );
		QAngle angles( 0, 0, 0 );
		Vector vForward, vRight, vUp;
		angles[YAW] = travel_yaw;
		AngleVectors( angles, &vForward, &vRight, &vUp );
		debugoverlay->AddTriangleOverlay( vBasePos+vRight*flBaseSize/2, vBasePos-vRight*flBaseSize/2, vBasePos+vForward*flHeight, 0, 255, 0, 255, false, 0.01 );		
	}

	// calculate our fraction of full anim velocity
	float speed_fraction = 0;
	float ground_speed = GetRunSpeed();
	if (ground_speed > 0)
		speed_fraction = clamp<float>(
			(v.Length()) / ground_speed,
			0.0f, 1.0f);
	speed_fraction = 1.0f - speed_fraction;

	// smooth out the travel yaw to prevent sudden changes in move_yaw pose parameter
	if (m_flCurrentTravelYaw == -1)
		m_flCurrentTravelYaw = travel_yaw;
	else
	{
		float travel_diff = AngleDiff(m_flCurrentTravelYaw, travel_yaw);
		if (travel_diff < 0)
			travel_diff = -travel_diff;
		travel_diff = clamp<float>(travel_diff, 32.0f, 256.0f);	// alter the yaw by this amount - i.e. faster if the angle is bigger, but clamped
		if (speed_fraction > 0.75f)	// change the angle even quicker if we're moving very slowly
		{
			travel_diff *= (2.0f + ((speed_fraction - 0.75f) * 8.0f));
		}
		if (speed_fraction < 1.0f)	// don't bother adjusting the yaw if we're standing still
			m_flCurrentTravelYaw = AI_ClampYaw( travel_diff * cl_asw_drone_travel_yaw_rate.GetFloat(), m_flCurrentTravelYaw, travel_yaw, gpGlobals->frametime );
		else
			m_flCurrentTravelYaw = travel_yaw;	// if we're standing still, immediately change
		//Msg("travel=%.1f current_travel=%.1f current=%.1f t=%f\n",
			//travel_yaw, m_flCurrentTravelYaw, current_yaw, gpGlobals->curtime);
		travel_yaw = m_flCurrentTravelYaw;
	}

	// set the move_yaw pose parameter
	float diff = AngleDiff(travel_yaw, current_yaw);

	// Draw a green triangle on the ground for the move yaw
	if (cl_asw_drone_travel_yaw.GetBool())
	{
		float flBaseSize = 10;
		float flHeight = 80;
		Vector vBasePos = GetAbsOrigin() + Vector( 0, 0, 5 );
		QAngle angles( 0, 0, 0 );
		Vector vForward, vRight, vUp;
		angles[YAW] = travel_yaw;
		AngleVectors( angles, &vForward, &vRight, &vUp );
		debugoverlay->AddTriangleOverlay( vBasePos+vRight*flBaseSize/2, vBasePos-vRight*flBaseSize/2, vBasePos+vForward*flHeight, 0, 0, 255, 255, false, 0.01 );		

		angles[YAW] = diff;
		AngleVectors( angles, &vForward, &vRight, &vUp );
		debugoverlay->AddTriangleOverlay( vBasePos+vRight*flBaseSize/2, vBasePos-vRight*flBaseSize/2, vBasePos+vForward*flHeight, 255, 0, 0, 255, false, 0.01 );
	}
	
	
	diff = clamp<float>(diff, -180.0f, 180.0f);
	int pose_index = LookupPoseParameter( "move_yaw" );
	if (pose_index >= 0)
	{
		SetPoseParameter(pose_index, diff);
	}
	
	// smooth out our speed fraction to prevent sudden changes to idle_move pose parameter
	//Msg("sf=%f cts=%f gs=%f vl=%f\n", speed_fraction, m_flCurrentTravelSpeed,
		//ground_speed, v.Length());
	if (m_flCurrentTravelSpeed == -1)
	{
		m_flCurrentTravelSpeed = speed_fraction;
	}
	else
	{
		if (m_flCurrentTravelSpeed < speed_fraction)
		{
			m_flCurrentTravelSpeed = clamp<float>(
				m_flCurrentTravelSpeed + gpGlobals->frametime,
				0.0f, speed_fraction);
		}
		else
		{
			m_flCurrentTravelSpeed = clamp<float>(
				m_flCurrentTravelSpeed - gpGlobals->frametime * 3.0f,
				speed_fraction, 1.0f);
		}
		speed_fraction = m_flCurrentTravelSpeed;
	}
	
	// set the idle_move pose parameter
	pose_index = LookupPoseParameter( "idle_move" );
	if (pose_index >= 0)
	{
		SetPoseParameter(pose_index, 100.0f * speed_fraction);
	}
}
