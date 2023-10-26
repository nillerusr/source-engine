#include "cbase.h"
#include "vgui/ivgui.h"
#include <vgui/vgui.h>
#include <vgui/ischeme.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/TextImage.h>
#include "convar.h"
#include "MedalCollectionPanel.h"
#include "asw_medal_collection_frame.h"
#include "SwarmopediaPanel.h"
#include <vgui/isurface.h>
#include "ienginevgui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_Medal_Collection_Frame::CASW_Medal_Collection_Frame( vgui::Panel *pParent, const char *pElementName) :
	Frame(pParent, pElementName)
{	
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx(0, "resource/SwarmFrameScheme.res", "SwarmFrameScheme");
	SetScheme(scheme);

	// create a propertysheet
	m_pSheet = new vgui::PropertySheet(this, "IAFDatabaseSheet");
	m_pSheet->SetPos(0,0);
	m_pSheet->SetSize(GetWide(), GetTall() - 30);
	m_pSheet->SetVisible(true);

	m_pMedalOffsetPanel = new vgui::Panel(m_pSheet, "MedalOffset");
	m_pCollectionPanel = new MedalCollectionPanel(m_pMedalOffsetPanel, "MedalCollection");	
	m_pSheet->AddPage(m_pMedalOffsetPanel, "#asw_medals_tab");

	m_pSwarmopediaPanel = new SwarmopediaPanel(m_pSheet, "SwarmopediaPanel");
	m_pSheet->AddPage(m_pSwarmopediaPanel, "#asw_swarmopedia");

	m_pCancelButton = new vgui::Button(this, "CancelButton", "#asw_chooser_close", this, "Cancel");
	m_pCancelButton->SetContentAlignment(vgui::Label::a_west);

	m_pSheet->SetActivePage(m_pCollectionPanel); //m_pSwarmopediaPanel);
}

CASW_Medal_Collection_Frame::~CASW_Medal_Collection_Frame()
{
}

void CASW_Medal_Collection_Frame::PerformLayout()
{
	BaseClass::PerformLayout();

	int sw, sh;
	vgui::surface()->GetScreenSize( sw, sh );
	float fScale = float(sh) / 768.0f;
	int padding = 5 * fScale;

	int x, y, wide, tall;
	GetClientArea(x, y, wide, tall);
	m_pSheet->SetBounds(x, 5, wide, GetTall());
	
	int iFooterSize = 32.0f * fScale;

	m_pCancelButton->GetTextImage()->ResizeImageToContent();
	m_pCancelButton->SizeToContents();
	int cancel_wide = m_pCancelButton->GetWide();
	//m_pCancelButton->SetSize(cancel_wide, iFooterSize - 4);
	m_pCancelButton->SetPos(GetWide() - (cancel_wide + padding * 2),
							GetTall() - iFooterSize);

	m_pCollectionPanel->SetBounds(0, y, wide, tall - iFooterSize);	// todo: reduce tall a bit?
	m_pMedalOffsetPanel->SetBounds(0, 0, wide, tall);	// todo: reduce tall a bit?
	m_pSwarmopediaPanel->SetBounds(x, y, wide, tall - iFooterSize);	// todo: reduce tall a bit?
	
}

void CASW_Medal_Collection_Frame::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	m_pCancelButton->SetFont( pScheme->GetFont( "DefaultButton", IsProportional() ) );	
	SetPaintBackgroundType(2);
	//m_pCancelButton->SetDefaultBorder(pScheme->GetBorder("ButtonBorder"));
	//m_pCancelButton->SetDepressedBorder(pScheme->GetBorder("ButtonBorder"));
	//m_pCancelButton->SetKeyFocusBorder(pScheme->GetBorder("ButtonBorder"));
	//m_pCancelButton->SetDefaultColor(Color(255,128,0,255), Color(0,0,0,255));
	//m_pCancelButton->SetArmedColor(Color(255,128,0,255), Color(65,74,96,255));
	//m_pCancelButton->SetDepressedColor(Color(255,128,0,255), Color(65,74,96,255));
}

void CASW_Medal_Collection_Frame::OnCommand(const char* command)
{
	if (!stricmp(command, "Cancel"))
	{
		Close();
		//SetVisible(false);
		//MarkForDeletion();
		return;
	}
	BaseClass::OnCommand(command);
}