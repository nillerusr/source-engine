#ifndef _INCLUDED_ASW_MEDAL_COLLECTION_FRAME_H
#define _INCLUDED_ASW_MEDAL_COLLECTION_FRAME_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

class MedalCollectionPanel;
class SwarmopediaPanel;
namespace vgui
{
	class Button;
	class PropertySheet;
};

class CASW_Medal_Collection_Frame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CASW_Medal_Collection_Frame, vgui::Frame );
public:
	CASW_Medal_Collection_Frame( vgui::Panel *pParent, const char *pElementName);
	virtual ~CASW_Medal_Collection_Frame();

	virtual void PerformLayout();
	virtual void OnCommand(const char* command);
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
		
	vgui::Panel *m_pMedalOffsetPanel;
	MedalCollectionPanel* m_pCollectionPanel;
	SwarmopediaPanel* m_pSwarmopediaPanel;
	vgui::Button *m_pCancelButton;
	vgui::PropertySheet* m_pSheet;
};

#endif // _INCLUDED_ASW_MEDAL_COLLECTION_FRAME_H