//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ITEM_AD_PANEL_H
#define ITEM_AD_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include "vgui_controls/EditablePanel.h"

using namespace vgui;
class CExButton;

class CBaseAdPanel : public EditablePanel
{
	DECLARE_CLASS_SIMPLE( CBaseAdPanel, EditablePanel );

public:
	CBaseAdPanel( Panel *parent, const char *panelName );
	virtual ~CBaseAdPanel() {}

	virtual void ApplySettings( KeyValues *inResourceData ) OVERRIDE;

	float GetPresentTime() const { return m_flPresentTime; }

	static bool CheckForRequiredSteamComponents( const char* pszSteamRequried, const char* pszOverlayRequired );

private:
	
	float m_flPresentTime;
};

class CItemAdPanel : public CBaseAdPanel
{
	DECLARE_CLASS_SIMPLE( CItemAdPanel, CBaseAdPanel );
public:
	CItemAdPanel( Panel *parent, const char *panelName, item_definition_index_t itemDefIndex );
	virtual ~CItemAdPanel() {}

	virtual void ApplySchemeSettings( IScheme *pScheme ) OVERRIDE;
	virtual void ApplySettings( KeyValues *inResourceData ) OVERRIDE;
	virtual void PerformLayout() OVERRIDE;
	virtual void OnTick() OVERRIDE;
	virtual void OnCommand( const char *command ) OVERRIDE;

private:

	const CTFItemDefinition* GetItemDef() const;
	bool m_bShowMarketButton;

	item_definition_index_t m_ItemDefIndex;
};

class CCyclingAdContainerPanel : public EditablePanel
{
	DECLARE_CLASS_SIMPLE( CCyclingAdContainerPanel, EditablePanel );
public:
	CCyclingAdContainerPanel( Panel *parent, const char *panelName );
	virtual ~CCyclingAdContainerPanel();

	virtual void ApplySchemeSettings( IScheme *pScheme ) OVERRIDE;
	virtual void ApplySettings( KeyValues *inResourceData ) OVERRIDE;
	virtual void PerformLayout() OVERRIDE;
	virtual void OnCommand( const char *command ) OVERRIDE;
	virtual void OnThink() OVERRIDE;

	void SetItemKVs( KeyValues* pKVItems );

private:

	void CreatePanels();
	void PresentIndex( int nIndex );
	void UpdateAdPanelPositions();
	float GetTransitionProgress() const;
	bool IsTransitioningOut() const;

	EditablePanel* m_pAdsContainer;
	EditablePanel* m_pFadePanel;
	CExButton* m_pPrevButton;
	CExButton* m_pNextButton;
	bool m_bNeedsToCreatePanels;
	bool m_bSettingsApplied;

	struct AdData_t
	{
		~AdData_t()
		{
			delete m_pAdPanel;
		}
		CBaseAdPanel* m_pAdPanel;
		KeyValues* m_pSettingsKVs;
	};

	KeyValues *m_pKVItems;

	CUtlVector< AdData_t > m_vecPossibleAds;
	int m_nTargetIndex;
	int m_nCurrentIndex;
	int m_nTransitionStartOffsetX;
	bool m_bTransitionRight;
	int m_nXPos;
	
	RealTimeCountdownTimer m_TransitionTimer;
	RealTimeCountdownTimer m_ShowTimer;
};

#endif // ITEM_AD_PANEL_H
