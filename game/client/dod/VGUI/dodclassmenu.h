//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DODCLASSMENU_H
#define DODCLASSMENU_H
#ifdef _WIN32
#pragma once
#endif

#include <classmenu.h>
#include <vgui_controls/EditablePanel.h>
#include <filesystem.h>
#include <dod_shareddefs.h>
#include "cbase.h"
#include "dod_gamerules.h"
#include "dodmenubackground.h"
#include "dodbutton.h"
#include "imagemouseoverbutton.h"
#include "IconPanel.h"
#include <vgui_controls/CheckButton.h>

using namespace vgui;

#define NUM_CLASSES	6

class CDODClassMenu : public CClassMenu
{
private:
	DECLARE_CLASS_SIMPLE( CDODClassMenu, CClassMenu );

public:
	CDODClassMenu(IViewPort *pViewPort);

	virtual void Update( void );
	virtual Panel *CreateControlByName( const char *controlName );
	virtual void OnTick( void );
	virtual void PaintBackground( void );
	virtual void OnKeyCodePressed(KeyCode code);
	virtual void SetVisible( bool state );

	MESSAGE_FUNC_CHARPTR( OnShowPage, "ShowPage", page );

	virtual void ShowPanel(bool bShow);

	void UpdateNumClassLabel( void );

	virtual int GetTeamNumber( void ) = 0;

#ifdef REFRESH_CLASSMENU_TOOL
	MESSAGE_FUNC( OnRefreshClassMenu, "refresh_classes" );
#endif

	MESSAGE_FUNC_PTR( OnSuicideOptionChanged, "CheckButtonChecked", panel );

private:
	CDODClassInfoPanel *m_pClassInfoPanel;
	CDODMenuBackground *m_pBackground;
	CheckButton *m_pSuicideOption;

	CImageMouseOverButton<CDODClassInfoPanel> *m_pInitialButton;
	int m_iActivePlayerClass;
	int m_iLastPlayerClassCount;
	int	m_iLastClassLimit;

	ButtonCode_t m_iClassMenuKey;

	vgui::Label *m_pClassNumLabel[NUM_CLASSES];
	vgui::Label *m_pClassFullLabel[NUM_CLASSES];
};

//-----------------------------------------------------------------------------
// Purpose: Draws the U.S. class menu
//-----------------------------------------------------------------------------

class CDODClassMenu_Allies : public CDODClassMenu
{
private:
	DECLARE_CLASS_SIMPLE( CDODClassMenu_Allies, CDODClassMenu );
	
public:
	CDODClassMenu_Allies(IViewPort *pViewPort) : BaseClass(pViewPort)
	{
		LoadControlSettings( "Resource/UI/ClassMenu_Allies.res" );
	}
	
	virtual const char *GetName( void )
	{ 
		return PANEL_CLASS_ALLIES; 
	}

	virtual int GetTeamNumber( void )
	{
		return TEAM_ALLIES;
	}

};


//-----------------------------------------------------------------------------
// Purpose: Draws the Wermacht class menu
//-----------------------------------------------------------------------------

class CDODClassMenu_Axis : public CDODClassMenu
{
private:
	DECLARE_CLASS_SIMPLE( CDODClassMenu_Axis, CDODClassMenu );
	
public:
	CDODClassMenu_Axis(IViewPort *pViewPort) : BaseClass(pViewPort)
	{
		LoadControlSettings( "Resource/UI/ClassMenu_Axis.res" );
	}

	virtual const char *GetName( void )
	{ 
		return PANEL_CLASS_AXIS;
	}

	virtual int GetTeamNumber( void )
	{
		return TEAM_AXIS;
	}
};

#endif // DODCLASSMENU_H
