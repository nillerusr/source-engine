//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include <stdio.h>
#include <mxtk/mxPopupMenu.h>
#include "hlfaceposer.h"
#include "choreochannelwidget.h"
#include "choreoeventwidget.h"
#include "choreoactorwidget.h"
#include "choreochannel.h"
#include "choreowidgetdrawhelper.h"
#include "choreoview.h"
#include "choreoevent.h"
#include "choreoviewcolors.h"
#include "utlrbtree.h"
#include "utllinkedlist.h"
#include "iclosecaptionmanager.h"
#include "PhonemeEditor.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "filesystem.h"

#define AUDIO_HEIGHT 18
#define STREAM_FONT			"Tahoma"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
CChoreoChannelWidget::CChoreoChannelWidget( CChoreoActorWidget *parent )
: CChoreoWidget( parent )
{
	m_pChannel = NULL;
	m_pParent = parent;
	m_bHasAudio = false;
	m_nBaseHeight = 0;
	m_nSelectorEventIndex = -1;
}
	
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CChoreoChannelWidget::~CChoreoChannelWidget( void )
{
	for ( int i = 0 ; i < m_Events.Size(); i++ )
	{
		CChoreoEventWidget *e = m_Events[ i ];
		delete e;
	}
	m_Events.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Create child windows
//-----------------------------------------------------------------------------
void CChoreoChannelWidget::Create( void )
{
	Assert( m_pChannel );

	// Create objects for children
	for ( int i = 0; i < m_pChannel->GetNumEvents(); i++ )
	{
		CChoreoEvent *e = m_pChannel->GetEvent( i );
		Assert( e );
		if ( !e )
		{
			continue;
		}

		CChoreoEventWidget *eventWidget = new CChoreoEventWidget( this );
		eventWidget->SetEvent( e );
		eventWidget->Create();
		
		AddEvent( eventWidget );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
// Output : float
//-----------------------------------------------------------------------------
float CChoreoChannelWidget::GetTimeForMousePosition( int mx )
{
	int dx = mx - m_pView->GetLabelWidth();
	float windowfrac = ( float ) dx / ( float ) ( w() -  m_pView->GetLabelWidth() );
	float time = m_pView->GetStartTime() + windowfrac * ( m_pView->GetEndTime() - m_pView->GetStartTime() );
	return time;
}

static bool EventStartTimeLessFunc( CChoreoEventWidget * const &p1, CChoreoEventWidget * const  &p2 )
{
	CChoreoEventWidget *w1;
	CChoreoEventWidget *w2;

	w1 = const_cast< CChoreoEventWidget * >( p1 );
	w2 = const_cast< CChoreoEventWidget * >( p2 );

	CChoreoEvent *e1;
	CChoreoEvent *e2;

	e1 = w1->GetEvent();
	e2 = w2->GetEvent();

	return e1->GetStartTime() < e2->GetStartTime();
}

void CChoreoChannelWidget::LayoutEventInRow( CChoreoEventWidget *event, int row, RECT& rc )
{
	int itemHeight = BaseClass::GetItemHeight();

	RECT rcEvent;
	rcEvent.left = m_pView->GetPixelForTimeValue( event->GetEvent()->GetStartTime() );
	if ( event->GetEvent()->HasEndTime() )
	{
		rcEvent.right = m_pView->GetPixelForTimeValue( event->GetEvent()->GetEndTime() );
	}
	else
	{
		rcEvent.right = rcEvent.left + 8;
	}
	rcEvent.top = rc.top + ( row ) * itemHeight + 2;
	rcEvent.bottom = rc.top + ( row + 1 ) * itemHeight - 2;
	event->Layout( rcEvent );
}

static bool EventCollidesWithRows( CUtlLinkedList< CChoreoEventWidget *, int >& list, CChoreoEventWidget *event )
{
	float st = event->GetEvent()->GetStartTime();
	float ed = event->GetEvent()->HasEndTime() ? event->GetEvent()->GetEndTime() : event->GetEvent()->GetStartTime();

	for ( int i = list.Head(); i != list.InvalidIndex(); i = list.Next( i ) )
	{
		CChoreoEvent *test = list[ i ]->GetEvent();

		float teststart = test->GetStartTime();
		float testend = test->HasEndTime() ? test->GetEndTime() : test->GetStartTime();

		// See if spans overlap
		if ( teststart >= ed )
			continue;
		if ( testend <= st )
			continue;

		return true;
	}

	return false;
}

int CChoreoChannelWidget::GetVerticalStackingCount( bool layout, RECT *rc )
{
	CUtlRBTree< CChoreoEventWidget * >  sorted( 0, 0, EventStartTimeLessFunc );

	CUtlVector< CUtlLinkedList< CChoreoEventWidget *, int > >	rows;

	int i;
	// Sort items
	int c = m_Events.Size();
	for ( i = 0; i < c; i++ )
	{
		sorted.Insert( m_Events[ i ] );
	}

	for ( i = sorted.FirstInorder(); i != sorted.InvalidIndex(); i = sorted.NextInorder( i ) )
	{
		CChoreoEventWidget *event = sorted[ i ];
		Assert( event );
		if ( !rows.Count() )
		{
			rows.AddToTail();

			CUtlLinkedList< CChoreoEventWidget *, int >& list = rows[ 0 ];
			list.AddToHead( event );

			if ( layout )
			{
				LayoutEventInRow( event, 0, *rc );
			}
			continue;
		}

		// Does it come totally after what's in rows[0]?
		int rowCount = rows.Count();
		bool addrow = true;

		for ( int j = 0; j < rowCount; j++ )
		{
			CUtlLinkedList< CChoreoEventWidget *, int >& list = rows[ j ];

			if ( !EventCollidesWithRows( list, event ) )
			{
				// Update row event list
				list.AddToHead( event );
				addrow = false;
				if ( layout )
				{
					LayoutEventInRow( event, j, *rc );
				}
				break;
			}
		}

		if ( addrow )
		{
			// Add a new row
			int idx = rows.AddToTail();
			CUtlLinkedList< CChoreoEventWidget *, int >& list = rows[ idx ];
			list.AddToHead( event );
			if ( layout )
			{
				LayoutEventInRow( event, rows.Count() - 1, *rc );
			}
		}
	}

	return max( 1, rows.Count() );
}

int	CChoreoChannelWidget::GetItemHeight( void )
{
	int itemHeight = BaseClass::GetItemHeight();
	int stackCount = GetVerticalStackingCount( false, NULL );

	CheckHasAudio();

	int h = stackCount * itemHeight;
	
	// Remember the base height
	m_nBaseHeight = h;

	if ( m_bHasAudio && m_pView->GetShowCloseCaptionData() )
	{
		h += 2 * AUDIO_HEIGHT;
	}

	return h;
}

bool CChoreoChannelWidget::CheckHasAudio()
{
	m_bHasAudio = false;
	// Create objects for children
	for ( int i = 0; i < m_Events.Size(); i++ )
	{
		CChoreoEventWidget *event = m_Events[ i ];
		if ( event->GetEvent()->GetType() == CChoreoEvent::SPEAK )
		{
			m_bHasAudio = true;
			break;
		}
	}
	return m_bHasAudio;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rc - 
//-----------------------------------------------------------------------------
void CChoreoChannelWidget::Layout( RECT& rc )
{
	setBounds( rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top );

	GetVerticalStackingCount( true, &rc );
	CheckHasAudio();

	/*
	// Create objects for children
	for ( int i = 0; i < m_Events.Size(); i++ )
	{
		CChoreoEventWidget *event = m_Events[ i ];
		Assert( event );
		if ( !event )
		{
			continue;
		}

		RECT rcEvent;
		rcEvent.left = m_pView->GetPixelForTimeValue( event->GetEvent()->GetStartTime() );
		if ( event->GetEvent()->HasEndTime() )
		{
			rcEvent.right = m_pView->GetPixelForTimeValue( event->GetEvent()->GetEndTime() );
		}
		else
		{
			rcEvent.right = rcEvent.left + 8;
		}
		rcEvent.top = rc.top + 2;
		rcEvent.bottom = rc.bottom - 2;
		event->Layout( rcEvent );
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoChannelWidget::redraw( CChoreoWidgetDrawHelper& drawHelper )
{
	if ( !getVisible() )
		return;

	CChoreoChannel *channel = GetChannel();
	if ( !channel )
		return;

	RECT rcText;
	rcText = getBounds();

	rcText.right = m_pView->GetLabelWidth();

	if ( !channel->GetActive() )
	{
		RECT rcBg = rcText;
		InflateRect( &rcBg, -5, -5 );

		drawHelper.DrawFilledRect( RGB( 210, 210, 210 ), rcBg );
	}

	RECT rcName = rcText;

	rcName.left += 20;
	char n[ 512 ];
	V_strcpy_safe( n, channel->GetName() );

	drawHelper.DrawColoredText( "Arial", 
		m_pView->GetFontSize() + 2, 
		FW_HEAVY, 
		channel->GetActive() ? COLOR_CHOREO_CHANNELNAME : COLOR_CHOREO_ACTORNAME_INACTIVE,
		rcName, n );

	if ( !channel->GetActive() )
	{
		strcpy( n, "(inactive)" );

		RECT rcInactive = rcName;
		int len = drawHelper.CalcTextWidth( "Arial", m_pView->GetFontSize(), 500, n );
		rcInactive.left = rcInactive.right - len;
		//rcInactive.top += 3;
		//rcInactive.bottom = rcInactive.top + m_pView->GetFontSize() - 2;

		drawHelper.DrawColoredText( "Arial", m_pView->GetFontSize() - 2, 500,
			COLOR_CHOREO_ACTORNAME_INACTIVE, rcInactive, n );
	}

	rcName.left -= 20;

	RECT rcEventArea = getBounds();
	rcEventArea.left = m_pView->GetLabelWidth() + 1;
	rcEventArea.top -= 20;

	drawHelper.StartClipping( rcEventArea );

	if ( m_bHasAudio )
	{
		RenderCloseCaptionExpandCollapseRect( drawHelper, rcEventArea );
		if ( m_pView->GetShowCloseCaptionData() )
		{
			RenderCloseCaptionExpandCollapseRect( drawHelper, rcEventArea );
			RenderCloseCaptionInfo( drawHelper, rcEventArea );
			RenderCloseCaptions( drawHelper, rcEventArea );
			RenderCloseCaptionSelectors( drawHelper, rcEventArea );
		}
	}

	for ( int j =  GetNumEvents()-1; j >= 0; j-- )
	{
		CChoreoEventWidget *event = GetEvent( j );
		if ( event )
		{
			event->redraw( drawHelper );
		}
	}

	drawHelper.StopClipping();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rcEventArea - 
//-----------------------------------------------------------------------------
void CChoreoChannelWidget::RenderCloseCaptionInfo( CChoreoWidgetDrawHelper& drawHelper, RECT& rcEventArea )
{
	wchar_t wstr[ 1024 ];
	COLORREF barColor = RGB( 100, 200, 255 );

	{
		RECT rcText = rcEventArea;
		rcText.left += 2;
		rcText.top = rcEventArea.bottom - 15;
		rcText.bottom = rcText.top + 12;
		drawHelper.DrawColoredText( "Arial", m_pView->GetFontSize() - 2, 500,
			COLOR_CHOREO_TEXT, rcText, "token/data:" );
	}

	// Walk the events looking for SPEAK events (esp if marked as MASTER with >= 1 slave)
	for ( int j =  GetNumEvents()-1; j >= 0; j-- )
	{
		CChoreoEventWidget *event = GetEvent( j );
		CChoreoEvent *e = event->GetEvent();
		
		if ( e->GetType() != CChoreoEvent::SPEAK )
			continue;

		if ( e->GetCloseCaptionType() == CChoreoEvent::CC_SLAVE )
			continue;

		char const *label = "";

		bool showState = false;
		bool stateValid = false;

		if ( e->GetCloseCaptionType() == CChoreoEvent::CC_MASTER )
		{
			showState = true;
			if ( e->GetNumSlaves() >= 1 )
			{
				barColor = RGB( 100, 200, 255 );
				label = e->GetCloseCaptionToken();
			}
			else
			{
				barColor = RGB( 100, 150, 100 );
				label = e->GetParameters();
			}

			char cctoken[ CChoreoEvent::MAX_CCTOKEN_STRING ];
			if ( e->GetPlaybackCloseCaptionToken( cctoken, sizeof( cctoken ) ) )
			{
				stateValid = closecaptionmanager->LookupUnicodeText( GetCloseCaptionLanguageId(), cctoken, wstr, sizeof( wstr ) / sizeof( wchar_t ) );
			}
		}
		else
		{
			barColor = RGB( 150, 150, 150 );
			label = "-disabled-";
		}

		// Found one!!!
		RECT rcEvent = event->getBounds();

		float bestEndTime = max( e->GetEndTime(), e->GetLastSlaveEndTime() );
		int pixeloffset = (int)( ( bestEndTime - e->GetStartTime() ) * m_pView->GetPixelsPerSecond() + 0.5f );

		rcEvent.right = rcEvent.left + pixeloffset;
		rcEvent.top = rcEventArea.bottom - 3;
		rcEvent.bottom = rcEventArea.bottom;

		
		drawHelper.DrawFilledRect( barColor, rcEvent );

		RECT rcTriangle;
		rcTriangle = rcEvent;
		rcTriangle.right = rcTriangle.left + 3;
		rcTriangle.left -= 3;

		OffsetRect( &rcTriangle, 0, -6 );
		rcTriangle.bottom += 6;
		drawHelper.DrawTriangleMarker( rcTriangle, barColor, true );

		rcTriangle.left = rcEvent.right - 3;
		rcTriangle.right = rcEvent.right + 3;

		drawHelper.DrawTriangleMarker( rcTriangle, barColor, true );

		RECT rcText = rcEvent;
		rcText.bottom = rcText.top + 12;
		OffsetRect( &rcText, 5, -12 );

		if ( showState )
		{
			int stateMarkWidth = 12;
			RECT rcState = rcText;
			rcState.right = rcState.left + stateMarkWidth;
			rcText.left += stateMarkWidth;

			COLORREF symColor = stateValid ? RGB( 40, 100, 40 ) : RGB( 200, 40, 40 );

			drawHelper.DrawColoredTextCharset( 
				"Marlett", 
				m_pView->GetFontSize() - 2,
				500,
				SYMBOL_CHARSET,
				symColor,
				rcState,
				stateValid ? "a" : "r" );

		}

		if ( e->IsSuppressingCaptionAttenuation() )
		{
			drawHelper.DrawColoredText( "Arial", m_pView->GetFontSize() - 2, 500,
				RGB( 80, 80, 100 ), rcText, "%s [no attenuate]", label );

		}
		else
		{
			drawHelper.DrawColoredText( "Arial", m_pView->GetFontSize() - 2, 500,
				RGB( 80, 80, 100 ), rcText, label );
		}
			


	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rcEventArea - 
//-----------------------------------------------------------------------------
void CChoreoChannelWidget::RenderCloseCaptions( CChoreoWidgetDrawHelper& drawHelper, RECT& rcEventArea )
{
	{
		RECT rcText = rcEventArea;
		rcText.top = rcEventArea.top + m_nBaseHeight + AUDIO_HEIGHT + 5;
		rcText.bottom = rcText.top + 12;
		rcText.left += 12;
		drawHelper.DrawColoredText( "Arial", m_pView->GetFontSize() - 2, 500,
			COLOR_CHOREO_TEXT, rcText, "%s", CSentence::NameForLanguage( GetCloseCaptionLanguageId() ) );

		// Previous
		GetCloseCaptionLanguageRect( rcText, true );
		drawHelper.DrawColoredTextCharset( 
			"Marlett", 
			m_pView->GetFontSize(),
			500,
			SYMBOL_CHARSET,
			COLOR_CHOREO_TEXT,
			rcText,
			"3" );

		// Next
		GetCloseCaptionLanguageRect( rcText, false );
		drawHelper.DrawColoredTextCharset( 
			"Marlett", 
			m_pView->GetFontSize(),
			500,
			SYMBOL_CHARSET,
			COLOR_CHOREO_TEXT,
			rcText,
			"4" );
	}

	// Walk the events looking for SPEAK events (esp if marked as MASTER with >= 1 slave)
	for ( int j =  GetNumEvents()-1; j >= 0; j-- )
	{
		CChoreoEventWidget *event = GetEvent( j );
		CChoreoEvent *e = event->GetEvent();
		
		if ( e->GetType() != CChoreoEvent::SPEAK )
			continue;

		if ( e->GetCloseCaptionType() == CChoreoEvent::CC_SLAVE ||
			e->GetCloseCaptionType() == CChoreoEvent::CC_DISABLED )
			continue;

		char cctoken[ CChoreoEvent::MAX_CCTOKEN_STRING ];

		bool valid = e->GetPlaybackCloseCaptionToken( cctoken, sizeof( cctoken ) );
		if ( !valid )
			continue;

		wchar_t wstr[ 1024 ];

		valid = closecaptionmanager->LookupStrippedUnicodeText( GetCloseCaptionLanguageId(), cctoken, wstr, sizeof( wstr ) / sizeof( wchar_t ) );

		// Found one!!!
		RECT rcEvent = event->getBounds();

		float bestEndTime = max( e->GetEndTime(), e->GetLastSlaveEndTime() );
		int pixeloffset = (int)( ( bestEndTime - e->GetStartTime() ) * m_pView->GetPixelsPerSecond() + 0.5f );

		rcEvent.right = rcEvent.left + pixeloffset;
		rcEvent.top = rcEventArea.top + m_nBaseHeight + AUDIO_HEIGHT + 5;
		rcEvent.bottom = rcEvent.top + 12;
		rcEvent.left += 5;

		COLORREF textColor = valid ? RGB( 80, 80, 100 ) : RGB( 225, 40, 40 );

		drawHelper.DrawColoredTextW( STREAM_FONT, m_pView->GetFontSize() - 2, 500,
				textColor, rcEvent, wstr );

	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CChoreoChannel
//-----------------------------------------------------------------------------
CChoreoChannel *CChoreoChannelWidget::GetChannel( void )
{
	return m_pChannel;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *channel - 
//-----------------------------------------------------------------------------
void CChoreoChannelWidget::SetChannel( CChoreoChannel *channel )
{
	m_pChannel = channel;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
//-----------------------------------------------------------------------------
void CChoreoChannelWidget::AddEvent( CChoreoEventWidget *event )
{
	m_Events.AddToTail( event );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
//-----------------------------------------------------------------------------
void CChoreoChannelWidget::RemoveEvent( CChoreoEventWidget *event )
{
	m_Events.FindAndRemove( event );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : num - 
// Output : CChoreoEventWidget
//-----------------------------------------------------------------------------
CChoreoEventWidget *CChoreoChannelWidget::GetEvent( int num )
{
	return m_Events[ num ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoChannelWidget::GetNumEvents( void )
{
	return m_Events.Size();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
//-----------------------------------------------------------------------------
void CChoreoChannelWidget::MoveEventToTail( CChoreoEventWidget *event )
{
	for ( int i = 0; i < GetNumEvents(); i++ )
	{
		CChoreoEventWidget *ew = GetEvent( i );
		if ( ew == event )
		{
			m_Events.Remove( i );
			m_Events.AddToTail( ew );
			break;
		}
	}
}

int CChoreoChannelWidget::GetChannelItemUnderMouse( int mx, int my )
{
	m_nSelectorEventIndex = -1;

	if ( !m_bHasAudio )
		return CLOSECAPTION_NONE;

	RECT rcCCArea;
	GetCloseCaptionExpandCollapseRect( rcCCArea );

	POINT pt;
	pt.x = mx;
	pt.y = my;

	if ( PtInRect( &rcCCArea, pt ) )
	{
		return CLOSECAPTION_EXPANDCOLLAPSE;
	}

	// previous
	GetCloseCaptionLanguageRect( rcCCArea, true );
	if ( PtInRect( &rcCCArea, pt ) )
	{
		return CLOSECAPTION_PREVLANGUAGE;
	}

	// next language
	GetCloseCaptionLanguageRect( rcCCArea, false );
	if ( PtInRect( &rcCCArea, pt ) )
	{
		return CLOSECAPTION_NEXTLANGUAGE;
	}

	CUtlVector< CloseCaptionInfo > vecSelectors;
	GetCloseCaptions( vecSelectors );
	int c = vecSelectors.Count();
	if ( vecSelectors.Count() > 0 )
	{
		int i;
		for ( i = 0; i < c; ++i )
		{
			CloseCaptionInfo& check = vecSelectors[ i ];
			if ( check.isSelector && PtInRect( &check.rcSelector, pt ) )
			{
				m_nSelectorEventIndex = check.eventindex;
				return CLOSECAPTION_SELECTOR;
			}
		}

		for ( i = 0; i < c; ++i )
		{
			CloseCaptionInfo& check = vecSelectors[ i ];
			if ( PtInRect( &check.rcCaption, pt ) )
			{
				m_nSelectorEventIndex = check.eventindex;
				return CLOSECAPTION_CAPTION;
			}
		}
	}

	return CLOSECAPTION_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoChannelWidget::HandleSelectorClicked()
{
	if ( m_nSelectorEventIndex < 0 )
		return;

	if ( m_nSelectorEventIndex >= m_Events.Count() )
		return;

	CChoreoEvent *event = GetEvent( m_nSelectorEventIndex )->GetEvent();
	SetUsingCombinedFieldByTokenName( event->GetCloseCaptionToken(), !event->IsUsingCombinedFile() );
}

void CChoreoChannelWidget::SetUsingCombinedFieldByTokenName( char const *token, bool usingcombinedfile )
{
	int c = GetNumEvents();
	for ( int i = 0; i < c; ++i )
	{
		CChoreoEvent *e = GetEvent( i )->GetEvent();
		if ( !Q_stricmp( e->GetCloseCaptionToken(), token ) )
		{
			e->SetUsingCombinedFile( usingcombinedfile );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CChoreoEvent
//-----------------------------------------------------------------------------
CChoreoEvent *CChoreoChannelWidget::GetCaptionClickedEvent()
{
	if ( m_nSelectorEventIndex < 0 )
		return NULL;

	if ( m_nSelectorEventIndex >= m_Events.Count() )
		return NULL;

	CChoreoEvent *event = GetEvent( m_nSelectorEventIndex )->GetEvent();
	return event;
}

void CChoreoChannelWidget::GetCloseCaptionExpandCollapseRect( RECT& rc )
{
	Assert( m_bHasAudio );

	rc = getBounds();
	rc.left = m_pView->GetLabelWidth() + 2;
	rc.right = rc.left + 12;

	rc.top += 2;
	rc.bottom = rc.top + 12;
}

void CChoreoChannelWidget::GetCloseCaptionLanguageRect( RECT& rc, bool previous )
{
	Assert( m_bHasAudio );

	RECT rcEventArea = getBounds();
	rcEventArea.left = m_pView->GetLabelWidth() + 1;
	rcEventArea.top -= 20;

	rc = rcEventArea;
	rc.top = rcEventArea.top + m_nBaseHeight + AUDIO_HEIGHT + 5;
	rc.bottom = rc.top + 12;
	rc.left += 2;
	rc.right = rc.left + 12;

	if ( !previous )
	{
		int textlen = CChoreoWidgetDrawHelper::CalcTextWidth
		( 
			"Arial", 
			m_pView->GetFontSize()-2, 
			500,
			CSentence::NameForLanguage( GetCloseCaptionLanguageId() ) 
		);

		OffsetRect( &rc, textlen + 10, 0 );
	}
}

void CChoreoChannelWidget::RenderCloseCaptionSelectors( CChoreoWidgetDrawHelper& drawHelper, RECT& rcEventArea )
{
	CUtlVector< CloseCaptionInfo > vecSelectors;
	GetCloseCaptions( vecSelectors );
	int c = vecSelectors.Count();
	if ( vecSelectors.Count() > 0 )
	{
		for ( int i = 0; i < c; ++i )
		{
			CloseCaptionInfo& check = vecSelectors[ i ];

			if ( !check.isSelector )
				continue;

			CChoreoEventWidget *e = GetEvent( check.eventindex );
			if ( !e )
				continue;

			CChoreoEvent *event = e->GetEvent();

			bool upArrow = !event->IsUsingCombinedFile();
			COLORREF clr = RGB( 63, 63, 63 ); // upArrow ? RGB( 255, 0, 0 ) : RGB( 0, 0, 255 );

			RECT rc = check.rcSelector;

			POINT endpt;
			endpt.x = rc.right - 2;

			if ( upArrow )
			{
				endpt.y = rc.top - 9;	
			}
			else
			{
				endpt.y = rc.bottom + 9;
			}

			POINT startpt;
			startpt.x = ( rc.left + rc.right ) * 0.5;
			startpt.y = ( rc.top + rc.bottom ) * 0.5;
		
			drawHelper.DrawCircle( 
				clr,
				endpt.x, 
				endpt.y,
				3	, true );

			drawHelper.DrawColoredLine( clr, PS_SOLID, 1, startpt.x, startpt.y, endpt.x, endpt.y );
			

			drawHelper.DrawCircle( 
				clr,
				startpt.x, 
				startpt.y,
				7, true );
		}
	}
}

void CChoreoChannelWidget::GetCloseCaptions( CUtlVector< CloseCaptionInfo >& selectors )
{
	selectors.RemoveAll();

	// Walk the events looking for SPEAK events (esp if marked as MASTER with >= 1 slave)
	for ( int j =  GetNumEvents()-1; j >= 0; j-- )
	{
		CChoreoEventWidget *event = GetEvent( j );
		CChoreoEvent *e = event->GetEvent();
		
		if ( e->GetType() != CChoreoEvent::SPEAK )
			continue;

		CChoreoEvent::CLOSECAPTION capType = e->GetCloseCaptionType();

		if ( capType == CChoreoEvent::CC_SLAVE )
			continue;

		bool isSelector = ( e->GetNumSlaves() >= 1 ) && capType == CChoreoEvent::CC_MASTER;

		// Found one!!!
		RECT rcEvent = event->getBounds();
		RECT rcCaption = rcEvent;

		rcEvent.right = rcEvent.left + 16;
		OffsetRect( &rcEvent, -16, rcEvent.bottom - rcEvent.top );
		rcEvent.bottom = rcEvent.top + 16;

		CloseCaptionInfo ccs;
		ccs.rcSelector = rcEvent;
		ccs.isSelector = isSelector;

		rcCaption.top += rcEvent.bottom - rcEvent.top;

		RECT rcEventArea = getBounds();

		rcCaption.bottom = rcEventArea.bottom;

		// Now compute true right edge
		float bestEndTime = max( e->GetEndTime(), e->GetLastSlaveEndTime() );
		int pixeloffset = (int)( ( bestEndTime - e->GetStartTime() ) * m_pView->GetPixelsPerSecond() + 0.5f );
		rcCaption.right = rcCaption.left + pixeloffset;

		ccs.rcCaption = rcCaption;

		ccs.eventindex = j;
		selectors.AddToTail( ccs );
	}
}


void CChoreoChannelWidget::RenderCloseCaptionExpandCollapseRect( CChoreoWidgetDrawHelper& drawHelper, RECT& rcEventArea )
{
	if ( !m_bHasAudio )
		return;

	RECT rcCCArea;
	GetCloseCaptionExpandCollapseRect( rcCCArea );

	COLORREF symColor = RGB( 100, 100, 100 );

	drawHelper.DrawColoredTextCharset( 
		"Marlett", 
		m_pView->GetFontSize(),
		900,
		SYMBOL_CHARSET,
		symColor,
		rcCCArea,
		m_pView->GetShowCloseCaptionData()  ? "6" : "4" );
}

void CChoreoChannelWidget::GetMasterAndSlaves( CChoreoEvent *master, CUtlVector< CChoreoEvent * >& fulllist )
{
	// Old
	int c = GetNumEvents();
	int i;
	for ( i = 0; i < c; ++i )
	{
		CChoreoEvent *e = GetEvent( i )->GetEvent();
		if ( !Q_stricmp( master->GetCloseCaptionToken(), e->GetCloseCaptionToken() ) )
		{
			if ( fulllist.Find( e ) == fulllist.InvalidIndex() )
			{
				fulllist.AddToTail( e );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : drawHelper - 
//			rcBounds - 
//-----------------------------------------------------------------------------
void CChoreoChannelWidget::redrawStatus( CChoreoWidgetDrawHelper& drawHelper, RECT& rcClient, int areaUnderMouse )
{
	if ( !getVisible() )
		return;

	if ( areaUnderMouse != CLOSECAPTION_CAPTION )
		return;

	CChoreoEvent *e = GetCaptionClickedEvent();
	if ( !e )
		return;

	int deflateborder = 1;
	int fontsize = 9;

	// Now draw the label
	RECT rcEventLabel;
	rcEventLabel = rcClient;

	InflateRect( &rcEventLabel, 0, -deflateborder );

	// rcEventLabel.top += 2;
	rcEventLabel.left += 2;
	//rcEventLabel.top = rcEventLabel.bottom - 2 * ( fontsize + 2 ) - 1;
	//rcEventLabel.bottom = rcEventLabel.top + fontsize + 2;

	/*
	HDC dc = drawHelper.GrabDC();

	int leftAdd = 16;

	if ( CChoreoEventWidget::GetImage( event->GetType() ) )
	{
		mxbitmapdata_t *image = CChoreoEventWidget::GetImage( event->GetType() );
		if ( image )
		{
			RECT rcFixed = rcEventLabel;
			drawHelper.OffsetSubRect( rcFixed );
			DrawBitmapToDC( dc, rcFixed.left, rcFixed.top, leftAdd, leftAdd,
				*image );	
		}
	}

	// Draw Type Name:
	//rcEventLabel.top -= 4;

	rcEventLabel.left = rcClient.left + 32;
	rcEventLabel.bottom = rcEventLabel.top + fontsize + 2;
	// OffsetRect( &rcEventLabel, 0, 2 );

	int len = drawHelper.CalcTextWidth( "Arial", fontsize, FW_NORMAL, "%s event \"%s\"", 
		event->NameForType( event->GetType() ), event->GetName() );
	drawHelper.DrawColoredText( "Arial", fontsize, FW_NORMAL, COLOR_INFO_TEXT, rcEventLabel, "%s event \"%s\"", 
		event->NameForType( event->GetType() ), event->GetName() );

	OffsetRect( &rcEventLabel, 0, fontsize + 2 );

	drawHelper.DrawColoredText( "Arial", fontsize, FW_NORMAL, COLOR_INFO_TEXT, 
		rcEventLabel, "parameters \"%s\"", GetLabelText() );
	*/

	char const *label = "";

	bool showState = false;
	bool stateValid = false;

	wchar_t wstr[ 1024 ];
	COLORREF labelColor = COLOR_INFO_TEXT;

	if ( e->GetCloseCaptionType() == CChoreoEvent::CC_MASTER )
	{
		showState = true;
		if ( e->GetNumSlaves() >= 1 )
		{
			label = e->GetCloseCaptionToken();
		}
		else
		{
			label = e->GetParameters();
		}
	}
	else if ( e->GetCloseCaptionType() == CChoreoEvent::CC_SLAVE )
	{
		showState = true;
		label = e->GetCloseCaptionToken();
	}
	else
	{
		label = "-disabled-";
	}

	char cctoken[ CChoreoEvent::MAX_CCTOKEN_STRING ];
	if ( showState && e->GetPlaybackCloseCaptionToken( cctoken, sizeof( cctoken ) ) )
	{
		stateValid = closecaptionmanager->LookupUnicodeText( GetCloseCaptionLanguageId(), cctoken, wstr, sizeof( wstr ) / sizeof( wchar_t ) );
	}

	RECT rcText = rcEventLabel;

	rcText.left += 250;
	rcText.bottom = rcText.top + fontsize + 1;

	if ( showState )
	{
		int stateMarkWidth = 12;
		RECT rcState = rcText;
		rcState.right = rcState.left + stateMarkWidth;
		rcText.left += stateMarkWidth;

		COLORREF symColor = stateValid ? RGB( 40, 100, 40 ) : RGB( 200, 40, 40 );

		drawHelper.DrawColoredTextCharset( 
			"Marlett", 
			fontsize+2,
			500,
			SYMBOL_CHARSET,
			symColor,
			rcState,
			stateValid ? "a" : "r" );

	}

	drawHelper.DrawColoredText( "Arial", fontsize, 500,
		labelColor, rcText, "closecaption token:  %s", label );

	RECT saveText = rcText;

	COLORREF statusClr = RGB( 20, 150, 20 );

	if ( e->GetCloseCaptionType() != CChoreoEvent::CC_DISABLED )
	{
		if ( e->GetNumSlaves() >= 1 ||
			e->GetCloseCaptionType() == CChoreoEvent::CC_SLAVE )
		{

			bool combinedValid = m_pView->ValidateCombinedSoundCheckSum( e );

			OffsetRect( &rcText, 0, fontsize + 3 );
		
			char cf[ 256 ];
			Q_strncpy( cf, "(no file)", sizeof( cf ) );

			// Get the filename, including expansion for gender
			e->ComputeCombinedBaseFileName( cf, sizeof( cf ), e->IsCombinedUsingGenderToken() );
			bool gendermacro = Q_stristr( cf, SOUNDGENDER_MACRO ) ? true : false;

			char exist[ 256 ];

			if ( gendermacro )
			{
				bool valid[2];
				char actualfile[ 256 ];
				soundemitter->GenderExpandString( GENDER_MALE, cf, actualfile, sizeof( actualfile ) );
				valid[ 0 ] = filesystem->FileExists( actualfile );
				soundemitter->GenderExpandString( GENDER_FEMALE, cf, actualfile, sizeof( actualfile ) );
				valid[ 1 ] = filesystem->FileExists( actualfile );

				if ( !valid[ 0 ] || !valid[ 1 ] )
				{
					statusClr = RGB( 255, 0, 0 );
				}

				Q_snprintf( exist, sizeof( exist ), "%s", valid ? "exist" : "missing!" );
			}
			else
			{
				bool valid = filesystem->FileExists( cf );
				if ( !valid )
				{
					statusClr = RGB( 255, 0, 0 );
				}

				Q_snprintf( exist, sizeof( exist ), "%s", valid ? "exists" : "missing!" );
			}

			RECT rcPartial = rcText;

			char sz[ 256 ];
			Q_snprintf( sz, sizeof( sz ), 
				"combined file active [ %s ] gender[ %s ] up-to-date[ ",  
				e->IsUsingCombinedFile() ? "yes" : "no",
				e->IsCombinedUsingGenderToken() ? "yes" : "no" );

			int len = drawHelper.CalcTextWidth( "Arial", fontsize, 500, sz );

			drawHelper.DrawColoredText( "Arial", fontsize, 500,
				labelColor, rcPartial, sz  );

			rcPartial.left += len;

			Q_snprintf( sz, sizeof( sz ), 
				"%s", 
				combinedValid ? "yes" : "no" );

			len = drawHelper.CalcTextWidth( "Arial", fontsize, 500, sz );

			drawHelper.DrawColoredText( "Arial", fontsize, 500,
				combinedValid ? RGB( 20, 150, 20 ) : RGB( 255, 0, 0 ), 
				rcPartial, sz  );

			rcPartial.left += len;

			Q_snprintf( sz, sizeof( sz ), 
				" ]:  %s, %s ", 
				cf,
				gendermacro ? "files" : "file" );

			len = drawHelper.CalcTextWidth( "Arial", fontsize, 500, sz );

			drawHelper.DrawColoredText( "Arial", fontsize, 500,
				labelColor, rcPartial, sz  );

			rcPartial.left += len;

			drawHelper.DrawColoredText( "Arial", fontsize, 500,
				statusClr, rcPartial, exist  );

		}

		rcText = saveText;

		OffsetRect( &rcText, 400, 0 );

		// Print out script file for sound
		int soundindex = soundemitter->GetSoundIndex( cctoken );
		if ( soundindex >= 0 )
		{
			char const *scriptfile = soundemitter->GetSourceFileForSound( soundindex );
			Assert( scriptfile );
			if ( scriptfile )
			{
				drawHelper.DrawColoredText( "Arial", fontsize, 500,
					labelColor, rcText, "sound script:  %s", scriptfile  );
			}
		}
		else
		{
			drawHelper.DrawColoredText( "Arial", fontsize, 500,
				RGB( 255, 0, 0 ), rcText, "sound not in game_sounds script files!" );
		}
	}
}