#include "cbase.h"
#include "c_asw_gun_smoke_emitter.h"
#include "c_asw_generic_emitter.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// a smoke emitter that should be attached to the end of a gun
// the gun should notify this when it fires and this class will make smoke get
//  thicker after you've been firing it for a while and then stopped

LINK_ENTITY_TO_CLASS( client_asw_gun_smoke_emitter, C_ASW_Gun_Smoke_Emitter );

#define ASW_SMOKE_DELAY 0.01f

C_ASW_Gun_Smoke_Emitter::C_ASW_Gun_Smoke_Emitter()
{
	m_fFireCount = 0;
	m_fLastFireTime = 0;
	UseTemplate("autogunsmoke", true);
}

void C_ASW_Gun_Smoke_Emitter::ClientThink()
{
	if (gpGlobals->curtime - m_fLastFireTime > ASW_SMOKE_DELAY)
	{
		StartSmoking();
		m_fFireCount -= gpGlobals->frametime * 3;
		if (m_fFireCount < 0)
			m_fFireCount = 0;
	}
	BaseClass::ClientThink();
}

void C_ASW_Gun_Smoke_Emitter::OnFired()
{
	m_fLastFireTime = gpGlobals->curtime;
	StopSmoking();
	m_fFireCount = MIN(m_fFireCount + 0.5f, 10.0f);
}

void C_ASW_Gun_Smoke_Emitter::StopSmoking()
{
	m_bEmit = false;
}

void C_ASW_Gun_Smoke_Emitter::StartSmoking()
{
	m_bEmit = true;

	float density = MIN(80.0f, m_fFireCount * 8.0f);
	if (m_hEmitter.GetObject())
		m_hEmitter->m_ParticlesPerSecond = density;
}