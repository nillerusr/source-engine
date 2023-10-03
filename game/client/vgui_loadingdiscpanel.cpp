//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "iloadingdisc.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ProgressBar.h"
#include "hud_numericdisplay.h"
#include "vgui/ISurface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
const int NumSegments = 7;
static int coord[NumSegments+1] = {
	0,
	1,
	2,
	3,
	4,
	6,
	9,
	10
};


//-----------------------------------------------------------------------------
static void DrawRoundedBackground( Color bgColor, int wide, int tall )
{
	int x1, x2, y1, y2;
	vgui::surface()->DrawSetColor(bgColor);
	vgui::surface()->DrawSetTextColor(bgColor);

	int i;

	// top-left corner --------------------------------------------------------
	int xDir = 1;
	int yDir = -1;
	int xIndex = 0;
	int yIndex = NumSegments - 1;
	int xMult = 1;
	int yMult = 1;
	int x = 0;
	int y = 0;
	for ( i=0; i<NumSegments; ++i )
	{
		x1 = MIN( x + coord[xIndex]*xMult, x + coord[xIndex+1]*xMult );
		x2 = MAX( x + coord[xIndex]*xMult, x + coord[xIndex+1]*xMult );
		y1 = MAX( y + coord[yIndex]*yMult, y + coord[yIndex+1]*yMult );
		y2 = y + coord[NumSegments];
		vgui::surface()->DrawFilledRect( x1, y1, x2, y2 );

		xIndex += xDir;
		yIndex += yDir;
	}

	// top-right corner -------------------------------------------------------
	xDir = 1;
	yDir = -1;
	xIndex = 0;
	yIndex = NumSegments - 1;
	x = wide;
	y = 0;
	xMult = -1;
	yMult = 1;
	for ( i=0; i<NumSegments; ++i )
	{
		x1 = MIN( x + coord[xIndex]*xMult, x + coord[xIndex+1]*xMult );
		x2 = MAX( x + coord[xIndex]*xMult, x + coord[xIndex+1]*xMult );
		y1 = MAX( y + coord[yIndex]*yMult, y + coord[yIndex+1]*yMult );
		y2 = y + coord[NumSegments];
		vgui::surface()->DrawFilledRect( x1, y1, x2, y2 );
		xIndex += xDir;
		yIndex += yDir;
	}

	// bottom-right corner ----------------------------------------------------
	xDir = 1;
	yDir = -1;
	xIndex = 0;
	yIndex = NumSegments - 1;
	x = wide;
	y = tall;
	xMult = -1;
	yMult = -1;
	for ( i=0; i<NumSegments; ++i )
	{
		x1 = MIN( x + coord[xIndex]*xMult, x + coord[xIndex+1]*xMult );
		x2 = MAX( x + coord[xIndex]*xMult, x + coord[xIndex+1]*xMult );
		y1 = y - coord[NumSegments];
		y2 = MIN( y + coord[yIndex]*yMult, y + coord[yIndex+1]*yMult );
		vgui::surface()->DrawFilledRect( x1, y1, x2, y2 );
		xIndex += xDir;
		yIndex += yDir;
	}

	// bottom-left corner -----------------------------------------------------
	xDir = 1;
	yDir = -1;
	xIndex = 0;
	yIndex = NumSegments - 1;
	x = 0;
	y = tall;
	xMult = 1;
	yMult = -1;
	for ( i=0; i<NumSegments; ++i )
	{
		x1 = MIN( x + coord[xIndex]*xMult, x + coord[xIndex+1]*xMult );
		x2 = MAX( x + coord[xIndex]*xMult, x + coord[xIndex+1]*xMult );
		y1 = y - coord[NumSegments];
		y2 = MIN( y + coord[yIndex]*yMult, y + coord[yIndex+1]*yMult );
		vgui::surface()->DrawFilledRect( x1, y1, x2, y2 );
		xIndex += xDir;
		yIndex += yDir;
	}

	// paint between top left and bottom left ---------------------------------
	x1 = 0;
	x2 = coord[NumSegments];
	y1 = coord[NumSegments];
	y2 = tall - coord[NumSegments];
	vgui::surface()->DrawFilledRect( x1, y1, x2, y2 );

	// paint between left and right -------------------------------------------
	x1 = coord[NumSegments];
	x2 = wide - coord[NumSegments];
	y1 = 0;
	y2 = tall;
	vgui::surface()->DrawFilledRect( x1, y1, x2, y2 );

	// paint between top right and bottom right -------------------------------
	x1 = wide - coord[NumSegments];
	x2 = wide;
	y1 = coord[NumSegments];
	y2 = tall - coord[NumSegments];
	vgui::surface()->DrawFilledRect( x1, y1, x2, y2 );
}

//-----------------------------------------------------------------------------
// Purpose: Displays the loading plaque
//-----------------------------------------------------------------------------
class CLoadingDiscPanel : public vgui::EditablePanel
{
	typedef vgui::EditablePanel BaseClass;
public:
	CLoadingDiscPanel( vgui::VPANEL parent );
	~CLoadingDiscPanel();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		int w, h;
		w = ScreenWidth();
		h = ScreenHeight();

