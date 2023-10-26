#ifndef _INCLUDED_STAT_GRAPH_H
#define _INCLUDED_STAT_GRAPH_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>


namespace vgui
{
	class Label;
};


// this class shows a number over a bar
//   both number and bar tick up from zero at the specified rate

class StatGraph : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( StatGraph, vgui::Panel );
public:
	StatGraph( vgui::Panel *parent, const char *name );
	
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	void SetTimeline( CTimeline *pTimeline );
	void SetVerticalPixelSeparation( int nNumPixels ) { m_nVerticalPixelSeparation = nNumPixels; }

	void SetMinValue( float flMinValue );
	void SetMaxValue( float flMaxValue );
	void SetMinMaxValues( float flMinValue, float flMaxValue );
	void SetLineColor( Color lineColor );
	void SetHighlight( bool bHighlight );

	bool HasPresented( void );
	bool IsPresenting( void );

	float GetMinValue( void ) { return m_fMinValue; }
	float GetMaxValue( void ) { return m_fMaxValue; }
	float GetCrestValue( void ) { return m_flCrestValue; }
	float GetTroughValue( void ) { return m_flTroughValue; }
	float GetAverageValue( void ) { return m_flAverageValue; }
	float GetFinalValue( void );

	float GetStartTime( void );
	float GetEndTime( void );

	virtual void Paint();

private:
	void CalculateMinMaxValues( void );

	CTimeline *m_pTimeline;

	float m_flCrestValue;
	float m_flTroughValue;
	float m_flAverageValue;

	float m_fMinValue;
	float m_fMaxValue;

	int m_nNumTimelineValues;
	int m_nVerticalPixelSeparation;
	
	// colors
	Color m_BarColor;

	bool m_bHighlight;
};

#endif // _STATS_GRAPH_H