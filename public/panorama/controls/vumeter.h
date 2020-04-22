//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef VUMETER_H
#define VUMETER_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/controls/panel2d.h"

namespace panorama
{

DECLARE_PANEL_EVENT1( VUMeterBarsChanged, int );

//////////////////////////////////////////////////////////////////////////
//
// volume bars control for volume/mic levels
//
class CVUMeter: public panorama::CPanel2D
{
	DECLARE_PANEL2D( CVUMeter, panorama::CPanel2D );
public:
	CVUMeter( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CVUMeter();

	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;

	virtual void OnInitializedFromLayout();

	int GetNumActiveBars() const { return m_numActive; }
	void SetNumActiveBars( int numActive );

	int GetNumBarsTotal() const { return m_numBars; }

	virtual bool OnMoveLeft( int cRepeats );
	virtual bool OnMoveRight( int cRepeats );

	// Override these to avoid focus slipping away when setting with analog
	virtual bool OnMoveUp( int nRepeats );
	virtual bool OnMoveDown( int nRepeats );

	virtual bool OnMouseButtonUp(const MouseData_t &code);
	virtual bool OnMouseWheel(const MouseData_t &code);
	virtual void OnMouseMove(float flMouseX, float flMouseY);

	virtual bool OnActivate(panorama::EPanelEventSource_t eSource);
	virtual bool OnCancel(panorama::EPanelEventSource_t eSource);
	virtual void OnStyleFlagsChanged();

	// if VU meter is "writable," it will be a tab stop, and be focusable. when activated
	// it will enter a mode where you can set the bar with the dpad. if VU meter is not
	// writable, it just displays a value.
	void SetWritable( bool bWritable );

	bool EventActivated( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource );
	bool EventCancelled( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource );
	bool EventStyleFlagsChanged( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE;
#endif

protected:
	bool OnLeftRight( int dx );

	bool m_bWritable;
	int m_numBars, m_numActive;
	CPanoramaSymbol m_symBarPanelType;
	CPanoramaSymbol m_symBarPanelAddClass;
	CPanoramaSymbol m_symBarPanelActiveClass;
	CUtlVector< panorama::CPanel2D * > m_arrBars;

	float m_flLastMouseX;
	float m_flLastMouseY;
};

} // namespace panorama

#endif // VUMETER_H


