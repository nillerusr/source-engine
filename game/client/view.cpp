//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "cbase.h"
#include "view.h"
#include "iviewrender.h"
#include "iviewrender_beams.h"
#include "view_shared.h"
#include "ivieweffects.h"
#include "iinput.h"
#include "iclientmode.h"
#include "prediction.h"
#include "viewrender.h"
#include "c_te_legacytempents.h"
#include "cl_mat_stub.h"
#include "tier0/vprof.h"
#include "IClientVehicle.h"
#include "engine/IEngineTrace.h"
#include "mathlib/vmatrix.h"
#include "rendertexture.h"
#include "c_world.h"
#include <KeyValues.h>
#include "igameevents.h"
#include "smoke_fog_overlay.h"
#include "bitmap/tgawriter.h"
#include "hltvcamera.h"
#if defined( REPLAY_ENABLED )
#include "replaycamera.h"
#endif
#include "input.h"
#include "filesystem.h"
#include "materialsystem/itexture.h"
#include "toolframework_client.h"
#include "tier0/icommandline.h"
#include "IEngineVGui.h"
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>
#include "ScreenSpaceEffects.h"
#include "vgui_int.h"
#include "engine/sndinfo.h"
#ifdef GAMEUI_UISYSTEM2_ENABLED
#include "gameui.h"
#endif
#ifdef GAMEUI_EMBEDDED

#if defined( SWARM_DLL )
#include "swarm/gameui/swarm/basemodpanel.h"
#else
#error "GAMEUI_EMBEDDED"
#endif
#endif
#ifdef INFESTED_DLL
#include "c_asw_marine.h"
#endif

#if defined( HL2_CLIENT_DLL ) || defined( INFESTED_DLL )
#define USE_MONITORS
#endif



	
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
		  
void ToolFramework_AdjustEngineViewport( int& x, int& y, int& width, int& height );
bool ToolFramework_SetupEngineView( Vector &origin, QAngle &angles, float &fov );
bool ToolFramework_SetupEngineMicrophone( Vector &origin, QAngle &angles );


extern ConVar default_fov;
extern bool g_bRenderingScreenshot;

#if !defined( _X360 )
#define SAVEGAME_SCREENSHOT_WIDTH	180
#define SAVEGAME_SCREENSHOT_HEIGHT	100
#else
#define SAVEGAME_SCREENSHOT_WIDTH	128
#define SAVEGAME_SCREENSHOT_HEIGHT	128
#endif

#ifndef _XBOX
extern ConVar sensitivity;
#endif

ConVar zoom_sensitivity_ratio( "zoom_sensitivity_ratio", "1.0", 0, "Additional mouse sensitivity scale factor applied when FOV is zoomed in." );

// Each MOD implements GetViewRenderInstance() and provides either a default object or a subclassed object!!!
IViewRender *view = NULL;	// set in cldll_client_init.cpp if no mod creates their own

#if _DEBUG
bool g_bRenderingCameraView = false;
#endif

static Vector g_vecRenderOrigin[ MAX_SPLITSCREEN_PLAYERS ];
static QAngle g_vecRenderAngles[ MAX_SPLITSCREEN_PLAYERS ];
static Vector g_vecPrevRenderOrigin[ MAX_SPLITSCREEN_PLAYERS ];	// Last frame's render origin
static QAngle g_vecPrevRenderAngles[ MAX_SPLITSCREEN_PLAYERS ]; // Last frame's render angles
static Vector g_vecVForward[ MAX_SPLITSCREEN_PLAYERS ], g_vecVRight[ MAX_SPLITSCREEN_PLAYERS ], g_vecVUp[ MAX_SPLITSCREEN_PLAYERS ];
static VMatrix g_matCamInverse[ MAX_SPLITSCREEN_PLAYERS ];

extern ConVar cl_forwardspeed;

static ConVar v_centermove( "v_centermove", "0.15");
static ConVar v_centerspeed( "v_centerspeed","500" );


// 54 degrees approximates a 35mm camera - we determined that this makes the viewmodels
// and motions look the most natural.
ConVar v_viewmodel_fov( "viewmodel_fov", "54", FCVAR_CHEAT );


static ConVar mat_viewportscale( "mat_viewportscale", "1.0", FCVAR_CHEAT, "Scale down the main viewport (to reduce GPU impact on CPU profiling)",
								  true, (1.0f / 640.0f), true, 1.0f );
ConVar cl_leveloverview( "cl_leveloverview", "0", FCVAR_CHEAT );

ConVar r_mapextents( "r_mapextents", "16384", FCVAR_CHEAT, 
						   "Set the max dimension for the map.  This determines the far clipping plane" );

