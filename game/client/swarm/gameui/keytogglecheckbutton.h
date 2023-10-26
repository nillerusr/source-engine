//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef KEYTOGGLECHECKBUTTON_H
#define KEYTOGGLECHECKBUTTON_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/CheckButton.h>

class CKeyToggleCheckButton : public vgui::CheckButton
{
public:
	CKeyToggleCheckButton( vgui::Panel *parent, const char *panelName, const char *text, 
		char const *keyname, char const *cmdname );
	~CKeyToggleCheckButton();

	//virtual void	SetSelected( bool state );
	virtual void	Paint();

	void			Reset();
	void			ApplyChanges();
	bool			HasBeenModified();

private:
	typedef vgui::CheckButton BaseClass;

	char			*m_pszKeyName;
	char			*m_pszCmdName;

	bool			m_bStartValue;
};
#endif // KEYTOGGLECHECKBUTTON_H
