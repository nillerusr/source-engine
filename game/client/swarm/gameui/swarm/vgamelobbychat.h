//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VGAMELOBBYCHAT_H__
#define __VGAMELOBBYCHAT_H__

#include "basemodui.h"

#include "vgui_controls/TextEntry.h"
#include "vgui_controls/Panel.h"
#include "vgui_controls/RichText.h"

namespace BaseModUI {

class GameLobby;

//-----------------------------------------------------------------------------
// CGameLobbyChatEntry
//-----------------------------------------------------------------------------
class CGameLobbyChatEntry : public vgui::TextEntry
{
	typedef vgui::TextEntry BaseClass;
public:
	CGameLobbyChatEntry( vgui::Panel *parent, char const *panelName, GameLobby *pLobby );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void OnKeyCodeTyped(vgui::KeyCode code);
	virtual void OnKeyTyped( wchar_t unichar );

private:
	GameLobby *m_pGameLobby;
};

//-----------------------------------------------------------------------------
// CGameLobbyChatInputLine
//-----------------------------------------------------------------------------
class CGameLobbyChatInputLine : public vgui::Panel
{
	typedef vgui::Panel BaseClass;
	
public:
	CGameLobbyChatInputLine( GameLobby *parent, char const *panelName );

	void SetPrompt( const wchar_t *prompt );
	void SetPrompt( const char *prompt );	// TERROR
	void ClearEntry( void );
	void SetEntry( const wchar_t *entry );
	void GetMessageText( wchar_t *buffer, int buffersizebytes );

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	vgui::Panel		*GetInputPanel( void );
	vgui::Label	*GetPrompt( void ) { return m_pPrompt; }
	void SetOnceNavUp( Panel* navUp );
	void SetOnceNavDown( Panel* navDown );
	void SetOnceNavFrom( Panel* navFrom );
	virtual Panel* NavigateUp();
	virtual Panel* NavigateDown();

	virtual void NavigateTo()
	{
		BaseClass::NavigateTo();
		if ( IsPC() )
		{
			m_pInput->RequestFocus( 0 );
		}
	}

public:
	void StartChat();

protected:
	vgui::Label	*m_pPrompt;
	CGameLobbyChatEntry	*m_pInput;
	vgui::Panel *m_pOnceNavUp;
	vgui::Panel *m_pOnceNavDown;
	vgui::Panel *m_pOnceNavFrom;
};

//-----------------------------------------------------------------------------
// CGameLobbyChatHistory
//-----------------------------------------------------------------------------
class CGameLobbyChatHistory : public vgui::RichText, public IMatchEventsSink
{
	DECLARE_CLASS_SIMPLE( CGameLobbyChatHistory, vgui::RichText );
public:

	CGameLobbyChatHistory( vgui::Panel *pParent, const char *panelName );
	~CGameLobbyChatHistory();

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

public:
	virtual void OnEvent( KeyValues *pEvent );

protected:
	void NotifyPlayerChange( KeyValues *pPlayer, char const *szFmtName );

protected:
	char m_chLastAccessPrinted[64];
};



} // namespace BaseModUI


#endif
