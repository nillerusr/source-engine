#include "cbase.h"
#include "c_asw_jeep.h"
#include "movevars_shared.h"
#include "view.h"
#include "flashlighteffect.h"
#include "c_baseplayer.h"
#include "c_te_effect_dispatch.h"
#include "c_asw_marine.h"
#include "c_asw_player.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include "asw_util_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar default_fov;

ConVar asw_r_JeepViewBlendTo( "asw_r_JeepViewBlendTo", "1", FCVAR_CHEAT );
ConVar asw_r_JeepViewBlendToScale( "asw_r_JeepViewBlendToScale", "0.03", FCVAR_CHEAT );
ConVar asw_r_JeepViewBlendToTime( "asw_r_JeepViewBlendToTime", "1.5", FCVAR_CHEAT );
ConVar asw_r_JeepFOV( "asw_r_JeepFOV", "90", FCVAR_CHEAT );

	//DEFINE_EMBEDDED( m_VehiclePhysics ),

	// These are necessary to save here because the 'owner' of these fields must be the prop_vehicle
	//DEFINE_PHYSPTR( m_VehiclePhysics.m_pVehicle ),
	//DEFINE_PHYSPTR_ARRAY( m_VehiclePhysics.m_pWheels ),

IMPLEMENT_CLIENTCLASS_DT( C_ASW_PropJeep, DT_ASW_PropJeep, CASW_PropJeep )
	RecvPropBool( RECVINFO( m_bHeadlightIsOn ) ),
	RecvPropEHandle( RECVINFO( m_hDriver ) ),
END_RECV_TABLE()

C_ASW_PropJeep::C_ASW_PropJeep()
{
	m_vecEyeSpeed.Init();	
	m_flViewAngleDeltaTime = 0.0f;
	m_pHeadlight = NULL;
	m_ViewSmoothingData.flFOV = asw_r_JeepFOV.GetFloat();
}

C_ASW_PropJeep::~C_ASW_PropJeep()
{
	if ( m_pHeadlight )
	{
		delete m_pHeadlight;
	}
}