static ConVar cl_camera_follow_bone_index( "cl_camera_follow_bone_index"  , "-2", FCVAR_CHEAT, "Index of the bone to follow.  -2 == disabled.  -1 == root bone.  0+ is bone index." );
Vector g_cameraFollowPos;

// UNDONE: Delete this or move to the material system?
ConVar	gl_clear( "gl_clear","0");
ConVar	gl_clear_randomcolor( "gl_clear_randomcolor", "0", FCVAR_CHEAT, "Clear the back buffer to random colors every frame. Helps spot open seams in geometry." );

static ConVar r_farz( "r_farz", "-1", FCVAR_CHEAT, "Override the far clipping plane. -1 means to use the value in env_fog_controller." );
static ConVar cl_demoviewoverride( "cl_demoviewoverride", "0", 0, "Override view during demo playback" );

static Vector s_DemoView;
static QAngle s_DemoAngle;

static void CalcDemoViewOverride( Vector &origin, QAngle &angles )
{
	engine->SetViewAngles( s_DemoAngle );

	input->ExtraMouseSample( gpGlobals->absoluteframetime, true );

	engine->GetViewAngles( s_DemoAngle );

	Vector forward, right, up;

	AngleVectors( s_DemoAngle, &forward, &right, &up );

	float speed = gpGlobals->absoluteframetime * cl_demoviewoverride.GetFloat() * 320;
	
	s_DemoView += speed * input->KeyState (&in_forward) * forward  ;
	s_DemoView -= speed * input->KeyState (&in_back) * forward ;

	s_DemoView += speed * input->KeyState (&in_moveright) * right ;
	s_DemoView -= speed * input->KeyState (&in_moveleft) * right ;

	origin = s_DemoView;
	angles = s_DemoAngle;
}


CViewSetup &CViewRender::GetView(int nSlot /*= -1*/)
{
	Assert( m_bAllowViewAccess );
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		return m_UserView[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
	}
	return m_UserView[ nSlot ];
}

const CViewSetup &CViewRender::GetView(int nSlot /*= -1*/) const
{
	Assert( m_bAllowViewAccess );
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		return m_UserView[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
	}
	return m_UserView[ nSlot ];
}

//-----------------------------------------------------------------------------
// Accessors to return the main view (where the player's looking)
//-----------------------------------------------------------------------------
const Vector &MainViewOrigin( int nSlot )
{
	return g_vecRenderOrigin[ nSlot ];
}

const QAngle &MainViewAngles( int nSlot )
{
	return g_vecRenderAngles[ nSlot ];
}

const Vector &MainViewForward( int nSlot )
{
	return g_vecVForward[ nSlot ];
}

const Vector &MainViewRight( int nSlot )
{
	return g_vecVRight[ nSlot ];
}

const Vector &MainViewUp( int nSlot )
{
	return g_vecVUp[ nSlot ];
}

const VMatrix &MainWorldToViewMatrix( int nSlot )
{
	return g_matCamInverse[ nSlot ];
}

const Vector &PrevMainViewOrigin( int nSlot )
{
	return g_vecPrevRenderOrigin[ nSlot ];
}

const QAngle &PrevMainViewAngles( int nSlot )
{
	return g_vecPrevRenderAngles[ nSlot ];
}

//-----------------------------------------------------------------------------
// Compute the world->camera transform
//-----------------------------------------------------------------------------
void ComputeCameraVariables( const Vector &vecOrigin, const QAngle &vecAngles, 
	Vector *pVecForward, Vector *pVecRight, Vector *pVecUp, VMatrix *pMatCamInverse )
{
	// Compute view bases
	AngleVectors( vecAngles, pVecForward, pVecRight, pVecUp );

	for (int i = 0; i < 3; ++i)
	{
		(*pMatCamInverse)[0][i] = (*pVecRight)[i];	
		(*pMatCamInverse)[1][i] = (*pVecUp)[i];	
		(*pMatCamInverse)[2][i] = -(*pVecForward)[i];	
		(*pMatCamInverse)[3][i] = 0.0F;
	}
	(*pMatCamInverse)[0][3] = -DotProduct( *pVecRight, vecOrigin );
	(*pMatCamInverse)[1][3] = -DotProduct( *pVecUp, vecOrigin );
	(*pMatCamInverse)[2][3] =  DotProduct( *pVecForward, vecOrigin );
	(*pMatCamInverse)[3][3] = 1.0F;
}


