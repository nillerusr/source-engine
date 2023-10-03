#include "cbase.h"
#include "vgui/ivgui.h"
#include <vgui/vgui.h>
#include "ServerOptionsPanel.h"
#include <vgui_controls/Label.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/TextImage.h>
#include <vgui/isurface.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_last_server_name("asw_last_server_name", "AS:I Server", FCVAR_ARCHIVE, "Last name used for starting a server from the main menu");
ConVar asw_last_max_players("asw_last_max_players", "6", FCVAR_ARCHIVE, "Last maxplayers used for starting a server from the main menu");
ConVar asw_last_sv_lan("asw_last_sv_lan", "0", FCVAR_ARCHIVE, "Last sv_lan used for starting a server from the main menu");

ServerOptionsPanel::ServerOptionsPanel( vgui::Panel *pParent, const char *pElementName)	: vgui::Panel(pParent, pElementName)
{
	//vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	//SetScheme(scheme);

	m_HostNameLabel = new vgui::Label(this, "HostNameLabel", "#asw_host_name");
	m_pHostNameEntry = new vgui::TextEntry(this, "HostNameEntry");
	m_pHostNameEntry->SetText(asw_last_server_name.GetString());

	m_pPasswordLabel = new vgui::Label(this, "PasswordLabel", "#asw_server_password");
	m_pServerPasswordEntry = new vgui::TextEntry(this, "ServerPasswordEntry");
	m_pServerPasswordEntry->SetText("");

	m_pMaxPlayersLabel = new vgui::Label(this, "MaxPlayersLabel", "#asw_max_players");
	m_pMaxPlayersCombo = new vgui::ComboBox(this, "MaxPlayersComboBox", 5, false);
	m_pMaxPlayersCombo->AddItem("2", NULL);
	m_pMaxPlayersCombo->AddItem("3", NULL);
	m_pMaxPlayersCombo->AddItem("4", NULL);
	m_pMaxPlayersCombo->AddItem("5", NULL);
	m_pMaxPlayersCombo->AddItem("6", NULL);
	char maxplayersbuffer[8];
	int players = clamp<int>(asw_last_max_players.GetInt(), 2, 6);
	Q_snprintf(maxplayersbuffer, sizeof(maxplayersbuffer), "%d", players);
	m_pMaxPlayersCombo->SetText(maxplayersbuffer);
	
	m_pLANCheck = new vgui::CheckButton(this, "LanCheckButton", "#asw_lan_server");
	m_pLANCheck->SetSelected(asw_last_sv_lan.GetBool());

	m_pCancelButton = new vgui::Button(this, "CancelButton", "#asw_chooser_close", this, "Cancel");	
}

ServerOptionsPanel::~ServerOptionsPanel()
{
}

void ServerOptionsPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	int sw, sh;
	vgui::surface()->GetScreenSize( sw, sh );
	float fScale = float(sh) / 768.0f;

	int w = GetParent()->GetWide();

	int padding = 8 * fScale;
	int option_spacing = 24 * fScale;
	int left_edge = padding * 3;
	int choice_left_edge = padding * 3;
	int option_width = w * 0.25f;
	int option_height = 20 * fScale;
	int ypos = option_spacing;
	
	m_HostNameLabel->SetBounds(left_edge, ypos, option_width, option_height); ypos+=option_height + padding;
	m_pHostNameEntry->SetBounds(choice_left_edge, ypos, option_width, option_height); ypos+=option_height + option_spacing;
	m_pPasswordLabel->SetBounds(left_edge, ypos, option_width, option_height); ypos+=option_height + padding;
	m_pServerPasswordEntry->SetBounds(choice_left_edge, ypos, option_width, option_height); ypos+=option_height + option_spacing;
	m_pMaxPlayersLabel->SetBounds(left_edge, ypos, option_width, option_height); ypos+=option_height + padding;
	m_pMaxPlayersCombo->SetBounds(choice_left_edge, ypos, option_width, option_height); ypos+=option_height + option_spacing;
	m_pLANCheck->SetBounds(left_edge, ypos, option_width, option_height); ypos+=option_height + padding;
	m_pCancelButton->SetTextInset(6.0f * fScale, 0);

	m_pCancelButton->GetTextImage()->ResizeImageToContent();
	m_pCancelButton->SizeToContents();
	int cancel_wide = m_pCancelButton->GetWide();
	int iFooterSize = 32 * fScale;	
	m_pCancelButton->SetSize(cancel_wide, iFooterSize - 4 * fScale);
	m_pCancelButton->SetPos(GetWide() - cancel_wide,
							GetTall() - iFooterSize);
}

void ServerOptionsPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	vgui::HFont defaultfont = pScheme->GetFont( "MissionChooserFont", IsProportional() );
	if (defaultfont == vgui::INVALID_FONT)
	{
		defaultfont = pScheme->GetFont( "Default", IsProportional() );
	}

	m_HostNameLabel->SetFont(defaultfont);
	m_pHostNameEntry->SetFont(defaultfont);
	m_pPasswordLabel->SetFont(defaultfont);
	m_pServerPasswordEntry->SetFont(defaultfont);
	m_pMaxPlayersLabel->SetFont(defaultfont);
	m_pMaxPlayersCombo->SetFont(defaultfont);
	m_pLANCheck->SetFont(defaultfont);
	m_pCancelButton->SetFont(defaultfont);

	Color white(255,255,255,255);
	Color nothing(0, 0, 0, 0);
	m_HostNameLabel->SetFgColor(white);
	m_pHostNameEntry->SetFgColor(white);
	m_pPasswordLabel->SetFgColor(white);
	m_pServerPasswordEntry->SetFgColor(white);
	m_pMaxPlayersLabel->SetFgColor(white);
	m_pMaxPlayersCombo->SetFgColor(white);
	m_pLANCheck->SetFgColor(white);
	m_pLANCheck->SetDefaultColor(white, nothing);
	m_pLANCheck->SetArmedColor(white, nothing);
	m_pLANCheck->SetDepressedColor(white, nothing);
}

const char* ServerOptionsPanel::GetHostName()
{
	if (!m_pHostNameEntry)
		return "";

	static char hostname_buffer[32];
	m_pHostNameEntry->GetText(hostname_buffer, 32);

	asw_last_server_name.SetValue(hostname_buffer);

	return hostname_buffer;
}

const char* ServerOptionsPanel::GetPassword()
{
	if (!m_pServerPasswordEntry)
		return "";

	static char password_buffer[32];
	m_pServerPasswordEntry->GetText(password_buffer, 32);

	return password_buffer;
}

int ServerOptionsPanel::GetMaxPlayers()
{
	if (!m_pMaxPlayersCombo)
		return 6;

	char maxplayersbuffer[8];
	m_pMaxPlayersCombo->GetText(maxplayersbuffer, 8);

	int players = clamp<int>(atoi(maxplayersbuffer), 2, 6);

	asw_last_max_players.SetValue(players);

	return players;
}

bool ServerOptionsPanel::GetLAN()
{
	bool b = (m_pLANCheck && m_pLANCheck->IsSelected());
	asw_last_sv_lan.SetValue(b);
	return b;
}

void ServerOptionsPanel::CloseSelf()
{
	// yarr, this assumes we're inside a propertysheet, inside a frame..
	//  if the options panel is placed in a different config, it'll close the wrong thing
	if (GetParent() && GetParent()->GetParent())
	{
		GetParent()->GetParent()->SetVisible(false);
		GetParent()->GetParent()->MarkForDeletion();
	}
}

void ServerOptionsPanel::OnCommand(const char* command)
{
	if (!stricmp(command, "Cancel"))
	{
		CloseSelf();
	}
}