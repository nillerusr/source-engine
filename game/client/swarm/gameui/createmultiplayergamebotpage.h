//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CREATEMULTIPLAYERGAMEBOTPAGE_H
#define CREATEMULTIPLAYERGAMEBOTPAGE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyPage.h>

class CPanelListPanel;
class CDescription;
class mpcontrol_t;
class CCvarToggleCheckButton;

//-----------------------------------------------------------------------------
// Purpose: advanced bot properties page of the create game server dialog
//-----------------------------------------------------------------------------
class CCreateMultiplayerGameBotPage : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( CCreateMultiplayerGameBotPage, vgui::PropertyPage );

public:
	CCreateMultiplayerGameBotPage( vgui::Panel *parent, const char *name, KeyValues *botKeys );
	~CCreateMultiplayerGameBotPage();

protected:
	virtual void OnResetChanges();
	virtual void OnApplyChanges();

private:
	CCvarToggleCheckButton *m_joinAfterPlayer;

	CCvarToggleCheckButton *m_allowRogues;

	CCvarToggleCheckButton *m_allowPistols;
	CCvarToggleCheckButton *m_allowShotguns;
	CCvarToggleCheckButton *m_allowSubmachineGuns;
	CCvarToggleCheckButton *m_allowMachineGuns;
	CCvarToggleCheckButton *m_allowRifles;
	CCvarToggleCheckButton *m_allowGrenades;
#ifdef CS_SHIELD_ENABLED
	CCvarToggleCheckButton *m_allowShields;
#endif // CS_SHIELD_ENABLED
	CCvarToggleCheckButton *m_allowSnipers;

	CCvarToggleCheckButton *m_deferToHuman;

	vgui::ComboBox *m_joinTeamCombo;
	void SetJoinTeamCombo( const char *team );

	vgui::ComboBox *m_chatterCombo;
	void SetChatterCombo( const char *team );

	vgui::TextEntry *m_prefixEntry;

	KeyValues *m_pSavedData;
};


#endif // CREATEMULTIPLAYERGAMEBOTPAGE_H
