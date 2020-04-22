//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "hlfaceposer.h"
#include <windows.h>
#include <stdio.h>
#include <mxtk/mxWindow.h>
#include <mxtk/mxScrollBar.h>
#include "mxexpressiontray.h"
#include "expressions.h"
#include "expclass.h"
#include "ControlPanel.h"
#include "FlexPanel.h"
#include <mxtk/mxPopupMenu.h>
#include "ChoreoView.h"
#include "StudioModel.h"
#include "ExpressionProperties.h"
#include "InputProperties.h"
#include "ViewerSettings.h"
#include "mxExpressionTab.h"
#include "choreowidgetdrawhelper.h"
#include "ExpressionTool.h"
#include "faceposer_models.h"
#include "tier0/icommandline.h"
#include "filesystem.h"

#define MAX_THUMBNAILSIZE 256
#define MIN_THUMBNAILSIZE 64
#define THUMBNAIL_SIZE_STEP 4
#define DEFAULT_THUMBNAIL_SIZE 128
#define TOP_GAP 45

mxExpressionTray *g_pExpressionTrayTool = 0;

mxExpressionTray::mxExpressionTray( mxWindow *parent, int id /*=0*/ )
: IFacePoserToolWindow( "ExpressionTrayTool", "Expressions" ), mxWindow( parent, 0, 0, 0, 0, "ExpressionTrayTool", id )
{
	setId( id );

	m_nTopOffset = 0;
	slScrollbar = new mxScrollbar( this, 0, 0, 18, 100, IDC_TRAYSCROLL, mxScrollbar::Vertical );

	m_nLastNumExpressions = -1;

	m_nGranularity = 10;

	m_nPrevCell = -1;
	m_nCurCell = -1;

	m_nClickedCell = -1;

	m_nButtonSquare = 16;

	m_nGap = 4;
	m_nDescriptionHeight = 34;
	m_nSnapshotWidth = g_viewerSettings.thumbnailsize;
	m_nSnapshotWidth = max( MIN_THUMBNAILSIZE, m_nSnapshotWidth );
	m_nSnapshotWidth = min( MAX_THUMBNAILSIZE, m_nSnapshotWidth );

	g_viewerSettings.thumbnailsize = m_nSnapshotWidth;

	m_nSnapshotHeight = m_nSnapshotWidth + m_nDescriptionHeight;

	m_pButtons = NULL;

	m_nPreviousExpressionCount = -1;

	m_bDragging = false;
	m_nDragCell = -1;

	CreateButtons();

	g_pExpressionClass = new mxExpressionTab( this, 5, 5, 500, 20, IDC_EXPRESSIONCLASS );

	m_pABButton = new mxButton( this, 520, 8, 50, 18, "A/B", IDC_AB );
	m_pThumbnailIncreaseButton = new mxButton( this, 0, 0, 18, 18, "+", IDC_THUMBNAIL_INCREASE );
	m_pThumbnailDecreaseButton = new mxButton( this, 0, 0, 18, 18, "-", IDC_THUMBNAIL_DECREASE );

}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : mxExpressionTray::~mxExpressionTray
//-----------------------------------------------------------------------------
mxExpressionTray::~mxExpressionTray ( void )
{
	DeleteAllButtons();
	g_pExpressionTrayTool = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : cellsize - 
//-----------------------------------------------------------------------------
void mxExpressionTray::SetCellSize( int cellsize )
{
	m_nSnapshotWidth = cellsize;
	m_nSnapshotHeight = cellsize + m_nDescriptionHeight;

	DeleteAllButtons();
	CreateButtons();

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void mxExpressionTray::Deselect( void )
{
	CExpClass *active = expressions->GetActiveClass();
	if ( active )
	{
		for ( int i = 0 ; i < active->GetNumExpressions(); i++ )
		{
			CExpression *exp = active->GetExpression( i );
			if ( exp )
			{
				exp->SetSelected( false );
			}
		}
	}

	m_nCurCell = m_nPrevCell = -1;
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : exp - 
//-----------------------------------------------------------------------------
void mxExpressionTray::Select( int exp, bool deselect /*=true*/ )
{
	int oldcell = m_nCurCell;

	if ( deselect )
	{
		Deselect();
	}

	m_nPrevCell = oldcell;
	m_nCurCell = exp;

	if ( m_nCurCell >= 0 )
	{
		CExpClass *active = expressions->GetActiveClass();
		if ( active )
		{
			CExpression *exp = active->GetExpression( m_nCurCell );
			if ( exp )
			{
				exp->SetSelected( true );
			}
		}
	}

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *btn - 
//-----------------------------------------------------------------------------
void mxExpressionTray::AddButton( const char *name, const char *tooltip, const char *bitmap, ETMEMBERFUNC pfnCallback,
	bool active, int x, int y, int w, int h )
{
	mxETButton *btn = new mxETButton;
	strcpy( btn->m_szName, name );
	strcpy( btn->m_szToolTip, tooltip );
	btn->m_bActive = active;
	btn->m_rc.left = x;
	btn->m_rc.top = y;
	btn->m_rc.right = x + w;
	btn->m_rc.bottom = y + h;

	btn->m_pImage = new mxbitmapdata_t;
	Assert( btn->m_pImage );
	btn->m_pImage->valid = false;
	LoadBitmapFromFile( bitmap, *btn->m_pImage );

	btn->m_fnCallback = pfnCallback;

	btn->next = m_pButtons;
	m_pButtons = btn;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void mxExpressionTray::CreateButtons( void )
{
	int x = m_nSnapshotWidth - 2 * ( m_nButtonSquare + 4 );
	int y = 4;

	AddButton( "undo", "Undo", "gfx/hlfaceposer/undo.bmp", &mxExpressionTray::ET_Undo, true, x, y, m_nButtonSquare, m_nButtonSquare );

	x += ( m_nButtonSquare + 4 );

	AddButton( "redo", "Redo", "gfx/hlfaceposer/redo.bmp", &mxExpressionTray::ET_Redo, true, x, y, m_nButtonSquare, m_nButtonSquare );
}

void mxExpressionTray::ActivateButton( const char *name, bool active )
{
	mxETButton *btn = FindButton( name );
	if ( !name )
		return;

	btn->m_bActive = active;
}

mxExpressionTray::mxETButton *mxExpressionTray::FindButton( const char *name )
{
	mxETButton *p = m_pButtons;
	while ( p )
	{
		if ( !stricmp( p->m_szName, name ) )
			return p;
		p = p->next;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void mxExpressionTray::DeleteAllButtons( void )
{
	mxETButton *p = m_pButtons, *n;
	while ( p )
	{
		n = p->next;
		delete p->m_pImage;
		delete p;
		p = n;
	}
	m_pButtons = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : x - 
//			y - 
// Output : mxExpressionTray::mxETButton
//-----------------------------------------------------------------------------
mxExpressionTray::mxETButton *mxExpressionTray::GetItemUnderCursor( int x, int y )
{
	// Convert to cell space
	int cell = GetCellUnderPosition( x, y );
	if ( cell == -1 )
	{
		return NULL;
	}

	// Cell is off screen?
	int cx, cy, cw, ch;
	if ( !ComputeRect( cell, cx, cy, cw, ch ) )
	{
		return NULL;
	}


	mxETButton *p = m_pButtons;
	while ( p )
	{
		if ( p->m_bActive &&
			x >= cx && 
			x <= cx + cw &&
			y >= cy &&
			y <= cy + ch )
		{
			// In-side cell
			int cellx = x - cx;
			int celly = y - cy;

			if ( cellx >= p->m_rc.left &&
				 cellx <= p->m_rc.right &&
				 celly >= p->m_rc.top &&
				 celly <= p->m_rc.bottom )
			{
				return p;
			}
		}
		p = p->next;
	}

	return NULL;
}

void mxExpressionTray::DrawButton( CChoreoWidgetDrawHelper& helper, int cell, mxETButton *btn )
{
	if ( !btn || !btn->m_pImage || !btn->m_pImage->valid )
		return;

	if ( !btn->m_bActive )
		return;

	int x, y, w, h;
	if ( !ComputeRect( cell, x, y, w, h ) )
		return;

	x += btn->m_rc.left;
	y += btn->m_rc.top;
	w = btn->m_rc.right - btn->m_rc.left;
	h = btn->m_rc.bottom - btn->m_rc.top;

	HDC dc = helper.GrabDC();

	DrawBitmapToDC( dc, x, y, w, h, *btn->m_pImage );
	helper.DrawOutlinedRect( RGB( 170, 170, 170 ), PS_SOLID, 1, x, y, x + w, y + h );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int mxExpressionTray::ComputePixelsNeeded( void )
{
	CExpClass *active = expressions->GetActiveClass();
	if ( !active )
		return 100;

	// Remove scroll bar
	int w = this->w2() - 16;

	int colsperrow;

	colsperrow = ( w - m_nGap ) / ( m_nSnapshotWidth + m_nGap );
	// At least one
	colsperrow = max( 1, colsperrow );

	int rowsneeded = ( ( active->GetNumExpressions() + colsperrow - 1 ) / colsperrow  );
	return rowsneeded * ( m_nSnapshotHeight + m_nGap ) + m_nGap + TOP_GAP + GetCaptionHeight();
}

bool mxExpressionTray::ComputeRect( int cell, int& rcx, int& rcy, int& rcw, int& rch )
{
	// Remove scroll bar
	int w = this->w2() - 16;

	int colsperrow;

	colsperrow = ( w - m_nGap ) / ( m_nSnapshotWidth + m_nGap );
	// At least one
	colsperrow = max( 1, colsperrow );

	int row, col;

	row = cell / colsperrow;
	col = cell % colsperrow;

	// don't allow partial columns

	rcx = m_nGap + col * ( m_nSnapshotWidth + m_nGap );
	rcy = GetCaptionHeight() + TOP_GAP + ( -m_nTopOffset * m_nGranularity ) + m_nGap + row * ( m_nSnapshotHeight + m_nGap );

	// Starts off screen
	if ( rcx < 0 )
		return false;

	// Ends off screen
	if ( rcx + m_nSnapshotWidth + m_nGap > this->w2() )
		return false;

	// Allow partial in y direction
	if ( rcy > this->h2() )
		return false;

	if ( rcy + m_nSnapshotHeight + m_nGap < 0 )
		return false;

	// Some portion is onscreen
	rcw = m_nSnapshotWidth;
	rch = m_nSnapshotHeight;
	return true;
}

void mxExpressionTray::DrawExpressionFocusRect( CChoreoWidgetDrawHelper& helper, int x, int y, int w, int h, COLORREF clr )
{
	helper.DrawOutlinedRect( clr, PS_SOLID, 4, x, y, x + w, y + h );
}

void mxExpressionTray::DrawExpressionDescription( CChoreoWidgetDrawHelper& helper, int x, int y, int w, int h, const char *expressionname, const char *description )
{
	int textheight = 15;

	RECT textRect;
	textRect.left = x + 5;
	textRect.top = y + h - 2 * textheight - 12;
	textRect.right = x + w - 10;
	textRect.bottom = y + h - 12;

	helper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 0, 0, 0 ), textRect, "%s", expressionname );

//	DrawText( hdc, expressionname, strlen( expressionname ), &textRect, DT_NOPREFIX | DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_WORD_ELLIPSIS );

	OffsetRect( &textRect, 0, textheight );

	helper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 63, 63, 63 ), textRect, "%s", description );

//	DrawText( hdc, description, strlen( description ), &textRect, DT_NOPREFIX | DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_WORD_ELLIPSIS );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dc - 
//			current - 
//			rcx - 
//			rcy - 
//			rcw - 
//			rch - 
//-----------------------------------------------------------------------------
void mxExpressionTray::DrawDirtyFlag( CChoreoWidgetDrawHelper& helper, CExpression *current, int rcx, int rcy, int rcw, int rch )
{
	// Not dirty
	if ( !current || ( !current->CanUndo() && !current->GetDirty() ) )
		return;

	int fontsize = 14;

	RECT textRect;
	textRect.left = rcx + 5;
	textRect.right = rcx + rcw;
	textRect.top = rcy + 5;
	textRect.bottom = textRect.top + fontsize + 2;

	helper.DrawColoredText( "Arial", fontsize, FW_NORMAL, RGB( 100, 240, 255 ), textRect, "*" );
}

bool mxExpressionTray::PaintBackground( void )
{
	redraw();
	return false;
}

void mxExpressionTray::DrawThumbNail( CExpClass *active, CExpression *current, CChoreoWidgetDrawHelper& helper, int rcx, int rcy, int rcw, int rch, int c, int selected, bool updateselection )
{
	if ( !current )
		return;

	HDC dc = helper.GrabDC();

	helper.DrawFilledRect( GetSysColor( COLOR_BTNFACE ), rcx, rcy, rcw + rcx, rch + rcy );

	if ( current->m_Bitmap[ models->GetActiveModelIndex() ].valid )
	{
		DrawBitmapToDC( dc, rcx, rcy, rcw, rch - m_nDescriptionHeight, current->m_Bitmap[ models->GetActiveModelIndex() ] );
		helper.DrawOutlinedRect( RGB( 127, 127, 127 ), PS_SOLID, 1, rcx, rcy, rcx + rcw, rcy + rch - m_nDescriptionHeight );
	}

	DrawDirtyFlag( helper, current, rcx, rcy, rcw, rch );

	DrawExpressionDescription( helper, rcx, rcy, rcw, rch, current->name, current->description );

	if ( c == selected )
	{
		DrawExpressionFocusRect( helper, rcx, rcy, rcw, rch - m_nDescriptionHeight, RGB( 255, 100, 63 ) );

		if ( updateselection )
		{
			m_nPrevCell = -1;
			m_nCurCell = c;
		}

		if ( current->CanUndo() || current->CanRedo() )
		{
			if ( current->CanUndo() )
			{
				DrawButton( helper, c, FindButton( "undo" ) );
			}

			if ( current->CanRedo() )
			{
				DrawButton( helper, c, FindButton( "redo" ) );
			}

			RECT rc;
			rc.left =  rcx + rcw - 2 * ( m_nButtonSquare + 4 );
			rc.top = rcy + m_nButtonSquare + 6;
			rc.right = rc.left + 2 * ( m_nButtonSquare + 4 );
			rc.bottom = rc.top + 15;

			helper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 200, 200, 200 ), rc, 
				"%i/%i", current->UndoCurrent(), current->UndoLevels() );
		}

	}
	else
	{
		if ( current->GetSelected() )
		{
			DrawExpressionFocusRect( helper, rcx, rcy, rcw, rch - m_nDescriptionHeight, RGB( 127, 127, 220 ) );
		}
	}
}

void mxExpressionTray::redraw()
{
	if ( !ToolCanDraw() )
		return;

	bool updateSelection = false;

	CExpClass *active = expressions->GetActiveClass();
	if ( active && active->GetNumExpressions() != m_nPreviousExpressionCount )
	{
		m_nTopOffset = 0;

		RepositionSlider();
		m_nPreviousExpressionCount = active->GetNumExpressions();
	}

	CChoreoWidgetDrawHelper helper( this, GetSysColor( COLOR_BTNFACE ) );
	HandleToolRedraw( helper );

	int w, h;
	w = w2();
	h = h2();

	if ( active )
	{
		RECT clipRect;
		helper.GetClientRect( clipRect );
		
		clipRect.top += TOP_GAP + GetCaptionHeight();

		helper.StartClipping( clipRect );

		if ( m_nLastNumExpressions != active->GetNumExpressions() )
		{
			m_nTopOffset = 0;
			m_nLastNumExpressions = active->GetNumExpressions();
			RepositionSlider();
			updateSelection = true;
		}

		int selected = active->GetSelectedExpression();

		int rcx, rcy, rcw, rch;

		int c = 0;
		while ( c < active->GetNumExpressions() )
		{
			if ( !ComputeRect( c, rcx, rcy, rcw, rch ) )
			{
				c++;
				continue;
			}

			CExpression *current = active->GetExpression( c );
			if ( !current )
				break;

			DrawThumbNail( active, current, helper, rcx, rcy, rcw, rch, c, selected, updateSelection );

			c++;
		}

		helper.StopClipping();

	}
	else
	{

		RECT rc;
		helper.GetClientRect( rc );

		// Arial 36 normal
		char sz[ 256 ];
		sprintf( sz, "No expression file loaded" );

		int pointsize = 18;

		int textlen = helper.CalcTextWidth( "Arial", pointsize, FW_NORMAL, sz );

		RECT rcText;
		rcText.top = ( rc.bottom - rc.top ) / 2 - pointsize / 2;
		rcText.bottom = rcText.top + pointsize + 10;
		int fullw = rc.right - rc.left;

		rcText.left = rc.left + ( fullw - textlen ) / 2;
		rcText.right = rcText.left + textlen;

		helper.DrawColoredText( "Arial", pointsize, FW_NORMAL,  RGB( 80, 80, 80 ), rcText, sz );
	}


// 	ValidateRect( (HWND)getHandle(), &rc );
}

int mxExpressionTray::GetCellUnderPosition( int x, int y )
{
	CExpClass *active = expressions->GetActiveClass();
	if ( !active )
		return -1;

	int rcx, rcy, rcw, rch;
	int c = 0;
	while ( c < active->GetNumExpressions() )
	{
		if ( !ComputeRect( c, rcx, rcy, rcw, rch ) )
		{
			c++;
			continue;
		}

		if ( x >= rcx && x <= rcx + rcw &&
			 y >= rcy && y <= rcy + rch )
		{
			return c;
		}

		c++;
	}
	return -1;
}

void mxExpressionTray::RepositionSlider( void )
{
	int trueh = h2() - GetCaptionHeight();

	int heightpixels = trueh / m_nGranularity;
	int rangepixels = ComputePixelsNeeded() / m_nGranularity;

	if ( rangepixels < heightpixels )
	{
		m_nTopOffset = 0;
		slScrollbar->setVisible( false );
	}
	else
	{
		slScrollbar->setVisible( true );
	}

	slScrollbar->setBounds( w2() - 16, GetCaptionHeight() + TOP_GAP, 16, trueh - TOP_GAP );

	m_nTopOffset = max( 0, m_nTopOffset );
	m_nTopOffset = min( rangepixels, m_nTopOffset );

	slScrollbar->setRange( 0, rangepixels );
	slScrollbar->setValue( m_nTopOffset );
	slScrollbar->setPagesize( heightpixels );
}

void mxExpressionTray::AB( void )
{
	if ( m_nPrevCell == -1 && m_nCurCell == -1 )
		return;

	CExpClass *active = expressions->GetActiveClass();
	if ( !active )
		return;

	if ( m_nPrevCell >= 0 && m_nPrevCell < active->GetNumExpressions() )
	{
		active->SelectExpression( m_nPrevCell );
	}
}

int mxExpressionTray::CountSelected( void )
{
	CExpClass *active = expressions->GetActiveClass();
	if ( !active )
		return 0;

	int c = 0;
	for ( int i = 0; i < active->GetNumExpressions(); i++ )
	{
		CExpression *exp = active->GetExpression( i );
		if ( !exp )
			continue;

		if ( exp->GetSelected() )
		{
			c++;
		}
	}

	return c;
}

void mxExpressionTray::SetClickedCell( int cell )
{
	m_nClickedCell = cell;
}

void mxExpressionTray::ShowRightClickMenu( int mx, int my )
{
	CExpClass *active = expressions->GetActiveClass();
	if ( !active )
		return;

	mxPopupMenu *pop = new mxPopupMenu();
	Assert( pop );

	CExpression *exp = NULL;
	if ( m_nClickedCell != -1 )
	{
		exp = active->GetExpression( m_nClickedCell );
	}

	pop->add( "New Expression...", IDC_CONTEXT_NEWEXP );
	if ( exp )
	{
		pop->addSeparator();
		pop->add( va( "Edit '%s'...", exp->name ), IDC_CONTEXT_EDITEXP );
		pop->add( va( "Save '%s'", exp->name ), IDC_CONTEXT_SAVEEXP );

		if ( exp->CanUndo() || exp->CanRedo() )
		{
			pop->add( va( "Revert '%s'", exp->name ), IDC_CONTEXT_REVERT );
		}
		pop->addSeparator();
		pop->add( va( "Delete '%s'", exp->name ), IDC_CONTEXT_DELETEXP );
		pop->addSeparator();
		pop->add( va( "Re-create thumbnail for '%s'", exp->name ), IDC_CONTEXT_CREATEBITMAP );
	}

	pop->popup( this, mx, my );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void mxExpressionTray::DrawFocusRect( void )
{
	HDC dc = GetDC( NULL );

	::DrawFocusRect( dc, &m_rcFocus );

	ReleaseDC( NULL, dc );
}

static bool IsWindowOrChild( mxWindow *parent, HWND test )
{
	HWND parentHwnd = (HWND)parent->getHandle();
	if ( test == parentHwnd || 
		IsChild( parentHwnd, test ) )
	{
		return true;
	}
	return false;
}

int mxExpressionTray::handleEvent (mxEvent *event)
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	int iret = 0;

	if ( HandleToolEvent( event ) )
	{
		return iret;
	}

	switch ( event->event )
	{
	case mxEvent::Action:
		{
			iret = 1;
			switch ( event->action )
			{
			default:
				iret = 0;
				break;
			case IDC_EXPRESSIONCLASS:
				{
					int index = g_pExpressionClass->getSelectedIndex();
					if ( index >= 0 )
					{
						CExpClass *current = expressions->GetClass( index );
						if ( current )
						{
							// Switch classname
							expressions->ActivateExpressionClass( current );
							current->SelectExpression( 0 );
						}
					}
				}
				break;
			case IDC_CONTEXT_NEWEXP:
				g_pFlexPanel->NewExpression();
				break;
			case IDC_CONTEXT_EDITEXP:
				if ( m_nClickedCell != -1 )
				{
					g_pFlexPanel->EditExpression();
				}
				break;
			case IDC_CONTEXT_REVERT:
				if ( m_nClickedCell != -1 )
				{
					g_pFlexPanel->RevertExpression( m_nClickedCell );
				}
				break;
			case IDC_CONTEXT_SAVEEXP:
				if ( m_nClickedCell != -1 )
				{
					g_pFlexPanel->SaveExpression( m_nClickedCell );
				}
				break;
			case IDC_CONTEXT_DELETEXP:
				if ( m_nClickedCell != -1 )
				{
					g_pControlPanel->DeleteExpression( m_nClickedCell );
				}
				break;
			case IDC_TRAYSCROLL:
				{
					if (event->modifiers == SB_THUMBTRACK)
					{
						int offset = event->height;
						
						slScrollbar->setValue( offset );
						
						m_nTopOffset = offset;
						
						redraw();
					}
					else if ( event->modifiers == SB_PAGEUP )
					{
						int offset = slScrollbar->getValue();
						
						offset -= m_nGranularity;
						offset = max( offset, slScrollbar->getMinValue() );
						
						slScrollbar->setValue( offset );
						InvalidateRect( (HWND)slScrollbar->getHandle(), NULL, TRUE );
						
						m_nTopOffset = offset;
						
						redraw();
					}
					else if ( event->modifiers == SB_PAGEDOWN )
					{
						int offset = slScrollbar->getValue();
						
						offset += m_nGranularity;
						offset = min( offset, slScrollbar->getMaxValue() );
						
						slScrollbar->setValue( offset );
						InvalidateRect( (HWND)slScrollbar->getHandle(), NULL, TRUE );
						
						m_nTopOffset = offset;
						
						redraw();
					}
				}
				break;
			case IDC_AB:
				{
					AB();	
				}
				break;
			case IDC_THUMBNAIL_INCREASE:
				{
					ThumbnailIncrease();
				}
				break;
			case IDC_THUMBNAIL_DECREASE:
				{
					ThumbnailDecrease();
				}
				break;
			case IDC_CONTEXT_CREATEBITMAP:
				{
					if ( m_nClickedCell >= 0 )
					{
						CExpClass *active = expressions->GetActiveClass();
						if ( active )
						{
							CExpression *exp = active->GetExpression( m_nClickedCell );
							if ( exp )
							{
								active->SelectExpression( m_nClickedCell );
								exp->CreateNewBitmap( models->GetActiveModelIndex() );
								redraw();
							}
						}
					}
				}
				break;
			}
			break;
		}
	case mxEvent::MouseDown:
		{
			if ( !( event->buttons & mxEvent::MouseRightButton ) )
			{
				// Figure out cell #
				int cell = GetCellUnderPosition( event->x, event->y );
				CExpClass *active = expressions->GetActiveClass();
				if ( active )
				{

					if ( cell == m_nCurCell && cell >= 0 && cell < active->GetNumExpressions() )
					{
						mxETButton *btn = GetItemUnderCursor( event->x, event->y );
						if ( btn && btn->m_fnCallback )
						{	
							(this->*(btn->m_fnCallback))( cell );
							return iret;
						}
					}
					
					if ( cell >= 0 && cell < active->GetNumExpressions() )
					{
						active->SelectExpression( cell, event->modifiers & mxEvent::KeyShift ? false : true );

						int cx, cy, cw, ch;
						if ( ComputeRect( cell, cx, cy, cw, ch ) )
						{
							m_bDragging = true;
							m_nDragCell = cell;
							
							m_nXStart = (short)event->x;
							m_nYStart = (short)event->y;

							m_rcFocus.left = cx;
							m_rcFocus.top = cy;
							m_rcFocus.right = cx + cw;
							m_rcFocus.bottom = cy + ch - m_nDescriptionHeight;

							POINT pt;
							pt.x = pt.y = 0;
							ClientToScreen( (HWND)getHandle(), &pt );

							OffsetRect( &m_rcFocus, pt.x, pt.y );

							m_rcOrig = m_rcFocus;

							DrawFocusRect();
						}
					}
					else
					{
						Deselect();
						active->DeselectExpression();
						redraw();
					}
				}
			}
			iret = 1;
		}
		break;
	case mxEvent::MouseDrag:
		{
			if ( m_bDragging )
			{
				// Draw drag line of some kind
				DrawFocusRect();
	
				// update pos
				m_rcFocus = m_rcOrig;
				OffsetRect( &m_rcFocus, ( (short)event->x - m_nXStart ), 
					( (short)event->y - m_nYStart ) );
				
				DrawFocusRect();
			}
			iret = 1;
		}
		break;
	case mxEvent::MouseUp:
		{
			iret = 1;

			if ( event->buttons & mxEvent::MouseRightButton )
			{
				SetClickedCell( GetCellUnderPosition( (short)event->x, (short)event->y ) );
				ShowRightClickMenu( (short)event->x, (short)event->y );
				return iret;
			}

			int cell = GetCellUnderPosition( event->x, event->y );
			CExpClass *active = expressions->GetActiveClass();

			if ( m_bDragging )
			{
				DrawFocusRect();
				m_bDragging = false;
				// See if we let go on top of the choreo view

				if ( active )
				{
					// Convert x, y to screen space
					POINT pt;
					pt.x = (short)event->x;
					pt.y = (short)event->y;
					ClientToScreen( (HWND)getHandle(), &pt );

					HWND maybeTool = WindowFromPoint( pt );

					// Now tell choreo view
					CExpression *exp = active->GetExpression( m_nDragCell );
					if ( exp && maybeTool )
					{
						if ( IsWindowOrChild( g_pChoreoView, maybeTool ) )
						{
							if ( g_pChoreoView->CreateExpressionEvent( pt.x, pt.y, active, exp ) )
							{
								return iret;
							}
						}
					
						if ( IsWindowOrChild( g_pExpressionTool, maybeTool ) )
						{
							if ( g_pExpressionTool->SetFlexAnimationTrackFromExpression( pt.x, pt.y, active, exp ) )
							{
								return iret;
							}
						}
					}
				}
			}

			if ( active )
			{
				// Over a new cell
				if ( cell >= 0 && 
					cell < active->GetNumExpressions() && 
					cell != m_nCurCell &&
					m_nCurCell != -1 )
				{
					// Swap cells
					CExpression *exp = active->GetExpression( m_nCurCell );
					if ( exp )
					{
						active->SwapExpressionOrder( m_nCurCell, cell );
						active->SetDirty( true );
						active->SelectExpression( cell );
					}
				}
			}
		}
		break;
	case mxEvent::Size:
		{
			int width = w2();

			int ch = GetCaptionHeight();

			g_pExpressionClass->setBounds( 5, 5 + ch, width - 120, 20 );

			m_pABButton->setBounds( width - 60, 4 + ch, 60, 16 );
			m_pThumbnailIncreaseButton->setBounds( width - 60 - 40, 4 + ch, 16, 16 );
			m_pThumbnailDecreaseButton->setBounds( width - 60 - 20, 4 + ch, 16, 16 );

			m_nTopOffset = 0;
			RepositionSlider();

			redraw();
			iret = 1;
		}
		break;
	case mxEvent::MouseWheeled:
		{
			// Figure out cell #
			POINT pt;

			pt.x = event->x;
			pt.y = event->y;

			ScreenToClient( (HWND)getHandle(), &pt );

			if ( event->height < 0 )
			{
				m_nTopOffset = min( m_nTopOffset + 10, slScrollbar->getMaxValue() );
			}
			else
			{
				m_nTopOffset = max( m_nTopOffset - 10, 0 );
			}
			RepositionSlider();
			redraw();
			iret = 1;
		}
		break;
	};

	if ( iret )
	{
		SetActiveTool( this );
	}
	return iret;
}

void mxExpressionTray::ET_Undo( int cell )
{
	g_pControlPanel->UndoExpression( cell );
}

void mxExpressionTray::ET_Redo( int cell )
{
	g_pControlPanel->RedoExpression( cell );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void mxExpressionTray::ThumbnailIncrease( void )
{
	if ( m_nSnapshotWidth + THUMBNAIL_SIZE_STEP <= MAX_THUMBNAILSIZE )
	{
		m_nSnapshotWidth += THUMBNAIL_SIZE_STEP;
		g_viewerSettings.thumbnailsize = m_nSnapshotWidth;
		m_nSnapshotHeight = m_nSnapshotWidth + m_nDescriptionHeight;

		Con_Printf( "Thumbnail size %i x %i\n", m_nSnapshotWidth, m_nSnapshotWidth );

		redraw();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void mxExpressionTray::ThumbnailDecrease( void )
{
	if ( m_nSnapshotWidth - THUMBNAIL_SIZE_STEP >= MIN_THUMBNAILSIZE )
	{
		m_nSnapshotWidth -= THUMBNAIL_SIZE_STEP;
		g_viewerSettings.thumbnailsize = m_nSnapshotWidth;
		m_nSnapshotHeight = m_nSnapshotWidth + m_nDescriptionHeight;
		
		Con_Printf( "Thumbnail size %i x %i\n", m_nSnapshotWidth, m_nSnapshotWidth );

		redraw();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void mxExpressionTray::RestoreThumbnailSize( void )
{
	m_nSnapshotWidth = g_viewerSettings.thumbnailsize;
	m_nSnapshotWidth = max( MIN_THUMBNAILSIZE, m_nSnapshotWidth );
	m_nSnapshotWidth = min( MAX_THUMBNAILSIZE, m_nSnapshotWidth );

	g_viewerSettings.thumbnailsize = m_nSnapshotWidth;

	m_nSnapshotHeight = m_nSnapshotWidth + m_nDescriptionHeight;

	redraw();
}

void mxExpressionTray::ReloadBitmaps( void )
{
	CExpClass *cl;
	int c = expressions->GetNumClasses();
	EnableStickySnapshotMode();
	for ( int i = 0 ; i < c; i++ )
	{
		cl = expressions->GetClass( i );
		if ( !cl )
			continue;

		cl->ReloadBitmaps();
	}
	DisableStickySnapshotMode();
	redraw();
}

bool IsUsingPerPlayerExpressions()
{
	bool bPerPlayerExpressions = false;
	if ( CommandLine()->CheckParm( "-perplayerexpressions" ) )
	{
		bPerPlayerExpressions = true;
	}
	else
	{
		// Returns the search path, each path is separated by ;s. Returns the length of the string returned
		char pSearchPath[2048];
		if ( g_pFullFileSystem->GetSearchPath( "GAME", false, pSearchPath, sizeof(pSearchPath) ) )
		{
			Q_FixSlashes( pSearchPath );
			if ( Q_stristr( pSearchPath, "\\tf" ) )
			{
				bPerPlayerExpressions = true;
			}
		}
	}
	return bPerPlayerExpressions;
}

void mxExpressionTray::OnModelChanged()
{
	if ( IsUsingPerPlayerExpressions() )
	{
		Msg( "Closing current phoneme set\n" );

		if ( !g_pControlPanel->Closeall() )
			return;

		// See if per-model overrides exist for this model
		char fn[ MAX_PATH ];
		Q_snprintf( fn, sizeof( fn ), "expressions/%s/phonemes/phonemes.txt", models->GetActiveModelName() );

		// Load appropriate classes
		char rootDir[ MAX_PATH ];
		Q_snprintf( rootDir, sizeof( rootDir ), "%s/phonemes/", models->GetActiveModelName() );
		FacePoser_SetPhonemeRootDir( rootDir );

		FacePoser_EnsurePhonemesLoaded();
	}

	ReloadBitmaps();
	RestoreThumbnailSize();
}