//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "client_pch.h"

#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <KeyValues.h>

#include <vgui_controls/BuildGroup.h>
#include <vgui_controls/Tooltip.h>
#include <vgui_controls/TextImage.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui_controls/FileOpenDialog.h>
#include <vgui_controls/ProgressBar.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/TextEntry.h>
#include <vgui/IInput.h>

#include "cl_demoactionmanager.h"

#include "filesystem.h"
#include "filesystem_engine.h"
#include "cl_demoeditorpanel.h"
#include "cl_demosmootherpanel.h"
#include "cl_demouipanel.h"
#include "iprediction.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//////////////////////////////////////////////////////////////////////////
//
// CDemoUIPanel
//
//////////////////////////////////////////////////////////////////////////

CDemoUIPanel *g_pDemoUI = NULL;

void CDemoUIPanel::InstallDemoUI( vgui::Panel *parent )
{
	if ( g_pDemoUI )
		return;	// UI already created

	g_pDemoUI = new CDemoUIPanel( parent );
	Assert( g_pDemoUI );
}

//-----------------------------------------------------------------------------
// Purpose: Basic help dialog
//-----------------------------------------------------------------------------
CDemoUIPanel::CDemoUIPanel( vgui::Panel *parent ) : vgui::Frame( parent, "DemoUIPanel")
{
	SetTitle("Demo Playback", true);

	m_pPlayPauseResume = new vgui::Button( this, "DemoPlayPauseResume", "PlayPauseResume" );
	m_pStop = new vgui::Button( this, "DemoStop", "Stop" );
	m_pLoad = new vgui::Button( this, "DemoLoad", "Load..." );
	m_pEdit = new vgui::Button( this, "DemoEdit", "Edit..." );
	m_pSmooth = new vgui::Button( this, "DemoSmooth", "Smooth..." );
	m_pDriveCamera = new vgui::ToggleButton( this, "DemoDriveCamera", "Drive..." );

	m_pGoStart = new vgui::ToggleButton( this, "DemoGoStart", "Go Start" );
	m_pGoEnd = new vgui::Button( this, "DemoGoEnd", "Go End" );
	m_pFastForward = new vgui::Button( this, "DemoFastForward", "Fast Fwd" );
	m_pFastBackward = new vgui::Button( this, "DemoFastBackward", "Fast Bwd" );
	m_pPrevFrame =  new vgui::Button( this, "DemoPrevFrame", "Prev Frame" );
	m_pNextFrame =  new vgui::Button( this, "DemoNextFrame", "Next Frame" );

	m_pCurrentDemo = new vgui::Label( this, "DemoName", "" );

	m_pProgress = new vgui::ProgressBar( this, "DemoProgress" );
	m_pProgress->SetSegmentInfo( 2, 2 );

	m_pProgressLabelFrame = new vgui::Label( this, "DemoProgressLabelFrame", "" );
	m_pProgressLabelTime = new vgui::Label( this, "DemoProgressLabelTime", "" );

	m_pSpeedScale = new vgui::Slider( this, "DemoSpeedScale" );
	// 1000 == 10x %
	m_pSpeedScale->SetRange( 0, 1000 );
	m_pSpeedScale->SetValue( 500 );
	m_pSpeedScale->AddActionSignalTarget( this );

	m_pSpeedScaleLabel = new vgui::Label( this, "SpeedScale", "" );

	m_pGo = new vgui::Button( this, "DemoGo", "Go" );
	m_pGotoTick = new vgui::TextEntry( this, "DemoGoToTick" );

	vgui::ivgui()->AddTickSignal( GetVPanel(), 0 );

	LoadControlSettings("Resource\\DemoUIPanel.res");

	SetVisible( false );
	SetSizeable( false );
	SetMoveable( true );

	m_ViewOrigin.Init();
	m_ViewAngles.Init();
	memset( m_nOldCursor, 0, sizeof( m_nOldCursor ) );
	m_bInputActive = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDemoUIPanel::~CDemoUIPanel()
{
}

void CDemoUIPanel::GetCurrentView()
{
	g_pClientSidePrediction->GetViewOrigin( m_ViewOrigin );
	g_pClientSidePrediction->GetViewAngles( m_ViewAngles );
}

bool CDemoUIPanel::IsInDriveMode()
{
	return m_pDriveCamera->IsSelected();
}

void CDemoUIPanel::GetDriveViewPoint( Vector &origin, QAngle &angle )
{
	origin = m_ViewOrigin;
	angle = m_ViewAngles;
}

void CDemoUIPanel::SetDriveViewPoint( Vector &origin, QAngle &angle )
{
	m_ViewOrigin = origin;
	m_ViewAngles = angle;
}

void CDemoUIPanel::OnTick()
{
	BaseClass::OnTick();

	if ( !IsVisible() )
		return;

	char curtime[32];
	char totaltime[32];
	int curtick = 0;
	int	totalticks = 0;
	float fProgress = 0.0f;

	bool bIsPlaying = demoplayer->IsPlayingBack();

	// enable/disable all playback control buttons
	m_pPlayPauseResume->SetEnabled( bIsPlaying );
	m_pStop->SetEnabled( bIsPlaying );
	m_pNextFrame->SetEnabled( bIsPlaying );
	m_pFastForward->SetEnabled( bIsPlaying );
	m_pGoStart->SetEnabled( bIsPlaying );
	m_pGoEnd->SetEnabled( bIsPlaying );
	m_pGo->SetEnabled( bIsPlaying );

	bool bEnabled = bIsPlaying && false;
		
	// if player can go back in time
	m_pFastBackward->SetEnabled( bEnabled );
	m_pPrevFrame->SetEnabled( bEnabled );
	
	// set filename text
	m_pCurrentDemo->SetText( demoaction->GetCurrentDemoFile() );
	bool bHasDemoFile = demoaction->GetCurrentDemoFile()[0] != 0;

	// set play button text
	if (  bIsPlaying )
	{
		m_pPlayPauseResume->SetText( demoplayer->IsPlaybackPaused() ? "Resume" : "Pause" );
	}
	else
	{
		if ( bHasDemoFile )
		{
			m_pPlayPauseResume->SetText( "Play" );
			m_pPlayPauseResume->SetEnabled( true );
		}
	}

	if ( bIsPlaying )
	{
		curtick = demoplayer->GetPlaybackTick();
		totalticks = demoplayer->GetTotalTicks();
		
		fProgress = (float)curtick/(float)totalticks;
		fProgress = clamp( fProgress, 0.0f, 1.0f );
	}
		
	m_pProgress->SetProgress( fProgress );
	m_pProgressLabelFrame->SetText( va( "Tick: %i / %i", curtick, totalticks ) );

	Q_strncpy( curtime, COM_FormatSeconds( host_state.interval_per_tick * curtick ), 32 );
	Q_strncpy( totaltime, COM_FormatSeconds( host_state.interval_per_tick * totalticks ), 32 );
	m_pProgressLabelTime->SetText( va( "Time: %s / %s", curtime, totaltime ) );

	m_pFastForward->SetEnabled( demoplayer->IsPlayingBack() && !demoplayer->IsPlaybackPaused() );

	float fScale = demoplayer->GetPlaybackTimeScale();

	SetPlaybackScale( fScale );  // set slider

	m_pSpeedScaleLabel->SetText( va( "%.1f %%", fScale * 100.0f ) );
}

// Command issued
void CDemoUIPanel::OnCommand(const char *command)
{
	if ( !Q_strcasecmp( command, "stop" ) )
	{
		Cbuf_AddText( "disconnect\n" );
	}
	else if ( !Q_strcasecmp( command, "play" ) )
	{
		if ( !demoplayer->IsPlayingBack() )
		{
			char cmd[ 256 ];
			Q_snprintf( cmd, sizeof( cmd ), "playdemo %s\n", demoaction->GetCurrentDemoFile() );

			Cbuf_AddText( cmd );
		}
		else
		{
			Cbuf_AddText( !demoplayer->IsPlaybackPaused() ? "demo_pause\n" : "demo_resume\n" );
		}
	}
	else if ( !Q_strcasecmp( command, "load" ) )
	{
		OnLoad();
	}
	else if ( !Q_strcasecmp( command, "reload" ) )
	{
		Cbuf_AddText( "demo_gototick 0 0 1\n" );
	}
	else if ( !Q_strcasecmp( command, "edit" ) )
	{
		OnEdit();
	}
	else if ( !Q_strcasecmp( command, "smooth" ) )
	{
		OnSmooth();
	}
	else if ( !Q_strcasecmp( command, "nextframe" ) )
	{
		demoplayer->SkipToTick( 1, true, true );
	}
	else if ( !Q_strcasecmp( command, "gototick" ) )
	{
		char tick[ 32 ];
		m_pGotoTick->GetText( tick, sizeof( tick ) );

		char cmd[256];
		Q_snprintf( cmd, sizeof(cmd), "demo_gototick %s 0 1\n", tick );

		Cbuf_AddText( cmd );
		
		// demoplayer->PausePlayback( -1 );
		// demoplayer->SkipToTick( Q_atoi(tick), false );
	}
	else if ( !Q_strcasecmp( command, "drive" ) )
	{
		GetCurrentView();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void CDemoUIPanel::OnMessage(const KeyValues *params, VPANEL fromPanel)
{
	BaseClass::OnMessage( params, fromPanel );

	if ( !Q_strcmp( "SliderMoved", params->GetName() ) )
	{
		demoplayer->SetPlaybackTimeScale( GetPlaybackScale() );
	}
}

void CDemoUIPanel::OnEdit()
{
	if ( m_hDemoEditor != 0 )
	{
		m_hDemoEditor->SetVisible( true );
		m_hDemoEditor->MoveToFront();
		m_hDemoEditor->OnVDMChanged();
		return;
	}

	m_hDemoEditor = new CDemoEditorPanel( this );
}

void CDemoUIPanel::OnSmooth()
{
	if ( m_hDemoSmoother != 0 )
	{
		m_hDemoSmoother->SetVisible( true );
		m_hDemoSmoother->MoveToFront();
		m_hDemoSmoother->OnVDMChanged();
		return;
	}

	m_hDemoSmoother = new CDemoSmootherPanel( this );
}

void CDemoUIPanel::OnLoad()
{
	if ( !m_hFileOpenDialog.Get() )
	{
		m_hFileOpenDialog = new FileOpenDialog( this, "Choose .dem file", true );
		if ( m_hFileOpenDialog != 0 )
		{
			m_hFileOpenDialog->AddFilter("*.dem", "Demo Files (*.dem)", true);
		}
	}
	if ( m_hFileOpenDialog )
	{
		char startPath[ MAX_PATH ];
		Q_strncpy( startPath, com_gamedir, sizeof( startPath ) );
		Q_FixSlashes( startPath );
		m_hFileOpenDialog->SetStartDirectory( startPath );
		m_hFileOpenDialog->DoModal( false );
	}
}

void CDemoUIPanel::OnFileSelected( char const *fullpath )
{
	if ( !fullpath || !fullpath[ 0 ] )
		return;

	char relativepath[ 512 ];
	g_pFileSystem->FullPathToRelativePath( fullpath, relativepath, sizeof( relativepath ) );

	char ext[ 10 ];
	Q_ExtractFileExtension( relativepath, ext, sizeof( ext ) );

	if ( Q_strcasecmp( ext, "dem" ) )
	{
		return;
	}

	// It's a dem file
	Cbuf_AddText( va( "playdemo %s\n", relativepath ) );
	Cbuf_AddText( "demopauseafterinit\n" );

	if ( m_hFileOpenDialog != 0 )
	{
		m_hFileOpenDialog->MarkForDeletion();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoUIPanel::OnVDMChanged( void )
{
	if ( m_hDemoEditor != 0 )
	{
		m_hDemoEditor->OnVDMChanged();
	}
	if ( m_hDemoSmoother != 0 )
	{
		m_hDemoSmoother->OnVDMChanged();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDemoUIPanel::IsHoldingFastForward( void )
{
	if ( !m_pFastForward->IsVisible() )
		return false;

	if ( !m_pFastForward->IsEnabled() )
		return false;

	return m_pFastForward->IsSelected();
}

float CDemoUIPanel::GetPlaybackScale( void )
{
	float scale = 1.0f;
	float curval = (float)m_pSpeedScale->GetValue() ;

	if ( curval <= 500.0f )
	{
		scale = curval / 500.0f;
	}
	else
	{
		scale = 1.0f + ( curval - 500.0f ) / 100.0f;
	}
	return scale;
}

void CDemoUIPanel::SetPlaybackScale( float scale )
{
	if ( scale <= 0 )
	{
		m_pSpeedScale->SetValue( 0 ) ;
	}
	else if ( scale <= 1.0f )
	{
		m_pSpeedScale->SetValue( scale * 500.0f ) ;
	}
	else
	{
		m_pSpeedScale->SetValue( (scale - 1.0f) * 100.0f + 500.0f ) ;
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frame - 
//			elapsed - 
//			info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDemoUIPanel::OverrideView( democmdinfo_t& info, int tick )
{
	if ( IsInDriveMode() )
	{
		// manual camera override, overrides anyting
		HandleInput( vgui::input()->IsMouseDown( MOUSE_LEFT ) );
				
		info.viewOrigin = m_ViewOrigin;
		info.viewAngles = m_ViewAngles;
		info.localViewAngles = m_ViewAngles;

		return true;
	}

	if ( m_hDemoSmoother != 0 )
	{
		// demo smoother override
		if ( m_hDemoSmoother->OverrideView( info, tick ) )
		{
			m_ViewOrigin = info.GetViewOrigin();
			m_ViewAngles = info.GetViewAngles();
			return true;
		}
	}

	m_ViewOrigin = info.GetViewOrigin();
	m_ViewAngles = info.GetViewAngles();
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frame - 
//			elapsed - 
//			smoothing - 
//-----------------------------------------------------------------------------
void CDemoUIPanel::DrawDebuggingInfo()
{
	if ( m_hDemoSmoother != 0 )
	{
		m_hDemoSmoother->DrawDebuggingInfo( 1, 1 ); // MOTODO
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoUIPanel::HandleInput( bool active )
{
	if ( m_bInputActive ^ active )
	{
		if ( m_bInputActive && !active )
		{
			// Restore mouse
			vgui::input()->SetCursorPos( m_nOldCursor[0], m_nOldCursor[1] );
		}
		else
		{
			GetCurrentView();
			vgui::input()->GetCursorPos( m_nOldCursor[0], m_nOldCursor[1] );
		}
	}

	if ( active )
	{
		float f = 0.0f;
		float s = 0.0f;
		float u = 0.0f;

		bool shiftdown = vgui::input()->IsKeyDown( KEY_LSHIFT ) || vgui::input()->IsKeyDown( KEY_RSHIFT );
		float movespeed = shiftdown ? 40.0f : 400.0f;

		if ( vgui::input()->IsKeyDown( KEY_W ) )
		{
			f = movespeed * host_frametime;
		}
		if ( vgui::input()->IsKeyDown( KEY_S ) )
		{
			f = -movespeed * host_frametime;
		}
		if ( vgui::input()->IsKeyDown( KEY_A ) )
		{
			s = -movespeed * host_frametime;
		}
		if ( vgui::input()->IsKeyDown( KEY_D ) )
		{
			s = movespeed * host_frametime;
		}
		if ( vgui::input()->IsKeyDown( KEY_X ) )
		{
			u = movespeed * host_frametime;
		}
		if ( vgui::input()->IsKeyDown( KEY_Z ) )
		{
			u = -movespeed * host_frametime;
		}

		int mx, my;
		int dx, dy;

		vgui::input()->GetCursorPos( mx, my );

		dx = mx - m_nOldCursor[0];
		dy = my - m_nOldCursor[1];

		vgui::input()->SetCursorPos( m_nOldCursor[0], m_nOldCursor[1] );

		// Convert to pitch/yaw

		float pitch = (float)dy * 0.22f;
		float yaw = -(float)dx * 0.22;

		// Apply mouse
		m_ViewAngles.x += pitch;

		m_ViewAngles.x = clamp( m_ViewAngles.x, -89.0f, 89.0f );

		m_ViewAngles.y += yaw;
		if ( m_ViewAngles.y > 180.0f )
		{
			m_ViewAngles.y -= 360.0f;
		}
		else if ( m_ViewAngles.y < -180.0f )
		{
			m_ViewAngles.y += 360.0f;
		}

		// Now apply forward, side, up

		Vector fwd, side, up;

		AngleVectors( m_ViewAngles, &fwd, &side, &up );

		m_ViewOrigin += fwd * f;
		m_ViewOrigin += side * s;
		m_ViewOrigin += up * u;
	}

	m_bInputActive = active;
}

void DemoUI_f()
{
	if ( !g_pDemoUI )
		return;

	if ( g_pDemoUI->IsVisible() )
	{
		g_pDemoUI->Close();
	}
	else
	{
		g_pDemoUI->Activate();
	}
}

static ConCommand demoui( "demoui", DemoUI_f, "Show/hide the demo player UI.", FCVAR_DONTRECORD );






//////////////////////////////////////////////////////////////////////////
//
// CDemoUIPanel2
//
//////////////////////////////////////////////////////////////////////////

CDemoUIPanel2 *g_pDemoUI2 = NULL;

void CDemoUIPanel2::Install( vgui::Panel *pParentBkgnd, vgui::Panel *pParentFgnd, bool bPutToForeground )
{
	if ( g_pDemoUI2 )
		return;	// UI already created

	g_pDemoUI2 = new CDemoUIPanel2( pParentBkgnd, pParentFgnd, bPutToForeground );
	Assert( g_pDemoUI2 );
}

//-----------------------------------------------------------------------------
// Purpose: Basic help dialog
//-----------------------------------------------------------------------------
CDemoUIPanel2::CDemoUIPanel2( vgui::Panel *pParentBkgnd, vgui::Panel *pParentFgnd, bool bPutToForeground ) :
	vgui::Frame( bPutToForeground ? pParentFgnd : pParentBkgnd, "DemoUIPanel2")
{
	m_arrParents[0] = pParentBkgnd;
	m_arrParents[1] = pParentFgnd;
	m_bIsInForeground = bPutToForeground;

	SetTitle("Demo Playback - ", true);

	m_pPlayPauseResume = new vgui::Button( this, "DemoPlayPauseResume", "PlayPauseResume" );
	m_pStop = new vgui::Button( this, "DemoStop", "Stop" );
	m_pLoad = new vgui::Button( this, "DemoLoad", "Load..." );

	m_pGoStart = new vgui::ToggleButton( this, "DemoGoStart", "Go Start" );
	m_pGoEnd = new vgui::Button( this, "DemoGoEnd", "Go End" );
	m_pFastForward = new vgui::Button( this, "DemoFastForward", "Fast Fwd" );
	m_pFastBackward = new vgui::Button( this, "DemoFastBackward", "Fast Bwd" );
	m_pPrevFrame =  new vgui::Button( this, "DemoPrevFrame", "Prev Frame" );
	m_pNextFrame =  new vgui::Button( this, "DemoNextFrame", "Next Frame" );

	m_pProgress = new vgui::Slider( this, "DemoProgress" );
	m_pProgress->SetRange( 0, 0 );
	m_pProgress->SetValue( 0, false );
	m_pProgress->AddActionSignalTarget( this );
	m_pProgress->SetDragOnRepositionNob( true );

	m_pProgressLabelFrame = new vgui::Label( this, "DemoProgressLabelFrame", "" );
	m_pProgressLabelTime = new vgui::Label( this, "DemoProgressLabelTime", "" );

	m_pSpeedScale = new vgui::Slider( this, "DemoSpeedScale" );
	// 1000 == 10x %
	m_pSpeedScale->SetRange( 0, 1000 );
	m_pSpeedScale->SetValue( 500 );
	m_pSpeedScale->AddActionSignalTarget( this );
	m_pSpeedScale->SetDragOnRepositionNob( true );

	m_pSpeedScaleLabel = new vgui::Label( this, "SpeedScale", "" );

	vgui::ivgui()->AddTickSignal( GetVPanel(), 0 );

	LoadControlSettings("Resource\\DemoUIPanel2.res");

	SetVisible( false );
	SetSizeable( false );
	SetMoveable( true );

	memset( m_nOldCursor, 0, sizeof( m_nOldCursor ) );
	m_bInputActive = false;
}

CDemoUIPanel2::~CDemoUIPanel2()
{
}

bool CDemoUIPanel2::IsInDriveMode()
{
	return false;
}

void CDemoUIPanel2::GetDriveViewPoint( Vector &origin, QAngle &angle )
{
	NULL;
}

void CDemoUIPanel2::SetDriveViewPoint( Vector &origin, QAngle &angle )
{
	NULL;
}

void CDemoUIPanel2::OnTick()
{
	BaseClass::OnTick();

	if ( !IsVisible() )
		return;

	char curtime[32];
	char totaltime[32];
	int curtick = 0;
	int	totalticks = 0;
	float fProgress = 0.0f;

	bool bIsPlaying = demoplayer->IsPlayingBack();

	// enable/disable all playback control buttons
	m_pPlayPauseResume->SetEnabled( bIsPlaying );
	m_pStop->SetEnabled( bIsPlaying );
	m_pNextFrame->SetEnabled( bIsPlaying );
	m_pFastForward->SetEnabled( bIsPlaying );
	m_pGoStart->SetEnabled( bIsPlaying );
	m_pGoEnd->SetEnabled( bIsPlaying );

	bool bEnabled = bIsPlaying && false;

	// if player can go back in time
	m_pFastBackward->SetEnabled( bEnabled );
	m_pPrevFrame->SetEnabled( bEnabled );

	// set filename text
	SetTitle( va( "Demo Playback - %s", demoaction->GetCurrentDemoFile() ), true );
	bool bHasDemoFile = demoaction->GetCurrentDemoFile()[0] != 0;

	// set play button text
	if (  bIsPlaying )
	{
		m_pPlayPauseResume->SetText( demoplayer->IsPlaybackPaused() ? "Resume" : "Pause" );
	}
	else
	{
		if ( bHasDemoFile )
		{
			m_pPlayPauseResume->SetText( "Play" );
			m_pPlayPauseResume->SetEnabled( true );
		}
	}

	if ( bIsPlaying )
	{
		curtick = demoplayer->GetPlaybackTick();
		totalticks = demoplayer->GetTotalTicks();

		fProgress = (float)curtick/(float)totalticks;
		fProgress = clamp( fProgress, 0.0f, 1.0f );
	}

	if ( !m_pProgress->IsDragged() )
	{
		m_pProgress->SetRange( 0, max( totalticks, 0 ) );
		m_pProgress->SetValue( min( max( curtick, 0 ), totalticks ), false );
		m_pProgressLabelFrame->SetText( va( "Tick: %i / %i", curtick, totalticks ) );
	}
	else
	{
		m_pProgressLabelFrame->SetText( va( "Tick: %i / %i", m_pProgress->GetValue(), totalticks ) );
	}

	// Color in red when dragging back
	m_pProgressLabelFrame->SetFgColor( ( m_pProgress->GetValue() < curtick ) ? Color( 255, 0, 0, 255 ) : m_pProgressLabelTime->GetFgColor() );

	Q_strncpy( curtime, COM_FormatSeconds( host_state.interval_per_tick * curtick ), 32 );
	Q_strncpy( totaltime, COM_FormatSeconds( host_state.interval_per_tick * totalticks ), 32 );
	m_pProgressLabelTime->SetText( va( "Time: %s / %s", curtime, totaltime ) );

	m_pFastForward->SetEnabled( demoplayer->IsPlayingBack() && !demoplayer->IsPlaybackPaused() );

	float fScale = demoplayer->GetPlaybackTimeScale();

	SetPlaybackScale( fScale );  // set slider

	m_pSpeedScaleLabel->SetText( va( "%.1f %%", fScale * 100.0f ) );
}

// Command issued
void CDemoUIPanel2::OnCommand(const char *command)
{
	if ( !Q_strcasecmp( command, "stop" ) )
	{
		Cbuf_AddText( "disconnect\n" );
	}
	else if ( !Q_strcasecmp( command, "play" ) )
	{
		if ( !demoplayer->IsPlayingBack() )
		{
			demoplayer->StartPlayback( demoaction->GetCurrentDemoFile(), false );
		}
		else
		{
			demoplayer->IsPlaybackPaused() ? demoplayer->ResumePlayback() : demoplayer->PausePlayback( -1.f );
		}
	}
	else if ( !Q_strcasecmp( command, "load" ) )
	{
		OnLoad();
	}
	else if ( !Q_strcasecmp( command, "reload" ) )
	{
		Cbuf_AddText( "demo_gototick 0 0 1\n" );
	}
	else if ( !Q_strcasecmp( command, "nextframe" ) )
	{
		Cbuf_AddText( "demo_gototick 1 1 1\n" );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void CDemoUIPanel2::OnMessage(const KeyValues *params, VPANEL fromPanel)
{
	BaseClass::OnMessage( params, fromPanel );

	//
	// Speed scale
	//
	if ( fromPanel == m_pSpeedScale->GetVPanel() )
	{
		if ( !Q_strcmp( "SliderMoved", params->GetName() ) )
		{
			demoplayer->SetPlaybackTimeScale( GetPlaybackScale() );
		}
	}

	//
	// Demo position
	//
	if ( fromPanel == m_pProgress->GetVPanel() )
	{
		if ( !Q_strcmp( "SliderDragStart", params->GetName() ) )
		{
			// Pause the demo when starting dragging around
			if ( demoplayer->IsPlayingBack() && !demoplayer->IsPlaybackPaused() )
			{
				demoplayer->PausePlayback( -1.f );
			}
		}

		if ( !Q_strcmp( "SliderDragEnd", params->GetName() ) )
		{
			int iNewTickPos = m_pProgress->GetValue();
			int iDemoCurrentTickPos = demoplayer->GetPlaybackTick();
			
			if ( iNewTickPos != iDemoCurrentTickPos )
				Cbuf_AddText( va( "demo_gototick %d 0 1\n", iNewTickPos ) );
		}

		if ( !Q_strcmp( "SliderMoved", params->GetName() ) )
		{
			NULL;
		}
	}
}

void CDemoUIPanel2::OnLoad()
{
	if ( !m_hFileOpenDialog.Get() )
	{
		m_hFileOpenDialog = new FileOpenDialog( this, "Choose .dem file", true );
		if ( m_hFileOpenDialog != 0 )
		{
			m_hFileOpenDialog->AddFilter("*.dem", "Demo Files (*.dem)", true);
		}
	}
	if ( m_hFileOpenDialog )
	{
		char startPath[ MAX_PATH ];
		Q_strncpy( startPath, com_gamedir, sizeof( startPath ) );
		Q_FixSlashes( startPath );
		m_hFileOpenDialog->SetStartDirectory( startPath );
		m_hFileOpenDialog->DoModal( false );
	}
}

void CDemoUIPanel2::OnFileSelected( char const *fullpath )
{
	if ( !fullpath || !fullpath[ 0 ] )
		return;

	char relativepath[ 512 ];
	g_pFileSystem->FullPathToRelativePath( fullpath, relativepath, sizeof( relativepath ) );

	char ext[ 10 ];
	Q_ExtractFileExtension( relativepath, ext, sizeof( ext ) );

	if ( Q_strcasecmp( ext, "dem" ) )
	{
		return;
	}

	// It's a dem file
	Cbuf_AddText( va( "playdemo %s\n", relativepath ) );
	Cbuf_AddText( "demopauseafterinit\n" );

	if ( m_hFileOpenDialog != 0 )
	{
		m_hFileOpenDialog->MarkForDeletion();
	}
}

void CDemoUIPanel2::OnVDMChanged( void )
{
	NULL;
}

bool CDemoUIPanel2::IsHoldingFastForward( void )
{
	if ( !m_pFastForward->IsVisible() )
		return false;

	if ( !m_pFastForward->IsEnabled() )
		return false;

	return m_pFastForward->IsSelected();
}

float CDemoUIPanel2::GetPlaybackScale( void )
{
	float scale = 1.0f;
	float curval = (float)m_pSpeedScale->GetValue() ;

	if ( curval <= 500.0f )
	{
		scale = curval / 500.0f;
	}
	else
	{
		scale = 1.0f + ( curval - 500.0f ) / 100.0f;
	}
	return scale;
}

void CDemoUIPanel2::SetPlaybackScale( float scale )
{
	if ( scale <= 0 )
	{
		m_pSpeedScale->SetValue( 0 ) ;
	}
	else if ( scale <= 1.0f )
	{
		m_pSpeedScale->SetValue( scale * 500.0f ) ;
	}
	else
	{
		m_pSpeedScale->SetValue( (scale - 1.0f) * 100.0f + 500.0f ) ;
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frame - 
//			elapsed - 
//			info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDemoUIPanel2::OverrideView( democmdinfo_t& info, int tick )
{
	return false;
}

void CDemoUIPanel2::DrawDebuggingInfo()
{
	NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoUIPanel2::HandleInput( bool active )
{
	if ( m_bInputActive ^ active )
	{
		if ( m_bInputActive && !active )
		{
			// Restore mouse
			vgui::input()->SetCursorPos( m_nOldCursor[0], m_nOldCursor[1] );
		}
		else
		{
			vgui::input()->GetCursorPos( m_nOldCursor[0], m_nOldCursor[1] );
		}
	}

	m_bInputActive = active;
}

void CDemoUIPanel2::MakePanelForeground( bool bPutToForeground )
{
	m_bIsInForeground = bPutToForeground;

	SetKeyBoardInputEnabled( m_bIsInForeground );
	SetMouseInputEnabled( m_bIsInForeground );

	SetParent( m_arrParents[ !!m_bIsInForeground ] );

	if ( m_bIsInForeground )
	{
		g_pDemoUI2->Activate();
	}
}

void DemoUI2_f()
{
	if ( !g_pDemoUI2 )
		return;

	if ( g_pDemoUI2->IsVisible() )
	{
		g_pDemoUI2->Close();
	}
	else
	{
		g_pDemoUI2->MakePanelForeground( true );
	}
}

void DemoUI2_on()
{
	if ( !g_pDemoUI2 )
		return;

	g_pDemoUI2->MakePanelForeground( true );
}

void DemoUI2_off()
{
	if ( !g_pDemoUI2 )
		return;

	g_pDemoUI2->MakePanelForeground( false );
}

static ConCommand demoui2( "demoui2", DemoUI2_f, "Show/hide the advanced demo player UI (demoui2).", FCVAR_DONTRECORD );
static ConCommand demoui2_on( "+demoui2", DemoUI2_on, "Bring the advanced demo player UI (demoui2) to foreground.", FCVAR_DONTRECORD );
static ConCommand demoui2_off( "-demoui2", DemoUI2_off, "Send the advanced demo player UI (demoui2) to background.", FCVAR_DONTRECORD );

