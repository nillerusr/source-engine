//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VFOOTERPANEL_H__
#define __VFOOTERPANEL_H__

#include "basemodui.h"

#define FOOTERPANEL_TEXTLEN 64

namespace BaseModUI
{

enum FooterFormat_t
{
	FF_NONE,
	FF_MAINMENU,
	FF_MAINMENU_FLYOUT,
	FF_CONTROLLER,
	FF_AB_ONLY,
	FF_ABX_ONLY,
	FF_ABY_ONLY,
	FF_A_GAMERCARD_BY,
	FF_A_GAMERCARD_BX,
	FF_A_GAMERCARD_BY__X_HIGH,
	FF_B_ONLY,
	FF_CONTROLLER_STICKS,
	FF_ACHIEVEMENTS,
	FF_MAX
};

enum Button_t
{
	FB_NONE		= 0x00,
	FB_ABUTTON	= 0x01,
	FB_BBUTTON	= 0x02,
	FB_XBUTTON	= 0x04,
	FB_YBUTTON	= 0x08,
	FB_DPAD		= 0x10,
};

class CBaseModFooterPanel : public CBaseModFrame
{
public:
	DECLARE_CLASS_SIMPLE( CBaseModFooterPanel , CBaseModFrame );

	typedef unsigned int FooterButtons_t;

	CBaseModFooterPanel( vgui::Panel *pParent, const char *pPanelName );
	~CBaseModFooterPanel();

	void			SetButtons( FooterButtons_t flag, FooterFormat_t format, bool bEnableHelp  );
	void			SetButtonText( Button_t button, const char *pText );
	FooterButtons_t GetButtons();
	FooterFormat_t	GetFormat();
	bool			GetHelpTextEnabled();
	void			SetHelpText( const char *text );
	const char *	GetHelpText() { return m_HelpText; }
	void			FadeHelpText( void );
	void			GetPosition( int &x, int &y );
	bool			HasContent( void );

	void			SetShowCloud( bool bShow );

protected:
	virtual void	ApplySchemeSettings( vgui::IScheme * pScheme );
	virtual void	PaintBackground();

private:
	void			UpdateHelp();
	void			DrawButtonAndText( bool bRightAligned, int x, int y, const char *pButton, const char *pText );
	void			SetHelpTextEnabled( bool bEnabled );

	char			m_AButtonText[FOOTERPANEL_TEXTLEN];
	char			m_BButtonText[FOOTERPANEL_TEXTLEN];
	char			m_XButtonText[FOOTERPANEL_TEXTLEN];
	char			m_YButtonText[FOOTERPANEL_TEXTLEN];
	char			m_DPadButtonText[FOOTERPANEL_TEXTLEN];
	char			m_HelpText[ 512 ];
	wchar_t			m_szconverted[ 512 ];

	FooterButtons_t	m_Buttons;
	FooterFormat_t	m_Format;
	bool			m_bHelpTextEnabled;

	vgui::HFont		m_hHelpTextFont;
	vgui::HFont		m_hButtonFont;
	vgui::HFont		m_hButtonTextFont;

	bool			m_bInitialized;

	float			m_fFadeHelpTextStart;
};

};

#endif