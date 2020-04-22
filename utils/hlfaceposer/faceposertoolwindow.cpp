//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "hlfaceposer.h"
#include "faceposertoolwindow.h"
#include "utlvector.h"
#include "tier1/strtools.h"
#include "MDLViewer.h"
#include "choreowidgetdrawhelper.h"
#include "StudioModel.h"
#include "faceposer_models.h"

extern MDLViewer *g_MDLViewer;

static CUtlVector< IFacePoserToolWindow * > g_Tools;
IFacePoserToolWindow *IFacePoserToolWindow::s_pActiveTool = NULL;

bool	IFacePoserToolWindow::s_bToolsCanDraw;
static CUtlVector< IFacePoserToolWindow * > s_NeedRedraw;


IFacePoserToolWindow::IFacePoserToolWindow( char const *toolname, char const *displaynameroot )
{
	m_bAutoProcess = false;
	m_bUseForMainWindowTitle = false;
	SetToolName( toolname );
	m_szPrefix[0]=0;
	m_szSuffix[0]=0;

	SetDisplayNameRoot( displaynameroot );

	g_Tools.AddToTail( this );

	m_nToolFrameCount = 0;
}

mxWindow *IFacePoserToolWindow::GetMxWindow( void )
{
	return dynamic_cast< mxWindow * >( this );
}

IFacePoserToolWindow *IFacePoserToolWindow::GetActiveTool( void )
{
	if ( s_pActiveTool )
		return s_pActiveTool;

	if ( GetToolCount() > 0 )
		return GetTool( 0 );

	return NULL;
}

void IFacePoserToolWindow::SetActiveTool( IFacePoserToolWindow *tool )
{
	if ( tool != s_pActiveTool && s_pActiveTool )
	{
		InvalidateRect( (HWND)s_pActiveTool->GetMxWindow()->getHandle(), NULL, TRUE );
		InvalidateRect( (HWND)tool->GetMxWindow()->getHandle(), NULL, TRUE );
	}
	s_pActiveTool = tool;
}

