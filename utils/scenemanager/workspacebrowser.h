//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WORKSPACEBROWSER_H
#define WORKSPACEBROWSER_H
#ifdef _WIN32
#pragma once
#endif

#include "mxtk/mxTreeView.h"
#include "commctrl.h"

class CWorkspace;
class CProject;
class CScene;
class CVCDFile;
class CSoundEntry;

class CBrowserTree;
class ITreeItem;
class CWorkspaceManager;

class CWorkspaceBrowser : public mxWindow
{
	typedef mxWindow BaseClass;
public:

	CWorkspaceBrowser( mxWindow *parent, CWorkspaceManager *manager, int id );

	CWorkspace	*GetWorkspace();
	void		SetWorkspace( CWorkspace *w );
	void		AddProject( CProject *project );

	virtual		int handleEvent( mxEvent *event );

	ITreeItem	*GetSelectedItem();

	void		PopulateTree();

	static int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);


	void		JumpTo( ITreeItem *item );

private:

	CWorkspaceManager *GetManager();

	void		OnTreeItemSelected( int x, int y, bool rightmouse, bool doubleclick );

	CWorkspace	*m_pCurrentWorkspace;

	CBrowserTree *m_pTree;

	enum
	{
		NUM_BITMAPS = 12,
	};

	ITreeItem	*m_pLastSelected;
	CWorkspaceManager *m_pManager;
};

#endif // WORKSPACEBROWSER_H
