//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "particle_smokegrenade.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_SERVERCLASS_ST(ParticleSmokeGrenade, DT_ParticleSmokeGrenade)
	SendPropTime(SENDINFO(m_flSpawnTime) ),
	SendPropFloat(SENDINFO(m_FadeStartTime), 0, SPROP_NOSCALE),
	SendPropFloat(SENDINFO(m_FadeEndTime), 0, SPROP_NOSCALE),
	SendPropInt(SENDINFO(m_CurrentStage), 1, SPROP_UNSIGNED),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( env_particlesmokegrenade, ParticleSmokeGrenade );

BEGIN_DATADESC( ParticleSmokeGrenade )

	DEFINE_FIELD( m_CurrentStage, FIELD_CHARACTER ),
	DEFINE_FIELD( m_FadeStartTime, FIELD_TIME ),
	DEFINE_FIELD( m_FadeEndTime, FIELD_TIME ),
	DEFINE_FIELD( m_flSpawnTime, FIELD_TIME ),

END_DATADESC()


ParticleSmokeGrenade::ParticleSmokeGrenade()
{
	m_CurrentStage = 0;
	m_FadeStartTime = 17;
	m_FadeEndTime = 22;

	m_flSpawnTime = gpGlobals->curtime;
}

ParticleSmokeGrenade::~ParticleSmokeGrenade()
{
}

// Smoke grenade particles should always transmitted to clients.  If not, a client who
// enters the PVS late will see the smoke start billowing from then, allowing better vision.
int ParticleSmokeGrenade::UpdateTransmitState( void )
{
	if ( IsEffectActive( EF_NODRAW ) )
		return SetTransmitState( FL_EDICT_DONTSEND );

	return SetTransmitState( FL_EDICT_ALWAYS );
}

void ParticleSmokeGrenade::Spawn()
{
	BaseClass::Spawn();

	SetNextThink( gpGlobals->curtime );
	m_creatorPlayer = NULL;
}

void ParticleSmokeGrenade::SetCreator(CBasePlayer *creator)
{
	m_creatorPlayer = creator;
}

CBasePlayer* ParticleSmokeGrenade::GetCreator()
{
	return (CBasePlayer*)m_creatorPlayer.Get();
}

void ParticleSmokeGrenade::FillVolume()
{
	m_CurrentStage = 1;
	CollisionProp()->SetCollisionBounds( Vector( -50, -50, -50 ), Vector( 50, 50, 50 ) );
}


void ParticleSmokeGrenade::SetFadeTime(float startTime, float endTime)
{
	m_FadeStartTime = startTime;
	m_FadeEndTime = endTime;
}

// Fade start and end are relative to current time
void ParticleSmokeGrenade::SetRelativeFadeTime(float startTime, float endTime)
{
	float flCurrentTime = gpGlobals->curtime - m_flSpawnTime;

	m_FadeStartTime = flCurrentTime + startTime;
	m_FadeEndTime = flCurrentTime + endTime;
}

void ParticleSmokeGrenade::Think()
{
	// Override, don't extend.  (Baseclass's Think just deletes.)

	float now = gpGlobals->curtime;
	if( now >= (m_flSpawnTime + m_FadeEndTime) )
	{
		// We are done
		UTIL_Remove(this);
		return;
	}

	const float PSG_THINK_DELAY = 1.0f;
	SetNextThink(now + PSG_THINK_DELAY);
}
