//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "workspace.h"
#include <KeyValues.h>
#include "project.h"
#include "vstdlib/random.h"
#include "cmdlib.h"
#include "FileSystem_Tools.h"
#include "utlbuffer.h"
#include "workspacemanager.h"
#include "workspacebrowser.h"
#include "soundbrowser.h"
#include "wavebrowser.h"

CWorkspace::CWorkspace( char const *filename )
{
	m_szVSSUserName[ 0 ] = 0;
	m_szVSSProject[ 0 ] = 0;

	Q_strncpy( m_szFile, filename, sizeof( m_szFile ) );
	// By default, name is the same as the filename
	Q_FileBase( m_szFile, m_szName, sizeof( m_szName ) );
	m_bDirty = false;
	LoadFromFile();
}

CWorkspace::~CWorkspace()
{
	while ( m_Projects.Count() > 0 )
	{
		CProject *p = m_Projects[ 0 ];
		m_Projects.Remove( 0 );
		delete p;
	}
}

char const	*CWorkspace::GetName() const
{
	return m_szName;
}

bool CWorkspace::IsDirty( void ) const
{
	int c = GetProjectCount();
	for ( int i = 0; i < c; i++ )
	{
		CProject *p = GetProject( i );
		Assert( p );
		if ( p->IsDirty() )
			return true;
	}

	return m_bDirty;
}

void CWorkspace::SetDirty( bool dirty )
{
	m_bDirty = dirty;
}

int CWorkspace::GetProjectCount() const
{
	return m_Projects.Count();
}

CProject *CWorkspace::GetProject( int index ) const
{
	if ( index < 0 || index >= m_Projects.Count() )
		return NULL;
	return m_Projects[ index ];
}

void CWorkspace::RemoveProject( CProject *project )
{
	if ( m_Projects.Find( project ) == m_Projects.InvalidIndex() )
		return;

	m_Projects.FindAndRemove( project );
	SetDirty( true );
}


void CWorkspace::AddProject( CProject *project )
{
	SetDirty( true );

	Assert( m_Projects.Find( project ) == m_Projects.InvalidIndex() );

	m_Projects.AddToTail( project );
}

CProject *CWorkspace::FindProjectFile( char const *filename ) const
{
	int c = GetProjectCount();
	for ( int i = 0 ; i < c; i++ )
	{
		CProject *p = GetProject( i );
		Assert( p );
		if ( !Q_stricmp( p->GetFileName(), filename ) )
			return p;
	}
	return NULL;
}

void CWorkspace::LoadFromFile()
{
	KeyValues *kv = new KeyValues( m_szName );
	if ( kv->LoadFromFile( filesystem, m_szFile ) )
	{
		for ( KeyValues *proj = kv->GetFirstSubKey(); proj; proj = proj->GetNextKey() )
		{
			// Add named projects
			if ( !Q_stricmp( proj->GetName(), "project" ) )
			{
				bool expanded = false;
				char filename[ 256 ];
				filename[0] = 0;

				for ( KeyValues *sub = proj->GetFirstSubKey(); sub; sub = sub->GetNextKey() )
				{
					if ( !Q_stricmp( sub->GetName(), "file" ) )
					{
						Q_strcpy( filename, sub->GetString() );
						continue;
					}
					else if ( !Q_stricmp( sub->GetName(), "expanded" ) )
					{
						expanded = sub->GetInt() ? true : false;
						continue;
					}
					else
					{
						Assert( 0 );
					}
				}

				CProject *p = new CProject( this, filename );
				p->SetExpanded( expanded );
				p->SetDirty( false );
				m_Projects.AddToTail( p );

				continue;
			}
			else if ( !Q_stricmp( proj->GetName(), "vss_username" ) )
			{
				SetVSSUserName( proj->GetString() );

				Con_Printf( "VSS User: '%s'\n", GetVSSUserName() );
				continue;
			}
			else if ( !Q_stricmp( proj->GetName(), "vss_project" ) )
			{
				SetVSSProject( proj->GetString() );

				Con_Printf( "VSS Project:  '%s'\n", GetVSSProject() );
				continue;
			}
			else if ( !Q_stricmp( proj->GetName(), "window" ) )
			{
				SetDirty( true );

				char const *windowname = proj->GetString( "windowname", "" );
				if ( !Q_stricmp( windowname, "workspace" ) )
				{
					SceneManager_LoadWindowPositions( proj, GetWorkspaceManager()->GetBrowser() );
					continue;
				}
				else if ( !Q_stricmp( windowname, "soundbrowser" ) )
				{
					SceneManager_LoadWindowPositions( proj, GetWorkspaceManager()->GetSoundBrowser() );
					continue;
				}
				else if ( !Q_stricmp( windowname, "wavebrowser" ) )
				{
					SceneManager_LoadWindowPositions( proj, GetWorkspaceManager()->GetWaveBrowser() );
					continue;
				}
				else if ( !Q_stricmp( windowname, "main" ) )
				{
					SceneManager_LoadWindowPositions( proj, GetWorkspaceManager() );
					continue;
				}
				else
				{
					Assert( 0 );
				}
			}

			Assert( 0 );
		}
	}
	kv->deleteThis();
}