bool R_CullSphere(
	VPlane const *pPlanes,
	int nPlanes,
	Vector const *pCenter,
	float radius)
{
	for(int i=0; i < nPlanes; i++)
		if(pPlanes[i].DistTo(*pCenter) < -radius)
			return true;
	
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void StartPitchDrift( void )
{
	view->StartPitchDrift();
}

static ConCommand centerview( "centerview", StartPitchDrift );

//-----------------------------------------------------------------------------
// Purpose: Initializes all view systems
//-----------------------------------------------------------------------------
void CViewRender::Init( void )
{
	memset( &m_PitchDrift, 0, sizeof( m_PitchDrift ) );

	m_bDrawOverlay = false;

	m_pDrawEntities		= cvar->FindVar( "r_drawentities" );
	m_pDrawBrushModels	= cvar->FindVar( "r_drawbrushmodels" );

	beams->InitBeams();
	tempents->Init();

	m_TranslucentSingleColor.Init( "debug/debugtranslucentsinglecolor", TEXTURE_GROUP_OTHER );
	m_ModulateSingleColor.Init( "engine/modulatesinglecolor", TEXTURE_GROUP_OTHER );
	m_WhiteMaterial.Init( "vgui/white", TEXTURE_GROUP_OTHER );
	
	extern CMaterialReference g_material_WriteZ;
	g_material_WriteZ.Init( "engine/writez", TEXTURE_GROUP_OTHER );

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS ; ++i )
	{
		g_vecRenderOrigin[ i ].Init();
		g_vecRenderAngles[ i ].Init();
		g_vecPrevRenderOrigin[ i ].Init();
		g_vecPrevRenderAngles[ i ].Init();
		g_vecVForward[ i ].Init();
		g_vecVRight[ i ].Init();
		g_vecVUp[ i ].Init();
		g_matCamInverse[ i ].Identity();
	}
}

CMaterialReference &CViewRender::GetWhite()
{
	return m_WhiteMaterial;
}

//-----------------------------------------------------------------------------
// Purpose: Called once per level change
//-----------------------------------------------------------------------------
void CViewRender::LevelInit( void )
{
	beams->ClearBeams();
	tempents->Clear();

	m_BuildWorldListsNumber = 0;
	m_BuildRenderableListsNumber = 0;

	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		m_FreezeParams[ i ].m_bTakeFreezeFrame = false;
		m_FreezeParams[ i ].m_flFreezeFrameUntil = 0;
	}

	// Clear our overlay materials
	m_ScreenOverlayMaterial.Init( NULL );

	// Init all IScreenSpaceEffects
	g_pScreenSpaceEffects->InitScreenSpaceEffects( );

	InitFadeData();
}

//-----------------------------------------------------------------------------
// Purpose: Called once per level change
//-----------------------------------------------------------------------------
void CViewRender::LevelShutdown( void )
{
	g_pScreenSpaceEffects->ShutdownScreenSpaceEffects( );
}

//-----------------------------------------------------------------------------
// Purpose: Called at shutdown
//-----------------------------------------------------------------------------
void CViewRender::Shutdown( void )
{

	m_TranslucentSingleColor.Shutdown();
	m_ModulateSingleColor.Shutdown();
	m_WhiteMaterial.Shutdown();

	beams->ShutdownBeams();
	tempents->Shutdown();
}


//-----------------------------------------------------------------------------
// Returns the worldlists build number
//-----------------------------------------------------------------------------

int CViewRender::BuildWorldListsNumber( void ) const
{
	return m_BuildWorldListsNumber;
}

