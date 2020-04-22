//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef CLICKABLETABBEDPANEL_H
#define CLICKABLETABBEDPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/MouseCode.h>

#include <vgui_controls/PropertySheet.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/Panel.h>

namespace vgui
{
class Panel;
};


class CClickableTabbedPanel: public vgui2::PropertySheet 
{

public:
	CClickableTabbedPanel(vgui2::Panel *parent, const char *panelName);
	~CClickableTabbedPanel();


private:
//	void onMousePressed(vgui2::MouseCode code);


};

#endif //CLICKABLETABBEDPANEL