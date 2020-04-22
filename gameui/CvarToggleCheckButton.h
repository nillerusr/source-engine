//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CVARTOGGLECHECKBUTTON_H
#define CVARTOGGLECHECKBUTTON_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/CheckButton.h>

class CCvarToggleCheckButton : public vgui::CheckButton
{
	DECLARE_CLASS_SIMPLE( CCvarToggleCheckButton, vgui::CheckButton );

public:
	CCvarToggleCheckButton( vgui::Panel *parent, const char *panelName, const char *text, 
		char const *cvarname );
	~CCvarToggleCheckButton();

	virtual void	SetSelected( bool state );

	virtual void	Paint();

	void			Reset();
	void			ApplyChanges();
	bool			HasBeenModified();
	virtual void	ApplySettings( KeyValues *inResourceData );

private:
	MESSAGE_FUNC( OnButtonChecked, "CheckButtonChecked" );

	char			*m_pszCvarName;
	bool			m_bStartValue;
};

#endif // CVARTOGGLECHECKBUTTON_H
