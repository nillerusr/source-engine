//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "c_prop_vehicle.h"
#include "movevars_shared.h"
#include "view.h"
#include "flashlighteffect.h"
#include "c_baseplayer.h"
#include "c_te_effect_dispatch.h"
#include "ammodef.h"

//#include "engine/ivdebugoverlay.h"
#include "datacache/imdlcache.h"
#include "view_scene.h"
//#include "ClientEffectPrecacheSystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar default_fov;

ConVar r_ApcViewBlendTo( "r_DrivableApcViewBlendTo", "1", FCVAR_CHEAT );
ConVar r_ApcViewBlendToScale( "r_DrivableApcViewBlendToScale", "0.03", FCVAR_CHEAT );
ConVar r_ApcViewBlendToTime( "r_DrivableApcViewBlendToTime", "1.5", FCVAR_CHEAT );
ConVar r_ApcFOV( "r_DrivableAPCFOV", "90", FCVAR_CHEAT );

#define APC_DELTA_LENGTH_MAX	12.0f			// 1 foot
#define APC_FRAMETIME_MIN		1e-6
#define APC_HEADLIGHT_DISTANCE 1000

extern void HLSS_DrawTargetHud(Vector vecOrigin, C_BasePlayer *pPlayer, C_BaseEntity *pTarget, int iEnemyType);

//=============================================================================
//
// Client-side Drivable APC Class
//
class C_PropDrivableAPC : public C_PropVehicleDriveable
{

	DECLARE_CLASS( C_PropDrivableAPC, C_PropVehicleDriveable );

public:

	DECLARE_CLIENTCLASS();
	DECLARE_INTERPOLATION();

	C_PropDrivableAPC();
	~C_PropDrivableAPC();

public:

	void UpdateViewAngles( C_BasePlayer *pLocalPlayer, CUserCmd *pCmd );
	void DampenEyePosition( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles );

	void OnEnteredVehicle( C_BasePlayer *pPlayer );
	void Simulate( void );

	virtual int GetPrimaryAmmoType() const;
	virtual int GetPrimaryAmmoClip() const;
	virtual bool PrimaryAmmoUsesClips() const;
	virtual int GetPrimaryAmmoCount() const;

	//virtual int GetPrimaryAmmoMaxClip();
	//virtual int GetPrimaryAmmoMaxCarry();

	virtual void DrawHudElements();

private:

	void DampenForwardMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime );
	void DampenUpMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime );
	void ComputePDControllerCoefficients( float *pCoefficientsOut, float flFrequency, float flDampening, float flDeltaTime );

private:

	Vector		m_vecLastEyePos;
	Vector		m_vecLastEyeTarget;
	Vector		m_vecEyeSpeed;
	Vector		m_vecTargetSpeed;

	Color		m_cCrosshairColor;

	float		m_flViewAngleDeltaTime;

	float		m_flAPCFOV;
	CHeadlightEffect *m_pHeadlight;
	bool		m_bHeadlightIsOn;

	int			m_iMachineGunBurstLeft;
	int			m_iRocketSalvoLeft;

	bool		m_bIsMounted;
	CHandle<C_BaseEntity>	m_hTarget;
	int			m_iTargetType;
};

