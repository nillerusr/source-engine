//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include <windows.h>
#include "AnimationBrowser.h"
#include "hlfaceposer.h"
#include "ChoreoView.h"
#include "StudioModel.h"
#include "ViewerSettings.h"
#include "choreowidgetdrawhelper.h"
#include "faceposer_models.h"
#include "tabwindow.h"
#include "inputproperties.h"
#include "KeyValues.h"
#include "filesystem.h"
#include "tier1/KeyValues.h"
#include "tier1/UtlBuffer.h"

#define MAX_THUMBNAILSIZE 256
#define MIN_THUMBNAILSIZE 64
#define THUMBNAIL_SIZE_STEP 4
#define DEFAULT_THUMBNAIL_SIZE 128
#define TOP_GAP 70

AnimationBrowser *g_pAnimationBrowserTool = 0;
extern double realtime;

void CreatePath( const char *pPath );

void CCustomAnim::LoadFromFile()
{
	char fn[ 512 ];
	if ( !filesystem->String( m_Handle, fn, sizeof( fn ) ) )
		return;

	KeyValues *kv = new KeyValues( "CustomAnimation" );
	if ( kv->LoadFromFile( filesystem, fn, "MOD" ) )
	{
		for ( KeyValues *sub = kv->GetFirstSubKey(); sub ; sub = sub->GetNextKey() )
		{
			CUtlSymbol anim;
			anim = sub->GetString();

			m_Animations.AddToTail( anim );
		}
	}
	kv->deleteThis();
}

void CCustomAnim::SaveToFile()
{
	char fn[ 512 ];
	if ( !filesystem->String( m_Handle, fn, sizeof( fn ) ) )
		return;

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	buf.Printf( "\"%s\"\n", m_ShortName.String() );
	buf.Printf( "{\n" );
	for ( int i = 0; i < m_Animations.Count(); ++i )
	{
		buf.Printf( "\t\"item%d\" \"%s\"\n", i + 1, m_Animations[ i ].String() );
	}
	buf.Printf( "}\n" );

	CreatePath( fn );
	filesystem->WriteFile( fn, "MOD", buf );
}

