//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef VGUITEXTWINDOW_H
#define VGUITEXTWINDOW_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/HTML.h>

#include <game/client/iviewport.h>
#include "shareddefs.h"
#include "GameEventListener.h"

namespace vgui
{
	class TextEntry;
}

enum
{
	FADE_STATUS_IN = 0,
	FADE_STATUS_HOLD, 
	FADE_STATUS_OUT,
	FADE_STATUS_OFF
};

//-----------------------------------------------------------------------------
// Purpose: displays the MOTD
//-----------------------------------------------------------------------------

class CTextWindow : public vgui::Frame, public IViewPortPanel, public CGameEventListener
{
private:
	DECLARE_CLASS_SIMPLE( CTextWindow, vgui::Frame );

public:
	CTextWindow(IViewPort *pViewPort);
	virtual ~CTextWindow();

	virtual const char *GetName( void ) { return PANEL_INFO; }
	virtual void SetData(KeyValues *data);
	virtual void Reset();
	virtual void Update();
	virtual void UpdateContents( void );
	virtual bool NeedsUpdate( void ) { return true; }
	virtual bool HasInputElements( void ) { return true; }
	virtual void ShowPanel( bool bShow );

	// both vgui::Frame and IViewPortPanel define these, so explicitly define them here as passthroughs to vgui
	vgui::VPANEL GetVPanel( void ) { return BaseClass::GetVPanel(); }
  	virtual bool IsVisible() { return BaseClass::IsVisible(); }
  	virtual void SetParent( vgui::VPANEL parent ) { BaseClass::SetParent( parent ); }

	virtual void FireGameEvent( IGameEvent *event );
	virtual bool WantsBackgroundBlurred( void );

public:

	virtual void SetData( int type, const char *title, const char *message, const char *command );
	virtual void ShowFile( const char *filename);
	virtual void ShowText( const char *text);
	virtual void ShowURL( const char *URL);
	virtual void ShowIndex( const char *entry);

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

protected:	
	// vgui overrides
	virtual void OnCommand( const char *command);

	IViewPort	*m_pViewPort;
	char		m_szTitle[255];
	char		m_szMessage[2048];
	char		m_szExitCommand[255];
	int			m_nContentType;

	vgui::TextEntry	*m_pTextMessage;
	vgui::HTML		*m_pHTMLMessage;
	vgui::Button	*m_pOK;
	vgui::Label		*m_pTitleLabel;

	float m_flNextFadeTime;
	int m_iFadeStatus;
	bool m_bMiniMode;
	bool m_bIgnoreMultipleShowRequests;
};


#endif // VGUITEXTWINDOW_H
