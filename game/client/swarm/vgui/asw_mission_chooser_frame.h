#ifndef _INCLUDED_IASW_MISSION_CHOOSER_FRAME_H
#define _INCLUDED_IASW_MISSION_CHOOSER_FRAME_H
#ifdef _WIN32
#pragma once
#endif

#include "missionchooser/iasw_mission_chooser_source.h"
#include <vgui_controls/Frame.h>

class CASW_Mission_Chooser_List;
class ServerOptionsPanel;
namespace vgui
{
	class PropertySheet;
};

class CASW_Mission_Chooser_Frame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CASW_Mission_Chooser_Frame, vgui::Frame );
public:
	CASW_Mission_Chooser_Frame( vgui::Panel *pParent, const char *pElementName, int iHostType, int iChooserType, IASW_Mission_Chooser_Source *pMissionSource );
	virtual ~CASW_Mission_Chooser_Frame();

	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnClose();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void RemoveTranslucency();
		
	CASW_Mission_Chooser_List *m_pChooserList;
	ServerOptionsPanel *m_pOptionsPanel;
	vgui::PropertySheet *m_pSheet;

	IASW_Mission_Chooser_Source* m_pMissionSource;
	bool m_bMadeModal;
	bool m_bAvoidTranslucency;
};

#endif // _INCLUDED_IASW_MISSION_CHOOSER_FRAME_H