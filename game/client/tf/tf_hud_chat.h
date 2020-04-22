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

	virtual void	Init( void );
	virtual void	Reset( void );
	int				GetChatInputOffset( void );
	void			CreateChatLines( void );

	virtual bool	ShouldDraw( void );

	virtual Color	GetTextColorForClient( TextColor colorNum, int clientIndex );
	virtual Color	GetClientColor( int clientIndex );

	virtual int		GetFilterForString( const char *pString );

	virtual const char *GetDisplayedSubtitlePlayerName( int clientIndex );
	virtual bool IsVisible();

	virtual int				GetFilterFlags( void );

#if defined( _X360 )
	// hide behind other panels ( stats , build menu ) in 360
	virtual int		GetRenderGroupPriority( void ) { return 35; }	// less than statpanel
#endif
};

#endif	//CS_HUD_CHAT_H