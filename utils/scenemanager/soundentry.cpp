//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "soundentry.h"
#include "sentence.h"
#include "iscenemanagersound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "snd_wave_source.h"
#include "cmdlib.h"
#include "workspacemanager.h"
#include "vcdfile.h"
#include "workspacebrowser.h"
#include "wavefile.h"
#include "wavebrowser.h"
#include <vgui/ILocalize.h>
#include "tier3/tier3.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CSoundEntry::CSoundEntry( CVCDFile *vcd, char const *name ) : m_pOwner( vcd )
{
	Q_memset( &m_Params, 0, sizeof( m_Params ) );

	m_szScriptFile[ 0 ] = 0;

	Q_strncpy( m_szName, name, sizeof( m_szName ) );

	// Get name out of sound emitter system
	char filebase [ 64 ];
	int slot = g_pSoundEmitterSystem->GetSoundIndex( name );
	if ( g_pSoundEmitterSystem->IsValidIndex( slot ) )
	{
		Q_FileBase( g_pSoundEmitterSystem->GetSourceFileForSound( slot ), filebase, sizeof( filebase ) );
		Q_strncpy( m_szScriptFile, filebase, sizeof( m_szScriptFile ) );

		// Look up and add the .wav files for the sound entry

		CSoundParametersInternal *p = g_pSoundEmitterSystem->InternalGetParametersForSound( slot );
		if ( p )
		{
			CWaveBrowser *wb = GetWorkspaceManager()->GetWaveBrowser();
			Assert( wb );

			int waveCount = p->NumSoundNames();
			for ( int wave = 0; wave < waveCount; wave++ )
			{
				char const *wavname = g_pSoundEmitterSystem->GetWaveName( p->GetSoundNames()[ wave ].symbol );
				if ( wavname )
				{
					CWaveFile *wavefile = wb->FindEntry( wavname, false );
					if ( wavefile )
					{
						AddWave( wavefile );
					}
				}
			}
		}
	}
}

CSoundEntry::~CSoundEntry()
{
	m_Waves.RemoveAll();
}

CVCDFile *CSoundEntry::GetOwnerVCDFile()
{
	return m_pOwner;
}

int CSoundEntry::GetWaveCount() const
{
	return m_Waves.Count();
}

CWaveFile *CSoundEntry::GetWave( int index )
{
	if ( index < 0 || index >= m_Waves.Count() )
		return NULL;
	return m_Waves[ index ];
}

void CSoundEntry::ClearWaves()
{
	m_Waves.RemoveAll();
}

void CSoundEntry::AddWave( CWaveFile *wave )
{
	if ( m_Waves.Find( wave ) != m_Waves.InvalidIndex() )
		return;

	m_Waves.AddToTail( wave );
}

