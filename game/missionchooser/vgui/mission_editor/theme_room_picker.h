#ifndef THEME_ROOM_PICKER_H
#define THEME_ROOM_PICKER_H
#ifdef _WIN32
#pragma once
#endif

#include "ThemesDialog.h"

namespace vgui {
	class PanelListPanel;
	class Label;
};
class CLevelTheme;

// this dialog allows you to select a theme and optionally a room within that theme

class CThemeRoomPicker : public CThemesDialog
{
	DECLARE_CLASS_SIMPLE( CThemeRoomPicker, CThemesDialog );

public:

	CThemeRoomPicker( vgui::Panel *parent, const char *name, KeyValues *pKey, bool bPickRooms );
	virtual ~CThemeRoomPicker();

	virtual void ThemeClicked( CThemeDetails *pThemeDetails );
	virtual void PopulateThemeList();
	virtual bool ShouldHighlight( CThemeDetails *pDetails );
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnCommand( const char *command );

	CLevelTheme *m_pSelectedTheme;
	KeyValues *m_pKey;
	bool m_bPickRooms;
};

#endif // THEME_ROOM_PICKER_H