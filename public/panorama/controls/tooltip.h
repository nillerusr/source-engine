//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_TOOLTIP_H
#define PANORAMA_TOOLTIP_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"
#include "../uievent.h"
#include "label.h"

namespace panorama
{

DECLARE_PANEL_EVENT0( TooltipVisible );

//-----------------------------------------------------------------------------
// Purpose: Top level panel for a tooltip
//-----------------------------------------------------------------------------
class CTooltip : public CPanel2D
{
	DECLARE_PANEL2D( CTooltip, CPanel2D );

public:
	CTooltip( IUIWindow *pParent, const char *pchName );
	CTooltip( CPanel2D *pParent, const char *pchName );
	virtual ~CTooltip();

	void SetTooltipTarget( const CPanelPtr< IUIPanel >& targetPanelPtr );

	// Get/set tooltip visibility.  This is actually determined by a .css class,
	// so that transitions can be supported
	bool IsTooltipVisible() const;
	void SetTooltipVisible( bool bVisible );

	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );

	// request the tooltip to do positioning on the next layout
	virtual void CalculatePosition() { m_bReposition = true; InvalidateSizeAndPosition(); }

private:

	void Init();
	void UpdatePosition();

	bool EventTooltipVisible( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );

	bool m_bReposition;

	CPanelPtr< IUIPanel > m_pTooltipTarget;
};


//-----------------------------------------------------------------------------
// Purpose: Simple tooltip that just shows a string of text
//-----------------------------------------------------------------------------
class CTextTooltip : public CTooltip
{
	DECLARE_PANEL2D( CTextTooltip, CTooltip );

public:
	CTextTooltip( IUIWindow *pParent, const char *pchName );
	CTextTooltip( CPanel2D *pParent, const char *pchName );
	virtual ~CTextTooltip();

	void SetText( const char *pchText, CLabel::ETextType eTextType = CLabel::k_ETextTypePlain );

private:
	void Init();

	CLabel *m_pText;
};

} // namespace panorama

#endif // PANORAMA_TOOLTIP_H