void CSoundEntry::RemoveWave( CWaveFile *wave )
{
	m_Waves.FindAndRemove( wave );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *wave - 
// Output : int
//-----------------------------------------------------------------------------
int CSoundEntry::FindWave( CWaveFile *wave )
{
	int idx = m_Waves.Find( wave );
	if ( idx != m_Waves.InvalidIndex() )
		return idx;

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void CSoundEntry::SetName( char const *name )
{
	if ( !Q_stricmp( m_szName, name ) )
		return;

	Q_strncpy( m_szName, name, sizeof( m_szName ) );

	// Get name out of sound emitter system
	char filebase [ 64 ];
	int slot = g_pSoundEmitterSystem->GetSoundIndex( name );
	if ( g_pSoundEmitterSystem->IsValidIndex( slot ) )
	{
		Q_FileBase( g_pSoundEmitterSystem->GetSourceFileForSound( slot ), filebase, sizeof( filebase ) );
		Q_strncpy( m_szScriptFile, filebase, sizeof( m_szScriptFile ) );
	}
}


char const	*CSoundEntry::GetName() const
{
	return m_szName;
}

void CSoundEntry::ValidateTree( mxTreeView *tree, mxTreeViewItem* parent )
{
	CUtlVector< mxTreeViewItem * >	m_KnownItems;

	int c = GetWaveCount();
	CWaveFile *wave;
	for ( int i = 0; i < c; i++ )
	{
		wave = GetWave( i );
		if ( !wave )
			continue;

		char sz[ 256 ];
		if ( wave->HasLoadedSentenceInfo() )
		{
			Q_snprintf( sz, sizeof( sz ), "\"%s\" : '%s'", wave->GetName(), wave->GetSentenceText() );
		}
		else
		{
			Q_snprintf( sz, sizeof( sz ), "\"%s\" : '%s'", wave->GetName(), "(loading...)" );
		}

		mxTreeViewItem *spot = wave->FindItem( tree, parent );
		if ( !spot )
		{
			spot = tree->add( parent, sz );
		}

		m_KnownItems.AddToTail( spot );

		wave->SetOrdinal( i );

		tree->setLabel( spot, sz );

		tree->setImages( spot, wave->GetIconIndex(), wave->GetIconIndex() );
		tree->setUserData( spot, wave );

		wave->ValidateTree( tree, spot );
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

void CSoundEntry::Play()
{
	if ( m_Waves.Count() == 0 )
		return;

	m_nLastPlay = ( m_nLastPlay + 1 ) % m_Waves.Count();

	CWaveFile *wave = GetWave( m_nLastPlay );
	if ( !wave )
		return;

	wave->Play();
}

char const *CSoundEntry::GetScriptFile() const
{
	return m_szScriptFile;
}

void CSoundEntry::SetScriptFile( char const *scriptfile )
{
	char filebase [ 64 ];
	int slot = g_pSoundEmitterSystem->GetSoundIndex( GetName() );
	Q_FileBase( g_pSoundEmitterSystem->GetSourceFileForSound( slot ), filebase, sizeof( filebase ) );

	if ( !Q_stricmp( GetScriptFile(), filebase ) )
		return;

	Q_strncpy( m_szScriptFile, filebase, sizeof( m_szScriptFile ) );
}

CSoundParametersInternal *CSoundEntry::GetSoundParameters()
{
	int soundindex = g_pSoundEmitterSystem->GetSoundIndex( GetName() );
	if ( g_pSoundEmitterSystem->IsValidIndex( soundindex ) )
	{
		return g_pSoundEmitterSystem->InternalGetParametersForSound( soundindex );
	}
	return NULL;
}

bool CSoundEntry::IsCheckedOut() const
{
	// FIXME:  This could use the wave state or the script state?
	return false;
}

int CSoundEntry::GetIconIndex() const
{
	if ( IsCheckedOut() )
	{
		return IMAGE_SPEAK_CHECKEDOUT;
	}
	else
	{
		return IMAGE_SPEAK;
	}
}


void CSoundEntry::Checkout(bool updatestateicons /*= true*/)
{
}

void CSoundEntry::Checkin(bool updatestateicons /*= true*/)
{
}


void CSoundEntry::MoveChildUp( ITreeItem *child )
{
}

void CSoundEntry::MoveChildDown( ITreeItem *child )
{
}

void CSoundEntry::SetDirty( bool dirty )
{
	if ( GetOwnerVCDFile() )
	{
		GetOwnerVCDFile()->SetDirty( dirty );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wavindex - 
// Output : char const
//-----------------------------------------------------------------------------
char const *CSoundEntry::GetSentenceText( int wavindex )
{
	if ( !GetWaveCount() )
		return "";

	CWaveFile *w = GetWave( wavindex );
	if ( w->HasLoadedSentenceInfo() )
	{
		return w->GetSentenceText();
	}
	else
	{
		return "(loading...)";
	}
}

void CSoundEntry::GetCCText( wchar_t *out, int maxchars )
{
	out[ 0 ] = 0;

	if ( !g_pVGuiLocalize )
		return;

	const wchar_t *str = g_pVGuiLocalize->Find( GetName() );
	if ( !str )
		return;

	wcsncpy( out, str, maxchars );
}


bool CSoundEntry::IsChildFirst( ITreeItem *child )
{
	return false;
}

bool CSoundEntry::IsChildLast( ITreeItem *child )
{
	return false;
}