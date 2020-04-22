//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "workspacebrowser.h"
#include "workspace.h"
#include "project.h"
#include <windows.h>
#include "resource.h"
#include "project.h"
#include "vcdfile.h"
#include "soundentry.h"
#include "scene.h"
#include "workspacemanager.h"

enum
{
	IDC_WSB_TREE = 101,
};

class CBrowserTree : public mxTreeView
{
public:
	CBrowserTree( mxWindow *parent, int id = 0 ) 
		: mxTreeView( parent, 0, 0, 0, 0, id )
	{
		// SendMessage ( (HWND)getHandle(), WM_SETFONT, (WPARAM) (HFONT) GetStockObject (ANSI_FIXED_FONT), MAKELPARAM (TRUE, 0));
	}
};

int CALLBACK CWorkspaceBrowser::CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	ITreeItem const *item1 = (ITreeItem *)lParam1;
	ITreeItem const *item2 = (ITreeItem *)lParam2;

	Assert( item1 && item2 );

	if ( item1->GetOrdinal() < item2->GetOrdinal() )
		return -1;
	else if ( item1->GetOrdinal() > item2->GetOrdinal() )
		return 1;
	
	// Ok, just compare pointers... sigh
	return ( item1 < item2 ) ? -1 : 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
CWorkspaceBrowser::CWorkspaceBrowser( mxWindow *parent, CWorkspaceManager *manager, int id ) :
	BaseClass( parent, 0, 0, 0, 0, "Workspace Browser", id )
{
	m_pManager = manager;

	SceneManager_MakeToolWindow( this, false );

	m_pLastSelected = NULL;

	m_pCurrentWorkspace = NULL;
	m_pTree = new CBrowserTree( this, IDC_WSB_TREE );

	HIMAGELIST list = GetWorkspaceManager()->CreateImageList();

	// Associate the image list with the tree-view control. 
    m_pTree->setImageList( (void *)list ); 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
// Output : int
//-----------------------------------------------------------------------------
int CWorkspaceBrowser::handleEvent( mxEvent *event )
{
	int iret = 0;
	switch ( event->event )
	{
	default:
		break;
	case mxEvent::Action:
		{
			switch ( event->action )
			{
			default:
				break;
			case IDC_WSB_TREE:
				{
					iret = 1;

					bool rightmouse = ( event->flags == mxEvent::RightClicked ) ? true : false;
					bool doubleclicked = ( event->flags == mxEvent::DoubleClicked ) ? true : false;

					OnTreeItemSelected( event->x, event->y, rightmouse, doubleclicked );
				}
				break;
			}
		}
		break;
	case mxEvent::Size:
		{
			m_pTree->setBounds( 0, 0, w2(), h2() );

			GetWorkspaceManager()->SetWorkspaceDirty();

			iret = 1;
		}
		break;
	case mxEvent::Close:
		{
			iret = 1;
		}
		break;
	}

	return iret;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CWorkspace
//-----------------------------------------------------------------------------
CWorkspace *CWorkspaceBrowser::GetWorkspace()
{
	return m_pCurrentWorkspace;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWorkspaceBrowser::PopulateTree()
{
	m_pLastSelected = NULL;

	CWorkspace *w = m_pCurrentWorkspace;

	if ( !w )
		return;

	// Add root element
	char sz[ 128 ];
	if ( w->GetProjectCount() == 1 )
	{
		Q_snprintf( sz, sizeof( sz ), "Workspace '%s': %i project", w->GetName(), w->GetProjectCount() );
	}
	else
	{
		Q_snprintf( sz, sizeof( sz ), "Workspace '%s': %i projects", w->GetName(), w->GetProjectCount() );
	}


	mxTreeViewItem *root = w->FindItem( m_pTree, NULL );
	if ( !root )
	{
		root = m_pTree->add( NULL, sz );
	}

	// Reset the label
	m_pTree->setLabel( root, sz );

	m_pTree->setImages( root, w->GetIconIndex(), w->GetIconIndex() );
	m_pTree->setUserData( root, w );

	w->ValidateTree( m_pTree, root );

	m_pTree->setOpen( root, true );

	m_pTree->sortTree( root, true, CompareFunc, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *w - 
//-----------------------------------------------------------------------------
void CWorkspaceBrowser::SetWorkspace( CWorkspace *w )
{
	m_pCurrentWorkspace = w;

	PopulateTree();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *project - 
//-----------------------------------------------------------------------------
void CWorkspaceBrowser::AddProject( CProject *project )
{
	if ( !m_pCurrentWorkspace )
		return;

	m_pCurrentWorkspace->AddProject( project );

	PopulateTree();
}

ITreeItem *CWorkspaceBrowser::GetSelectedItem()
{
	return m_pLastSelected;
}

CWorkspaceManager *CWorkspaceBrowser::GetManager()
{
	return m_pManager;
}

void CWorkspaceBrowser::OnTreeItemSelected( int x, int y, bool rightmouse, bool doubleclick )
{
	mxTreeViewItem *item = m_pTree->getSelectedItem();
	if ( !item )
	{
		Con_Printf( "No item selected\n" );
		return;
	}

	ITreeItem *p = (ITreeItem *)m_pTree->getUserData( item );
	if ( !p )
	{
		Con_Printf( "No userdata for item\n" );
		return;
	}

	m_pLastSelected = p;

	GetManager()->UpdateMenus();

	if ( p->GetSoundEntry() || p->GetWaveFile() )
	{
		GetManager()->OnSoundShowInBrowsers();
	}

	if ( !rightmouse )
	{
		if ( doubleclick )
		{
			GetManager()->OnDoubleClicked( GetSelectedItem() );
		}
		return;
	}

	POINT pt;
	GetCursorPos( &pt );

	ScreenToClient( (HWND)GetManager()->getHandle(), &pt );

	GetManager()->ShowContextMenu( pt.x, pt.y, GetSelectedItem() );
}

void CWorkspaceBrowser::JumpTo( ITreeItem *item )
{
	mxTreeViewItem *found = item->FindItem( m_pTree, NULL, true );
	if ( found )
	{
		m_pTree->scrollTo( found );
	}
}