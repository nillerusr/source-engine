//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_DROPDOWN_H
#define PANORAMA_DROPDOWN_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"

namespace panorama
{

DECLARE_PANEL_EVENT0( DropDownSelectionChanged );
DECLARE_PANORAMA_EVENT2( DropDownMenuClosed, bool, CPanelPtr< CPanel2D > );

class CDropDownMenu;

//-----------------------------------------------------------------------------
// Purpose: Drop Down Control
//-----------------------------------------------------------------------------
class CDropDown : public CPanel2D
{
	DECLARE_PANEL2D( CDropDown, CPanel2D );

public:
	CDropDown( CPanel2D *parent, const char * pchPanelID );
	virtual ~CDropDown();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );
	void AddOption( CPanel2D *pPanel );
	bool HasOption( const char *pchID );
	void RemoveOption( const char *pchID );
	void RemoveAllOptions();

	void FindOptionIDsByClass( const char *pchClassName, CUtlVector< CUtlString > &vecIDsOut );
	void SortOptions( int( __cdecl *pfnCompare )(const ClientPanelPtr_t *, const ClientPanelPtr_t *) );

	CPanel2D *GetSelected() { return m_pSelected.Get(); }
	void SetSelected( const char *pchID, bool bNotify );
	void SetSelected( const char *pchID ) { return SetSelected( pchID, true ); }
	void SetDefault( const char *pchID ) { m_strDefaultSelection.Set( pchID ); }
	void ResetToDefault( bool bNotify );

	CPanel2D *AccessDropDownMenu() { return (CPanel2D*)m_pMenu.Get(); }

	virtual bool OnMouseButtonDown( const MouseData_t &mouseData );
	virtual bool OnClick( IUIPanel *pPanel, const MouseData_t &code );
	virtual void OnResetToDefaultValue();
	
	// Call if you know you've changed the contents of one of the option panels
	void InvalidateOptions( bool bForceReload );

	CPanel2D *FindDropDownMenuChild( const char *pchID );
	virtual bool BIsClientPanelEvent( CPanoramaSymbol symProperty ) OVERRIDE;

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE;
#endif

protected:
	bool EventPanelActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	bool EventDropDownMenuClosed( bool bSelectionChanged, CPanelPtr< CPanel2D > pPanel );
	bool EventResetToDefault( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );

	virtual void OnInitializedFromLayout();
	void ShowMenu();
	void UpdateSelectedChild( bool bSuppressChangedEvent, bool bInvalidateAlways = false );

	virtual void OnStylesChanged() OVERRIDE;

	CPanelPtr< CDropDownMenu > GetMenu() { return m_pMenu; }

private:
	CPanelPtr< CDropDownMenu >m_pMenu;
	CPanelPtr<CPanel2D> m_pSelected;
	bool m_bSuppressClick;

	CUtlString m_strInitialSelection;
	CUtlString m_strDefaultSelection;
};


//-----------------------------------------------------------------------------
// Purpose: Drop Down Menu (shown when activated)
//-----------------------------------------------------------------------------
class CDropDownMenu : public CPanel2D
{
	DECLARE_PANEL2D( CDropDownMenu, CPanel2D );

public:
	CDropDownMenu( CDropDown *pDropDown, const char * pchPanelID );
	virtual ~CDropDownMenu();

	void Show();
	void Close() { Hide( false ); }

	CPanel2D *GetSelectedChild();
	void SetSelected( const char *pchID );
	void AddOption( CPanel2D *pPanel );
	bool HasOption( const char *pchID );
	void RemoveOption( const char *pchID );
	void RemoveAll();

	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );
	virtual void OnResetToDefaultValue();
	virtual IUIPanel *GetLocalizationParent() const { return m_pDropDown->UIPanel(); }

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE;
#endif

protected:
	bool EventPanelActivated( const CPanelPtr< IUIPanel > &ptrPanel, EPanelEventSource_t eSource );
	bool EventCancelled( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource );
	bool EventInputFocusTopLevelChanged( CPanelPtr< CPanel2D > ptrPanel );
	bool EventResetToDefault( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );
	bool EventInputFocusSet( const panorama::CPanelPtr< IUIPanel > &ptrPanel );

private:
	void Hide( bool bSelectionChanged );

	CDropDown *m_pDropDown;
	CPanel2D *m_pSelectedChild;
};


} // namespace panorama

#endif // PANORAMA_DROPDOWN_H
