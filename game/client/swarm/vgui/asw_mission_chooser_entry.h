#ifndef _INCLUDED_IASW_MISSION_CHOOSER_ENTRY_H
#define _INCLUDED_IASW_MISSION_CHOOSER_ENTRY_H
#ifdef _WIN32
#pragma once
#endif

#include "missionchooser/iasw_mission_chooser_source.h"

namespace vgui
{
	class Button;
};

class CASW_Mission_Chooser_Entry : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASW_Mission_Chooser_Entry, vgui::Panel );
public:
	CASW_Mission_Chooser_Entry( vgui::Panel *pParent, const char *pElementName, int iChooserType, int iHostType );
	virtual ~CASW_Mission_Chooser_Entry();

	virtual void OnThink();
	virtual void OnCommand(const char* command);

	vgui::ImagePanel *m_pImagePanel;
	vgui::Label *m_pNameLabel;
	vgui::Label *m_pDescriptionLabel;

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	void OnMouseReleased(vgui::MouseCode code);
	void SetDetails(const char *szMapName, int nChooserType = -1);
	void SetSavedCampaignDetails(ASW_Mission_Chooser_Saved_Campaign *pSaved);
	void SetVoteDisabled( bool bDisabled );

	int m_ChooserType;
	int m_HostType;
	int m_iLabelHeight;
	char m_szMapName[256];
	
	vgui::Button *m_pDeleteButton;

	KeyValues * m_MapKeyValues; // keyvalues describing overview parameters
	bool m_bMouseOver;
	bool m_bMouseReleased;
	bool m_bVoteDisabled;
};

#endif // _INCLUDED_IASW_MISSION_CHOOSER_ENTRY_H