//-----------------------------------------------------------------------------
// Purpose: Start moving pitch toward ideal
//-----------------------------------------------------------------------------
void CViewRender::StartPitchDrift (void)
{
	if ( m_PitchDrift.laststop == gpGlobals->curtime )
	{
		// Something else is blocking the drift.
		return;		
	}

	if ( m_PitchDrift.nodrift || !m_PitchDrift.pitchvel )
	{
		m_PitchDrift.pitchvel	= v_centerspeed.GetFloat();
		m_PitchDrift.nodrift	= false;
		m_PitchDrift.driftmove	= 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CViewRender::StopPitchDrift (void)
{
	m_PitchDrift.laststop	= gpGlobals->curtime;
	m_PitchDrift.nodrift	= true;
	m_PitchDrift.pitchvel	= 0;
}

//-----------------------------------------------------------------------------
// Purpose: Moves the client pitch angle towards cl.idealpitch sent by the server.
// If the user is adjusting pitch manually, either with lookup/lookdown,
//   mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.
//-----------------------------------------------------------------------------
void CViewRender::DriftPitch (void)
{
	float		delta, move;

	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

#if defined( REPLAY_ENABLED )
	if ( g_bEngineIsHLTV || engine->IsReplay() || ( player->GetGroundEntity() == NULL ) || engine->IsPlayingDemo() )
#else
	if ( g_bEngineIsHLTV || ( player->GetGroundEntity() == NULL ) || engine->IsPlayingDemo() )
#endif
	{
		m_PitchDrift.driftmove = 0;
		m_PitchDrift.pitchvel = 0;
		return;
	}

	// Don't count small mouse motion
	if ( m_PitchDrift.nodrift )
	{
		if ( fabs( input->GetLastForwardMove() ) < cl_forwardspeed.GetFloat() )
		{
			m_PitchDrift.driftmove = 0;
		}
		else
		{
			m_PitchDrift.driftmove += gpGlobals->frametime;
		}
	
		if ( m_PitchDrift.driftmove > v_centermove.GetFloat() )
		{
			StartPitchDrift ();
		}
		return;
	}
	
	// How far off are we
	delta = prediction->GetIdealPitch( player->GetSplitScreenPlayerSlot() ) - player->GetAbsAngles()[ PITCH ];
	if ( !delta )
	{
		m_PitchDrift.pitchvel = 0;
		return;
	}

	// Determine movement amount
	move = gpGlobals->frametime * m_PitchDrift.pitchvel;
	// Accelerate
	m_PitchDrift.pitchvel += gpGlobals->frametime * v_centerspeed.GetFloat();
	
	// Move predicted pitch appropriately
	if (delta > 0)
	{
		if ( move > delta )
		{
			m_PitchDrift.pitchvel = 0;
			move = delta;
		}
		player->SetLocalAngles( player->GetLocalAngles() + QAngle( move, 0, 0 ) );
	}
	else if ( delta < 0 )
	{
		if ( move > -delta )
		{
			m_PitchDrift.pitchvel = 0;
			move = -delta;
		}
		player->SetLocalAngles( player->GetLocalAngles() - QAngle( move, 0, 0 ) );
	}
}

// This is called by cdll_client_int to setup view model origins. This has to be done before
// simulation so entities can access attachment points on view models during simulation.
void CViewRender::OnRenderStart()
{
	VPROF_("CViewRender::OnRenderStart", 2, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0);
	IterateRemoteSplitScreenViewSlots_Push( true );
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );

		// This will fill in one of the m_UserView[ hh ] slots
		SetUpView();

		// Adjust mouse sensitivity based upon the current FOV
		C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
		if ( player )
		{
			default_fov.SetValue( player->m_iDefaultFOV );

			//Update our FOV, including any zooms going on
			int iDefaultFOV = default_fov.GetInt();
			int	localFOV	= player->GetFOV();
			int min_fov		= player->GetMinFOV();

			// Don't let it go too low
			localFOV = MAX( min_fov, localFOV );

			GetHud().m_flFOVSensitivityAdjust = 1.0f;
	#ifndef _XBOX
			if ( GetHud().m_flMouseSensitivityFactor )
			{
				GetHud().m_flMouseSensitivity = sensitivity.GetFloat() * GetHud().m_flMouseSensitivityFactor;
			}
			else
	#endif
			{
				// No override, don't use huge sensitivity
				if ( localFOV == iDefaultFOV )
				{
	#ifndef _XBOX
					// reset to saved sensitivity
					GetHud().m_flMouseSensitivity = 0;
	#endif
				}
				else
				{  
					// Set a new sensitivity that is proportional to the change from the FOV default and scaled
					//  by a separate compensating factor
					if ( iDefaultFOV == 0 )
					{
						Assert(0); // would divide by zero, something is broken with iDefatulFOV
						iDefaultFOV = 1;
					}
					GetHud().m_flFOVSensitivityAdjust = 
						((float)localFOV / (float)iDefaultFOV) * // linear fov downscale
						zoom_sensitivity_ratio.GetFloat(); // sensitivity scale factor
	#ifndef _XBOX
					GetHud().m_flMouseSensitivity = GetHud().m_flFOVSensitivityAdjust * sensitivity.GetFloat(); // regular sensitivity
	#endif
				}
			}
		}
	}

	// Setup the frustum cache for this frame.
	m_bAllowViewAccess = true;
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( iSlot )
	{
		const CViewSetup &view = GetView( iSlot );
		FrustumCache()->Add( &view, iSlot );
	}
	FrustumCache()->SetUpdated();
	m_bAllowViewAccess = false;

	IterateRemoteSplitScreenViewSlots_Pop();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : const CViewSetup
//-----------------------------------------------------------------------------
const CViewSetup *CViewRender::GetViewSetup( void ) const
{
	return &m_CurrentView;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const CViewSetup
//-----------------------------------------------------------------------------
const CViewSetup *CViewRender::GetPlayerViewSetup( int nSlot /*= -1*/ ) const
{   
	// NOTE:  This code path doesn't require m_bAllowViewAccess == true!!!
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		return &m_UserView[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
	}
	return &m_UserView[ nSlot ];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CViewRender::DisableVis( void )
{
	m_bForceNoVis = true;
}

#ifdef DBGFLAG_ASSERT
static Vector s_DbgSetupOrigin[ MAX_SPLITSCREEN_PLAYERS ];
static QAngle s_DbgSetupAngles[ MAX_SPLITSCREEN_PLAYERS ];
#endif

//-----------------------------------------------------------------------------
// Gets znear + zfar
//-----------------------------------------------------------------------------
float CViewRender::GetZNear()
{
	return VIEW_NEARZ;
}

float CViewRender::GetZFar()
{
	// Initialize view structure with default values
	float farZ;
	if ( r_farz.GetFloat() < 1 )
	{
		// Use the far Z from the map's parameters.
		farZ = r_mapextents.GetFloat() * 1.73205080757f;
		
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
		if( pPlayer && pPlayer->GetFogParams() )
		{
			if ( pPlayer->GetFogParams()->farz > 0 )
			{
				farZ = pPlayer->GetFogParams()->farz;
			}
		}
	}
	else
	{
		farZ = r_farz.GetFloat();
	}

	return farZ;
}

	
//-----------------------------------------------------------------------------
// Sets up the view parameters
//-----------------------------------------------------------------------------
void CViewRender::SetUpView()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	m_bAllowViewAccess = true;
	VPROF("CViewRender::SetUpView");
	// Initialize view structure with default values
	float farZ = GetZFar();

	CViewSetup &view = GetView();

	view.zFar				= farZ;
	view.zFarViewmodel	= farZ;
	// UNDONE: Make this farther out? 
	//  closest point of approach seems to be view center to top of crouched box
	view.zNear			= GetZNear();
	view.zNearViewmodel	= 1;
	view.fov				= default_fov.GetFloat();

	view.m_bOrtho			= false;

	// Enable spatial partition access to edicts
	partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, false );

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();

	bool bNoViewEnt = false;
	if( pPlayer == NULL )
	{
		pPlayer = GetSplitScreenViewPlayer( nSlot );
		bNoViewEnt = true;
	}

	if ( g_bEngineIsHLTV )
	{
		HLTVCamera()->CalcView( view.origin, view.angles, view.fov );
	}
#if defined( REPLAY_ENABLED )
	else if ( engine->IsReplay() )
	{
		ReplayCamera()->CalcView( view.origin, view.angles, view.fov );
	}
#endif
	else
	{
		// FIXME: Are there multiple views? If so, then what?
		// FIXME: What happens when there's no player?
		if (pPlayer)
		{
			pPlayer->CalcView( view.origin, view.angles, view.zNear, view.zFar, view.fov );

			// If we are looking through another entities eyes, then override the angles/origin for GetView()
			int viewentity = render->GetViewEntity();

			if ( !bNoViewEnt && !g_nKillCamMode && (pPlayer->entindex() != viewentity) )
			{
				C_BaseEntity *ve = cl_entitylist->GetEnt( viewentity );
				if ( ve )
				{
					VectorCopy( ve->GetAbsOrigin(), view.origin );
					VectorCopy( ve->GetAbsAngles(), view.angles );
				}
			}

			pPlayer->CalcViewModelView( view.origin, view.angles );

			// Is this the proper place for this code?
			if ( cl_camera_follow_bone_index.GetInt() >= -1 && input->CAM_IsThirdPerson() )
			{
				VectorCopy( g_cameraFollowPos, view.origin );
			}
		}

		// Even if the engine is paused need to override the view
		// for keeping the camera control during pause.
		GetClientMode()->OverrideView( &GetView() );
	}

	// give the toolsystem a chance to override the view
	ToolFramework_SetupEngineView( view.origin, view.angles, view.fov );

	if ( engine->IsPlayingDemo() )
	{
		if ( cl_demoviewoverride.GetFloat() > 0.0f )
		{
			// Retreive view angles from engine ( could have been set in IN_AdjustAngles above )
			CalcDemoViewOverride( view.origin, view.angles );
		}
		else
		{
			s_DemoView = view.origin;
			s_DemoAngle = view.angles;
		}
	}

	// Disable spatial partition access
	partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, true );

	//Find the offset our current FOV is from the default value
	float flFOVOffset = default_fov.GetFloat() - view.fov;

	//Adjust the viewmodel's FOV to move with any FOV offsets on the viewer's end
	view.fovViewmodel = GetClientMode()->GetViewModelFOV() - flFOVOffset;

	// Compute the world->main camera transform
	ComputeCameraVariables( view.origin, view.angles, 
		&g_vecVForward[ nSlot ], &g_vecVRight[ nSlot ], &g_vecVUp[ nSlot ], &g_matCamInverse[ nSlot ] );

	// set up the hearing origin...
	AudioState_t audioState;
	audioState.m_Origin = view.origin;
	audioState.m_Angles = view.angles;
	audioState.m_bIsUnderwater = pPlayer && pPlayer->AudioStateIsUnderwater( view.origin );

	ToolFramework_SetupAudioState( audioState );

	view.origin = audioState.m_Origin;
	view.angles = audioState.m_Angles;

	GetClientMode()->OverrideAudioState( &audioState );
	engine->SetAudioState( audioState );

	g_vecPrevRenderOrigin[ nSlot ] = g_vecRenderOrigin[ nSlot ];
	g_vecPrevRenderAngles[ nSlot ] = g_vecRenderAngles[ nSlot ];
	g_vecRenderOrigin[ nSlot ] = view.origin;
	g_vecRenderAngles[ nSlot ] = view.angles;

#ifdef DBGFLAG_ASSERT
	s_DbgSetupOrigin[ nSlot ] = view.origin;
	s_DbgSetupAngles[ nSlot ] = view.angles;
#endif

	m_bAllowViewAccess = false;
}

