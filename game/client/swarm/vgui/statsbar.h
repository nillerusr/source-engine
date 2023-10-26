#ifndef _INCLUDED_STATSBAR_H
#define _INCLUDED_STATSBAR_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/PHandle.h>

namespace vgui
{
	class Label;
};

// this class shows a number over a bar
//   both number and bar tick up from zero at the specified rate

class StatsBar : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( StatsBar, vgui::Panel );
public:
	StatsBar(vgui::Panel *parent, const char *name);
	
	virtual void OnThink();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void Init(float fCurrent, float fTarget, float fIncreaseRate, bool bDisplayInteger, bool bDisplayPercentage);
	virtual void SetStartCountingTime(float fTime) { m_fStartTime = fTime; }
	void AddMinMax( float flBarMin, float flBarMax );
	void ClearMinMax();
	void UseExternalCounter( vgui::Label *pCounter );
	void SetColors(Color text, Color bar, Color increasebar, Color unusedbar, Color background);
	void SetShowMaxOnCounter( bool bShowMax ) { m_bShowMaxOnCounter = bShowMax; }
	
	vgui::Label *m_pLabel;
	vgui::DHANDLE<vgui::Label> m_hExternalCounter;
	vgui::Panel *m_pBar;
	vgui::Panel *m_pIncreaseBar;
	vgui::Panel *m_pUnusedBar;
	CSoundPatch		*m_pLoopingSound;

	// find the min/max which contains the current value
	float GetBarMin();
	float GetBarMax();

	bool IsDoneAnimating( void ) { return m_fCurrent == m_fTarget; }
	void SetUpdateInterval( float flUpdateInterval ) { m_flUpdateInterval = m_flNextUpdateTime; }

	bool m_bShowCumulativeTotal;

	float m_fStart;
	float m_fTarget;
	float m_fCurrent;
	float m_fIncreaseRate;
	bool m_bDisplayInteger;
	bool m_bDisplayPercentage;
	bool m_bDisplayTime;
	float m_flBorder;
	
	bool m_bInit;
	bool m_bShowMaxOnCounter;
	float m_fStartTime;
	float m_flNextUpdateTime;
	float m_flUpdateInterval;

	// colors
	Color m_TextColor;
	Color m_BarColor;
	Color m_IncreaseBarColor;
	Color m_BackgroundColor;
	Color m_UnusedColor;

	// list of future bar max's for looping bars (e.g. XP bar)
	struct StatsBarMinMax
	{
		float flMin;
		float flMax;
	};
	CUtlVector<StatsBarMinMax> m_Bounds;
};

#endif // _INCLUDED_STATSBAR_H