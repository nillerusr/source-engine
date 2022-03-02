//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//=============================================================================//

#include "cbase.h"
#include "tier3/tier3.h"
#include "vgui/ILocalize.h"
#include "lifetime_stats_page.h"
#include <vgui_controls/SectionedListPanel.h>
#include "cs_client_gamestats.h" 
#include "filesystem.h"
#include "cs_weapon_parse.h"
#include "buy_presets/buy_presets.h"
#include "../vgui_controls/ScrollBar.h"
#include "stat_card.h"

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


KeyValues *g_pPreloadedCSBaseStatGroupLayout = NULL;

//-----------------------------------------------------------------------------
// Purpose: creates child panels, passes down name to pick up any settings from res files.
//-----------------------------------------------------------------------------
CBaseStatsPage::CBaseStatsPage(vgui::Panel *parent, const char *name) : BaseClass(parent, "CSBaseStatsDialog")
{
	vgui::IScheme *pScheme = scheme()->GetIScheme( GetScheme() );

	m_listItemFont = pScheme->GetFont( "StatsPageText", IsProportional() );

	m_statsList = new SectionedListPanel( this, "StatsList" );
	m_statsList->SetClickable(false);
	m_statsList->SetDrawHeaders(false);

	m_bottomBar = new ImagePanel(this, "BottomBar");

	m_pGroupsList = new vgui::PanelListPanel( this, "listpanel_groups" );
	m_pGroupsList->SetFirstColumnWidth( 0 );

	SetBounds(0, 0, 900, 780);
	SetMinimumSize( 256, 780 );

	SetBgColor(GetSchemeColor("ListPanel.BgColor", GetBgColor(), pScheme));

	m_pStatCard = new StatCard(this, "ignored");

	ListenForGameEvent( "player_stats_updated" );

	m_bStatsDirty = true;
}

CBaseStatsPage::~CBaseStatsPage()
{
	delete m_statsList;
}


void CBaseStatsPage::MoveToFront()
{
    UpdateStatsData();
	m_pStatCard->UpdateInfo();
}

void CBaseStatsPage::UpdateStatsData()
{
	// Hide the group list scrollbar
	if (m_pGroupsList->GetScrollbar())
	{
		m_pGroupsList->GetScrollbar()->SetWide(0);
	}

	UpdateGroupPanels();
	RepopulateStats();

	m_bStatsDirty = false;
}

//-----------------------------------------------------------------------------
// Purpose: Loads settings from statsdialog.res in hl2/resource/ui/
//-----------------------------------------------------------------------------
void CBaseStatsPage::ApplySchemeSettings( vgui::IScheme *pScheme )
{
    BaseClass::ApplySchemeSettings( pScheme );
    LoadControlSettings("resource/ui/CSBaseStatsDialog.res");

	m_statsList->SetClickable(false);
	m_statsList->SetDrawHeaders(false);

	m_statsList->SetVerticalScrollbar(true);

	SetBgColor(Color(86,86,86,255));

	//Remove any pre-existing sections and add then fresh (this can happen on a resolution change)
	m_statsList->RemoveAllSections();

	m_statsList->AddSection( 0, "Players");

	m_statsList->SetFontSection(0, m_listItemFont);

	m_pGroupsList->SetBgColor(Color(86,86,86,255));
	m_statsList->SetBgColor(Color(52,52,52,255));
}

void CBaseStatsPage::SetActiveStatGroup (CBaseStatGroupPanel* groupPanel)
{
	for (int i = 0; i < m_pGroupsList->GetItemCount(); i++)
	{
		CBaseStatGroupPanel *pPanel = (CBaseStatGroupPanel*)m_pGroupsList->GetItemPanel(i);
		if ( pPanel )
		{
			if ( pPanel != groupPanel )
			{
				pPanel->SetGroupActive( false );
			}
			else
			{
				pPanel->SetGroupActive( true );
			}
		}
	}
} 