void CViewRender::WriteSaveGameScreenshotOfSize( const char *pFilename, int width, int height )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();

	g_bRenderingScreenshot = true;
	m_bAllowViewAccess = true;

	// Push back buffer on the stack with small viewport
	pRenderContext->PushRenderTargetAndViewport( NULL, 0, 0, width, height );

	// render out to the backbuffer
	CViewSetup viewSetup = GetView();
	viewSetup.x = 0;
	viewSetup.y = 0;
	viewSetup.width = width;
	viewSetup.height = height;
	viewSetup.fov = ScaleFOVByWidthRatio( GetView().fov, ( (float)width / (float)height ) / ( 4.0f / 3.0f ) );
	viewSetup.m_bRenderToSubrectOfLargerScreen = true;

	// draw out the scene
	// Don't draw the HUD or the viewmodel
	RenderView( viewSetup, viewSetup, VIEW_CLEAR_DEPTH | VIEW_CLEAR_COLOR, 0 );

	// get the data from the backbuffer and save to disk
	// bitmap bits
	unsigned char *pImage = ( unsigned char * )malloc( width * 3 * height );

	// Get Bits from the material system
	pRenderContext->ReadPixels( 0, 0, width, height, pImage, IMAGE_FORMAT_RGB888 );

	// allocate a buffer to write the tga into
	int iMaxTGASize = 1024 + (width * height * 4);
	void *pTGA = malloc( iMaxTGASize );
	CUtlBuffer buffer( pTGA, iMaxTGASize );

	if( !TGAWriter::WriteToBuffer( pImage, buffer, width, height, IMAGE_FORMAT_RGB888, IMAGE_FORMAT_RGB888 ) )
	{
		Error( "Couldn't write bitmap data snapshot.\n" );
	}
	
	free( pImage );

	// async write to disk (this will take ownership of the memory)
	char szPathedFileName[_MAX_PATH];
	Q_snprintf( szPathedFileName, sizeof(szPathedFileName), "//MOD/%s", pFilename );

	filesystem->AsyncWrite( szPathedFileName, buffer.Base(), buffer.TellPut(), true );

	// restore our previous state
	pRenderContext->PopRenderTargetAndViewport();
	
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
	
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	g_bRenderingScreenshot = false;
	m_bAllowViewAccess = false;
}

