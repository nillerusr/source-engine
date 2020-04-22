//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "cl_screenshotmanager.h"
#include "replay/screenshot.h"
#include "replaysystem.h"
#include "cl_replaymanager.h"
#include "replay/replayutils.h"
#include "replay/ireplayscreenshotsystem.h"
#include "gametrace.h"
#include "icliententity.h"
#include "imageutils.h"
#include "filesystem.h"
#include "fmtstr.h"
#include "vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

#define SCREENSHOTS_SUBDIR	"screenshots"

//----------------------------------------------------------------------------------------

CScreenshotManager::CScreenshotManager()
:	m_flLastScreenshotTime( 0.0f ),
	m_hScreenshotReplay( REPLAY_HANDLE_INVALID )
{
}

CScreenshotManager::~CScreenshotManager()
{
}

bool CScreenshotManager::Init()
{
	// Create thumbnails directory
	CFmtStr fmtThumbnailsPath(
		"%s%cmaterials%cvgui%creplay%cthumbnails",
		g_pEngine->GetGameDir(), CORRECT_PATH_SEPARATOR, CORRECT_PATH_SEPARATOR,
		CORRECT_PATH_SEPARATOR, CORRECT_PATH_SEPARATOR
	);
	g_pFullFileSystem->CreateDirHierarchy( fmtThumbnailsPath.Access() );

	// Compute all possible resolutions if first time we're running this function
	for ( int iAspect = 0; iAspect < 3; ++iAspect )
	{
		for ( int iRes = 0; iRes < 2; ++iRes )
		{
			int nWidth = (int)FastPow2( 9 + iRes );
			m_aScreenshotWidths[ iAspect ][ iRes ] = nWidth;
		}
	}

	// Set current screenshot dims to 0 - screenshot cache will be created first time we see
	// proper screen dimensions
	m_nPrevScreenDims[0] = 0;
	m_nPrevScreenDims[1] = 0;

	// Initialize screenshot taker in client
	g_pClient->GetReplayScreenshotSystem()->UpdateReplayScreenshotCache();

	return true;
}

float CScreenshotManager::GetNextThinkTime() const
{
	return 0.0f;
}

void CScreenshotManager::Think()
{
	VPROF_BUDGET( "CScreenshotManager::Think", VPROF_BUDGETGROUP_REPLAY );

	//
	// NOTE:DoCaptureScreenshot() gets called from CReplaySystem::CL_Render()
	//

	IReplayScreenshotSystem *pScreenshotSystem = g_pClient->GetReplayScreenshotSystem();	Assert( pScreenshotSystem );

	// Check to see if screen resolution changed, and if so, update the client-side screenshot cache.
	int nScreenWidth = g_pEngineClient->GetScreenWidth();
	int nScreenHeight = g_pEngineClient->GetScreenHeight();
	if ( !CL_GetMovieManager()->IsRendering() && ( m_nPrevScreenDims[0] != nScreenWidth || m_nPrevScreenDims[1] != nScreenHeight ) )
	{
		if ( m_nPrevScreenDims[0] != 0 )	// If this is not the first update
		{
			pScreenshotSystem->UpdateReplayScreenshotCache();
		}

		m_nPrevScreenDims[0] = nScreenWidth;
		m_nPrevScreenDims[1] = nScreenHeight;
	}
}

bool CScreenshotManager::ShouldCaptureScreenshot()
{
	// Record a screenshot if its been setup
	return ( m_flScreenshotCaptureTime >= 0.0f && m_flScreenshotCaptureTime <= g_pEngine->GetHostTime() );
}

void CScreenshotManager::CaptureScreenshot( CaptureScreenshotParams_t& params )
{
	extern ConVar replay_enableeventbasedscreenshots;
	if ( !replay_enableeventbasedscreenshots.GetBool() && !params.m_bPrimary )
		return;

	// Schedule screenshot
	m_flScreenshotCaptureTime = g_pEngine->GetHostTime() + params.m_flDelay;

	// Cache parameters for when we take the screenshot
	V_memcpy( &m_screenshotParams, &params, sizeof( params ) );
}

