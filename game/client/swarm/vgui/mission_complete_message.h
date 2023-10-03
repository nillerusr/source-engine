#ifndef _INCLUDED_MISSION_COMPLETE_MESSAGE_H
#define _INCLUDED_MISSION_COMPLETE_MESSAGE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>

class vgui::Label;
class vgui::ImagePanel;
class vgui::TextEntry;

struct CAnimating_Letter
{
	float m_flStartTime;
	float m_flEndTime;
	int m_nStartX;
	int m_nStartY;
	int m_nEndX;
	int m_nEndY;
	int m_nStartWidth;
	int m_nStartHeight;
	int m_nEndWidth;	
	int m_nEndHeight;
	char m_chLetter;
};

class CMission_Complete_Message : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CMission_Complete_Message, vgui::EditablePanel );
public:
	CMission_Complete_Message( vgui::Panel *parent, const char *name );
	virtual ~CMission_Complete_Message();
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void Paint();

	void PaintMessageBackground();
	void StartMessage( bool bSuccess );
	void AddLetter( char letter, int x, int y, float letter_offset, float flStartTime );
	void PaintLetters();
	void PaintLetter( CAnimating_Letter *pLetter, bool bGlow );
	bool m_bSuccess;
	float m_flMessageBackgroundStartTime;
	float m_flMessageBackgroundFadeDuration;
	
	CUtlVector<CAnimating_Letter*> m_aAnimatingLetters;
};

#endif // _INCLUDED_MISSION_COMPLETE_MESSAGE_H














