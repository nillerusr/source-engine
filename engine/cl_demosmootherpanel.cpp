//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#include "client_pch.h"
#include "cl_demosmootherpanel.h"
#include <vgui_controls/Button.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Label.h>

#include <vgui_controls/Controls.h>
#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui/IVGui.h>
#include <vgui_controls/FileOpenDialog.h>
#include <vgui_controls/ProgressBar.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/MenuButton.h>
#include <vgui_controls/Menu.h>
#include <vgui_controls/TextEntry.h>
#include <vgui/IInput.h>

#include "cl_demouipanel.h"
#include "demofile/demoformat.h"
#include "cl_demoactionmanager.h"
#include "tier2/renderutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static float Ease_In( float t )
{
	float out = sqrt( t );
	return out;
}

static float Ease_Out( float t )
{
	float out = t * t;
	return out;
}

static float Ease_Both( float t )
{
	return SimpleSpline( t );
}

//-----------------------------------------------------------------------------
// Purpose: A menu button that knows how to parse cvar/command menu data from gamedir\scripts\debugmenu.txt
//-----------------------------------------------------------------------------
class CSmoothingTypeButton : public vgui::MenuButton
{
	typedef vgui::MenuButton BaseClass;

public:
	// Construction
	CSmoothingTypeButton( vgui::Panel *parent, const char *panelName, const char *text );

private:
	// Menu associated with this button
	Menu	*m_pMenu;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSmoothingTypeButton::CSmoothingTypeButton(Panel *parent, const char *panelName, const char *text)
	: BaseClass( parent, panelName, text )
{
	// Assume no menu
	m_pMenu = new Menu( this, "DemoSmootherTypeMenu" );

	m_pMenu->AddMenuItem( "Smooth Selection Angles", "smoothselectionangles", parent );
	m_pMenu->AddMenuItem( "Smooth Selection Origin", "smoothselectionorigin", parent );
	m_pMenu->AddMenuItem( "Linear Interp Angles", "smoothlinearinterpolateangles", parent );
	m_pMenu->AddMenuItem( "Linear Interp Origin", "smoothlinearinterpolateorigin", parent );
	m_pMenu->AddMenuItem( "Spline Angles", "splineangles", parent );
	m_pMenu->AddMenuItem( "Spline Origin", "splineorigin", parent );
	m_pMenu->AddMenuItem( "Look At Points", "lookatpoints", parent );
	m_pMenu->AddMenuItem( "Look At Points Spline", "lookatpointsspline", parent );
	m_pMenu->AddMenuItem( "Two Point Origin Ease Out", "origineaseout", parent );
	m_pMenu->AddMenuItem( "Two Point Origin Ease In", "origineasein", parent );
	m_pMenu->AddMenuItem( "Two Point Origin Ease In/Out", "origineaseboth", parent );
	m_pMenu->AddMenuItem( "Auto-setup keys 1/2 second", "keyshalf", parent );
	m_pMenu->AddMenuItem( "Auto-setup keys 1 second", "keys1", parent );
	m_pMenu->AddMenuItem( "Auto-setup keys 2 second", "keys2", parent );
	m_pMenu->AddMenuItem( "Auto-setup keys 4 second", "keys4", parent );
	
	m_pMenu->MakePopup();
	MenuButton::SetMenu(m_pMenu);
	SetOpenDirection(Menu::UP);
}

//-----------------------------------------------------------------------------
// Purpose: A menu button that knows how to parse cvar/command menu data from gamedir\scripts\debugmenu.txt
//-----------------------------------------------------------------------------
class CFixEdgeButton : public vgui::MenuButton
{
	typedef vgui::MenuButton BaseClass;

public:
	// Construction
	CFixEdgeButton( vgui::Panel *parent, const char *panelName, const char *text );

private:
	// Menu associated with this button
	Menu	*m_pMenu;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFixEdgeButton::CFixEdgeButton(Panel *parent, const char *panelName, const char *text)
	: BaseClass( parent, panelName, text )
{
	// Assume no menu
	m_pMenu = new Menu( this, "DemoSmootherEdgeFixType" );

	m_pMenu->AddMenuItem( "Smooth Left", "smoothleft", parent );
	m_pMenu->AddMenuItem( "Smooth Right", "smoothright", parent );
	m_pMenu->AddMenuItem( "Smooth Both", "smoothboth", parent );
	
	m_pMenu->MakePopup();
	MenuButton::SetMenu(m_pMenu);
	SetOpenDirection(Menu::UP);
}

//-----------------------------------------------------------------------------
// Purpose: Basic help dialog
//-----------------------------------------------------------------------------
CDemoSmootherPanel::CDemoSmootherPanel( vgui::Panel *parent ) : Frame( parent, "DemoSmootherPanel")
{
	int w = 440;
	int h = 300;

	SetSize( w, h );

	SetTitle("Demo Smoother", true);

	m_pType = new CSmoothingTypeButton( this, "DemoSmootherType", "Process->" );

	m_pRevert = new vgui::Button( this, "DemoSmoothRevert", "Revert" );;
	m_pOK = new vgui::Button( this, "DemoSmoothOk", "OK" );
	m_pCancel = new vgui::Button( this, "DemoSmoothCancel", "Cancel" );

	m_pSave = new vgui::Button( this, "DemoSmoothSave", "Save" );
	m_pReloadFromDisk = new vgui::Button( this, "DemoSmoothReload", "Reload" );

	m_pStartFrame = new vgui::TextEntry( this, "DemoSmoothStartFrame" );
	m_pEndFrame = new vgui::TextEntry( this, "DemoSmoothEndFrame" );

	m_pPreviewOriginal = new vgui::Button( this, "DemoSmoothPreviewOriginal", "Show Original" );
	m_pPreviewProcessed = new vgui::Button( this, "DemoSmoothPreviewProcessed", "Show Processed" );

	m_pBackOff = new vgui::CheckButton( this, "DemoSmoothBackoff", "Back off" );	
	m_pHideLegend = new vgui::CheckButton( this, "DemoSmoothHideLegend", "Hide legend" );

	m_pHideOriginal = new vgui::CheckButton( this, "DemoSmoothHideOriginal", "Hide original" );
	m_pHideProcessed = new vgui::CheckButton( this, "DemoSmoothHideProcessed", "Hide processed" );

	m_pSelectionInfo = new vgui::Label( this, "DemoSmoothSelectionInfo", "" );
	m_pShowAllSamples = new vgui::CheckButton( this, "DemoSmoothShowAll", "Show All" );	
	m_pSelectSamples = new vgui::Button( this, "DemoSmoothSelect", "Select" );

	m_pPauseResume = new vgui::Button( this, "DemoSmoothPauseResume", "Pause" );
	m_pStepForward = new vgui::Button( this, "DemoSmoothStepForward", ">>" );
	m_pStepBackward = new vgui::Button( this, "DemoSmoothStepBackward", "<<" );

	m_pRevertPoint = new vgui::Button( this, "DemoSmoothRevertPoint", "Revert Pt." );
	m_pToggleKeyFrame = new vgui::Button( this, "DemoSmoothSetKeyFrame", "Mark Keyframe" );
	m_pToggleLookTarget = new vgui::Button( this, "DemoSmoothSetLookTarget", "Mark Look Target" );

	m_pUndo = new vgui::Button( this, "DemoSmoothUndo", "Undo" );
	m_pRedo = new vgui::Button( this, "DemoSmoothRedo", "Redo" );

	m_pNextKey = new vgui::Button( this, "DemoSmoothNextKey", "+Key" );
	m_pPrevKey = new vgui::Button( this, "DemoSmoothPrevKey", "-Key" );

	m_pNextTarget = new vgui::Button( this, "DemoSmoothNextTarget", "+Target" );
	m_pPrevTarget = new vgui::Button( this, "DemoSmoothPrevTarget", "-Target" );

	m_pMoveCameraToPoint = new vgui::Button( this, "DemoSmoothCameraAtPoint", "Set View" );

	m_pFixEdges = new CFixEdgeButton( this, "DemoSmoothFixFrameButton", "Edge->" );
	m_pFixEdgeFrames = new vgui::TextEntry( this, "DemoSmoothFixFrames" );

	m_pProcessKey = new vgui::Button( this, "DemoSmoothSaveKey", "Save Key" );

	m_pGotoFrame = new vgui::TextEntry( this, "DemoSmoothGotoFrame" );
	m_pGoto = new vgui::Button( this, "DemoSmoothGoto", "Jump To" );

	//m_pCurrentDemo = new vgui::Label( this, "DemoName", "" );

	vgui::ivgui()->AddTickSignal( GetVPanel(), 0 );

	LoadControlSettings("Resource\\DemoSmootherPanel.res");

	/*
	int xpos, ypos;
	parent->GetPos( xpos, ypos );
	ypos += parent->GetTall();

	SetPos( xpos, ypos );
	*/

	OnRefresh();

	SetVisible( true );
	SetSizeable( false );
	SetMoveable( true );

	Reset();

	m_vecEyeOffset = Vector( 0, 0, 64 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDemoSmootherPanel::~CDemoSmootherPanel()
{
}

void CDemoSmootherPanel::Reset( void )
{
	ClearSmoothingInfo( m_Smoothing );

	m_bPreviewing = false;
	m_bPreviewPaused = false;
	m_bPreviewOriginal = false;
	m_iPreviewStartTick = 0;
	m_fPreviewCurrentTime = 0.0f;
	m_nPreviewLastFrame = 0;

	m_bHasSelection = false;
	memset( m_nSelection, 0, sizeof( m_nSelection ) );
	m_iSelectionTicksSpan = 0;
	
	m_bInputActive = false;
	memset( m_nOldCursor, 0, sizeof( m_nOldCursor ) );

	WipeUndo();
	WipeRedo();
	m_bRedoPending = false;
	m_nUndoLevel = 0;
	m_bDirty = false;
}


void CDemoSmootherPanel::OnTick()
{
	BaseClass::OnTick();

	m_pUndo->SetEnabled( CanUndo() );
	m_pRedo->SetEnabled( CanRedo() );

	m_pPauseResume->SetEnabled( m_bPreviewing );
	m_pStepForward->SetEnabled( m_bPreviewing );
	m_pStepBackward->SetEnabled( m_bPreviewing );

	m_pSave->SetEnabled( m_bDirty );

	demosmoothing_t *p = GetCurrent();
	if ( p )
	{
		m_pToggleKeyFrame->SetEnabled( true );
		m_pToggleLookTarget->SetEnabled( true );

		m_pToggleKeyFrame->SetText( p->samplepoint ? "Delete Key" : "Make Key" );
		m_pToggleLookTarget->SetText( p->targetpoint ? "Delete Target" : "Make Target" );

		m_pProcessKey->SetEnabled( p->samplepoint );
	}
	else
	{
		m_pToggleKeyFrame->SetEnabled( false );
		m_pToggleLookTarget->SetEnabled( false );

		m_pProcessKey->SetEnabled( false );
	}

	if ( m_bPreviewing )
	{
		m_pPauseResume->SetText( m_bPreviewPaused ? "Resume" : "Pause" );
	}

	if ( !m_Smoothing.active )
	{
		m_pSelectionInfo->SetText( "No smoothing info loaded" );
		return;
	}

	if ( !demoplayer->IsPlayingBack() )
	{
		m_pSelectionInfo->SetText( "Not playing back .dem" );
		return;
	}

	if ( !m_bHasSelection )
	{
		m_pSelectionInfo->SetText( "No selection." );
		return;
	}

	char sz[ 512 ];
	if ( m_bPreviewing )
	{
		Q_snprintf( sz, sizeof( sz ), "%.3f at tick %i (%.3f s)", 
			m_fPreviewCurrentTime,
			GetTickForFrame( m_nPreviewLastFrame ),
			TICKS_TO_TIME( m_iSelectionTicksSpan ) );
	}
	else
	{
		Q_snprintf( sz, sizeof( sz ), "%i to %i (%.3f s)", 
			m_Smoothing.smooth[ m_nSelection[ 0 ] ].frametick,
			m_Smoothing.smooth[ m_nSelection[ 1 ] ].frametick,
			TICKS_TO_TIME( m_iSelectionTicksSpan ) );
	}
	m_pSelectionInfo->SetText( sz );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDemoSmootherPanel::CanEdit()
{
	if ( !m_Smoothing.active )
		return false;

	if ( !demoplayer->IsPlayingBack() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::OnCommand(const char *command)
{
	if ( !Q_strcasecmp( command, "cancel" ) )
	{
		OnRevert();
		MarkForDeletion();
		Reset();
		OnClose();
	}
	else if ( !Q_strcasecmp( command, "close" ) )
	{
		OnSave();
		MarkForDeletion();
		Reset();
		OnClose();
	}
	else if ( !Q_strcasecmp( command, "gotoframe" ) )
	{
		OnGotoFrame();
	}
	else if ( !Q_strcasecmp( command, "undo" ) )
	{
		Undo();
	}
	else if ( !Q_strcasecmp( command, "redo" ) )
	{
		Redo();
	}
	else if ( !Q_strcasecmp( command, "revert" ) )
	{
		OnRevert();
	}
	else if ( !Q_strcasecmp( command, "original" ) )
	{
		OnPreview( true );
	}
	else if ( !Q_strcasecmp( command, "processed" ) )
	{
		OnPreview( false );
	}
	else if ( !Q_strcasecmp( command, "save" ) )
	{
		OnSave();
	}
	else if ( !Q_strcasecmp( command, "reload" ) )
	{
		OnReload();
	}
	else if ( !Q_strcasecmp( command, "select" ) )
	{
		OnSelect();
	}
	else if ( !Q_strcasecmp( command, "togglepause" ) )
	{
		OnTogglePause();
	}
	else if ( !Q_strcasecmp( command, "stepforward" ) )
	{
		OnStep( true );
	}
	else if ( !Q_strcasecmp( command, "stepbackward" ) )
	{
		OnStep( false );
	}
	else if ( !Q_strcasecmp( command, "revertpoint" ) )
	{
		OnRevertPoint();
	}
	else if ( !Q_strcasecmp( command, "keyframe" ) )
	{
		OnToggleKeyFrame();
	}
	else if ( !Q_strcasecmp( command, "looktarget" ) )
	{
		OnToggleLookTarget();
	}
	else if ( !Q_strcasecmp( command, "nextkey" ) )
	{
		OnNextKey();
	}
	else if ( !Q_strcasecmp( command, "prevkey" ) )
	{
		OnPrevKey();
	}
	else if ( !Q_strcasecmp( command, "nexttarget" ) )
	{
		OnNextTarget();
	}
	else if ( !Q_strcasecmp( command, "prevtarget" ) )
	{
		OnPrevTarget();
	}
	else if ( !Q_strcasecmp( command, "smoothselectionangles" ) )
	{
		OnSmoothSelectionAngles();
	}
	else if ( !Q_strcasecmp( command, "keyshalf" ) )
	{
		OnSetKeys( 0.5f );
	}
	else if ( !Q_strcasecmp( command, "keys1" ) )
	{
		OnSetKeys( 1.0f );
	}
	else if ( !Q_strcasecmp( command, "keys2" ) )
	{
		OnSetKeys( 2.0f );
	}
	else if ( !Q_strcasecmp( command, "keys4" ) )
	{
		OnSetKeys( 4.0f );
	}
	else if ( !Q_strcasecmp( command, "smoothselectionorigin" ) )
	{
		OnSmoothSelectionOrigin();
	}
	else if ( !Q_strcasecmp( command, "smoothlinearinterpolateangles" ) )
	{
		OnLinearInterpolateAnglesBasedOnEndpoints();
	}
	else if ( !Q_strcasecmp( command, "smoothlinearinterpolateorigin" ) )
	{
		OnLinearInterpolateOriginBasedOnEndpoints();
	}
	else if ( !Q_strcasecmp( command, "splineorigin" ) )
	{
		OnSplineSampleOrigin();
	}
	else if ( !Q_strcasecmp( command, "splineangles" ) )
	{
		OnSplineSampleAngles();
	}
	else if ( !Q_strcasecmp( command, "lookatpoints" ) )
	{
		OnLookAtPoints( false );
	}
	else if ( !Q_strcasecmp( command, "lookatpointsspline" ) )
	{
		OnLookAtPoints( true );
	}
	else if ( !Q_strcasecmp( command, "smoothleft" ) )
	{
		OnSmoothEdges( true, false );
	}
	else if ( !Q_strcasecmp( command, "smoothright" ) )
	{
		OnSmoothEdges( false, true );
	}
	else if ( !Q_strcasecmp( command, "smoothboth" ) )
	{
		OnSmoothEdges( true, true );
	}
	else if ( !Q_strcasecmp( command, "origineasein" ) )
	{
		OnOriginEaseCurve( Ease_In );
	}
	else if ( !Q_strcasecmp( command, "origineaseout" ) )
	{
		OnOriginEaseCurve( Ease_Out );
	}
	else if ( !Q_strcasecmp( command, "origineaseboth" ) )
	{
		OnOriginEaseCurve( Ease_Both );
	}
	else if ( !Q_strcasecmp( command, "processkey" ) )
	{
		OnSaveKey();
	}
	else if ( !Q_strcasecmp( command, "setview" ) )
	{
		OnSetView();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::OnSave()
{
	if ( !m_Smoothing.active )
		return;

	SaveSmoothingInfo( demoaction->GetCurrentDemoFile(), m_Smoothing );
	WipeUndo();
	m_bDirty = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::OnReload()
{
	WipeUndo();
	WipeRedo();
	LoadSmoothingInfo( demoaction->GetCurrentDemoFile(), m_Smoothing );
	m_bDirty = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::OnVDMChanged( void )
{
	if ( IsVisible() )
	{
		OnReload();
	}
	else
	{
		Reset();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::OnRevert()
{
	OnRefresh();
	if ( !m_Smoothing.active )
	{
		LoadSmoothingInfo( demoaction->GetCurrentDemoFile(), m_Smoothing );
		WipeUndo();
		WipeRedo();
	}
	else
	{
		ClearSmoothingInfo( m_Smoothing );
		WipeUndo();
		WipeRedo();
	}

	m_bDirty = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::OnRefresh()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pScheme - 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CDemoSmootherPanel::GetStartFrame()
{
	char text[ 32 ];
	m_pStartFrame->GetText( text, sizeof( text ) );
	int tick = atoi( text );
	return GetFrameForTick( tick );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CDemoSmootherPanel::GetEndFrame()
{
	char text[ 32 ];
	m_pEndFrame->GetText( text, sizeof( text ) );
	int tick = atoi( text );
	return GetFrameForTick( tick );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : original - 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::OnPreview( bool original )
{
	if ( !CanEdit() )
		return;

	if ( !m_bHasSelection )
	{
		ConMsg( "Must have smoothing selection active\n" );
		return;
	}

	m_bPreviewing = true;
	m_bPreviewPaused = false;
	m_bPreviewOriginal = original;
	SetLastFrame( false, max( 0, m_nSelection[0] - 10 ) );
	m_iPreviewStartTick = GetTickForFrame( m_nPreviewLastFrame );
	m_fPreviewCurrentTime = TICKS_TO_TIME( m_iPreviewStartTick );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frame - 
//			elapsed - 
//			info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDemoSmootherPanel::OverrideView( democmdinfo_t& info, int tick )
{
	if ( !CanEdit() )
		return false;

	if ( !demoplayer->IsPlaybackPaused() )
		return false;

	if ( m_bPreviewing )
	{
		if ( m_bPreviewPaused && GetCurrent() && GetCurrent()->samplepoint )
		{
			info.viewOrigin = GetCurrent()->vecmoved;
			info.viewAngles = GetCurrent()->angmoved;
			info.localViewAngles = info.viewAngles;

			bool back_off = m_pBackOff->IsSelected();
			if ( back_off )
			{
				Vector fwd;
				AngleVectors( info.viewAngles, &fwd, NULL, NULL );

				info.viewOrigin -= fwd * 75.0f;
			}

			return true;
		}

		// TODO:  Hook up previewing view
		if ( !m_bPreviewPaused )
		{
			m_fPreviewCurrentTime += host_frametime;
		}

		if ( GetInterpolatedViewPoint( info.viewOrigin, info.viewAngles ) )
		{
			info.localViewAngles = info.viewAngles;
			return true;
		}
		else
		{
			return false;
		}
	}

	bool back_off = m_pBackOff->IsSelected();
	if ( back_off )
	{
		int useframe = GetFrameForTick( tick );

		if ( useframe < m_Smoothing.smooth.Count() && useframe >= 0 )
		{
			demosmoothing_t	*p = &m_Smoothing.smooth[ useframe ];
			Vector fwd;
			AngleVectors( p->info.viewAngles, &fwd, NULL, NULL );

			info.viewOrigin = p->info.viewOrigin - fwd * 75.0f;
		}
	}

	return false;
}

void DrawVecForward( bool active, const Vector& origin, const QAngle& angles, int r, int g, int b )
{
	Vector fwd;
	AngleVectors( angles, &fwd, NULL, NULL );

	Vector end;
	end = origin + fwd * ( active ? 64 : 16 );

	RenderLine( origin, end, Color( r, g, b, 255 ), true );
}

void GetColorForSample( bool original, bool samplepoint, bool targetpoint, demosmoothing_t *sample, int& r, int& g, int& b )
{
	if ( samplepoint && sample->samplepoint )
	{
		r = 0;
		g = 255;
		b = 0;
		return;
	}

	if ( targetpoint && sample->targetpoint )
	{
		r = 255;
		g = 0;
		b = 0;
		return;
	}

	if ( sample->selected )
	{
		if( original )
		{
			r = 255;
			g = 200;
			b = 100;
		}
		else
		{
			r = 200;
			g = 100;
			b = 255;
		}

		if ( sample->samplepoint || sample->targetpoint )
		{
			r = 255;
			g = 255;
			b = 0;
		}

		return;
	}

	if ( original )
	{
		r = g = b = 255;
	}
	else
	{
		r = 150;
		g = 255;
		b = 100;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : origin - 
//			mins - 
//			maxs - 
//			angles - 
//			r - 
//			g - 
//			b - 
//			a - 
//-----------------------------------------------------------------------------
void Draw_Box( const Vector& origin, const Vector& mins, const Vector& maxs, const QAngle& angles, int r, int g, int b, int a )
{
	RenderBox( origin, angles, mins, maxs, Color( r, g, b, a ), false );
	RenderWireframeBox( origin, angles, mins, maxs, Color( r, g, b, a ), true );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *sample - 
//			*next - 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::DrawSmoothingSample( bool original, bool processed, int samplenumber, demosmoothing_t *sample, demosmoothing_t *next )
{
	int r, g, b;

	if ( original )
	{
		RenderLine( sample->info.viewOrigin + m_vecEyeOffset, next->info.viewOrigin + m_vecEyeOffset,
			Color( 180, 180, 180, 255 ), true );

		GetColorForSample( true, false, false, sample, r, g, b );

		DrawVecForward( false, sample->info.viewOrigin + m_vecEyeOffset, sample->info.viewAngles, r, g, b );
	}

	if ( processed && sample->info.flags != 0 )
	{
		RenderLine( sample->info.GetViewOrigin() + m_vecEyeOffset, next->info.GetViewOrigin() + m_vecEyeOffset,
			Color( 255, 255, 180, 255 ), true );

		GetColorForSample( false, false, false, sample, r, g, b );

		DrawVecForward( false, sample->info.GetViewOrigin() + m_vecEyeOffset, sample->info.GetViewAngles(), r, g, b );
	}
	if ( sample->samplepoint )
	{
		GetColorForSample( false, true, false, sample, r, g, b );
		RenderBox( sample->vecmoved + m_vecEyeOffset, sample->angmoved, Vector( -2, -2, -2 ), Vector( 2, 2, 2 ), Color( r, g, b, 127 ), false );
		DrawVecForward( false, sample->vecmoved + m_vecEyeOffset, sample->angmoved, r, g, b );
	}

	if ( sample->targetpoint )
	{
		GetColorForSample( false, false, true, sample, r, g, b );
		RenderBox( sample->vectarget, vec3_angle, Vector( -2, -2, -2 ), Vector( 2, 2, 2 ), Color( r, g, b, 127 ), false );
	}

	if ( samplenumber == m_nPreviewLastFrame + 1 )
	{
		r = 50;
		g = 100;
		b = 250;
		RenderBox( sample->info.GetViewOrigin() + m_vecEyeOffset, sample->info.GetViewAngles(), Vector( -2, -2, -2 ), Vector( 2, 2, 2 ), Color( r, g, b, 92 ), false );
	}

	if ( sample->targetpoint )
	{
		r = 200;
		g = 200;
		b = 220;

		RenderLine( sample->info.GetViewOrigin() + m_vecEyeOffset, sample->vectarget, Color( r, g, b, 255 ), true );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::DrawDebuggingInfo(  int frame, float elapsed )
{
	if ( !CanEdit() )
		return;

	if ( !IsVisible() )
		return;

	int c = m_Smoothing.smooth.Count();
	if ( c < 2 )
		return;

	int start = 0;
	int end = c - 1;

	bool showall = m_pShowAllSamples->IsSelected();
	if ( !showall )
	{
		start = max( frame - 200, 0 );
		end = min( frame + 200, c - 1 );
	}

	if ( m_bHasSelection && !showall )
	{
		start = max( m_nSelection[ 0 ] - 10, 0 );
		end = min( m_nSelection[ 1 ] + 10, c - 1 );
	}

	bool draworiginal = !m_pHideOriginal->IsSelected();
	bool drawprocessed = !m_pHideProcessed->IsSelected();
	int i;

	demosmoothing_t	*p = NULL;
	demosmoothing_t	*prev = NULL;
	for ( i = start; i < end; i++ )
	{
		p = &m_Smoothing.smooth[ i ];
		if ( prev && p )
		{
			DrawSmoothingSample( draworiginal, drawprocessed, i, prev, p );
		}
		prev = p;
	}

	Vector org;
	QAngle ang;

	if ( m_bPreviewing )
	{
		if ( GetInterpolatedOriginAndAngles( true, org, ang ) )
		{
			DrawVecForward( true, org + m_vecEyeOffset, ang, 200, 10, 50 );
		}
	}

	int useframe = frame;

	useframe = clamp( useframe, 0, c - 1 );
	if ( useframe < c )
	{
		p = &m_Smoothing.smooth[ useframe ];
		org = p->info.GetViewOrigin();
		ang = p->info.GetViewAngles();

		DrawVecForward( true, org + m_vecEyeOffset, ang, 100, 220, 250 );
		Draw_Box( org + m_vecEyeOffset, Vector( -1, -1, -1 ), Vector( 1, 1, 1 ), ang, 100, 220, 250, 127 );
	}

	DrawKeySpline();
	DrawTargetSpline();


	if ( !m_pHideLegend->IsSelected() )
	{
		DrawLegend( start, end );
	}
}

void CDemoSmootherPanel::OnSelect()
{
	if ( !CanEdit() )
		return;

	m_bHasSelection = false;
	m_iSelectionTicksSpan = 0;
	memset( m_nSelection, 0, sizeof( m_nSelection ) );

	int start, end;
	start = GetStartFrame();
	end = GetEndFrame();

	int c = m_Smoothing.smooth.Count();
	if ( c < 2 )
		return;

	start = clamp( start, 0, c - 1 );
	end = clamp( end, 0, c - 1 );

	if ( start >= end )
		return;

	m_nSelection[ 0 ] = start;
	m_nSelection[ 1 ] = end;
	m_bHasSelection = true;

	demosmoothing_t	*startsample = &m_Smoothing.smooth[ start ];
	demosmoothing_t	*endsample = &m_Smoothing.smooth[ end ];

	m_bDirty = true;
	PushUndo( "select" );

	int i = 0;
	for ( i = 0; i < c; i++ )
	{
		if ( i >= start && i <= end )
		{
			m_Smoothing.smooth[ i ].selected = true;
		}
		else
		{
			m_Smoothing.smooth[ i ].selected = false;
		}
	}
	
	PushRedo( "select" );

	m_iSelectionTicksSpan = endsample->frametick - startsample->frametick;
}

int CDemoSmootherPanel::GetFrameForTick( int tick )
{
	int count = m_Smoothing.smooth.Count();
	int last = count - 1;
	int first = m_Smoothing.m_nFirstSelectableSample;

	if ( first > last )
		return -1;

	if ( count <= 0 )
	{
		return -1; // no valid index
	}
	else if ( count == 1 )
	{
		return 0; // return the one and only frame we have
	}

	if ( tick <= m_Smoothing.smooth[ first ].frametick )
		return first;

	if ( tick >= m_Smoothing.smooth[ last ].frametick )
		return last;

	// binary search
	int middle;

	while ( true )
	{
		middle = (first+last)/2;

		int middleTick = m_Smoothing.smooth[ middle ].frametick;

		if ( tick == middleTick )
			return middle;
        		
		if ( tick > middleTick )
		{
			if ( first == middle )
				return first;

			first = middle;
		}
		else
		{
			if ( last == middle )
				return last;

			last = middle;
		}
	}


}


int CDemoSmootherPanel::GetTickForFrame( int frame )
{
	if ( !CanEdit() )
		return -1;

	int c = m_Smoothing.smooth.Count();
	if ( c < 1 )
		return -1;

	if ( frame < 0 )
		return m_Smoothing.smooth[ 0 ].frametick;

	if ( frame >= c )
		return m_Smoothing.smooth[ c - 1 ].frametick;


	return m_Smoothing.smooth[ frame ].frametick;
}

//-----------------------------------------------------------------------------
// Purpose: Interpolate Euler angles using quaternions to avoid singularities
// Input  : start - 
//			end - 
//			output - 
//			frac - 
//-----------------------------------------------------------------------------
static void InterpolateAngles( const QAngle& start, const QAngle& end, QAngle& output, float frac )
{
	Quaternion src, dest;

	// Convert to quaternions
	AngleQuaternion( start, src );
	AngleQuaternion( end, dest );

	Quaternion result;

	// Slerp
	QuaternionSlerp( src, dest, frac, result );

	// Convert to euler
	QuaternionAngles( result, output );
}

bool CDemoSmootherPanel::GetInterpolatedOriginAndAngles( bool readonly, Vector& origin, QAngle& angles )
{
	origin.Init();
	angles.Init();

	Assert( m_bPreviewing );

	// Figure out the best samples
	int startframe	= m_nPreviewLastFrame;
	int nextframe	= startframe + 1;

	float time = m_fPreviewCurrentTime;

	int c = m_Smoothing.smooth.Count();

	do
	{
		if ( startframe >= c || nextframe >= c )
		{
			if ( !readonly )
			{
				//m_bPreviewing = false;
			}
			return false;
		}

		demosmoothing_t	*startsample = &m_Smoothing.smooth[ startframe ];
		demosmoothing_t	*endsample = &m_Smoothing.smooth[ nextframe ];

		if ( nextframe >= min( m_nSelection[1] + 10, c - 1 ) )
		{
			if ( !readonly )
			{
				OnPreview( m_bPreviewOriginal );
			}
			return false;
		}

		// If large dt, then jump ahead quickly in time
		float dt = TICKS_TO_TIME( endsample->frametick - startsample->frametick );
		if ( dt > 1.0f )
		{
			startframe++;
			nextframe++;
			continue;
		}

		if ( TICKS_TO_TIME( endsample->frametick ) >= time )
		{
			// Found a spot
			dt = TICKS_TO_TIME( endsample->frametick - startsample->frametick );
			// Should never occur!!!
			if ( dt <= 0.0f )
			{
				return false;
			}

			float frac = (float)( time - TICKS_TO_TIME(startsample->frametick) ) / dt;

			frac = clamp( frac, 0.0f, 1.0f );

			// Compute render origin/angles
			Vector renderOrigin;
			QAngle renderAngles;

			if ( m_bPreviewOriginal )
			{
				VectorLerp( startsample->info.viewOrigin, endsample->info.viewOrigin, frac, renderOrigin );
				InterpolateAngles( startsample->info.viewAngles, endsample->info.viewAngles, renderAngles, frac );
			}
			else
			{
				VectorLerp( startsample->info.GetViewOrigin(), endsample->info.GetViewOrigin(), frac, renderOrigin );
				InterpolateAngles( startsample->info.GetViewAngles(), endsample->info.GetViewAngles(), renderAngles, frac );
			}

			origin = renderOrigin;
			angles = renderAngles;

			if ( !readonly )
			{
				SetLastFrame( false, startframe );
			}

			break;
		}

		startframe++;
		nextframe++;

	} while ( true );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : t - 
//-----------------------------------------------------------------------------
bool CDemoSmootherPanel::GetInterpolatedViewPoint(  Vector& origin, QAngle& angles )
{
	Assert( m_bPreviewing );

	if ( !GetInterpolatedOriginAndAngles( false, origin, angles ) )
		return false;

	bool back_off = m_pBackOff->IsSelected();
	if ( back_off )
	{
		Vector fwd;
		AngleVectors( angles, &fwd, NULL, NULL );

		origin = origin - fwd * 75.0f;
	}

	return true;
}

void CDemoSmootherPanel::OnTogglePause()
{
	if ( !m_bPreviewing )
		return;

	m_bPreviewPaused = !m_bPreviewPaused;
}

void CDemoSmootherPanel::OnStep( bool forward )
{
	if ( !m_bPreviewing )
		return;

	if ( !m_bPreviewPaused )
		return;

	int c = m_Smoothing.smooth.Count();

	SetLastFrame( false, m_nPreviewLastFrame + ( forward ? 1 : -1 ) );
	SetLastFrame( false, clamp( m_nPreviewLastFrame, max( m_nSelection[ 0 ] - 10, 0 ), min( m_nSelection[ 1 ] + 10, c - 1 ) ) );
	m_fPreviewCurrentTime = TICKS_TO_TIME( GetTickForFrame( m_nPreviewLastFrame ) );
}

void CDemoSmootherPanel::DrawLegend( int startframe, int endframe )
{
	int i;
	int skip = 20;

	bool back_off = m_pBackOff->IsSelected();

	for ( i = startframe; i <= endframe; i++ )
	{
		bool show = ( i % skip ) == 0;
		demosmoothing_t	*sample = &m_Smoothing.smooth[ i ];

		if ( sample->samplepoint || sample->targetpoint )
			show = true;

		if ( !show )
			continue;

		char sz[ 512 ];
		Q_snprintf( sz, sizeof( sz ), "%.3f", TICKS_TO_TIME(sample->frametick) );

		Vector fwd;
		AngleVectors( sample->info.GetViewAngles(), &fwd, NULL, NULL );

		CDebugOverlay::AddTextOverlay( sample->info.GetViewOrigin() + m_vecEyeOffset + fwd * ( back_off ? 5.0f : 50.0f ), 0, -1.0f, sz );
	}
}

#define EASE_TIME 0.2f

Quaternion SmoothAngles( CUtlVector< Quaternion >& stack )
{
	int c = stack.Count();
	Assert( c >= 1 );

	float weight = 1.0f / (float)c;

	Quaternion output;
	output.Init();
	
	int i;
	for ( i = 0; i < c; i++ )
	{
		Quaternion t = stack[ i ];
		QuaternionBlend( output, t, weight, output );
	}

	return output;
}

Vector SmoothOrigin( CUtlVector< Vector >& stack )
{
	int c = stack.Count();
	Assert( c >= 1 );

	Vector output;
	output.Init();
	
	int i;
	for ( i = 0; i < c; i++ )
	{
		Vector t = stack[ i ];
		VectorAdd( output, t, output );
	}

	VectorScale( output, 1.0f / (float)c, output );

	return output;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::OnSetKeys(float interval)
{
	if ( !m_bHasSelection )
		return;

	m_bDirty = true;
	PushUndo( "OnSetKeys" );

	int c = m_Smoothing.smooth.Count();
	int i;

	demosmoothing_t *lastkey = NULL;

	for ( i = 0; i < c; i++ )
	{
		demosmoothing_t	*p = &m_Smoothing.smooth[ i ];
		if ( !p->selected )
			continue;

		p->angmoved = p->info.GetViewAngles();;
		p->vecmoved = p->info.GetViewOrigin();
		p->samplepoint = false;

		if ( !lastkey || 
			TICKS_TO_TIME( p->frametick - lastkey->frametick ) >= interval )
		{
			lastkey = p;
			p->samplepoint = true;
		}
	}

	PushRedo( "OnSetKeys" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::OnSmoothSelectionAngles( void )
{
	if ( !m_bHasSelection )
		return;

	int c = m_Smoothing.smooth.Count();
	int i;

	CUtlVector< Quaternion > stack;

	m_bDirty = true;
	PushUndo( "smooth angles" );

	for ( i = 0; i < c; i++ )
	{
		demosmoothing_t	*p = &m_Smoothing.smooth[ i ];
		if ( !p->selected )
			continue;

		while ( stack.Count() > 10 )
		{
			stack.Remove( 0 );
		}

		Quaternion q;
		AngleQuaternion( p->info.GetViewAngles(), q );
		stack.AddToTail( q );

		p->info.flags |= FDEMO_USE_ANGLES2;

		Quaternion aveq = SmoothAngles( stack );

		QAngle outangles;
		QuaternionAngles( aveq, outangles );

		p->info.viewAngles2 = outangles;
		p->info.localViewAngles2 = outangles;
	}

	PushRedo( "smooth angles" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::OnSmoothSelectionOrigin( void )
{
	if ( !m_bHasSelection )
		return;

	int c = m_Smoothing.smooth.Count();
	int i;

	CUtlVector< Vector > stack;

	m_bDirty = true;
	PushUndo( "smooth origin" );

	for ( i = 0; i < c; i++ )
	{
		demosmoothing_t	*p = &m_Smoothing.smooth[ i ];
		if ( !p->selected )
			continue;

		if ( i < 2 )
			continue;

		if ( i >= c - 2 )
			continue;

		stack.RemoveAll();

		for ( int j = -2; j <= 2; j++ )
		{
			stack.AddToTail( m_Smoothing.smooth[ i + j ].info.GetViewOrigin() );
		}

		p->info.flags |= FDEMO_USE_ORIGIN2;

		Vector org = SmoothOrigin( stack );

		p->info.viewOrigin2 = org;
	}

	PushRedo( "smooth origin" );
}

void CDemoSmootherPanel::PerformLinearInterpolatedAngleSmoothing( int startframe, int endframe )
{
	demosmoothing_t	*pstart		= &m_Smoothing.smooth[ startframe ];
	demosmoothing_t	*pend		= &m_Smoothing.smooth[ endframe ];

	int dt = pend->frametick - pstart->frametick;
	if ( dt <= 0 )
	{
		dt = 1;
	}

	CUtlVector< Quaternion > stack;

	Quaternion qstart, qend;
	AngleQuaternion( pstart->info.GetViewAngles(), qstart );
	AngleQuaternion( pend->info.GetViewAngles(), qend );

	for ( int i = startframe; i <= endframe; i++ )
	{
		demosmoothing_t	*p = &m_Smoothing.smooth[ i ];

		int elapsed = p->frametick - pstart->frametick;
		float frac = (float)elapsed / (float)dt;

		frac = clamp( frac, 0.0f, 1.0f );

		p->info.flags |= FDEMO_USE_ANGLES2;

		Quaternion interpolated;

		QuaternionSlerp( qstart, qend, frac, interpolated );

		QAngle outangles;
		QuaternionAngles( interpolated, outangles );

		p->info.viewAngles2 = outangles;
		p->info.localViewAngles2 = outangles;
	}
}

void CDemoSmootherPanel::OnLinearInterpolateAnglesBasedOnEndpoints( void )
{
	if ( !m_bHasSelection )
		return;

	int c = m_Smoothing.smooth.Count();
	if ( c < 2 )
		return;

	m_bDirty = true;
	PushUndo( "linear interp angles" );

	PerformLinearInterpolatedAngleSmoothing( m_nSelection[ 0 ], m_nSelection[ 1 ] );

	PushRedo( "linear interp angles" );
}

void CDemoSmootherPanel::OnLinearInterpolateOriginBasedOnEndpoints( void )
{
	if ( !m_bHasSelection )
		return;

	int c = m_Smoothing.smooth.Count();

	if ( c < 2 )
		return;

	demosmoothing_t	*pstart		= &m_Smoothing.smooth[ m_nSelection[ 0 ] ];
	demosmoothing_t	*pend		= &m_Smoothing.smooth[ m_nSelection[ 1 ] ];

	int dt = pend->frametick - pstart->frametick;
	if ( dt <= 0 )
		return;

	m_bDirty = true;
	PushUndo( "linear interp origin" );

	Vector vstart, vend;
	vstart = pstart->info.GetViewOrigin();
	vend = pend->info.GetViewOrigin();

	for ( int i = m_nSelection[0]; i <= m_nSelection[1]; i++ )
	{
		demosmoothing_t	*p = &m_Smoothing.smooth[ i ];

		float elapsed = p->frametick - pstart->frametick;
		float frac = elapsed / (float)dt;

		frac = clamp( frac, 0.0f, 1.0f );

		p->info.flags |= FDEMO_USE_ORIGIN2;

		Vector interpolated;

		VectorLerp( vstart, vend, frac, interpolated );

		p->info.viewOrigin2 = interpolated;
	}

	PushRedo( "linear interp origin" );

}

void CDemoSmootherPanel::OnRevertPoint( void )
{
	demosmoothing_t *p = GetCurrent();
	if ( !p )
		return;

	m_bDirty = true;
	PushUndo( "revert point" );

	p->angmoved = p->info.GetViewAngles();
	p->vecmoved = p->info.GetViewOrigin();
	p->samplepoint = false;

	p->vectarget = p->info.GetViewOrigin();
	p->targetpoint = false;

//	m_ViewOrigin = p->info.viewOrigin;
//	m_ViewAngles = p->info.viewAngles;

	PushRedo( "revert point" );
}

demosmoothing_t *CDemoSmootherPanel::GetCurrent( void )
{
	if ( !CanEdit() )
		return NULL;

	int c = m_Smoothing.smooth.Count();
	if ( c < 1 )
		return NULL;

	int frame = clamp( m_nPreviewLastFrame, 0, c - 1 );

	return &m_Smoothing.smooth[ frame ];
}

void CDemoSmootherPanel::AddSamplePoints( bool usetarget, bool includeboundaries, CUtlVector< demosmoothing_t * >& points, int start, int end )
{
	points.RemoveAll();

	int i;
	for ( i = start; i <= end; i++ )
	{
		demosmoothing_t	*p = &m_Smoothing.smooth[ i ];

		if ( includeboundaries )
		{
			if ( i == start )
			{
				// Add it twice
				points.AddToTail( p );
				continue;
			}
			else if ( i == end )
			{
				// Add twice
				points.AddToTail( p );
				continue;
			}
		}

		if ( usetarget && p->targetpoint )
		{
			points.AddToTail( p );
		}
		if ( !usetarget && p->samplepoint )
		{
			points.AddToTail( p );
		}
	}
}

demosmoothing_t *CDemoSmootherPanel::GetBoundedSample( CUtlVector< demosmoothing_t * >& points, int sample )
{
	int c = points.Count();
	if ( sample < 0 )
		return points[ 0 ];
	else if ( sample >= c )
		return points[ c - 1 ];
	return points[ sample ];
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : t - 
//			points - 
//			prev - 
//			next - 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::FindSpanningPoints( int tick, CUtlVector< demosmoothing_t * >& points, int& prev, int& next )
{
	prev = -1;
	next = 0;
	int c = points.Count();
	int i;

	for ( i = 0; i < c; i++ )
	{
		demosmoothing_t	*p = points[ i ];
		
		if ( tick < p->frametick )
			break;
	}

	next = i;
	prev = i - 1;

	next = clamp( next, 0, c - 1 );
	prev = clamp( prev, 0, c - 1 );
}

void CDemoSmootherPanel::OnSplineSampleOrigin( void )
{
	if ( !m_bHasSelection )
		return;

	int c = m_Smoothing.smooth.Count();
	
	if ( c < 2 )
		return;

	demosmoothing_t	*pstart		= &m_Smoothing.smooth[ m_nSelection[ 0 ] ];
	demosmoothing_t	*pend		= &m_Smoothing.smooth[ m_nSelection[ 1 ] ];

	if ( pend->frametick - pstart->frametick <= 0 )
		return;

	CUtlVector< demosmoothing_t * > points;
	AddSamplePoints( false, false, points, m_nSelection[ 0 ], m_nSelection[ 1 ] );

	if ( points.Count() <= 0 )
		return;

	m_bDirty = true;
	PushUndo( "spline origin" );

	for ( int i = m_nSelection[0]; i <= m_nSelection[1]; i++ )
	{
		demosmoothing_t	*p = &m_Smoothing.smooth[ i ];

		demosmoothing_t	*earliest;
		demosmoothing_t	*current;
		demosmoothing_t	*next;
		demosmoothing_t	*latest;
		
		int cur;
		int cur2;

		FindSpanningPoints( p->frametick, points, cur, cur2 );

		earliest = GetBoundedSample( points, cur - 1 );
		current = GetBoundedSample( points, cur );
		next = GetBoundedSample( points, cur2 );
		latest = GetBoundedSample( points, cur2 + 1 );

		float frac = 0.0f;
		float dt = next->frametick - current->frametick;
		if ( dt > 0.0f )
		{
			frac = (float)( p->frametick - current->frametick ) / dt;
		}

		frac = clamp( frac, 0.0f, 1.0f );

		Vector splined;

		Catmull_Rom_Spline_Normalize( earliest->vecmoved, current->vecmoved, next->vecmoved, latest->vecmoved, frac, splined );

		p->info.flags |= FDEMO_USE_ORIGIN2;
		p->info.viewOrigin2 = splined;
	}

	PushRedo( "spline origin" );

}

void CDemoSmootherPanel::OnSplineSampleAngles( void )
{
	if ( !m_bHasSelection )
		return;

	int c = m_Smoothing.smooth.Count();
	
	if ( c < 2 )
		return;

	demosmoothing_t	*pstart		= &m_Smoothing.smooth[ m_nSelection[ 0 ] ];
	demosmoothing_t	*pend		= &m_Smoothing.smooth[ m_nSelection[ 1 ] ];

	if ( pend->frametick - pstart->frametick <= 0 )
		return;

	CUtlVector< demosmoothing_t * > points;
	AddSamplePoints( false, false, points, m_nSelection[ 0 ], m_nSelection[ 1 ] );

	if ( points.Count() <= 0 )
		return;

	m_bDirty = true;
	PushUndo( "spline angles" );

	for ( int i = m_nSelection[0]; i <= m_nSelection[1]; i++ )
	{
		demosmoothing_t	*p = &m_Smoothing.smooth[ i ];

		demosmoothing_t	*current;
		demosmoothing_t	*next;
		
		int cur;
		int cur2;

		FindSpanningPoints( p->frametick, points, cur, cur2 );

		current = GetBoundedSample( points, cur );
		next = GetBoundedSample( points, cur2 );

		float frac = 0.0f;
		float dt = next->frametick - current->frametick;
		if ( dt > 0.0f )
		{
			frac = (float)( p->frametick - current->frametick ) / dt;
		}

		frac = clamp( frac, 0.0f, 1.0f );

		frac = SimpleSpline( frac );

		QAngle splined;

		InterpolateAngles( current->angmoved, next->angmoved, splined, frac );

		p->info.flags |= FDEMO_USE_ANGLES2;
		p->info.viewAngles2 = splined;
		p->info.localViewAngles2 = splined;
	}

	PushRedo( "spline angles" );
}

void CDemoSmootherPanel::OnLookAtPoints( bool spline )
{
	if ( !m_bHasSelection )
		return;

	int c = m_Smoothing.smooth.Count();
	int i;

	if ( c < 2 )
		return;

	demosmoothing_t	*pstart		= &m_Smoothing.smooth[ m_nSelection[ 0 ] ];
	demosmoothing_t	*pend		= &m_Smoothing.smooth[ m_nSelection[ 1 ] ];

	if ( pend->frametick - pstart->frametick <= 0 )
		return;

	CUtlVector< demosmoothing_t * > points;
	AddSamplePoints( true, false, points, m_nSelection[ 0 ], m_nSelection[ 1 ] );

	if ( points.Count() < 1 )
		return;

	m_bDirty = true;
	PushUndo( "lookat points" );

	for ( i = m_nSelection[0]; i <= m_nSelection[1]; i++ )
	{
		demosmoothing_t	*p = &m_Smoothing.smooth[ i ];

		demosmoothing_t	*earliest;
		demosmoothing_t	*current;
		demosmoothing_t	*next;
		demosmoothing_t	*latest;
		
		int cur;
		int cur2;

		FindSpanningPoints( p->frametick, points, cur, cur2 );

		earliest = GetBoundedSample( points, cur - 1 );
		current = GetBoundedSample( points, cur );
		next = GetBoundedSample( points, cur2 );
		latest = GetBoundedSample( points, cur2 + 1 );

		float frac = 0.0f;
		float dt = next->frametick - current->frametick;
		if ( dt > 0.0f )
		{
			frac = (float)( p->frametick - current->frametick ) / dt;
		}

		frac = clamp( frac, 0.0f, 1.0f );

		Vector splined;

		if ( spline )
		{
			Catmull_Rom_Spline_Normalize( earliest->vectarget, current->vectarget, next->vectarget, latest->vectarget, frac, splined );
		}
		else
		{
			Vector d = next->vectarget - current->vectarget;
			VectorMA( current->vectarget, frac, d, splined );
		}
		
		Vector vecToTarget = splined - ( p->info.GetViewOrigin() + m_vecEyeOffset );
		VectorNormalize( vecToTarget );

		QAngle angles;
		VectorAngles( vecToTarget, angles );

		p->info.flags |= FDEMO_USE_ANGLES2;
		p->info.viewAngles2 = angles;
		p->info.localViewAngles2 = angles;
	}

	PushRedo( "lookat points" );
}

void CDemoSmootherPanel::SetLastFrame( bool jumptotarget, int frame )
{
	// bool changed = frame != m_nPreviewLastFrame;
	
	int useFrame = max( m_Smoothing.m_nFirstSelectableSample, frame );

	m_nPreviewLastFrame = useFrame;

	/* if ( changed && !m_pLockCamera->IsSelected()  )
	{
		// Reset default view/angles
		demosmoothing_t	*p = GetCurrent();
		if ( p )
		{
			if ( p->samplepoint && !jumptotarget )
			{
				m_ViewOrigin = p->vecmoved;
				m_ViewAngles = p->angmoved;
			}
			else if ( p->targetpoint && jumptotarget )
			{
				m_ViewOrigin = p->vectarget - m_vecEyeOffset;
			}
			else
			{
				if ( m_bPreviewing && m_bPreviewOriginal )
				{
					m_ViewOrigin = p->info.viewOrigin;
					m_ViewAngles = p->info.viewAngles;
				}
				else
				{
					m_ViewOrigin = p->info.GetViewOrigin();
					m_ViewAngles = p->info.GetViewAngles();
				}
			}
		}
	} */
}

// Undo/Redo
void CDemoSmootherPanel::Undo( void )
{
	if ( m_UndoStack.Size() > 0 && m_nUndoLevel > 0 )
	{
		m_nUndoLevel--;
		DemoSmoothUndo *u = m_UndoStack[ m_nUndoLevel ];
		Assert( u->undo );

		m_Smoothing = *(u->undo);
	}
	InvalidateLayout();
}

void CDemoSmootherPanel::Redo( void )
{
	if ( m_UndoStack.Size() > 0 && m_nUndoLevel <= m_UndoStack.Size() - 1 )
	{
		DemoSmoothUndo *u = m_UndoStack[ m_nUndoLevel ];
		Assert( u->redo );

		m_Smoothing = *(u->redo);
		m_nUndoLevel++;
	}

	InvalidateLayout();
}

void CDemoSmootherPanel::PushUndo( const char *description )
{
	Assert( !m_bRedoPending );
	m_bRedoPending = true;
	WipeRedo();

	// Copy current data
	CSmoothingContext *u = new CSmoothingContext;
	*u = m_Smoothing;
	DemoSmoothUndo *undo = new DemoSmoothUndo;
	undo->undo = u;
	undo->redo = NULL;
	undo->udescription = COM_StringCopy( description );
	undo->rdescription = NULL;
	m_UndoStack.AddToTail( undo );
	m_nUndoLevel++;
}

void CDemoSmootherPanel::PushRedo( const char *description )
{
	Assert( m_bRedoPending );
	m_bRedoPending = false;

	// Copy current data
	CSmoothingContext *r = new CSmoothingContext;
	*r = m_Smoothing;
	DemoSmoothUndo *undo = m_UndoStack[ m_nUndoLevel - 1 ];
	undo->redo = r;
	undo->rdescription = COM_StringCopy( description );
}

void CDemoSmootherPanel::WipeUndo( void )
{
	while ( m_UndoStack.Size() > 0 )
	{
		DemoSmoothUndo *u = m_UndoStack[ 0 ];
		delete u->undo;
		delete u->redo;
		delete[] u->udescription;
		delete[] u->rdescription;
		delete u;
		m_UndoStack.Remove( 0 );
	}
	m_nUndoLevel = 0;
}

void CDemoSmootherPanel::WipeRedo( void )
{
	// Wipe everything above level
	while ( m_UndoStack.Size() > m_nUndoLevel )
	{
		DemoSmoothUndo *u = m_UndoStack[ m_nUndoLevel ];
		delete u->undo;
		delete u->redo;
		delete[] u->udescription;
		delete[] u->rdescription;
		delete u;
		m_UndoStack.Remove( m_nUndoLevel );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CDemoSmootherPanel::GetUndoDescription( void )
{
	if ( m_nUndoLevel != 0 )
	{
		DemoSmoothUndo *u = m_UndoStack[ m_nUndoLevel - 1 ];
		return u->udescription;
	}
	return "???undo";
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CDemoSmootherPanel::GetRedoDescription( void )
{
	if ( m_nUndoLevel != m_UndoStack.Size() )
	{
		DemoSmoothUndo *u = m_UndoStack[ m_nUndoLevel ];
		return u->rdescription;
	}
	return "???redo";
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDemoSmootherPanel::CanRedo( void )
{
	if ( !m_UndoStack.Count() )
		return false;

	if ( m_nUndoLevel == m_UndoStack.Count()  )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDemoSmootherPanel::CanUndo( void )
{
	if ( !m_UndoStack.Count() )
		return false;

	if ( m_nUndoLevel == 0 )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::OnToggleKeyFrame( void )
{
	demosmoothing_t *p = GetCurrent();
	if ( !p )
		return;

	m_bDirty = true;
	PushUndo( "toggle keyframe" );

	// use orginal data by default
	p->angmoved = p->info.GetViewAngles();
	p->vecmoved = p->info.GetViewOrigin();

	if ( !p->samplepoint )
	{
		if ( g_pDemoUI->IsInDriveMode() )
		{
			g_pDemoUI->GetDriveViewPoint( p->vecmoved, p->angmoved );
		}

		if ( g_pDemoUI2->IsInDriveMode() )
		{
			g_pDemoUI2->GetDriveViewPoint( p->vecmoved, p->angmoved );
		}

		p->samplepoint = true;
	}
	else
	{
		p->samplepoint = false;
	}

	PushRedo( "toggle keyframe" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::OnToggleLookTarget( void )
{
	demosmoothing_t *p = GetCurrent();
	if ( !p )
		return;

	m_bDirty = true;
	PushUndo( "toggle look target" );

	// use orginal data by default
	p->vectarget = p->info.GetViewOrigin();

	if ( !p->targetpoint )
	{
		QAngle angles;
		g_pDemoUI->GetDriveViewPoint( p->vectarget, angles );
		g_pDemoUI2->GetDriveViewPoint( p->vectarget, angles );

		p->targetpoint = true;
	}
	else
	{
		p->targetpoint = false;
	}

	PushRedo( "toggle look target" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::OnNextKey()
{
	if( !m_bHasSelection )
		return;

	int start = m_nPreviewLastFrame + 1;
	int maxmove = m_nSelection[1] - m_nSelection[0] + 1;

	int moved = 0;

	while ( moved < maxmove )
	{
		demosmoothing_t *p = &m_Smoothing.smooth[ start ];
		if ( p->samplepoint )
		{
			SetLastFrame( false, start );
			break;
		}

		start++;

		if ( start > m_nSelection[1] )
			start = m_nSelection[0];

		moved++;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::OnPrevKey()
{
	if( !m_bHasSelection )
		return;

	int start = m_nPreviewLastFrame - 1;
	int maxmove = m_nSelection[1] - m_nSelection[0] + 1;

	int moved = 0;

	while ( moved < maxmove && start >= 0 )
	{
		demosmoothing_t *p = &m_Smoothing.smooth[ start ];
		if ( p->samplepoint )
		{
			SetLastFrame( false, start );
			break;
		}

		start--;

		if ( start < m_nSelection[0] )
			start = m_nSelection[1];

		moved++;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::OnNextTarget()
{
	if( !m_bHasSelection )
		return;

	int start = m_nPreviewLastFrame + 1;
	int maxmove = m_nSelection[1] - m_nSelection[0] + 1;

	int moved = 0;

	while ( moved < maxmove )
	{
		demosmoothing_t *p = &m_Smoothing.smooth[ start ];
		if ( p->targetpoint )
		{
			SetLastFrame( true, start );
			break;
		}

		start++;

		if ( start > m_nSelection[1] )
			start = m_nSelection[0];

		moved++;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::OnPrevTarget()
{
	if( !m_bHasSelection )
		return;

	int start = m_nPreviewLastFrame - 1;
	int maxmove = m_nSelection[1] - m_nSelection[0] + 1;

	int moved = 0;

	while ( moved < maxmove )
	{
		demosmoothing_t *p = &m_Smoothing.smooth[ start ];
		if ( p->targetpoint )
		{
			SetLastFrame( true, start );
			break;
		}

		start--;

		if ( start < m_nSelection[0] )
			start = m_nSelection[1];

		moved++;
	}
}

void CDemoSmootherPanel::DrawTargetSpline()
{
	if ( !m_bHasSelection )
		return;

	int c = m_Smoothing.smooth.Count();
	int i;

	if ( c < 2 )
		return;

	demosmoothing_t	*pstart		= &m_Smoothing.smooth[ m_nSelection[ 0 ] ];
	demosmoothing_t	*pend		= &m_Smoothing.smooth[ m_nSelection[ 1 ] ];

	if ( pend->frametick - pstart->frametick <= 0 )
		return;

	CUtlVector< demosmoothing_t * > points;
	AddSamplePoints( true, false, points, m_nSelection[ 0 ], m_nSelection[ 1 ] );

	if ( points.Count() < 1 )
		return;

	Vector previous(0,0,0);

	for ( i = m_nSelection[0]; i <= m_nSelection[1]; i++ )
	{
		demosmoothing_t	*p = &m_Smoothing.smooth[ i ];

		demosmoothing_t	*earliest;
		demosmoothing_t	*current;
		demosmoothing_t	*next;
		demosmoothing_t	*latest;
		
		int cur;
		int cur2;

		FindSpanningPoints( p->frametick, points, cur, cur2 );

		earliest = GetBoundedSample( points, cur - 1 );
		current = GetBoundedSample( points, cur );
		next = GetBoundedSample( points, cur2 );
		latest = GetBoundedSample( points, cur2 + 1 );

		float frac = 0.0f;
		float dt = next->frametick - current->frametick;
		if ( dt > 0.0f )
		{
			frac = (float)( p->frametick - current->frametick ) / dt;
		}

		frac = clamp( frac, 0.0f, 1.0f );

		Vector splined;

		Catmull_Rom_Spline_Normalize( earliest->vectarget, current->vectarget, next->vectarget, latest->vectarget, frac, splined );

		if ( i > m_nSelection[0] )
		{
			RenderLine( previous, splined, Color( 0, 255, 0, 255 ), true );
		}

		previous = splined;
	}
}

void CDemoSmootherPanel::DrawKeySpline()
{
	if ( !m_bHasSelection )
		return;

	int c = m_Smoothing.smooth.Count();
	int i;

	if ( c < 2 )
		return;

	demosmoothing_t	*pstart		= &m_Smoothing.smooth[ m_nSelection[ 0 ] ];
	demosmoothing_t	*pend		= &m_Smoothing.smooth[ m_nSelection[ 1 ] ];

	if ( pend->frametick - pstart->frametick <= 0 )
		return;

	CUtlVector< demosmoothing_t * > points;
	AddSamplePoints( false, false, points, m_nSelection[ 0 ], m_nSelection[ 1 ] );

	if ( points.Count() < 1 )
		return;

	Vector previous(0,0,0);

	for ( i = m_nSelection[0]; i <= m_nSelection[1]; i++ )
	{
		demosmoothing_t	*p = &m_Smoothing.smooth[ i ];

		demosmoothing_t	*earliest;
		demosmoothing_t	*current;
		demosmoothing_t	*next;
		demosmoothing_t	*latest;
		
		int cur;
		int cur2;

		FindSpanningPoints( p->frametick, points, cur, cur2 );

		earliest = GetBoundedSample( points, cur - 1 );
		current = GetBoundedSample( points, cur );
		next = GetBoundedSample( points, cur2 );
		latest = GetBoundedSample( points, cur2 + 1 );

		float frac = 0.0f;
		float dt = next->frametick - current->frametick;
		if ( dt > 0.0f )
		{
			frac = (float)( p->frametick - current->frametick ) / dt;
		}

		frac = clamp( frac, 0.0f, 1.0f );

		Vector splined;

		Catmull_Rom_Spline_Normalize( earliest->vecmoved, current->vecmoved, next->vecmoved, latest->vecmoved, frac, splined );

		splined += m_vecEyeOffset;

		if ( i > m_nSelection[0] )
		{
			RenderLine( previous, splined, Color( 0, 255, 0, 255 ), true );
		}

		previous = splined;
	}
}

void CDemoSmootherPanel::OnSmoothEdges( bool left, bool right )
{
	if ( !m_bHasSelection )
		return;

	if ( !left && !right )
		return;

	int c = m_Smoothing.smooth.Count();

	// Get number of frames
	char sz[ 512 ];
	m_pFixEdgeFrames->GetText( sz, sizeof( sz ) );

	int frames = atoi( sz );
	if ( frames <= 2 )
		return;

	m_bDirty = true;
	PushUndo( "smooth edges" );

	if ( left && m_nSelection[0] > 0 )
	{
		PerformLinearInterpolatedAngleSmoothing( m_nSelection[ 0 ] - 1, m_nSelection[ 0 ] + frames );
	}
	if ( right && m_nSelection[1] < c - 1 )
	{
		PerformLinearInterpolatedAngleSmoothing( m_nSelection[ 1 ] - frames, m_nSelection[ 1 ] + 1 );
	}

	PushRedo( "smooth edges" );
}

void CDemoSmootherPanel::OnSaveKey()
{
	if ( !m_bHasSelection )
		return;

	demosmoothing_t *p = GetCurrent();
	if ( !p )
		return;

	if ( !p->samplepoint )
		return;

	m_bDirty = true;
	PushUndo( "save key" );

	p->info.viewAngles2 = p->angmoved;
	p->info.localViewAngles2 = p->angmoved;
	p->info.viewOrigin2 = p->vecmoved;
	p->info.flags |= FDEMO_USE_ORIGIN2;
	p->info.flags |= FDEMO_USE_ANGLES2;
	
	PushRedo( "save key" );
}

void CDemoSmootherPanel::OnSetView()
{
	if ( !m_bHasSelection )
		return;

	demosmoothing_t *p = GetCurrent();
	if ( !p )
		return;

	Vector origin = p->info.GetViewOrigin();
	QAngle angle = p->info.GetViewAngles();

	g_pDemoUI->SetDriveViewPoint( origin, angle );
	g_pDemoUI2->SetDriveViewPoint( origin, angle );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoSmootherPanel::OnGotoFrame()
{
	int c = m_Smoothing.smooth.Count();
	if ( c < 2 )
		return;

	char sz[ 256 ];
	m_pGotoFrame->GetText( sz, sizeof( sz ) );
	int frame = atoi( sz );

	if ( !m_bPreviewing )
	{
		if ( !m_bHasSelection )
		{
			m_pStartFrame->SetText( va( "%i", 0 ) );
			m_pEndFrame->SetText( va( "%i", c - 1 ) );
			OnSelect();
		}
		OnPreview( false );
		OnTogglePause();
	}

	if ( !m_bPreviewing )
		return;

	SetLastFrame( false, frame );
	m_iPreviewStartTick = GetTickForFrame( m_nPreviewLastFrame );
	m_fPreviewCurrentTime = TICKS_TO_TIME( m_iPreviewStartTick );
}

void CDemoSmootherPanel::OnOriginEaseCurve( EASEFUNC easefunc )
{
	if ( !m_bHasSelection )
		return;

	int c = m_Smoothing.smooth.Count();
	
	if ( c < 2 )
		return;

	demosmoothing_t	*pstart		= &m_Smoothing.smooth[ m_nSelection[ 0 ] ];
	demosmoothing_t	*pend		= &m_Smoothing.smooth[ m_nSelection[ 1 ] ];

	float dt = pend->frametick - pstart->frametick;
	if ( dt <= 0.0f )
		return;

	m_bDirty = true;
	PushUndo( "ease origin" );

	Vector vstart, vend;
	vstart = pstart->info.GetViewOrigin();
	vend = pend->info.GetViewOrigin();

	for ( int i = m_nSelection[0]; i <= m_nSelection[1]; i++ )
	{
		demosmoothing_t	*p = &m_Smoothing.smooth[ i ];

		float elapsed = p->frametick - pstart->frametick;
        float frac = elapsed / dt;

		// Apply ease function
		frac = (*easefunc)( frac );

		frac = clamp( frac, 0.0f, 1.0f );

		p->info.flags |= FDEMO_USE_ORIGIN2;

		Vector interpolated;

		VectorLerp( vstart, vend, frac, interpolated );

		p->info.viewOrigin2 = interpolated;
	}

	PushRedo( "ease origin" );
}

void CDemoSmootherPanel::ParseSmoothingInfo( CDemoFile &demoFile, CSmoothingContext& smoothing )
{
	democmdinfo_t	info;
	int				dummy;

	bool foundFirstSelectable = false;

	bool demofinished = false;
	while ( !demofinished )
	{
		int			tick = 0;
		byte		cmd;

		bool swallowmessages = true;
		do
		{
			demoFile.ReadCmdHeader( cmd, tick );

			// COMMAND HANDLERS
			switch ( cmd )
			{
			case dem_synctick:
				break;
			case dem_stop:
				{
					swallowmessages = false;
					demofinished = true;
				}
				break;
			case dem_consolecmd:
				{
					demoFile.ReadConsoleCommand();
				}
				break;
			case dem_datatables:
				{
					demoFile.ReadNetworkDataTables( NULL );
				}
				break;
			case dem_stringtables:
				{
					demoFile.ReadStringTables( NULL );
				}
				break;
			case dem_usercmd:
				{
					demoFile.ReadUserCmd( NULL, dummy );
					
				}
				break;
			default:
				{
					swallowmessages = false;
				}
				break;
			}
		}
		while ( swallowmessages );

		if ( demofinished )
		{
			// StopPlayback();
			return;
		}

		int curpos = demoFile.GetCurPos( true );

		demoFile.ReadCmdInfo( info );
		demoFile.ReadSequenceInfo( dummy, dummy ); 
		demoFile.ReadRawData( NULL, 0 );

		// Add to end of list
		demosmoothing_t smoothing_entry;

		smoothing_entry.file_offset = curpos;
		smoothing_entry.frametick = tick; 
		smoothing_entry.info = info;
		smoothing_entry.samplepoint = false;
		smoothing_entry.vecmoved = 	info.GetViewOrigin();
		smoothing_entry.angmoved = 	info.GetViewAngles();
		smoothing_entry.targetpoint = false;
		smoothing_entry.vectarget = info.GetViewOrigin();

		int sampleIndex = smoothing.smooth.AddToTail( smoothing_entry );

		if ( !foundFirstSelectable && 
			smoothing_entry.vecmoved.LengthSqr() > 0.0f )
		{
			foundFirstSelectable = true;
			smoothing.m_nFirstSelectableSample = sampleIndex;
		}
	}
}

void CDemoSmootherPanel::LoadSmoothingInfo( const char *filename, CSmoothingContext& smoothing )
{
	char name[ MAX_OSPATH ];
	Q_strncpy (name, filename, sizeof(name) );
	Q_DefaultExtension( name, ".dem", sizeof( name ) );

	CDemoFile demoFile;

	if ( !demoFile.Open( filename, true )  )
	{
		ConMsg( "ERROR: couldn't open %s.\n", name );
		return;
	}

	demoheader_t * header = demoFile.ReadDemoHeader();

	if ( !header )
	{
		demoFile.Close();
		return;
	}

	ConMsg ("Smoothing demo from %s ...", name );

	smoothing.active = true;
	Q_strncpy( smoothing.filename, name, sizeof(smoothing.filename) );

	smoothing.smooth.RemoveAll();

	ClearSmoothingInfo( smoothing );

	ParseSmoothingInfo( demoFile, smoothing );
	
	demoFile.Close();
	
	//Performsmoothing( smooth );
	//SaveSmoothedDemo( name, smooth );

	ConMsg ( " done.\n" );
}

void CDemoSmootherPanel::ClearSmoothingInfo( CSmoothingContext& smoothing )
{
	int c = smoothing.smooth.Count();
	int i;

	for ( i = 0; i < c; i++ )
	{
		demosmoothing_t	*p = &smoothing.smooth[ i ];
		p->info.Reset();
		p->vecmoved = p->info.GetViewOrigin();
		p->angmoved = p->info.GetViewAngles();
		p->samplepoint = false;
		p->vectarget = p->info.GetViewOrigin();
		p->targetpoint = false;
	}
}

void CDemoSmootherPanel::SaveSmoothingInfo( char const *filename, CSmoothingContext& smoothing )
{
	// Nothing to do
	int c = smoothing.smooth.Count();
	if ( !c )
		return;

	IFileSystem *fs = g_pFileSystem;

	FileHandle_t infile, outfile;

	COM_OpenFile( filename, &infile );
	if ( infile == FILESYSTEM_INVALID_HANDLE )
		return;

	int filesize = fs->Size( infile );

	char outfilename[ 512 ];
	Q_StripExtension( filename, outfilename, sizeof( outfilename ) );
	Q_strncat( outfilename, "_smooth", sizeof(outfilename), COPY_ALL_CHARACTERS );
	Q_DefaultExtension( outfilename, ".dem", sizeof( outfilename ) );
	outfile = fs->Open( outfilename, "wb" );
	if ( outfile == FILESYSTEM_INVALID_HANDLE )
	{
		fs->Close( infile );
		return;
	}

	int i;

	int lastwritepos = 0;
	for ( i = 0; i < c; i++ )
	{
		demosmoothing_t	*p = &smoothing.smooth[ i ];

		int copyamount = p->file_offset - lastwritepos;

		COM_CopyFileChunk( outfile, infile, copyamount );

		fs->Seek( infile, p->file_offset, FILESYSTEM_SEEK_HEAD );

		// wacky hacky overwriting 
		fs->Write( &p->info, sizeof( democmdinfo_t ), outfile );

		lastwritepos = fs->Tell( outfile );
		fs->Seek( infile, p->file_offset + sizeof( democmdinfo_t ), FILESYSTEM_SEEK_HEAD );
	}

	int final = filesize - lastwritepos;

	COM_CopyFileChunk( outfile, infile, final );

	fs->Close( outfile );
	fs->Close( infile );
}
