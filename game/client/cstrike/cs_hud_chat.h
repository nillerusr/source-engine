//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CS_HUD_CHAT_H
#define CS_HUD_CHAT_H
#ifdef _WIN32
#pragma once
#endif

#include <hud_basechat.h>

//--------------------------------------------------------------------------------------------------------------
class CHudChatLine : public CBaseHudChatLine
{
	DECLARE_CLASS_SIMPLE( CHudChatLine, CBaseHudChatLine );

public:
	CHudChatLine( vgui::Panel *parent, const char *panelName );

	virtual void	ApplySchemeSettings(vgui::IScheme *pScheme);

private:
	CHudChatLine( const CHudChatLine & ); // not defined, not accessible
	
};

//-----------------------------------------------------------------------------
// Purpose: The prompt and text entry area for chat messages
//-----------------------------------------------------------------------------
class CHudChatInputLine : public CBaseHudChatInputLine
{
	DECLARE_CLASS_SIMPLE( CHudChatInputLine, CBaseHudChatInputLine );
	
public:
	CHudChatInputLine( CBaseHudChat *parent, char const *panelName ) : CBaseHudChatInputLine( parent, panelName ) {}

	virtual void	ApplySchemeSettings(vgui::IScheme *pScheme);
};

class CHudChat : public CBaseHudChat
{
	DECLARE_CLASS_SIMPLE( CHudChat, CBaseHudChat );

public:
	CHudChat( const char *pElementName );

	virtual void	CreateChatInputLine( void );
	virtual void	CreateChatLines( void );

	virtual void	Init( void );
	virtual void	Reset( void );

	virtual void	StartMessageMode( int iMessageModeType );
	virtual void	StopMessageMode( void );

	virtual void	MsgFunc_SayText2( bf_read &msg );
	virtual void	MsgFunc_RadioText( bf_read &msg );
	void			MsgFunc_RawAudio( bf_read &msg );

	int				GetChatInputOffset( void );


	virtual Color	GetTextColorForClient( TextColor colorNum, int clientIndex );
	virtual Color	GetClientColor( int clientIndex );

	virtual int GetFilterForString( const char *pString );
};

#endif	//CS_HUD_CHAT_H
