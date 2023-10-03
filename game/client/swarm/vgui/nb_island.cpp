#include "cbase.h"
#include "nb_island.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Label.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CNB_Island::CNB_Island( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_pBackground = new vgui::Panel( this, "Background" );
	m_pBackgroundInner = new vgui::Panel( this, "BackgroundInner" );
	m_pTitleBG = new vgui::Panel( this, "TitleBG" );
	m_pTitleBGBottom = new vgui::Panel( this, "TitleBGBottom" );
	m_pTitle = new vgui::Label( this, "Title", "" );
	m_pTitleBGLine = new vgui::Panel( this, "TitleBGLine" );
}

CNB_Island::~CNB_Island()
{

}

void CNB_Island::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/ui/nb_island.res" );
}

void CNB_Island::PerformLayout()
{
	BaseClass::PerformLayout();

	int x, y, w, t;
	GetBounds( x, y, w, t );

	m_pBackground->SetBounds( 0, 0, w, t );

	int border = YRES( 2 );
	m_pBackgroundInner->SetBounds( border, border, w - border * 2, t - border * 2 );

	int title_border = YRES( 3 );
	m_pTitleBG->SetBounds( title_border, title_border, w - title_border * 2, YRES( 24 ) );
	m_pTitleBGBottom->SetBounds( title_border, YRES( 15 ), w - title_border * 2, YRES( 12 ) );
	m_pTitleBGLine->SetBounds( title_border, YRES( 28 ), w - title_border * 2, YRES( 1 ) );
	m_pTitle->SetBounds( title_border, title_border, w - title_border * 2, YRES( 24 ) );
}
