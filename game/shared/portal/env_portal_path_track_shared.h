//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A version of path_track which draws.
//
//=============================================================================//

#ifndef ENV_PORTAL_PATH_TRACK_SHARED_H
#define ENV_PORTAL_PATH_TRACK_SHARED_H
#ifdef _WIN32
#pragma once
#endif

// States for track drawing
enum
{
	PORTAL_PATH_TRACK_STATE_OFF,
	PORTAL_PATH_TRACK_STATE_INACTIVE,
	PORTAL_PATH_TRACK_STATE_ACTIVE,
	PORTAL_PATH_TRACK_STATE_COUNT
};


#ifndef CLIENT_DLL 

#include "pathtrack.h"

class CBeam;

//==============================================================
// 
//==============================================================
class CEnvPortalPathTrack : public CPathTrack
{
	DECLARE_CLASS( CEnvPortalPathTrack, CPathTrack );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

public:
	CEnvPortalPathTrack();
	~CEnvPortalPathTrack();
	virtual void Precache();
	void	Spawn( void );
	void	Activate( void );

	void	InitTrackFX();
	void	ShutDownTrackFX();
	void	InitEndpointFX();
	void	ShutDownEndpointFX();

	void	InputActivateTrack( inputdata_t &inputdata );
	void	InputActivateEndpoint( inputdata_t &inputdata );

	void	InputDeactivateTrack( inputdata_t &inputdata );
	void	InputDeactivateEndpoint( inputdata_t &inputdata );

	void	ActivateTrackFX ( void );		//Activate all of the track's beams (at least the ones that are flagged to display)
	void	ActivateEndpointFX ( void );	//Activate all of the endpoint's glowy bits that are flagged to display

	void	DeactivateTrackFX ( void );		//Activate all of the track's beams (at least the ones that are flagged to display)
	void	DeactivateEndpointFX ( void );	//Activate all of the endpoint's glowy bits that are flagged to display 

protected:
	CNetworkVar( bool, m_bTrackActive );
	CNetworkVar( bool, m_bEndpointActive );
//	CNetworkVar( float, m_fScaleEndpoint );				// Scale of the endpoint for this beam
//	CNetworkVar( float, m_fScaleTrack );				// Scale of the track effect
//	CNetworkVar( float, m_fFadeOutEndpoint );			// Scale of the track effect
//	CNetworkVar( float, m_fFadeInEndpoint );			// Scale of the track effect
	CNetworkVar( int, m_nState );						// particle emmision state

	COutputEvent m_OnActivatedEndpoint;

	CBeam		*m_pBeam;								// Pointer to look at a cbeam object for the track fx

};

#endif // CLIENT_DLL

#endif //ENV_PORTAL_PATH_TRACK_SHARED_H