//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_SLIDER_H
#define PANORAMA_SLIDER_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/controls/panel2d.h"

namespace panorama
{

DECLARE_PANEL_EVENT1( SliderValueChanged, float );
DECLARE_PANEL_EVENT1( SlottedSliderValueChanged, int );
DECLARE_PANEL_EVENT1( SliderFocusChanged, bool );

//-----------------------------------------------------------------------------
// Purpose: Slider control which includes track, progress & thumb
//-----------------------------------------------------------------------------
class CSlider: public CPanel2D
{
	DECLARE_PANEL2D( CSlider, CPanel2D );

public:
	CSlider( CPanel2D *pParent, const char *pchID );
	virtual ~CSlider();

	enum ESliderDirection
	{
		k_EDirectionVertical,
		k_EDirectionHorizontal
	};

	void SetMin( float flMin ) { m_flMin = flMin; InvalidateSizeAndPosition(); }
	void SetMax( float flMax ) { m_flMax = flMax; InvalidateSizeAndPosition(); }
	void SetIncrement( float flValue ) { m_flIncrement = flValue; }
	virtual void SetValue( float flValue );
	float GetValue() { return m_flCur; }
	float GetDefaultValue() { return m_flDefault; }
	void SetDefaultValue ( float flValue ) { m_flDefault = flValue; }
	void SetShowDefaultValue( bool bShow ) { m_bShowDefault = bShow; }
	void SetDirection( ESliderDirection eValue );

	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue );

	virtual bool OnMouseButtonDown( const MouseData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonUp( const MouseData_t &code ) OVERRIDE;
	virtual void OnMouseMove( float flMouseX, float flMouseY ) OVERRIDE;
	virtual bool OnMoveUp( int nRepeats ) OVERRIDE;
	virtual bool OnMoveRight( int nRepeats ) OVERRIDE;
	virtual bool OnMoveDown( int nRepeats ) OVERRIDE;
	virtual bool OnMoveLeft( int nRepeats ) OVERRIDE;

	virtual bool OnActivate(panorama::EPanelEventSource_t eSource);
	virtual bool OnCancel(panorama::EPanelEventSource_t eSource);
	virtual void OnStyleFlagsChanged();
	virtual void OnResetToDefaultValue();

	void SetRequiresSelection( bool bRequireSelection ) { m_bRequiresSelection = bRequireSelection; }

protected:
	bool EventPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );
	void SetValueFromMouse( float x, float y );
	float GetMin() { return m_flMin; }
	float GetMax() { return m_flMax; }
	ESliderDirection GetDirection() { return m_eDirection; }

	CPanel2D *m_pThumb;
	CPanel2D *m_pTrack;
	CPanel2D *m_pProgress;
	CPanel2D *m_pDefaultTick;
	bool EventActivated( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource );
	bool EventCancelled( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource );
	bool EventStyleFlagsChanged( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );
	bool EventResetToDefault( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );

private:
	bool AllowInteraction( void );
	bool ShouldShowDefault( void ) { return m_bShowDefault; }

	float m_flMin;
	float m_flMax;
	float m_flDefault;
	float m_flCur;
	float m_flLast;
	float m_flIncrement;
	bool m_bRequiresSelection;
	bool m_bDraggingThumb;
	bool m_bShowDefault;
	ESliderDirection m_eDirection;

	float m_flLastMouseX;
	float m_flLastMouseY;
};

class CSlottedSlider : public CSlider
{
	DECLARE_PANEL2D( CSlottedSlider, CSlider );
public:
	CSlottedSlider( CPanel2D *pParent, const char *pchID );
	virtual ~CSlottedSlider();

	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue );
	virtual void SetValue( float flValue );
	void SetValue( int nValue );
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );
	int GetCurrentNotch() { return m_nCurNotch; }

private:
	int m_nNumNotches;
	int m_nCurNotch;
	CUtlVector< CPanel2D* > m_pNotches;
};

} // namespace panorama

#endif // PANORAMA_SLIDER_H
