//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef CUSTOMGAMES_H
#define CUSTOMGAMES_H
#ifdef _WIN32
#pragma once
#endif

#define MAX_TAG_CHARACTERS			128

class TagInfoLabel : public vgui::URLLabel
{
	DECLARE_CLASS_SIMPLE( TagInfoLabel, vgui::URLLabel );
public:
	TagInfoLabel(Panel *parent, const char *panelName);
	TagInfoLabel(Panel *parent, const char *panelName, const char *text, const char *pszURL);

	virtual void	OnMousePressed(vgui::MouseCode code);

	MESSAGE_FUNC( DoOpenCustomServerInfoURL, "DoOpenCustomServerInfoURL" );
};

class TagMenuButton : public vgui::MenuButton
{
	DECLARE_CLASS_SIMPLE( TagMenuButton, vgui::MenuButton );
public:
	TagMenuButton( Panel *parent, const char *panelName, const char *text);

	virtual void OnShowMenu(vgui::Menu *menu);
};

//-----------------------------------------------------------------------------
// Purpose: Internet games with tags
//-----------------------------------------------------------------------------

class CCustomGames : public CInternetGames
{
	DECLARE_CLASS_SIMPLE( CCustomGames, CInternetGames );
public:
	CCustomGames(vgui::Panel *parent);
	~CCustomGames();

	virtual void	UpdateDerivedLayouts( void ) OVERRIDE;
	virtual void	OnLoadFilter(KeyValues *filter) OVERRIDE;
	virtual void	OnSaveFilter(KeyValues *filter) OVERRIDE;
	bool	CheckTagFilter( gameserveritem_t &server ) OVERRIDE;
	bool	CheckWorkshopFilter( gameserveritem_t &server ) OVERRIDE;
	virtual void	SetRefreshing(bool state) OVERRIDE;
	virtual void	ServerResponded( int iServer, gameserveritem_t *pServerItem ) OVERRIDE;

	MESSAGE_FUNC_PARAMS( OnAddTag, "AddTag", params );
	MESSAGE_FUNC( OnTagMenuButtonOpened, "TagMenuButtonOpened" );

	void			RecalculateCommonTags( void );
	void			AddTagToFilterList( const char *pszTag );

private:
	TagInfoLabel	*m_pTagInfoURL;
	TagMenuButton	*m_pAddTagList;
	vgui::Menu		*m_pTagListMenu;
	vgui::TextEntry	*m_pTagFilter;
	char			m_szTagFilter[MAX_TAG_CHARACTERS];
};


#endif // CUSTOMGAMES_H
