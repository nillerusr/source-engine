#ifndef _INCLUDED_SCANLINEPANEL_H
#define _INCLUDED_SCANLINEPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

// this panel draws scanlines over its area

class ScanLinePanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( ScanLinePanel, vgui::Panel );
public:
	ScanLinePanel(vgui::Panel *parent, const char *panelName, bool bDouble);
	virtual void Paint();
	
	bool m_bDouble;	
	CPanelAnimationVarAliasType( int, m_nDoubleScanLine, "DoubleScanLine", "vgui/swarm/DoubleScanLine", "textureid" );
	CPanelAnimationVarAliasType( int, m_nSingleScanLine, "SingleScanLine", "vgui/swarm/SingleScanLine", "textureid" );
};


#endif // _INCLUDED_SCANLINEPANEL_H