bool CCustomAnim::HasAnimation( char const *search )
{
	CUtlSymbol searchSym;
	searchSym = search;
	if ( m_Animations.Find( searchSym ) != m_Animations.InvalidIndex() )
		return true;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CAnimBrowserTab : public CTabWindow
{
	typedef CTabWindow BaseClass;

public:


	CAnimBrowserTab( AnimationBrowser *parent, int x, int y, int w, int h, int id = 0, int style = 0 ) :
		CTabWindow( (mxWindow *)parent, x, y, w, h, id, style )
	{
		// SetInverted( true );
	}

	void Init( void )
	{
		add( "all" );
		add( "gestures" );
		add( "postures" );
		add( "search results" );
	}

	virtual void ShowRightClickMenu( int mx, int my )
	{
		POINT pt;
		GetCursorPos( &pt );
		ScreenToClient( (HWND)getHandle(), &pt );

		// New scene, edit comments
		mxPopupMenu *pop = new mxPopupMenu();

		pop->add ("&New Group...", IDC_AB_CREATE_CUSTOM );

		mxPopupMenu *sub = NULL;
		for ( int i = 0; i < m_CustomGroups.Count(); ++i )
		{
			if ( !sub ) 
			{
				sub = new mxPopupMenu();
			}
			sub->add( va( "%s", m_CustomGroups[ i ].String() ), IDC_AB_DELETEGROUPSTART + i ); 
		}
		if ( sub )
		{
			pop->addMenu( "Delete Group", sub );
		}

		pop->addSeparator();

		sub = new mxPopupMenu();
		for ( int i = 0; i < m_CustomGroups.Count(); ++i )
		{
			sub->add( va( "%s", m_CustomGroups[ i ].String() ), IDC_AB_RENAMEGROUPSTART + i ); 
		}

		pop->addMenu( "Rename Group", sub );

		pop->popup( getParent(), pt.x, pt.y );
	}

	void UpdateCustomTabs( CUtlVector< CCustomAnim * >& list )
	{
		m_CustomGroups.Purge();

		while ( getItemCount() > AnimationBrowser::FILTER_FIRST_CUSTOM )
		{
			remove( getItemCount() - 1 );
		}

		for ( int i = 0; i < list.Count(); ++i )
		{
			const CCustomAnim *anim = list[ i ];
			add( anim->m_ShortName.String() );
			m_CustomGroups.AddToTail( anim->m_ShortName );
		}
	}

private:

	CUtlVector< CUtlSymbol >	m_CustomGroups;
};



AnimationBrowser::AnimationBrowser( mxWindow *parent, int id /*=0*/ )
: IFacePoserToolWindow( "AnimationBrowser", "Animations" ), 
	mxWindow( parent, 0, 0, 0, 0, "AnimationBrowser", id )
{
	setId( id );

	m_nTopOffset = 0;
	slScrollbar = new mxScrollbar( this, 0, 0, 18, 100, IDC_AB_TRAYSCROLL, mxScrollbar::Vertical );

	m_nLastNumAnimations = -1;

	m_nGranularity = 10;

	m_nCurCell = -1;

	m_nClickedCell = -1;

	m_nGap = 4;
	m_nDescriptionHeight = 34;
	m_nSnapshotWidth = g_viewerSettings.thumbnailsizeanim;
	m_nSnapshotWidth = max( MIN_THUMBNAILSIZE, m_nSnapshotWidth );
	m_nSnapshotWidth = min( MAX_THUMBNAILSIZE, m_nSnapshotWidth );

	g_viewerSettings.thumbnailsizeanim = m_nSnapshotWidth;

	m_nSnapshotHeight = m_nSnapshotWidth + m_nDescriptionHeight;

	m_bDragging = false;
	m_nDragCell = -1;

	m_szSearchString[0]=0;

	m_pFilterTab = new CAnimBrowserTab( this, 5, 5, 240, 20, IDC_AB_FILTERTAB );
	m_pFilterTab->Init();

	m_pSearchEntry = new mxLineEdit( this, 0, 0, 0, 0, "" );

	m_pThumbnailIncreaseButton = new mxButton( this, 0, 0, 18, 18, "+", IDC_AB_THUMBNAIL_INCREASE );
	m_pThumbnailDecreaseButton = new mxButton( this, 0, 0, 18, 18, "-", IDC_AB_THUMBNAIL_DECREASE );
	m_nCurFilter = FILTER_NONE;

	m_flDragTime = 0.0f;

	OnFilter();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : AnimationBrowser::~AnimationBrowser
//-----------------------------------------------------------------------------
AnimationBrowser::~AnimationBrowser ( void )
{
	g_pAnimationBrowserTool = NULL;
}

void AnimationBrowser::Shutdown()
{
	PurgeCustom();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : cellsize - 
//-----------------------------------------------------------------------------
void AnimationBrowser::SetCellSize( int cellsize )
{
	m_nSnapshotWidth = cellsize;
	m_nSnapshotHeight = cellsize + m_nDescriptionHeight;

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void AnimationBrowser::Deselect( void )
{
	m_nCurCell = -1;
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : exp - 
//-----------------------------------------------------------------------------
void AnimationBrowser::Select( int sequence )
{
	m_nCurCell = sequence;
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int AnimationBrowser::ComputePixelsNeeded( void )
{
	int seqcount = GetSequenceCount();

	if ( !seqcount )
		return 100;

	// Remove scroll bar
	int w = this->w2() - 16;

	int colsperrow;

	colsperrow = ( w - m_nGap ) / ( m_nSnapshotWidth + m_nGap );
	// At least one
	colsperrow = max( 1, colsperrow );

	int rowsneeded = ( ( seqcount + colsperrow - 1 ) / colsperrow  );
	return rowsneeded * ( m_nSnapshotHeight + m_nGap ) + m_nGap + TOP_GAP + GetCaptionHeight();
}

bool AnimationBrowser::ComputeRect( int cell, int& rcx, int& rcy, int& rcw, int& rch )
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

void AnimationBrowser::DrawSequenceFocusRect( CChoreoWidgetDrawHelper& helper, int x, int y, int w, int h, COLORREF clr )
{
	helper.DrawOutlinedRect( clr, PS_SOLID, 4, x, y, x + w, y + h );
}

void AnimationBrowser::DrawSequenceDescription( CChoreoWidgetDrawHelper& helper, int x, int y, int w, int h, int sequence, mstudioseqdesc_t &seqdesc )
{
	int textheight = 15;

	RECT textRect;
	textRect.left = x + 5;
	textRect.top = y + h - 2 * textheight - 12;
	textRect.right = x + w - 10;
	textRect.bottom = y + h - 12;

	helper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 63, 63, 63 ), textRect, "%s", seqdesc.pszLabel() );

	StudioModel *mdl = models->GetActiveStudioModel();
	if ( !mdl )
		return;

	OffsetRect( &textRect, 0, textheight );

	helper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 63, 63, 63 ), textRect, "%.2f seconds", 
		mdl->GetDuration( sequence ) );

	textRect.top = y + h - 4 * textheight - 1;
	textRect.bottom = textRect.top + textheight;

	helper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 50, 200, 255 ), textRect, "frames %i", 
		mdl->GetNumFrames( sequence ) );

	OffsetRect( &textRect, 0, textheight - 4 );
	
	helper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 50, 200, 255 ), textRect, "fps %.2f", 
		(float)mdl->GetFPS( sequence ) );

}

