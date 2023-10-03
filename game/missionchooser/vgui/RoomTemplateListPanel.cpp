#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include "vgui_controls/Controls.h"
#include <vgui/IScheme.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Button.h>
#include <KeyValues.h>

#include "TileGenDialog.h"
#include "RoomTemplateListPanel.h"
#include "RoomTemplatePanel.h"
#include "TileSource/RoomTemplate.h"
#include "TileSource/LevelTheme.h"
#include "vgui/missionchooser_tgaimagepanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

CRoomTemplateListPanel::CRoomTemplateListPanel( Panel *parent, const char *name ) : 
BaseClass( parent, name ),
m_RoomTemplateFolders( 0, 20 )
{	
	KeyValues *pMessageKV;

	m_FilterText[0] = '\0';

	m_pRefreshList = new Button( this, "RefreshListButton", "Refresh List", this, "RefreshList" );
	pMessageKV = new KeyValues( "RefreshList" );
	m_pRefreshList->SetCommand( pMessageKV );
	
	m_pExpandAll = new Button( this, "ExpandAllButton", "Expand All", this, "ExpandAllFolders" );
	pMessageKV = new KeyValues( "ExpandAllFolders" );
	m_pExpandAll->SetCommand( pMessageKV );
	
	m_pCollapseAll = new Button( this, "CollapseAllButton", "Collapse All", this, "CollapseAllFolders" );
	pMessageKV = new KeyValues( "CollapseAllFolders" );
	m_pCollapseAll->SetCommand( pMessageKV );
	
}

CRoomTemplateListPanel::~CRoomTemplateListPanel( void )
{
}

void CRoomTemplateListPanel::PerformLayout()
{		
	// Set width to match parent panel
	SetWide( GetParent()->GetWide() );
	int padding = 8;
	int cur_x = padding;
	int cur_y = padding;
	int nPanelWidth = GetWide();	
	int tallest_on_this_row = 0;

	// Place the refresh and expand/collapse buttons
	int nButtonWidth = nPanelWidth - 2 * padding;
	m_pRefreshList->SetBounds( padding, cur_y, nButtonWidth, 24 );
	cur_y += 24 + padding;
	nButtonWidth = ( nPanelWidth - 3 * padding ) / 2;
	nButtonWidth = MAX( nButtonWidth, 40 );
	m_pExpandAll->SetBounds( padding, cur_y, nButtonWidth, 24 );
	m_pCollapseAll->SetBounds( 2 * padding + nButtonWidth, cur_y, nButtonWidth, 24 );
	cur_y += 24 + padding;

	// Place buttons and images for each folder of room templates
	for ( int i = 0; i < m_RoomTemplateFolders.Count(); ++ i )
	{
		m_RoomTemplateFolders[i].SetButtonText();
		m_RoomTemplateFolders[i].m_pFolderButton->SetBounds(padding, cur_y, GetWide() - padding * 2, 24);
		cur_y += 24;		

		// Add every room template in this category
		for ( int j = 0; j < m_Thumbnails.Count(); ++ j )
		{
			if ( Q_stricmp( m_Thumbnails[j]->m_pRoomTemplate->GetFolderName(), m_RoomTemplateFolders[i].m_FolderName ) != 0 )
				continue;

			if ( m_RoomTemplateFolders[i].m_bExpanded && FilterTemplate( m_Thumbnails[j]->m_pRoomTemplate ) )
			{
				m_Thumbnails[j]->SetVisible( true );

				// make sure the template has sized itself
				m_Thumbnails[j]->InvalidateLayout( true );
				int tw = m_Thumbnails[j]->GetWide();
				int tt = m_Thumbnails[j]->GetTall();

				// if the panel is too wide to fit, then move down a row
				if (cur_x + tw > nPanelWidth)
				{
					cur_x = padding;
					cur_y += tallest_on_this_row + padding;
					tallest_on_this_row = 0;
				}

				m_Thumbnails[j]->SetPos(cur_x, cur_y);
				if (tt > tallest_on_this_row)
					tallest_on_this_row = tt;

				cur_x += tw + padding;
			}
			else
			{
				m_Thumbnails[j]->SetVisible( false );
			}
		}
		
		cur_y += tallest_on_this_row + padding;
		tallest_on_this_row = 0;
		cur_x = padding;
	}
	
	SetTall( cur_y );
}

void CRoomTemplateListPanel::UpdatePanelsWithTemplate( const CRoomTemplate* pTemplate )
{
	int thumbs = m_Thumbnails.Count();
	for (int i=0;i<thumbs;i++)
	{
		CRoomTemplatePanel* pPanel = m_Thumbnails[i];
		if (!pPanel)
			continue;

		if (pPanel->m_pRoomTemplate == pTemplate)
			pPanel->UpdateImages();
	}
}

