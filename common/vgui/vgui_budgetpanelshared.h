//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef VGUI_BUDGETPANELSHARED_H
#define VGUI_BUDGETPANELSHARED_H

#ifdef _WIN32
#pragma once
#endif


#include "vgui_basebudgetpanel.h"


class IConVar;


// Shared between the engine and dedicated server.
class CBudgetPanelShared : public CBaseBudgetPanel
{
	typedef CBaseBudgetPanel BaseClass;

public:

	// budgetFlagsFilter is a combination of BUDGETFLAG_ defines that filters out which budget groups we are interested in.
	CBudgetPanelShared( vgui::Panel *pParent, const char *pElementName, int budgetFlagsFilter );
	~CBudgetPanelShared();

	// Override this to set the window position.
	virtual void SetupCustomConfigData( CBudgetPanelConfigData &data );

	virtual void SetTimeLabelText();
	virtual void SetHistoryLabelText();

	virtual void PaintBackground();
	virtual void Paint( void );
	virtual void PostChildPaint();

	virtual void SnapshotVProfHistory( float filteredtime  );

	// Command handlers
	void OnNumBudgetGroupsChanged( void );

	void SendConfigDataToBase();


public:

	static double g_fFrameTimeLessBudget;
	static double g_fFrameRate;


private:

	void DrawColoredText( 
		vgui::HFont font, 
		int x, int y, 
		int r, int g, int b, int a,
		PRINTF_FORMAT_STRING const char *pText,
		... ) FMTFUNCTION( 9, 10 );
};


// CVars that change the layout can hook this.
void PanelGeometryChangedCallBack( IConVar *var, const char *pOldString, float flOldValue );


#endif // VGUI_BUDGETPANELSHARED_H