void IFacePoserToolWindow::Think( float dt )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void IFacePoserToolWindow::SetToolName( char const *name )
{
	Q_strncpy( m_szToolName, name, sizeof( m_szToolName ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *IFacePoserToolWindow::GetToolName( void ) const
{
	return m_szToolName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void IFacePoserToolWindow::SetDisplayNameRoot( char const *name )
{
	Q_snprintf( m_szDisplayRoot, sizeof( m_szDisplayRoot ), "%s", name );
	ComputeNewTitle();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *IFacePoserToolWindow::GetDisplayNameRoot( void  ) const
{
	return m_szDisplayRoot;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *suffix - 
//-----------------------------------------------------------------------------
void IFacePoserToolWindow::SetSuffix( char const *suffix )
{
	Q_snprintf( m_szSuffix, sizeof( m_szSuffix ), "%s", suffix );
	ComputeNewTitle();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *prefix - 
//-----------------------------------------------------------------------------
void IFacePoserToolWindow::SetPrefix( char const *prefix )
{
	Q_snprintf( m_szPrefix, sizeof( m_szPrefix ), "%s", prefix );
	ComputeNewTitle();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *IFacePoserToolWindow::GetWindowTitle( void ) const
{
	return m_szWindowTitle;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : use - 
//-----------------------------------------------------------------------------
void IFacePoserToolWindow::SetUseForMainWindowTitle( bool use )
{
	m_bUseForMainWindowTitle = use;
	if ( use )
	{
		g_MDLViewer->setLabel( m_szWindowTitle );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void IFacePoserToolWindow::ComputeNewTitle( void )
{
	Q_snprintf( m_szWindowTitle, sizeof( m_szWindowTitle ), "%s%s%s", m_szPrefix, m_szDisplayRoot, m_szSuffix );
	if ( GetMxWindow() )
	{
		GetMxWindow()->setLabel( m_szWindowTitle );
	}
	if ( !m_bUseForMainWindowTitle )
		return;

	g_MDLViewer->setLabel( m_szWindowTitle );
}

IFacePoserToolWindow::~IFacePoserToolWindow( void )
{
	g_Tools.FindAndRemove( this );
}

struct ToolTranslate
{
	char const *toolname;
	float		xfrac;
	float		yfrac;
	float		wfrac;
	float		hfrac;
	bool		locked;
};

static ToolTranslate s_ToolTranslate[]=
{
	{ "3D View", 0.0, 0.0, 0.4, 0.5, false },
	{ "ControlPanel", 0.4, 0.0, 0.2, 0.25, false },
	{ "FlexPanel", 0.6, 0.0, 0.4, 0.25, false },
	{ "RampTool", 0.4, 0.25, 0.6, 0.25, false },
	{ "CChoreoView", 0.0, 0.5, 1.0, 0.45, false },
//	{ "Status Window", 0.0, 0.85, 1.0, 0.15, false },
};

static bool TranslateToolPos( char const *toolname, int workspacew, int workspaceh, int& x, int& y, int &w, int &h, bool& locked )
{
	int c = ARRAYSIZE( s_ToolTranslate );

	for ( int i = 0; i < c; ++i )
	{
		ToolTranslate& tt = s_ToolTranslate[ i ];

		if ( !Q_stricmp( toolname, tt.toolname ) )
		{
			x = (int)((float)workspacew * tt.xfrac + 0.5f );
			y = (int)((float)workspaceh * tt.yfrac + 0.5f );
			w = (int)((float)workspacew * tt.wfrac + 0.5f );
			h = (int)((float)workspaceh * tt.hfrac + 0.5f );
			locked = tt.locked;
			return true;
		}
	}

	return false;
}

static int s_nToolCount = 0;

void IFacePoserToolWindow::LoadPosition( void )
{
	bool visible;
	bool locked;
	bool zoomed;
	int x, y, w, h;

	FacePoser_LoadWindowPositions( GetToolName(), visible, x, y, w, h, locked, zoomed );

	if ( w == 0 || h == 0 )
	{
		int idx = g_Tools.Find( this );
		Assert( idx != g_Tools.InvalidIndex() );
		if ( idx == 0 )
		{
			s_nToolCount = 0;
		}

		zoomed = false;
		locked = false;
		visible = true;

		// Just do a simple tiling
		w = g_MDLViewer->w2() * 0.5;
		h = g_MDLViewer->h2() * 0.5;

		x = g_MDLViewer->w2() * 0.25f + s_nToolCount * 20;
		y = s_nToolCount * 20;

		bool translated = TranslateToolPos
		( 
			GetToolName(), 
			g_MDLViewer->w2(), 
			g_MDLViewer->h2(),
			x,
			y,
			w,
			h,
			locked
		);
		if ( !translated )
		{
			++s_nToolCount;
			visible = false;
		}
	}

	GetMxWindow()->setBounds( x, y, w, h );
	if ( locked ^ IsLocked() )
	{
		ToggleLockedState();
	}
	GetMxWindow()->setVisible( visible );
}

void IFacePoserToolWindow::SavePosition( void )
{
	bool visible;
	int xpos, ypos, width, height;

	visible = GetMxWindow()->isVisible();
	xpos = GetMxWindow()->x();
	ypos = GetMxWindow()->y();
	width = GetMxWindow()->w();
	height = GetMxWindow()->h();

	// xpos and ypos are screen space
	POINT pt;
	pt.x = xpos;
	pt.y = ypos;

	// Convert from screen space to relative to client area of parent window so
	//  the setBounds == MoveWindow call will offset to the same location
	if ( GetMxWindow()->getParent() )
	{
		ScreenToClient( (HWND)GetMxWindow()->getParent()->getHandle(), &pt );
		xpos = (short)pt.x;
		ypos = (short)pt.y;
	}

	FacePoser_SaveWindowPositions( GetToolName(), visible, xpos, ypos, width, height, IsLocked(), false );
}
	
int IFacePoserToolWindow::GetToolCount( void )
{
	return g_Tools.Count();
}

IFacePoserToolWindow *IFacePoserToolWindow::GetTool( int index )
{
	return g_Tools[ index ];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void IFacePoserToolWindow::InitTools( void )
{
	int c = GetToolCount();
	int i;
	for ( i = 0; i < c ; i++ )
	{
		IFacePoserToolWindow *tool = GetTool( i );

		FacePoser_MakeToolWindow( tool->GetMxWindow(), true );
		tool->GetMxWindow()->setLabel( tool->GetWindowTitle() );
	}
}

void IFacePoserToolWindow::ShutdownTools( void )
{
	int c = GetToolCount();
	int i;
	for ( i = 0; i < c ; i++ )
	{
		IFacePoserToolWindow *tool = GetTool( i );
		tool->Shutdown();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void IFacePoserToolWindow::ToolThink( float dt )
{
	int c = GetToolCount();
	int i;
	for ( i = 0; i < c ; i++ )
	{
		IFacePoserToolWindow *tool = GetTool( i );
		tool->Think( dt );
	}

	// Don't self animate, all animation driven by thinking of various tools now
	
	if ( !ShouldAutoProcess() )
	{
		c = models->Count();
		for ( i = 0; i < c; i++ )
		{
			StudioModel *m = models->GetStudioModel( i );
			if ( m )
			{
				m->AdvanceFrame ( dt );
			}
		}
	}
}

bool IFacePoserToolWindow::IsLocked( void )
{
	mxWindow *w = GetMxWindow();
	if ( !w )
		return false;

	return !FacePoser_HasWindowStyle( w, WS_SYSMENU );
}

void IFacePoserToolWindow::ToggleLockedState( void )
{
	mxWindow *w = GetMxWindow();
	if ( !w )
		return;

	bool visible = w->isVisible();

	bool islocked = IsLocked();
	if ( islocked )
	{
		FacePoser_MakeToolWindow( w, true );
	}
	else
	{
		FacePoser_RemoveWindowStyle( w, WS_OVERLAPPEDWINDOW ); 
		FacePoser_AddWindowExStyle( w, WS_EX_OVERLAPPEDWINDOW );
	}

	w->setVisible( false );

	// If visible, force it to redraw, etc.
	if ( visible )
	{
		w->setVisible( true );
	}
}

#define LOCK_INSET 2
#define LOCK_SIZE 8

void IFacePoserToolWindow::	GetLockRect( RECT& rc )
{
	mxWindow *w = GetMxWindow();
	Assert( w );
	if ( !w )
		return;

	GetCloseRect( rc );

	OffsetRect( &rc, - ( LOCK_SIZE + 2 * LOCK_INSET ), 0 );
}

void IFacePoserToolWindow::GetCloseRect( RECT& rc )
{
	mxWindow *w = GetMxWindow();
	Assert( w );
	if ( !w )
		return;

	rc.right = w->w2() - LOCK_INSET;
	rc.left = rc.right - LOCK_SIZE;
	rc.top = LOCK_INSET;
	rc.bottom = rc.top + LOCK_SIZE;
}

bool IFacePoserToolWindow::HandleToolEvent( mxEvent *event )
{
	bool handled = false;
	switch ( event->event )
	{
	default:
		break;
	case mxEvent::Close:
		{
			g_MDLViewer->UpdateWindowMenu();
			handled = true;
		}
		break;
	case mxEvent::ParentNotify:
		{
			mxWindow *w = GetMxWindow();
			if ( w )
			{
				HWND wnd = (HWND)w->getHandle();
				SetFocus( wnd );
				SetWindowPos( wnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
				SetActiveTool( this );
			}
			handled = true;
		}
		break;
	case mxEvent::PosChanged:
		{
			SetActiveTool( this );
			mxWindow *w = GetMxWindow();
			if ( w )
			{
				SetFocus( (HWND)w->getHandle() );
			}
			handled = true;
		}
		break;
	case mxEvent::MouseDown:
	case mxEvent::MouseUp:
		{
			bool isup = event->event == mxEvent::MouseUp;

			mxWindow *w = GetMxWindow();

			if ( !isup && w )
			{
				SetFocus( (HWND)w->getHandle() );
				SetWindowPos( (HWND)w->getHandle(), HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

				SetActiveTool( this );
			}

			if ( w && IsLocked() )
			{
				RECT captionRect;
				captionRect.left = 0;
				captionRect.right = w->w2();
				captionRect.top = 0;
				captionRect.bottom = GetCaptionHeight();

				POINT pt;
				pt.x = (short)event->x;
				pt.y = (short)event->y;

				if ( PtInRect( &captionRect, pt ) )
				{
					handled = !isup;

					// Right button anywhere
					if ( event->buttons & mxEvent::MouseRightButton )
					{
						if ( isup )
						{
							ToggleLockedState();
						}
					}

					// Left button on lock icon
					RECT lockRect, closeRect;
					GetLockRect( lockRect );
					GetCloseRect( closeRect );

					if ( PtInRect( &lockRect, pt ) )
					{
						if ( isup )
						{
							ToggleLockedState();
						}
					}

					if ( PtInRect( &closeRect, pt ) )
					{
						if ( isup )
						{
							w->setVisible( !w->isVisible() );
							g_MDLViewer->UpdateWindowMenu();
						}
					}
				}
			}
		}
		break;
	case mxEvent::NCMouseUp:
		{
			if ( event->buttons & mxEvent::MouseRightButton )
			{	
				ToggleLockedState();
				handled = true;
			}
		}
		break;

	case mxEvent::NCMouseDown:
	case mxEvent::Focus:
		{
			SetActiveTool( this );
			// don't mark handled = true, do this passively
		}
		break;
	}

	return handled;
}

void IFacePoserToolWindow::HandleToolRedraw( CChoreoWidgetDrawHelper& helper )
{
	if ( !IsLocked() )
		return;

	mxWindow *w = GetMxWindow();
	if ( !w )
		return;

	++m_nToolFrameCount;

	RECT lockRect, closeRect;
	GetLockRect( lockRect );
	GetCloseRect( closeRect );

	RECT captionRect;
	helper.GetClientRect( captionRect );
	RECT rcClient = captionRect;
	captionRect.bottom = captionRect.top + LOCK_SIZE + 2 * LOCK_INSET;

	COLORREF textColor = GetSysColor( COLOR_MENUTEXT ); //GetSysColor( COLOR_INACTIVECAPTIONTEXT );

	if ( IsActiveTool() )
	{
		helper.DrawFilledRect( GetSysColor( COLOR_ACTIVECAPTION ), captionRect );
	}
	else
	{
		helper.DrawFilledRect( GetSysColor( COLOR_INACTIVECAPTION ), captionRect );
	}

	captionRect.top += 1;

	InflateRect( &captionRect, -LOCK_INSET, 0 );

	helper.DrawColoredText( "Small Fonts", 9, FW_NORMAL, textColor, captionRect,
		GetWindowTitle() );

	//RECT rcFrame = captionRect;
	//rcFrame.left = rcFrame.right - 50;
	//rcFrame.right = rcFrame.left + 30;
	// helper.DrawColoredText( "Small Fonts", 9, FW_NORMAL, textColor, rcFrame, va( "%i", m_nToolFrameCount ) );

	lockRect.bottom++;
	OffsetRect( &lockRect, 1, 1 );
	helper.DrawColoredTextCharset( "Marlett", 8, FW_NORMAL, SYMBOL_CHARSET, textColor, lockRect, "v" );

	closeRect.bottom++;
	helper.DrawOutlinedRect( textColor, PS_SOLID, 1, closeRect );
	OffsetRect( &closeRect, 1, 1 );
	helper.DrawColoredTextCharset( "Marlett", 8, FW_NORMAL, SYMBOL_CHARSET, textColor, closeRect, "r" );

	rcClient.top += captionRect.bottom;

	helper.StartClipping( rcClient );
}

int IFacePoserToolWindow::GetCaptionHeight( void )
{
	if ( !IsLocked() )
		return 0;

	return LOCK_SIZE + 2 * LOCK_INSET;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : autoprocess - 
//-----------------------------------------------------------------------------
void IFacePoserToolWindow::SetAutoProcess( bool autoprocess )
{
	m_bAutoProcess = autoprocess;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool IFacePoserToolWindow::GetAutoProcess( void ) const
{
	return m_bAutoProcess;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool IFacePoserToolWindow::IsActiveTool( void )
{
	if ( this == s_pActiveTool )
		return true;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool IFacePoserToolWindow::IsAnyToolScrubbing( void )
{
	int c = GetToolCount();
	int i;
	for ( i = 0; i < c ; i++ )
	{
		IFacePoserToolWindow *tool = GetTool( i );
		if ( tool->IsScrubbing() )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool IFacePoserToolWindow::IsAnyToolProcessing( void )
{
	int c = GetToolCount();
	int i;
	for ( i = 0; i < c ; i++ )
	{
		IFacePoserToolWindow *tool = GetTool( i );
		if ( tool->IsProcessing() )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool IFacePoserToolWindow::ShouldAutoProcess( void )
{
	IFacePoserToolWindow *tool = GetActiveTool();
	if ( !tool )
		return false;

	return tool->GetAutoProcess();
}

void IFacePoserToolWindow::EnableToolRedraw( bool enabled )
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	s_bToolsCanDraw = enabled;

	if ( s_bToolsCanDraw )
	{
		int c = s_NeedRedraw.Count();
		int i;
		for ( i = 0; i < c; i++ )
		{
			IFacePoserToolWindow *tool = s_NeedRedraw[ i ];
			tool->GetMxWindow()->redraw();
		}

		s_NeedRedraw.Purge();
	}
}

bool IFacePoserToolWindow::ToolCanDraw()
{
	if ( !s_bToolsCanDraw )
	{
		if ( s_NeedRedraw.Find( this ) == s_NeedRedraw.InvalidIndex() )
		{
			s_NeedRedraw.AddToTail( this );
		}

		return false;
	}

	return true;
}

void IFacePoserToolWindow::OnModelChanged()
{
}

void IFacePoserToolWindow::ModelChanged()
{
	int c = GetToolCount();
	int i;
	for ( i = 0; i < c ; i++ )
	{
		IFacePoserToolWindow *tool = GetTool( i );
		tool->OnModelChanged();
	}
}


