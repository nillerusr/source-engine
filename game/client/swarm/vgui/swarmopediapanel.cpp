#include "cbase.h"
#include "vgui/ivgui.h"
#include <vgui/vgui.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/TextImage.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/PanelListPanel.h>
#include <vgui_controls/HTML.h>
#include <vgui_controls/ScrollBar.h>
#include "FileSystem.h"
#include <vgui_controls/Panel.h>
#include <vgui/isurface.h>
#include "SwarmopediaPanel.h"
#include "SwarmopediaTopics.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


SwarmopediaPanel::SwarmopediaPanel( vgui::Panel *pParent, const char *pElementName)	: vgui::Panel(pParent, pElementName)
{
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmFrameScheme.res", "SwarmFrameScheme");
	SetScheme(scheme);

	m_pHTML = NULL;
	m_pList = NULL;
	m_pTopicLabel = NULL;
	m_bCreatedPanels = false;
}

SwarmopediaPanel::~SwarmopediaPanel()
{
}

void SwarmopediaPanel::OnThink()
{
	BaseClass::OnThink();

	if (!m_bCreatedPanels)
	{
		m_bCreatedPanels = true;

		m_pHTML = new vgui::HTML( this, "SwarmopediaHTML");
		m_pList = new vgui::PanelListPanel(this, "TopicsList");
		m_pList->SetVerticalBufferPixels( 0 );

		m_pTopicLabel = new vgui::Label( this, "TopicsLabel", "#asw_so_topics");

		ShowDoc("index");
		ShowList("initial");
		InvalidateLayout(true, true);
	}
}

void SwarmopediaPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	int sw, sh;
	vgui::surface()->GetScreenSize( sw, sh );
	float fScale = float(sh) / 768.0f;

	int list_wide = 200.0f * fScale;
	int padding = 5 * fScale;
	int iFooterSize = 32.0f * fScale;
	
	int html_left = list_wide + padding;
	int html_wide = GetWide() - (html_left + padding);
	if (m_pHTML)
		m_pHTML->SetBounds(html_left, padding, html_wide, GetTall() - (iFooterSize + padding * 3));

	int text_high = fScale * 20;
	if (m_pTopicLabel)
		m_pTopicLabel->SetBounds(padding, padding, list_wide - padding * 2, text_high);

	Msg("SwarmopediaPanel::PerformLayout.  fscale=%f list_wide=%d padding=%d footer=%d\n", fScale, list_wide, padding, iFooterSize);

	if (m_pList)
	{
		m_pList->SetBounds(padding, padding + text_high, list_wide - padding * 2, GetTall() - (iFooterSize + padding * 3));
		m_pList->SetFirstColumnWidth(0);
	}
}

void SwarmopediaPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	if (m_pHTML)
		m_pHTML->SetBgColor(Color(0,0,0,255));
}

void SwarmopediaPanel::OnCommand(const char* command)
{
	BaseClass::OnCommand(command);
}

bool SwarmopediaPanel::ShowDoc(const char *szDoc)
{
	if (!m_pHTML)
		return false;

	//Msg("SwarmopediaPanel::ShowDoc %s\n", szDoc);
	char fileRES[ MAX_PATH ];

	char uilanguage[ 64 ];
	engine->GetUILanguage( uilanguage, sizeof( uilanguage ) );

	Q_snprintf( fileRES, sizeof( fileRES ), "resource/swarmopedia/%s_%s.html", szDoc, uilanguage );

	bool bFoundHTML = false;

	if ( !g_pFullFileSystem->FileExists( fileRES ) )
	{
		// try english
		Q_snprintf( fileRES, sizeof( fileRES ), "resource/swarmopedia/%s_english.html", szDoc );
	}
	else
	{
		bFoundHTML = true;
	}

	if( bFoundHTML || g_pFullFileSystem->FileExists( fileRES ) )
	{
		// it's a local HTML file
		char localURL[ _MAX_PATH + 7 ];
		Q_strncpy( localURL, "file://", sizeof( localURL ) );

		char pPathData[ _MAX_PATH ];
		g_pFullFileSystem->GetLocalPath( fileRES, pPathData, sizeof(pPathData) );
		Q_strncat( localURL, pPathData, sizeof( localURL ), COPY_ALL_CHARACTERS );

		// force steam to dump a local copy
		g_pFullFileSystem->GetLocalCopy( pPathData );
		
		m_pHTML->SetVisible( true );
		m_pHTML->OpenURL( localURL );

		InvalidateLayout();
		Repaint();
		return true;
	}
	else
	{
		Msg("Couldn't find html %s\n", fileRES);
		m_pHTML->SetVisible( false );	
	}
	return false;
}


