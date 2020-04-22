//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "project.h"
#include "cmdlib.h"
#include <KeyValues.h>
#include "scene.h"
#include "UtlBuffer.h"
#include "vcdfile.h"
#include "workspacemanager.h"
#include "workspacebrowser.h"

CProject::CProject( CWorkspace *ws, char const *filename ) : m_pOwner( ws )
{
	Q_strncpy( m_szFile, filename, sizeof( m_szFile ) );
	// By default, name is the same as the filename
	Q_FileBase( m_szFile, m_szName, sizeof( m_szName ) );

	m_bDirty = false;
	m_pszComments = NULL;

	LoadFromFile();
}

CProject::~CProject()
{
	while ( m_Scenes.Count() > 0 )
	{
		CScene *p = m_Scenes[ 0 ];
		m_Scenes.Remove( 0 );
		delete p;
	}
	delete[] m_pszComments;
}

CWorkspace	*CProject::GetOwnerWorkspace()
{
	return m_pOwner;
}

char const *CProject::GetName() const
{
	return m_szName;
}

char const *CProject::GetFileName() const
{
	return m_szFile;
}

bool CProject::IsDirty( void ) const
{
	return m_bDirty;
}

void CProject::SetDirty( bool dirty )
{
	m_bDirty = dirty;
}

void CProject::SetComments( char const *comments )
{
	delete[] m_pszComments;
	m_pszComments = V_strdup( comments );

	SetDirty( true );
}

char const *CProject::GetComments() const
{
	return m_pszComments ? m_pszComments : "";
}

int	 CProject::GetSceneCount() const
{
	return m_Scenes.Count();
}

CScene *CProject::GetScene( int index ) const
{
	if ( index < 0 || index >= m_Scenes.Count() )
		return NULL;
	return m_Scenes[ index ];
}

void CProject::AddScene( CScene *scene )
{
	SetDirty( true );

	Assert( m_Scenes.Find( scene ) == m_Scenes.InvalidIndex() );

	m_Scenes.AddToTail( scene );
}

void CProject::RemoveScene( CScene *scene )
{
	if ( m_Scenes.Find( scene ) == m_Scenes.InvalidIndex() )
		return;

	m_Scenes.FindAndRemove( scene );
	SetDirty( true );
}

void CProject::LoadFromFile()
{
	KeyValues *kv = new KeyValues( m_szName );
	if ( kv->LoadFromFile( filesystem, m_szFile ) )
	{
		for ( KeyValues *s = kv->GetFirstSubKey(); s; s = s->GetNextKey() )
		{
			if ( !Q_stricmp( s->GetName(), "comments" ) )
			{
				SetComments( s->GetString() );
				continue;
			}

			// Add named scenes
			CScene *scene = new CScene( this, s->GetName() );

			for ( KeyValues *sub = s->GetFirstSubKey(); sub; sub = sub->GetNextKey() )
			{
				if ( !Q_stricmp( sub->GetName(), "comments" ) )
				{
					scene->SetComments( sub->GetString() );
					continue;
				}

				if ( !Q_stricmp( sub->GetName(), "expanded" ) )
				{
					scene->SetExpanded( sub->GetInt() ? true : false );
					continue;
				}

				if ( !Q_stricmp( sub->GetName(), "vcd" ) )
				{
					char filename[ 256 ];
					char comments[ 512 ];
					bool expanded = false;

					filename[ 0 ] = 0;
					comments[ 0 ] = 0;

					for ( KeyValues *vcdKeys = sub->GetFirstSubKey(); vcdKeys; vcdKeys = vcdKeys->GetNextKey() )
					{
						if ( !Q_stricmp( vcdKeys->GetName(), "expanded" ) )
						{
							expanded = vcdKeys->GetInt() ? true : false;
							continue;
						}
						else if ( !Q_stricmp( vcdKeys->GetName(), "file" ) )
						{
							Q_strncpy( filename, vcdKeys->GetString(), sizeof( filename ) );
							continue;
						}
						if ( !Q_stricmp( vcdKeys->GetName(), "comments" ) )
						{
							Q_strncpy( comments, vcdKeys->GetString(), sizeof( comments ) );
							continue;
						}

						Assert( 0 );

					}

					CVCDFile *file = new CVCDFile( scene, filename);
					file->SetExpanded( expanded );
					if ( comments[0] )
					{
						file->SetComments( comments );
					}

					scene->AddVCD( file );
					continue;
				}

				Assert( !va( "Unknown scene token %s\n", sub->GetName() ) );
			}

			m_Scenes.AddToTail( scene );
		}
	}
	kv->deleteThis();

	SetDirty( false );
}

