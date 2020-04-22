//========= Copyright Valve Corporation, All rights reserved. ============//
//
// This menu bar displays a couple extra labels:
// one which contains the tool name, and one which contains a arbitrary info 
//
//=============================================================================

#ifndef TOOLMENUBAR_H
#define TOOLMENUBAR_H

#ifdef _WIN32
#pragma once
#endif


#include "vgui_controls/menubar.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
	class Panel;
	class Label;
}

class CBaseToolSystem;

//-----------------------------------------------------------------------------
// Main menu bar
//-----------------------------------------------------------------------------
class CToolMenuBar : public vgui::MenuBar
{
	DECLARE_CLASS_SIMPLE( CToolMenuBar, vgui::MenuBar );

public:
	CToolMenuBar( CBaseToolSystem *parent, const char *panelName );
	virtual void PerformLayout();
	void SetToolName( const char *name );
	void SetInfo( const char *text );

	CBaseToolSystem *GetToolSystem();

protected:
	Label		*m_pInfo;
	Label		*m_pToolName;
	CBaseToolSystem *m_pToolSystem;
};


//-----------------------------------------------------------------------------
// Main menu bar version that stores file name on it
//-----------------------------------------------------------------------------
class CToolFileMenuBar : public CToolMenuBar
{
	DECLARE_CLASS_SIMPLE( CToolFileMenuBar, CToolMenuBar );

public:
	CToolFileMenuBar( CBaseToolSystem *parent, const char *panelName );
	virtual void PerformLayout();
	void SetFileName( const char *pFileName );

private:
	Label		*m_pFileName;
};


#endif // TOOLMENUBAR_H