#ifndef ASW_HUD_CHAT_H
#define ASW_HUD_CHAT_H
#ifdef _WIN32
#pragma once
#endif

#include <hud_basechat.h>

extern ConVar asw_draw_hud;

class CHudChat : public CBaseHudChat
{
	DECLARE_CLASS_SIMPLE( CHudChat, CBaseHudChat );

public:
	CHudChat( const char *pElementName );

	virtual void	Init( void );

	void			MsgFunc_SayText(bf_read &msg);
	void			MsgFunc_SayText2( bf_read &msg );
	void			MsgFunc_TextMsg(bf_read &msg);

#ifdef INFESTED_DLL
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PerformLayout();
	virtual bool ShouldDraw( void ) { return asw_draw_hud.GetBool() && CHudElement::ShouldDraw(); }
	int GetChatInputOffset( void );

	virtual void	OnTick( void );

	virtual void	StartMessageMode( int iMessageModeType );
	virtual void	StopMessageMode( bool bFade = true );

	virtual Color	GetTextColorForClient( TextColor colorNum, int clientIndex );
	virtual Color	GetClientColor( int clientIndex );

	virtual void InsertBlankPage();

	//virtual void OnMouseReleased( vgui::MouseCode code );

	void ShowChatPanel();

	virtual void			FadeChatHistory();

	void SetBriefingPosition( bool bUseBriefingPosition );
	bool m_bBriefingPosition;

	vgui::Panel *m_pSwarmBackground;
	vgui::Panel *m_pSwarmBackgroundInner;
#endif
};

#endif	//ASW_HUD_CHAT_H