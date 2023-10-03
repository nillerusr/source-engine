#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include "vgui_controls/Controls.h"
#include <vgui/IScheme.h>
#include <vgui_controls/Scrollbar.h>
#include <vgui_controls/ScrollbarSlider.h>

#include "ScrollingWindow.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

CScrollingWindow::CScrollingWindow( Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_pView = new vgui::Panel(this, "View");
	m_pVertScrollbar = new vgui::ScrollBar(this, "VertScrollbar", true);
	m_pHorizScrollbar = new vgui::ScrollBar(this, "HorizScrollbar", false);
	m_pVertScrollbar->SetButtonPressedScrollValue(160);
	m_pHorizScrollbar->SetButtonPressedScrollValue(160);
	m_pHorizScrollbar->AddActionSignalTarget(this);
	m_pVertScrollbar->SetValue(0);
	m_pHorizScrollbar->SetValue(0);
	m_pVertScrollbar->AddActionSignalTarget(this);
	m_iChildTall = 0;
	m_iChildWide = 0;
}

CScrollingWindow::~CScrollingWindow( void )
{
}

void CScrollingWindow::PerformLayout()
{
	BaseClass::PerformLayout();

	// arrange our scrollbars and view
	m_pVertScrollbar->SetBounds(GetWide() - 20, 0, 20, GetTall() - 30);
	m_pHorizScrollbar->SetBounds(0, GetTall() - 20, GetWide() - 30, 20);
	m_pView->SetBounds(0, 0, GetWide() - 30, GetTall() - 30);

	// position our child panel such that the area visible through us corresponds to the scrollbar positions
	if (!m_hChildPanel.Get())
		return;

	int cw = m_hChildPanel->GetWide();
	int ct = m_hChildPanel->GetTall();

	int xrange = MAX(0, cw - m_pView->GetWide());
	int yrange = MAX(0, ct - m_pView->GetTall());

	// make sure our scrollbars are setup with the correct range for the size of our child panel
	m_pHorizScrollbar->SetRange(0, xrange+m_pView->GetWide());
	m_pHorizScrollbar->SetRangeWindow(m_pView->GetWide());
	m_pVertScrollbar->SetRange(0, yrange+m_pView->GetTall());
	m_pVertScrollbar->SetRangeWindow(m_pView->GetTall());

	// move the child panel
	m_hChildPanel->SetPos(-m_pHorizScrollbar->GetValue(), -m_pVertScrollbar->GetValue());

	m_iChildTall = cw;
	m_iChildWide = ct;
}

void CScrollingWindow::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_pView->SetPaintBackgroundEnabled(true);
	m_pView->SetPaintBackgroundType(0);
	m_pView->SetBgColor(Color(0,0,0,255));
}

void CScrollingWindow::OnThink()
{
	BaseClass::OnThink();

	if ( m_hChildPanel.Get() )
	{
		if ( m_hChildPanel->GetTall() != m_iChildTall || m_hChildPanel->GetWide() != m_iChildWide) 
		{
			InvalidateLayout( true );
			m_iChildTall = m_hChildPanel->GetTall();
			m_iChildWide = m_hChildPanel->GetWide();
		}
	}
}

void CScrollingWindow::SetChildPanel(vgui::Panel *pPanel)
{
	if (!pPanel)
		return;

	if ( pPanel != m_hChildPanel.Get() )
	{
		m_iChildTall = 0;
		m_iChildWide = 0;
	}
	m_hChildPanel = pPanel;
	pPanel->SetParent(m_pView);
}

void CScrollingWindow::OnSliderMoved(int position)
{
	InvalidateLayout();
	Repaint();
}

void CScrollingWindow::MoveToTopLeft()
{
	int hmin, hmax;
	m_pHorizScrollbar->GetRange(hmin, hmax);
	m_pHorizScrollbar->SetValue(hmin);

	m_pVertScrollbar->GetRange(hmin, hmax);
	m_pVertScrollbar->SetValue(hmin);

	InvalidateLayout();
	m_pHorizScrollbar->InvalidateLayout();
	m_pHorizScrollbar->GetSlider()->InvalidateLayout();
	m_pVertScrollbar->InvalidateLayout();
	m_pVertScrollbar->GetSlider()->InvalidateLayout();
	Repaint();
	m_pHorizScrollbar->Repaint();
	m_pHorizScrollbar->GetSlider()->Repaint();
	m_pVertScrollbar->Repaint();
	m_pVertScrollbar->GetSlider()->Repaint();
}

void CScrollingWindow::MoveToCenter()
{
	int hmin, hmax;
	m_pHorizScrollbar->GetRange(hmin, hmax);
	m_pHorizScrollbar->SetValue(hmin + (hmax - hmin) * 0.5f - m_pHorizScrollbar->GetRangeWindow() * 0.5f);

	m_pVertScrollbar->GetRange(hmin, hmax);
	m_pVertScrollbar->SetValue(hmin + (hmax - hmin) * 0.5f - m_pVertScrollbar->GetRangeWindow() * 0.5f);
}

void CScrollingWindow::MoveToLowerCenter()
{
	int hmin, hmax;
	m_pHorizScrollbar->GetRange( hmin, hmax );
	m_pHorizScrollbar->SetValue( hmin + (hmax - hmin) * 0.5f - m_pHorizScrollbar->GetRangeWindow() * 0.5f );

	m_pVertScrollbar->GetRange( hmin, hmax );
	m_pVertScrollbar->SetValue( hmax - m_pVertScrollbar->GetRangeWindow() * 2 );
}

void CScrollingWindow::OnMouseWheeled(int delta)
{
	int val = m_pVertScrollbar->GetValue();
	val -= (delta * 30);
	m_pVertScrollbar->SetValue(val);
}