void CBaseStatsPage::UpdateGroupPanels()
{
	int iGroupCount = m_pGroupsList->GetItemCount();
	vgui::IScheme *pGroupScheme = scheme()->GetIScheme( GetScheme() );

	for ( int i = 0; i < iGroupCount; i++ )
	{
		CBaseStatGroupPanel *pPanel = (CBaseStatGroupPanel*)m_pGroupsList->GetItemPanel(i);
		if ( pPanel )
		{
			pPanel->Update( pGroupScheme );
		}
	}
}



void CBaseStatsPage::OnSizeChanged(int newWide, int newTall)
{
	BaseClass::OnSizeChanged(newWide, newTall);

	if (m_statsList)
	{
		int labelX, labelY, listX, listY, listWide, listTall;
		m_statsList->GetBounds(listX, listY, listWide, listTall);

		if (m_bottomBar)
		{
			m_bottomBar->GetPos(labelX, labelY);
			m_bottomBar->SetPos(labelX, listY + listTall);
		}
	}
}

const wchar_t* CBaseStatsPage::TranslateWeaponKillIDToAlias( int statKillID )
{
	CSWeaponID weaponIDIndex = WEAPON_MAX;
	for ( int i = 0; WeaponName_StatId_Table[i].killStatId != CSSTAT_UNDEFINED; ++i )
	{
		if( WeaponName_StatId_Table[i].killStatId == statKillID )
		{
			weaponIDIndex = WeaponName_StatId_Table[i].weaponId;
			break;
		}
	}

	if (weaponIDIndex == WEAPON_MAX)
	{
		return NULL;
	}
	else
	{
		return WeaponIDToDisplayName(weaponIDIndex);
	}
}

const wchar_t* CBaseStatsPage::LocalizeTagOrUseDefault( const char* tag, const wchar_t* def )
{
	const wchar_t* result = g_pVGuiLocalize->Find( tag );

	if ( !result )
		result = def ? def : L"\0";

	return result;
}

CBaseStatGroupPanel* CBaseStatsPage::AddGroup( const wchar_t* name, const char* title_tag, const wchar_t* def )
{
	CBaseStatGroupPanel* newGroup = new CBaseStatGroupPanel( m_pGroupsList, this, "StatGroupPanel", 0 );
	newGroup->SetGroupInfo( name, LocalizeTagOrUseDefault( title_tag, def ) );
	newGroup->SetGroupActive( false );

	m_pGroupsList->AddItem( NULL, newGroup );

	return newGroup;
}

void CBaseStatsPage::FireGameEvent( IGameEvent * event )
{
	const char *type = event->GetName();

	if ( 0 == Q_strcmp( type, "player_stats_updated" ) )
		m_bStatsDirty = true;
}

void CBaseStatsPage::OnThink()
{
	if ( m_bStatsDirty )
		UpdateStatsData();
}

CBaseStatGroupPanel::CBaseStatGroupPanel( vgui::PanelListPanel *parent, CBaseStatsPage *owner, const char* name, int iListItemID ) : BaseClass( parent, name )
{
	m_pParent = parent;
	m_pOwner = owner;
	m_pSchemeSettings = NULL;

	m_pGroupIcon = SETUP_PANEL(new vgui::ImagePanel( this, "GroupIcon" ));
	m_pBaseStatGroupLabel = new vgui::Label( this, "GroupName", "name" );
	m_pGroupButton = new CBaseStatGroupButton(this, "GroupButton", "" );
	m_pGroupButton->SetPos( 0, 0 );
	m_pGroupButton->SetZPos( 20 );
	m_pGroupButton->SetWide( 256 );
	m_pGroupButton->SetTall( 64 );
	SetMouseInputEnabled( true );
	parent->SetMouseInputEnabled( true );

	m_bActiveButton = false;
}

