//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client Side Effects for the env_portal_path_track.
//
//=============================================================================//

#include "cbase.h"
#include "env_portal_path_track_shared.h"
#include "particles_simple.h"
#include "particles_attractor.h"

class C_EnvPortalPathTrack : public C_BaseEntity
{
	DECLARE_CLASS( C_EnvPortalPathTrack, C_BaseEntity );
	DECLARE_CLIENTCLASS();

public:
	void			OnDataChanged( DataUpdateType_t updateType );
	RenderGroup_t	GetRenderGroup( void );

	void			ClientThink( void );
	void			NotifyShouldTransmit( ShouldTransmitState_t state );

	void			UpdateParticles_Inactive( void );
	void			UpdateParticles_Active( void );
private:	

	float			m_flScale;
	int				m_nState;
	float			m_flDuration;
	float			m_flStartTime;

	bool			m_bTrackActive;
	bool			m_bEndpointActive;

	bool			SetupEmitters( void );						// Init our particle emitters

	CSmartPtr<CSimpleEmitter>		m_pSimpleEmitter;			// particle system, emits particles
	CSmartPtr<CParticleAttractor>	m_pAttractorEmitter;		// particle system, attracts particles
};

IMPLEMENT_CLIENTCLASS_DT( C_EnvPortalPathTrack, DT_EnvPortalPathTrack, CEnvPortalPathTrack )

	RecvPropBool( RECVINFO(m_bTrackActive) ),
	RecvPropBool( RECVINFO(m_bEndpointActive) ),
	RecvPropInt( RECVINFO(m_nState) ),

END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
// Output : RenderGroup_t
//-----------------------------------------------------------------------------
RenderGroup_t C_EnvPortalPathTrack::GetRenderGroup( void )
{
	return RENDER_GROUP_TRANSLUCENT_ENTITY;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void C_EnvPortalPathTrack::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_EnvPortalPathTrack::SetupEmitters( void )
{
	// Setup the basic core emitter
	if ( m_pSimpleEmitter.IsValid() == false )
	{
		m_pSimpleEmitter = CSimpleEmitter::Create( "portaltracktrainendpoint" );

		if ( m_pSimpleEmitter.IsValid() == false )
			return false;
	}

	// Setup the attractor emitter
	if ( m_pAttractorEmitter.IsValid() == false )
	{
		m_pAttractorEmitter = CParticleAttractor::Create( GetAbsOrigin(), "portaltracktrainendpointattractor" );

		if ( m_pAttractorEmitter.IsValid() == false )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_EnvPortalPathTrack::ClientThink( void )
{
	if ( gpGlobals->frametime <= 0.0f )
		return;

	switch( m_nState )
	{
	case PORTAL_PATH_TRACK_STATE_OFF:
		break;

	case PORTAL_PATH_TRACK_STATE_INACTIVE:
		UpdateParticles_Inactive();
		break;

	case PORTAL_PATH_TRACK_STATE_ACTIVE:
		UpdateParticles_Active();
		break;
	}
}

void C_EnvPortalPathTrack::UpdateParticles_Inactive( void )
{
}

void C_EnvPortalPathTrack::UpdateParticles_Active ( void )
{
	// Emitters must be valid
	if ( SetupEmitters() == false )
		return;

	// Reset our sort origin
	m_pSimpleEmitter->SetSortOrigin( GetAbsOrigin() );

	SimpleParticle *sParticle;

	// Do the charging particles
	m_pAttractorEmitter->SetAttractorOrigin( GetAbsOrigin() );

	Vector forward, right, up;
	AngleVectors( GetAbsAngles(), &forward, &right, &up );

	Vector	offset;
	float	dist;

	int numParticles = floor( 4.0f );

	for ( int i = 0; i < numParticles; i++ )
	{
		dist = random->RandomFloat( 4.0f, 64.0f );

		offset = forward * dist;

		dist = RemapValClamped( dist, 4.0f, 64.0f, 6.0f, 1.0f );
		offset += right * random->RandomFloat( -4.0f * dist, 4.0f * dist );
		offset += up * random->RandomFloat( -4.0f * dist, 4.0f * dist );

		offset += GetAbsOrigin();

		sParticle = (SimpleParticle *) m_pAttractorEmitter->AddParticle( sizeof(SimpleParticle), m_pAttractorEmitter->GetPMaterial( "effects/strider_muzzle" ), offset );

		if ( sParticle == NULL )
			return;
		
		sParticle->m_vecVelocity	= Vector(0,0,8);
		sParticle->m_flDieTime		= 0.5f;
		sParticle->m_flLifetime		= 0.0f;

		sParticle->m_flRoll			= Helper_RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= 0.0f;

		float alpha = 255;

		sParticle->m_uchColor[0]	= alpha;
		sParticle->m_uchColor[1]	= alpha;
		sParticle->m_uchColor[2]	= alpha;
		sParticle->m_uchStartAlpha	= alpha;
		sParticle->m_uchEndAlpha	= 0;

		sParticle->m_uchStartSize	= random->RandomFloat( 1, 2 );
		sParticle->m_uchEndSize		= 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_EnvPortalPathTrack::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	BaseClass::NotifyShouldTransmit( state );

	// Turn off
	if ( state == SHOULDTRANSMIT_END )
	{
		SetNextClientThink( CLIENT_THINK_NEVER );
	}

	// Turn on
	if ( state == SHOULDTRANSMIT_START )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}
