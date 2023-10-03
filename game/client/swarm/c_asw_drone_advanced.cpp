#include "cbase.h"
#include "c_asw_drone_advanced.h"
#include "engine/IVDebugOverlay.h"
#include "asw_shareddefs.h"
#include "tier0/vprof.h"
#include "con_nprint.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Drone_Advanced, DT_ASW_Drone_Advanced, CASW_Drone_Advanced)
	RecvPropEHandle( RECVINFO( m_hAimTarget ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_ASW_Drone_Advanced )
	DEFINE_PRED_FIELD( m_flPoseParameter, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
END_PREDICTION_DATA()

ConVar cl_asw_drone_travel_yaw("cl_asw_drone_travel_yaw", "0", FCVAR_CHEAT, "Show the clientside estimated travel yaw for swarm drones");
ConVar cl_asw_drone_travel_yaw_rate("cl_asw_drone_travel_yaw_rate", "4.0f", FCVAR_CHEAT, "How fast the drones alter their move_yaw param");
ConVar asw_debug_drone_pose( "asw_debug_drone_pose", "0", FCVAR_NONE, "Set to drone entity index to output drone pose params" );
ConVar asw_drone_jump_pitch_min( "asw_drone_jump_pitch_min", "-45", FCVAR_NONE, "Min pitch for drone's jumping pose parameter" );
ConVar asw_drone_jump_pitch_max( "asw_drone_jump_pitch_max", "45", FCVAR_NONE, "Min pitch for drone's jumping pose parameter" );
ConVar asw_drone_jump_pitch_speed( "asw_drone_jump_pitch_speed", "3.0", FCVAR_NONE, "Speed for drone's pitch jumping pose parameter transition" );

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

C_ASW_Drone_Advanced::C_ASW_Drone_Advanced()
{
	m_flCurrentTravelYaw = -1;
	m_flCurrentTravelSpeed = 0;
	m_bWasJumping = 0.0f;

	for (int i=0;i<MAXSTUDIOPOSEPARAM;i++)
	{
		m_flClientPoseParameter[i] = 0;
	}
}


C_ASW_Drone_Advanced::~C_ASW_Drone_Advanced()
{
}

// get the full velocity we can run at for the current direction (i.e. relative to our facing)
float C_ASW_Drone_Advanced::GetRunSpeed()
{
	return GetSequenceGroundSpeed(LookupSequence("run_idle"));	
}

void C_ASW_Drone_Advanced::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		// We want to think every frame.
		SetNextClientThink( CLIENT_THINK_ALWAYS );
		return;
	}
}

extern ConVar asw_alien_object_motion_blur_scale;

void C_ASW_Drone_Advanced::ClientThink()
{
	BaseClass::ClientThink();

	UpdatePoseParams();
	
	// TODO: Fix this to not be doing all these lookups/strcmps (could make drone attack activities server/client shared and use GetSequenceActivity( GetSequence() )
	/*
	if ( GetSequence() == LookupSequence( "Lunge_Attack01" )
		|| GetSequence() == LookupSequence( "Lunge_Attack02" )
		|| GetSequence() == LookupSequence( "Lunge_Attack03" )
		|| GetSequence() == LookupSequence( "Jump_Glide" )
		)
	{
		m_MotionBlurObject.SetVelocityScale( asw_alien_object_motion_blur_scale.GetFloat() );
	}
	else
	{
		m_MotionBlurObject.SetVelocityScale( 0.0f );
	}
	*/
}

