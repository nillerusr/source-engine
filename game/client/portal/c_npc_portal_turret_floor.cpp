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


#define FLOOR_TURRET_PORTAL_EYE_ATTACHMENT 1
#define FLOOR_TURRET_PORTAL_LASER_ATTACHMENT 2
#define FLOOR_TURRET_PORTAL_LASER_RANGE 8192

#define FLOOR_TURRET_PORTAL_END_POINT_PULSE_SCALE 4.0f


class C_NPC_Portal_FloorTurret : public C_AI_BaseNPC
{
public:
	DECLARE_CLASS( C_NPC_Portal_FloorTurret, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();

	virtual ~C_NPC_Portal_FloorTurret( void );

	virtual void	Spawn( void );
	virtual void	ClientThink( void );

	bool	IsLaserOn( void ) { return m_pBeam != NULL; }
	void	LaserOff( void );
	void	LaserOn( void );
	float	LaserEndPointSize( void );

private:
	CBeam	*m_pBeam;

	bool	m_bOutOfAmmo;
	bool	m_bLaserOn;
	int		m_sLaserHaloSprite;
	float	m_fPulseOffset;

	float	m_bBeamFlickerOff;
	float	m_fBeamFlickerTime;

};


IMPLEMENT_CLIENTCLASS_DT( C_NPC_Portal_FloorTurret, DT_NPC_Portal_FloorTurret, CNPC_Portal_FloorTurret )

	RecvPropBool( RECVINFO( m_bOutOfAmmo ) ),
	RecvPropBool( RECVINFO( m_bLaserOn ) ),
	RecvPropInt( RECVINFO( m_sLaserHaloSprite ) ),

END_RECV_TABLE()


C_NPC_Portal_FloorTurret::~C_NPC_Portal_FloorTurret( void )
{
	LaserOff();
	if( m_pBeam )
		m_pBeam->Remove();
}


void C_NPC_Portal_FloorTurret::Spawn( void )
{
	SetThink( &C_NPC_Portal_FloorTurret::ClientThink );
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	m_pBeam = NULL;
	m_fPulseOffset = RandomFloat( 0.0f, 2.0f * M_PI );

	m_bBeamFlickerOff = false;
	m_fBeamFlickerTime = 0.0f;

	BaseClass::Spawn();
}

void C_NPC_Portal_FloorTurret::ClientThink( void )
{
	if ( m_bOutOfAmmo && m_fBeamFlickerTime < gpGlobals->curtime )
	{
		m_fBeamFlickerTime = gpGlobals->curtime + RandomFloat( 0.05f, 0.3f );
		m_bBeamFlickerOff = !m_bBeamFlickerOff;
	}

	if ( m_bLaserOn && !m_bBeamFlickerOff )
		LaserOn();
	else
		LaserOff();
}

void C_NPC_Portal_FloorTurret::LaserOff( void )
{
	if( m_pBeam )
	{
		m_pBeam->AddEffects( EF_NODRAW );
	}
}

void C_NPC_Portal_FloorTurret::LaserOn( void )
{
	if ( !IsBoneAccessAllowed() )
	{
		LaserOff();
		return;
	}

	Vector vecMuzzle;
	QAngle angMuzzleDir;
	GetAttachment( FLOOR_TURRET_PORTAL_LASER_ATTACHMENT, vecMuzzle, angMuzzleDir );

	Vector vecEye;
	QAngle angEyeDir;
	GetAttachment( FLOOR_TURRET_PORTAL_EYE_ATTACHMENT, vecEye, angEyeDir );

	Vector vecMuzzleDir;
	AngleVectors( angEyeDir, &vecMuzzleDir );

	if (!m_pBeam)
	{
		m_pBeam = CBeam::BeamCreate( "effects/redlaser1.vmt", 0.2 );
		m_pBeam->SetColor( 255, 32, 32 );
		m_pBeam->SetBrightness( 255 );
		m_pBeam->SetNoise( 0 );
		m_pBeam->SetWidth( IsX360() ? ( 1.5f ) : ( 0.75f ) );	// On low end TVs these lasers are very hard to see at a distance
		m_pBeam->SetEndWidth( 0 );
		m_pBeam->SetScrollRate( 0 );
		m_pBeam->SetFadeLength( 0 );
		m_pBeam->SetHaloTexture( m_sLaserHaloSprite );
		m_pBeam->SetHaloScale( 4.0f );
		m_pBeam->SetCollisionGroup( COLLISION_GROUP_NONE );
		m_pBeam->PointsInit( vecMuzzle + vecMuzzleDir, vecMuzzle );
		m_pBeam->SetBeamFlag( FBEAM_REVERSED );
		m_pBeam->SetStartEntity( this );
	}
	else
	{
		m_pBeam->RemoveEffects( EF_NODRAW );
	}

	// Trace to find an endpoint
	Vector vEndPoint;
	float fEndFraction;
	Ray_t rayPath;
	rayPath.Init( vecMuzzle, vecMuzzle + vecMuzzleDir * FLOOR_TURRET_PORTAL_LASER_RANGE );

	CTraceFilterSkipClassname traceFilter( this, "prop_energy_ball", COLLISION_GROUP_NONE );

	if ( UTIL_Portal_TraceRay_Beam( rayPath, MASK_SHOT, &traceFilter, &fEndFraction ) )
		vEndPoint = vecMuzzle + vecMuzzleDir * FLOOR_TURRET_PORTAL_LASER_RANGE;	// Trace went through portal and endpoint is unknown
	else
		vEndPoint = vecMuzzle + vecMuzzleDir * FLOOR_TURRET_PORTAL_LASER_RANGE * fEndFraction;	// Trace hit a wall

	// The beam is backwards, sort of. The endpoint is the sniper. This is
	// so that the beam can be tapered to very thin where it emits from the turret.
	m_pBeam->PointsInit( vEndPoint, vecMuzzle );

	m_pBeam->SetHaloScale( LaserEndPointSize() );
}

float C_NPC_Portal_FloorTurret::LaserEndPointSize( void )
{
	return ( ( MAX( 0.0f, sinf( gpGlobals->curtime * M_PI + m_fPulseOffset ) ) ) * FLOOR_TURRET_PORTAL_END_POINT_PULSE_SCALE + 3.0f ) * ( IsX360() ? ( 3.0f ) : ( 1.5f ) );
}