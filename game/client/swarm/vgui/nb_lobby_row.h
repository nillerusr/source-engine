#ifndef _INCLUDED_NB_LOBBY_ROW_H
#define _INCLUDED_NB_LOBBY_ROW_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>
#include "asw_shareddefs.h"
#include "steam/steam_api.h"

// == MANAGED_CLASS_DECLARATIONS_START: Do not edit by hand ==
class vgui::ImagePanel;
class vgui::Label;
class StatsBar;
class vgui::Panel;
class CAvatarImagePanel;
// == MANAGED_CLASS_DECLARATIONS_END ==
class CBitmapButton;
class CNB_Lobby_Tooltip;
namespace BaseModUI
{
	class DropDownMenu;
}

class CNB_Lobby_Row : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CNB_Lobby_Row, vgui::EditablePanel );
public:
	CNB_Lobby_Row( vgui::Panel *parent, const char *name );
	virtual ~CNB_Lobby_Row();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand( const char *command );
	void OpenPlayerFlyout();

	virtual void UpdateDetails();
	virtual void CheckTooltip( CNB_Lobby_Tooltip *pTooltip );
	
	// == MANAGED_MEMBER_POINTERS_START: Do not edit by hand ==
	vgui::ImagePanel	*m_pBackground;
	vgui::Panel	*m_pBackgroundWeapon0;
	vgui::Panel	*m_pBackgroundWeapon1;
	vgui::Panel	*m_pBackgroundWeapon2;
	vgui::Panel	*m_pBackgroundInnerWeapon0;
	vgui::Panel	*m_pBackgroundInnerWeapon1;
	vgui::Panel	*m_pBackgroundInnerWeapon2;
	vgui::ImagePanel	*m_pSilhouetteWeapon0;
	vgui::ImagePanel	*m_pSilhouetteWeapon1;
	vgui::ImagePanel	*m_pSilhouetteWeapon2;
	vgui::Panel *m_pAvatarBackground;
	CAvatarImagePanel	*m_pAvatarImage;
	vgui::ImagePanel	*m_pClassImage;
	BaseModUI::DropDownMenu	*m_pNameDropdown;
	vgui::Label	*m_pLevelLabel;
	StatsBar	*m_pXPBar;
	vgui::Label	*m_pClassLabel;	
	// == MANAGED_MEMBER_POINTERS_END ==
	CBitmapButton	*m_pPortraitButton;
	CBitmapButton	*m_pWeaponButton0;
	CBitmapButton	*m_pWeaponButton1;
	CBitmapButton	*m_pWeaponButton2;
	vgui::ImagePanel *m_pVoiceIcon;
	vgui::ImagePanel *m_pPromotionIcon;

	char m_szLastWeaponImage[ ASW_NUM_INVENTORY_SLOTS ][ 255 ];
	char m_szLastPortraitImage[ 255 ];
	CSteamID m_lastSteamID;
	int m_nLastPromotion;

	int m_nLobbySlot;
};

#endif // _INCLUDED_NB_LOBBY_ROW_H










