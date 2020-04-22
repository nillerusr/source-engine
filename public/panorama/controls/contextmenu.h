//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CONTEXTMENU_H
#define CONTEXTMENU_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/controls/panel2d.h"

DECLARE_PANEL_EVENT1( ContextMenuEvent, const char * )
DECLARE_PANEL_EVENT1( ContextMenuEventDirect, panorama::IUIEvent * );

namespace panorama 
{

//-----------------------------------------------------------------------------
// Purpose: Helper class to derive from for creating context menus
//-----------------------------------------------------------------------------
class CContextMenu : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CContextMenu, panorama::CPanel2D );

public:
	CContextMenu( CPanel2D *pParent, const char *pchName, CPanel2D *pEventParent );
	CContextMenu( IUIWindow *pParent, const char *pchName, CPanel2D *pEventParent );
	virtual ~CContextMenu();
	virtual bool OnClick( IUIPanel *pPanel, const panorama::MouseData_t &code );

	void SetMenuTarget( const CPanelPtr< IUIPanel >& targetPanelPtr );

	void CalculatePosition() { m_bReposition = true; InvalidateSizeAndPosition(); }

protected:
	CPanel2D *GetEventParent() { return m_pEventParent; }

private:
	void Initialize( CPanel2D *pEventParent );

	void OnLayoutTraverse( float flFinalWidth, float flFinalHeight );

	bool OnFireEvent( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, const char *pchEventText );
	bool OnFireEvent( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, IUIEvent *pEvent );
	bool OnCancelled( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource );

	CPanel2D *m_pEventParent;
	CPanelPtr< IUIPanel > m_pMenuTarget;
	double m_flCreateTime;
	bool m_bReposition;
};


//-----------------------------------------------------------------------------
// Purpose: Helper class for simple context menus that doesn't require derivation
//-----------------------------------------------------------------------------
class CSimpleContextMenu : public panorama::CContextMenu
{
	DECLARE_PANEL2D( CSimpleContextMenu, panorama::CContextMenu );

public:
	CSimpleContextMenu( CPanel2D *pParent, const char *pchName, CPanel2D *pEventParent  );
	CSimpleContextMenu( IUIWindow *pParent, const char *pchName, CPanel2D *pEventParent  );
	virtual ~CSimpleContextMenu();

	void AddMenuItem( const char *pchLabelText, const char *pchEventText );
	void AddMenuItemEvent( const char *pchLabel, IUIEvent *pEvent );

private:

};

} // namespace panorama

#endif // CONTEXTMENU_H