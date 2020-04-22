//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "vcdfile.h"
#include "soundentry.h"
#include "choreoscene.h"
#include "scriplib.h"
#include "cmdlib.h"
#include "iscenetokenprocessor.h"
#include "choreoevent.h"
#include "scene.h"
#include "project.h"
#include "workspacemanager.h"
#include "workspacebrowser.h"

//-----------------------------------------------------------------------------
// Purpose: Helper to scene module for parsing the .vcd file
//-----------------------------------------------------------------------------
class CSceneTokenProcessor : public ISceneTokenProcessor
{
public:
	const char	*CurrentToken( void );
	bool		GetToken( bool crossline );
	bool		TokenAvailable( void );
	void		Error( const char *fmt, ... );
};

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const
//-----------------------------------------------------------------------------
const char	*CSceneTokenProcessor::CurrentToken( void )
{
	return token;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : crossline - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSceneTokenProcessor::GetToken( bool crossline )
{
	return ::GetToken( crossline ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSceneTokenProcessor::TokenAvailable( void )
{
	return ::TokenAvailable() ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CSceneTokenProcessor::Error( const char *fmt, ... )
{
	char string[ 2048 ];
	va_list argptr;
	va_start( argptr, fmt );
	vsprintf( string, fmt, argptr );
	va_end( argptr );

	Con_ColorPrintf( ERROR_R, ERROR_G, ERROR_B, string );
	::Error( string );
}

static CSceneTokenProcessor g_TokenProcessor;
ISceneTokenProcessor *tokenprocessor = &g_TokenProcessor;

CVCDFile::CVCDFile( CScene *scene, char const *filename ) : m_pOwner( scene )
{
	Q_strncpy( m_szName, filename, sizeof( m_szName ) );

	m_pScene = LoadScene( filename );
	LoadSoundsFromScene( m_pScene );

	m_pszComments = NULL;
}

CVCDFile::~CVCDFile()
{
	while ( m_Sounds.Count() > 0 )
	{
		CSoundEntry *p = m_Sounds[ 0 ];
		m_Sounds.Remove( 0 );
		delete p;
	}
	delete[] m_pszComments;
}

CScene *CVCDFile::GetOwnerScene()
{
	return m_pOwner;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
// Output : CChoreoScene
//-----------------------------------------------------------------------------
CChoreoScene *CVCDFile::LoadScene( char const *filename )
{
	if ( filesystem->FileExists( filename ) )
	{
		char fullpath[ 512 ];
		filesystem->RelativePathToFullPath( filename, "GAME", fullpath, sizeof( fullpath ) );
		LoadScriptFile( fullpath );
		CChoreoScene *scene = ChoreoLoadScene( filename, this, &g_TokenProcessor, Con_Printf );
		return scene;
	}

	return NULL;
}

void CVCDFile::LoadSoundsFromScene( CChoreoScene *scene )
{
	if ( !scene )
		return;

	CChoreoEvent *e;

	int c = scene->GetNumEvents();
	for ( int i = 0; i < c; i++ )
	{
		e = scene->GetEvent( i );
		if ( !e )
			continue;

		if ( e->GetType() != CChoreoEvent::SPEAK )
			continue;

		CSoundEntry *se = new CSoundEntry( this, e->GetParameters() );
		m_Sounds.AddToTail( se );
	}
}

char const *CVCDFile::GetName() const
{
	return m_szName;
}

char const *CVCDFile::GetComments()
{
	return m_pszComments ? m_pszComments : "";
}

void CVCDFile::SetComments( char const *comments )
{
	delete[] m_pszComments;
	m_pszComments = V_strdup( comments );

	if ( GetOwnerScene() )
	{
		if ( GetOwnerScene()->GetOwnerProject() )
		{
			GetOwnerScene()->GetOwnerProject()->SetDirty( true );
		}
	}
}

int CVCDFile::GetSoundEntryCount() const
{
	return m_Sounds.Count();
}

CSoundEntry *CVCDFile::GetSoundEntry( int index )
{
	if ( index < 0 || index >= m_Sounds.Count() )
		return NULL;
	return m_Sounds[ index ];
}

void CVCDFile::ValidateTree( mxTreeView *tree, mxTreeViewItem* parent )
{
	CUtlVector< mxTreeViewItem * >	m_KnownItems;

	int c = GetSoundEntryCount();
	CSoundEntry *sound;
	for ( int i = 0; i < c; i++ )
	{
		sound = GetSoundEntry( i );
		if ( !sound )
			continue;

		char sz[ 256 ];
		Q_snprintf( sz, sizeof( sz ), "\"%s\" : script %s", sound->GetName(), sound->GetScriptFile() );

		mxTreeViewItem *spot = sound->FindItem( tree, parent );
		if ( !spot )
		{
			spot = tree->add( parent, sz );
		}

		m_KnownItems.AddToTail( spot );

		sound->SetOrdinal( i );

		tree->setLabel( spot, sz );

		tree->setImages( spot, sound->GetIconIndex(), sound->GetIconIndex() );
		tree->setUserData( spot, sound );

		sound->ValidateTree( tree, spot );
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

void CVCDFile::Checkout(bool updatestateicons /*= true*/)
{
	VSS_Checkout( GetName(), updatestateicons );
}

void CVCDFile::Checkin(bool updatestateicons /*= true*/)
{
	VSS_Checkin( GetName(), updatestateicons );
}


bool CVCDFile::IsCheckedOut() const
{
	return filesystem->IsFileWritable( GetName() );
}

int CVCDFile::GetIconIndex() const
{
	if ( IsCheckedOut() )
	{
		return IMAGE_VCD_CHECKEDOUT;
	}
	else
	{
		return IMAGE_VCD;
	}
}

void CVCDFile::MoveChildUp( ITreeItem *child )
{
}

void CVCDFile::MoveChildDown( ITreeItem *child )
{
}	

void CVCDFile::SetDirty( bool dirty )
{
	if ( GetOwnerScene() )
	{
		GetOwnerScene()->SetDirty( dirty );
	}
}


bool CVCDFile::IsChildFirst( ITreeItem *child )
{
	return false;
}

bool CVCDFile::IsChildLast( ITreeItem *child )
{
	return false;
}
