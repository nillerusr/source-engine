#ifndef _INCLUDED_ASW_BUILD_MAP_FRAME_H
#define _INCLUDED_ASW_BUILD_MAP_FRAME_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

namespace vgui
{
	class Button;
	class ProgressBar;
};

class IASW_Map_Builder;

class CASW_Build_Map_Frame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CASW_Build_Map_Frame, vgui::Frame );
public:
	CASW_Build_Map_Frame( vgui::Panel *pParent, const char *pElementName );
	virtual ~CASW_Build_Map_Frame();

	virtual void PerformLayout();
	virtual void OnThink();

	IASW_Map_Builder* GetMapBuilder() { return m_pMapBuilder; }
	void SetRunMapAfterBuild( bool bRunMap ) { m_bRunMapAfterBuild = bRunMap; }
	void SetEditMapAfterBuild( bool bEditMap, const char *szMapEditFilename );

private:
	void UpdateProgress();

	vgui::Label *m_pStatusLabel;
	vgui::ProgressBar *m_pProgressBar;
	float m_fCloseWindowTime;
	bool m_bRunMapAfterBuild;
	bool m_bEditMapAfterBuild;
	IASW_Map_Builder *m_pMapBuilder;
	char m_szMapEditFilename[MAX_PATH];
};

#endif // _INCLUDED_ASW_BUILD_MAP_FRAME_H