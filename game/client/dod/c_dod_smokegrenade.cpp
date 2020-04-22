//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "c_dod_smokegrenade.h"
#include "dod_shareddefs.h"
#include "tier1/KeyValues.h"
#include "toolframework_client.h"
#include "fx.h"
#include "view.h"
#include "smoke_fog_overlay.h"

IMPLEMENT_NETWORKCLASS_ALIASED( DODSmokeGrenade, DT_DODSmokeGrenade )

BEGIN_RECV_TABLE(C_DODSmokeGrenade, DT_DODSmokeGrenade )
	RecvPropTime(RECVINFO(m_flSmokeSpawnTime)),
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_DODSmokeGrenade::C_DODSmokeGrenade( void )
{
}

const char *C_DODSmokeGrenade::GetOverviewSpriteName( void )
{
	const char *pszSprite = "";

	switch( GetTeamNumber() )
	{
	case TEAM_ALLIES:
		pszSprite = "sprites/minimap_icons/minimap_smoke_us";
		break;
	case TEAM_AXIS:
		pszSprite = "sprites/minimap_icons/minimap_smoke_ger";
		break;
	default:
		break;
	}

	return pszSprite;
}

void C_DODSmokeGrenade::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged(updateType);

	if(updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

static inline float& EngineGetSmokeFogOverlayAlpha()
{
	return g_SmokeFogOverlayAlpha;
}

#define SMOKE_CLOUD_RADIUS 330
#define EXPAND_TIME	2.0

float C_DODSmokeGrenade::CalcSmokeCloudRadius( void )
{
	float flLifetime = gpGlobals->curtime - m_flSmokeSpawnTime;
	if( flLifetime > EXPAND_TIME )
		flLifetime = EXPAND_TIME;
		
	float flRadius = SMOKE_CLOUD_RADIUS * (float)sin(flLifetime * M_PI * 0.5 / EXPAND_TIME);

	return flRadius;
}

float C_DODSmokeGrenade::CalcSmokeCloudAlpha( void )
{
	float flLifetime = gpGlobals->curtime - m_flSmokeSpawnTime;
	//if( flLifetime > SMOKESPHERE_EXPAND_TIME )
	//	flLifetime = SMOKESPHERE_EXPAND_TIME;

	const float flFadedInTime = 3;
	const float flStartFadingOutTime = 9;
	const float flEndFadingOutTime = 12;

	float flFadeAlpha;

	// Update our fade alpha.
	if( flLifetime < flFadedInTime )
	{
		float fadePercent = flLifetime / flFadedInTime;
		flFadeAlpha = SimpleSpline( fadePercent );
	}
	else if ( flLifetime > flEndFadingOutTime )
	{
		flFadeAlpha = 0.0;
	}
	else if ( flLifetime > flStartFadingOutTime )
	{
		float fadePercent = ( flLifetime - flStartFadingOutTime ) / ( flEndFadingOutTime - flStartFadingOutTime );
		flFadeAlpha = SimpleSpline( 1.0 - fadePercent );
	}
	else
	{
		flFadeAlpha = 1.0;
	}

	return flFadeAlpha;
}

// Add our influence to the global smoke fog alpha.
void C_DODSmokeGrenade::ClientThink( void )
{
	if ( m_flSmokeSpawnTime > 0 )
	{
		float flExpandRadius = CalcSmokeCloudRadius();

		Vector vecSmokePos = GetAbsOrigin();
		vecSmokePos.z += 32;

		float testDist = (MainViewOrigin() - vecSmokePos).Length();

		// The center of the smoke cloud that always gives full fog overlay
		float flCoreDistance = flExpandRadius * 0.3;

		if( testDist < flExpandRadius )
		{			
			float flFadeAlpha = CalcSmokeCloudAlpha();

			if( testDist < flCoreDistance )
			{
				EngineGetSmokeFogOverlayAlpha() += flFadeAlpha;
			}
			else
			{
				EngineGetSmokeFogOverlayAlpha() += (1 - ( testDist - flCoreDistance ) / ( flExpandRadius - flCoreDistance ) ) * flFadeAlpha;
			}
		}	
	}
}