void C_ASW_Drone_Advanced::UpdatePoseParams()
{
	VPROF_BUDGET( "C_ASW_Drone_Advanced::UpdatePoseParams", VPROF_BUDGETGROUP_ASW_CLIENT );
	// update pose params based on velocity and our angles

	// calculate the angle difference between our facing and our velocity
	Vector v;
	EstimateAbsVelocity(v);	
	float travel_yaw = anglemod(UTIL_VecToYaw(v));
	float current_yaw = anglemod( GetLocalAngles().y );
	float travel_pitch = UTIL_VecToPitch(v);
	
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

		travel_yaw = m_flCurrentTravelYaw;
	}

	// set the move_yaw pose parameter
	float diff = AngleDiff(travel_yaw, current_yaw);

	// Draw a green triangle on the ground for the move yaw
	if (cl_asw_drone_travel_yaw.GetBool())
	{
		float flBaseSize = 10;
		float flHeight = 10 + (100 * speed_fraction);
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
		m_flClientPoseParameter[pose_index] = ((diff + 180.0f) / 360.0f);
	}
	
	// smooth out our speed fraction to prevent sudden changes to idle_move pose parameter
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
		// blend to goal
		if (speed_fraction > m_flClientPoseParameter[pose_index])
			m_flClientPoseParameter[pose_index] = MIN(speed_fraction, m_flClientPoseParameter[pose_index] + gpGlobals->frametime * 12.0f);
		else
			m_flClientPoseParameter[pose_index] = MAX(speed_fraction, m_flClientPoseParameter[pose_index] - gpGlobals->frametime * 12.0f);
	}

	pose_index = LookupPoseParameter( "aim_yaw" );
	if ( pose_index >= 0 )
	{
		float flTargetAimPose = 0.0f;
		if ( m_hAimTarget.Get() )
		{
			C_BaseEntity *pEnemy = m_hAimTarget.Get();
			Vector vecToEnemy = pEnemy->WorldSpaceCenter() - WorldSpaceCenter();			
			float flYaw = UTIL_VecToYaw( vecToEnemy );
			flYaw = AngleDiff( flYaw, GetAbsAngles()[ YAW ] );
			//Msg( "Yaw to enemy = %f ", flYaw );
			flYaw /= -45.0f;
			flYaw = clamp<float>( flYaw, -1.0f, 1.0f );
			flYaw = 0.5f + 0.5f * flYaw;
			//Msg( " clamped+scaled = %f ", flYaw );
			flTargetAimPose = flYaw;
			//Msg( " current = %f\n", m_flClientPoseParameter[ pose_index ] );
		}

		UTIL_ApproachTarget( flTargetAimPose, 3.0f, 3.0f, &m_flClientPoseParameter[ pose_index ] );
	}

	static int s_nJumpSequence = LookupSequence( "Jump_Glide" );
	bool bJumping = ( GetSequence() == s_nJumpSequence );
	if ( bJumping )
	{
		pose_index = LookupPoseParameter( "jump_angle" );
		if ( pose_index >= 0 )
		{
			if ( !m_bWasJumping )
			{
				m_flClientPoseParameter[ pose_index ] = 0.0f;
			}

			float flTargetPose = clamp<float>( travel_pitch, asw_drone_jump_pitch_min.GetFloat(), asw_drone_jump_pitch_max.GetFloat() );
			flTargetPose = ( flTargetPose - asw_drone_jump_pitch_min.GetFloat() ) / ( asw_drone_jump_pitch_max.GetFloat() - asw_drone_jump_pitch_min.GetFloat() );
			UTIL_ApproachTarget( flTargetPose, asw_drone_jump_pitch_speed.GetFloat(), asw_drone_jump_pitch_speed.GetFloat(), &m_flClientPoseParameter[ pose_index ] );
		}
	}
	m_bWasJumping = bJumping;

	if ( asw_debug_drone_pose.GetInt() == entindex() )
	{		
		con_nprint_t np;
		np.fixed_width_font = true;
		np.color[0] = 0.8f;
		np.color[1] = 1.0f;
		np.color[2] = 1.0f;
		np.time_to_live = 2.0f;
		np.index = 2;

		pose_index = LookupPoseParameter( "idle_move" );
		if ( pose_index >= 0 )
		{
			engine->Con_NXPrintf( &np, "idle_move: %.2f", m_flClientPoseParameter[pose_index] );
			np.index++;
		}
		pose_index = LookupPoseParameter( "aim_yaw" );
		if ( pose_index >= 0 )
		{
			engine->Con_NXPrintf( &np, "aim_yaw: %.2f", m_flClientPoseParameter[pose_index] );
			np.index++;
		}
		pose_index = LookupPoseParameter( "move_yaw" );
		if ( pose_index >= 0 )
		{
			engine->Con_NXPrintf( &np, "move_yaw: %.2f", m_flClientPoseParameter[pose_index] );
			np.index++;
		}
		pose_index = LookupPoseParameter( "jump_angle" );
		if ( pose_index >= 0 )
		{
			engine->Con_NXPrintf( &np, "jump_angle: %.2f", m_flClientPoseParameter[pose_index] );
			np.index++;
			engine->Con_NXPrintf( &np, "travel_pitch: %.2f", travel_pitch );
			np.index++;

// 			if ( !engine->IsPaused() )
// 			{
// 				float flTargetPose = clamp<float>( travel_pitch, asw_drone_jump_pitch_min.GetFloat(), asw_drone_jump_pitch_max.GetFloat() );
// 				flTargetPose = ( flTargetPose - asw_drone_jump_pitch_min.GetFloat() ) / ( asw_drone_jump_pitch_max.GetFloat() - asw_drone_jump_pitch_min.GetFloat() );
// 				Msg( "Travel pitch = %.2f angle = %.2f flTargetPose = %.2f\n", travel_pitch, m_flClientPoseParameter[pose_index], flTargetPose );
// 			}
		}
	}
}

// hardcoded to match with the gun offset in the marine's autoaim
	//  this is to generally keep the marine's gun horizontal, so guns like the shotgun can more easily hit multiple enemies in one shot
const Vector& C_ASW_Drone_Advanced::GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming)
{ 
	static Vector aim_pos;
	aim_pos = m_vecLastRenderedPos - (WorldSpaceCenter() - GetAbsOrigin());	// last rendered stores our worldspacecenter, so convert to back origin
	aim_pos.z += ASW_MARINE_GUN_OFFSET_Z;
	return aim_pos;
}

void C_ASW_Drone_Advanced::GetPoseParameters( CStudioHdr *pStudioHdr, float poseParameter[MAXSTUDIOPOSEPARAM])
{
	if ( !pStudioHdr )
		return;

	for( int i=0; i < pStudioHdr->GetNumPoseParameters(); i++)
	{
		poseParameter[i] = m_flClientPoseParameter[i];
	}
}