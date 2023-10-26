//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


extern bool g_bUseCustomAutoExposureMin;
extern bool g_bUseCustomAutoExposureMax;
extern bool g_bUseCustomBloomScale;
extern float g_flCustomAutoExposureMin;
extern float g_flCustomAutoExposureMax;
extern float g_flCustomBloomScale;
extern float g_flCustomBloomScaleMinimum;

EHANDLE g_hTonemapControllerInUse = NULL;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_EnvTonemapController : public C_BaseEntity
{
	DECLARE_CLASS( C_EnvTonemapController, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();

	C_EnvTonemapController();

private:
	bool m_bUseCustomAutoExposureMin;
	bool m_bUseCustomAutoExposureMax;
	bool m_bUseCustomBloomScale;
	float m_flCustomAutoExposureMin;
	float m_flCustomAutoExposureMax;
	float m_flCustomBloomScale;
	float m_flCustomBloomScaleMinimum;
	float m_flBloomExponent;
	float m_flBloomSaturation;
private:
	C_EnvTonemapController( const C_EnvTonemapController & );

	friend void GetTonemapSettingsFromEnvTonemapController( void );
};

IMPLEMENT_CLIENTCLASS_DT( C_EnvTonemapController, DT_EnvTonemapController, CEnvTonemapController )
	RecvPropInt( RECVINFO(m_bUseCustomAutoExposureMin) ),
	RecvPropInt( RECVINFO(m_bUseCustomAutoExposureMax) ),
	RecvPropInt( RECVINFO(m_bUseCustomBloomScale) ),
	RecvPropFloat( RECVINFO(m_flCustomAutoExposureMin) ),
	RecvPropFloat( RECVINFO(m_flCustomAutoExposureMax) ),
	RecvPropFloat( RECVINFO(m_flCustomBloomScale) ),
	RecvPropFloat( RECVINFO(m_flCustomBloomScaleMinimum) ),
	RecvPropFloat( RECVINFO(m_flBloomExponent) ),
	RecvPropFloat( RECVINFO(m_flBloomSaturation) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_EnvTonemapController::C_EnvTonemapController( void )
{
	m_bUseCustomAutoExposureMin = false;
	m_bUseCustomAutoExposureMax = false;
	m_bUseCustomBloomScale = false;
	m_flCustomAutoExposureMin = 0;
	m_flCustomAutoExposureMax = 0;
	m_flCustomBloomScale = 0.0f;
	m_flCustomBloomScaleMinimum = 0.0f;
	m_flBloomExponent = 2.5f;
	m_flBloomSaturation = 1.0f;
}



void GetTonemapSettingsFromEnvTonemapController( void )
{
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	if ( localPlayer )
	{
		C_EnvTonemapController *tonemapController = dynamic_cast< C_EnvTonemapController * >(localPlayer->m_hTonemapController.Get());
		if ( tonemapController != NULL )
		{
			g_bUseCustomAutoExposureMin = tonemapController->m_bUseCustomAutoExposureMin;
			g_bUseCustomAutoExposureMax = tonemapController->m_bUseCustomAutoExposureMax;
			g_bUseCustomBloomScale = tonemapController->m_bUseCustomBloomScale;
			g_flCustomAutoExposureMin = tonemapController->m_flCustomAutoExposureMin;
			g_flCustomAutoExposureMax = tonemapController->m_flCustomAutoExposureMax;
			g_flCustomBloomScale = tonemapController->m_flCustomBloomScale;
			g_flCustomBloomScaleMinimum = tonemapController->m_flCustomBloomScaleMinimum;
			return;
		}
	}

	g_bUseCustomAutoExposureMax = false;
	g_bUseCustomBloomScale = false;
}