IMPLEMENT_CLIENTCLASS_DT( C_PropDrivableAPC, DT_PropDrivableAPC, CPropDrivableAPC )
	RecvPropBool( RECVINFO( m_bHeadlightIsOn ) ),
	RecvPropInt( RECVINFO( m_iMachineGunBurstLeft ) ),
	RecvPropInt( RECVINFO( m_iRocketSalvoLeft ) ),
	RecvPropBool( RECVINFO( m_bIsMounted ) ),
	RecvPropEHandle( RECVINFO(m_hTarget) ),
	RecvPropInt( RECVINFO( m_iTargetType ) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
C_PropDrivableAPC::C_PropDrivableAPC()
{
	m_vecEyeSpeed.Init();
	m_flViewAngleDeltaTime = 0.0f;
	m_pHeadlight = NULL;
	m_ViewSmoothingData.flFOV = r_ApcFOV.GetFloat();

	m_cCrosshairColor = Color(0,174,160,255);
}

//-----------------------------------------------------------------------------
// Purpose: Deconstructor
//-----------------------------------------------------------------------------
C_PropDrivableAPC::~C_PropDrivableAPC()
{
	if ( m_pHeadlight )
	{
		delete m_pHeadlight;
	}
}

void C_PropDrivableAPC::Simulate( void )
{
	// The dim light is the flashlight.
	if ( m_bHeadlightIsOn )
	{
		if ( m_pHeadlight == NULL )
		{
			// Turned on the headlight; create it.
			m_pHeadlight = new CHeadlightEffect;

			if ( m_pHeadlight == NULL )
				return;

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
		
			m_pHeadlight->UpdateLight( vVector, vecForward, vecRight, vecUp, APC_HEADLIGHT_DISTANCE );
		}
	}
	else if ( m_pHeadlight )
	{
		// Turned off the flashlight; delete it.
		delete m_pHeadlight;
		m_pHeadlight = NULL;
	}

	BaseClass::Simulate();
}

//-----------------------------------------------------------------------------
// Purpose: Blend view angles.
//-----------------------------------------------------------------------------
void C_PropDrivableAPC::UpdateViewAngles( C_BasePlayer *pLocalPlayer, CUserCmd *pCmd )
{
	if ( r_ApcViewBlendTo.GetInt() )
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

		if ( m_flViewAngleDeltaTime > r_ApcViewBlendToTime.GetFloat() )
		{
			// Blend the view angles.
			int eyeAttachmentIndex = LookupAttachment( "vehicle_driver_eyes" );
			Vector vehicleEyeOrigin;
			QAngle vehicleEyeAngles;
			GetAttachmentLocal( eyeAttachmentIndex, vehicleEyeOrigin, vehicleEyeAngles );
			
			QAngle outAngles;
			InterpolateAngles( pCmd->viewangles, vehicleEyeAngles, outAngles, r_ApcViewBlendToScale.GetFloat() );
			pCmd->viewangles = outAngles;
		}
	}

	BaseClass::UpdateViewAngles( pLocalPlayer, pCmd );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_PropDrivableAPC::DampenEyePosition( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles )
{
#ifdef HL2_CLIENT_DLL
	// Get the frametime. (Check to see if enough time has passed to warrent dampening).
	float flFrameTime = gpGlobals->frametime;

	if ( flFrameTime < APC_FRAMETIME_MIN )
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
void C_PropDrivableAPC::ComputePDControllerCoefficients( float *pCoefficientsOut,
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
void C_PropDrivableAPC::DampenForwardMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime )
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
	if ( flDeltaLength > APC_DELTA_LENGTH_MAX )
	{
		// Clamp.
		float flDelta = flDeltaLength - APC_DELTA_LENGTH_MAX;
		if ( flDelta > 40.0f )
		{
			// This part is a bit of a hack to get rid of large deltas (at level load, etc.).
			m_vecLastEyePos = vecVehicleEyePos;
			m_vecEyeSpeed = vecVehicleEyeSpeed;
		}
		else
		{
			// Position clamp.
			float flRatio = APC_DELTA_LENGTH_MAX / flDeltaLength;
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
void C_PropDrivableAPC::DampenUpMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime )
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
void C_PropDrivableAPC::OnEnteredVehicle( C_BasePlayer *pPlayer )
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
// Draws crosshair in the forward direction of the boat
//-----------------------------------------------------------------------------
void C_PropDrivableAPC::DrawHudElements( )
{
	BaseClass::DrawHudElements();

	MDLCACHE_CRITICAL_SECTION();

	CHudTexture *pIcon = NULL;

	//pIcon = gHUD.GetIcon( "apc_hud"  );
	pIcon = gHUD.GetIcon( "gunhair" );

	if ( pIcon != NULL )
	{
		pIcon->EffectiveHeight(0.25f);
		pIcon->EffectiveWidth(0.25f);

		float x, y;
		Vector screen;

		x = ScreenWidth()/2;
		y = ScreenHeight()/2;

		int eyeAttachmentIndex = LookupAttachment( "Muzzle" );		//vehicle_driver_eyes
		Vector vehicleEyeOrigin;
		QAngle vehicleEyeAngles;

		//TERO: attempt to fix the zooming aiming
		//float angleFix = (m_hPlayer->GetDefaultFOV() / m_hPlayer->GetFOV()) - 1;
		//vehicleEyeAngles.x -= angleFix; // * 0.5;

		GetAttachment( eyeAttachmentIndex, vehicleEyeOrigin, vehicleEyeAngles );

		// Only worry about yaw.
		//vehicleEyeAngles.x = vehicleEyeAngles.z = 0.0f;

		Vector vecForward;
		AngleVectors( vehicleEyeAngles, &vecForward );
		VectorMA( vehicleEyeOrigin, 700.0f, vecForward, vehicleEyeOrigin );

		ScreenTransform( vehicleEyeOrigin, screen );
		x += 0.5 * screen[0] * ScreenWidth() + 0.5;
		y -= 0.5 * screen[1] * ScreenHeight() + 0.5;

		x -= pIcon->Width() / 2; 
		y -= pIcon->Height() / 2; 

		int r,g,b,a;
		if ( m_hTarget )
		{
			r = (int)Approach( 255,	m_cCrosshairColor.r(), 20);
			g = (int)Approach( 0,	m_cCrosshairColor.g(), 20);
			b = (int)Approach( 0,	m_cCrosshairColor.b(), 20);
		}
		else
		{
			r = (int)Approach( 0,	m_cCrosshairColor.r(), 16);
			g = (int)Approach( 174,	m_cCrosshairColor.g(), 16);
			b = (int)Approach( 160,	m_cCrosshairColor.b(), 16);
		}

		if (m_hPlayer->IsZoomed())
		{
			a = (int)Approach( 0, m_cCrosshairColor.a(), 20);
		}
		else
		{
			a = (int)Approach( 255, m_cCrosshairColor.a(), 20);
		}

		m_cCrosshairColor.SetColor(r,g,b,a);
		pIcon->DrawSelf( x, y, m_cCrosshairColor );

	}

	int eyeAttachmentIndex = LookupAttachment( "vehicle_driver_eyes" );

	Vector vecOrigin;
	GetAttachment( eyeAttachmentIndex, vecOrigin );

#ifndef EZ // Disabled for now
	HLSS_DrawTargetHud(vecOrigin, m_hPlayer, m_hTarget, m_iTargetType);
#endif
}

//-----------------------------------------------------------------------------
// Draws the ammo for the airboat
//-----------------------------------------------------------------------------
int C_PropDrivableAPC::GetPrimaryAmmoType() const
{
	int nAmmoType = GetAmmoDef()->Index( "CombineCannon" );
	return nAmmoType; 
}

int C_PropDrivableAPC::GetPrimaryAmmoCount() const
{ 
	return m_iRocketSalvoLeft; 
}

bool C_PropDrivableAPC::PrimaryAmmoUsesClips() const
{ 
	return true; 
}

int C_PropDrivableAPC::GetPrimaryAmmoClip() const
{ 
	return m_iMachineGunBurstLeft; 
}

/*int C_PropDrivableAPC::GetPrimaryAmmoMaxClip()
{
	return 30;
}
int C_PropDrivableAPC::GetPrimaryAmmoMaxCarry()
{
	return 3;
}*/