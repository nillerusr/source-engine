#include "cbase.h"
#include "asw_director.h"
#include "asw_game_resource.h"
#include "asw_marine_resource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_director_debug;

ConVar asw_intensity_scale("asw_intensity_scale", "0.25", FCVAR_CHEAT, "Scales intensity increases for marines");
ConVar asw_intensity_decay_time("asw_intensity_decay_time", "20", FCVAR_CHEAT, "Seconds to decay full intensity to zero");
ConVar asw_intensity_inhibit_delay("asw_intensity_inhibit_delay", "3.5", FCVAR_CHEAT, "Seconds before intensity starts to decay after an increase");
ConVar asw_intensity_far_range( "asw_intensity_far_range", "200", FCVAR_CHEAT, "Enemies killed past this distance will only slightly increase intensity" );

CASW_Intensity::CASW_Intensity()
{
	m_flIntensity = 0.0f;
}

void CASW_Intensity::Increase( IntensityType i )
{
	float value = 0.0f;

	switch( i )
	{
	case MILD:
		value = 0.05f;
		break;

	case MODERATE:
		value = 0.2f;
		break;

	case HIGH:
		value = 0.5f;
		break;

	case EXTREME:
		value = 1.0f;
		break;

	case MAXIMUM:
		value = 999999.9f;		// force intensity to max
		break;
	}

	m_flIntensity += asw_intensity_scale.GetFloat() * value;
	if (m_flIntensity > 1.0f)
	{
		m_flIntensity = 1.0f;
	}

	// don't decay immediately
	InhibitDecay( asw_intensity_inhibit_delay.GetFloat() );
}

void CASW_Intensity::Update( float fDeltaTime )
{
	if (m_decayInhibitTimer.IsElapsed())
	{
		m_flIntensity -= fDeltaTime / asw_intensity_decay_time.GetFloat();
		if (m_flIntensity < 0.0f)
		{
			m_flIntensity = 0.0f;
		}
	}
}


void CASW_Director::UpdateIntensity()
{
	float fDeltaTime = m_IntensityUpdateTimer.GetElapsedTime();
	m_IntensityUpdateTimer.Start();

	CASW_Game_Resource *pGameResource = ASWGameResource();
	if ( !pGameResource )
		return;

	for ( int i=0;i<pGameResource->GetMaxMarineResources();i++ )
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
		if ( !pMR )
			continue;

		pMR->GetIntensity()->Update( fDeltaTime );

		if ( asw_director_debug.GetInt() > 0 )
		{
			engine->Con_NPrintf( i + 2, "Marine %d Intensity = %f", i, pMR->GetIntensity()->GetCurrent() );
		}
	}
}


float CASW_Director::GetMaxIntensity()
{
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if ( !pGameResource )
		return 0.0f;

	float flIntensity = 0;
	for ( int i=0;i<pGameResource->GetMaxMarineResources();i++ )
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
		if ( !pMR )
			continue;

		flIntensity = MAX( flIntensity, pMR->GetIntensity()->GetCurrent() );
	}

	return flIntensity;
}