//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CSBASESTATSPAGE_H
#define CSBASESTATSPAGE_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/PanelListPanel.h"
#include "vgui_controls/Label.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/PropertyPage.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"
#include "GameEventListener.h"

struct PlayerStatData_t;
class IScheme;
class CBaseStatGroupPanel;
class StatCard;
struct StatsCollection_t;
struct RoundStatsDirectAverage_t;

class CBaseStatsPage : public vgui::PropertyPage, public CGameEventListener
{
    DECLARE_CLASS_SIMPLE ( CBaseStatsPage, vgui::PropertyPage );

public:
    CBaseStatsPage( vgui::Panel *parent, const char *name );
	
	~CBaseStatsPage();
	
    virtual void ApplySchemeSettings( vgui::IScheme *pScheme );    
    virtual void MoveToFront();
	virtual void OnSizeChanged(int wide, int tall);
	virtual void OnThink();

	void UpdateStatsData();
	void SetActiveStatGroup (CBaseStatGroupPanel* groupPanel);

	virtual void FireGameEvent( IGameEvent * event );

protected:

	void UpdateGroupPanels();
	CBaseStatGroupPanel* AddGroup( const wchar_t* name, const char* title_tag, const wchar_t* def = NULL );
	const wchar_t* TranslateWeaponKillIDToAlias( int statKillID );
	const wchar_t* LocalizeTagOrUseDefault( const char* tag, const wchar_t* def = NULL );
	
	virtual void RepopulateStats() = 0;	

	vgui::SectionedListPanel	*m_statsList;
	vgui::HFont					m_listItemFont;

private:

	vgui::PanelListPanel		*m_pGroupsList;
	vgui::ImagePanel* m_bottomBar;
	StatCard*	m_pStatCard;
	bool		m_bStatsDirty;
};




class CBaseStatGroupButton : public vgui::Button
{
	DECLARE_CLASS_SIMPLE( CBaseStatGroupButton, vgui::Button );

public:

	CBaseStatGroupButton(  vgui::Panel *pParent, const char *pName, const char *pText );

	virtual void DoClick( void );
};





class CBaseStatGroupPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CBaseStatGroupPanel, vgui::EditablePanel );

public:
	CBaseStatGroupPanel( vgui::PanelListPanel *parent, CBaseStatsPage *owner, const char* name, int iListItemID );
	~CBaseStatGroupPanel();

	void SetGroupInfo ( const wchar_t* name, const wchar_t* title);
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	void Update( vgui::IScheme* pScheme );

	vgui::PanelListPanel* GetParent() { return m_pParent; }
	CBaseStatsPage* GetOwner() { return m_pOwner; }

	void SetGroupActive(bool active) { m_bActiveButton = active; }
	bool IsGroupActive() { return m_bActiveButton; }

protected:

	// Loads an icon into a specified image panel, or turns the panel off if no icon was found.
	bool LoadIcon( const char* pFilename);

private:
	void PreloadResourceFile( void );

	vgui::PanelListPanel    *m_pParent;
	CBaseStatsPage       *m_pOwner;

	vgui::Label             *m_pBaseStatGroupLabel;	

	CBaseStatGroupButton            *m_pGroupButton;

	vgui::ImagePanel        *m_pGroupIcon;

	vgui::IScheme           *m_pSchemeSettings;

	bool                    m_bActiveButton;

	wchar_t                 *m_pGroupName;
	wchar_t                 *m_pGroupTitle;
};





#endif // CSBASESTATSPAGE_H
