//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef VGUI_BASEBUDGETPANEL_H
#define VGUI_BASEBUDGETPANEL_H

#ifdef _WIN32
#pragma once
#endif


#include <vgui_controls/Frame.h>
#include <vgui/IScheme.h>
#include "vgui_budgethistorypanel.h"
#include "vgui_budgetbargraphpanel.h"
#include "utlsymbol.h"
//#include "hudelement.h"


#define BUDGET_HISTORY_COUNT 1024

class CBudgetGroupInfo
{
public:
	CUtlSymbol m_Name;
	Color m_Color;
};

// Derived classes supply this configuration data with OnConfigDataChanged.
class CBudgetPanelConfigData
{
public:
	// NOTE: nothing can ever be removed from this list once you've called 
	// OnConfigDataChanged. Elements can only be added to it.
	CUtlVector<CBudgetGroupInfo> m_BudgetGroupInfo;

	float m_flHistoryRange;
	float m_flBottomOfHistoryFraction;
	CUtlVector<float> m_HistoryLabelValues;	// A label will be placed at each of these values.

	// How much range the bar graph represents.
	float m_flBarGraphRange;

	// Controls how many labels are shown.
	float m_flTimeLabelInterval;
	int m_nLinesPerTimeLabel;	// How many vertical lines per time label?

	// How translucent is the background.
	float m_flBackgroundAlpha; 

	// Where to position it on the screen.
	int m_xCoord;
	int m_yCoord;
	int m_Width;
	int m_Height;
};


class CBaseBudgetPanel : public vgui::Panel
{
	typedef vgui::Panel BaseClass;

public:
	CBaseBudgetPanel( vgui::Panel *pParent, const char *pElementName );
	~CBaseBudgetPanel();

	// This should be called when starting up and whenever this data changes.
	void OnConfigDataChanged( const CBudgetPanelConfigData &data );

	// Call this to reset everything.
	virtual void ResetAll();

	// The derived class should implement this and set the text in the time labels.
	virtual void SetTimeLabelText() {}
	virtual void SetHistoryLabelText() {}

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Paint();
	virtual void PaintBackground();
	virtual void PerformLayout();
	void MarkAsDedicatedServer() { m_bDedicated = true; } // plays nicer as part of a vgui window setup
	bool IsDedicated() const { return m_bDedicated; }

	const double *GetBudgetGroupData( int &nGroups, int &nSamplesPerGroup, int &nSampleOffset ) const;
	
	void GetGraphLabelScreenSpaceTopAndBottom( int id, int &top, int &bottom );

	// What percentage is the specified value of the (bargraph) range?
	float GetBudgetGroupPercent( float value );

	// Get the current config data.
	const CBudgetPanelConfigData& GetConfigData() const;

	// Returns the number of budget groups in the last OnConfigDataChanged call.
	int GetNumCachedBudgetGroups() const;

	// (Used by dedicated server, mark everything for a repaint).
	void MarkForFullRepaint();

protected:
	void UpdateWindowGeometry();
	void ClearTimesForAllGroupsForThisFrame( void );
	void ClearAllTimesForGroup( int groupID );
	void Rebuild( const CBudgetPanelConfigData &data );
	
protected:
	int m_BudgetHistoryOffset;

	// This defines all the positioning, label names, etc.
	CBudgetPanelConfigData m_ConfigData;
	
	CUtlVector<vgui::Label *> m_GraphLabels;
	CUtlVector<vgui::Label *> m_TimeLabels;
	CUtlVector<vgui::Label *> m_HistoryLabels;
	
	CBudgetHistoryPanel *m_pBudgetHistoryPanel;
	CBudgetBarGraphPanel *m_pBudgetBarGraphPanel;

	struct BudgetGroupTimeData_t
	{
		double m_Time[BUDGET_HISTORY_COUNT];
	};
	CUtlVector<BudgetGroupTimeData_t> m_BudgetGroupTimes; // [m_CachedNumBudgetGroups][BUDGET_HISTORY_COUNT]
	int m_CachedNumTimeLabels;
	vgui::HFont		m_hFont;

	bool m_bDedicated;
};


inline const CBudgetPanelConfigData& CBaseBudgetPanel::GetConfigData() const
{
	return m_ConfigData;
}

inline int CBaseBudgetPanel::GetNumCachedBudgetGroups() const
{
	return m_ConfigData.m_BudgetGroupInfo.Count();
}


#endif // VGUI_BASEBUDGETPANEL_H
