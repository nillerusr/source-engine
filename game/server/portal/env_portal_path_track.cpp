//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "env_portal_path_track_shared.h"
#include "beam_shared.h"

//**********( MODULE CONSTANTS )*************

#define TRACK_FX_WIDTH_ON			4.0f
#define TRACK_FX_WIDTH_OFF			4.0f
#define TRACK_FX_BRIGHTNESS_ON		100
#define TRACK_FX_BRIGHTNESS_OFF		10
#define TRACK_FX_COLOR_ON			140,235,255
#define TRACK_FX_COLOR_OFF			235,243,243
#define TRACK_FX_SCROLL				25.6f


ConVar sv_portal_pathtrack_track_width_on ( "sv_portal_pathtrack_track_width_on", "4.0", FCVAR_CHEAT );

//**********( DATA TABLE )*******************

BEGIN_DATADESC( CEnvPortalPathTrack )

	// Data members
	DEFINE_FIELD( m_bTrackActive, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bEndpointActive, FIELD_BOOLEAN ),

	// keyfield data
//	DEFINE_KEYFIELD( m_fScaleEndpoint,	FIELD_FLOAT, "End_point_scale" ),
//	DEFINE_KEYFIELD( m_fScaleTrack,	FIELD_FLOAT, "Track_beam_scale" ),
//	DEFINE_KEYFIELD( m_fFadeOutEndpoint, FIELD_FLOAT, "End_point_fadeout"),
//	DEFINE_KEYFIELD( m_fFadeInEndpoint, FIELD_FLOAT, "End_point_fadein"),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "ActivateTrackFX", InputActivateTrack ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ActivateEndPointFX", InputActivateEndpoint ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DeactivateTrackFX", InputDeactivateTrack ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DeactivateEndPointFX", InputDeactivateEndpoint ),

	// Outputs
	DEFINE_OUTPUT(m_OnActivatedEndpoint, "OnActivateFX"),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CEnvPortalPathTrack, DT_EnvPortalPathTrack )

	SendPropBool( SENDINFO(m_bTrackActive) ),
	SendPropBool( SENDINFO(m_bEndpointActive) ),
	SendPropInt	( SENDINFO(m_nState) ),

END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( env_portal_path_track, CEnvPortalPathTrack );


//*********( FUNCTION IMPLEMENTATIONS )***************

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
CEnvPortalPathTrack::CEnvPortalPathTrack()
{
	m_bTrackActive = false;
	m_bEndpointActive = false; 
//	m_fScaleEndpoint = 1.0f; 
//	m_fScaleTrack = 1.0f;
}

CEnvPortalPathTrack::~CEnvPortalPathTrack()
{
	ShutDownTrackFX();	//Make sure to deallocate the track beam and particle effects
}

//-----------------------------------------------------------------------------
// Precache: 
//-----------------------------------------------------------------------------
void CEnvPortalPathTrack::Precache()
{
	BaseClass::Precache();
	PrecacheMaterial( "effects/combinemuzzle2_dark" ); 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvPortalPathTrack::Spawn( void )
{
	Precache();
	BaseClass::Spawn();

	UTIL_SetSize( this, Vector( -8, -8, -8 ), Vector( 8, 8, 8 ) );

	// No model but we still need to force this!
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
}

void CEnvPortalPathTrack::Activate( void )
{
	BaseClass::Activate();	//Link the happy friends so I know where my next target entity is
	InitTrackFX();			//Initialize the FX for the track beam
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the track beam and it's partical effects
// Input  :  - 
//-----------------------------------------------------------------------------
void CEnvPortalPathTrack::InitTrackFX()
{
	m_pBeam = CBeam::BeamCreate( "sprites/track_beam.vmt", TRACK_FX_WIDTH_ON );		//Allocate a mr. beamy, and set his scroll sprite and width

	if ( m_pnext )
	{
		m_pBeam->PointEntInit( GetAbsOrigin(), m_pnext );							//Set up the beam to draw from its center to it's next track.
	}
	
	ActivateTrackFX();																//Set prettiness
}

//-----------------------------------------------------------------------------
// Purpose: Kills the track beam and it's partical effects
// Input  :  - 
//-----------------------------------------------------------------------------
void CEnvPortalPathTrack::ShutDownTrackFX()
{
	if ( m_pBeam )
	{
		UTIL_Remove( m_pBeam );
		m_pBeam = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the endpoint particle effects
// Input  :  - 
//-----------------------------------------------------------------------------
void CEnvPortalPathTrack::InitEndpointFX()
{
}

//-----------------------------------------------------------------------------
// Purpose: Activates the visual effects on the path track between two endpoints
// Input  :  - 
//-----------------------------------------------------------------------------
void CEnvPortalPathTrack::InputActivateTrack(inputdata_t &inputdata)
{
	ActivateTrackFX();
}

//-----------------------------------------------------------------------------
// Purpose: Activates the visual effects on the endpoint
// Input  :  - 
//-----------------------------------------------------------------------------
void CEnvPortalPathTrack::InputActivateEndpoint(inputdata_t &inputdata)
{
	ActivateEndpointFX();
}

//-----------------------------------------------------------------------------
// Purpose: Activates the visual effects on the path track between two endpoints
// Input  :  - 
//-----------------------------------------------------------------------------
void CEnvPortalPathTrack::InputDeactivateTrack(inputdata_t &inputdata)
{
	DeactivateTrackFX();
}

//-----------------------------------------------------------------------------
// Purpose: Activates the visual effects on the endpoint
// Input  :  - 
//-----------------------------------------------------------------------------
void CEnvPortalPathTrack::InputDeactivateEndpoint(inputdata_t &inputdata)
{
	DeactivateEndpointFX();
}

//-----------------------------------------------------------------------------
// Purpose: Activate all of the track's beams (at least the ones that are flagged to display)
// Input  :  - 
//-----------------------------------------------------------------------------
void CEnvPortalPathTrack::ActivateTrackFX ( void )
{
	m_pBeam->SetColor( TRACK_FX_COLOR_ON );
	m_pBeam->SetScrollRate( (int)TRACK_FX_SCROLL );
	m_pBeam->SetBrightness( TRACK_FX_BRIGHTNESS_ON );
	m_pBeam->TurnOff();
	m_nState = (int)PORTAL_PATH_TRACK_STATE_ACTIVE;
}

//-----------------------------------------------------------------------------
// Purpose: Activate all of the track's beams (at least the ones that are flagged to display)
// Input  :  - 
//-----------------------------------------------------------------------------
void CEnvPortalPathTrack::DeactivateTrackFX ( void )
{
	m_pBeam->SetColor( TRACK_FX_COLOR_OFF );
	m_pBeam->SetScrollRate( (int)TRACK_FX_SCROLL );
	m_pBeam->SetBrightness( TRACK_FX_BRIGHTNESS_OFF );
}

//-----------------------------------------------------------------------------
// Purpose: Activate all of the endpoint's glowy bits that are flagged to display
// Input  :  - 
//-----------------------------------------------------------------------------
void CEnvPortalPathTrack::ActivateEndpointFX ( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Activate all of the endpoint's glowy bits that are flagged to display
// Input  :  - 
//-----------------------------------------------------------------------------
void CEnvPortalPathTrack::DeactivateEndpointFX ( void )
{
}