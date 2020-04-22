//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef TOOLWINDOWFACTORY_H
#define TOOLWINDOWFACTORY_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/ToolWindow.h"
#include "vgui/IInput.h"

#define TOOLWINDOW_DEFAULT_WIDTH	640
#define TOOLWINDOW_DEFAULT_HEIGHT	480
#define TOOLWINDOW_MIN_WIDTH		120
#define TOOLWINDOW_MIN_HEIGHT		80

template < class T >
class CToolWindowFactory : public vgui::IToolWindowFactory
{
public:
	virtual	vgui::ToolWindow *InstanceToolWindow
	( 
		vgui::Panel *parent,
		bool contextLabel,		// Tool window shows context button for pages with context menus?
		vgui::Panel *firstPage, 
		char const *title,
		bool contextMenu		// Page has context menu
	);
};

//-----------------------------------------------------------------------------
// Methods related to CToolWindowFactory
//-----------------------------------------------------------------------------
template < class T >
vgui::ToolWindow *CToolWindowFactory<T>::InstanceToolWindow( vgui::Panel *parent, bool contextLabel, vgui::Panel *firstPage, char const *title, bool contextMenu )
{
	Assert( parent );
	if ( !parent )
		return NULL;

	int mx, my;
	vgui::input()->GetCursorPos( mx, my );
	parent->ScreenToLocal( mx, my );

	T *container = new T( parent, contextLabel, this, firstPage, title, contextMenu );
	Assert( container );
	if ( container )
	{
		container->SetBounds( mx, my, TOOLWINDOW_DEFAULT_WIDTH, TOOLWINDOW_DEFAULT_HEIGHT );
		container->SetMinimumSize( TOOLWINDOW_MIN_WIDTH, TOOLWINDOW_MIN_HEIGHT );
	}
	return container;
}

#endif // TOOLWINDOWFACTORY_H
