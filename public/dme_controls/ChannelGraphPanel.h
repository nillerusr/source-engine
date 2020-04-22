//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CHANNELGRAPHPANEL_H
#define CHANNELGRAPHPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include <vgui_controls/Frame.h>
#include "utllinkedlist.h"
#include "utlvector.h"
#include "movieobjects/dmechannel.h"
#include "datamodel/dmehandle.h"

namespace vgui
{

typedef DmeTime_t (*TimeAccessor_t)();

//-----------------------------------------------------------------------------
// Purpose: Holds and displays a chart of dmechannel data
//-----------------------------------------------------------------------------
class CChannelGraphPanel : public Panel
{
	DECLARE_CLASS_SIMPLE( CChannelGraphPanel, Panel );

public:
	CChannelGraphPanel( Panel *parent, const char *name );

	void SetChannel( CDmeChannel *pChannel );

	// input messages
	virtual void OnCursorMoved( int mx, int my );
	virtual void OnMousePressed( MouseCode code );
	virtual void OnMouseReleased( MouseCode code );
	virtual void OnMouseWheeled( int delta );
	virtual void OnSizeChanged( int newWide, int newTall );	// called after the size of a panel has been changed

protected:
	virtual void Paint();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings( IScheme *pScheme );

	int TimeToPixel( DmeTime_t time );
	int ValueToPixel( float flValue );

private:
	CDmeHandle< CDmeChannel > m_hChannel;
	HFont m_font;
	TimeAccessor_t m_timeFunc;
	DmeTime_t m_graphMinTime, m_graphMaxTime;
	float m_graphMinValue, m_graphMaxValue;
	int m_nMouseStartX, m_nMouseStartY;
	int m_nMouseLastX, m_nMouseLastY;
	int m_nTextBorder;
	int m_nGraphOriginX;
	int m_nGraphOriginY;
	float m_flTimeToPixel;
	float m_flValueToPixel;
};


//-----------------------------------------------------------------------------
// CChannelGraphFrame
//-----------------------------------------------------------------------------
class CChannelGraphFrame : public Frame
{
	DECLARE_CLASS_SIMPLE( CChannelGraphFrame, Frame );

public:
	CChannelGraphFrame( Panel *parent, const char *pTitle );

	void SetChannel( CDmeChannel *pChannel );

	virtual void OnCommand( const char *cmd );
	virtual void PerformLayout();

protected:
	CChannelGraphPanel *m_pChannelGraph;
};

} // namespace vgui

#endif // CHANNELGRAPHPANEL_H
