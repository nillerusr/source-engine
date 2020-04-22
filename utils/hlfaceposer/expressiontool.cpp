//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include <stdio.h>
#include "hlfaceposer.h"
#include "ExpressionTool.h"
#include "mdlviewer.h"
#include "choreowidgetdrawhelper.h"
#include "TimelineItem.h"
#include "expressions.h"
#include "expclass.h"
#include "choreoevent.h"
#include "StudioModel.h"
#include "choreoscene.h"
#include "choreoactor.h"
#include "choreochannel.h"
#include "ChoreoView.h"
#include "InputProperties.h"
#include "ControlPanel.h"
#include "FlexPanel.h"
#include "mxExpressionTray.h"
#include "ExpressionProperties.h"
#include "tier1/strtools.h"
#include "faceposer_models.h"
#include "UtlBuffer.h"
#include "filesystem.h"
#include "iscenetokenprocessor.h"
#include "MatSysWin.h"
#include "choreoviewcolors.h"
#include "scriplib.h"
#include "EdgeProperties.h"

ExpressionTool *g_pExpressionTool = 0;

#define TRAY_HEIGHT 55 

#define TRAY_ITEM_INSET 10

#define MAX_TIME_ZOOM 1000
// 10% per step
#define TIME_ZOOM_STEP 2

void SetupFlexControllerTracks( CStudioHdr *hdr, CChoreoEvent *event );

class CExpressionToolWorkspace : public mxWindow
{
public:
	CExpressionToolWorkspace( mxWindow *parent );
	~CExpressionToolWorkspace();

	virtual int			handleEvent( mxEvent *event );
	virtual void		redraw( void );
	virtual bool		PaintBackground( void )
	{
		redraw();
		return false;
	}

	void				RepositionVSlider( void );
	int					ComputeVPixelsNeeded( void );
	// Playback tick
	void				Think( float dt );

	void				LayoutItems( bool force = false );

	void				HideTimelines( void );
	void				CollapseAll( TimelineItem *keepExpanded );

	void				ExpandAll( void );
	void				ExpandValid( void );
	void				DisableAllExcept( void );
	void				EnableValid( void );

	TimelineItem		*GetItem( int number );
	TimelineItem		*GetClickedItem( void );
	void				ClearClickedItem( void );

	void				OnSnapAll();
	void				OnDeleteColumn();

	void				MoveSelectedSamples( float dfdx, float dfdy, bool snap );
	void				DeleteSelectedSamples( void );
	int					CountSelectedSamples( void );
	void				DeselectAll( void );
	void				SelectPoints( float start, float end );

	void				DrawEventEnd( CChoreoWidgetDrawHelper& drawHelper );

	void				OnSortByUsed( void );
	void				OnSortByName( void );

private:

	int					GetItemUnderMouse( int mx, int my );

	void				MouseToToolMouse( int& mx, int& my, char *reason );

	TimelineItem		*m_pItems[ GLOBAL_STUDIO_FLEX_CONTROL_COUNT ];

	// The scroll bars
	mxScrollbar			*m_pVertScrollBar;
	int					m_nLastVPixelsNeeded;

	int					m_nTopOffset;
	int					m_nScrollbarHeight;

	int					m_nItemGap;
	int					m_nFocusItem;
};

CExpressionToolWorkspace::CExpressionToolWorkspace( mxWindow *parent ) :
	mxWindow( parent, 0, 0, 0, 0 )
{
	HWND wnd = (HWND)getHandle();
	DWORD style = GetWindowLong( wnd, GWL_STYLE );
	style |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	SetWindowLong( wnd, GWL_STYLE, style );

	for ( int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++ )
	{
		m_pItems[ i ] = new TimelineItem( this );
	}

	m_nItemGap		= 2;

	m_nScrollbarHeight = 12;
	m_nTopOffset	= 0;

	m_nLastVPixelsNeeded = -1;

	m_pVertScrollBar = new mxScrollbar( this, 0, 0, 12, 100, IDC_EXPRESSIONTOOLVSCROLL, mxScrollbar::Vertical );

	m_nFocusItem = -1;

	HideTimelines();
	LayoutItems();
}

CExpressionToolWorkspace::~CExpressionToolWorkspace()
{
}