bool AnimationBrowser::PaintBackground( void )
{
	redraw();
	return false;
}

void AnimationBrowser::DrawThumbNail( int sequence, CChoreoWidgetDrawHelper& helper, int rcx, int rcy, int rcw, int rch )
{
	HDC dc = helper.GrabDC();

	helper.DrawFilledRect( GetSysColor( COLOR_BTNFACE ), rcx, rcy, rcw + rcx, rch + rcy );

	mstudioseqdesc_t *pseqdesc = GetSeqDesc( sequence );
	if ( !pseqdesc )
		return;
	
	mxbitmapdata_t *bm = models->GetBitmapForSequence( models->GetActiveModelIndex(), TranslateSequenceNumber( sequence ) );
	if ( bm && bm->valid )
	{
		DrawBitmapToDC( dc, rcx, rcy, rcw, rch - m_nDescriptionHeight, *bm );
		helper.DrawOutlinedRect( RGB( 127, 127, 127 ), PS_SOLID, 1, rcx, rcy, rcx + rcw, rcy + rch - m_nDescriptionHeight );
	}

	DrawSequenceDescription( helper, rcx, rcy, rcw, rch, TranslateSequenceNumber( sequence ), *pseqdesc );

	if ( sequence == m_nCurCell )
	{
		DrawSequenceFocusRect( helper, rcx, rcy, rcw, rch - m_nDescriptionHeight, RGB( 255, 100, 63 ) );
	}
}

void AnimationBrowser::redraw()
{
	if ( !ToolCanDraw() )
		return;

	bool updateSelection = false;

	int curcount = GetSequenceCount();
	if ( curcount != m_nLastNumAnimations )
	{
		m_nTopOffset = 0;
		RepositionSlider();
		m_nLastNumAnimations = curcount;
		updateSelection = true;
	}

	CChoreoWidgetDrawHelper helper( this, GetSysColor( COLOR_BTNFACE ) );
	HandleToolRedraw( helper );

	int w, h;
	w = w2();
	h = h2();

	RECT clipRect;
	helper.GetClientRect( clipRect );
	
	clipRect.top += TOP_GAP + GetCaptionHeight();

	helper.StartClipping( clipRect );

	int rcx, rcy, rcw, rch;

	EnableStickySnapshotMode( );

	int c = curcount;
	for ( int i = 0; i < c; i++ )
	{
		if ( !ComputeRect( i, rcx, rcy, rcw, rch ) )
		{
			// Cache in .bmp no matter what
			// This was too slow, so turning it back off
			//models->GetBitmapForSequence( models->GetActiveModelIndex(), TranslateSequenceNumber( i ) );
			continue;
		}

		DrawThumbNail( i, helper, rcx, rcy, rcw, rch );
	}

	DisableStickySnapshotMode( );

	helper.StopClipping();

	RECT rcText;
	rcText.right = w2();
	rcText.left = rcText.right - 120;
	rcText.top = 8;
	rcText.bottom = rcText.top + 15;

	helper.DrawColoredText( "Arial", 9, FW_NORMAL, RGB( 63, 63, 63 ), rcText, "%i sequences", 
		curcount );

}

