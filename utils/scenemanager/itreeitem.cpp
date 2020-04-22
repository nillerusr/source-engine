//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "itreeitem.h"
#include "mxtk/mxTreeView.h"
#include "project.h"
#include "scene.h"
#include "soundentry.h"
#include "vcdfile.h"
#include "wavefile.h"
#include "workspace.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *tree - 
//			*parent - 
// Output : mxTreeViewItem
//-----------------------------------------------------------------------------
mxTreeViewItem *ITreeItem::FindItem( mxTreeView *tree, mxTreeViewItem *parent, bool recurse )
{
	if ( !tree )
		return NULL;

	mxTreeViewItem *child = tree->getFirstChild( parent );
	while ( child )
	{
		ITreeItem *treeItem = (ITreeItem *)tree->getUserData( child );
		if ( treeItem )
		{
			if ( treeItem == this )
			{
				return child;
			}

			if ( recurse )
			{
				mxTreeViewItem *found = FindItem( tree, child, recurse );
				if ( found )
				{
					return found;
				}
			}
		}

		child = tree->getNextChild( child );
	}

	return NULL;
}

ITreeItem *ITreeItem::GetParentItem()
{
	if ( GetSoundEntry() )
	{
		return GetSoundEntry()->GetOwnerVCDFile();
	}

	if ( GetVCDFile() )
	{
		return GetVCDFile()->GetOwnerScene();
	}

	if ( GetScene() )
	{
		return GetScene()->GetOwnerProject();
	}
	
	if ( GetProject() )
	{
		return GetProject()->GetOwnerWorkspace();
	}

	if ( GetWaveFile() )
	{
		return GetWaveFile()->GetOwnerSoundEntry();
	}

	return NULL;
}