//-----------------------------------------------------------------------------
// Purpose: takes a screenshot of the save game
//-----------------------------------------------------------------------------
void CViewRender::WriteSaveGameScreenshot( const char *pFilename )
{
	WriteSaveGameScreenshotOfSize( pFilename, SAVEGAME_SCREENSHOT_WIDTH, SAVEGAME_SCREENSHOT_HEIGHT );
}


float ScaleFOVByWidthRatio( float fovDegrees, float ratio )
{
	float halfAngleRadians = fovDegrees * ( 0.5f * M_PI / 180.0f );
	float t = tan( halfAngleRadians );
	t *= ratio;
	float retDegrees = ( 180.0f / M_PI ) * atan( t );
	return retDegrees * 2.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Sets view parameters for level overview mode
// Input  : *rect - 
//-----------------------------------------------------------------------------
void CViewRender::SetUpOverView()
{
	static int oldCRC = 0;

	GetView().m_bOrtho = true;

	float aspect = (float)GetView().width/(float)GetView().height;

	int size_y = 1024.0f * cl_leveloverview.GetFloat(); // scale factor, 1024 = OVERVIEW_MAP_SIZE
	int	size_x = size_y * aspect;	// standard screen aspect 

	GetView().origin.x -= size_x / 2;
	GetView().origin.y += size_y / 2;

	GetView().m_OrthoLeft   = 0;
	GetView().m_OrthoTop    = -size_y;
	GetView().m_OrthoRight  = size_x;
	GetView().m_OrthoBottom = 0;

	GetView().angles = QAngle( 90, 90, 0 );

	// simple movement detector, show position if moved
	int newCRC = GetView().origin.x + GetView().origin.y + GetView().origin.z;
	if ( newCRC != oldCRC )
	{
		Msg( "Overview: scale %.2f, pos_x %.0f, pos_y %.0f\n", cl_leveloverview.GetFloat(),
			GetView().origin.x, GetView().origin.y );
		oldCRC = newCRC;
	}

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->ClearColor4ub( 0, 255, 0, 255 );

	// render->DrawTopView( true );
}

//-----------------------------------------------------------------------------
// Purpose: Render current view into specified rectangle
// Input  : *rect - 
//-----------------------------------------------------------------------------
ConVar ss_debug_draw_player( "ss_debug_draw_player", "-1", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
void CViewRender::Render( vrect_t *rect )
{
	VPROF_BUDGET( "CViewRender::Render", "CViewRender::Render" );

	m_bAllowViewAccess = true;

	CUtlVector< vgui::Panel * > roots;
	VGui_GetPanelList( roots );

	// Stub out the material system if necessary.
	CMatStubHandler matStub;
	engine->EngineStats_BeginFrame();

	// Assume normal vis
	m_bForceNoVis			= false;

	float flViewportScale = mat_viewportscale.GetFloat();

	vrect_t engineRect = *rect;

	// The tool framework wants to adjust the entire 3d viewport, not the per-split screen one from below
	ToolFramework_AdjustEngineViewport( engineRect.x, engineRect.y, engineRect.width, engineRect.height );

	IterateRemoteSplitScreenViewSlots_Push( true );
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );

		CViewSetup &view = GetView( hh );

		float engineAspectRatio = engine->GetScreenAspectRatio( view.width, view.height );

		Assert( s_DbgSetupOrigin[ hh ] == view.origin );
		Assert( s_DbgSetupAngles[ hh ] == view.angles );

		// Using this API gives us a chance to "inset" the 3d views as needed for splitscreen
		int insetX, insetY;
		VGui_GetEngineRenderBounds( hh, view.x, view.y, view.width, view.height, insetX, insetY );
			
		float aspectRatio = engineAspectRatio * 0.75f;	 // / (4/3)
		view.fov = ScaleFOVByWidthRatio( view.fov,  aspectRatio );
		view.fovViewmodel = ScaleFOVByWidthRatio( view.fovViewmodel, aspectRatio );

		// Let the client mode hook stuff.
		GetClientMode()->PreRender( &view );
		GetClientMode()->AdjustEngineViewport( view.x, view.y, view.width, view.height );

		view.width *= flViewportScale;
		view.height *= flViewportScale;
		if ( IsX360() )
		{
			// view must be compliant to resolve restrictions
			view.width = AlignValue( view.width, GPU_RESOLVE_ALIGNMENT );
			view.height = AlignValue( view.height, GPU_RESOLVE_ALIGNMENT );
		}

		view.m_flAspectRatio = ( engineAspectRatio > 0.0f ) ? engineAspectRatio : ( (float)view.width / (float)view.height );

		int nClearFlags = VIEW_CLEAR_DEPTH | VIEW_CLEAR_STENCIL;

		if ( gl_clear_randomcolor.GetBool() )
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->ClearColor3ub( rand()%256, rand()%256, rand()%256 );
			pRenderContext->ClearBuffers( true, false, false );
			pRenderContext->Release();
		}
		else if ( gl_clear.GetBool() )
		{
			nClearFlags |= VIEW_CLEAR_COLOR;
		}

		// Determine if we should draw view model ( client mode override )
		bool drawViewModel = GetClientMode()->ShouldDrawViewModel();
		// Apply any player specific overrides
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pPlayer )
		{
			// Override view model if necessary
			if ( !pPlayer->m_Local.m_bDrawViewmodel )
			{
				drawViewModel = false;
			}
		}

		if ( cl_leveloverview.GetFloat() > 0 )
		{
			SetUpOverView();		
			nClearFlags |= VIEW_CLEAR_COLOR;
			drawViewModel = false;
		}

		render->SetMainView( view.origin, view.angles );

		int flags = (pPlayer == NULL) ? 0 : RENDERVIEW_DRAWHUD;
		if ( drawViewModel )
		{
			flags |= RENDERVIEW_DRAWVIEWMODEL;
		}

		// This is the hook for per-split screen player views
		C_BaseEntity::PreRenderEntities( hh );

		if ( ( ss_debug_draw_player.GetInt() < 0 ) || ( hh == ss_debug_draw_player.GetInt() ) )
		{
			CViewSetup hudViewSetup;
			VGui_GetHudBounds( hh, hudViewSetup.x, hudViewSetup.y, hudViewSetup.width, hudViewSetup.height );
			RenderView( view, hudViewSetup, nClearFlags, flags );
		}

		GetClientMode()->PostRender();
	}
	IterateRemoteSplitScreenViewSlots_Pop();

	engine->EngineStats_EndFrame();

