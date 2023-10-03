#ifndef _INCLUDED_ASW_DIFFICULTY_CHOOSER_H
#define _INCLUDED_ASW_DIFFICULTY_CHOOSER_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

class CASW_Difficulty_Entry;
namespace vgui
{
	class Button;
	class ImagePanel;
};

#define ASW_NUM_DIFFICULTIES 4

class CASW_Difficulty_Chooser : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CASW_Difficulty_Chooser, vgui::Frame );
public:
	CASW_Difficulty_Chooser( vgui::Panel *pParent, const char *pElementName, const char *szLaunchCommand );
	virtual ~CASW_Difficulty_Chooser();

	virtual void PerformLayout();
	virtual void OnCommand(const char* command);
	virtual void DifficultyEntryClicked(int iDifficulty);
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();

	char m_szLaunchCommand[1024];
	CASW_Difficulty_Entry* m_pDifficulty[ASW_NUM_DIFFICULTIES];
	vgui::Button *m_pCancelButton;
};

extern vgui::DHANDLE<CASW_Difficulty_Chooser> g_hDifficultyFrame;

class CASW_Difficulty_Entry : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASW_Difficulty_Entry, vgui::Panel );
public:
	CASW_Difficulty_Entry( vgui::Panel *pParent, const char *pElementName, int iDifficulty);
	virtual ~CASW_Difficulty_Entry();

	virtual void OnThink();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	void OnMouseReleased(vgui::MouseCode code);

	vgui::ImagePanel *m_pImagePanel;
	vgui::Label *m_pDifficultyLabel;
	vgui::Label *m_pDifficultyDescriptionLabel;

	vgui::HFont m_hDefaultFont;
	int m_iDifficulty;
	bool m_bMouseOver;
};

#endif // _INCLUDED_ASW_DIFFICULTY_CHOOSER_H