void CExpressionToolWorkspace::redraw()
{
	CChoreoWidgetDrawHelper drawHelper( this );

	DrawEventEnd( drawHelper );

	for ( int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++ )
	{
		TimelineItem *item = GetItem( i );
		if ( !item )
			continue;

		if ( !item->GetVisible() )
			continue;

		RECT rcBounds;
		item->GetBounds( rcBounds );

		if ( rcBounds.bottom < 0 )
			continue;
		if ( rcBounds.top > h2() )
			continue;

		item->Draw( drawHelper );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *elem1 - 
//			*elem2 - 
// Output : int
//-----------------------------------------------------------------------------
int SortFuncByUse(const void *elem1, const void *elem2 )
{
	TimelineItem *item1 = *( TimelineItem ** )elem1;
	TimelineItem *item2 = *( TimelineItem ** )elem2;

	if ( item1->IsValid() == item2->IsValid() )
		return 0;

	if ( !item2->IsValid() && item1->IsValid() )
		return -1;

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *elem1 - 
//			*elem2 - 
// Output : int
//-----------------------------------------------------------------------------
int SortFuncByName(const void *elem1, const void *elem2 )
{
	TimelineItem *item1 = *( TimelineItem ** )elem1;
	TimelineItem *item2 = *( TimelineItem ** )elem2;

	CFlexAnimationTrack *track1 = item1->GetSafeTrack();
	CFlexAnimationTrack *track2 = item2->GetSafeTrack();

	if ( !track1 || !track2 )
	{
		if ( track1 )
			return -1;
		if ( track2 )
			return 1;
		return 0;
	}

	return stricmp( track1->GetFlexControllerName(), track2->GetFlexControllerName() );
}

void CExpressionToolWorkspace::OnSortByUsed( void )
{
	qsort( m_pItems, GLOBAL_STUDIO_FLEX_CONTROL_COUNT, sizeof( TimelineItem * ), SortFuncByUse );
	LayoutItems( false );
}

void CExpressionToolWorkspace::OnSortByName( void )
{
	qsort( m_pItems, GLOBAL_STUDIO_FLEX_CONTROL_COUNT, sizeof( TimelineItem * ), SortFuncByName );
	LayoutItems( false );
}

void CExpressionToolWorkspace::DrawEventEnd( CChoreoWidgetDrawHelper& drawHelper )
{
	if ( !g_pExpressionTool )
		return;

	CChoreoEvent *e = g_pExpressionTool->GetSafeEvent();
	if ( !e )
		return;

	float duration = e->GetDuration();
	if ( !duration )
		return;

	int leftx = g_pExpressionTool->GetPixelForTimeValue( duration ) -5;
	if ( leftx >= w2() )
		return;

	RECT rcClient;
	drawHelper.GetClientRect( rcClient );

	drawHelper.DrawColoredLine(
		COLOR_CHOREO_ENDTIME, PS_SOLID, 1,
		leftx, rcClient.top, leftx, rcClient.bottom );

}

int CExpressionToolWorkspace::GetItemUnderMouse( int mx, int my )
{
	POINT pt;
	pt.x = mx;
	pt.y = my;

	for ( int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++ )
	{
		TimelineItem *item = GetItem( i );
		if ( !item )
			continue;

		if ( !item->GetVisible() )
			continue;

		RECT rc;
		item->GetBounds( rc );

		if ( PtInRect( &rc, pt ) )
		{
			return i;
		}
	}

	return -1;
}

void CExpressionToolWorkspace::MouseToToolMouse( int& mx, int& my, char *reason )
{
	POINT pt;
	pt.x = mx;
	pt.y = my;

	ClientToScreen( (HWND)getHandle(), &pt );
	ScreenToClient( (HWND)getParent()->getHandle(), &pt );

	mx = pt.x;
	my = pt.y;
}

int	CExpressionToolWorkspace::handleEvent( mxEvent *event )
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	int iret = 0;

	switch ( event->event )
	{
	case mxEvent::MouseDown:
		{
			HWND wnd = (HWND)getParent()->getHandle();
			SetFocus( wnd );
			SetWindowPos( wnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

			{
				int mx = (short)event->x;
				int my = (short)event->y;

				MouseToToolMouse( mx, my, "CExpressionToolWorkspace mousedown" );

				g_pExpressionTool->SetClickedPos( mx, my );
				g_pExpressionTool->SetMouseOverPos( mx, my );
				g_pExpressionTool->DrawMouseOverPos();

			}

			int oldFocus = m_nFocusItem;
			m_nFocusItem = GetItemUnderMouse( (short)event->x, (short)event->y );

			if ( oldFocus != -1 &&
				oldFocus != m_nFocusItem )
			{
				TimelineItem *item = GetItem( oldFocus );
				if ( item )
				{
					item->DrawSelf();
				}
			}
			if ( m_nFocusItem != -1 )
			{
				TimelineItem *item = GetItem( m_nFocusItem );
				if ( item )
				{
					RECT rc;
					item->GetBounds( rc );

					event->x -= rc.left;
					event->y -= rc.top;

					iret = item->handleEvent( event );
				}
			}
			
			iret = 1;
		}
		break;
	case mxEvent::MouseDrag:
	case mxEvent::MouseMove:
		{
			// 
			bool handled = false;

			if ( m_nFocusItem != -1 )
			{
				TimelineItem *item = GetItem( m_nFocusItem );
				if ( item )
				{
					RECT rc;
					item->GetBounds( rc );

					event->x -= rc.left;
					event->y -= rc.top;

					iret = item->handleEvent( event );

					if ( event->event == mxEvent::MouseDrag )
					{
						int mx, my;
						
						item->GetLastMouse( mx, my );
						mx += rc.left;
						my += rc.top;

						MouseToToolMouse( mx, my, "CExpressionToolWorkspace mousedrag" );

						g_pExpressionTool->SetMouseOverPos( mx, my );
						g_pExpressionTool->DrawMouseOverPos();
						handled = true;
					}
				}
			}

			if ( !handled )
			{
				int mx = (short)event->x;
				int my = (short)event->y;

				mx += TRAY_ITEM_INSET;

				MouseToToolMouse( mx, my, "CExpressionToolWorkspace mousemove" );

				g_pExpressionTool->SetMouseOverPos( mx, my );
				g_pExpressionTool->DrawMouseOverPos();
			}
		}
		break;
	case mxEvent::MouseUp:
		{
			// 
			{
				int mx = (short)event->x;
				int my = (short)event->y;

				MouseToToolMouse( mx, my, "CExpressionToolWorkspace mouseup" );

				g_pExpressionTool->SetMouseOverPos( mx, my );
				g_pExpressionTool->DrawMouseOverPos();

			}

			if ( m_nFocusItem != -1 )
			{
				TimelineItem *item = GetItem( m_nFocusItem );
				if ( item )
				{
					RECT rc;
					item->GetBounds( rc );

					event->x -= rc.left;
					event->y -= rc.top;

					iret = item->handleEvent( event );
				}
			}
		}
		break;
	case mxEvent::Size:
		{
			RepositionVSlider();
			LayoutItems();
			iret = 1;
		}
		break;
	case mxEvent::MouseWheeled:
		// Tell parent
		{
			if ( event->modifiers & mxEvent::KeyShift )
			{
				CChoreoScene *scene = g_pChoreoView->GetScene();
				if ( scene )
				{
					int tz = g_pChoreoView->GetTimeZoom( g_pExpressionTool->GetToolName() );

					// Zoom time in  / out
					if ( event->height > 0 )
					{
						g_pChoreoView->SetTimeZoom( g_pExpressionTool->GetToolName(), min( tz + TIME_ZOOM_STEP, MAX_TIME_ZOOM ), false );
					}
					else
					{
						g_pChoreoView->SetTimeZoom( g_pExpressionTool->GetToolName(), min( tz - TIME_ZOOM_STEP, MAX_TIME_ZOOM ), false );
					}
					g_pExpressionTool->RepositionHSlider();
				}
				redraw();
				iret = 1;
				return iret;
			}

			int offset = 0;
			int jump = 50;

			if ( event->height < 0 )
			{
				offset = m_pVertScrollBar->getValue();
				offset += jump;
				offset = min( offset, m_pVertScrollBar->getMaxValue() );
			}
			else
			{
				offset = m_pVertScrollBar->getValue();
				offset -= jump;
				offset = max( offset, m_pVertScrollBar->getMinValue() );
			}

			m_pVertScrollBar->setValue( offset );
			InvalidateRect( (HWND)m_pVertScrollBar->getHandle(), NULL, TRUE );
			m_nTopOffset = offset;
			LayoutItems();
			iret = 1;
		}
		break;
	case mxEvent::Action:
		{
			iret = 1;
			switch ( event->action )
			{
			default:
				iret = 0;
				break;
			case IDC_EXPRESSIONTOOLVSCROLL:
				{
					int offset = 0;
					bool processed = true;

					switch ( event->modifiers )
					{
					case SB_THUMBTRACK:
						offset = event->height;
						break;
					case SB_PAGEUP:
						offset = m_pVertScrollBar->getValue();
						offset -= 100;
						offset = max( offset, m_pVertScrollBar->getMinValue() );
						break;
					case SB_PAGEDOWN:
						offset = m_pVertScrollBar->getValue();
						offset += 100;
						offset = min( offset, m_pVertScrollBar->getMaxValue() );
						break;
					case SB_LINEDOWN:
						offset = m_pVertScrollBar->getValue();
						offset += 10;
						offset = min( offset, m_pVertScrollBar->getMaxValue() );
						break;
					case SB_LINEUP:
						offset = m_pVertScrollBar->getValue();
						offset -= 10;
						offset = max( offset, m_pVertScrollBar->getMinValue() );
						break;
					default:
						processed = false;
						break;
					}
		
					if ( processed )
					{
						m_pVertScrollBar->setValue( offset );
						InvalidateRect( (HWND)m_pVertScrollBar->getHandle(), NULL, TRUE );
						m_nTopOffset = offset;
						LayoutItems();
					}
				}
			}
		}
		break;
	}
	return iret;
}

void CExpressionToolWorkspace::HideTimelines( void )
{
	for ( int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++ )
	{
		TimelineItem *item = GetItem( i );
		Assert( item );
		item->SetVisible( false );
	}

	redraw();
}


TimelineItem *CExpressionToolWorkspace::GetItem( int number )
{
	if ( number < 0 || number >= GLOBAL_STUDIO_FLEX_CONTROL_COUNT )
	{
		return NULL;
	}
	return m_pItems[ number ];
}

TimelineItem *CExpressionToolWorkspace::GetClickedItem( void )
{
	return GetItem( m_nFocusItem );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CExpressionToolWorkspace::ClearClickedItem( void )
{
	m_nFocusItem = -1;
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : force - force vert scrollbar recomputation	
//-----------------------------------------------------------------------------
void CExpressionToolWorkspace::LayoutItems( bool force /* = false */ )
{
	int x = TRAY_ITEM_INSET;
	int y = - m_nTopOffset;
	int width = w2() - 2 * TRAY_ITEM_INSET - m_nScrollbarHeight;
	int height;

	for ( int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++ )
	{
		TimelineItem *item = GetItem( i );
		if ( !item || !item->GetVisible() )
			continue;

		height = item->GetHeight();

		RECT rcBounds;
		rcBounds.left = x;
		rcBounds.top = y;
		rcBounds.right = x + width;
		rcBounds.bottom = y + height;

		item->SetBounds( rcBounds );
		y += height + m_nItemGap;
	}

	if ( force || ( ComputeVPixelsNeeded() != m_nLastVPixelsNeeded ) )
	{
		RepositionVSlider();
	}

	redraw();
}

int CExpressionToolWorkspace::ComputeVPixelsNeeded( void )
{
	int pixels = 0;

	// Count visible
	int c = 0;
	for ( int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++ )
	{
		TimelineItem *item = GetItem( i );
		if ( !item || !item->GetVisible() )
			continue;

		c += item->GetHeight();
		c += m_nItemGap;
	}

	pixels += c;

	return pixels;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CExpressionToolWorkspace::RepositionVSlider( void )
{
	int pixelsneeded = ComputeVPixelsNeeded();

	if ( pixelsneeded <= ( h2() ))
	{
		m_pVertScrollBar->setVisible( false );
		m_nTopOffset = 0;
	}
	else
	{
		m_pVertScrollBar->setVisible( true );
	}

	m_pVertScrollBar->setBounds( 
		w2() - m_nScrollbarHeight, 
		0, 
		m_nScrollbarHeight, 
		h2() );

	m_nTopOffset = max( 0, m_nTopOffset );
	m_nTopOffset = min( pixelsneeded, m_nTopOffset );

	m_pVertScrollBar->setRange( 0, pixelsneeded );
	m_pVertScrollBar->setValue( m_nTopOffset );
	m_pVertScrollBar->setPagesize( h2() );

	m_nLastVPixelsNeeded = pixelsneeded;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CExpressionToolWorkspace::DisableAllExcept( void )
{
	TimelineItem *keepExpanded = GetClickedItem();
	if ( !keepExpanded )
		return;

	g_pChoreoView->SetDirty( true );
	g_pChoreoView->PushUndo( "Disable All Except" );

	for ( int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++ )
	{
		TimelineItem *item = GetItem( i );

		item->SetActive( item == keepExpanded ? true : false );
	}

	LayoutItems();
	g_pChoreoView->PushRedo( "Disable All Except" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CExpressionToolWorkspace::EnableValid( void )
{
	g_pChoreoView->SetDirty( true );
	g_pChoreoView->PushUndo( "Enable Valid" );
	for ( int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++ )
	{
		TimelineItem *item = GetItem( i );

		item->SetActive( item->IsValid() );
	}

	LayoutItems();
	g_pChoreoView->PushRedo( "Enable Valid" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CExpressionToolWorkspace::CollapseAll( TimelineItem *keepExpanded )
{
	for ( int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++ )
	{
		TimelineItem *item = GetItem( i );

		item->SetCollapsed( item == keepExpanded ? false : true );
	}

	LayoutItems();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CExpressionToolWorkspace::ExpandAll( void )
{
	for ( int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++ )
	{
		TimelineItem *item = GetItem( i );
		item->SetCollapsed( false );
	}

	LayoutItems();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CExpressionToolWorkspace::OnSnapAll()
{
	g_pChoreoView->SetDirty( true );
	g_pChoreoView->PushUndo( "Snap All" );

	for ( int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++ )
	{
		TimelineItem *item = GetItem( i );
		item->SnapAll();
	}

	g_pChoreoView->PushRedo( "Snap All" );
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CExpressionToolWorkspace::OnDeleteColumn()
{
	float t = g_pExpressionTool->GetTimeForClickedPos();

	float snapped = FacePoser_SnapTime( t );
	int scenefps = FacePoser_GetSceneFPS();

	if ( scenefps <= 0 )
	{
		Con_Printf( "Can't delete column, scene fps is <= 0 (%i)\n", scenefps );
		return;
	}

	int clickedframe = ( int ) ( scenefps * snapped + 0.5f );

	// One half of 1/fps on each side
	float epsilon = epsilon = 0.5f / (float)scenefps;

	CInputParams params;
	memset( &params, 0, sizeof( params ) );
	strcpy( params.m_szDialogTitle, "Delete Column" );
	strcpy( params.m_szPrompt, "Frame(s) to delete [e.g., 82 or 81-91 ]:" );
	Q_snprintf( params.m_szInputText, sizeof( params.m_szInputText ), "%i", clickedframe );

	if ( !InputProperties( &params ) )
		return;

	int deleteframestart;
	int deleteframeend;

	char *sep = Q_strstr( params.m_szInputText, "-" );
	if ( sep )
	{
		*sep = 0;
		deleteframestart = atoi( params.m_szInputText );
		deleteframeend = atoi( sep + 1 );
		deleteframeend = max( deleteframestart, deleteframeend );
	}
	else
	{
		deleteframestart = atoi( params.m_szInputText );
		deleteframeend = deleteframestart;
	}

	float start, end;

	start = (float)deleteframestart / (float)scenefps;
	end = (float)deleteframeend / (float)scenefps;

	g_pChoreoView->SetDirty( true );
	g_pChoreoView->PushUndo( "Delete Column" );

	for ( int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++ )
	{
		TimelineItem *item = GetItem( i );
		item->DeletePoints( start - epsilon, end + epsilon );
	}

	g_pChoreoView->PushRedo( "Delete Column" );
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CExpressionToolWorkspace::ExpandValid( void )
{
	for ( int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++ )
	{
		TimelineItem *item = GetItem( i );
		bool valid = item->IsValid();
		item->SetCollapsed( !valid );
	}

	LayoutItems();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CExpressionToolWorkspace::CountSelectedSamples( void )
{
	int c = 0;
	for ( int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++ )
	{
		TimelineItem *item = GetItem( i );
		Assert( item );
		item->CountSelected();
		c += item->GetNumSelected();
	}
	return c;
}

void CExpressionToolWorkspace::MoveSelectedSamples( float dfdx, float dfdy, bool snap )
{
	int selecteditems = CountSelectedSamples();
	if ( !selecteditems )
		return;

	CChoreoEvent *e = g_pExpressionTool->GetSafeEvent();
	if ( !e )
		return;

	float eventduration = e->GetDuration();

	for ( int controller = 0; controller < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; controller++ )
	{
		TimelineItem *item = GetItem( controller );
		if ( !item )
			continue;

		CFlexAnimationTrack *track = item->GetSafeTrack();
		if ( !track )
			continue;

		// If the track is a combo type track, then move any underlying selected samples, too
		for ( int edittype = 0; edittype <= ( track->IsComboType() ? 1 : 0 ); edittype++ )
		{
			for ( int i = 0; i < (int)track->GetNumSamples( edittype ); i++ )
			{
				CExpressionSample *sample = track->GetSample( i, edittype );
				if ( !sample || !sample->selected )
					continue;

				sample->time += dfdx;
				sample->time = clamp( sample->time, 0.0f, eventduration );

				if ( snap )
				{
					sample->time = FacePoser_SnapTime( sample->time );
				}

				sample->value -= dfdy;
				sample->value = clamp( sample->value, 0.0f, 1.0f );
			}
		}
				
		track->Resort();

		item->DrawSelf();
	}
}

void CExpressionToolWorkspace::DeleteSelectedSamples( void )
{
	int i, t;

	int selecteditems = CountSelectedSamples();
	if ( !selecteditems )
		return;

	g_pChoreoView->SetDirty( true );
	g_pChoreoView->PushUndo( "Delete points" );

	for ( int controller = 0; controller < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; controller++ )
	{
		TimelineItem *item = GetItem( controller );
		if ( !item )
			continue;

		CFlexAnimationTrack *track = item->GetSafeTrack();
		if ( !track )
			continue;

		for ( t = 0; t < 2; t++ )
		{
			for ( i = track->GetNumSamples( t ) - 1; i >= 0 ; i-- )
			{
				CExpressionSample *sample = track->GetSample( i, t );
				if ( !sample->selected )
					continue;

				track->RemoveSample( i, t );
			}
		}

		item->DrawSelf();
	}

	g_pChoreoView->PushRedo( "Delete points" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CExpressionToolWorkspace::DeselectAll( void )
{
	int i, t;

	int selecteditems = CountSelectedSamples();
	if ( !selecteditems )
		return;

	for ( int controller = 0; controller < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; controller++ )
	{
		TimelineItem *item = GetItem( controller );
		if ( !item )
			continue;

		CFlexAnimationTrack *track = item->GetSafeTrack();
		if ( !track )
			continue;

		for ( t = 0; t < 2; t++ )
		{
			for ( i = track->GetNumSamples( t ) - 1; i >= 0 ; i-- )
			{
				CExpressionSample *sample = track->GetSample( i, t );
				sample->selected = false;
			}
		}

		item->DrawSelf();
	}
}

void CExpressionToolWorkspace::SelectPoints( float start, float end )
{
	int i, t;

	for ( int controller = 0; controller < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; controller++ )
	{
		TimelineItem *item = GetItem( controller );
		if ( !item )
			continue;

		CFlexAnimationTrack *track = item->GetSafeTrack();
		if ( !track )
			continue;

		for ( t = 0; t < 2; t++ )
		{
			for ( i = track->GetNumSamples( t ) - 1; i >= 0 ; i-- )
			{
				CExpressionSample *sample = track->GetSample( i, t );
				bool inrange = ( sample->time >= start && sample->time <= end );
				sample->selected = inrange;
			}
		}

		item->DrawSelf();
	}
}

ExpressionTool::ExpressionTool( mxWindow *parent )
: IFacePoserToolWindow( "ExpressionTool", "Flex Animation" ), mxWindow( parent, 0, 0, 0, 0 )
{
	m_bSuppressLayout = false;

	SetAutoProcess( true );

	m_pWorkspace = new CExpressionToolWorkspace( this );

	m_nFocusEventGlobalID = -1;

	m_flScrub			= 0.0f;
	m_flScrubTarget		= 0.0f;
	m_nDragType			= DRAGTYPE_NONE;

	m_nClickedX			= 0;
	m_nClickedY			= 0;

	m_hPrevCursor		= 0;
	
	m_nStartX			= 0;
	m_nStartY			= 0;

	m_nMinX				= 0;
	m_nMaxX				= 0;
	m_bUseBounds		= false;

	m_pLastEvent		= NULL;

	m_nMousePos[ 0 ] = m_nMousePos[ 1 ] = 0;

	m_flSelection[ 0 ] = m_flSelection[ 1 ] = 0.0f;
	m_bSelectionActive = false;

	m_bLayoutIsValid = false;
	m_flPixelsPerSecond = 500.0f;

	m_flLastDuration = 0.0f;
	m_nScrollbarHeight	= 12;
	m_flLeftOffset = 0.0f;
	m_nLastHPixelsNeeded = -1;
	m_pHorzScrollBar = new mxScrollbar( this, 0, 0, 18, 100, IDC_FLEXHSCROLL, mxScrollbar::Horizontal );
	m_pHorzScrollBar->setVisible( false );

	m_bInSetEvent = false;
	m_flScrubberTimeOffset = 0.0f;
}

ExpressionTool::~ExpressionTool( void )
{
}

void ExpressionTool::DoTrackLookup( CChoreoEvent *event )
{
	if ( !event || !models->GetActiveStudioModel() )
		return;

	//if ( event->GetTrackLookupSet() )
	//	return;

	// Force recompute
	SetEvent( event );
}

#pragma optimize( "g", off )

void ExpressionTool::SetEvent( CChoreoEvent *event )
{
	if ( m_bInSetEvent )
		return;

	m_bInSetEvent = true;

	if ( event == m_pLastEvent )
	{
		if ( event )
		{
			float dur = event->GetDuration();
			if ( dur != m_flLastDuration )
			{
				m_flLastDuration = dur;
				m_nLastHPixelsNeeded = -1;
				m_flLeftOffset = 0.0f;
				InvalidateLayout();
			}

			m_nFocusEventGlobalID = event->GetGlobalID();
		}
		m_bInSetEvent = false;
		return;
	}

	m_pLastEvent = event;

	m_pWorkspace->HideTimelines();

	m_nFocusEventGlobalID = -1;
	if ( event )
	{
		m_nFocusEventGlobalID = event->GetGlobalID();

		if ( models->GetActiveStudioModel() )
		{
			CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
			if ( hdr )
			{
				// Force re-lookup
				event->SetTrackLookupSet( false );
				
				SetupFlexControllerTracks( hdr, event );

				int itemCount = 0;

				for ( int i = 0; i < event->GetNumFlexAnimationTracks(); i++ )
				{
					CFlexAnimationTrack *track = event->GetFlexAnimationTrack( i );
					Assert( track );
					if ( !track )
						continue;

					TimelineItem *item = m_pWorkspace->GetItem( itemCount++ );
					item->SetExpressionInfo( track, track->GetFlexControllerIndex( 0 ) );
					item->SetCollapsed( track->GetNumSamples( 0 ) <= 0 );
					item->SetVisible( true );
				}

				m_pWorkspace->LayoutItems( true );
			}
		}
	}

	DeselectAll();
	
	if ( event )
	{
		m_flLastDuration = event->GetDuration();
	}
	else
	{
		m_flLastDuration = 0.0f;
	}

	m_flLeftOffset = 0.0f;
	m_nLastHPixelsNeeded = -1;
	InvalidateLayout();

	m_bInSetEvent = false;
}

#pragma optimize( "g", on )

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ExpressionTool::HasCopyData( void )
{
	return ( m_CopyData[0].Size() != 0 ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *source - 
//-----------------------------------------------------------------------------
void ExpressionTool::Copy( CFlexAnimationTrack *source )
{
	for ( int t = 0; t < 2; t++ )
	{
		m_CopyData[ t ].RemoveAll();

		if ( t == 0 || source->IsComboType() )
		{
			for ( int i = 0 ; i < source->GetNumSamples( t ); i++ )
			{
				CExpressionSample *s = source->GetSample( i, t );
				m_CopyData[ t ].AddToTail( *s );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *destination - 
//-----------------------------------------------------------------------------
void ExpressionTool::Paste( CFlexAnimationTrack *destination )
{
	g_pChoreoView->SetDirty( true );
	g_pChoreoView->PushUndo( "Paste" );

	destination->Clear();

	for ( int t = 0; t < 2; t++ )
	{
		for ( int i = 0; i < m_CopyData[ t ].Size() ; i++ )
		{
			CExpressionSample *s = &m_CopyData[ t ][ i ];

			if ( t == 0 || destination->IsComboType() )
			{
				destination->AddSample( s->time, s->value, t );
			}
		}

		destination->Resort( t );
	}
	g_pChoreoView->PushRedo( "Paste" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CChoreoEvent *ExpressionTool::GetSafeEvent( void )
{
	if ( m_nFocusEventGlobalID == -1 )
		return NULL;

	if ( !g_pChoreoView )
		return NULL;

	CChoreoScene *scene = g_pChoreoView->GetScene();
	if ( !scene )
		return NULL;

	// look to see if it's focused any any event
	for ( int i = 0; i < scene->GetNumEvents() ; i++ )
	{
		CChoreoEvent *e = scene->GetEvent( i );
		if ( !e || e->GetType() != CChoreoEvent::FLEXANIMATION )
			continue;

		if ( e->GetGlobalID() == m_nFocusEventGlobalID )
		{
			DoTrackLookup( e );
			return e;
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rcHandle - 
//-----------------------------------------------------------------------------
void ExpressionTool::GetScrubHandleRect( RECT& rcHandle, bool clipped )
{
	float pixel = 0.0f;
	if ( m_pWorkspace->w2() > 0 )
	{
		pixel = GetPixelForTimeValue( m_flScrub );
		if  ( clipped )
		{
			pixel = clamp( pixel, SCRUBBER_HANDLE_WIDTH/2, w2() - SCRUBBER_HANDLE_WIDTH/2 );
		}
	}

	rcHandle.left = pixel-SCRUBBER_HANDLE_WIDTH/2;
	rcHandle.right = pixel + SCRUBBER_HANDLE_WIDTH/2;
	rcHandle.top = 2 + GetCaptionHeight();
	rcHandle.bottom = rcHandle.top + SCRUBBER_HANDLE_HEIGHT;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rcHandle - 
//-----------------------------------------------------------------------------
void ExpressionTool::DrawScrubHandle( CChoreoWidgetDrawHelper& drawHelper, RECT& rcHandle )
{
	HBRUSH br = CreateSolidBrush( RGB( 0, 150, 100 ) );

	COLORREF areaBorder = RGB( 230, 230, 220 );

	drawHelper.DrawColoredLine( areaBorder,
		PS_SOLID, 1, 0, rcHandle.top, w2(), rcHandle.top );
	drawHelper.DrawColoredLine( areaBorder,
		PS_SOLID, 1, 0, rcHandle.bottom, w2(), rcHandle.bottom );

	drawHelper.DrawFilledRect( br, rcHandle );

	// 
	char sz[ 32 ];
	sprintf( sz, "%.3f", m_flScrub );

	CChoreoEvent *ev = GetSafeEvent();
	if ( ev )
	{
		float st, ed;
		st = ev->GetStartTime();
		ed = ev->GetEndTime();

		float dt = ed - st;
		if ( dt > 0.0f )
		{
			sprintf( sz, "%.3f", st + m_flScrub );
		}
	}

	int len = drawHelper.CalcTextWidth( "Arial", 9, 500, sz );

	RECT rcText = rcHandle;

	int textw = rcText.right - rcText.left;

	rcText.left += ( textw - len ) / 2;

	drawHelper.DrawColoredText( "Arial", 9, 500, RGB( 255, 255, 255 ), rcText, sz );

	DeleteObject( br );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ExpressionTool::IsMouseOverScrubHandle( mxEvent *event )
{
	RECT rcHandle;
	GetScrubHandleRect( rcHandle, true );
	InflateRect( &rcHandle, 2, 2 );

	POINT pt;
	pt.x = (short)event->x;
	pt.y = (short)event->y;
	if ( PtInRect( &rcHandle, pt ) )
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ExpressionTool::IsProcessing( void )
{
	if ( !GetSafeEvent() )
		return false;

	if ( m_flScrub != m_flScrubTarget )
		return true;

	return false;
}

bool ExpressionTool::IsScrubbing( void ) const
{
	bool scrubbing = ( m_nDragType == DRAGTYPE_SCRUBBER ) ? true : false;
	return scrubbing;
}

void ExpressionTool::Think( float dt )
{
	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return;

	bool scrubbing = IsScrubbing();

	ScrubThink( dt, scrubbing );
}

void ExpressionTool::SetScrubTime( float t )
{
	m_flScrub = t;
	CChoreoEvent *e = GetSafeEvent();
	if ( e )
	{
		float realtime = e->GetStartTime() + m_flScrub;

		g_pChoreoView->SetScrubTime( realtime );
		g_pChoreoView->DrawScrubHandle();
	}
}

void ExpressionTool::SetScrubTargetTime( float t )
{
	m_flScrubTarget = t;
	CChoreoEvent *e = GetSafeEvent();
	if ( e )
	{
		float realtime = e->GetStartTime() + m_flScrubTarget;

		g_pChoreoView->SetScrubTargetTime( realtime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dt - 
//-----------------------------------------------------------------------------
void ExpressionTool::ScrubThink( float dt, bool scrubbing )
{
	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return;

	if ( m_flScrubTarget == m_flScrub && !scrubbing )
		return;

	float d = m_flScrubTarget - m_flScrub;
	int sign = d > 0.0f ? 1 : -1;

	float maxmove = dt;

	if ( sign > 0 )
	{
		if ( d < maxmove )
		{
			SetScrubTime( m_flScrubTarget );
		}
		else
		{
			SetScrubTime( m_flScrub + maxmove );
		}
	}
	else
	{
		if ( -d < maxmove )
		{
			SetScrubTime( m_flScrubTarget );
		}
		else
		{
			SetScrubTime( m_flScrub - maxmove );
		}
	}

	if ( scrubbing )
	{
		g_pMatSysWindow->Frame();
	}
}

void ExpressionTool::redraw()
{
	if ( !ToolCanDraw() )
		return;

	CChoreoWidgetDrawHelper drawHelper( this );
	HandleToolRedraw( drawHelper );

	COLORREF areaBorder = RGB( 230, 230, 220 );

	RECT rcSelection;
	GetWorkspaceRect( rcSelection );

	drawHelper.DrawColoredLine( areaBorder,
		PS_SOLID, 1, 0, rcSelection.top, w2(), rcSelection.top );
	drawHelper.DrawColoredLine( areaBorder,
		PS_SOLID, 1, 0, rcSelection.bottom, w2(), rcSelection.bottom );

	if ( m_bSelectionActive )
	{
		RECT rcClient;
		drawHelper.GetClientRect( rcClient );

		int left, right;
		left = GetPixelForTimeValue( m_flSelection[ 0 ] );
		right = GetPixelForTimeValue( m_flSelection[ 1 ] );

		rcSelection.left = left;
		rcSelection.right = right;
		rcSelection.bottom = TRAY_HEIGHT;
		
		drawHelper.DrawFilledRect( RGB( 200, 220, 230 ), rcSelection );

		drawHelper.DrawColoredLine( RGB( 100, 100, 255 ), PS_SOLID, 3, rcSelection.left, rcSelection.top, rcSelection.left, rcSelection.bottom );
		drawHelper.DrawColoredLine( RGB( 100, 100, 255 ), PS_SOLID, 3, rcSelection.right, rcSelection.top, rcSelection.right, rcSelection.bottom );

	}

	CChoreoEvent *ev = GetSafeEvent();
	if ( ev )
	{
		RECT rcText;
		drawHelper.GetClientRect( rcText );
		rcText.top += GetCaptionHeight()+1;
		rcText.bottom = rcText.top + 13;
		rcText.left += 5;
		rcText.right -= 5;

		OffsetRect( &rcText, 0, 12 );

		int current, total;

		g_pChoreoView->GetUndoLevels( current, total );
		if ( total > 0 )
		{
			RECT rcUndo = rcText;
			OffsetRect( &rcUndo, 0, 2 );

			drawHelper.DrawColoredText( "Small Fonts", 8, FW_NORMAL, RGB( 0, 100, 0 ), rcUndo,
				"Undo:  %i/%i", current, total );
		}

		rcText.left += 60;
		
		// Found it, write out description
		// 
		drawHelper.DrawColoredText( "Arial", 11, 900, RGB( 200, 150, 100 ), rcText,
			"Event:  %s",
			ev->GetName() );

		OffsetRect( &rcText, 0, 30 );

		rcText.left = 5;

		RECT timeRect = rcText;

		timeRect.right = timeRect.left + 100;

		char sz[ 32 ];

		float st, ed;

		GetStartAndEndTime( st, ed );

		st += ev->GetStartTime();
		ed += ev->GetStartTime();

		Q_snprintf( sz, sizeof( sz ), "%.2f", st );

		drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 0, 0, 0 ), timeRect, sz );

		timeRect = rcText;

		Q_snprintf( sz, sizeof( sz ), "%.2f", ed );

		int textW = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, sz );

		timeRect.right = w2() - 10;
		timeRect.left = timeRect.right - textW;

		drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 0, 0, 0 ), timeRect, sz );
	}

	RECT rcHandle;
	GetScrubHandleRect( rcHandle, true );
	DrawScrubHandle( drawHelper, rcHandle );

	DrawRelativeTags( drawHelper );

	RECT rcPos;
	GetMouseOverPosRect( rcPos );
	DrawMouseOverPos( drawHelper, rcPos );

	DrawEventEnd( drawHelper );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *tag - 
//-----------------------------------------------------------------------------
bool ExpressionTool::GetTimingTagRect( RECT& rcClient, CChoreoEvent *event, CFlexTimingTag *tag, RECT& rcTag )
{
	rcTag = rcClient;

	int tagx = GetPixelForTimeValue( tag->GetStartTime() - event->GetStartTime() );

	rcTag.top		= rcClient.bottom - 6;
	rcTag.bottom	= rcTag.top + 6;
	rcTag.left		= tagx - 3;
	rcTag.right		= tagx + 3;

	return true;
}

// Get workspace min, max point in terms of tool window
void ExpressionTool::GetWorkspaceLeftRight( int& left, int& right )
{
	POINT pt;
	pt.x = TRAY_ITEM_INSET;
	pt.y = 0;

	ClientToScreen( (HWND)m_pWorkspace->getHandle(), &pt );
	ScreenToClient( (HWND)getHandle(), &pt );

	left = (short)pt.x;

	pt.x = m_pWorkspace->w2() - TRAY_ITEM_INSET - 12;
	pt.y = 0;

	ClientToScreen( (HWND)m_pWorkspace->getHandle(), &pt );
	ScreenToClient( (HWND)getHandle(), &pt );

	right = (short)pt.x;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
// Output : CFlexTimingTag
//-----------------------------------------------------------------------------
CFlexTimingTag *ExpressionTool::IsMouseOverTag( int mx, int my )
{
	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return NULL;

	RECT rcClient;
	GetClientRect( (HWND)getHandle(), &rcClient );

	int left, right;

	GetWorkspaceLeftRight( left, right );

	rcClient.left	= left;
	rcClient.right	= right;
	rcClient.top	= GetCaptionHeight();
	rcClient.bottom = rcClient.top + TRAY_HEIGHT;

	POINT pt;
	pt.x = mx;
	pt.y = my;

	for ( int i = 0 ; i < event->GetNumTimingTags(); i++ )
	{
		CFlexTimingTag *tag = event->GetTimingTag( i );
		if ( !tag )
			continue;

		RECT rcTag;

		if ( !GetTimingTagRect( rcClient, event, tag, rcTag ) )
			continue;

		if ( !PtInRect( &rcTag, pt ) )
			continue;

		return tag;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//-----------------------------------------------------------------------------
void ExpressionTool::DrawRelativeTags( CChoreoWidgetDrawHelper& drawHelper )
{
	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return;

	float st, ed;
	GetStartAndEndTime( st, ed );

	if ( event->GetDuration() <= 0.0f )
		return;

	CChoreoScene *scene = g_pChoreoView->GetScene();
	if ( !scene )
		return;

	RECT rcClient;
	drawHelper.GetClientRect( rcClient );

	int left, right;

	GetWorkspaceLeftRight( left, right );

	rcClient.top += GetCaptionHeight();

	rcClient.left	= left;
	rcClient.right	= right;

	rcClient.bottom = rcClient.top + TRAY_HEIGHT;

	// Iterate relative tags
	for ( int i = 0; i < scene->GetNumActors(); i++ )
	{
		CChoreoActor *a = scene->GetActor( i );
		if ( !a )
			continue;

		for ( int j = 0; j < a->GetNumChannels(); j++ )
		{
			CChoreoChannel *c = a->GetChannel( j );
			if ( !c )
				continue;

			for ( int k = 0 ; k < c->GetNumEvents(); k++ )
			{
				CChoreoEvent *e = c->GetEvent( k );
				if ( !e )
					continue;

				// add each tag to combo box
				for ( int t = 0; t < e->GetNumRelativeTags(); t++ )
				{
					CEventRelativeTag *tag = e->GetRelativeTag( t );
					if ( !tag )
						continue;

					//SendMessage( control, CB_ADDSTRING, 0, (LPARAM)va( "\"%s\" \"%s\"", tag->GetName(), e->GetParameters() ) ); 
					bool clipped;
					int tagx = GetPixelForTimeValue( tag->GetStartTime() - event->GetStartTime(), &clipped );
					if ( clipped )
						continue;

					//drawHelper.DrawColoredLine( RGB( 180, 180, 220 ), PS_SOLID, 1, tagx, rcClient.top, tagx, rcClient.bottom );
					
					RECT rcMark;
					rcMark = rcClient;
					rcMark.top = rcClient.bottom - 6;
					rcMark.left = tagx - 3;
					rcMark.right = tagx + 3;
					
					drawHelper.DrawTriangleMarker( rcMark, RGB( 0, 100, 250 ) );
					
					RECT rcText;
					rcText = rcMark;
					rcText.top -= 10;
					
					int len = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, tag->GetName() );
					rcText.left = tagx - len / 2;
					rcText.right = rcText.left + len + 2;
					
					rcText.bottom = rcText.top + 10;
					
					drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 0, 100, 200 ), rcText, tag->GetName() );

				}
			}
		}
	}

	for ( int t = 0; t < event->GetNumTimingTags(); t++ )
	{
		CFlexTimingTag *tag = event->GetTimingTag( t );
		if ( !tag )
			continue;

		RECT rcMark;

		if ( !GetTimingTagRect( rcClient, event, tag, rcMark ) )
			continue;

		drawHelper.DrawTriangleMarker( rcMark, RGB( 250, 100, 0 ) );
		
		RECT rcText;
		rcText = rcMark;
		rcText.top -= 20;
		
		char text[ 256 ];
		sprintf( text, "%s", tag->GetName() );
		if ( tag->GetLocked() )
		{
			strcat( text, " - locked" );
		}

		int len = drawHelper.CalcTextWidth( "Arial", 9, FW_NORMAL, text );
		rcText.left = ( rcMark.left + rcMark.right ) / 2 - len / 2;
		rcText.right = rcText.left + len + 2;
		
		rcText.bottom = rcText.top + 10;
		
		drawHelper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 200, 100, 0 ), rcText, text );

	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ExpressionTool::ShowContextMenu( mxEvent *event, bool include_track_menus )
{
	// Construct main menu
	mxPopupMenu *pop = new mxPopupMenu();

	TimelineItem *item = NULL;
	CFlexAnimationTrack *track = NULL;

	if ( include_track_menus )
	{
		item = m_pWorkspace->GetClickedItem();
		if ( item )
		{
			item->CountSelected();
			track = item->GetSafeTrack();
		}
	}

	int current, total;
	g_pChoreoView->GetUndoLevels( current, total );
	if ( total > 0 )
	{
		if ( current > 0 )
		{
			pop->add( va( "Undo %s", g_pChoreoView->GetUndoDescription() ), IDC_UNDO_FA );
		}
		
		if ( current <= total - 1 )
		{
			pop->add( va( "Redo %s", g_pChoreoView->GetRedoDescription() ), IDC_REDO_FA );
		}
		pop->addSeparator();
	}

	// Create expand menu
	mxPopupMenu *expand = new mxPopupMenu();
	if ( item && track && item->IsCollapsed() )
	{
		expand->add( va( "Track '%s'", track->GetFlexControllerName() ), IDC_TL_EXPAND );
	}
	expand->add( "All tracks", IDC_EXPANDALL );
	expand->add( "Used tracks", IDC_EXPANDVALID );

	pop->addMenu( "Expand", expand );

	mxPopupMenu *collapse = new mxPopupMenu;

	if ( item && track && !item->IsCollapsed() )
	{
		collapse->add( va( "Track '%s'", track->GetFlexControllerName() ), IDC_TL_COLLAPSE );
		collapse->add( va( "All tracks except '%s'", track->GetFlexControllerName() ), IDC_COLLAPSE_ALL_EXCEPT );
	}

	collapse->add( "All tracks", IDC_COLLAPSEALL );

	pop->addMenu( "Collapse", collapse );

	pop->addSeparator();

	pop->add( va( "Enable all valid" ), IDC_ENABLE_ALL_VALID );
	
	if ( item && track )
	{
		if ( item->IsActive() )
		{
			pop->add( va( "Disable '%s'", track->GetFlexControllerName() ), IDC_TL_DISABLE );
		}
		else
		{
			pop->add( va( "Enable '%s'", track->GetFlexControllerName() ), IDC_TL_ENABLE );
		}
		pop->add( va( "Disable all except '%s'", track->GetFlexControllerName() ), IDC_DISABLE_ALL_EXCEPT );

		pop->addSeparator();
		pop->add( "Copy", IDC_TL_COPY );
		if ( HasCopyData() )
		{
			pop->add( "Paste", IDC_TL_PASTE );
		}

		pop->addSeparator();
		if ( item->GetNumSelected() > 0 )
		{
			pop->add( va( "Delete" ), IDC_TL_DELETE );
			pop->add( "Deselect all", IDC_TL_DESELECT );
			pop->add( va( "Scale selected..." ), IDC_FLEX_SCALESAMPLES );
		}
		pop->add( "Select all", IDC_TL_SELECTALL );

		if ( FacePoser_IsSnapping() )
		{
			mxPopupMenu *snap = new mxPopupMenu();

			snap->add( va( "All points" ), IDC_TL_SNAPALL );
			snap->add( va( "All points in '%s'", track->GetFlexControllerName() ), IDC_TL_SNAPPOINTS );
			snap->add( va( "Selected points in '%s'", track->GetFlexControllerName() ), IDC_TL_SNAPSELECTED );

			pop->addSeparator();

			pop->addMenu( "Snap", snap );
		}

		if ( track->IsComboType() )
		{
			pop->addSeparator();

			if ( item->GetEditType() == 0 )
			{
				pop->add( "Edit <left/right>", IDC_TL_EDITLEFTRIGHT );
			}
			else
			{
				pop->add( "Edit <amount>", IDC_TL_EDITNORMAL );
			}
		}

		pop->addSeparator();
		mxPopupMenu *heightMenu = new mxPopupMenu();
		heightMenu->add( va( "Reset '%s'", track->GetFlexControllerName() ) , IDC_ET_RESET_ITEM_SIZE );
		heightMenu->add( "Reset All", IDC_ET_RESET_ALL_ITEM_SIZES );
		pop->addMenu( "Height", heightMenu );

		pop->addSeparator();
		pop->add( "Edge Properties...", IDC_ET_EDGEPROPERTIES );
	}
	pop->addSeparator();

	mxPopupMenu *tagmenu = new mxPopupMenu();

	CFlexTimingTag *tag = IsMouseOverTag( (short)event->x, (short)event->y );
	if ( tag )
	{
		if ( tag->GetLocked() )
		{
			tagmenu->add( va( "Unlock tag '%s'...", tag->GetName() ), IDC_UNLOCK_TIMING_TAG );
		}
		else
		{
			tagmenu->add( va( "Lock tag '%s'...", tag->GetName() ), IDC_LOCK_TIMING_TAG );
		}
		tagmenu->addSeparator();
		tagmenu->add( va( "Delete tag '%s'...", tag->GetName() ), IDC_DELETE_TIMING_TAG );
	}
	else
	{
		tagmenu->add( "Insert...", IDC_INSERT_TIMING_TAG );
	}

	bool bMouseOverSelection = IsMouseOverSelection( (short)event->x, (short)event->y );

	if ( bMouseOverSelection || HasCopiedColumn() )
	{
		mxPopupMenu *selectionMenu = new mxPopupMenu();

		if ( bMouseOverSelection ) 
		{
			selectionMenu->add( "Copy samples", IDC_ET_SELECTION_COPY );
		}

		if ( HasCopiedColumn() )
		{
			selectionMenu->add( "Paste samples", IDC_ET_SELECTION_PASTE );
		}

		if ( bMouseOverSelection )
		{
			selectionMenu->addSeparator();
			selectionMenu->add( "Delete samples", IDC_ET_SELECTION_DELETE );
			selectionMenu->add( "Delete samples and shift remainder", IDC_ET_SELECTION_EXCISE );
		}
		pop->addMenu( "Column", selectionMenu );
	}



	pop->addMenu( "Timing Tags", tagmenu );

	if ( FacePoser_IsSnapping() )
	{
		pop->addSeparator();
		pop->add( "Delete keys by frame", IDC_TL_DELETECOLUMN );
		pop->addSeparator();
	}

	mxPopupMenu *flexmenu = new mxPopupMenu();

	flexmenu->add( "Copy to sliders", IDC_COPY_TO_FLEX );
	flexmenu->add( "Copy from sliders", IDC_COPY_FROM_FLEX );
	pop->addMenu( "Flex", flexmenu );

	pop->add( "Create expression...", IDC_NEW_EXPRESSION_FROM_FLEXANIMATION ); 

	CChoreoEvent *e = GetSafeEvent();
	if ( e )
	{
		mxPopupMenu *importexport = new mxPopupMenu();

		importexport->add( "Export flex animation...", IDC_EXPORT_FA );
		importexport->add( "Import flex animation...", IDC_IMPORT_FA );

		pop->addMenu( "Import/Export", importexport );
	}

	pop->add( va( "Change scale..." ), IDC_FLEX_CHANGESCALE );

	mxPopupMenu *sortmenu = new mxPopupMenu();
	sortmenu->add( "Sort by name", IDC_ET_SORT_BY_NAME );
	sortmenu->add( "Sort by used", IDC_ET_SORT_BY_USED );

	pop->addMenu( "Sort", sortmenu );

	pop->popup( this, (short)event->x, (short)event->y );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ExpressionTool::DrawFocusRect( void )
{
	HDC dc = GetDC( NULL );

	for ( int i = 0; i < m_FocusRects.Size(); i++ )
	{
		RECT rc = m_FocusRects[ i ].m_rcFocus;

		::DrawFocusRect( dc, &rc );
	}

	ReleaseDC( NULL, dc );
}

void ExpressionTool::SetClickedPos( int x, int y )
{
	m_nClickedX = x;
	m_nClickedY = y;
}

float ExpressionTool::GetTimeForClickedPos( void )
{
	CChoreoEvent *e = GetSafeEvent();
	if ( !e )
		return 0.0f;

	float t = GetTimeValueForMouse( m_nClickedX );

	// Get spline intensity for controller
	float faketime = e->GetStartTime() + t;
	return faketime;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dragtype - 
//			startx - 
//			cursor - 
//-----------------------------------------------------------------------------
void ExpressionTool::StartDragging( int dragtype, int startx, int starty, HCURSOR cursor )
{
	m_nDragType = dragtype;
	m_nStartX	= startx;
	m_nLastX	= startx;
	m_nStartY	= starty;
	m_nLastY	= starty;
	
	if ( m_hPrevCursor )
	{
		SetCursor( m_hPrevCursor );
		m_hPrevCursor = NULL;
	}
	m_hPrevCursor = SetCursor( cursor );

	m_FocusRects.Purge();

	RECT rc;
	GetWorkspaceRect( rc );

	RECT rcStart;
	rcStart.left = startx;
	rcStart.right = startx;

	bool addrect = true;
	switch ( dragtype )
	{
	default:
	case DRAGTYPE_SCRUBBER:
		{
			RECT rcScrub;
			GetScrubHandleRect( rcScrub, true );

			rcStart = rcScrub;
			rcStart.left = ( rcScrub.left + rcScrub.right ) / 2;
			rcStart.right = rcStart.left;
			rcStart.top = rcScrub.bottom;

			rcStart.bottom = h2();
		}
		break;
	case DRAGTYPE_FLEXTIMINGTAG:
		{
			rcStart.top = rc.top;
			rcStart.bottom = h2();
		}
		break;
	case DRAGTYPE_SELECTSAMPLES:
		{
			float st = GetTimeValueForMouse( startx );
			rcStart.left = GetPixelForTimeValue( st );
			rcStart.right = rcStart.left;

			m_nStartX	= rcStart.left;
			m_nLastX	= rcStart.left;
		}
	case DRAGTYPE_MOVESELECTIONSTART:
	case DRAGTYPE_MOVESELECTIONEND:
		{
			rcStart.top = rc.top;
			rcStart.bottom = rc.bottom;
		}
		break;
	case DRAGTYPE_MOVESELECTION:
		{
			rcStart.top = rc.top;
			rcStart.bottom = rc.bottom;

			// Compute left/right pixels for selection
			rcStart.left = GetPixelForTimeValue( m_flSelection[ 0 ] );
			rcStart.right = GetPixelForTimeValue( m_flSelection[ 1 ] );
		}
		break;
	}


	if ( addrect )
	{
		AddFocusRect( rcStart );
	}
	
	DrawFocusRect();
}

void ExpressionTool::OnMouseMove( mxEvent *event )
{
	int mx = (short)event->x;
	int my = (short)event->y;

	event->x = (short)mx;

	if ( m_nDragType != DRAGTYPE_NONE )
	{
		DrawFocusRect();

		for ( int i = 0; i < m_FocusRects.Size(); i++ )
		{
			CFocusRect *f = &m_FocusRects[ i ];
			f->m_rcFocus = f->m_rcOrig;

			switch ( m_nDragType )
			{
			default:
				{
					OffsetRect( &f->m_rcFocus, ( (short)event->x - m_nStartX ),	0 );
				}
				break;
			case DRAGTYPE_SELECTSAMPLES:
				{
					float st = GetTimeValueForMouse( mx );
					int snapx = GetPixelForTimeValue( st );
					f->m_rcFocus.left = min( snapx, m_nStartX );
					f->m_rcFocus.right = max( snapx, m_nStartY );

					POINT offset;
					offset.x = 0;
					offset.y = 0;
					ClientToScreen( (HWND)getHandle(), &offset );
					OffsetRect( &f->m_rcFocus, offset.x, 0 );

				}
				break;
			}
		}

		DrawFocusRect();
	}
	else
	{
		if ( m_hPrevCursor )
		{
			SetCursor( m_hPrevCursor );
			m_hPrevCursor = NULL;
		}

		if ( IsMouseOverScrubHandle( event ) )
		{
			m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );
		}
		else if ( IsMouseOverTag( mx, my ) )
		{
			m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );
		}
		else if ( IsMouseOverSelection( (short)event->x, (short)event->y ) )
		{
			if ( IsMouseOverSelectionStartEdge( event ) )
			{
				m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );
			}
			else if ( IsMouseOverSelectionEndEdge( event ) )
			{
				m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEWE ) );
			}
			else
			{
				if ( event->modifiers & mxEvent::KeyShift )
				{
					m_hPrevCursor = SetCursor( LoadCursor( NULL, IDC_SIZEALL ) );
				}
			}
		}
	}

	switch ( m_nDragType )
	{
	default:
		break;
	case DRAGTYPE_FLEXTIMINGTAG:
		{
			ApplyBounds( mx, my );
		}
		break;
	case DRAGTYPE_SCRUBBER:
		{
			ApplyBounds( mx, my );
			if ( w2() > 0 )
			{
				float t = GetTimeValueForMouse( mx );
				t += m_flScrubberTimeOffset;
				ForceScrubPosition( t );
			}
		}
		break;
	}

	m_nLastX = (short)event->x;
	m_nLastY = (short)event->y;
}

int	ExpressionTool::handleEvent( mxEvent *event )
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	int iret = 0;

	if ( HandleToolEvent( event ) )
	{
		return iret;
	}

	switch ( event->event )
	{
	case mxEvent::Size:
		{
			int w, h;
			w = event->width;
			h = event->height;

			m_pWorkspace->setBounds( 5, TRAY_HEIGHT + GetCaptionHeight(), w - 10, h - ( TRAY_HEIGHT + 5 + GetCaptionHeight() ) - m_nScrollbarHeight );

			m_nLastHPixelsNeeded = 0;
			InvalidateLayout();

			iret = 1;
		}
		break;
	case mxEvent::MouseWheeled:
		{
			CChoreoScene *scene = g_pChoreoView->GetScene();
			if ( scene )
			{
				int tz = g_pChoreoView->GetTimeZoom( GetToolName() );
				bool shiftdown = ( event->modifiers & mxEvent::KeyShift ) ? true : false;
				int stepMultipiler = shiftdown ? 5 : 1;

				// Zoom time in  / out
				if ( event->height > 0 )
				{
					tz = min( tz + TIME_ZOOM_STEP * stepMultipiler, MAX_TIME_ZOOM );
				}
				else
				{
					tz = max( tz - TIME_ZOOM_STEP * stepMultipiler, TIME_ZOOM_STEP );
				}

				g_pChoreoView->SetPreservedTimeZoom( this, tz );
			}
			//RepositionHSlider();
			m_pWorkspace->redraw();
			redraw();
			iret = 1;
		}
		break;
	case mxEvent::MouseDown:
		{
//			bool ctrldown = ( event->modifiers & mxEvent::KeyCtrl ) ? true : false;
			bool shiftdown = ( event->modifiers & mxEvent::KeyShift ) ? true : false;

			iret = 1;

			int mx = (short)event->x;
			int my = (short)event->y;

			SetClickedPos( mx, my );

			SetMouseOverPos( mx, my );
			DrawMouseOverPos();

			if ( event->buttons & mxEvent::MouseRightButton )
			{
				ShowContextMenu( event, false );
				return iret;
			}
		
			if ( m_nDragType == DRAGTYPE_NONE )
			{
				if ( IsMouseOverScrubHandle( event ) )
				{
					if ( w2() > 0 )
					{
						float t = GetTimeValueForMouse( (short)event->x );
						m_flScrubberTimeOffset = m_flScrub - t;
						float maxoffset = 0.5f * (float)SCRUBBER_HANDLE_WIDTH / GetPixelsPerSecond();
						m_flScrubberTimeOffset = clamp( m_flScrubberTimeOffset, -maxoffset, maxoffset );
						t += m_flScrubberTimeOffset;
						ForceScrubPosition( t );
					}

					StartDragging( DRAGTYPE_SCRUBBER, m_nClickedX, m_nClickedY, LoadCursor( NULL, IDC_SIZEWE ) );
				}
				else if ( IsMouseOverTag( m_nClickedX, m_nClickedY ) )
				{
					StartDragging( DRAGTYPE_FLEXTIMINGTAG, m_nClickedX, m_nClickedY, LoadCursor( NULL, IDC_SIZEWE ) );
				}
				else if ( IsMouseOverPoints( m_nClickedX, m_nClickedY ) )
				{
					if ( !m_bSelectionActive )
					{
						StartDragging( DRAGTYPE_SELECTSAMPLES, m_nClickedX, m_nClickedY, LoadCursor( NULL, IDC_SIZEWE ) );
					}
					else
					{
						// Either move, move edge if ctrl key is held, or deselect
						if ( IsMouseOverSelection( m_nClickedX,m_nClickedY ) )
						{
							if ( IsMouseOverSelectionStartEdge( event ) )
							{
								StartDragging( DRAGTYPE_MOVESELECTIONSTART, m_nClickedX, m_nClickedY, LoadCursor( NULL, IDC_SIZEWE ) );
							}
							else if ( IsMouseOverSelectionEndEdge( event ) )
							{
								StartDragging( DRAGTYPE_MOVESELECTIONEND, m_nClickedX, m_nClickedY, LoadCursor( NULL, IDC_SIZEWE ) );
							}
							else
							{
								if ( shiftdown )
								{
									StartDragging( DRAGTYPE_MOVESELECTION, m_nClickedX, m_nClickedY, LoadCursor( NULL, IDC_SIZEALL ) );
								}
							}
						}
						else
						{
							m_bSelectionActive = false;
							redraw();
							return iret;
						}
					}
				}
				else
				{
					if ( w2() > 0 )
					{
						float t = GetTimeValueForMouse( (short)event->x );

						SetScrubTargetTime( t );
					}
				}

				CalcBounds( m_nDragType );
			}
		}
		break;
	case mxEvent::MouseDrag:
	case mxEvent::MouseMove:
		{
			int mx = (short)event->x;
			int my = (short)event->y;

			SetMouseOverPos( mx, my );
			DrawMouseOverPos();

			OnMouseMove( event );

			iret = 1;
		}
		break;
	case mxEvent::MouseUp:
		{
			if ( event->buttons & mxEvent::MouseRightButton )
			{
				return 1;
			}

			int mx = (short)event->x;
			int my = (short)event->y;

			if ( m_nDragType != DRAGTYPE_NONE )
			{
				DrawFocusRect();
			}

			if ( m_hPrevCursor )
			{
				SetCursor( m_hPrevCursor );
				m_hPrevCursor = 0;
			}

			switch ( m_nDragType )
			{
			case DRAGTYPE_NONE:
				break;
			case DRAGTYPE_SELECTSAMPLES:
				FinishSelect( m_nStartX, mx );
				break;
			case DRAGTYPE_MOVESELECTION:
				FinishMoveSelection( m_nStartX, mx );
				break;
			case DRAGTYPE_MOVESELECTIONSTART:
				FinishMoveSelectionStart( m_nStartX, mx );
				break;
			case DRAGTYPE_MOVESELECTIONEND:
				FinishMoveSelectionEnd( m_nStartX, mx );
				break;
			case DRAGTYPE_SCRUBBER:
				{
					ApplyBounds( mx, my );

//					int dx = mx - m_nStartX;
//					int dy = my = m_nStartY;

					if ( w2() > 0 )
					{
						float t = GetTimeValueForMouse( (short)event->x );
						t += m_flScrubberTimeOffset;
						m_flScrubberTimeOffset = 0.0f;
						ForceScrubPosition( t );
					}
				}
				break;
			case DRAGTYPE_FLEXTIMINGTAG:
				{
					ApplyBounds( mx, my );

//					int dx = mx - m_nStartX;
//					int dy = my = m_nStartY;

					// Compute dx, dy and apply to sections
					//Con_Printf( "dx == %i\n", dx );
					CFlexTimingTag *tag = IsMouseOverTag( m_nStartX, m_nStartY );
					CChoreoEvent *ev = GetSafeEvent();
					if ( tag && g_pChoreoView && ev && ev->GetDuration() )
					{
						float t = GetTimeValueForMouse( mx );

						float percent = t / ev->GetDuration();

						g_pChoreoView->SetDirty( true );

						g_pChoreoView->PushUndo( "Move Timing Tag" );

						if ( tag->GetLocked() )
						{
							// Resample all control points on right/left
							//  of locked tags all the way to the next lock or edge
							ResampleControlPoints( tag, percent );
						}

						tag->SetPercentage( percent );

						g_pChoreoView->PushRedo( "Move Timing Tag" );
					}

					LayoutItems( true );
					redraw();
				}
				break;
			}

			m_nDragType = DRAGTYPE_NONE;

			SetMouseOverPos( mx, my );
			DrawMouseOverPos();

			iret = 1;
		}
		break;
	case mxEvent::Action:
		{
			iret = 1;
			switch ( event->action )
			{
			default:
				iret = 0;
				break;
			case IDC_ET_RESET_ITEM_SIZE:
				OnResetItemSize();
				break;
			case IDC_ET_RESET_ALL_ITEM_SIZES:
				OnResetAllItemSizes();
				break;
			case IDC_ET_SELECTION_DELETE:
				OnDeleteSelection( false );
				break;
			case IDC_ET_SELECTION_EXCISE:
				OnDeleteSelection( true );
				break;
			case IDC_ET_SELECTION_COPY:
				OnCopyColumn();
				break;
			case IDC_ET_SELECTION_PASTE:
				OnPasteColumn();
				break;
			case IDC_ET_SORT_BY_USED:
				OnSortByUsed();
				break;
			case IDC_ET_SORT_BY_NAME:
				OnSortByName();
				break;
			case IDC_EXPORT_FA:
				OnExportFlexAnimation();
				break;
			case IDC_IMPORT_FA:
				OnImportFlexAnimation();
				break;
			case IDC_LOCK_TIMING_TAG:
				LockTimingTag();
				break;
			case IDC_UNLOCK_TIMING_TAG:
				UnlockTimingTag();
				break;
			case IDC_DELETE_TIMING_TAG:
				DeleteFlexTimingTag( m_nClickedX, m_nClickedY );
				break;
			case IDC_INSERT_TIMING_TAG:
				AddFlexTimingTag( m_nClickedX );
				break;
			case IDC_EXPANDALL:
				m_pWorkspace->ExpandAll();
				break;
			case IDC_COLLAPSEALL:
				m_pWorkspace->CollapseAll( NULL );
				break;
			case IDC_COLLAPSE_ALL_EXCEPT:
				m_pWorkspace->CollapseAll( m_pWorkspace->GetClickedItem() );
				break;
			case IDC_EXPANDVALID:
				m_pWorkspace->ExpandValid();
				break;
			case IDC_COPY_TO_FLEX:
				OnCopyToFlex( true );
				break;
			case IDC_COPY_FROM_FLEX:
				OnCopyFromFlex( false );
				break;
			case IDC_NEW_EXPRESSION_FROM_FLEXANIMATION:
				OnNewExpression();
				break;
			case IDC_UNDO_FA:
				OnUndo();
				break;
			case IDC_REDO_FA:
				OnRedo();
				break;
			case IDC_TL_EDITNORMAL:
				{
					TimelineItem *item = m_pWorkspace->GetClickedItem();
					if ( item )
					{
						item->SetEditType( 0 );
						item->DrawSelf();
					}
				}
				break;
			case IDC_TL_EDITLEFTRIGHT:
				{
					TimelineItem *item = m_pWorkspace->GetClickedItem();
					if ( item )
					{
						item->SetEditType( 1 );
						item->DrawSelf();
					}
				}
				break;
			case IDC_TL_EXPAND:
				{
					TimelineItem *item = m_pWorkspace->GetClickedItem();
					if ( item )
					{
						item->SetCollapsed( false );
					}
					LayoutItems();
				}
				break;
			case IDC_TL_COLLAPSE:
				{
					TimelineItem *item = m_pWorkspace->GetClickedItem();
					if ( item )
					{
						item->SetCollapsed( true );
					}
					LayoutItems();
				}
				break;
			case IDC_TL_ENABLE:
				{
					TimelineItem *item = m_pWorkspace->GetClickedItem();
					if ( item )
					{
						g_pChoreoView->SetDirty( true );
						g_pChoreoView->PushUndo( "Enable item" );

						item->SetActive( true );

						g_pChoreoView->PushRedo( "Enable item" );

						item->DrawSelf();
					}
				}
				break;
			case IDC_TL_DISABLE:
				{
					TimelineItem *item = m_pWorkspace->GetClickedItem();
					if ( item )
					{
						g_pChoreoView->SetDirty( true );
						g_pChoreoView->PushUndo( "Disable item" );

						item->SetActive( false );

						g_pChoreoView->PushRedo( "Disable item" );

						item->DrawSelf();
					}
				}
				break;
			case IDC_TL_COPY:
				{
					TimelineItem *item = m_pWorkspace->GetClickedItem();
					if ( item )
					{
						item->Copy();
					}
				}
				break;
			case IDC_TL_PASTE:
				{
					TimelineItem *item = m_pWorkspace->GetClickedItem();
					if ( item )
					{
						item->Paste();
						item->DrawSelf();
					}
				}
				break;
			case IDC_TL_DELETE:
				{
					TimelineItem *item = m_pWorkspace->GetClickedItem();
					if ( item )
					{
						item->Delete();
						item->DrawSelf();
					}
				}
				break;
			case IDC_TL_DESELECT:
				{
					TimelineItem *item = m_pWorkspace->GetClickedItem();
					if ( item )
					{
						item->DeselectAll();
						item->DrawSelf();
					}
				}
				break;
			case IDC_TL_SELECTALL:
				{
					TimelineItem *item = m_pWorkspace->GetClickedItem();
					if ( item )
					{
						item->SelectAll();
						item->DrawSelf();
					}
				}
				break;
			case IDC_DISABLE_ALL_EXCEPT:
				{
					m_pWorkspace->DisableAllExcept();
				}
				break;
			case IDC_ENABLE_ALL_VALID:
				{
					m_pWorkspace->EnableValid();
				}
				break;
			case IDC_TL_SNAPSELECTED:
				{
					TimelineItem *item = m_pWorkspace->GetClickedItem();
					if ( item )
					{
						g_pChoreoView->SetDirty( true );
						g_pChoreoView->PushUndo( "Snap Selected" );

						item->SnapSelected();

						g_pChoreoView->PushRedo( "Snap Selected" );
						item->DrawSelf();
					}
				}
				break;
			case IDC_TL_SNAPPOINTS:
				{
					TimelineItem *item = m_pWorkspace->GetClickedItem();
					if ( item )
					{
						g_pChoreoView->SetDirty( true );
						g_pChoreoView->PushUndo( "Snap Item" );

						item->SnapAll();

						g_pChoreoView->PushRedo( "Snap Item" );
						item->DrawSelf();
					}
				}
				break;
			case IDC_TL_DELETECOLUMN:
				{
					m_pWorkspace->OnDeleteColumn();
				}
				break;
			case IDC_TL_SNAPALL:
				{
					m_pWorkspace->OnSnapAll();
				}
				break;
			case IDC_FLEXHSCROLL:
				{
					int offset = 0;
					bool processed = true;

					switch ( event->modifiers )
					{
					case SB_THUMBTRACK:
						offset = event->height;
						break;
					case SB_PAGEUP:
						offset = m_pHorzScrollBar->getValue();
						offset -= 20;
						offset = max( offset, m_pHorzScrollBar->getMinValue() );
						break;
					case SB_PAGEDOWN:
						offset = m_pHorzScrollBar->getValue();
						offset += 20;
						offset = min( offset, m_pHorzScrollBar->getMaxValue() );
						break;
					case SB_LINEUP:
						offset = m_pHorzScrollBar->getValue();
						offset -= 10;
						offset = max( offset, m_pHorzScrollBar->getMinValue() );
						break;
					case SB_LINEDOWN:
						offset = m_pHorzScrollBar->getValue();
						offset += 10;
						offset = min( offset, m_pHorzScrollBar->getMaxValue() );
						break;
					default:
						processed = false;
						break;
					}

					if ( processed )
					{
						MoveTimeSliderToPos( offset );
					}
				}
				break;
			case IDC_FLEX_CHANGESCALE:
				{
					OnChangeScale();
				}
				break;
			case IDC_FLEX_SCALESAMPLES:
				{
					OnScaleSamples();
				}
				break;
			case IDC_ET_EDGEPROPERTIES:
				{
					OnEdgeProperties();
				}
				break;
			}
		}
		break;
	case mxEvent::KeyDown:
	case mxEvent::KeyUp:
		{
			TimelineItem *item = m_pWorkspace->GetClickedItem();
			if ( item )
			{
				iret = item->handleEvent( event );					
			}

			if ( !iret )
			{
				switch ( event->key )
				{
				default:
					break;
				case VK_ESCAPE:
					{
						DeselectAll();
						iret = 1;
					}
					break;
				}
			}
		}
		break;
	}
	return iret;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : false - 
//-----------------------------------------------------------------------------
void ExpressionTool::LayoutItems( bool force /*= false*/ )
{
	m_pWorkspace->LayoutItems( force );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
//-----------------------------------------------------------------------------
void ExpressionTool::AddFlexTimingTag( int mx )
{
	Assert( g_pChoreoView );

	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return;

	if ( event->GetType() != CChoreoEvent::FLEXANIMATION )
	{
		Con_ErrorPrintf( "Timing Tag:  Can only tag FLEXANIMATION events\n" );
		return;
	}

	CInputParams params;
	memset( &params, 0, sizeof( params ) );

	strcpy( params.m_szDialogTitle, "Event Tag Name" );
	strcpy( params.m_szPrompt, "Name:" );

	strcpy( params.m_szInputText, "" );

	if ( !InputProperties( &params ) )
		return;

	if ( strlen( params.m_szInputText ) <= 0 )
	{
		Con_ErrorPrintf( "Timing Tag Name:  No name entered!\n" );
		return;
	}
	
	// Convert click to frac
	float t = GetTimeValueForMouse( mx );
	float frac = 0.0f;
	if ( event->GetDuration() )
	{
		frac = t / event->GetDuration();
		frac = clamp( frac, 0.0f, 1.0f );
	}

	g_pChoreoView->SetDirty( true );

	g_pChoreoView->PushUndo( "Add Timing Tag" );

	event->AddTimingTag( params.m_szInputText, frac, true );

	g_pChoreoView->PushRedo( "Add Timing Tag" );

	// Redraw this window
	m_pWorkspace->redraw();
	redraw();
}

void ExpressionTool::DeleteFlexTimingTag( int mx, int my )
{
	Assert( g_pChoreoView );

	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return;

	CFlexTimingTag *tag = IsMouseOverTag( mx, my );
	if ( !tag )
		return;
	
	g_pChoreoView->SetDirty( true );

	g_pChoreoView->PushUndo( "Delete Timing Tag" );

	event->RemoveTimingTag( tag->GetName() );

	g_pChoreoView->PushRedo( "Delete Timing Tag" );

	LayoutItems( true );
	// Redraw this window
	redraw();

}

void ExpressionTool::LockTimingTag( void )
{
	Assert( g_pChoreoView );

	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return;

	CFlexTimingTag *tag = IsMouseOverTag( m_nClickedX, m_nClickedY );
	if ( !tag )
		return;

	if ( tag->GetLocked() )
		return;

	g_pChoreoView->SetDirty( true );

	g_pChoreoView->PushUndo( "Lock Timing Tag" );

	tag->SetLocked( true );

	g_pChoreoView->PushRedo( "Lock Timing Tag" );

	redraw();
}

void ExpressionTool::UnlockTimingTag( void )
{
	Assert( g_pChoreoView );

	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return;

	CFlexTimingTag *tag = IsMouseOverTag( m_nClickedX, m_nClickedY );
	if ( !tag )
		return;

	if ( !tag->GetLocked() )
		return;

	g_pChoreoView->SetDirty( true );

	g_pChoreoView->PushUndo( "Unlock Timing Tag" );

	tag->SetLocked( false );

	g_pChoreoView->PushRedo( "Unlock Timing Tag" );

	redraw();
}

void ExpressionTool::ApplyBounds( int& mx, int& my )
{
	if ( !m_bUseBounds )
		return;

	mx = clamp( mx, m_nMinX, m_nMaxX );
}

void ExpressionTool::CalcBounds( int movetype )
{
	switch ( movetype )
	{
	default:
	case DRAGTYPE_NONE:
		m_bUseBounds = false;
		m_nMinX = 0;
		m_nMaxX = 0;
		break;
	case DRAGTYPE_SCRUBBER:
		m_bUseBounds = true;
		m_nMinX = 0;
		m_nMaxX = w2();
		break;
	case DRAGTYPE_FLEXTIMINGTAG:
		{
			m_bUseBounds = true;

			int left, right;
			GetWorkspaceLeftRight( left, right );

			m_nMinX = left;
			m_nMaxX = right;

			RECT rcClient;
			rcClient.left = left;
			rcClient.right = right;
			rcClient.top = 0;
			rcClient.bottom = TRAY_HEIGHT;

			CFlexTimingTag *tag = IsMouseOverTag( m_nStartX, m_nStartY );
			if ( tag && 
				tag->GetOwner() )
			{
				CChoreoEvent *e = tag->GetOwner();
				
				float st = e->GetStartTime();
				float ed = e->GetEndTime();
				
				if ( ed > st )
				{
					
					
					// Find previous tag, if any
					CFlexTimingTag *prev = NULL;
					CFlexTimingTag *next = NULL;
					
					for ( int i = 0; i < e->GetNumTimingTags(); i++ )
					{
						CFlexTimingTag *test = e->GetTimingTag( i );
						if ( test != tag )
							continue;
						
						// Found it
						if ( i > 0 )
						{
							prev = e->GetTimingTag( i - 1 );
						}
						
						if ( i + 1  < e->GetNumTimingTags() )
						{
							next = e->GetTimingTag( i + 1 );
						}
						break;
					}
					
					if ( prev )
					{
						// Compute x pixel of prev tag
						float frac = ( prev->GetStartTime() - st ) / ( ed - st );
						if ( frac >= 0.0f && frac <= 1.0f )
						{
							int tagx = rcClient.left + (int)( frac * (float)( rcClient.right - rcClient.left ) );
							
							m_nMinX = max( m_nMinX, tagx + 5 );
						}
					}
					
					if ( next )
					{
						// Compute x pixel of next tag
						float frac = ( next->GetStartTime() - st ) / ( ed - st );
						if ( frac >= 0.0f && frac <= 1.0f )
						{
							int tagx = rcClient.left + (int)( frac * (float)( rcClient.right - rcClient.left ) );
							m_nMaxX = min( m_nMaxX, tagx - 5 );
						}
					}
				}

			}
		}
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *tag - 
//			newposition - 
//-----------------------------------------------------------------------------
void ExpressionTool::ResampleControlPoints( CFlexTimingTag *tag, float newposition )
{
	CChoreoEvent *e = tag->GetOwner();
	if ( !e )
		return;

	float duration = e->GetDuration();

	float leftedge = 0.0f;
	float rightedge = duration;
	
	// Find neighboring locked tags, if any
	CFlexTimingTag *prev = NULL;
	CFlexTimingTag *next = NULL;
	
	int i;
	for ( i = 0; i < e->GetNumTimingTags(); i++ )
	{
		CFlexTimingTag *test = e->GetTimingTag( i );
		if ( test != tag )
			continue;
		
		// Found it
		if ( i > 0 )
		{
			int i1 = i - 1;
			while ( 1 )
			{
				if ( i1 < 0 )
				{
					prev = NULL;
					break;
				}

				prev = e->GetTimingTag( i1 );
				if ( prev->GetLocked() )
					break;

				i1--;
			}
		}
		
		if ( i + 1  < e->GetNumTimingTags() )
		{
			int i1 = i + 1;
			while ( 1 )
			{
				if ( i1 >= e->GetNumTimingTags() )
				{
					next = NULL;
					break;
				}

				next = e->GetTimingTag( i1 );
				if ( next->GetLocked() )
					break;

				i1++;
			}
		}
		break;
	}

	if ( prev )
	{
		leftedge = prev->GetPercentage() * duration;
	}

	if ( next )
	{
		rightedge = next->GetPercentage() * duration;
	}

	// Now, using the tags old position as a pivot, rescale intervening
	//  sample points based on size delta of new vs old range
	float oldpivot = tag->GetPercentage() * duration;
	float newpivot = newposition * duration;

	float oldleftrange = oldpivot - leftedge;
	float oldrightrange = rightedge - oldpivot;

	float newleftrange = newpivot - leftedge;
	float newrightrange = rightedge - newpivot;

	if ( oldleftrange <= 0.0f ||
		 oldrightrange <= 0.0f ||
		 newleftrange <= 0.0f ||
		 newrightrange <= 0.0f )
	{
		Con_Printf( "Range problem!!! avoiding division by zero\n" );
		return;
	}
		 
	for ( i = 0 ; i < e->GetNumFlexAnimationTracks(); i++ )
	{
		CFlexAnimationTrack *track = e->GetFlexAnimationTrack( i );
		if ( !track )
			continue;

		for ( int t = 0; t < ( track->IsComboType() ? 2 : 1 ); t++ )
		{
			for ( int j = 0; j < track->GetNumSamples( t ); j++ )
			{
				CExpressionSample *s = track->GetSample( j, t );
				if ( !s )
					continue;

				float oldtime = s->time;

				// In old range?
				if ( oldtime < leftedge )
					continue;
				if ( oldtime > rightedge )
					continue;

				// In left or right side( tiebreak toward left )
				float newtime = oldtime;

				if ( oldtime <= oldpivot )
				{
					float n = ( oldtime - leftedge ) / oldleftrange;
					newtime = leftedge + n * newleftrange;
				}
				else
				{
					float n = ( oldtime - oldpivot ) / oldrightrange;
					newtime = newpivot + n * newrightrange;
				}

				//newtime = FacePoser_SnapTime( newtime );

				s->time = newtime;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ExpressionTool::OnNewExpression( void )
{
	CChoreoEvent *e = GetSafeEvent();
	if ( !e )
		return;

	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if ( !hdr )
	{
		Con_ErrorPrintf( "ExpressionTool::OnNewExpression:  Can't create new face pose, must load a model first!\n" );
		return;
	}

	CExpClass *active = expressions->GetActiveClass();
	if ( !active )
	{
		Con_ErrorPrintf( "ExpressionTool::OnNewExpression:  Can't create new face pose, must load an expression file first!\n" );
		return;
	}

	g_pExpressionTrayTool->Deselect();

	float t = GetTimeValueForMouse( m_nClickedX );

	// Get spline intensity for controller
	float faketime = e->GetStartTime() + t;

	float settings[ GLOBAL_STUDIO_FLEX_CONTROL_COUNT ];
	float weights[ GLOBAL_STUDIO_FLEX_CONTROL_COUNT ];
	memset( settings, 0, sizeof( settings ) );
	memset( weights, 0, sizeof( settings ) );

	for ( int i = 0 ; i < e->GetNumFlexAnimationTracks(); i++ )
	{
		CFlexAnimationTrack *track = e->GetFlexAnimationTrack( i );
		if ( !track )
			continue;

		// Disabled
		if ( !track->IsTrackActive() )
			continue;

		// Map track flex controller to global name
		if ( track->IsComboType() )
		{
			for ( int side = 0; side < 2; side++ )
			{
				int controller = track->GetFlexControllerIndex( side );
				if ( controller != -1 )
				{
					// Get spline intensity for controller
					float flIntensity = track->GetIntensity( faketime, side );

					settings[ controller ] = flIntensity;
					weights[ controller ] = 1.0f;
				}
			}
		}
		else
		{
			int controller = track->GetFlexControllerIndex( 0 );
			if ( controller != -1 )
			{
				// Get spline intensity for controller
				float flIntensity = track->GetIntensity( faketime, 0 );

				settings[ controller ] = flIntensity;
				weights[ controller ] = 1.0f;
			}
		}
	}

	CExpressionParams params;
	memset( &params, 0, sizeof( params ) );

	strcpy( params.m_szDialogTitle, "Add Expression" );
	strcpy( params.m_szName, "" );
	strcpy( params.m_szDescription, "" );

	if ( !ExpressionProperties( &params ) )
		return;

	if ( ( strlen( params.m_szName ) <= 0 ) ||
		!stricmp( params.m_szName, "unnamed" ) )
	{
		Con_ErrorPrintf( "You must type in a valid name\n" );
		return;
	}

	if ( ( strlen( params.m_szDescription ) <= 0 ) ||
   	   !stricmp( params.m_szDescription, "description" ) )
	{
		Con_ErrorPrintf( "You must type in a valid description\n" );
		return;
	}

	active->AddExpression( params.m_szName, params.m_szDescription, settings, weights, true, true );
}

LocalFlexController_t FindFlexControllerIndexByName( StudioModel *model, char const *searchname )
{
	if ( !model )
		return LocalFlexController_t(-1);

	CStudioHdr *hdr = model->GetStudioHdr();
	if ( !hdr )
		return LocalFlexController_t(-1);

	for ( LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++ )
	{
		char const *name = hdr->pFlexcontroller( i )->pszName();
		if ( !name )
			continue;

		if ( strcmp( name, searchname ) )
			continue;

		return i;
	}
	return LocalFlexController_t(-1);
}

void ExpressionTool::OnCopyToFlex( bool isEdited )
{
	// local time in the expression tool for the last mouse click
	float t = GetTimeValueForMouse( m_nClickedX );

	CChoreoEvent *e = GetSafeEvent();
	if ( !e )
		return;

	float scenetime = e->GetStartTime() + t;

	OnCopyToFlex( scenetime, isEdited );

	return;
}


void ExpressionTool::OnCopyToFlex( float scenetime, bool isEdited )
{
	CChoreoEvent *e = GetSafeEvent();
	if ( !e )
		return;

	if ( scenetime < e->GetStartTime() || scenetime > e->GetEndTime() )
		return;

	bool needundo = false;

	float *settings = NULL;
	float *weights = NULL;
	CExpression *exp = NULL;
	CExpClass *active = expressions->GetActiveClass();
	if ( active )
	{
		
		int index = active->GetSelectedExpression();
		if ( index != -1 )
		{
			exp = active->GetExpression( index );
			if ( exp )
			{
				needundo = true;
				settings = exp->GetSettings();
				weights = exp->GetWeights();
			}
		}
	}

	if ( needundo && exp )
	{
		exp->PushUndoInformation();
		active->SetDirty( true );
	}

	g_pFlexPanel->ResetSliders( false, true );

	StudioModel *model = models->GetActiveStudioModel();

	for ( int i = 0 ; i < e->GetNumFlexAnimationTracks(); i++ )
	{
		CFlexAnimationTrack *track = e->GetFlexAnimationTrack( i );
		if ( !track )
			continue;

		// Disabled
		if ( !track->IsTrackActive() )
			continue;

		// Map track flex controller to global name
		for ( int side = 0; side < 1 + track->IsComboType(); side++ )
		{
			int controller = track->GetFlexControllerIndex( side );
			if ( controller != -1 )
			{
				// Get spline intensity for controller
				float flIntensity = track->GetIntensity( scenetime, side );

				g_pFlexPanel->SetSlider( controller, flIntensity );
				g_pFlexPanel->SetInfluence( controller, 1.0f );
				g_pFlexPanel->SetEdited( controller, isEdited );
				if( model )
				{
					LocalFlexController_t raw = track->GetRawFlexControllerIndex( side );
					if ( raw != LocalFlexController_t(-1) )
					{
						model->SetFlexController( raw, flIntensity );
					}
				}
				if ( settings && weights )
				{
					settings[ controller ] = flIntensity;
					weights[ controller ] = 1.0f;
				}
			}
		}
	}

	if ( needundo && exp )
	{
		exp->PushRedoInformation();
	}
}

void ExpressionTool::OnCopyFromFlex( bool isEdited )
{
	// local time in the expression tool for the last mouse click
	float t = GetTimeValueForMouse( m_nClickedX );

	CChoreoEvent *e = GetSafeEvent();
	if ( !e )
		return;

	float scenetime = e->GetStartTime() + t;

	OnCopyFromFlex( scenetime, isEdited );

	return;
}

void ExpressionTool::OnSetSingleKeyFromFlex( char const *sliderName )
{
	CChoreoEvent *e = GetSafeEvent();
	if ( !e || !e->GetDuration() )
		return;

	float scenetime = g_pChoreoView->GetScene()->GetTime();
	
	if ( scenetime < e->GetStartTime() || scenetime > e->GetEndTime() )
		return;

	scenetime = FacePoser_SnapTime( scenetime );

	float relativetime = scenetime - e->GetStartTime();

	// Get spline intensity for controller

	float				setting;
	float				influence;
	float				minvalue, maxvalue;

	g_pChoreoView->SetDirty( true );
	g_pChoreoView->PushUndo( "Set Single Key" );

	for (int j = 0; j < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; j++)
	{
		if ( !g_pFlexPanel->IsValidSlider( j ) )
			continue;

		if ( Q_stricmp( g_pFlexPanel->getLabel(), sliderName ) )
			continue;

		setting			= g_pFlexPanel->GetSliderRawValue( j );
		influence		= g_pFlexPanel->GetInfluence( j );

		// g_pFlexPanel->SetEdited( j, isEdited );

		g_pFlexPanel->GetSliderRange( j, minvalue, maxvalue );

		bool found = false;
		for ( int i = 0 ; i < e->GetNumFlexAnimationTracks() && !found; i++ )
		{
			CFlexAnimationTrack *track = e->GetFlexAnimationTrack( i );
			if ( !track )
				continue;

			for ( int side = 0; side < 1 + track->IsComboType(); side++ )
			{
				if ( track->GetFlexControllerIndex( side ) != j )
					continue;

				float normalized = setting;
				if ( side == 0 )
				{
					if ( minvalue != maxvalue )
					{
						normalized = ( setting - minvalue ) / ( maxvalue - minvalue );
					}
					if (track->IsInverted())
					{
						normalized = 1.0 - normalized;
					}
				}

				found = true;

				int nSampleCount = track->GetNumSamples( side );

				int j = 0;
				for ( ; j < nSampleCount; ++j )
				{
					CExpressionSample *s = track->GetSample( j, side );
					if ( s->time == relativetime )
						break;
				}

				if ( j >= nSampleCount )
				{
					track->AddSample( relativetime, normalized, side );
					track->Resort( side );
				}
				else
				{
					CExpressionSample *s = track->GetSample( j, side );
					s->value = normalized;
				}
				
				track->SetTrackActive( true );

				break;
			}
		}
	}

	g_pChoreoView->PushRedo( "Set Single Key" );

	m_pWorkspace->redraw();
	redraw();
}

void ExpressionTool::OnCopyFromFlex( float scenetime, bool isEdited )
{
	CChoreoEvent *e = GetSafeEvent();
	if ( !e || !e->GetDuration() )
		return;

	if ( scenetime < e->GetStartTime() || scenetime > e->GetEndTime() )
		return;

	scenetime = FacePoser_SnapTime( scenetime );

	float relativetime = scenetime - e->GetStartTime();

	// Get spline intensity for controller

	float				setting;
	float				influence;
	float				minvalue, maxvalue;

	g_pChoreoView->SetDirty( true );
	g_pChoreoView->PushUndo( "Copy from Flex" );

	for (int j = 0; j < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; j++)
	{
		if ( !g_pFlexPanel->IsValidSlider( j ) )
			continue;

		setting			= g_pFlexPanel->GetSliderRawValue( j );
		//setting = g_pFlexPanel->GetSlider( j );
		influence		= g_pFlexPanel->GetInfluence( j );

		g_pFlexPanel->SetEdited( j, isEdited );

		g_pFlexPanel->GetSliderRange( j, minvalue, maxvalue );

		// Found it
		if ( !influence )
		{
			continue;
		}

		bool found = false;
		for ( int i = 0 ; i < e->GetNumFlexAnimationTracks() && !found; i++ )
		{
			CFlexAnimationTrack *track = e->GetFlexAnimationTrack( i );
			if ( !track )
				continue;

			for ( int side = 0; side < 1 + track->IsComboType(); side++ )
			{
				if ( track->GetFlexControllerIndex( side ) != j )
					continue;

				float normalized = setting;
				if ( side == 0 )
				{
					if ( minvalue != maxvalue )
					{
						normalized = ( setting - minvalue ) / ( maxvalue - minvalue );
					}
					if (track->IsInverted())
					{
						normalized = 1.0 - normalized;
					}
				}

				found = true;

				track->AddSample( relativetime, normalized, side );
				track->Resort( side );
				track->SetTrackActive( true );

				break;
			}
		}
	}

	g_pChoreoView->PushRedo( "Copy from Flex" );

	m_pWorkspace->redraw();
	redraw();
}

bool ExpressionTool::SetFlexAnimationTrackFromExpression( int mx, int my, CExpClass *cl, CExpression *exp )
{
	CChoreoEvent *e = GetSafeEvent();
	if ( !e | !e->GetDuration() )
	{
		return false;
	}

	if ( !exp )
	{
		return false;
	}

	// Convert screen to client
	POINT pt;
	pt.x = mx;
	pt.y = my;

	ScreenToClient( (HWND)getHandle(), &pt );

	if ( pt.x < 0 || pt.y < 0 )
	{
		return false;
	}

	if ( pt.x > w2() || pt.y > h2() )
	{
		return false;
	}

	float t = GetTimeValueForMouse( (short)pt.x );
	
	// Get spline intensity for controller
	// Get spline intensity for controller
	float relativetime = t;
	float faketime = e->GetStartTime() + relativetime;

	faketime = FacePoser_SnapTime( faketime );

	float *settings = exp->GetSettings();
	float *influence = exp->GetWeights();

	if ( !settings || !influence )
		return false;

	g_pChoreoView->SetDirty( true );
	g_pChoreoView->PushUndo( "Copy from Expression" );

	for ( int i = 0 ; i < e->GetNumFlexAnimationTracks(); i++ )
	{
		CFlexAnimationTrack *track = e->GetFlexAnimationTrack( i );
		if ( !track )
			continue;

		if ( track->IsComboType() )
		{
			int left = track->GetFlexControllerIndex( 0 );
			int right = track->GetFlexControllerIndex( 1 );

			float leftval = settings[ left ];
			float leftinfluence = influence[ left ];
			float rightval = settings[ right ];
			float rightinfluence = influence[ right ];

			if ( leftinfluence || rightinfluence )
			{

				//Con_Printf( "%s %i(side %i):  amount %f inf %f\n", track->GetFlexControllerName(), j, side, s, inf );
				
				float mag, leftright;

				if (leftval < rightval)
				{
					mag = rightval;
					leftright = 1.0 - (leftval / rightval) * 0.5;
				}
				else if (leftval > rightval)
				{
					mag = leftval;
					leftright = (rightval / leftval) * 0.5;
				}
				else
				{
					mag = leftval;
					leftright = 0.5;
				}

				track->AddSample( relativetime, mag * leftinfluence, 0 );
				track->AddSample( relativetime, leftright, 1 );

				track->Resort( 0 );
				track->Resort( 1 );

				track->SetTrackActive( true );
			}
		}
		else
		{
			int j = track->GetFlexControllerIndex( 0 );

			float s = settings[ j ];
			float inf = influence[ j ];

			if ( inf )
			{
				track->AddSample( relativetime, s, 0 );

				track->Resort( 0 );

				track->SetTrackActive( true );
			}
		}
	}

	g_pChoreoView->PushRedo( "Copy from Expression" );

	m_pWorkspace->redraw();
	redraw();

	return true;
}

bool ExpressionTool::PaintBackground()
{
	redraw();
	return false;
}

void ExpressionTool::OnExportFlexAnimation( void )
{
	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return;

	// Create flexanimations dir
	CreatePath( "flexanimations/foo" );

	char fafilename[ 512 ];
	if ( !FacePoser_ShowSaveFileNameDialog( fafilename, sizeof( fafilename ), "flexanimations", "*.vfa" ) )
	{
		return;
	}

	Q_DefaultExtension( fafilename, ".vfa", sizeof( fafilename ) );

	Con_Printf( "Exporting events to %s\n", fafilename );

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	CChoreoScene::FileSaveFlexAnimations( buf, 0, event );

	// Write it out baby
	FileHandle_t fh = filesystem->Open( fafilename, "wt" );
	if (fh)
	{
		filesystem->Write( buf.Base(), buf.TellPut(), fh );
		filesystem->Close(fh);
	}
	else
	{
		Con_Printf( "Unable to write file %s!!!\n", fafilename );
	}
}

void ExpressionTool::OnImportFlexAnimation( void )
{
	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return;

	char fafilename[ 512 ];
	if ( !FacePoser_ShowOpenFileNameDialog( fafilename, sizeof( fafilename ), "flexanimations", "*.vfa" ) )
	{
		return;
	}

	if ( !filesystem->FileExists( fafilename ) )
		return;

	char fullpath[ 512 ];
	filesystem->RelativePathToFullPath( fafilename, "MOD", fullpath, sizeof( fullpath ) );

	LoadScriptFile( (char *)fullpath );

	tokenprocessor->GetToken( true );
	if ( stricmp( tokenprocessor->CurrentToken(), "flexanimations" ) )
	{
		Con_Printf( "ExpressionTool::OnImportFlexAnimation:  %s, expecting \"flexanimations\"\n",
			fullpath );
	}
	else
	{
		g_pChoreoView->SetDirty( true );
		g_pChoreoView->PushUndo( "Import flex animations" );

		CChoreoScene::ParseFlexAnimations( tokenprocessor, event, true );

		// Force a full reset
		m_pLastEvent = NULL;
		SetEvent( event );

		g_pChoreoView->PushRedo( "Import flex animations" );

		Con_Printf( "Parsed flex animations from %s\n", fullpath );
	}
}

void ExpressionTool::OnUndo( void )
{
	g_pChoreoView->Undo();
}

void ExpressionTool::OnRedo( void )
{
	g_pChoreoView->Redo();
}

void ExpressionTool::ForceScrubPositionFromSceneTime( float scenetime )
{
	CChoreoEvent *e = GetSafeEvent();
	if ( !e || !e->GetDuration() )
		return;

	float t = scenetime - e->GetStartTime();
	m_flScrub = t;
	m_flScrubTarget = t;

	DrawScrubHandles();
}

void ExpressionTool::ForceScrubPosition( float frac )
{
	m_flScrub = frac;
	m_flScrubTarget = frac;
	
	CChoreoEvent *e = GetSafeEvent();
	if ( e )
	{
		float realtime = e->GetStartTime() + frac;

		g_pChoreoView->SetScrubTime( realtime );
		g_pChoreoView->SetScrubTargetTime( realtime );

		g_pChoreoView->DrawScrubHandle();
	}

	DrawScrubHandles();
}

void ExpressionTool::DrawScrubHandles()
{
	RECT rcHandle;
	GetScrubHandleRect( rcHandle, true );

	RECT rcTray = rcHandle;
	rcTray.left = 0;
	rcTray.right = w2();

	CChoreoWidgetDrawHelper drawHelper( this, rcTray );
	DrawScrubHandle( drawHelper, rcHandle );
}

void ExpressionTool::SetMouseOverPos( int x, int y )
{
	m_nMousePos[ 0 ] = x;
	m_nMousePos[ 1 ] = y;
}

void ExpressionTool::GetMouseOverPos( int &x, int& y )
{
	x = m_nMousePos[ 0 ];
	y = m_nMousePos[ 1 ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rcPos - 
//-----------------------------------------------------------------------------
void ExpressionTool::GetMouseOverPosRect( RECT& rcPos )
{
	rcPos.top = GetCaptionHeight() + 12;
	rcPos.left = w2() - 200;
	rcPos.right = w2() - 5;
	rcPos.bottom = rcPos.top + 13;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rcPos - 
//-----------------------------------------------------------------------------
void ExpressionTool::DrawMouseOverPos( CChoreoWidgetDrawHelper& drawHelper, RECT& rcPos )
{
	// Compute time for pixel x
	float t = GetTimeValueForMouse( m_nMousePos[ 0 ] );
	CChoreoEvent *e = GetSafeEvent();
	if ( !e )
		return;

	t += e->GetStartTime();

	float snapped = FacePoser_SnapTime( t );

	// Found it, write out description
	// 
	char sz[ 128 ];
	if ( t != snapped )
	{
		Q_snprintf( sz, sizeof( sz ), "%s", FacePoser_DescribeSnappedTime( t ) );
	}
	else
	{
		Q_snprintf( sz, sizeof( sz ), "%.3f", t );
	}

	int len = drawHelper.CalcTextWidth( "Arial", 11, 900, sz );

	RECT rcText = rcPos;
	rcText.left = max( rcPos.left, rcPos.right - len );

	drawHelper.DrawColoredText( "Arial", 11, 900, RGB( 255, 50, 70 ), rcText, sz );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ExpressionTool::DrawMouseOverPos()
{
	RECT rcPos;
	GetMouseOverPosRect( rcPos );

	CChoreoWidgetDrawHelper drawHelper( this, rcPos );
	DrawMouseOverPos( drawHelper, rcPos );
}

int ExpressionTool::CountSelectedSamples( void )
{
	return m_pWorkspace->CountSelectedSamples();
}

void ExpressionTool::MoveSelectedSamples( float dfdx, float dfdy, bool snap )
{
	m_pWorkspace->MoveSelectedSamples( dfdx, dfdy, snap );
}

void ExpressionTool::DeleteSelectedSamples( void )
{
	m_pWorkspace->DeleteSelectedSamples();
}

void ExpressionTool::DeselectAll( void )
{
	m_pWorkspace->DeselectAll();
	m_bSelectionActive = false;
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : start - 
//			end - 
//-----------------------------------------------------------------------------
void ExpressionTool::SelectPoints( float starttime, float endtime )
{
	// Make sure order is correct
	if ( endtime < starttime )
	{
		float temp = endtime;
		endtime = starttime;
		starttime = temp;
	}

	DeselectAll();

	m_flSelection[ 0 ] = starttime;
	m_flSelection[ 1 ] = endtime;
	m_bSelectionActive = true;

	// Select any words that span the selection
	//
	m_pWorkspace->SelectPoints( starttime, endtime );

	redraw();
}

void ExpressionTool::FinishMoveSelection( int startx, int mx )
{
	float start = GetTimeValueForMouse( startx );
	float end = GetTimeValueForMouse( mx );

	float delta = end - start;

	for ( int i = 0; i < 2; i++ )
	{
		m_flSelection[ i ] += delta;
	}

	SelectPoints( m_flSelection[ 0 ], m_flSelection[ 1 ] );

	redraw();
}

void ExpressionTool::FinishMoveSelectionStart( int startx, int mx )
{
	float start = GetTimeValueForMouse( startx );
	float end = GetTimeValueForMouse( mx );

	float delta = end - start;

	m_flSelection[ 0 ] += delta;

	SelectPoints( m_flSelection[ 0 ], m_flSelection[ 1 ] );

	redraw();
}

void ExpressionTool::FinishMoveSelectionEnd( int startx, int mx )
{
	float start = GetTimeValueForMouse( startx );
	float end = GetTimeValueForMouse( mx );

	float delta = end - start;

	m_flSelection[ 1 ] += delta;

	SelectPoints( m_flSelection[ 0 ], m_flSelection[ 1 ] );

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : startx - 
//			mx - 
//-----------------------------------------------------------------------------
void ExpressionTool::FinishSelect( int startx, int mx )
{
	// Don't select really small areas
	if ( abs( startx - mx ) < 1 )
		return;

	float start = GetTimeValueForMouse( startx );
	float end = GetTimeValueForMouse( mx );

	SelectPoints( start, end );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ExpressionTool::IsMouseOverPoints( int mx, int my )
{
	RECT rc;
	GetWorkspaceRect( rc );

	// Over tag
	if ( my > TRAY_HEIGHT )
		return false;

	if ( my <= 12 + GetCaptionHeight() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ExpressionTool::IsMouseOverSelection( int mx, int my )
{
	if ( !m_bSelectionActive )
		return false;

	if ( !IsMouseOverPoints( mx, my ) )
		return false;

	float t = GetTimeValueForMouse( mx );

	if ( t >= m_flSelection[ 0 ] &&
		 t <= m_flSelection[ 1 ] )
	{
		return true;
	}

	return false;
}

bool ExpressionTool::IsMouseOverSelectionStartEdge( mxEvent *event )
{
	int mx, my;
	mx = (short)event->x;
	my = (short)event->y;

	if ( !(event->modifiers & mxEvent::KeyCtrl ) )
		return false;

	if ( !IsMouseOverSelection( mx, my ) )
		return false;

	int left;

	left = GetPixelForTimeValue( m_flSelection[ 0 ] );

	if ( abs( left - mx ) <= 2 )
	{
		return true;
	}

	return false;
}

bool ExpressionTool::IsMouseOverSelectionEndEdge( mxEvent *event )
{
	int mx, my;
	mx = (short)event->x;
	my = (short)event->y;

	if ( !(event->modifiers & mxEvent::KeyCtrl ) )
		return false;

	if ( !IsMouseOverSelection( mx, my ) )
		return false;

	int right;

	right = GetPixelForTimeValue( m_flSelection[ 1 ] );

	if ( abs( right - mx ) <= 2 )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &rc - 
//-----------------------------------------------------------------------------
void ExpressionTool::GetWorkspaceRect( RECT &rc )
{
	GetClientRect( (HWND)getHandle(), &rc );
	
	rc.top = TRAY_HEIGHT - 17;
	rc.bottom = TRAY_HEIGHT - 1;
	//InflateRect( &rc, -1, -1 );
}

void ExpressionTool::AddFocusRect( RECT& rc )
{
	RECT rcFocus = rc;

	POINT offset;
	offset.x = 0;
	offset.y = 0;
	ClientToScreen( (HWND)getHandle(), &offset );
	OffsetRect( &rcFocus, offset.x, offset.y );

	// Convert to screen space?
	CFocusRect fr;
	fr.m_rcFocus = rcFocus;
	fr.m_rcOrig = rcFocus;

	m_FocusRects.AddToTail( fr );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int ExpressionTool::ComputeHPixelsNeeded( void )
{
	CChoreoEvent *event = GetSafeEvent();
	if ( !event )
		return 0;

	int pixels = 0;
	float maxtime = event->GetDuration();
	pixels = (int)( ( maxtime + 5.0 ) * GetPixelsPerSecond() + 10 );

	return pixels;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ExpressionTool::RepositionHSlider( void )
{
	int pixelsneeded = ComputeHPixelsNeeded();

	if ( pixelsneeded <= w2() )
	{
		m_pHorzScrollBar->setVisible( false );
	}
	else
	{
		m_pHorzScrollBar->setVisible( true );
	}
	m_pHorzScrollBar->setBounds( 0, h2() - m_nScrollbarHeight, w2(), m_nScrollbarHeight );

	m_flLeftOffset = max( 0.f, m_flLeftOffset );
	m_flLeftOffset = min( (float)pixelsneeded, m_flLeftOffset );

	m_pHorzScrollBar->setRange( 0, pixelsneeded );
	m_pHorzScrollBar->setValue( m_flLeftOffset );
	m_pHorzScrollBar->setPagesize( w2() );

	m_nLastHPixelsNeeded = pixelsneeded;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float ExpressionTool::GetPixelsPerSecond( void )
{
	return m_flPixelsPerSecond * (float)g_pChoreoView->GetTimeZoom( GetToolName() ) / 100.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//-----------------------------------------------------------------------------
void ExpressionTool::MoveTimeSliderToPos( int x )
{
	m_flLeftOffset = x;
	m_pHorzScrollBar->setValue( m_flLeftOffset );
	InvalidateRect( (HWND)m_pHorzScrollBar->getHandle(), NULL, TRUE );
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ExpressionTool::InvalidateLayout( void )
{
	if ( m_bSuppressLayout )
		return;

	if ( ComputeHPixelsNeeded() != m_nLastHPixelsNeeded )
	{
		RepositionHSlider();
	}

	m_bLayoutIsValid = false;
	m_pWorkspace->redraw();
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : time - 
//			*clipped - 
// Output : int
//-----------------------------------------------------------------------------
int ExpressionTool::GetPixelForTimeValue( float time, bool *clipped /*=NULL*/ )
{
	int left, right;
	
	GetWorkspaceLeftRight( left, right );

	if ( clipped )
	{
		*clipped = false;
	}

	float st, ed;
	GetStartAndEndTime( st, ed );

	float frac = ( time - st ) / ( ed - st );
	if ( frac < 0.0 || frac > 1.0 )
	{
		if ( clipped )
		{
			*clipped = true;
		}
	}

	int pixel = left + ( int )( frac * (right - left ) );
	return pixel;
}

void ExpressionTool::OnChangeScale( void )
{
	CChoreoScene *scene = g_pChoreoView->GetScene();
	if ( !scene )
	{
		return;
	}

	// Zoom time in  / out
	CInputParams params;
	memset( &params, 0, sizeof( params ) );

	strcpy( params.m_szDialogTitle, "Change Zoom" );
	strcpy( params.m_szPrompt, "New scale (e.g., 2.5x):" );

	Q_snprintf( params.m_szInputText, sizeof( params.m_szInputText ), "%.2f", (float)g_pChoreoView->GetTimeZoom( GetToolName() ) / 100.0f );

	if ( !InputProperties( &params ) )
		return;

	g_pChoreoView->SetTimeZoom( GetToolName(), clamp( (int)( 100.0f * atof( params.m_szInputText ) ), 1, MAX_TIME_ZOOM ), false );

	m_nLastHPixelsNeeded = -1;
	InvalidateLayout();
	Con_Printf( "Zoom factor %i %%\n", g_pChoreoView->GetTimeZoom( GetToolName() ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : st - 
//			ed - 
//-----------------------------------------------------------------------------
void ExpressionTool::GetStartAndEndTime( float& st, float& ed )
{
	st = m_flLeftOffset / GetPixelsPerSecond();
	int left, right;
	GetWorkspaceLeftRight( left, right );
	if ( right <= left )
	{
		ed = st;
	}
	else
	{
        ed = st + (float)( right - left ) / GetPixelsPerSecond();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : float
//-----------------------------------------------------------------------------
float ExpressionTool::GetEventEndTime()
{
	CChoreoEvent *ev = GetSafeEvent();
	if ( !ev )
		return 1.0f;

	return ev->GetDuration();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			clip - 
// Output : float
//-----------------------------------------------------------------------------
float ExpressionTool::GetTimeValueForMouse( int mx, bool clip /*=false*/)
{
	int left, right;
	
	GetWorkspaceLeftRight( left, right );

	float st, ed;
	GetStartAndEndTime( st, ed );

	if ( clip )
	{
		if ( mx < 0 )
		{
			return st;
		}
		if ( mx > w2() )
		{
			return ed;
		}
	}

	float frac = (float)( mx - left )  / (float)( right - left );
	return st + frac * ( ed - st );
}

void ExpressionTool::DrawEventEnd( CChoreoWidgetDrawHelper& drawHelper )
{
	CChoreoEvent *e = GetSafeEvent();
	if ( !e )
		return;

	float duration = e->GetDuration();
	if ( !duration )
		return;

	int leftx = GetPixelForTimeValue( duration );
	if ( leftx >= w2() )
		return;

	RECT rcClient;
	drawHelper.GetClientRect( rcClient );

	drawHelper.DrawColoredLine(
		COLOR_CHOREO_ENDTIME, PS_SOLID, 1,
		leftx, rcClient.top + TRAY_HEIGHT, leftx, rcClient.bottom );

}

void ExpressionTool::OnSortByUsed( void )
{
	m_pWorkspace->OnSortByUsed();
}

void ExpressionTool::OnSortByName( void )
{
	m_pWorkspace->OnSortByName();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *item - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ExpressionTool::IsFocusItem( TimelineItem *item )
{
	return m_pWorkspace->GetClickedItem() == item;
}

//-----------------------------------------------------------------------------
// Purpose: Delete a vertical column of samples between the selection
// markers.  If excise_time is true, shifts remaining samples left
// Input  : excise_time - 
//-----------------------------------------------------------------------------
void ExpressionTool::OnDeleteSelection( bool excise_time )
{
	if ( !m_bSelectionActive )
		return;

	// Force selection of everything again!
	SelectPoints( m_flSelection[ 0 ], m_flSelection[ 1 ] );

	int i, t;

	char const *undotext = excise_time ? "Excise column" : "Delete column";

	float shift_left_time = m_flSelection[ 1 ] - m_flSelection[ 0 ];
	Assert( shift_left_time > 0.0f );

	g_pChoreoView->SetDirty( true );
	g_pChoreoView->PushUndo( undotext );

	for ( int controller = 0; controller < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; controller++ )
	{
		TimelineItem *item = m_pWorkspace->GetItem( controller );
		if ( !item )
			continue;

		CFlexAnimationTrack *track = item->GetSafeTrack();
		if ( !track )
			continue;

		for ( t = 0; t < 2; t++ )
		{
			for ( i = track->GetNumSamples( t ) - 1; i >= 0 ; i-- )
			{
				CExpressionSample *sample = track->GetSample( i, t );
				if ( !sample->selected )
					continue;

				track->RemoveSample( i, t );
			}

			if ( !excise_time )
				continue;

	
			// Now shift things after m_flSelection[0] to the left
			for ( i = track->GetNumSamples( t ) - 1; i >= 0 ; i-- )
			{
				CExpressionSample *sample = track->GetSample( i, t );
				if ( sample->time < m_flSelection[ 1 ] )
					continue;

				// Shift it
				sample->time -= shift_left_time;
			}
		}

		item->DrawSelf();
	}

	g_pChoreoView->PushRedo( undotext );

	// Clear selection and redraw()
	DeselectAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ExpressionTool::OnResetItemSize()
{
	TimelineItem *item = m_pWorkspace->GetClickedItem();
	if ( !item )
		return;

	item->ResetHeight();
	m_pWorkspace->LayoutItems( true );
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ExpressionTool::OnResetAllItemSizes()
{
	for ( int controller = 0; controller < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; controller++ )
	{
		TimelineItem *item = m_pWorkspace->GetItem( controller );
		if ( !item )
			continue;
		item->ResetHeight();
	}

	m_pWorkspace->LayoutItems( true );
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ExpressionTool::OnScaleSamples()
{
	int t, i;

	//Scale samples
	CInputParams params;
	memset( &params, 0, sizeof( params ) );

	strcpy( params.m_szDialogTitle, "Scale selected samples" );
	strcpy( params.m_szPrompt, "Factor:" );
	strcpy( params.m_szInputText, "1.0" );

	if ( !InputProperties( &params ) )
		return;

	float scale_factor = atof( params.m_szInputText );
	if( scale_factor <= 0.0f )
	{
		Con_Printf( "Can't scale to %.2f\n", scale_factor );
	}

	char const *undotext = "Scale samples";

	g_pChoreoView->SetDirty( true );
	g_pChoreoView->PushUndo( undotext );

	for ( int controller = 0; controller < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; controller++ )
	{
		TimelineItem *item = m_pWorkspace->GetItem( controller );
		if ( !item )
			continue;

		CFlexAnimationTrack *track = item->GetSafeTrack();
		if ( !track )
			continue;

		for ( t = 0; t < 2; t++ )
		{
			for ( i = track->GetNumSamples( t ) - 1; i >= 0 ; i-- )
			{
				CExpressionSample *sample = track->GetSample( i, t );
				if ( !sample->selected )
					continue;

				// Scale it
				float curvalue = sample->value;
				curvalue *= scale_factor;
				// Clamp it
				curvalue = clamp( curvalue, 0.0f, 1.0f );
				sample->value = curvalue;
			}
		}
	}

	g_pChoreoView->PushRedo( undotext );

	m_pWorkspace->redraw();
	redraw();
}

void ExpressionTool::OnModelChanged()
{
	SetEvent( NULL );
	redraw();
}

void ExpressionTool::OnEdgeProperties()
{
	TimelineItem *item = m_pWorkspace->GetClickedItem();
	if ( !item )
		return;

	CFlexAnimationTrack *track = item->GetSafeTrack();
	if ( !track )
		return;

	CEdgePropertiesParams params;
	Q_memset( &params, 0, sizeof( params ) );
	Q_strcpy( params.m_szDialogTitle, "Edge Properties" );

	params.SetFromFlexTrack( track );

	if ( !EdgeProperties( &params ) )
	{
		return;
	}

	char const *undotext = "Change Edge Properties";
	
	g_pChoreoView->SetDirty( true );
	g_pChoreoView->PushUndo( undotext );

	// Apply changes.
	params.ApplyToTrack( track );

	g_pChoreoView->PushRedo( undotext );

	m_pWorkspace->redraw();
	redraw();
}

float ExpressionTool::GetScrubberSceneTime()
{
	CChoreoEvent *ev = GetSafeEvent();
	if ( !ev )
		return 0.0f;

	float curtime = GetScrub();
	curtime += ev->GetStartTime();
	return curtime;
}

void ExpressionTool::GetTimelineItems( CUtlVector< TimelineItem * >& list )
{
	for ( int i = 0; i < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; i++ )
	{
		TimelineItem *item = m_pWorkspace->GetItem( i );
		if ( !item )
			continue;

		list.AddToTail( item );
	}
}



bool ExpressionTool::HasCopiedColumn()
{
	return m_ColumnCopy.m_bActive;
}

void ExpressionTool::OnCopyColumn()
{
	m_ColumnCopy.Reset();

	m_ColumnCopy.m_bActive = true;
	m_ColumnCopy.m_flCopyTimes[ 0 ] = m_flSelection[ 0 ];
	m_ColumnCopy.m_flCopyTimes[ 1 ] = m_flSelection[ 1 ];

	for ( int controller = 0; controller < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; ++controller )
	{
		TimelineItem *item = m_pWorkspace->GetItem( controller );
		if ( !item )
			continue;

		CFlexAnimationTrack *track = item->GetSafeTrack();
		if ( !track )
			continue;

		for ( int t = 0; t < 2; t++ )
		{
			for ( int i = track->GetNumSamples( t ) - 1; i >= 0 ; i-- )
			{
				CExpressionSample *sample = track->GetSample( i, t );
				if ( !sample->selected )
					continue;

				// Add to dictionary
				CExpressionSample copy( *sample );
				copy.selected = false;

				int tIndex = m_ColumnCopy.m_Data.Find( track->GetFlexControllerName() );
				if ( tIndex == m_ColumnCopy.m_Data.InvalidIndex() )
				{
					tIndex = m_ColumnCopy.m_Data.Insert( track->GetFlexControllerName() );
				}

				CColumnCopier::CTrackData &data = m_ColumnCopy.m_Data[ tIndex ];
				data.m_Samples[ t ].AddToTail( copy );
			}
		}
	}
}

void ExpressionTool::OnPasteColumn()
{
	if ( !m_ColumnCopy.m_bActive )
	{
		Msg( "Nothing to paste\n" );
		return;
	}

	float flPasteTime = GetTimeForClickedPos();

	float flPasteEndTime = flPasteTime + m_ColumnCopy.m_flCopyTimes[ 1 ] - m_ColumnCopy.m_flCopyTimes[ 0 ];

	// Clear selection and redraw()
	DeselectAll();

	// Select everthing in the paste region so we can delete the existing stuff
	SelectPoints( flPasteTime, flPasteEndTime );

	int i, t;

	char const *undotext = "Paste column";

	g_pChoreoView->SetDirty( true );
	g_pChoreoView->PushUndo( undotext );

	for ( int controller = 0; controller < GLOBAL_STUDIO_FLEX_CONTROL_COUNT; controller++ )
	{
		TimelineItem *item = m_pWorkspace->GetItem( controller );
		if ( !item )
			continue;

		CFlexAnimationTrack *track = item->GetSafeTrack();
		if ( !track )
			continue;

		int tIndex = m_ColumnCopy.m_Data.Find( track->GetFlexControllerName() );

		for ( t = 0; t < 2; t++ )
		{
			// Remove all selected samples
			for ( i = track->GetNumSamples( t ) - 1; i >= 0 ; i-- )
			{
				CExpressionSample *sample = track->GetSample( i, t );
				if ( !sample->selected )
					continue;

				track->RemoveSample( i, t );
			}

			// Now add the new samples, if any in the time selection
			if ( tIndex != m_ColumnCopy.m_Data.InvalidIndex() )
			{
				CColumnCopier::CTrackData &data = m_ColumnCopy.m_Data[ tIndex ];

				for ( int j = 0; j < data.m_Samples[ t ].Count(); ++j )
				{
					CExpressionSample *s = &data.m_Samples[ t ][ j ];
					CExpressionSample *newSample = track->AddSample( s->time - m_ColumnCopy.m_flCopyTimes[ 0 ] + flPasteTime, s->value, t );
					newSample->selected = true;
				}
			}
			track->Resort( t );
		}

		item->DrawSelf();
	}

	g_pChoreoView->PushRedo( undotext );
}

void ExpressionTool::ClearColumnCopy()
{
	m_ColumnCopy.Reset();
}