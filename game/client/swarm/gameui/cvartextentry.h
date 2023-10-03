//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CVARTEXTENTRY_H
#define CVARTEXTENTRY_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/TextEntry.h>

class CCvarTextEntry : public vgui::TextEntry
{
	DECLARE_CLASS_SIMPLE( CCvarTextEntry, vgui::TextEntry );

public:
	CCvarTextEntry( vgui::Panel *parent, const char *panelName, char const *cvarname );
	~CCvarTextEntry();

	MESSAGE_FUNC( OnTextChanged, "TextChanged" );
	void			ApplyChanges(  bool immediate = false );
	virtual void	ApplySchemeSettings(vgui::IScheme *pScheme);
    void            Reset();
    bool            HasBeenModified();

private:
	char			*m_pszCvarName;
	char			m_pszStartValue[64];
};

#endif // CVARTEXTENTRY_H
