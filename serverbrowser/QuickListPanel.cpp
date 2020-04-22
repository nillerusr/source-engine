//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "pch_serverbrowser.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Purpose: Invisible panel that forwards up mouse movement
//-----------------------------------------------------------------------------
class CMouseMessageForwardingPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CMouseMessageForwardingPanel, vgui::Panel );
public:
	CMouseMessageForwardingPanel( Panel *parent, const char *name );

	virtual void PerformLayout( void );
	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnMouseDoublePressed( vgui::MouseCode code );
	virtual void OnMouseWheeled(int delta);
};

CMouseMessageForwardingPanel::CMouseMessageForwardingPanel( Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// don't draw an
	SetPaintEnabled(false);
	SetPaintBackgroundEnabled(false);
	SetPaintBorderEnabled(false);
}

void CMouseMessageForwardingPanel::PerformLayout()
{
	// fill out the whole area
	int w, t;
	GetParent()->GetSize(w, t);
	SetBounds(0, 0, w, t);
}

void CMouseMessageForwardingPanel::OnMousePressed( vgui::MouseCode code )
{
	if ( GetParent() )
	{
		GetParent()->OnMousePressed( code );
	}
}

void CMouseMessageForwardingPanel::OnMouseDoublePressed( vgui::MouseCode code )
{
	if ( GetParent() )
	{
		GetParent()->OnMouseDoublePressed( code );
	}
}

