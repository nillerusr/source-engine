//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VSIGNINDIALOG_H__
#define __VSIGNINDIALOG_H__

#include "basemodui.h"

namespace BaseModUI {

class SignInDialog : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( SignInDialog, CBaseModFrame );

public:
	SignInDialog(vgui::Panel *parent, const char *panelName);
	~SignInDialog();

	void OnCommand(const char *command);
	void OnThink();

	void OnKeyCodePressed( vgui::KeyCode code );

	void LoadLayout();
	virtual void PaintBackground();

	void ApplySettings( KeyValues * inResourceData );
	void ApplySchemeSettings( vgui::IScheme *pScheme );


	void SetSignInTitle( const wchar_t* title );
	void SetSignInTitle( const char* title );

private:
	vgui::Panel* NavigateBack( int numSlotsRequested );

private:
	bool m_bSignedIn;
	float m_flTimeAutoClose;
};

}

#endif // __VSIGNINDIALOG_H__