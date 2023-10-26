#include "cbase.h"
#include "asw_holoscan.h"
#include "beam_shared.h"
#include "ai_debug_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_holoscan, CASW_HoloScan );
PRECACHE_REGISTER( asw_holoscan );

BEGIN_DATADESC( CASW_HoloScan )
	DEFINE_THINKFUNC( ScanThink ),
	DEFINE_KEYFIELD( m_bEnabled,		FIELD_BOOLEAN, "enabled" ),
	DEFINE_KEYFIELD( m_bStartDisabled,	FIELD_BOOLEAN, "StartDisabled" ),

	DEFINE_INPUTFUNC( FIELD_VOID,		"Enable",		InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID,		"Disable",		InputDisable ),
END_DATADESC()

#define GROUNDTURRET_VIEWCONE 60.0f
#define GROUNDTURRET_BEAM_SPRITE "materials/swarm/effects/greenlaser1.vmt"
#define HOLOBEAMLENGTH 350.0f

void CASW_HoloScan::Spawn( void )
{
	Precache();

	BaseClass::Spawn();

	m_bEnabled = !m_bStartDisabled;

	m_flTimeNextScanPing = 0;

	SetThink( &CASW_HoloScan::ScanThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CASW_HoloScan::Precache()
{
	BaseClass::Precache();

	PrecacheModel( GROUNDTURRET_BEAM_SPRITE );
}

int CASW_HoloScan::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	return FL_EDICT_ALWAYS;
}

int CASW_HoloScan::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_FULLCHECK );
}

void CASW_HoloScan::ScanThink()
{	
	if ( m_bEnabled )
	{
		Scan();
	}
	
	SetNextThink( gpGlobals->curtime + 0.1f );		
}

void CASW_HoloScan::Scan()
{	
	if( gpGlobals->curtime >= m_flTimeNextScanPing )
	{
		//EmitSound( "NPC_FloorTurret.Ping" );
		m_flTimeNextScanPing = gpGlobals->curtime + 1.0f;
	}

	QAngle	scanAngle;
	Vector	forward;
	Vector	vecEye = GetAbsOrigin();// + m_vecLightOffset;

	// Draw the outer extents
	scanAngle = GetAbsAngles();
	scanAngle.y += (GROUNDTURRET_VIEWCONE / 2.0f);
	AngleVectors( scanAngle, &forward, NULL, NULL );
	ProjectBeam( vecEye, forward, 1, 30, 0.1 );

	scanAngle = GetAbsAngles();
	scanAngle.y -= (GROUNDTURRET_VIEWCONE / 2.0f);
	AngleVectors( scanAngle, &forward, NULL, NULL );
	ProjectBeam( vecEye, forward, 1, 30, 0.1 );

	// Draw a sweeping beam
	scanAngle = GetAbsAngles();
	scanAngle.y += (GROUNDTURRET_VIEWCONE / 2.0f) * sin( gpGlobals->curtime * 3.0f );
	
	AngleVectors( scanAngle, &forward, NULL, NULL );
	ProjectBeam( vecEye, forward, 1, 30, 0.3 );
}

void CASW_HoloScan::ProjectBeam( const Vector &vecStart, const Vector &vecDir, int width, int brightness, float duration )
{
	CBeam *pBeam;
	pBeam = CBeam::BeamCreate( GROUNDTURRET_BEAM_SPRITE, width );
	if ( !pBeam )
		return;

	trace_t tr;
	AI_TraceLine( vecStart, vecStart + vecDir * HOLOBEAMLENGTH, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
	
	pBeam->SetStartPos( tr.endpos );
	pBeam->SetEndPos( tr.startpos );
	pBeam->SetWidth( width );
	pBeam->SetEndWidth( 0.1 );
	pBeam->SetFadeLength( 16 );

	pBeam->SetBrightness( brightness );
	pBeam->SetColor( 255, 255, 255 );
	pBeam->RelinkBeam();
	pBeam->LiveForTime( duration );
}

void CASW_HoloScan::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;
}

void CASW_HoloScan::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;
}