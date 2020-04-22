//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "scene.h"
#include "vcdfile.h"
#include "project.h"
#include "workspacemanager.h"
#include "workspacebrowser.h"

CScene::CScene( CProject *proj, char const *name ) : m_pOwner( proj )
{
	Q_strncpy( m_szName, name, sizeof( m_szName ) );
	m_pszComments = NULL;
}

CScene::~CScene()
{
	while ( m_Files.Count() > 0 )
	{
		CVCDFile *f = m_Files[ 0 ];
		m_Files.Remove( 0 );
		delete f;
	}
	delete[] m_pszComments;
}

CProject *CScene::GetOwnerProject()
{
	return m_pOwner;
}

void CScene::SetComments( char const *comments )
{
	delete[] m_pszComments;
	m_pszComments = V_strdup( comments );

	GetOwnerProject()->SetDirty( true );
}

char const *CScene::GetComments( void ) const
{
	return m_pszComments ? m_pszComments : "";
}

char const *CScene::GetName() const
{
	return m_szName;
}

int CScene::GetVCDCount() const
{
	return m_Files.Count();
}

CVCDFile *CScene::GetVCD( int index )
{
	if ( index < 0 || index >= m_Files.Count() )
		return NULL;
	return m_Files[ index ];
}

void CScene::AddVCD( CVCDFile *vcd )
{
	Assert( m_Files.Find( vcd ) == m_Files.InvalidIndex() );

	m_Files.AddToTail( vcd );

	GetOwnerProject()->SetDirty( true );
}

void CScene::RemoveVCD( CVCDFile *vcd )
{
	if ( m_Files.Find( vcd ) == m_Files.InvalidIndex() )
		return;

	m_Files.FindAndRemove( vcd );

	GetOwnerProject()->SetDirty( true );
}

CVCDFile *CScene::FindVCD( char const *filename )
{
	int c = GetVCDCount();
	CVCDFile *vcd;
	for ( int i = 0; i < c; i++ )
	{
		vcd = GetVCD( i );
		if ( !vcd )
			continue;

		if ( !Q_stricmp( filename, vcd->GetName() ) )
			return vcd;
	}
	return NULL;
}

void CScene::ValidateTree( mxTreeView *tree, mxTreeViewItem* parent )
{
	CUtlVector< mxTreeViewItem * >	m_KnownItems;

	int c = GetVCDCount();
	CVCDFile *vcd;
	for ( int i = 0; i < c; i++ )
	{
		vcd = GetVCD( i );
		if ( !vcd )
			continue;

		char sz[ 256 ];
		if ( vcd->GetComments() && vcd->GetComments()[0] )
		{
			Q_snprintf( sz, sizeof( sz ), "%s : %s", vcd->GetName(), vcd->GetComments() );
		}
		else
		{
			Q_snprintf( sz, sizeof( sz ), "%s", vcd->GetName() );
		}

		mxTreeViewItem *spot = vcd->FindItem( tree, parent );
		if ( !spot )
		{
			spot = tree->add( parent, sz );
		}

		m_KnownItems.AddToTail( spot );

		vcd->SetOrdinal( i );

		tree->setLabel( spot, sz );

		tree->setImages( spot, vcd->GetIconIndex(), vcd->GetIconIndex() );
		tree->setUserData( spot, vcd );
		//tree->setOpen( spot, vcd->IsExpanded() );

		vcd->ValidateTree( tree, spot );
	}

	mxTreeViewItem *start = tree->getFirstChild( parent );
	while ( start )
	{
		mxTreeViewItem *next = tree->getNextChild( start );

		if ( m_KnownItems.Find( start ) == m_KnownItems.InvalidIndex() )
		{
			tree->remove( start );
		}

		start = next;
	}

	tree->sortTree( parent, true, CWorkspaceBrowser::CompareFunc, 0 );
}

bool CScene::IsCheckedOut() const
{
	return false;
}

int CScene::GetIconIndex() const
{
	/*
	if ( IsCheckedOut() )
	{
		return IMAGE_SCENE_CHECKEDOUT;
	}
	else
	*/
	{
		return IMAGE_SCENE;
	}
}

void CScene::Checkout(bool updatestateicons /*= true*/)
{
	// Scenes aren't made for checkin / checkout
}

void CScene::Checkin(bool updatestateicons /*= true*/)
{
	// Scenes aren't made for checkin / checkout
}

void CScene::MoveChildUp( ITreeItem *child )
{
	int c = GetVCDCount();
	for ( int i = 1; i < c; i++ )
	{
		CVCDFile *p = GetVCD( i );
		if ( p != child )
			continue;

		CVCDFile *prev = GetVCD( i - 1 );
		// Swap
		m_Files[ i - 1 ] = p;
		m_Files[ i ] = prev;
		return;
	}
}

void CScene::MoveChildDown( ITreeItem *child )
{
	int c = GetVCDCount();
	for ( int i = 0; i < c - 1; i++ )
	{
		CVCDFile *p = GetVCD( i );
		if ( p != child )
			continue;

		CVCDFile *next = GetVCD( i + 1 );
		// Swap
		m_Files[ i ]     = next;
		m_Files[ i + 1 ] = p;
		return;
	}
}

bool CScene::IsChildFirst( ITreeItem *child )
{
	int idx = m_Files.Find( (CVCDFile *)child );
	if ( idx == m_Files.InvalidIndex() )
		return false;

	if ( idx != 0 )
		return false;

	return true;
}

bool CScene::IsChildLast( ITreeItem *child )
{
	int idx = m_Files.Find( (CVCDFile *)child );
	if ( idx == m_Files.InvalidIndex() )
		return false;

	if ( idx != m_Files.Count() - 1 )
		return false;

	return true;
}