CBaseStatGroupPanel::~CBaseStatGroupPanel()
{
	delete m_pBaseStatGroupLabel;
	delete m_pGroupIcon;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the parameter pIconPanel to display the specified achievement's icon file.
//-----------------------------------------------------------------------------
bool CBaseStatGroupPanel::LoadIcon( const char* pFilename)
{
	char imagePath[_MAX_PATH];
	Q_strncpy( imagePath, "achievements\\", sizeof(imagePath) );
	Q_strncat( imagePath, pFilename, sizeof(imagePath), COPY_ALL_CHARACTERS );
	Q_strncat( imagePath, ".vtf", sizeof(imagePath), COPY_ALL_CHARACTERS );

	char checkFile[_MAX_PATH];
	Q_snprintf( checkFile, sizeof(checkFile), "materials\\vgui\\%s", imagePath );
	if ( !g_pFullFileSystem->FileExists( checkFile ) )
	{
		Q_snprintf( imagePath, sizeof(imagePath), "hud\\icon_locked.vtf" );
	}

	m_pGroupIcon->SetShouldScaleImage( true );
	m_pGroupIcon->SetImage( imagePath );
	m_pGroupIcon->SetVisible( true );

	return m_pGroupIcon->IsVisible();
}


//-----------------------------------------------------------------------------
// Purpose: Loads settings from hl2/resource/ui/achievementitem.res
//			Sets display info for this achievement item.
//-----------------------------------------------------------------------------
void CBaseStatGroupPanel::ApplySchemeSettings( vgui::IScheme* pScheme )
{
	if ( !g_pPreloadedCSBaseStatGroupLayout )
	{
		PreloadResourceFile();
	}

	LoadControlSettings( "", NULL, g_pPreloadedCSBaseStatGroupLayout );

	m_pSchemeSettings = pScheme;

	BaseClass::ApplySchemeSettings( pScheme );
}

void CBaseStatGroupPanel::Update( vgui::IScheme* pScheme )
{
	if ( m_pSchemeSettings )
	{

		// Set group name text
		m_pBaseStatGroupLabel->SetText( m_pGroupTitle );
		m_pBaseStatGroupLabel->SetFgColor(Color(157, 194, 80, 255));

		if ( !m_bActiveButton )
		{
			LoadIcon( "achievement-btn-up" );
		}
		else
		{
			LoadIcon( "achievement-btn-select" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseStatGroupPanel::PreloadResourceFile( void )
{
	const char *controlResourceName = "resource/ui/StatGroup.res";

	g_pPreloadedCSBaseStatGroupLayout = new KeyValues(controlResourceName);
	g_pPreloadedCSBaseStatGroupLayout->LoadFromFile(g_pFullFileSystem, controlResourceName);
}



//-----------------------------------------------------------------------------
// Purpose: Assigns a name and achievement id bounds for an achievement group.
//-----------------------------------------------------------------------------
void CBaseStatGroupPanel::SetGroupInfo ( const wchar_t* name, const wchar_t* title)
{
	// Store away the group name
	short   _textLen = (short)wcslen(name) + 1;
	m_pGroupName = new wchar_t[_textLen];
	Q_memcpy( m_pGroupName, name, _textLen * sizeof(wchar_t) );

	_textLen = (short)wcslen(title) + 1;
	m_pGroupTitle = new wchar_t[_textLen];
	Q_memcpy( m_pGroupTitle, title, _textLen * sizeof(wchar_t) );
}


CBaseStatGroupButton::CBaseStatGroupButton( vgui::Panel *pParent, const char *pName, const char *pText ) :
BaseClass( pParent, pName, pText )
{
}

//-----------------------------------------------------------------------------
// Purpose: Handle the case where the user presses an achievement group button.
//-----------------------------------------------------------------------------
void CBaseStatGroupButton::DoClick( void )
{
	// Process when a group button is hit
	CBaseStatGroupPanel*    pParent = static_cast<CBaseStatGroupPanel*>(GetParent());

	if (pParent)
	{
		CBaseStatsPage*  pBaseStatsPage = static_cast<CBaseStatsPage*>(pParent->GetOwner());

		if (pBaseStatsPage)
		{
			pBaseStatsPage->SetActiveStatGroup( pParent );
			pBaseStatsPage->UpdateStatsData();
		}
	}
}