void CRoomTemplateListPanel::UpdateRoomList()
{
	// clear out the list
	for ( int i = 0; i < m_Thumbnails.Count(); ++ i )
	{
		m_Thumbnails[i]->MarkForDeletion();
	}
	m_Thumbnails.RemoveAll();

	// Try to re-use folder buttons and preserve their state when possible.
	// Mark all folder buttons invisible to begin with.  If they are needed, they will be
	// made visible again; otherwise, we can cull them.
	for ( int i = 0; i < m_RoomTemplateFolders.Count(); ++ i )
	{
		m_RoomTemplateFolders[i].m_pFolderButton->SetVisible( false );
	}

	// Add every room template for the currently selected theme
	CLevelTheme *pCurrentTheme = CLevelTheme::s_pCurrentTheme;
	if ( pCurrentTheme == NULL )
		return;

	for (int i = 0; i < pCurrentTheme->m_RoomTemplates.Count(); ++ i )
	{
		CRoomTemplatePanel *pPanel = new CRoomTemplatePanel( this, "RoomTemplatePanel" );
		pPanel->m_bRoomTemplateBrowserMode = true;
		pPanel->SetRoomTemplate( pCurrentTheme->m_RoomTemplates[i] );
		m_Thumbnails.AddToTail( pPanel );

		AddFolder( pCurrentTheme->m_RoomTemplates[i]->GetFolderName() );
	}

	// Remove unused folder buttons
	for ( int i = 0; i < m_RoomTemplateFolders.Count(); ++ i )
	{
		if ( !m_RoomTemplateFolders[i].m_pFolderButton->IsVisible() )
		{
			m_RoomTemplateFolders[i].m_pFolderButton->MarkForDeletion();
			m_RoomTemplateFolders.FastRemove( i );
			// go back 1 since we just deleted an entry
			-- i;
		}
	}

	// Sort alphabetically
	m_RoomTemplateFolders.Sort( CompareFolders );

	InvalidateLayout();
}

void CRoomTemplateListPanel::OnRefreshList()
{
	CLevelTheme *pCurrentTheme = CLevelTheme::s_pCurrentTheme;
	if ( pCurrentTheme == NULL )
		return;

	CMissionChooserTGAImagePanel::ClearImageCache();
	pCurrentTheme->LoadRoomTemplates();
	UpdateRoomList();
}

void CRoomTemplateListPanel::OnExpandAll()
{
	for ( int i = 0; i < m_RoomTemplateFolders.Count(); ++ i )
	{
		m_RoomTemplateFolders[i].m_bExpanded = true;
	}
	InvalidateLayout();
}

void CRoomTemplateListPanel::OnCollapseAll()
{
	for ( int i = 0; i < m_RoomTemplateFolders.Count(); ++ i )
	{
		m_RoomTemplateFolders[i].m_bExpanded = false;
	}
	InvalidateLayout();
}

void CRoomTemplateListPanel::OnToggleFolder( KeyValues *pKV )
{
	const char *pFolderName = pKV->GetString( "folder", NULL );
	Assert( pFolderName != NULL);
	for ( int i = 0; i < m_RoomTemplateFolders.Count(); ++ i )
	{
		if ( Q_stricmp( pFolderName, m_RoomTemplateFolders[i].m_FolderName ) == 0 )
		{
			m_RoomTemplateFolders[i].m_bExpanded = !m_RoomTemplateFolders[i].m_bExpanded;
			InvalidateLayout();
			return;
		}
	}
}

void CRoomTemplateListPanel::SetFilterText( const char *pText )
{
	Q_strncpy( m_FilterText, pText, m_nFilterTextLength );
}

void CRoomTemplateListPanel::AddFolder( const char *pFolderName )
{
	int nNumFolders = m_RoomTemplateFolders.Count();
	for ( int i = 0; i < nNumFolders; ++ i )
	{
		if ( Q_stricmp( pFolderName, m_RoomTemplateFolders[i].m_FolderName ) == 0 )
		{
			// Mark this folder as being used
			m_RoomTemplateFolders[i].m_pFolderButton->SetVisible( true );
			return;
		}
	}

	// New folder, create a new entry
	m_RoomTemplateFolders.AddToTail();
	Q_strncpy( m_RoomTemplateFolders[nNumFolders].m_FolderName, pFolderName, MAX_PATH );
	m_RoomTemplateFolders[nNumFolders].m_bExpanded = true;
	m_RoomTemplateFolders[nNumFolders].m_pFolderButton = new Button( this, "FolderButton", "", this, "ToggleFolder" );
	
	KeyValues *pMessageKV = new KeyValues( "ToggleFolder" );
	KeyValues *pIndexKV = new KeyValues( "folder" );
	pIndexKV->SetString( NULL, m_RoomTemplateFolders[nNumFolders].m_FolderName );
	pMessageKV->AddSubKey( pIndexKV );
	m_RoomTemplateFolders[nNumFolders].m_pFolderButton->SetCommand( pMessageKV );
}

bool CRoomTemplateListPanel::FilterTemplate( const CRoomTemplate *pTemplate )
{
	bool bInvert = false;
	const char *pFilterText = m_FilterText;

	// If the user types in a "!" as the first character, invert the search results.
	if ( pFilterText[0] == '!' )
	{
		bInvert = true;
		++ pFilterText;
	}

	if ( pFilterText[0] == '\0' )
	{
		return true;
	}
	else
	{
		if ( Q_stristr( pTemplate->GetDescription(), pFilterText ) != NULL )
		{
			return !bInvert;
		}
		if ( Q_stristr( pTemplate->GetFullName(), pFilterText ) != NULL )
		{
			return !bInvert;
		}
		int nNumTags = pTemplate->GetNumTags();
		for ( int i = 0; i < nNumTags; ++ i )
		{
			if ( Q_stristr( pTemplate->GetTag( i ), pFilterText ) != NULL )
			{
				return !bInvert;
			}
		}
	}
	return bInvert;
}

void CRoomTemplateListPanel::RoomTemplateFolder_t::SetButtonText()
{
	char buttonLabel[MAX_PATH + 5];
	Q_snprintf( buttonLabel, MAX_PATH + 5, "%s %s", m_bExpanded ? "-" : "+", m_FolderName );
	m_pFolderButton->SetText( buttonLabel );
}