//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef QUICKLISTPANEL
#define QUICKLISTPANEL
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Spectator games list
//-----------------------------------------------------------------------------
class CQuickListPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CQuickListPanel, vgui::EditablePanel );

public:
	CQuickListPanel( vgui::Panel *parent, const char *panelName );

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	void SetMapName( const char *pMapName );
	void SetImage( const char *pMapName );
	void SetGameType( const char *pGameType );
	const char *GetMapName( void ) { return m_szMapName; }
	void	SetRefreshing( void );

	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnMouseDoublePressed( vgui::MouseCode code );
	void	SetServerInfo ( KeyValues *pKV, int iListID, int iTotalServers );
	int		GetListID( void ) { return m_iListID; }


	MESSAGE_FUNC_INT( OnPanelSelected, "PanelSelected", state )
	{
		if ( state )
		{
			vgui::IScheme *pScheme = vgui::scheme()->GetIScheme( GetScheme() );

			if ( pScheme && m_pBGroundPanel )
			{
				m_pBGroundPanel->SetBgColor( pScheme->GetColor("QuickListBGSelected", Color(255, 255, 255, 0 ) ) );
			}
		}
		else
		{
			vgui::IScheme *pScheme = vgui::scheme()->GetIScheme( GetScheme() );

			if ( pScheme && m_pBGroundPanel )
			{
				m_pBGroundPanel->SetBgColor( pScheme->GetColor("QuickListBGDeselected", Color(255, 255, 255, 0 ) ) );
			}
		}

		PostMessage( GetParent()->GetVParent(), new KeyValues("PanelSelected") );
	}

private:

	char m_szMapName[128];

	vgui::ImagePanel *m_pLatencyImage;
	vgui::Label	*m_pLatencyLabel;
	vgui::Label	*m_pPlayerCountLabel;
	vgui::Label	*m_pOtherServersLabel;
	vgui::Label	*m_pServerNameLabel;
	vgui::Panel *m_pBGroundPanel;
	vgui::ImagePanel *m_pMapImage;

	vgui::Panel *m_pListPanelParent;
	vgui::Label *m_pGameTypeLabel;
	vgui::Label *m_pMapNameLabel;

	vgui::ImagePanel *m_pReplayImage;

	int m_iListID;
};


#endif // QUICKLISTPANEL