void CMouseMessageForwardingPanel::OnMouseWheeled(int delta)
{
	if ( GetParent() )
	{
		GetParent()->OnMouseWheeled( delta );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CQuickListPanel::CQuickListPanel( vgui::Panel* pParent, const char *pElementName ) : BaseClass( pParent, pElementName )
{
	SetParent( pParent );

	m_pListPanelParent = pParent;

	CMouseMessageForwardingPanel *panel = new CMouseMessageForwardingPanel(this, NULL);
	panel->SetZPos(3);

	m_pLatencyImage = new ImagePanel( this, "latencyimage" );
	m_pPlayerCountLabel = new Label( this, "playercount", "" );
	m_pOtherServersLabel = new Label( this, "otherservercount", "" );
	m_pServerNameLabel = new Label( this, "servername", "" );
	m_pBGroundPanel = new Panel( this, "background" );
	m_pMapImage = new ImagePanel( this, "mapimage" );
	m_pGameTypeLabel = new Label( this, "gametype", "" );
	m_pMapNameLabel = new Label( this, "mapname", "" );
	m_pLatencyLabel = new Label( this, "latencytext", "" );
	m_pReplayImage = new ImagePanel( this, "replayimage" );

	const char *pPathID = "PLATFORM";

	if ( g_pFullFileSystem->FileExists( "servers/QuickListPanel.res", "MOD" ) )
	{
		pPathID = "MOD";
	}
	
	LoadControlSettings( "servers/QuickListPanel.res", pPathID );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuickListPanel::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	
	if ( pScheme && m_pBGroundPanel )
	{
		m_pBGroundPanel->SetBgColor( pScheme->GetColor("QuickListBGDeselected", Color(255, 255, 255, 0 ) ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuickListPanel::SetRefreshing( void )
{
	if ( m_pServerNameLabel )
	{
		m_pServerNameLabel->SetText( g_pVGuiLocalize->Find("#ServerBrowser_QuickListRefreshing") );
	}

	if ( m_pPlayerCountLabel )
	{
		m_pPlayerCountLabel->SetVisible( false );
	}
	if ( m_pOtherServersLabel )
	{
		m_pOtherServersLabel->SetVisible( false );
	}

	if ( m_pLatencyImage )
	{
		m_pLatencyImage->SetVisible( false );
	}

	if ( m_pReplayImage )
	{
		m_pReplayImage->SetVisible( false );
	}

	if ( m_pLatencyLabel )
	{
		m_pLatencyLabel->SetVisible( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuickListPanel::SetMapName( const char *pMapName )
{
	Q_strncpy( m_szMapName, pMapName, sizeof( m_szMapName ) );

	if ( m_pMapNameLabel )
	{
		m_pMapNameLabel->SetText( pMapName );
		m_pMapNameLabel->SizeToContents();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuickListPanel::SetGameType( const char *pGameType )
{
	if ( strlen ( pGameType ) == 0 )
	{
		m_pGameTypeLabel->SetVisible( false );
		return;
	}

	char gametype[ 512 ];
	Q_snprintf( gametype, sizeof( gametype ), "(%s)", pGameType );

	m_pGameTypeLabel->SetText( gametype );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuickListPanel::SetServerInfo ( KeyValues *pKV, int iListID, int iTotalServers )
{
	if ( pKV == NULL )
		return;

	m_iListID = iListID;

	m_pServerNameLabel->SetText( pKV->GetString( "name", " " ) );

	int iPing = pKV->GetInt( "ping", 0 );

	if ( iPing <= 100 )
	{
		m_pLatencyImage->SetImage( "../vgui/icon_con_high.vmt" );
	}
	else if ( iPing <= 150 )
	{
		m_pLatencyImage->SetImage( "../vgui/icon_con_medium.vmt" );
	}
	else
	{
		m_pLatencyImage->SetImage( "../vgui/icon_con_low.vmt" );
	}

	m_pLatencyImage->SetVisible( false );

	if ( GameSupportsReplay() )
	{
		if ( pKV->GetInt( "Replay", 0 ) > 0 )
		{
			m_pReplayImage->SetVisible( true );
		}
	}

	char ping[ 512 ];
	Q_snprintf( ping, sizeof( ping ), "%d ms", iPing );

	m_pLatencyLabel->SetText( ping );
	m_pLatencyLabel->SetVisible( true );

	wchar_t players[ 512 ];
	wchar_t playercount[16];
	wchar_t *pwszPlayers = g_pVGuiLocalize->Find("#ServerBrowser_Players");

	g_pVGuiLocalize->ConvertANSIToUnicode( pKV->GetString( "players", " " ), playercount,  sizeof( playercount ) );

	_snwprintf( players, ARRAYSIZE( players ), L"%ls %ls",  playercount, pwszPlayers );
	
	m_pPlayerCountLabel->SetText( players );
	m_pPlayerCountLabel->SetVisible( true );


	// Now setup the other server count
	if ( iTotalServers == 2 )
	{
		m_pOtherServersLabel->SetText( g_pVGuiLocalize->Find("#ServerBrowser_QuickListOtherServer") );
		m_pOtherServersLabel->SetVisible( true );
	}
	else if ( iTotalServers > 2 )
	{
		wchar_t *pwszServers = g_pVGuiLocalize->Find("#ServerBrowser_QuickListOtherServers");
		_snwprintf( playercount, Q_ARRAYSIZE(playercount), L"%d", (iTotalServers-1) );
		g_pVGuiLocalize->ConstructString( players, sizeof( players ), pwszServers, 1, playercount );
		m_pOtherServersLabel->SetText( players );
		m_pOtherServersLabel->SetVisible( true );
	}
	else
	{
		m_pOtherServersLabel->SetVisible( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CQuickListPanel::SetImage( const char *pMapName )
{
	char path[ 512 ];
	Q_snprintf( path, sizeof( path ), "materials/vgui/maps/menu_thumb_%s.vmt", pMapName );

	char map[ 512 ];
	Q_snprintf( map, sizeof( map ), "maps/%s.bsp", pMapName );

	if ( g_pFullFileSystem->FileExists( map, "MOD" ) == false  )
	{
		pMapName = "default_download";
	}
	else
	{
		if ( g_pFullFileSystem->FileExists( path, "MOD" ) == false  )
		{
			pMapName = "default";
		}
	}

	if ( m_pMapImage )
	{
		char imagename[ 512 ];
		Q_snprintf( imagename, sizeof( imagename ), "..\\vgui\\maps\\menu_thumb_%s", pMapName );

		m_pMapImage->SetImage ( imagename );
		m_pMapImage->SetMouseInputEnabled( false );
	}							
}

void CQuickListPanel::OnMousePressed( vgui::MouseCode code )
{
	if ( m_pListPanelParent )
	{
		vgui::PanelListPanel *pParent = dynamic_cast < vgui::PanelListPanel *> ( m_pListPanelParent );

		if ( pParent )
		{
			pParent->SetSelectedPanel( this );
			m_pListPanelParent->CallParentFunction( new KeyValues("ItemSelected", "itemID", -1 ) );
		}

		if ( code == MOUSE_RIGHT )
		{
			m_pListPanelParent->CallParentFunction( new KeyValues("OpenContextMenu", "itemID", -1 ) );
		}

	}
}

void CQuickListPanel::OnMouseDoublePressed( vgui::MouseCode code )
{
	if ( code == MOUSE_RIGHT )
		return;

	// call the panel
	OnMousePressed( code );

	m_pListPanelParent->CallParentFunction( new KeyValues("ConnectToServer", "code", code) );
}