#if !defined( _X360 )
	// Stop stubbing the material system so we can see the budget panel
	matStub.End();
#endif

	// Render the new-style embedded UI
	// TODO: when embedded UI will be used for HUD, we will need it to maintain
	// a separate screen for HUD and a separate screen stack for pause menu & main menu.
	// for now only render embedded UI in pause menu & main menu
#if defined( GAMEUI_UISYSTEM2_ENABLED ) && 0
	BaseModUI::CBaseModPanel *pBaseModPanel = BaseModUI::CBaseModPanel::GetSingletonPtr();
	// render the new-style embedded UI only if base mod panel is not visible (game-hud)
	// otherwise base mod panel will render the embedded UI on top of video/productscreen
	if ( !pBaseModPanel || !pBaseModPanel->IsVisible() )
	{
		Rect_t uiViewport;
		uiViewport.x		= rect->x;
		uiViewport.y		= rect->y;
		uiViewport.width	= rect->width;
		uiViewport.height	= rect->height;
		g_pGameUIGameSystem->Render( uiViewport, gpGlobals->curtime );
	}
#endif

	// Draw all of the UI stuff "fullscreen"
	if ( true ) // For PIXEVENT
	{
		#if PIX_ENABLE
		{
			CMatRenderContextPtr pRenderContext( materials );
			PIXEVENT( pRenderContext, "VGui UI" );
		}
		#endif

		CViewSetup view2d;
		view2d.x				= rect->x;
		view2d.y				= rect->y;
		view2d.width			= rect->width;
		view2d.height			= rect->height;
		render->Push2DView( view2d, 0, NULL, GetFrustum() );
		render->VGui_Paint( PAINT_UIPANELS );
		{
			// The engine here is trying to access CurrentView() etc. which is bogus
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
			render->PopView( GetFrustum() );
		}
	}

	m_bAllowViewAccess = false;
}