static void SaveWindowPositions( CUtlBuffer& buf, char const *windowname, mxWindow *wnd )
{
	buf.Printf( "\t\"window\"\n" );
	buf.Printf( "\t{\n" );
	buf.Printf( "\t\twindowname\t\"%s\"\n", windowname );

	SceneManager_SaveWindowPositions( buf, 2, wnd );

	buf.Printf( "\t}\n" );
}

void CWorkspace::SaveToFile()
{
	SetDirty( false );

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	buf.Printf( "%s\n{\n", GetName() );

	// walk projects
	int c = GetProjectCount();
	for ( int i = 0; i < c; i++ )
	{
		CProject *p = GetProject( i );
		Assert( p );

		buf.Printf( "\t\"project\"\n" );
		buf.Printf( "\t{\n" );
		buf.Printf( "\t\t\"expanded\"\t\"%i\"\n", p->IsExpanded() );
		buf.Printf( "\t\t\"file\"\t\"%s\"\n", p->GetFileName() );
		buf.Printf( "\t}\n" );

		p->SaveChanges();
	}

	buf.Printf( "\t\"vss_username\"\t\"%s\"\n", GetVSSUserName() );
	buf.Printf( "\t\"vss_project\"\t\"%s\"\n", GetVSSProject() );

	// Save window positions
	SaveWindowPositions( buf, "main", GetWorkspaceManager() );

	SaveWindowPositions( buf, "workspace", GetWorkspaceManager()->GetBrowser() );
	SaveWindowPositions( buf, "soundbrowser", GetWorkspaceManager()->GetSoundBrowser() );
	SaveWindowPositions( buf, "wavebrowser", GetWorkspaceManager()->GetWaveBrowser() );
	
	buf.Printf( "}\n" );

	if ( filesystem->FileExists( m_szFile ) && !filesystem->IsFileWritable( m_szFile ) )
	{
		int retval = mxMessageBox( NULL, va( "Check out '%s'?", m_szFile ), g_appTitle, MX_MB_YESNOCANCEL );
		if ( retval != 0 )
			return;

		VSS_Checkout( m_szFile );

		if ( !filesystem->IsFileWritable( m_szFile ) )
		{
			mxMessageBox( NULL, va( "Unable to check out'%s'!!!", m_szFile ), g_appTitle, MX_MB_OK );
			return;
		}
	}

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

void CWorkspace::SaveChanges()
{
	if ( !IsDirty() )
		return;

	SaveToFile();
}


void CWorkspace::ValidateTree( mxTreeView *tree, mxTreeViewItem *parent )
{
	CUtlVector< mxTreeViewItem * >	m_KnownItems;

	int c = GetProjectCount();
	CProject *proj;
	for ( int i = 0; i < c; i++ )
	{
		proj = GetProject( i );
		if ( !proj )
			continue;

		char sz[ 256 ];
		if ( proj->GetComments() && proj->GetComments()[0] )
		{
			Q_snprintf( sz, sizeof( sz ), "%s : %s", proj->GetName(), proj->GetComments() );
		}
		else
		{
			Q_strncpy( sz, proj->GetName(), sizeof( sz ) );
		}

		mxTreeViewItem *spot = proj->FindItem( tree, parent );
		if ( !spot )
		{
			spot = tree->add( parent, sz );
		}

		m_KnownItems.AddToTail( spot );

		proj->SetOrdinal ( i );

		tree->setLabel( spot, sz );

		tree->setImages( spot, proj->GetIconIndex(), proj->GetIconIndex() );
		tree->setUserData( spot, proj );
		//tree->setOpen( spot, proj->IsExpanded() );

		proj->ValidateTree( tree, spot );
	}

	// Now check for dangling items
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

bool CWorkspace::CanClose( void )
{
	if ( !IsDirty() )
		return true;

	int retval = mxMessageBox( NULL, va( "Save changes to workspace '%s'?", GetName() ), g_appTitle, MX_MB_YESNOCANCEL );
	if ( retval == 2 )
		return false;

	if ( retval == 0 )
	{
		SaveChanges();
	}

	return true;
}

char const *CWorkspace::GetVSSUserName() const
{
	return m_szVSSUserName;
}
	
char const *CWorkspace::GetVSSProject() const
{
	return m_szVSSProject;
}

void CWorkspace::SetVSSUserName( char const *username )
{
	Q_strncpy( m_szVSSUserName, username, sizeof( m_szVSSUserName ) );
}

void CWorkspace::SetVSSProject( char const *projectname )
{
	Q_strncpy( m_szVSSProject, projectname, sizeof( m_szVSSProject ) );
	while ( Q_strlen( m_szVSSProject ) > 0 )
	{
		if ( m_szVSSProject[ Q_strlen( m_szVSSProject ) - 1 ] == '/' ||
			 m_szVSSProject[ Q_strlen( m_szVSSProject ) - 1 ] == '\\' )
		{
			m_szVSSProject[ Q_strlen( m_szVSSProject ) - 1 ] = 0;
		}
		else
		{
			break;
		}
	}
}

void CWorkspace::Checkout( bool updatestateicons /*= true*/ )
{
	VSS_Checkout( GetFileName(), updatestateicons );
}

void CWorkspace::Checkin(bool updatestateicons /*= true*/)
{
	VSS_Checkin( GetFileName(), updatestateicons );
}

bool CWorkspace::IsCheckedOut() const
{
	return filesystem->IsFileWritable( GetFileName() );
}

int CWorkspace::GetIconIndex() const
{
	if ( IsCheckedOut() )
	{
		return IMAGE_WORKSPACE_CHECKEDOUT;
	}
	else
	{
		return IMAGE_WORKSPACE;
	}
}

void CWorkspace::MoveChildUp( ITreeItem *child )
{
	int c = GetProjectCount();
	for ( int i = 1; i < c; i++ )
	{
		CProject *p = GetProject( i );
		if ( p != child )
			continue;

		CProject *prev = GetProject( i - 1 );
		// Swap
		m_Projects[ i - 1 ] = p;
		m_Projects[ i ] = prev;
		return;
	}
}

void CWorkspace::MoveChildDown( ITreeItem *child )
{
	int c = GetProjectCount();
	for ( int i = 0; i < c - 1; i++ )
	{
		CProject *p = GetProject( i );
		if ( p != child )
			continue;

		CProject *next = GetProject( i + 1 );
		// Swap
		m_Projects[ i ]     = next;
		m_Projects[ i + 1 ] = p;
		return;
	}
}

bool CWorkspace::IsChildFirst( ITreeItem *child )
{
	int idx = m_Projects.Find( (CProject *)child );
	if ( idx == m_Projects.InvalidIndex() )
		return false;

	if ( idx != 0 )
		return false;

	return true;
}

bool CWorkspace::IsChildLast( ITreeItem *child )
{
	int idx = m_Projects.Find( (CProject *)child );
	if ( idx == m_Projects.InvalidIndex() )
		return false;

	if ( idx != m_Projects.Count() - 1 )
		return false;

	return true;
}