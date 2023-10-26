#include "cbase.h"
#include "c_asw_railgun_beam.h"
#include "iviewrender_beams.h"
#include "beamdraw.h"
#include "c_te_effect_dispatch.h"
#include "c_asw_fx.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_railgun_beam_lifetime("asw_railgun_beam_lifetime", "0.5f", 0, "How long the railgun beam trails last");
ConVar asw_railgun_beam_width("asw_railgun_beam_width", "20.0f", 0, "Initial width of the railgun beam trail");

#define ASW_RAILGUN_BEAM_LIFETIME asw_railgun_beam_lifetime.GetFloat()
#define ASW_RAILGUN_BEAM_WIDTH asw_railgun_beam_width.GetFloat()

C_ASW_Railgun_Beam::C_ASW_Railgun_Beam()
{
	m_fLifeLeft = 0;
	m_pBeam = NULL;
}

void C_ASW_Railgun_Beam::InitBeam(Vector vecStartPoint, Vector vecEndPoint)
{	
	m_fLifeLeft = ASW_RAILGUN_BEAM_LIFETIME;

	// create our beam
	m_BeamInfo.m_nType = TE_BEAMPOINTS;
	m_BeamInfo.m_pStartEnt = NULL;
	m_BeamInfo.m_nStartAttachment = -1;
	m_BeamInfo.m_pEndEnt = NULL;
	m_BeamInfo.m_nEndAttachment = -1;
	m_BeamInfo.m_vecStart = vecStartPoint;
	m_BeamInfo.m_vecEnd = vecEndPoint;
	m_BeamInfo.m_pszModelName = "swarm/effects/railgun.vmt";
	m_BeamInfo.m_flHaloScale = 0.0f;
	m_BeamInfo.m_flLife = ASW_RAILGUN_BEAM_LIFETIME * 5;
	m_BeamInfo.m_flWidth = ASW_RAILGUN_BEAM_WIDTH;
	m_BeamInfo.m_flEndWidth = ASW_RAILGUN_BEAM_WIDTH;
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

C_ASW_Railgun_Beam::~C_ASW_Railgun_Beam()
{

}

void C_ASW_Railgun_Beam::ClientThink()
{
	m_fLifeLeft -= gpGlobals->frametime;

	if (m_fLifeLeft <= 0)
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
			float fFractionLifeLeft = m_fLifeLeft / ASW_RAILGUN_BEAM_LIFETIME;
			m_pBeam->width = ASW_RAILGUN_BEAM_WIDTH * fFractionLifeLeft;
			m_pBeam->endWidth = ASW_RAILGUN_BEAM_WIDTH * fFractionLifeLeft;			
		}
		SetNextClientThink(CLIENT_THINK_ALWAYS);
	}	
}


Vector GetRailgunBeamStart( const CEffectData &data )
{
	Vector vecStart = data.m_vStart;
	QAngle vecAngles;

	int iAttachment = data.m_nAttachmentIndex;

	// Attachment?
	if ( data.m_fFlags & TRACER_FLAG_USEATTACHMENT )
	{
		// If the entity specified is a weapon being carried by this player, use the viewmodel instead
		IClientRenderable *pRenderable = data.GetRenderable();
		if ( !pRenderable )
			return vecStart;

		C_BaseEntity *pEnt = data.GetEntity();
		if ( pEnt && pEnt->IsDormant() )
			return vecStart;

		// Get the attachment origin
		if ( !pRenderable->GetAttachment( iAttachment, vecStart, vecAngles ) )
		{
			DevMsg( "GetTracerOrigin: Couldn't find attachment %d on model %s\n", iAttachment, 
				modelinfo->GetModelName( pRenderable->GetModel() ) );
		}
	}

	return vecStart;
}

void RailgunBeamCallback( const CEffectData &data )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	// Grab the data
	Vector vecStart = GetRailgunBeamStart( data );
	FX_ASW_RGEffect(vecStart, data.m_vOrigin);
}

DECLARE_CLIENT_EFFECT( RailgunBeam, RailgunBeamCallback );
