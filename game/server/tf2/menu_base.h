//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef MENU_BASE_H
#define MENU_BASE_H
#pragma once

class CBasePlayer;
class CMenu;

enum
{
	MENU_DEFAULT = 0,
	MENU_TEAM,
	MENU_CLASS,

	// Insert new Menus here
	MENU_LAST,					// Total Number of menus
};

// Global list of menus
extern CMenu	*gMenus[];

//-----------------------------------------------------------------------------
// Purpose: Base Menu Class
//-----------------------------------------------------------------------------
class CMenu
{
public:
	CMenu();

	virtual void RecalculateMenu( CBaseTFPlayer *pViewer );
	virtual void Display( CBaseTFPlayer *pViewer, int allowed = 0xFFFF, int display_time = -1 );
	virtual bool Input( CBaseTFPlayer *pViewer, int iInput );

protected:
	char	m_szMenuString[1024];
};


//-----------------------------------------------------------------------------
// Purpose: Team Menu
//-----------------------------------------------------------------------------
class CMenuTeam : public CMenu
{
public:
	CMenuTeam();

	virtual void RecalculateMenu( CBaseTFPlayer *pViewer );
	virtual bool Input( CBaseTFPlayer *pViewer, int iInput );
};


//-----------------------------------------------------------------------------
// Purpose: Class Menu
//-----------------------------------------------------------------------------
class CMenuClass : public CMenu
{
public:
	CMenuClass();

	virtual void RecalculateMenu( CBaseTFPlayer *pViewer );
	virtual bool Input( CBaseTFPlayer *pViewer, int iInput );
};


#endif // MENU_BASE_H
