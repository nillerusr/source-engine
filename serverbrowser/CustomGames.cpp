//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "pch_serverbrowser.h"
#include <vgui_controls/HTML.h>
#include <vgui_controls/MessageDialog.h>

using namespace vgui;

#define NUM_COMMON_TAGS			20

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
TagMenuButton::TagMenuButton(Panel *parent, const char *panelName, const char *text) : BaseClass(parent,panelName,text)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TagMenuButton::OnShowMenu( vgui::Menu *menu )
{
	PostActionSignal(new KeyValues("TagMenuButtonOpened"));
	BaseClass::OnShowMenu(menu);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCustomServerInfoURLQuery : public vgui::QueryBox
{
	DECLARE_CLASS_SIMPLE( CCustomServerInfoURLQuery, vgui::QueryBox );
public:
	CCustomServerInfoURLQuery(const char *title, const char *queryText,vgui::Panel *parent) : BaseClass( title, queryText, parent )
	{
		SetOKButtonText( "#ServerBrowser_CustomServerURLButton" );
	}
};

DECLARE_BUILD_FACTORY( TagInfoLabel );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
TagInfoLabel::TagInfoLabel(Panel *parent, const char *panelName) : BaseClass(parent,panelName, (const char *)NULL, NULL)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
TagInfoLabel::TagInfoLabel(Panel *parent, const char *panelName, const char *text, const char *pszURL) : BaseClass(parent,panelName,text,pszURL)
{
}

//-----------------------------------------------------------------------------
// Purpose: If we were left clicked on, launch the URL
//-----------------------------------------------------------------------------
void TagInfoLabel::OnMousePressed(MouseCode code)
{
	if (code == MOUSE_LEFT)
	{
		if ( GetURL() )
		{
			// Pop up the dialog with the url in it
			CCustomServerInfoURLQuery *qb = new CCustomServerInfoURLQuery( "#ServerBrowser_CustomServerURLWarning", "#ServerBrowser_CustomServerURLOpen", this );
			if (qb != NULL)
			{
				qb->SetOKCommand( new KeyValues("DoOpenCustomServerInfoURL") );
				qb->AddActionSignalTarget(this);
				qb->MoveToFront();
				qb->DoModal();
			}
		}
	} 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TagInfoLabel::DoOpenCustomServerInfoURL( void )
{
	if ( GetURL() )
	{
		system()->ShellExecute("open", GetURL() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCustomGames::CCustomGames(vgui::Panel *parent) : 
	BaseClass(parent, "CustomGames", eInternetServer )
{
	m_pGameList->AddColumnHeader(10, "Tags", "#ServerBrowser_Tags", 200);
	m_pGameList->SetSortFunc(10, TagsCompare);

	if ( !IsSteamGameServerBrowsingEnabled() )
	{
		m_pGameList->SetEmptyListText("#ServerBrowser_OfflineMode");
		m_pConnect->SetEnabled( false );
		m_pRefreshAll->SetEnabled( false );
		m_pRefreshQuick->SetEnabled( false );
		m_pAddServer->SetEnabled( false );
		m_pFilter->SetEnabled( false );
	}

	m_szTagFilter[0] = 0;

	m_pTagFilter = new TextEntry(this, "TagFilter");
	m_pTagFilter->SetEnabled( false );
	m_pTagFilter->SetMaximumCharCount( MAX_TAG_CHARACTERS );

	m_pAddTagList = new TagMenuButton( this, "AddTagList", "#ServerBrowser_AddCommonTags" );
	m_pTagListMenu = new Menu( m_pAddTagList, "TagList" );
	m_pAddTagList->SetMenu( m_pTagListMenu );
	m_pAddTagList->SetOpenDirection( Menu::UP );
	m_pAddTagList->SetEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCustomGames::~CCustomGames()
{
}		

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCustomGames::UpdateDerivedLayouts( void )
{
	const char *pPathID = "PLATFORM";

	KeyValues *pConditions = NULL;
	if ( ServerBrowser().IsWorkshopEnabled() )
	{
		pConditions = new KeyValues( "conditions" );
		if ( pConditions )
		{
			KeyValues *pNewKey = new KeyValues( "if_workshop_enabled" );
			if ( pNewKey )
			{
				pConditions->AddSubKey( pNewKey );
			}
		}
	}

	if ( m_pFilter->IsSelected() )
	{
		if ( g_pFullFileSystem->FileExists( "servers/CustomGamesPage_Filters.res", "MOD" ) )
		{
			pPathID = "MOD";
		}

		LoadControlSettings( "servers/CustomGamesPage_Filters.res", pPathID, NULL, pConditions );
	}
	else
	{
		if ( g_pFullFileSystem->FileExists( "servers/CustomGamesPage.res", "MOD" ) )
		{
			pPathID = "MOD";
		}

		LoadControlSettings( "servers/CustomGamesPage.res", pPathID, NULL, pConditions );
	}

	if ( pConditions )
	{
		pConditions->deleteThis();
	}

	if ( !GameSupportsReplay() )
	{
		HideReplayFilter();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCustomGames::OnLoadFilter(KeyValues *filter)
{
	BaseClass::OnLoadFilter( filter );

	Q_strncpy(m_szTagFilter, filter->GetString("gametype"), sizeof(m_szTagFilter));

	if ( m_pTagFilter )
	{
		m_pTagFilter->SetText(m_szTagFilter);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCustomGames::CheckTagFilter( gameserveritem_t &server )
{
	bool bRetVal = true;

	// Custom games substring matches tags with the server's tags
	int count = Q_strlen( m_szTagFilter );
	if ( count )
	{
		CUtlVector<char*> TagList;
		V_SplitString( m_szTagFilter, ",", TagList );
		for ( int i = 0; i < TagList.Count(); i++ )
		{
			if ( ( Q_strnistr( server.m_szGameTags, TagList[i], MAX_TAG_CHARACTERS ) > 0 ) == TagsExclude() )
			{
				bRetVal = false;
				break;
			}
		}

		TagList.PurgeAndDeleteElements();
	}

	return bRetVal;
}

//-----------------------------------------------------------------------------
// Purpose: Checks the workshop filtering setting, taking into account workshop filtering might be disabled
//-----------------------------------------------------------------------------
bool CCustomGames::CheckWorkshopFilter( gameserveritem_t &server )
{
	eWorkshopMode workshopMode = WorkshopMode();
	const char szWorkshopPrefix[] = "workshop/";
	if ( workshopMode == eWorkshop_WorkshopOnly )
	{
		return V_strncasecmp( server.m_szMap, szWorkshopPrefix, sizeof( szWorkshopPrefix ) - 1 ) == 0;
	}
	else if ( workshopMode == eWorkshop_SubscribedOnly )
	{
		return ServerBrowser().IsWorkshopSubscribedMap( server.m_szMap );
	}

	Assert( workshopMode == eWorkshop_None );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: gets filter settings from controls
//-----------------------------------------------------------------------------
void CCustomGames::OnSaveFilter(KeyValues *filter)
{
	BaseClass::OnSaveFilter( filter );

	if ( m_pTagFilter )
	{
		// tags
		m_pTagFilter->GetText(m_szTagFilter, sizeof(m_szTagFilter) - 1);
	}

	if ( m_szTagFilter[0] )
	{
		Q_strlower(m_szTagFilter);
	}

	if ( TagsExclude() )
	{
		m_vecServerFilters.AddToTail( MatchMakingKeyValuePair_t( "gametype", "" ) );
	}
	else
	{
		m_vecServerFilters.AddToTail( MatchMakingKeyValuePair_t( "gametype", m_szTagFilter ) );
	}

	filter->SetString("gametype", m_szTagFilter);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCustomGames::SetRefreshing(bool state)
{
	if ( state )
	{
		m_pAddTagList->SetEnabled( false );
	}

	BaseClass::SetRefreshing( state );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCustomGames::ServerResponded( int iServer, gameserveritem_t *pServerItem )
{
	CBaseGamesPage::ServerResponded( iServer, pServerItem );

	// If we've found a server with some tags, enable the add tag button
	if ( pServerItem->m_szGameTags[0] )
	{
		m_pAddTagList->SetEnabled( true );
	}
}

struct tagentry_t
{
	const char *pszTag;
	int iCount;
};
int __cdecl SortTagsInUse( const tagentry_t *pTag1, const tagentry_t *pTag2 )
{
	return (pTag1->iCount < pTag2->iCount);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCustomGames::RecalculateCommonTags( void )
{
	// Regenerate our tag list
	m_pTagListMenu->DeleteAllItems();

	// Loop through our servers, and build a list of all the tags
	CUtlVector<tagentry_t> aTagsInUse;

	int iCount = m_pGameList->GetItemCount();
	for ( int i = 0; i < iCount; i++ )
	{
		int serverID = m_pGameList->GetItemUserData( i );
		gameserveritem_t *pServer = GetServer( serverID ); 
		if ( pServer && pServer->m_szGameTags && pServer->m_szGameTags[0] )
		{
			CUtlVector<char*> TagList;
			V_SplitString( pServer->m_szGameTags, ",", TagList );

			for ( int iTag = 0; iTag < TagList.Count(); iTag++ )
			{
				// First make sure it's not already in our list
				bool bFound = false;
				for ( int iCheck = 0; iCheck < aTagsInUse.Count(); iCheck++ )
				{
					if ( !Q_strnicmp(TagList[iTag], aTagsInUse[iCheck].pszTag, MAX_TAG_CHARACTERS ) )
					{
						aTagsInUse[iCheck].iCount++;
						bFound = true;
					}
				}

				if ( !bFound )
				{
					int iIdx = aTagsInUse.AddToTail();
					aTagsInUse[iIdx].pszTag = TagList[iTag];
					aTagsInUse[iIdx].iCount = 0;
				}
			}
		}
	}

	aTagsInUse.Sort( SortTagsInUse );

	int iTagsToAdd = min( aTagsInUse.Count(), NUM_COMMON_TAGS );
	for ( int i = 0; i < iTagsToAdd; i++ )
	{
		const char *pszTag = aTagsInUse[i].pszTag;
		m_pTagListMenu->AddMenuItem( pszTag, new KeyValues("AddTag", "tag", pszTag), this, new KeyValues( "data", "tag", pszTag ) );
	}

	m_pTagListMenu->SetFixedWidth( m_pAddTagList->GetWide() );
	m_pTagListMenu->InvalidateLayout( true, false );
	m_pTagListMenu->PositionRelativeToPanel( m_pAddTagList, Menu::UP );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCustomGames::OnTagMenuButtonOpened( void )
{
	RecalculateCommonTags();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the text from the message
//-----------------------------------------------------------------------------
void CCustomGames::OnAddTag(KeyValues *params)
{
	KeyValues *pkvText = params->FindKey("tag", false);
	if (!pkvText)
		return;

	AddTagToFilterList( pkvText->GetString() );
}


int SortServerTags( char* const *p1, char* const *p2 )
{
	return ( Q_strcmp( *p1, *p2 ) > 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCustomGames::AddTagToFilterList( const char *pszTag )
{
	char txt[ 128 ];
	m_pTagFilter->GetText( txt, sizeof( txt ) );

	CUtlVector<char*> TagList;
	V_SplitString( txt, ",", TagList );

	if ( txt[0] )
	{
		for ( int i = 0; i < TagList.Count(); i++ )
		{
			// Already in the tag list?
			if ( !Q_stricmp( TagList[i], pszTag ) )
			{
				TagList.PurgeAndDeleteElements();
				return;
			}
		}
	}

	char *pszNewTag = new char[64];
	Q_strncpy( pszNewTag, pszTag, 64 );
	TagList.AddToHead( pszNewTag );

	TagList.Sort( SortServerTags );

	// Append it
	char tmptags[MAX_TAG_CHARACTERS];
	tmptags[0] = '\0';

	for ( int i = 0; i < TagList.Count(); i++ )
	{
		if ( i > 0 )
		{
			Q_strncat( tmptags, ",", MAX_TAG_CHARACTERS );
		}

		Q_strncat( tmptags, TagList[i], MAX_TAG_CHARACTERS );
	}

	m_pTagFilter->SetText( tmptags );
	TagList.PurgeAndDeleteElements();

	// Update & apply filters now that the tag list has changed
	UpdateFilterSettings();
	ApplyGameFilters();
}
