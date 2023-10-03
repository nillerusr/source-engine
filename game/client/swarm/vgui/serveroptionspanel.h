#ifndef _INCLUDED_SERVER_OPTIONS_PANEL_H
#define _INCLUDED_SERVER_OPTIONS_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>

namespace vgui
{
	class Label;
	class TextEntry;
	class ComboBox;
	class CheckButton;
	class Button;
};

class ServerOptionsPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( ServerOptionsPanel, vgui::Panel );
public:
	ServerOptionsPanel( vgui::Panel *pParent, const char *pElementName);
	virtual ~ServerOptionsPanel();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnCommand(const char* command);
	virtual void CloseSelf();

	const char* GetHostName();
	const char* GetPassword();
	int GetMaxPlayers();
	bool GetLAN();

private:
	vgui::Label* m_HostNameLabel;
	vgui::TextEntry* m_pHostNameEntry;

	vgui::Label* m_pPasswordLabel;
	vgui::TextEntry* m_pServerPasswordEntry;

	vgui::Label* m_pMaxPlayersLabel;
	vgui::ComboBox* m_pMaxPlayersCombo;

	vgui::CheckButton *m_pLANCheck;

	vgui::Button* m_pCancelButton;
};


#endif // _INCLUDED_SERVER_OPTIONS_PANEL_H