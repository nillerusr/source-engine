//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Does anyone ever read this?
//
//=============================================================================//

#ifndef VGUI_BUDGETPANEL_H
#define VGUI_BUDGETPANEL_H
#ifdef _WIN32
#pragma once
#endif


#include "vgui/vgui_budgetpanelshared.h"
#include "tier0/vprof.h"


#define NUM_BUDGET_FPS_LABELS 3


// Use the shared budget panel between the engine and dedicated server.
class CBudgetPanelEngine : public CBudgetPanelShared
{
	typedef CBudgetPanelShared BaseClass;

public:
	CBudgetPanelEngine( vgui::Panel *pParent, const char *pElementName );
	~CBudgetPanelEngine();

	virtual void SetTimeLabelText();
	virtual void SetHistoryLabelText();

	virtual void PostChildPaint();
	virtual void OnTick( void );

	// Command handlers
	void UserCmd_ShowBudgetPanel( void );
	void UserCmd_HideBudgetPanel( void );

	bool IsBudgetPanelShown() const;

private:

	bool m_bShowBudgetPanelHeld;
};

CBudgetPanelEngine *GetBudgetPanel( void );


#endif // VGUI_BUDGETPANEL_H
