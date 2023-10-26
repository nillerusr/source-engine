#include "cbase.h"
#include "c_asw_trail_Beam.h"
#include "iviewrender_beams.h"
#include "beamdraw.h"
#include "c_te_effect_dispatch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_trail_beam_lifetime("asw_trail_beam_lifetime", "0.5f", 0, "How long the pellet trails last");
ConVar asw_trail_beam_width("asw_trail_beam_width", "3.0f", 0, "Initial width of the pellet trails");

#define ASW_TRAIL_BEAM_LIFETIME asw_trail_beam_lifetime.GetFloat()
#define ASW_TRAIL_BEAM_WIDTH asw_trail_beam_width.GetFloat()

C_ASW_Trail_Beam::C_ASW_Trail_Beam()
{
	m_fLifeLeft = 0;
	m_pBeam = NULL;
	m_hTarget = NULL;
	m_vecStartPoint.Init(0,0,0);
}

void C_ASW_Trail_Beam::InitBeam(Vector vecStartPoint, C_BaseEntity *pTarget)
{	
	if (!pTarget)
		return;

	m_fLifeLeft = ASW_TRAIL_BEAM_LIFETIME;

	m_hTarget = pTarget;
	m_vecStartPoint = vecStartPoint;

	// create our beam
	m_BeamInfo.m_nType = TE_BEAMPOINTS;
	m_BeamInfo.m_pStartEnt = NULL;
	m_BeamInfo.m_nStartAttachment = -1;
	m_BeamInfo.m_pEndEnt = NULL;
	m_BeamInfo.m_nEndAttachment = -1;
	m_BeamInfo.m_vecStart = vecStartPoint;
	m_BeamInfo.m_vecEnd = pTarget->GetAbsOrigin();
	m_BeamInfo.m_pszModelName = "swarm/sprites/greylaser1.vmt";
	m_BeamInfo.m_flHaloScale = 0.0f;
	m_BeamInfo.m_flLife = ASW_TRAIL_BEAM_LIFETIME * 5;
	m_BeamInfo.m_flWidth = ASW_TRAIL_BEAM_WIDTH;
	m_BeamInfo.m_flEndWidth = ASW_TRAIL_BEAM_WIDTH;
	m_BeamInfo.m_flFadeLength = 0.0f;
	m_BeamInfo.m_flAmplitude = 0;
	m_BeamInfo.m_flBrightness = 255.0;
	m_BeamInfo.m_flSpeed = 0.0f;
	m_BeamInfo.m_nStartFrame = 0.0;
	m_BeamInfo.m_flFrameRate = 0.0;
	m_BeamInfo.m_flRed = 255.0;
	m_BeamInfo.m_flGreen = 255.0;
	m_BeamInfo.m_flBlue = 255.0;
	m_BeamInfo.m_nSegments = 8;
	m_BeamInfo.m_bRenderable = true;
	m_BeamInfo.m_nFlags = FBEAM_FOREVER | FBEAM_ONLYNOISEONCE | FBEAM_SOLID;

	m_pBeam = beams->CreateBeamPoints(m_BeamInfo);

	SetNextClientThink(CLIENT_THINK_ALWAYS);
}

C_ASW_Trail_Beam::~C_ASW_Trail_Beam()
{

}

void C_ASW_Trail_Beam::ClientThink()
{
	m_fLifeLeft -= gpGlobals->frametime;

	C_BaseEntity *pTarget = GetBeamTarget();

	if (m_fLifeLeft <= 0 || !pTarget)
	{
		if (m_pBeam)
		{
			m_pBeam->life = 0.01f;
			m_pBeam->flags &= ~FBEAM_FOREVER;
			m_pBeam->width = 0;
			m_pBeam->endWidth = 0;
		}
		Release();
	}
	else
	{
		// set width of beam based on life left
		if (m_pBeam)
		{
			float fFractionLifeLeft = m_fLifeLeft / ASW_TRAIL_BEAM_LIFETIME;
			m_pBeam->width = ASW_TRAIL_BEAM_WIDTH * fFractionLifeLeft;
			m_pBeam->endWidth = ASW_TRAIL_BEAM_WIDTH * fFractionLifeLeft;

			BeamInfo_t beamInfo;
			beamInfo.m_vecStart = m_vecStartPoint;
			beamInfo.m_vecEnd = GetBeamTarget()->GetAbsOrigin();

			beams->UpdateBeamInfo( m_pBeam, beamInfo );
		}
		SetNextClientThink(CLIENT_THINK_ALWAYS);
	}	
}

C_BaseEntity* C_ASW_Trail_Beam::GetBeamTarget()
{
	if (!m_hTarget || !m_hTarget.IsValid())
		return NULL;
	return m_hTarget.Get();
}