bool C_ASW_PropJeep::Simulate( void )
{
	// The dim light is the flashlight.
	if ( m_bHeadlightIsOn )
	{
		if ( m_pHeadlight == NULL )
		{
			// Turned on the headlight; create it.
			m_pHeadlight = new CHeadlightEffect;

			if ( m_pHeadlight == NULL )
				return false;

			m_pHeadlight->TurnOn();
		}

		QAngle vAngle;
		Vector vVector;
		Vector vecForward, vecRight, vecUp;

		int iAttachment = LookupAttachment( "headlight" );

		if ( iAttachment != -1 )
		{
			GetAttachment( iAttachment, vVector, vAngle );
			AngleVectors( vAngle, &vecForward, &vecRight, &vecUp );
		
			m_pHeadlight->UpdateLight( vVector, vecForward, vecRight, vecUp, JEEP_HEADLIGHT_DISTANCE );
		}
	}
	else if ( m_pHeadlight )
	{
		// Turned off the flashlight; delete it.
		delete m_pHeadlight;
		m_pHeadlight = NULL;
	}

	BaseClass::Simulate();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Blend view angles.
//-----------------------------------------------------------------------------
void C_ASW_PropJeep::UpdateViewAngles( C_BasePlayer *pLocalPlayer, CUserCmd *pCmd )
{
	if ( asw_r_JeepViewBlendTo.GetInt() )
	{
		// Check to see if the mouse has been touched in a bit or that we are not throttling.
		if ( ( pCmd->mousedx != 0 || pCmd->mousedy != 0 ) || ( fabsf( m_flThrottle ) < 0.01f ) )
		{
			m_flViewAngleDeltaTime = 0.0f;
		}
		else
		{
			m_flViewAngleDeltaTime += gpGlobals->frametime;
		}

		if ( m_flViewAngleDeltaTime > asw_r_JeepViewBlendToTime.GetFloat() )
		{
			// Blend the view angles.
			int eyeAttachmentIndex = LookupAttachment( "vehicle_driver_eyes" );
			Vector vehicleEyeOrigin;
			QAngle vehicleEyeAngles;
			GetAttachmentLocal( eyeAttachmentIndex, vehicleEyeOrigin, vehicleEyeAngles );
			
			QAngle outAngles;
			InterpolateAngles( pCmd->viewangles, vehicleEyeAngles, outAngles, asw_r_JeepViewBlendToScale.GetFloat() );
			pCmd->viewangles = outAngles;
		}
	}

	BaseClass::UpdateViewAngles( pLocalPlayer, pCmd );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_ASW_PropJeep::DampenEyePosition( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles )
{
#ifdef HL2_CLIENT_DLL
	// Get the frametime. (Check to see if enough time has passed to warrent dampening).
	float flFrameTime = gpGlobals->frametime;

	if ( flFrameTime < JEEP_FRAMETIME_MIN )
	{
		vecVehicleEyePos = m_vecLastEyePos;
		DampenUpMotion( vecVehicleEyePos, vecVehicleEyeAngles, 0.0f );
		return;
	}

	// Keep static the sideways motion.
	// Dampen forward/backward motion.
	DampenForwardMotion( vecVehicleEyePos, vecVehicleEyeAngles, flFrameTime );

	// Blend up/down motion.
	DampenUpMotion( vecVehicleEyePos, vecVehicleEyeAngles, flFrameTime );
#endif
}


//-----------------------------------------------------------------------------
// Use the controller as follows:
// speed += ( pCoefficientsOut[0] * ( targetPos - currentPos ) + pCoefficientsOut[1] * ( targetSpeed - currentSpeed ) ) * flDeltaTime;
//-----------------------------------------------------------------------------
void C_ASW_PropJeep::ComputePDControllerCoefficients( float *pCoefficientsOut,
												  float flFrequency, float flDampening,
												  float flDeltaTime )
{
	float flKs = 9.0f * flFrequency * flFrequency;
	float flKd = 4.5f * flFrequency * flDampening;

	float flScale = 1.0f / ( 1.0f + flKd * flDeltaTime + flKs * flDeltaTime * flDeltaTime );

	pCoefficientsOut[0] = flKs * flScale;
	pCoefficientsOut[1] = ( flKd + flKs * flDeltaTime ) * flScale;
}
 
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_ASW_PropJeep::DampenForwardMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime )
{
	// vecVehicleEyePos = real eye position this frame

	// m_vecLastEyePos = eye position last frame
	// m_vecEyeSpeed = eye speed last frame
	// vecPredEyePos = predicted eye position this frame (assuming no acceleration - it will get that from the pd controller).
	// vecPredEyeSpeed = predicted eye speed
	Vector vecPredEyePos = m_vecLastEyePos + m_vecEyeSpeed * flFrameTime;
	Vector vecPredEyeSpeed = m_vecEyeSpeed;

	// m_vecLastEyeTarget = real eye position last frame (used for speed calculation).
	// Calculate the approximate speed based on the current vehicle eye position and the eye position last frame.
	Vector vecVehicleEyeSpeed = ( vecVehicleEyePos - m_vecLastEyeTarget ) / flFrameTime;
	m_vecLastEyeTarget = vecVehicleEyePos;
	if (vecVehicleEyeSpeed.Length() == 0.0)
		return;

	// Calculate the delta between the predicted eye position and speed and the current eye position and speed.
	Vector vecDeltaSpeed = vecVehicleEyeSpeed - vecPredEyeSpeed;
	Vector vecDeltaPos = vecVehicleEyePos - vecPredEyePos;

	// Forward vector.
	Vector vecForward;
	AngleVectors( vecVehicleEyeAngles, &vecForward );

	float flDeltaLength = vecDeltaPos.Length();
	if ( flDeltaLength > JEEP_DELTA_LENGTH_MAX )
	{
		// Clamp.
		float flDelta = flDeltaLength - JEEP_DELTA_LENGTH_MAX;
		if ( flDelta > 40.0f )
		{
			// This part is a bit of a hack to get rid of large deltas (at level load, etc.).
			m_vecLastEyePos = vecVehicleEyePos;
			m_vecEyeSpeed = vecVehicleEyeSpeed;
		}
		else
		{
			// Position clamp.
			float flRatio = JEEP_DELTA_LENGTH_MAX / flDeltaLength;
			vecDeltaPos *= flRatio;
			Vector vecForwardOffset = vecForward * ( vecForward.Dot( vecDeltaPos ) );
			vecVehicleEyePos -= vecForwardOffset;
			m_vecLastEyePos = vecVehicleEyePos;

			// Speed clamp.
			vecDeltaSpeed *= flRatio;
			float flCoefficients[2];
			ComputePDControllerCoefficients( flCoefficients, r_JeepViewDampenFreq.GetFloat(), r_JeepViewDampenDamp.GetFloat(), flFrameTime );
			m_vecEyeSpeed += ( ( flCoefficients[0] * vecDeltaPos + flCoefficients[1] * vecDeltaSpeed ) * flFrameTime );
		}
	}
	else
	{
		// Generate an updated (dampening) speed for use in next frames position prediction.
		float flCoefficients[2];
		ComputePDControllerCoefficients( flCoefficients, r_JeepViewDampenFreq.GetFloat(), r_JeepViewDampenDamp.GetFloat(), flFrameTime );
		m_vecEyeSpeed += ( ( flCoefficients[0] * vecDeltaPos + flCoefficients[1] * vecDeltaSpeed ) * flFrameTime );
		
		// Save off data for next frame.
		m_vecLastEyePos = vecPredEyePos;
		
		// Move eye forward/backward.
		Vector vecForwardOffset = vecForward * ( vecForward.Dot( vecDeltaPos ) );
		vecVehicleEyePos -= vecForwardOffset;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_ASW_PropJeep::DampenUpMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime )
{
	// Get up vector.
	Vector vecUp;
	AngleVectors( vecVehicleEyeAngles, NULL, NULL, &vecUp );
	vecUp.z = clamp( vecUp.z, 0.0f, vecUp.z );
	vecVehicleEyePos.z += r_JeepViewZHeight.GetFloat() * vecUp.z;

	// NOTE: Should probably use some damped equation here.
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ASW_PropJeep::OnEnteredVehicle( C_BasePlayer *pPlayer )
{
	int eyeAttachmentIndex = LookupAttachment( "vehicle_driver_eyes" );
	Vector vehicleEyeOrigin;
	QAngle vehicleEyeAngles;
	GetAttachment( eyeAttachmentIndex, vehicleEyeOrigin, vehicleEyeAngles );

	m_vecLastEyeTarget = vehicleEyeOrigin;
	m_vecLastEyePos = vehicleEyeOrigin;
	m_vecEyeSpeed = vec3_origin;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void ASWWheelDustCallback( const CEffectData &data )
{
	CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "dust" );
	pSimple->SetSortOrigin( data.m_vOrigin );
	pSimple->SetNearClip( 32, 64 );

	SimpleParticle	*pParticle;

	Vector	offset;

	//FIXME: Better sampling area
	offset = data.m_vOrigin + ( data.m_vNormal * data.m_flScale );
	
	//Find area ambient light color and use it to tint smoke
	Vector	worldLight = WorldGetLightForPoint( offset, true );

	PMaterialHandle	hMaterial = pSimple->GetPMaterial("particle/particle_smokegrenade");;

	//Throw puffs
	offset.Random( -(data.m_flScale*16.0f), data.m_flScale*16.0f );
	offset.z = 0.0f;
	offset += data.m_vOrigin + ( data.m_vNormal * data.m_flScale );

	pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof(SimpleParticle), hMaterial, offset );

	if ( pParticle != NULL )
	{			
		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= random->RandomFloat( 0.25f, 0.5f );
		
		pParticle->m_vecVelocity = RandomVector( -1.0f, 1.0f );
		VectorNormalize( pParticle->m_vecVelocity );
		pParticle->m_vecVelocity[2] += random->RandomFloat( 16.0f, 32.0f ) * (data.m_flScale*2.0f);

		int	color = random->RandomInt( 100, 150 );

		pParticle->m_uchColor[0] = 16 + ( worldLight[0] * (float) color );
		pParticle->m_uchColor[1] = 8 + ( worldLight[1] * (float) color );
		pParticle->m_uchColor[2] = ( worldLight[2] * (float) color );

		pParticle->m_uchStartAlpha	= random->RandomInt( 64.0f*data.m_flScale, 128.0f*data.m_flScale );
		pParticle->m_uchEndAlpha	= 0;
		pParticle->m_uchStartSize	= random->RandomInt( 16, 24 ) * data.m_flScale;
		pParticle->m_uchEndSize		= random->RandomInt( 32, 48 ) * data.m_flScale;
		pParticle->m_flRoll			= random->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= random->RandomFloat( -2.0f, 2.0f );
	}
}

DECLARE_CLIENT_EFFECT( ASWWheelDust, ASWWheelDustCallback );

// implement driver interface
C_ASW_Marine* C_ASW_PropJeep::ASWGetDriver()
{
	return dynamic_cast<C_ASW_Marine*>(m_hDriver.Get());
}

// implement client vehicle interface
bool C_ASW_PropJeep::s_bLoadedDriveIconTexture = false;
int  C_ASW_PropJeep::s_nDriveIconTextureID = -1;
bool C_ASW_PropJeep::s_bLoadedRideIconTexture = false;
int  C_ASW_PropJeep::s_nRideIconTextureID = -1;

int C_ASW_PropJeep::GetDriveIconTexture()
{
	if (!s_bLoadedDriveIconTexture)
	{
		// load the portrait textures
		s_nDriveIconTextureID = vgui::surface()->CreateNewTextureID();		
		vgui::surface()->DrawSetTextureFile( s_nDriveIconTextureID, "vgui/swarm/UseIcons/PanelUnlocked", true, false);
		s_bLoadedDriveIconTexture = true;
	}

	return s_nDriveIconTextureID;
}
int C_ASW_PropJeep::GetRideIconTexture()
{
	if (!s_bLoadedRideIconTexture)
	{
		// load the portrait textures
		s_nRideIconTextureID = vgui::surface()->CreateNewTextureID();		
		vgui::surface()->DrawSetTextureFile( s_nRideIconTextureID, "vgui/swarm/UseIcons/PanelUnlocked", true, false);
		s_bLoadedRideIconTexture = true;
	}

	return s_nRideIconTextureID;
}

bool C_ASW_PropJeep::MarineInVehicle()
{
	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	return (pPlayer && pPlayer->GetMarine() && pPlayer->GetMarine()->IsInVehicle());
}

const char* C_ASW_PropJeep::GetDriveIconText()
{
	if (MarineInVehicle())
		return "Exit Vehicle";

	return "Drive";
}

const char* C_ASW_PropJeep::GetRideIconText()
{
	if (MarineInVehicle())
		return "Exit Vehicle";

	return "Passenger";
}

bool C_ASW_PropJeep::IsUsable(C_BaseEntity *pUser)
{
	return (pUser && pUser->GetAbsOrigin().DistTo(GetAbsOrigin()) < ASW_MARINE_USE_RADIUS);	// near enough?
}

bool C_ASW_PropJeep::GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser)
{
	action.iUseIconTexture = GetDriveIconTexture();
	TryLocalize( GetDriveIconText(), action.wszText, sizeof( action.wszText ) );
	action.UseTarget = GetEntity();
	action.fProgress = -1;
	action.UseIconRed = 255;
	action.UseIconGreen = 255;
	action.UseIconBlue = 255;
	action.bShowUseKey = true;
	action.iInventorySlot = -1;
	return true;
}