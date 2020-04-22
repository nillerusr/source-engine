//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef VGUI_BUDGETBARGRAPHPANEL_H
#define VGUI_BUDGETBARGRAPHPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>


class CBaseBudgetPanel;


class CBudgetBarGraphPanel : public vgui::Panel
{
	typedef vgui::Panel BaseClass;

public:
	CBudgetBarGraphPanel( CBaseBudgetPanel *pParent, const char *pPanelName );
	~CBudgetBarGraphPanel();
	virtual void Paint( void );

private:

	void DrawInstantaneous();
	void DrawPeaks();
	void DrawAverages();
	void DrawTimeLines( void );
	void GetBudgetGroupTopAndBottom( int id, int &top, int &bottom );
	void DrawBarAtIndex( int id, float percent );
	void DrawTickAtIndex( int id, float percent, int red, int green, int blue, int alpha );

private:
	
	CBaseBudgetPanel *m_pBudgetPanel;
};

#endif // VGUI_BUDGETBARGRAPHPANEL_H