void CScreenshotManager::DoCaptureScreenshot()
{
	// Reset screenshot capture schedule time, even if we don't end up actually taking the screenshot
	m_flScreenshotCaptureTime = -1.0f;

	// Make sure we're in-game
	if ( !g_pEngineClient->IsConnected() )
		return;

	// Get a pointer to the replay
	CReplay *pReplay = ::GetReplay( m_hScreenshotReplay );
	if ( !pReplay )
	{
		AssertMsg( 0, "Failed to take screenshot!\n" );
		return;
	}

	// Max # of screenshots already taken?
	extern ConVar replay_maxscreenshotsperreplay;
	int nScreenshotLimit = replay_maxscreenshotsperreplay.GetInt();
	if ( nScreenshotLimit && pReplay->GetScreenshotCount() >= nScreenshotLimit )
		return;

	// If not enough time has passed since the last screenshot was taken, get out
	extern ConVar replay_mintimebetweenscreenshots;
	if ( !m_screenshotParams.m_bIgnoreMinTimeBetweenScreenshots &&
		 ( g_pEngine->GetHostTime() - m_flLastScreenshotTime < replay_mintimebetweenscreenshots.GetInt() ) )
		return;

	// Update last screenshot taken time
	m_flLastScreenshotTime = g_pEngine->GetHostTime();

	// Setup screenshot base filename as <.dem base filename>_<N>
	char szBaseFilename[ MAX_OSPATH ];
	char szIdealBaseFilename[ MAX_OSPATH ];
	V_strcpy_safe( szIdealBaseFilename, CL_GetRecordingSessionManager()->m_ServerRecordingState.m_strSessionName.Get() );
	Replay_GetFirstAvailableFilename( szBaseFilename, sizeof( szBaseFilename ), szIdealBaseFilename, ".vtf",
		"materials\\vgui\\replay\\thumbnails", pReplay->GetScreenshotCount() );

	// Remove extension
	int i = V_strlen( szBaseFilename ) - 1;
	while ( i >= 0 )
	{
		if ( szBaseFilename[ i ] == '.' )
			break;
		--i;
	}
	szBaseFilename[ i ] = '\0';

	// Get destination file
	char szScreenshotPath[ MAX_OSPATH ];
	V_snprintf( szScreenshotPath, sizeof( szScreenshotPath ), "materials\\vgui\\replay\\thumbnails\\%s.vtf", szBaseFilename );

	// Make sure we're using the correct path separator
	V_FixSlashes( szScreenshotPath );

	// Setup screenshot dimensions
	int nScreenshotDims[2];
	GetUnpaddedScreenshotSize( nScreenshotDims[0], nScreenshotDims[1] );

	// Setup parameters for screenshot
	WriteReplayScreenshotParams_t params;
	V_memset( &params, 0, sizeof( params ) );

	// Setup the camera
	Vector origin;
	QAngle angles;
	if ( m_screenshotParams.m_nEntity > 0 )
	{
		IClientEntity *pEntity = entitylist->GetClientEntity( m_screenshotParams.m_nEntity );
		if ( pEntity )
		{
			// Clip the camera position if any world geometry is in the way
			trace_t trace;
			Ray_t ray;
			CTraceFilterWorldAndPropsOnly traceFilter;
			Vector vStartPos = pEntity->GetAbsOrigin();
			ray.Init( vStartPos, pEntity->GetAbsOrigin() + m_screenshotParams.m_posCamera );
			g_pEngineTraceClient->TraceRay( ray, MASK_PLAYERSOLID, &traceFilter, &trace );

			// Setup world position and angles for camera
			origin = trace.endpos;

			if ( trace.DidHit() )
			{
				float d = 5;	// The distance to push in if we 
				Vector dir = trace.endpos - vStartPos;
				VectorNormalize( dir );
				origin -= Vector( d * dir.x, d * dir.y, d * dir.z );
			}

			// Use the new camera origin
			params.m_pOrigin = &origin;

			// Use angles too if appropriate
			if ( m_screenshotParams.m_bUseCameraAngles )
			{
				angles = m_screenshotParams.m_angCamera;
				params.m_pAngles = &angles;
			}
		}
	}	

	// Write the screenshot to disk
	params.m_nWidth = nScreenshotDims[0];
	params.m_nHeight = nScreenshotDims[1];
	params.m_pFilename = szScreenshotPath;
	g_pClient->GetReplayScreenshotSystem()->WriteReplayScreenshot( params );

	// Write a generic VMT
	char szVTFFullPath[ MAX_OSPATH ];
	V_snprintf( szVTFFullPath, sizeof( szVTFFullPath ), "%s\\materials\\vgui\\replay\\thumbnails\\%s.vtf", g_pEngine->GetGameDir(), szBaseFilename );
	V_FixSlashes( szVTFFullPath );
	ImgUtl_WriteGenericVMT( szVTFFullPath, "vgui/replay/thumbnails" );

	// Create the new screenshot info
	pReplay->AddScreenshot( nScreenshotDims[0], nScreenshotDims[1], szBaseFilename );
}

void CScreenshotManager::DeleteScreenshotsForReplay( CReplay *pReplay )
{
	char szFilename[ MAX_OSPATH ];
	for ( int i = 0; i < pReplay->GetScreenshotCount(); ++i )
	{
		const CReplayScreenshot *pScreenshot = pReplay->GetScreenshot( i );

		// Delete the VGUI thumbnail VTF
		V_snprintf( szFilename, sizeof( szFilename ), "materials\\vgui\\replay\\thumbnails\\%s.vtf", pScreenshot->m_szBaseFilename );
		V_FixSlashes( szFilename );
		g_pFullFileSystem->RemoveFile( szFilename );

		// Delete the VGUI thumbnail VMT
		V_snprintf( szFilename, sizeof( szFilename ), "materials\\vgui\\replay\\thumbnails\\%s.vmt", pScreenshot->m_szBaseFilename );
		V_FixSlashes( szFilename );
		g_pFullFileSystem->RemoveFile( szFilename );
	}
}
	
void CScreenshotManager::GetUnpaddedScreenshotSize( int &nOutWidth, int &nOutHeight )
{
	// Figure out the proper screenshot size to use based on the aspect ratio
	int nScreenWidth = g_pEngineClient->GetScreenWidth();
	int nScreenHeight = g_pEngineClient->GetScreenHeight();
	float flAspectRatio = (float)nScreenWidth / nScreenHeight;

	// Get the screenshot res
	extern ConVar replay_screenshotresolution;
	int iRes = clamp( replay_screenshotresolution.GetInt(), 0, 1 );

	int iAspect;
	if ( flAspectRatio == 16.0f/9 )
	{
		iAspect = 0;
	}
	else if ( flAspectRatio == 16.0f/10 )
	{
		iAspect = 1;
	}
	else
	{
		iAspect = 2;	// 4:3
	}

	static float s_flInvAspectRatios[3] = { 9.0f/16.0f, 10.0f/16, 3.0f/4 };
	nOutWidth = min( nScreenWidth, m_aScreenshotWidths[ iAspect ][ iRes ] );
	nOutHeight = m_aScreenshotWidths[ iAspect ][ iRes ] * s_flInvAspectRatios[ iAspect ];
}

void CScreenshotManager::SetScreenshotReplay( ReplayHandle_t hReplay )
{
	m_hScreenshotReplay = hReplay;
}

//----------------------------------------------------------------------------------------
