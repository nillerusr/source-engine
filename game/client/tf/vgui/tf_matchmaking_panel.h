//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MATCH_MAKING_PANEL_H
#define MATCH_MAKING_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/EditablePanel.h"
#include <game/client/iviewport.h>
#include "quest_log_panel.h"

using namespace vgui;

class CBaseLobbyPanel;
class CMatchMakingPanel *GetMatchMakingPanel();

//-----------------------------------------------------------------------------
// The default quest log panel
//-----------------------------------------------------------------------------
class CMatchMakingPanel : public EditablePanel, public IViewPortPanel, public CGameEventListener
{
	DECLARE_CLASS_SIMPLE( CMatchMakingPanel, EditablePanel );
public:
	CMatchMakingPanel( IViewPort *pViewPort );
	virtual ~CMatchMakingPanel();

	void AttachToGameUI();
	virtual const char *GetName( void ) OVERRIDE;
	virtual void SetData( KeyValues *data ) OVERRIDE {}
	virtual void Reset() OVERRIDE { Update(); SetVisible( true ); }
	virtual void Update() OVERRIDE { return; }
	virtual bool NeedsUpdate( void ) OVERRIDE { return false; }
	virtual bool HasInputElements( void ) OVERRIDE { return true; }
	virtual void ShowPanel( bool bShow ) OVERRIDE;

	// both vgui::Frame and IViewPortPanel define these, so explicitly define them here as passthroughs to vgui
	vgui::VPANEL GetVPanel( void ){ return BaseClass::GetVPanel(); }
	virtual bool IsVisible() OVERRIDE { return BaseClass::IsVisible(); }
	virtual void SetParent( vgui::VPANEL parent ) OVERRIDE { BaseClass::SetParent( parent ); }

	virtual GameActionSet_t GetPreferredActionSet() { return GAME_ACTION_SET_MENUCONTROLS; }

	virtual void ApplySchemeSettings( IScheme *pScheme ) OVERRIDE;
	virtual void PerformLayout() OVERRIDE;
	virtual void OnCommand( const char *pCommand ) OVERRIDE;
	virtual void FireGameEvent( IGameEvent *event ) OVERRIDE;
	virtual void SetVisible( bool bState ) OVERRIDE;
	virtual void OnKeyCodePressed( KeyCode code ) OVERRIDE;
	virtual void OnKeyCodeTyped(KeyCode code) OVERRIDE;
	virtual void OnThink() OVERRIDE;

	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );

private:
	void StartSearch( void );
	void StopSearch( void );

	ButtonCode_t m_iMMPanelKey;
	vgui::EditablePanel *m_pMainContainer;
	vgui::EditablePanel *m_pCompetitiveModeGroupPanel;
	vgui::Label *m_pModeLabel;
	vgui::ComboBox *m_pModeComboBox;
	vgui::Button *m_pStopSearchButton;
	vgui::Button *m_pSearchButton;
	vgui::EditablePanel *m_pSearchActiveGroupBox;
	vgui::Label *m_pSearchActiveTitleLabel;
};

#endif // MATCH_MAKING_PANEL_H
