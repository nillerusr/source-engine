//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#include <stdio.h>
#include "hlfaceposer.h"
#include "choreoactorwidget.h"
#include "choreochannelwidget.h"
#include "choreoactor.h"
#include "choreoview.h"
#include "choreowidgetdrawhelper.h"
#include "mxBitmapButton.h"
#include "choreoviewcolors.h"
#include "choreochannel.h"
#include "filesystem.h"
#include "StudioModel.h"

#define ACTOR_NAME_HEIGHT 26
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actor - 
//			*parent - 
//			x - 
//			y - 
//			w - 
//			h - 
//			0 - 
//			0 - 
//-----------------------------------------------------------------------------
CActorBitmapButton::CActorBitmapButton( CChoreoActorWidget *actor, mxWindow *parent, int x, int y, int w, int h, int id /*= 0*/, const char *bitmap /*= 0*/ )
: mxBitmapButton( parent, x, y, w, h, id, bitmap )
{
	m_pActor = actor;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CChoreoActorWidget	*CActorBitmapButton::GetActor( void )
{
	return m_pActor;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CActorActiveCheckBox::CActorActiveCheckBox( CChoreoActorWidget *actor, mxWindow *parent, int x, int y, int w, int h, const char *label /*= 0*/, int id /*= 0*/ )
	: mxCheckBox( parent, x, y, w, h, label, id )
{
	m_pActor = actor;
}

CChoreoActorWidget *CActorActiveCheckBox::GetActor( void )
{
	return m_pActor;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
CChoreoActorWidget::CChoreoActorWidget( CChoreoWidget *parent )
: CChoreoWidget( parent )
{
	m_pParent = parent;

	m_pActor = NULL;

	m_bShowChannels = true;

	m_btnOpen = new CActorBitmapButton( this, m_pView, 0, 0, 0, 0, IDC_CHANNELOPEN, "gfx/hlfaceposer/channelopen.bmp" );
	m_btnClose = new CActorBitmapButton( this, m_pView, 0, 0, 0, 0, IDC_CHANNELCLOSE, "gfx/hlfaceposer/channelclose.bmp"  );

	ShowChannels( m_bShowChannels );

	memset( m_rgCurrentSetting, 0, sizeof( m_rgCurrentSetting ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CChoreoActorWidget::~CChoreoActorWidget( void )
{
	for ( int i = 0 ; i < m_Channels.Size(); i++ )
	{
		CChoreoChannelWidget *c = m_Channels[ i ];
		delete c;
	}
	m_Channels.RemoveAll();

	delete m_btnOpen;
	delete m_btnClose;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CChoreoActorWidget::GetShowChannels( void )
{
	return m_bShowChannels;
}

//-----------------------------------------------------------------------------
// Purpose: Switch modes
// Input  : show - 
//-----------------------------------------------------------------------------
void CChoreoActorWidget::ShowChannels( bool show )
{
	m_bShowChannels = show;

	m_btnOpen->setVisible( !m_bShowChannels );
	m_btnClose->setVisible( m_bShowChannels );

	m_pView->InvalidateLayout();

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoActorWidget::Create( void )
{
	// Create objects for children
	Assert( m_pActor );

	// Create objects for children
	for ( int i = 0; i < m_pActor->GetNumChannels(); i++ )
	{
		CChoreoChannel *channel = m_pActor->GetChannel( i );
		Assert( channel );
		if ( !channel )
		{
			continue;
		}

		CChoreoChannelWidget *channelWidget = new CChoreoChannelWidget( this );
		channelWidget->SetChannel( channel );
		channelWidget->Create();

		AddChannel( channelWidget );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rc - 
//-----------------------------------------------------------------------------
void CChoreoActorWidget::Layout( RECT& rc )
{
	setBounds( rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top );

	int buttonSize = 16;
	int ypos = rc.top + ( ACTOR_NAME_HEIGHT - buttonSize )/ 2;

	m_btnOpen->setBounds( rc.left + 2, ypos, buttonSize, buttonSize );
	m_btnClose->setBounds( rc.left + 2, ypos, buttonSize, buttonSize );

	bool buttonsVisible = ( ypos > m_pView->GetStartRow() && ( ypos + buttonSize ) < m_pView->GetEndRow() ) ? true : false;

	if ( !buttonsVisible )
	{
		m_btnOpen->setVisible( false );
		m_btnClose->setVisible( false );
	}
	else
	{
		m_btnOpen->setVisible( !m_bShowChannels );
		m_btnClose->setVisible( m_bShowChannels );
	}

	RECT rcChannels;
	rcChannels = rc;
	rcChannels.top += ACTOR_NAME_HEIGHT;

	// Create objects for children
	for ( int i = 0; i < m_Channels.Size(); i++ )
	{
		CChoreoChannelWidget *channel = m_Channels[ i ];
		Assert( channel );
		if ( !channel )
		{
			continue;
		}

		rcChannels.bottom = rcChannels.top + channel->GetItemHeight();

		channel->Layout( rcChannels );

		OffsetRect( &rcChannels, 0, channel->GetItemHeight() );

		channel->setVisible( m_bShowChannels );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CChoreoActorWidget::GetItemHeight( void )
{
	int itemHeight = ACTOR_NAME_HEIGHT + 2;
	if ( m_bShowChannels )
	{
		for ( int i = 0; i < m_Channels.Size(); i++ )
		{
			CChoreoChannelWidget *channel = m_Channels[ i ];
			itemHeight += channel->GetItemHeight();
		}
	}
	return itemHeight;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoActorWidget::redraw( CChoreoWidgetDrawHelper& drawHelper )
{
	if ( !getVisible() )
		return;

	CChoreoActor *actor = GetActor();
	if ( !actor )
		return;

	RECT rcClient = getBounds();

	if ( !actor->GetActive() )
	{
		RECT rcBg = rcClient;
		rcBg.right = rcBg.left + m_pView->GetLabelWidth() ;
		InflateRect( &rcBg, -3, -5 );

		drawHelper.DrawFilledRect( RGB( 220, 220, 220 ), rcBg );
	}

	RECT rcText;

	rcText.left = rcClient.left;
	rcText.right = rcClient.left + m_pView->GetLabelWidth();
	rcText.top = rcClient.top;
	rcText.bottom = rcClient.top + ACTOR_NAME_HEIGHT;

	drawHelper.DrawColoredLine( COLOR_CHOREO_ACTORLINE, PS_SOLID, 1, 0, rcText.top,
		rcClient.right, rcText.top );

	drawHelper.DrawColoredLine( COLOR_CHOREO_ACTORLINE, PS_SOLID, 1, 0, rcClient.bottom-2,
		rcClient.right, rcClient.bottom-2 );
	drawHelper.DrawColoredLine( RGB(200,206,255), PS_SOLID, 1, 0, rcClient.bottom-1,
		rcClient.right, rcClient.bottom-1 );

	drawHelper.DrawColoredLine( COLOR_CHOREO_DIVIDER, PS_SOLID, 1, rcText.right, rcClient.top,
		rcText.right, rcClient.bottom-1 );

	RECT rcName = rcText;

	rcName.left += 18;
	char n[ 512 ];
	V_strcpy_safe( n, actor->GetName() );

	drawHelper.DrawColoredText( "Arial", 
		m_pView->GetFontSize() + 5, 
		1000, 
		actor->GetActive() ? COLOR_CHOREO_ACTORNAME : COLOR_CHOREO_ACTORNAME_INACTIVE,
		rcName, n );

	if ( !actor->GetActive() )
	{
		strcpy( n, "(inactive)" );

		RECT rcInactive = rcName;
		int len = drawHelper.CalcTextWidth( "Arial", m_pView->GetFontSize() - 2, 500, n );
		rcInactive.left = rcInactive.right - len - 5;
		rcInactive.top += 3;
		rcInactive.bottom = rcInactive.top + m_pView->GetFontSize() - 2;

		drawHelper.DrawColoredText( "Arial", m_pView->GetFontSize() - 2, 500,
			COLOR_CHOREO_ACTORNAME_INACTIVE, rcInactive, n );
	}

	rcName.left -= 18;


	if ( actor->GetFacePoserModelName()[0] )
	{
		int textWidth = drawHelper.CalcTextWidth( "Arial", m_pView->GetFontSize() + 5, 1000, actor->GetName() );
		RECT rcModelName = rcName;
		rcModelName.left += ( 14 + textWidth + 2 );

		int fontsize = m_pView->GetFontSize() - 2;

		char shortname[ 512 ];
		Q_FileBase (actor->GetFacePoserModelName(), shortname, sizeof( shortname ) );
		strcat( shortname, ".mdl" );

		int len = drawHelper.CalcTextWidth( "Arial", fontsize, FW_NORMAL, shortname );

		rcModelName.left = rcModelName.right - len - 5;

		rcModelName.top = rcModelName.bottom - fontsize;
		OffsetRect( &rcModelName, 0, -3 );
		drawHelper.DrawColoredText( "Arial", fontsize, FW_NORMAL, COLOR_CHOREO_LIGHTTEXT, rcModelName, shortname );
	}
	if ( m_bShowChannels )
	{
		for ( int j = 0; j < GetNumChannels(); j++ )
		{
			CChoreoChannelWidget *channel = GetChannel( j );
			if ( channel )
			{
				channel->redraw( drawHelper );
			}

			RECT rcChannel = channel->getBounds();

			drawHelper.DrawColoredLine( COLOR_CHOREO_ACTORLINE, PS_SOLID, 1, rcText.right+1, rcChannel.top,
				rcChannel.right, rcChannel.top );

			drawHelper.DrawColoredLine( COLOR_CHOREO_ACTORLINE, PS_SOLID, 1, rcText.right+1, rcChannel.bottom,
				rcChannel.right, rcChannel.bottom );

		}
		return;
	}

	OffsetRect( &rcName, m_pView->GetLabelWidth() + 10, 0 );
	rcName.right = w();

	char sz[ 256 ];
	// count channels and events
	int ev = 0;
	for ( int i = 0; i < actor->GetNumChannels(); i++ )
	{
		CChoreoChannel *ch = actor->GetChannel( i );
		if ( ch )
		{
			ev += ch->GetNumEvents();
		}
	}
	sprintf( sz, "%i channels with %i events hidden", actor->GetNumChannels(), ev );
	drawHelper.DrawColoredText( "Arial", m_pView->GetFontSize(), FW_NORMAL, COLOR_CHOREO_ACTORNAME, rcName, sz );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CChoreoActor
//-----------------------------------------------------------------------------
CChoreoActor *CChoreoActorWidget::GetActor( void )
{
	return m_pActor;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actor - 
//-----------------------------------------------------------------------------
void CChoreoActorWidget::SetActor( CChoreoActor *actor )
{
	m_pActor = actor;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *channel - 
//-----------------------------------------------------------------------------
void CChoreoActorWidget::AddChannel( CChoreoChannelWidget *channel )
{
	m_Channels.AddToTail( channel );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *channel - 
//-----------------------------------------------------------------------------
void CChoreoActorWidget::RemoveChannel( CChoreoChannelWidget *channel )
{
	m_Channels.FindAndRemove( channel );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : num - 
// Output : CChoreoChannelWidget
//-----------------------------------------------------------------------------
CChoreoChannelWidget *CChoreoActorWidget::GetChannel( int num )
{
	return m_Channels[ num ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CChoreoActorWidget::GetNumChannels( void )
{
	return m_Channels.Size();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float *CChoreoActorWidget::GetSettings( void )
{
	return m_rgCurrentSetting;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CChoreoActorWidget::ResetSettings( void )
{
	memset( m_rgCurrentSetting, 0, sizeof( m_rgCurrentSetting ) );
}