		if ( w != m_ScreenSize[ 0 ] || 
			 h != m_ScreenSize[ 1 ] )
		{
			m_ScreenSize[ 0 ] = w;
			m_ScreenSize[ 1 ] = h;

			// Re-perform the layout if the screen size changed
			LoadControlSettings( "resource/LoadingDiscPanel.res" );
		}

		// center the dialog
		int wide, tall;
		GetSize( wide, tall );
		SetPos( ( w - wide ) / 2, ( h - tall ) / 2 );
	}

	virtual void PaintBackground( void )
	{
		int wide, tall;
		GetSize( wide, tall );

		DrawRoundedBackground( Color(0, 0, 0, 255), wide, tall );
	}

	virtual void SetText( const char *text )
	{
		m_pLoadingLabel->SetText( text );
	}

	bool UpdateProgressBar( float progress, const char *statusText )
	{
		m_pProgressBar->SetBarInset( 2 );
		float currentProgress = m_pProgressBar->GetProgress();
		m_pProgressBar->SetProgress( progress );

		return m_pProgressBar->GetProgress() != currentProgress;
	}

	vgui::VPANEL GetProgressBarVPanel( void )
	{
		if ( !m_pProgressBar )
			return 0;

		return m_pProgressBar->GetVPanel();
	}

private:
	vgui::Label *m_pLoadingLabel;
	vgui::ProgressBar *m_pProgressBar;
	int			m_ScreenSize[ 2 ];
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CLoadingDiscPanel::CLoadingDiscPanel( vgui::VPANEL parent ) : BaseClass( NULL, "CLoadingDiscPanel" )
{
	int w, h;
	w = ScreenWidth();
	h = ScreenHeight();

	SetParent( parent );
	SetProportional( true );
	SetScheme( "ClientScheme" );
	SetVisible( false );
	SetCursor( NULL );

	m_pLoadingLabel = vgui::SETUP_PANEL(new vgui::Label( this, "LoadingLabel", "" ));
	m_pLoadingLabel->SetPaintBackgroundEnabled( false );

	m_pProgressBar = vgui::SETUP_PANEL(new vgui::ProgressBar( this, "LoadingProgress" ));

	LoadControlSettings( "resource/LoadingDiscPanel.res" );

	// center the dialog
	int wide, tall;
	GetSize( wide, tall );
	SetPos( ( w - wide ) / 2, ( h - tall ) / 2 );

	m_ScreenSize[ 0 ] = w;
	m_ScreenSize[ 1 ] = h;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CLoadingDiscPanel::~CLoadingDiscPanel()
{
}

class CLoadingDisc : public ILoadingDisc
{
private:
	CLoadingDiscPanel *loadingDiscPanel;
	CLoadingDiscPanel *m_pPauseDiscPanel;
	vgui::VPANEL m_hParent;

public:
	CLoadingDisc( void )
	{
		loadingDiscPanel = NULL;
	}

	void Create( vgui::VPANEL parent )
	{
		// don't create now, only when it's needed
		m_hParent = parent;
	}

	void Destroy( void )
	{
		if ( loadingDiscPanel )
		{
			loadingDiscPanel->SetParent( (vgui::Panel *)NULL );
			delete loadingDiscPanel;
		}

		if ( m_pPauseDiscPanel )
		{
			m_pPauseDiscPanel->SetParent( (vgui::Panel *)NULL );
			delete m_pPauseDiscPanel;
		}
	}

	void SetLoadingVisible( bool bVisible )
	{
		// demand-create the dialog
		if ( bVisible && !loadingDiscPanel )
		{
			loadingDiscPanel = vgui::SETUP_PANEL(new CLoadingDiscPanel( m_hParent ) );
		}

		if ( loadingDiscPanel )
		{
			loadingDiscPanel->SetVisible( bVisible );
		}
	}


	void SetPausedVisible( bool bVisible )
	{
		if ( bVisible && !m_pPauseDiscPanel )
		{
			m_pPauseDiscPanel = vgui::SETUP_PANEL(new CLoadingDiscPanel( m_hParent ) );
			m_pPauseDiscPanel->SetText( "#gameui_paused" );
		}

		if ( m_pPauseDiscPanel )
		{
			m_pPauseDiscPanel->SetVisible( bVisible );
		}
	}

	bool UpdateProgressBar( float progress, const char *statusText )
	{
		if ( loadingDiscPanel && loadingDiscPanel->IsVisible() )
		{
			return loadingDiscPanel->UpdateProgressBar( progress, statusText );
		}

		return false;
	}

	unsigned int GetLoadingVPANEL( void )
	{
		if ( loadingDiscPanel )
			return loadingDiscPanel->GetProgressBarVPanel();

		return 0;
	}
};

static CLoadingDisc g_LoadingDisc;
ILoadingDisc *loadingdisc = ( ILoadingDisc * )&g_LoadingDisc;
