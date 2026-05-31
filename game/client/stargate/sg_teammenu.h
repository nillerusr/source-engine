//========= StargateTC ============================================================//
//
// sg_teammenu.h - combined team + class selection panel (IViewPortPanel).
//
// 1:1 port of the GoldSrc "Identifiez-vous" panel (decompiled FUN_1002bec0):
// two team columns (Tau'ri / Goa'uld), five classes each, plus Auto-Assign,
// Spectate and Cancel. Class buttons issue `jointeam <id>` + `joinclass <name>`.
// Opened by the `chooseteam` / `changeclass` console commands or server VGUIMenu.
//=================================================================================//
#ifndef SG_TEAMMENU_H
#define SG_TEAMMENU_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>
#include <game/client/iviewport.h>
#include <utlvector.h>

class CSgTeamMenu : public vgui::Frame, public IViewPortPanel
{
	DECLARE_CLASS_SIMPLE( CSgTeamMenu, vgui::Frame );

public:
	explicit CSgTeamMenu( IViewPort *pViewPort );
	virtual ~CSgTeamMenu();

	// IViewPortPanel
	virtual const char *GetName( void ) { return PANEL_TEAM; }
	virtual void SetData( KeyValues *data ) {}
	virtual void Reset( void ) {}
	virtual void Update( void ) {}
	virtual bool NeedsUpdate( void ) { return false; }
	virtual bool HasInputElements( void ) { return true; }
	virtual void ShowPanel( bool bShow );

	// both vgui::Frame and IViewPortPanel define these -> passthroughs
	vgui::VPANEL GetVPanel( void ) { return BaseClass::GetVPanel(); }
	virtual bool IsVisible() { return BaseClass::IsVisible(); }
	virtual void SetParent( vgui::VPANEL parent ) { BaseClass::SetParent( parent ); }
	virtual GameActionSet_t GetPreferredActionSet() { return GAME_ACTION_SET_IN_GAME_HUD; }

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PaintBackground( void );
	virtual void PerformLayout( void );
	virtual void OnKeyCodePressed( vgui::KeyCode code );

	MESSAGE_FUNC_PARAMS( OnJoinClass, "JoinClass", data );
	MESSAGE_FUNC_PARAMS( OnJoinTeam, "JoinTeam", data );

private:
	vgui::Button *AddClassButton( int teamId, const char *cls, const char *label );
	vgui::Button *AddTeamButton( int teamId, const char *label, const char *key );

	IViewPort *m_pViewPort;

	vgui::Label  *m_pTitle;
	vgui::Label  *m_pTauHeader;
	vgui::Label  *m_pGoaHeader;
	CUtlVector< vgui::Button * > m_TauButtons;
	CUtlVector< vgui::Button * > m_GoaButtons;
	CUtlVector< vgui::Button * > m_BottomButtons;
};

#endif // SG_TEAMMENU_H