void CProject::SaveToFile()
{
	SetDirty( false );

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	buf.Printf( "%s\n{\n", GetName() );

	if ( GetComments() && GetComments()[0] )
	{
		buf.Printf( "\t\"comments\"\t\"%s\"\n",
			GetComments() );
	}

	// walk projects
	int c = GetSceneCount();
	for ( int i = 0; i < c; i++ )
	{
		CScene *scene = GetScene( i );
		Assert( scene );

		buf.Printf( "\t\"%s\"\n", scene->GetName() );
		buf.Printf( "\t{\n" );

		buf.Printf( "\t\t\"expanded\"\t\"%i\"\n", scene->IsExpanded() ? 1 : 0 );

		if ( scene->GetComments() && scene->GetComments()[0] )
		{
			buf.Printf( "\t\t\"comments\"\t\"%s\"\n",
				scene->GetComments() );		
		}

		int vcdcount = scene->GetVCDCount();
		for ( int j = 0; j < vcdcount; j++ )
		{
			CVCDFile *vcd = scene->GetVCD( j );
			Assert( vcd );

			buf.Printf( "\t\tvcd\n" );
			buf.Printf( "\t\t{\n" );

			buf.Printf( "\t\t\t\"expanded\"\t\"%i\"\n",
				vcd->IsExpanded() ? 1 : 0 );
			buf.Printf( "\t\t\t\"file\"\t\"%s\"\n",
				vcd->GetName() );
			if ( vcd->GetComments() && vcd->GetComments()[0] )
			{
				buf.Printf( "\t\t\t\"comments\"\t\"%s\"\n",
					vcd->GetComments() );		
			}

			buf.Printf( "\t\t}\n" );
		}

		buf.Printf( "\t}\n" );

		if ( i != c - 1 )
		{
			buf.Printf( "\n" );
		}
	}

	buf.Printf( "}\n" );

	// Write it out baby
	FileHandle_t fh = filesystem->Open( m_szFile, "wt" );
	if (fh)
	{
		filesystem->Write( buf.Base(), buf.TellPut(), fh );
		filesystem->Close(fh);
	}
	else
	{
		Con_Printf( "CWorkspace::SaveToFile:  Unable to write file %s!!!\n", m_szFile );
	}
}

void CProject::SaveChanges()
{
	if ( !IsDirty() )
		return;

	SaveToFile();
}

void CProject::ValidateTree( mxTreeView *tree, mxTreeViewItem* parent )
{
	CUtlVector< mxTreeViewItem * >	m_KnownItems;

	int c = GetSceneCount();
	CScene *scene;
	for ( int i = 0; i < c; i++ )
	{
		scene = GetScene( i );
		if ( !scene )
			continue;

		char sz[ 256 ];
		if ( scene->GetComments() && scene->GetComments()[0] )
		{
			Q_snprintf( sz, sizeof( sz ) , "%s : %s", scene->GetName(), scene->GetComments() );
		}
		else
		{
			Q_snprintf( sz, sizeof( sz ) , "%s", scene->GetName() );
		}
		

		mxTreeViewItem *spot = scene->FindItem( tree, parent );
		if ( !spot )
		{
			spot = tree->add( parent, sz );
		}

		m_KnownItems.AddToTail( spot );

		scene->SetOrdinal( i );

		tree->setLabel( spot, sz );

		tree->setImages( spot, scene->GetIconIndex(), scene->GetIconIndex() );
		tree->setUserData( spot, scene );
		//tree->setOpen( spot, scene->IsExpanded() );

		scene->ValidateTree( tree, spot );
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

void CProject::Checkout(bool updatestateicons /*= true*/)
{
	VSS_Checkout( GetFileName(), updatestateicons );
}

void CProject::Checkin(bool updatestateicons /*= true*/)
{
	VSS_Checkin( GetFileName(), updatestateicons );
}

bool CProject::IsCheckedOut() const
{
	return filesystem->IsFileWritable( GetFileName() );
}

int CProject::GetIconIndex() const
{
	if ( IsCheckedOut() )
	{
		return IMAGE_PROJECT_CHECKEDOUT;
	}
	else
	{
		return IMAGE_PROJECT;
	}
}

bool CProject::IsChildFirst( ITreeItem *child )
{
	int idx = m_Scenes.Find( (CScene *)child );
	if ( idx == m_Scenes.InvalidIndex() )
		return false;

	if ( idx != 0 )
		return false;

	return true;
}

bool CProject::IsChildLast( ITreeItem *child )
{
	int idx = m_Scenes.Find( (CScene *)child );
	if ( idx == m_Scenes.InvalidIndex() )
		return false;

	if ( idx != m_Scenes.Count() - 1 )
		return false;

	return true;
}

void CProject::MoveChildUp( ITreeItem *child )
{
	int c = GetSceneCount();
	for ( int i = 1; i < c; i++ )
	{
		CScene *p = GetScene( i );
		if ( p != child )
			continue;

		CScene *prev = GetScene( i - 1 );
		// Swap
		m_Scenes[ i - 1 ] = p;
		m_Scenes[ i ] = prev;
		return;
	}
}

void CProject::MoveChildDown( ITreeItem *child )
{
	int c = GetSceneCount();
	for ( int i = 0; i < c - 1; i++ )
	{
		CScene *p = GetScene( i );
		if ( p != child )
			continue;

		CScene *next = GetScene( i + 1 );
		// Swap
		m_Scenes[ i ]     = next;
		m_Scenes[ i + 1 ] = p;
		return;
	}
}
