#include "cbase.h"
#include "c_asw_rocket.h"
#include "c_asw_generic_emitter_entity.h"
#include "iviewrender_beams.h"
#include "beamdraw.h"
#include "engine/ivmodelinfo.h"
#include "soundenvelope.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Rocket, DT_ASW_Rocket, CASW_Rocket)
	
END_RECV_TABLE()

ConVar asw_rocket_volume_time( "asw_rocket_volume_time", "0.3", FCVAR_NONE, "Time taken to fade in rocket loop sound" );

C_ASW_Rocket::C_ASW_Rocket()
{
	m_pSmokeTrail = NULL;
	m_pLoopingSound = NULL;
}

C_ASW_Rocket::~C_ASW_Rocket()
{
}

void C_ASW_Rocket::ClientThink()
{
	SetNextClientThink(CLIENT_THINK_ALWAYS);
}

void C_ASW_Rocket::OnDataChanged(DataUpdateType_t updateType)
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		CreateSmokeTrail();
		SoundInit();
		SetNextClientThink(CLIENT_THINK_ALWAYS);
	}
	BaseClass::OnDataChanged(updateType);
}

void C_ASW_Rocket::CreateSmokeTrail()
{
	if ( m_pSmokeTrail )
		return;

	m_pSmokeTrail = ParticleProp()->Create( "rocket_trail_small", PATTACH_ABSORIGIN_FOLLOW, -1, Vector( 0, 0, 0 ) );
}

void C_ASW_Rocket::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();
	if( m_pSmokeTrail )
	{
		m_pSmokeTrail->StopEmission(false, false, true);
		m_pSmokeTrail = NULL;
	}

	if ( m_pLoopingSound )
	{
		CSoundEnvelopeController::GetController().SoundDestroy( m_pLoopingSound );
		m_pLoopingSound = NULL;
	}
}

ConVar asw_rocket_trail_width("asw_rocket_trail_width", "1.5f", FCVAR_CHEAT);
ConVar asw_rocket_trail_fade("asw_rocket_trail_fade", "0.0f", FCVAR_CHEAT);
ConVar asw_rocket_trail_life("asw_rocket_trail_life", "0.5f", FCVAR_CHEAT);
ConVar asw_rocket_trail_r("asw_rocket_trail_r", "255", FCVAR_CHEAT);
ConVar asw_rocket_trail_g("asw_rocket_trail_g", "255", FCVAR_CHEAT);
ConVar asw_rocket_trail_b("asw_rocket_trail_b", "128", FCVAR_CHEAT);
ConVar asw_rocket_trail_a("asw_rocket_trail_a", "40", FCVAR_CHEAT);
ConVar asw_rocket_trail_material("asw_rocket_trail_material", "sprites/laserbeam.vmt", FCVAR_CHEAT);


void C_ASW_Rocket::SoundInit()
{
	CPASAttenuationFilter filter( this );

	// Start the parasite's looping sound
	if( !m_pLoopingSound )
	{
		m_pLoopingSound = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), "ASWRocket.Loop" );
		CSoundEnvelopeController::GetController().Play( m_pLoopingSound, 0.0, 100 );
		CSoundEnvelopeController::GetController().SoundChangeVolume( m_pLoopingSound, 1.0, asw_rocket_volume_time.GetFloat() );
	}
}