void SwarmopediaPanel::AddListEntry(const char* szName, const char* szArticle, const char* szListTarget, int iSectionHeader)
{
	if (!m_pList)
		return;

	SwarmopediaTopicButton *pTopic = new SwarmopediaTopicButton(this, "TopicButton", this);
	pTopic->m_pLabel->SetText(szName);
	Q_snprintf(pTopic->m_szArticleTarget, sizeof(pTopic->m_szArticleTarget), "%s", szArticle);
	Q_snprintf(pTopic->m_szListTarget, sizeof(pTopic->m_szListTarget), "%s", szListTarget);
	pTopic->m_iSectionHeader = iSectionHeader;

	m_pList->AddItem(NULL, pTopic);

	pTopic->InvalidateLayout(true, true);
}

void SwarmopediaPanel::ListEntryClicked(SwarmopediaTopicButton *pTopic)
{	
	if (!pTopic)
		return;

	vgui::surface()->PlaySound("swarm/interface/tabclick2.wav");
	
	// change article if needed
	if (Q_strlen(pTopic->m_szArticleTarget) > 0)
	{
		ShowDoc(pTopic->m_szArticleTarget);
	}

	// change list if needed
	if (Q_strlen(pTopic->m_szListTarget) > 0)
	{
		ShowList(pTopic->m_szListTarget);
	}
}

bool SwarmopediaPanel::ShowList(const char *szListName)
{
	if (!m_pList)
		return false;

	//Msg("ShowList(%s)\n", szListName);
	if (!GetSwarmopediaTopics())
	{
		Msg("Show list failed to find topics\n");
		return false;
	}

	m_pList->DeleteAllItems();
	//m_pList->SetVerticalBufferPixels(0);

	if (!GetSwarmopediaTopics()->SetupList(this, szListName))
	{
		GetSwarmopediaTopics()->SetupList(this, "initial");
		return false;
	}
	m_pList->InvalidateLayout(true);
	return true;
}


// =============================================================

SwarmopediaTopicButton::SwarmopediaTopicButton( vgui::Panel *pParent, const char *pElementName, SwarmopediaPanel *pSwarmopedia)	: vgui::Panel(pParent, pElementName)
{	
	m_pSwarmopedia = pSwarmopedia;
	m_pLabel = new vgui::Label(this, "TopicLabel", "");	
	m_pLabel->SetContentAlignment(vgui::Label::a_northwest);

	SetMouseInputEnabled(true);
	m_pLabel->SetMouseInputEnabled(false);

	m_hFont = vgui::INVALID_FONT;
	m_iSectionHeader = 0;

	m_szArticleTarget[0] = '\0';
	m_szListTarget[0] = '\0';
}

SwarmopediaTopicButton::~SwarmopediaTopicButton()
{
}

void SwarmopediaTopicButton::PerformLayout()
{
	BaseClass::PerformLayout();

	vgui::PanelListPanel *pParent = dynamic_cast<vgui::PanelListPanel*>(GetParent());
	int iScrollbarSize = 20;
	if (pParent)
		iScrollbarSize = pParent->GetScrollBar() ? pParent->GetScrollBar()->GetWide() : 20;

	int sw, sh;
	vgui::surface()->GetScreenSize( sw, sh );
	float fScale = float(sh) / 768.0f;

	int list_wide = 160.0f * fScale;
	int font_tall = vgui::surface()->GetFontTall(m_hFont);
	int padding = 5 * fScale;

	int button_wide = list_wide - padding * 2;
	int xindent = 0;
	if (m_iSectionHeader <= 0)
	{
		xindent = (padding * 3) * (1 - m_iSectionHeader);
	}
	
	SetSize(button_wide*5, font_tall);
	m_pLabel->SetBounds(xindent, 0, button_wide - xindent, font_tall);
	m_pLabel->InvalidateLayout();
	//m_pLabel->GetTextImage()->RecalculateNewLinePositions();
}

void SwarmopediaTopicButton::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_hFont = pScheme->GetFont( "DefaultButton", IsProportional() );
	m_pLabel->SetFont(m_hFont);

	if (m_iSectionHeader <= 0)
		m_pLabel->SetFgColor(Color(66,142,192,255));
	else
		m_pLabel->SetFgColor(Color(192,192,192,255));
}

void SwarmopediaTopicButton::OnMouseReleased(vgui::MouseCode code)
{
	if (!IsCursorOver())
		return;

	if (!m_pSwarmopedia)
		return;

	if ( code != MOUSE_LEFT )
		return;

	m_pSwarmopedia->ListEntryClicked(this);
}

void SwarmopediaTopicButton::OnCursorEntered()
{
	BaseClass::OnCursorEntered();

	m_pLabel->SetFgColor(Color(0,0,0,255));
	m_pLabel->SetBgColor(Color(192,192,192,255));
	m_pLabel->SetPaintBackgroundEnabled(true);
	m_pLabel->SetPaintBackgroundType(0);
}

void SwarmopediaTopicButton::OnCursorExited()
{
	BaseClass::OnCursorExited();

	if (m_iSectionHeader <= 0)
		m_pLabel->SetFgColor(Color(66,142,192,255));
	else
		m_pLabel->SetFgColor(Color(192,192,192,255));
	m_pLabel->SetBgColor(Color(0,0,0,0));
	m_pLabel->SetPaintBackgroundEnabled(false);
}