static void GetPos( const CCommand &args, Vector &vecOrigin, QAngle &angles )
{
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	vecOrigin = MainViewOrigin(nSlot);
	angles = MainViewAngles(nSlot);

#ifdef INFESTED_DLL
	C_ASW_Marine *pMarine = C_ASW_Marine::GetLocalMarine();
	if ( pMarine )
	{
		vecOrigin = pMarine->GetAbsOrigin();
		angles = pMarine->GetAbsAngles();
	}
#endif
	if ( ( args.ArgC() == 2 && atoi( args[1] ) == 2 ) || FStrEq( args[0], "getpos_exact" ) )
	{
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pPlayer )
		{
			vecOrigin = pPlayer->GetAbsOrigin();
			angles = pPlayer->GetAbsAngles();
		}
	}
}

CON_COMMAND( spec_pos, "dump position and angles to the console" )
{
	Vector vecOrigin;
	QAngle angles;
	GetPos( args, vecOrigin, angles );
	Warning( "spec_goto %.1f %.1f %.1f %.1f %.1f\n", vecOrigin.x, vecOrigin.y, 
		vecOrigin.z, angles.x, angles.y );
}

CON_COMMAND( getpos, "dump position and angles to the console" )
{
	Vector vecOrigin;
	QAngle angles;
	GetPos( args, vecOrigin, angles );

	const char *pCommand1 = "setpos";
	const char *pCommand2 = "setang";
	if ( ( args.ArgC() == 2 && atoi( args[1] ) == 2 ) || FStrEq( args[0], "getpos_exact" ) )
	{
		pCommand1 = "setpos_exact";
		pCommand2 = "setang_exact";
	}

	Warning( "%s %f %f %f;", pCommand1, vecOrigin.x, vecOrigin.y, vecOrigin.z );
	Warning( "%s %f %f %f\n", pCommand2, angles.x, angles.y, angles.z );
}

ConCommand getpos_exact( "getpos_exact", getpos, "dump origin and angles to the console" );
