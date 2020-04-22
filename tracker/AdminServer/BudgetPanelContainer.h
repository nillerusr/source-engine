//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef BUDGETPANELCONTAINER_H
#define BUDGETPANELCONTAINER_H
#ifdef _WIN32
#pragma once
#endif


#include <KeyValues.h>

#include <vgui_controls/Frame.h>
#include <vgui_controls/PHandle.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/PropertyPage.h>
#include "utlvector.h"
#include "RemoteServer.h"


class CBudgetPanelAdmin;


//-----------------------------------------------------------------------------
// Purpose: Dialog for displaying information about a game server
//-----------------------------------------------------------------------------
class CBudgetPanelContainer : public vgui::PropertyPage, public IServerDataResponse
{
	DECLARE_CLASS_SIMPLE( CBudgetPanelContainer, vgui::PropertyPage );
public:
	CBudgetPanelContainer(vgui::Panel *parent, const char *name);
	~CBudgetPanelContainer();


// Panel overrides.
public:
	virtual void Paint();
	virtual void PerformLayout();


// PropertyPage overrides.
public:
	void OnPageShow();
	void OnPageHide();


// IServerDataResponse overrides.
public:
	virtual void OnServerDataResponse(const char *value, const char *response);


private:
	CBudgetPanelAdmin *m_pBudgetPanelAdmin;
};


#endif // BUDGETPANELCONTAINER_H
