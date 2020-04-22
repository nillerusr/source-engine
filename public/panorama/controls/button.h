//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_BUTTON_H
#define PANORAMA_BUTTON_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"
#include "label.h"

namespace panorama
{

class CLabel;

//-----------------------------------------------------------------------------
// Purpose: Button
//-----------------------------------------------------------------------------
class CButton : public CPanel2D
{
	DECLARE_PANEL2D( CButton, CPanel2D );

public:
	CButton( CPanel2D *parent, const char * pchPanelID );
	virtual ~CButton();

	// clone
	virtual bool IsClonable() { return AreChildrenClonable(); }
	virtual CPanel2D *Clone();

	// events
	bool EventActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );

protected:
	virtual void InitClonedPanel( CPanel2D *pPanel );
};


//-----------------------------------------------------------------------------
// Purpose: ToggleButton
//-----------------------------------------------------------------------------
class CToggleButton : public CButton
{
	DECLARE_PANEL2D( CToggleButton, CButton );

public:
	CToggleButton( CPanel2D *parent, const char * pchPanelID );
	virtual ~CToggleButton();

	void SetSelected( bool bSelected );
	void SetText( const char *pchText );
	virtual bool OnKeyTyped( const KeyData_t &unichar );

	// events
	virtual bool EventActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	virtual const char *PchGetText() const { return m_pLabel ? m_pLabel->PchGetText() : ""; }	

protected:
	virtual bool BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties );	

	CPanel2D *m_pButton;
	CLabel *m_pLabel;
	CLabel::ETextType m_eTextType;
};


//-----------------------------------------------------------------------------
// Purpose: RadioButton
//-----------------------------------------------------------------------------
class CRadioButton : public CButton
{
	DECLARE_PANEL2D( CRadioButton, CButton );

public:
	void SetSelected( bool bSelected );
	void SetText( const char *pchText );
	const char *GetGroup() { return m_sGroup.String(); }
	void SetGroup( const char *pchGroup ) { m_sGroup = pchGroup; }

	virtual bool BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties );

public:
	CRadioButton( CPanel2D *parent, const char * pchPanelID );
	virtual ~CRadioButton();

	// events:

	// i have been activated
	bool EventActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );

	// the specified radio, member of the specified group, has been activated (broadcast)
	bool EventOtherActivated( const CPanelPtr< IUIPanel > &pPanel, const char *szGroup );

protected:
	void FireSelectionEvent();

	CPanel2D *m_pButton;
	CLabel *m_pLabel;
	CLabel::ETextType m_eTextType;
	CUtlString m_sGroup;
};

} // namespace panorama

#endif // PANORAMA_BUTTON_H
