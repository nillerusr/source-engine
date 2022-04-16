//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "c_ai_basenpc.h"
#include "beam_shared.h"
#include "prop_portal_shared.h"


#define ROCKET_TURRET_LASER_ATTACHMENT 2
#define ROCKET_TURRET_LASER_RANGE 8192

#define ROCKET_TURRET_END_POINT_PULSE_SCALE 5.0f


class C_NPC_RocketTurret : public C_AI_BaseNPC
{
public:
	DECLARE_CLASS( C_NPC_RocketTurret, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();

	C_NPC_RocketTurret( void );
	virtual ~C_NPC_RocketTurret( void );

	virtual void	Spawn( void );
	virtual void	ClientThink( void );

	virtual ITraceFilter*	GetBeamTraceFilter( void );

	void	LaserOff( void );
	void	LaserOn( void );
	float	LaserEndPointSize( void );

private:
	CBeam	*m_pBeam;

	int		m_iLaserState;

	int		m_nSiteHalo;
	float	m_fPulseOffset;
	QAngle  m_vecCurrentAngles;

	CTraceFilterSkipTwoEntities		m_filterBeams;

};


IMPLEMENT_CLIENTCLASS_DT( C_NPC_RocketTurret, DT_NPC_RocketTurret, CNPC_RocketTurret )

	RecvPropInt( RECVINFO( m_iLaserState ) ),
	RecvPropInt( RECVINFO( m_nSiteHalo ) ),
	RecvPropVector( RECVINFO( m_vecCurrentAngles ) ),

END_RECV_TABLE()


C_NPC_RocketTurret::C_NPC_RocketTurret( void )
	: m_filterBeams( NULL, NULL, COLLISION_GROUP_DEBRIS )
{
	m_filterBeams.SetPassEntity( this );
	m_filterBeams.SetPassEntity2( UTIL_PlayerByIndex( 1 ) );
}

C_NPC_RocketTurret::~C_NPC_RocketTurret( void )
{
	LaserOff();
	if( m_pBeam )
		m_pBeam->Remove();
}


void C_NPC_RocketTurret::Spawn( void )
{
	SetThink( &C_NPC_RocketTurret::ClientThink );
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	m_pBeam = NULL;
	m_fPulseOffset = RandomFloat( 0.0f, 2.0f * M_PI );

	BaseClass::Spawn();
}

void C_NPC_RocketTurret::ClientThink( void )
{
	if ( m_iLaserState == 0 )
		LaserOff();
	else
		LaserOn();
}

ITraceFilter* C_NPC_RocketTurret::GetBeamTraceFilter( void )
{
	return &m_filterBeams;
}

void C_NPC_RocketTurret::LaserOff( void )
{
	if( m_pBeam )
		m_pBeam->AddEffects( EF_NODRAW );
}

void C_NPC_RocketTurret::LaserOn( void )
{
	if ( !IsBoneAccessAllowed() )
	{
		LaserOff();
		return;
	}

	Vector vecMuzzle;
	QAngle angMuzzleDir;
	GetAttachment( ROCKET_TURRET_LASER_ATTACHMENT, vecMuzzle, angMuzzleDir );

	QAngle angAimDir = m_vecCurrentAngles;
	Vector vecAimDir;
	AngleVectors ( angAimDir, &vecAimDir );

	if (!m_pBeam)
	{
		m_pBeam = CBeam::BeamCreate( "effects/bluelaser1.vmt", 0.1 );
		m_pBeam->SetHaloTexture( m_nSiteHalo );
		m_pBeam->SetColor( 100, 100, 255 );
		m_pBeam->SetBrightness( 100 );
		m_pBeam->SetNoise( 0 );
		m_pBeam->SetWidth( 1 );
		m_pBeam->SetEndWidth( 0 );
		m_pBeam->SetScrollRate( 0 );
		m_pBeam->SetFadeLength( 0 );
		m_pBeam->SetHaloScale( 16.0f );
		m_pBeam->SetCollisionGroup( COLLISION_GROUP_NONE );
		m_pBeam->SetBeamFlag( FBEAM_REVERSED );
		m_pBeam->PointsInit( vecMuzzle + vecAimDir, vecMuzzle );
		m_pBeam->SetStartEntity( this );
	}
	else
	{
		m_pBeam->RemoveEffects( EF_NODRAW );
	}

	if ( m_iLaserState == 2 )
	{
		// Beam is freaking out
		float fSize = RandomFloat( 0.5f, 5.0f );
		int	iRate = RandomInt( 4, 20 );
		m_pBeam->SetWidth( fSize );
		m_pBeam->SetScrollRate( iRate );
	}
	else
	{
		m_pBeam->SetWidth( 1 );
		m_pBeam->SetScrollRate( 0 );
	}

	// Trace to find an endpoint (so the beam draws through portals)
	Vector vEndPoint;
	float fEndFraction;
	Ray_t rayPath;
	rayPath.Init( vecMuzzle, vecMuzzle + vecAimDir * ROCKET_TURRET_LASER_RANGE );

	if ( UTIL_Portal_TraceRay_Beam( rayPath, MASK_SHOT, &m_filterBeams, &fEndFraction ) )
		vEndPoint = vecMuzzle + vecAimDir * ROCKET_TURRET_LASER_RANGE;	// Trace went through portal and endpoint is unknown
	else
		vEndPoint = vecMuzzle + vecAimDir * ROCKET_TURRET_LASER_RANGE * fEndFraction;	// Trace hit a wall

	m_pBeam->PointsInit( vEndPoint, vecMuzzle );

	m_pBeam->SetHaloScale( LaserEndPointSize() );
}

float C_NPC_RocketTurret::LaserEndPointSize( void )
{
	return ( MAX( 0.0f, sinf( gpGlobals->curtime * M_PI + m_fPulseOffset ) ) ) * ROCKET_TURRET_END_POINT_PULSE_SCALE + 1.0f;
}