int AnimationBrowser::GetCellUnderPosition( int x, int y )
{
	int count = GetSequenceCount();
	if ( !count )
		return -1;

	int rcx, rcy, rcw, rch;
	int c = 0;
	while ( c < count )
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

void AnimationBrowser::RepositionSlider( void )
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

void AnimationBrowser::SetClickedCell( int cell )
{
	m_nClickedCell = cell;
	Select( cell );
}

void AnimationBrowser::ShowRightClickMenu( int mx, int my )
{
	mstudioseqdesc_t *pseqdesc = GetSeqDesc( m_nCurCell );
	if ( !pseqdesc )
		return;

	mxPopupMenu *pop = new mxPopupMenu();
	Assert( pop );

	pop->add( va( "New Group..." ), IDC_AB_CREATE_CUSTOM );

	if ( m_CustomAnimationTabs.Count() > 0 )
	{
		mxPopupMenu *ca = new mxPopupMenu();
		Assert( ca );

		for ( int i = 0; i < m_CustomAnimationTabs.Count() ; ++i )
		{
			CCustomAnim *anim = m_CustomAnimationTabs[ i ];
			ca->add( va( "%s", anim->m_ShortName.String() ), IDC_AB_ADDTOGROUPSTART + i );
		}

		pop->addMenu( "Add to Group", ca );

		ca = new mxPopupMenu();

		bool useMenu = false;
		for ( int i = 0; i < m_CustomAnimationTabs.Count() ; ++i )
		{
			CCustomAnim *anim = m_CustomAnimationTabs[ i ];
			if ( anim->HasAnimation( pseqdesc->pszLabel() ) )
			{
                ca->add( va( "%s", anim->m_ShortName.String() ), IDC_AB_REMOVEFROMGROUPSTART + i );
				useMenu = true;
			}
		}

		if ( useMenu )
		{
			pop->addMenu( "Remove from Group", ca );
		}
		else
		{
			delete ca;
		}
	}

	pop->addSeparator();

	pop->add( va( "Re-create thumbnail for '%s'", pseqdesc->pszLabel() ), IDC_AB_CONTEXT_CREATEBITMAP );
	pop->add( va( "Re-create all thumbnails" ), IDC_AB_CONTEXT_CREATEALLBITMAPS );

	pop->popup( this, mx, my );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void AnimationBrowser::DrawFocusRect( void )
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

int AnimationBrowser::handleEvent (mxEvent *event)
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
				if ( event->action >= IDC_AB_ADDTOGROUPSTART && event->action <= IDC_AB_ADDTOGROUPEND )
				{
					int index = event->action - IDC_AB_ADDTOGROUPSTART;
					mstudioseqdesc_t *pseqdesc = GetSeqDesc( m_nCurCell );
					if ( pseqdesc )
					{
						AddAnimationToCustomFile( index, pseqdesc->pszLabel() );
					}
				}
				else if ( event->action >= IDC_AB_REMOVEFROMGROUPSTART && event->action <= IDC_AB_REMOVEFROMGROUPEND )
				{
					int index = event->action - IDC_AB_REMOVEFROMGROUPSTART;
					mstudioseqdesc_t *pseqdesc = GetSeqDesc( m_nCurCell );
					if ( pseqdesc )
					{
						RemoveAnimationFromCustomFile( index, pseqdesc->pszLabel() );
					}
				}
				else if ( event->action >= IDC_AB_DELETEGROUPSTART && event->action <= IDC_AB_DELETEGROUPEND )
				{
					int index = event->action - IDC_AB_DELETEGROUPSTART;
					DeleteCustomFile( index );
				}
				else if ( event->action >= IDC_AB_RENAMEGROUPSTART && event->action <= IDC_AB_RENAMEGROUPEND )
				{
					int index = event->action - IDC_AB_RENAMEGROUPSTART;
					RenameCustomFile( index );
				}
				else
				{
					iret = 0;
				}
				break;
			case IDC_AB_CREATE_CUSTOM:
				{
					OnAddCustomAnimationFilter();
				}
				break;
			case IDC_AB_FILTERTAB:
				{
					int index = m_pFilterTab->getSelectedIndex();
					if ( index >= 0 )
					{
						m_nCurFilter = index;
						OnFilter();
					}
				}
				break;
			case IDC_AB_TRAYSCROLL:
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
			case IDC_AB_THUMBNAIL_INCREASE:
				{
					ThumbnailIncrease();
				}
				break;
			case IDC_AB_THUMBNAIL_DECREASE:
				{
					ThumbnailDecrease();
				}
				break;
			case IDC_AB_CONTEXT_CREATEBITMAP:
				{
					int current_model = models->GetActiveModelIndex();

					if ( m_nClickedCell >= 0 )
					{
						models->RecreateAnimationBitmap( current_model, TranslateSequenceNumber( m_nClickedCell ) );
					}
					redraw();
				}
				break;
			case IDC_AB_CONTEXT_CREATEALLBITMAPS:
				{
					int current_model = models->GetActiveModelIndex();
					models->RecreateAllAnimationBitmaps( current_model );
					redraw();
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
				if ( cell >= 0 && cell < GetSequenceCount() )
				{
					int cx, cy, cw, ch;
					if ( ComputeRect( cell, cx, cy, cw, ch ) )
					{
						m_flDragTime = realtime;
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

						Select( cell );
						m_nClickedCell = cell;
					}
				}
				else
				{
					Deselect();
					redraw();
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

			if ( m_bDragging && m_nClickedCell >= 0 )
			{
				mstudioseqdesc_t *pseqdesc = GetSeqDesc( m_nClickedCell );

				DrawFocusRect();
				m_bDragging = false;
				// See if we let go on top of the choreo view

				// Convert x, y to screen space
				POINT pt;
				pt.x = (short)event->x;
				pt.y = (short)event->y;
				ClientToScreen( (HWND)getHandle(), &pt );

				HWND maybeTool = WindowFromPoint( pt );

				// Now tell choreo view
				if ( maybeTool && pseqdesc )
				{
					if ( IsWindowOrChild( g_pChoreoView, maybeTool ) )
					{
						if ( g_pChoreoView->CreateAnimationEvent( pt.x, pt.y, pseqdesc->pszLabel() ) )
						{
							return iret;
						}
					}
				}
			}
		}
		break;
	case mxEvent::Size:
		{
			int width = w2();
	//		int height = h2();

			int ch = GetCaptionHeight() + 10;

			m_pSearchEntry->setBounds( 5, ch, width - 10 - 170, 18 );

			m_pThumbnailIncreaseButton->setBounds( width - 40, 4 + ch, 16, 16 );
			m_pThumbnailDecreaseButton->setBounds( width - 20, 4 + ch, 16, 16 );

			m_pFilterTab->setBounds( 5, ch + 20, width - 10, 20 );

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
	case mxEvent::KeyDown:
	case mxEvent::KeyUp:
		{
			bool search = false;
			// int n = 3;
			if ( event->key == VK_ESCAPE && m_szSearchString[ 0 ] )
			{
				m_pSearchEntry->setLabel( "" );
				m_szSearchString[ 0 ] = 0;
				m_pFilterTab->select( FILTER_NONE );
				m_nCurFilter = FILTER_NONE;
				OnFilter();
			}
			else
			{
				// Text changed?
				char sz[ 512 ];
				m_pSearchEntry->getText( sz, sizeof( sz ) );
				if ( Q_stricmp( sz, m_szSearchString ) )
				{
					Q_strncpy( m_szSearchString, sz, sizeof( m_szSearchString ) );
					search = true;
				}
			}

			if ( search )
			{
				if ( Q_strlen( m_szSearchString ) > 0 )
				{
					m_pFilterTab->select( FILTER_STRING );
					m_nCurFilter = FILTER_STRING;
				}
				else
				{
					m_pFilterTab->select( FILTER_NONE );
					m_nCurFilter = FILTER_NONE;
				}
				OnFilter();
			}
		}
		break;
	};

	if ( iret )
	{
		SetActiveTool( this );
	}
	return iret;
}

// HACK HACK:  VS2005 is generating bogus code for this little operation in the function below...
#pragma optimize( "g", off )
float roundcycle( float cycle )
{
	int rounded = (int)(cycle);
	float cy2 = cycle - rounded;
	return cy2;
}
#pragma optimize( "", on )

void AnimationBrowser::Think( float dt )
{
	if ( !m_bDragging )
		return;

	if ( m_nClickedCell < 0 )
		return;

	StudioModel *model = models->GetActiveStudioModel();
	if ( model )
	{
		int iSequence = TranslateSequenceNumber( m_nClickedCell );
		float dur = model->GetDuration( iSequence );
		if ( dur > 0.0f )
		{
			float elapsed = (float)realtime - m_flDragTime;

			float flFrameRate = 0.0f;
			float flGroundSpeed = 0.0f;
			model->GetSequenceInfo( iSequence, &flFrameRate, &flGroundSpeed );

			float cycle = roundcycle( elapsed * flFrameRate );

			// This should be the only thing on the model!!!
			model->ClearAnimationLayers();

			// FIXME: shouldn't sequences always be lower priority than gestures?
			int iLayer = model->GetNewAnimationLayer( 0 );
			model->SetOverlaySequence( iLayer, iSequence, 1.0f );
			model->SetOverlayRate( iLayer, cycle, 0.0f );
		}
	}
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void AnimationBrowser::ThumbnailIncrease( void )
{
	if ( m_nSnapshotWidth + THUMBNAIL_SIZE_STEP <= MAX_THUMBNAILSIZE )
	{
		m_nSnapshotWidth += THUMBNAIL_SIZE_STEP;
		g_viewerSettings.thumbnailsizeanim = m_nSnapshotWidth;
		m_nSnapshotHeight = m_nSnapshotWidth + m_nDescriptionHeight;

		Con_Printf( "Thumbnail size %i x %i\n", m_nSnapshotWidth, m_nSnapshotWidth );

		redraw();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void AnimationBrowser::ThumbnailDecrease( void )
{
	if ( m_nSnapshotWidth - THUMBNAIL_SIZE_STEP >= MIN_THUMBNAILSIZE )
	{
		m_nSnapshotWidth -= THUMBNAIL_SIZE_STEP;
		g_viewerSettings.thumbnailsizeanim = m_nSnapshotWidth;
		m_nSnapshotHeight = m_nSnapshotWidth + m_nDescriptionHeight;
		
		Con_Printf( "Thumbnail size %i x %i\n", m_nSnapshotWidth, m_nSnapshotWidth );

		redraw();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void AnimationBrowser::RestoreThumbnailSize( void )
{
	m_nSnapshotWidth = g_viewerSettings.thumbnailsizeanim;
	m_nSnapshotWidth = max( MIN_THUMBNAILSIZE, m_nSnapshotWidth );
	m_nSnapshotWidth = min( MAX_THUMBNAILSIZE, m_nSnapshotWidth );

	g_viewerSettings.thumbnailsizeanim = m_nSnapshotWidth;

	m_nSnapshotHeight = m_nSnapshotWidth + m_nDescriptionHeight;

	redraw();
}

void AnimationBrowser::ReloadBitmaps( void )
{
	Assert( 0 );
	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *model - 
//			sequence - 
// Output : static bool
//-----------------------------------------------------------------------------
static bool IsTypeOfSequence( StudioModel *model, int sequence, char const *typestring )
{
	bool match = false;

	if ( !model->GetStudioHdr() )
		return match;

	KeyValues *seqKeyValues = new KeyValues("");
	if ( seqKeyValues->LoadFromBuffer( model->GetFileName( ), model->GetKeyValueText( sequence ) ) )
	{
		// Do we have a build point section?
		KeyValues *pkvAllFaceposer = seqKeyValues->FindKey("faceposer");
		if ( pkvAllFaceposer )
		{
			char const *t = pkvAllFaceposer->GetString( "type", "" );
			if ( t && !Q_stricmp( t, typestring ) )
			{
				match = true;
			}
		}
	}

	seqKeyValues->deleteThis();

	return match;
}

bool AnimationBrowser::SequencePassesFilter( StudioModel *model, int sequence, mstudioseqdesc_t &seqdesc )
{
	if (model->IsHidden( sequence ))
		return false;

	switch ( m_nCurFilter )
	{
	default:
		{

			int offset = m_nCurFilter - FILTER_FIRST_CUSTOM;
			if ( offset >= 0 && offset < m_CustomAnimationTabs.Count() )
			{
				// Find the name
				CCustomAnim *anim = m_CustomAnimationTabs[ offset ];
				return anim->HasAnimation( seqdesc.pszLabel() );
			}
			return true;
		}
		break;
	case FILTER_NONE:
		{
			return true;
		}
		break;
	case FILTER_GESTURES:
		if ( IsTypeOfSequence( model, sequence, "gesture" ) )
		{
			return true;
		}
		break;
	case FILTER_POSTURES:
		if ( IsTypeOfSequence( model, sequence, "posture" ) )
		{
			return true;
		}
		break;
	case FILTER_STRING:
		if ( Q_stristr( seqdesc.pszLabel(), m_szSearchString ) )
		{
			return true;
		}
	}

	return false;
}

void AnimationBrowser::OnFilter()
{
	m_Filtered.RemoveAll();

	StudioModel *model = models->GetActiveStudioModel();
	if ( !model )
		return;

	CStudioHdr *hdr = model->GetStudioHdr();
	if ( !hdr )
		return;

	int count = hdr->GetNumSeq();

	for ( int i = 0; i < count; i++ )
	{
		mstudioseqdesc_t &seqdesc = hdr->pSeqdesc( i );

		// if it passes the filter, add it
		if ( SequencePassesFilter( model, i, seqdesc ) )
		{
			m_Filtered.AddToTail( i );
		}
	}

	redraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int AnimationBrowser::GetSequenceCount()
{
	return m_Filtered.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : mstudioseqdesc_t
//-----------------------------------------------------------------------------
mstudioseqdesc_t *AnimationBrowser::GetSeqDesc( int index )
{
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if ( !hdr )
		return NULL;

	index = TranslateSequenceNumber( index );

	if ( index < 0 || index >= hdr->GetNumSeq() )
		return NULL;

	return &hdr->pSeqdesc( index );
}

int	 AnimationBrowser::TranslateSequenceNumber( int index )
{
	if ( index < 0 || index >= m_Filtered.Count() )
		return NULL;

	// Lookup the true index
	index = m_Filtered[ index ];
	return index;
}

void AnimationBrowser::FindCustomFiles( char const *subdir, CUtlVector< FileNameHandle_t >& files )
{
	char search[ 512 ];
	Q_snprintf( search, sizeof( search ), "%s/*.txt", subdir );

	FileFindHandle_t findHandle;
	const char *pFileName = filesystem->FindFirst( search, &findHandle );
	while( pFileName )
	{
		if( !filesystem->FindIsDirectory( findHandle ) )
		{
			// Strip off the 'sound/' part of the name.
			char fn[ 512 ];
			Q_snprintf( fn, sizeof( fn ), "%s/%s", subdir, pFileName );

			FileNameHandle_t fh;
			fh = filesystem->FindOrAddFileName( fn );
			files.AddToTail( fh );
		}
		pFileName = filesystem->FindNext( findHandle );
	}

	filesystem->FindClose( findHandle );
}

void AnimationBrowser::PurgeCustom()
{
	for ( int i = 0; i < m_CustomAnimationTabs.Count(); ++i )
	{
		if ( m_CustomAnimationTabs[ i ]->m_bDirty )
		{
			m_CustomAnimationTabs[ i ]->SaveToFile();
		}
		delete m_CustomAnimationTabs[ i ];
	}
	m_CustomAnimationTabs.Purge();
}

void AnimationBrowser::BuildCustomFromFiles( CUtlVector< FileNameHandle_t >& files )
{
	PurgeCustom();

	for ( int i = 0; i < files.Count(); ++i )
	{
		char fn[ 512 ];
		if ( !filesystem->String( files[ i ], fn, sizeof( fn ) ) )
			continue;

		Q_FixSlashes( fn );
		Q_strlower( fn );

		char basename[ 128 ];
		Q_FileBase( fn, basename, sizeof( basename ) );

		CCustomAnim *anim = new CCustomAnim( files[ i ] );
		anim->m_ShortName = basename;
		anim->LoadFromFile();

		m_CustomAnimationTabs.AddToTail( anim );
	}

	UpdateCustomTabs();
}

void AnimationBrowser::RenameCustomFile( int index )
{
	if ( index < 0 || index >= m_CustomAnimationTabs.Count() )
		return;

	CCustomAnim *anim = m_CustomAnimationTabs[ index ];
 	CInputParams params;
	memset( &params, 0, sizeof( params ) );
	Q_snprintf( params.m_szDialogTitle, sizeof( params.m_szDialogTitle ), "Custom Animation Group" );
	Q_strcpy( params.m_szPrompt, "Group Name:" );
	Q_strcpy( params.m_szInputText, anim->m_ShortName.String() );

	if ( !InputProperties( &params ) )
		return;

	if ( !params.m_szInputText[ 0 ] )
		return;

	// No change
	if ( !Q_stricmp( anim->m_ShortName.String(), params.m_szInputText ) )
		return;

	char fn[ 512 ];
	if ( !filesystem->String( anim->m_Handle, fn, sizeof( fn ) ) )
	{
		Assert( 0 );
		return;
	}

	StudioModel *model = models->GetActiveStudioModel();
	if ( !model )
	{
		return;
	}

	CStudioHdr *hdr = model->GetStudioHdr();
	if ( !hdr )
	{
		return;
	}

	// Delete the old file
	filesystem->RemoveFile( fn, "MOD" );

	anim->m_ShortName = params.m_szInputText;
	
	char basename[ 128 ];
	Q_StripExtension( hdr->pszName(), basename, sizeof( basename ) );
	Q_snprintf( fn, sizeof( fn ), "expressions/%s/animation/%s.txt", basename, params.m_szInputText );
	Q_FixSlashes( fn );
	Q_strlower( fn );
	CreatePath( fn );

	anim->m_Handle = filesystem->FindOrAddFileName( fn );

	anim->m_bDirty = true;
	UpdateCustomTabs();
}

void AnimationBrowser::AddCustomFile( const FileNameHandle_t& handle )
{
	char fn[ 512 ];
	if ( !filesystem->String( handle, fn, sizeof( fn ) ) )
		return;

	Q_FixSlashes( fn );
	Q_strlower( fn );
	char basename[ 128 ];
	Q_FileBase( fn, basename, sizeof( basename ) );

	CCustomAnim *anim = new CCustomAnim( handle );
	anim->m_ShortName = basename;
	anim->LoadFromFile();
	anim->m_bDirty = true;

	if ( m_nCurCell != -1 )
	{
		StudioModel *model = models->GetActiveStudioModel();
		if ( model )
		{
			CStudioHdr *hdr = model->GetStudioHdr();
			if ( hdr )
			{
				mstudioseqdesc_t &seqdesc = hdr->pSeqdesc( m_nCurCell );
				CUtlSymbol sym;
				sym = seqdesc.pszLabel();
				anim->m_Animations.AddToTail( sym );
			}
		}
	}

	m_CustomAnimationTabs.AddToTail( anim );

	UpdateCustomTabs();
}

void AnimationBrowser::DeleteCustomFile( int index )
{
	if ( index < 0 || index >= m_CustomAnimationTabs.Count() )
		return;

	CCustomAnim *anim = m_CustomAnimationTabs[ index ];

	char fn[ 512 ];
	if ( !filesystem->String( anim->m_Handle, fn, sizeof( fn ) ) )
		return;

	m_CustomAnimationTabs.Remove( index );
	filesystem->RemoveFile( fn );
	delete anim;

	UpdateCustomTabs();
}

void AnimationBrowser::UpdateCustomTabs()
{
	m_pFilterTab->UpdateCustomTabs( m_CustomAnimationTabs );
}

void AnimationBrowser::OnModelChanged()
{
	CUtlVector< FileNameHandle_t > files;

	StudioModel *model = models->GetActiveStudioModel();
	if ( model )
	{
		CStudioHdr *hdr = model->GetStudioHdr();
		if ( hdr )
		{
			char subdir[ 512 ];
			char basename[ 512 ];
			Q_StripExtension( hdr->pszName(), basename, sizeof( basename ) );
			Q_snprintf( subdir, sizeof( subdir ), "expressions/%s/animation", basename );
			Q_FixSlashes( subdir );
			Q_strlower( subdir );
			FindCustomFiles( subdir, files );
		}
	}

	BuildCustomFromFiles( files );

	RestoreThumbnailSize();
	
	// Just reapply filter
	OnFilter();
}

 void AnimationBrowser::OnAddCustomAnimationFilter()
 {
	StudioModel *model = models->GetActiveStudioModel();
	if ( !model )
	{
		return;
	}

	CStudioHdr *hdr = model->GetStudioHdr();
	if ( !hdr )
	{
		return;
	}

 	CInputParams params;
	memset( &params, 0, sizeof( params ) );
	Q_snprintf( params.m_szDialogTitle, sizeof( params.m_szDialogTitle ), "Custom Animation Group" );
	Q_strcpy( params.m_szPrompt, "Group Name:" );
	Q_strcpy( params.m_szInputText, "" );

	if ( !InputProperties( &params ) )
		return;

	if ( !params.m_szInputText[ 0 ] )
		return;

	if ( FindCustomFile( params.m_szInputText ) != -1 )
	{
		Warning( "Can't add duplicate tab '%s'\n", params.m_szInputText );
		return;
	}

	// Create it
	char fn[ 512 ];
	char basename[ 512 ];
	Q_StripExtension( hdr->pszName(), basename, sizeof( basename ) );
	Q_snprintf( fn, sizeof( fn ), "expressions/%s/animation/%s.txt", basename, params.m_szInputText );
	Q_FixSlashes( fn );
	Q_strlower( fn );
	CreatePath( fn );

	FileNameHandle_t fh = filesystem->FindOrAddFileName( fn );
	AddCustomFile( fh );
 }

 int AnimationBrowser::FindCustomFile( char const *shortName )
 {
	 CUtlSymbol search;
	 search = shortName;

	 for ( int i = 0; i < m_CustomAnimationTabs.Count(); ++i )
	 {
		 CCustomAnim *anim = m_CustomAnimationTabs[ i ];
		 if ( anim->m_ShortName == search )
			 return i;
	 }
	 return -1;
 }

void AnimationBrowser::AddAnimationToCustomFile( int index, char const *animationName )
{
	if ( index < 0 || index >= m_CustomAnimationTabs.Count() )
		return;

	CCustomAnim *anim = m_CustomAnimationTabs[ index ];

	CUtlSymbol search;
	search = animationName;
	
	if ( anim->m_Animations.Find( search ) == anim->m_Animations.InvalidIndex() )
	{
		anim->m_Animations.AddToTail( search );
		anim->m_bDirty = true;
	}

	OnFilter();
}

void AnimationBrowser::RemoveAnimationFromCustomFile( int index, char const *animationName )
{
	if ( index < 0 || index >= m_CustomAnimationTabs.Count() )
		return;

	CCustomAnim *anim = m_CustomAnimationTabs[ index ];

	CUtlSymbol search;
	search = animationName;
	
	int slot = anim->m_Animations.Find( search );
	if ( slot != anim->m_Animations.InvalidIndex() )
	{
		anim->m_Animations.Remove( slot );
		anim->m_bDirty = true;
		OnFilter();
	}
}

void AnimationBrowser::RemoveAllAnimationsFromCustomFile( int index )
{
	if ( index < 0 || index >= m_CustomAnimationTabs.Count() )
		return;

	CCustomAnim *anim = m_CustomAnimationTabs[ index ];
	anim->m_Animations.Purge();
	anim->m_bDirty = true;

	OnFilter();
}