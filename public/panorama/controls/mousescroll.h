//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef MOUSESCROLL_H
#define MOUSESCROLL_H

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/mathlib.h"
#include "mathlib/beziercurve.h"
#include "panel2d.h"
#include "panorama/controls/label.h"
#include "panorama/controls/mousescroll.h"
#include "panorama/uischeduleddel.h"


namespace panorama
{
DECLARE_PANEL_EVENT1( MouseScroll, int );

//-----------------------------------------------------------------------------
// Purpose: Panel which is the clickable mouse region to scroll a carousel or other horizontal list type panel
//-----------------------------------------------------------------------------
class CMouseScrollRegion : public CPanel2D
{
	DECLARE_PANEL2D( CMouseScrollRegion, CPanel2D );

public:
	CMouseScrollRegion( CPanel2D *parent, const char * pchPanelID );
	virtual ~CMouseScrollRegion();
	
	virtual bool OnMouseButtonDown( const MouseData_t &code );
	virtual bool OnMouseButtonDoubleClick( const MouseData_t &code );
	virtual bool OnMouseButtonTripleClick( const MouseData_t &code );
	virtual bool OnMouseButtonUp( const MouseData_t &code );

private:
	void DispatchScrollEvent();
	void MouseButtonDown();	

	CCubicBezierCurve< Vector2D > m_repeatCurve;
	panorama::CUIScheduledDel m_scheduledScrollRepeat;
	double m_flMouseDownTimestamp;
	int m_cMouseDownRepeats;
};

} // namespace panorama

#endif // MOUSESCROLL_H
