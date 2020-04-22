//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "wavefile.h"
#include "sentence.h"
#include "iscenemanagersound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "snd_wave_source.h"
#include "cmdlib.h"
#include "workspacemanager.h"
#include "vcdfile.h"
#include "workspacebrowser.h"
#include "multiplerequest.h"
#include "UtlBuffer.h"
#include "scenemanager_tools.h"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CWaveFile::CWaveFile( CVCDFile *vcd, CSoundEntry *se, char const *filename ) 
	: m_pOwner( vcd ), m_pOwnerSE( se )
{
	m_bSentenceLoaded = false;

	m_Sentence.Reset();

	Q_strncpy( m_szName, filename, sizeof( m_szName ) );

	m_pWaveFile = NULL;

	Q_snprintf( m_szFileName, sizeof( m_szFileName ), "sound/%s", filename );
}

CWaveFile::~CWaveFile()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CWaveFile::GetLanguageId()
{
	return GetWorkspaceManager()->GetLanguageId();
}

void CWaveFile::EnsureSentence()
{
	if ( m_bSentenceLoaded )
		return;

	m_bSentenceLoaded = true;

	if ( m_szFileName[ 0 ] )
	{
		SceneManager_LoadSentenceFromWavFile( m_szFileName, m_Sentence );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWaveFile::HasLoadedSentenceInfo() const
{
	return m_bSentenceLoaded;
}


CVCDFile *CWaveFile::GetOwnerVCDFile()
{
	return m_pOwner;
}

CSoundEntry *CWaveFile::GetOwnerSoundEntry()
{
	return m_pOwnerSE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void CWaveFile::SetName( char const *filename )
{
	if ( !Q_stricmp( m_szName, filename ) )
		return;

	Q_strncpy( m_szName, filename, sizeof( m_szName ) );

	Q_snprintf( m_szFileName, sizeof( m_szFileName ), "sound/%s", filename );

	if ( m_szFileName[ 0 ] )
	{
		SceneManager_LoadSentenceFromWavFile( m_szFileName, m_Sentence );
		m_bSentenceLoaded = true;
	}
}

char const	*CWaveFile::GetName() const
{
	return m_szName;
}

char const *CWaveFile::GetFileName() const
{
	return m_szFileName;
}

char const	*CWaveFile::GetSentenceText()
{
	EnsureSentence();
	return m_Sentence.GetText();
}

void CWaveFile::SetSentenceText( char const *newText )
{
	EnsureSentence();
	if ( !Q_stricmp( GetSentenceText(), newText ) )
		return;

	if ( !IsCheckedOut() )
	{
		int retval = MultipleRequest( va( "Check out '%s'?", GetFileName() ) );
		if ( retval != 0 )
			return;

		VSS_Checkout( GetFileName() );
	}

	m_Sentence.SetText( newText );

	SceneManager_SaveSentenceToWavFile( GetFileName(), m_Sentence );
}

void CWaveFile::ValidateTree( mxTreeView *tree, mxTreeViewItem* parent )
{
}

void CWaveFile::Play()
{
	if ( !m_pWaveFile )
	{
		m_pWaveFile = sound->FindOrAddSound( m_szFileName );
	}

	if ( !m_pWaveFile )
	{
		Con_Printf( "Can't play '%s', no wave file loaded\n", GetFileName() );
		return;
	}

	Con_Printf( "Playing '%s' : '%s'\n", GetFileName(), GetSentenceText() );

	CAudioMixer *temp;
	sound->PlaySound( m_pWaveFile, &temp );
}

bool CWaveFile::GetVoiceDuck()
{
	EnsureSentence();
	return m_Sentence.GetVoiceDuck();
}

void CWaveFile::SetVoiceDuck( bool duck )
{
	EnsureSentence();
	if ( GetVoiceDuck() == duck )
		return;

	m_Sentence.SetVoiceDuck( duck );

	if ( !IsCheckedOut() )
	{
		int retval = MultipleRequest( va( "Check out '%s'?", GetFileName() ) );
		if ( retval != 0 )
			return;

		VSS_Checkout( GetFileName() );
	}

	SceneManager_SaveSentenceToWavFile( GetFileName(), m_Sentence );
}

void CWaveFile::ToggleVoiceDucking()
{
	EnsureSentence();
	m_Sentence.SetVoiceDuck( !m_Sentence.GetVoiceDuck() );

	if ( !IsCheckedOut() )
	{
		int retval = MultipleRequest( va( "Check out '%s'?", GetFileName() ) );
		if ( retval != 0 )
			return;

		VSS_Checkout( GetFileName() );
	}

	SceneManager_SaveSentenceToWavFile( GetFileName(), m_Sentence );
}

bool CWaveFile::IsCheckedOut() const
{
	return filesystem->IsFileWritable( GetFileName() );
}

int CWaveFile::GetIconIndex() const
{
	if ( IsCheckedOut() )
	{
		return IMAGE_WAV_CHECKEDOUT;
	}
	else
	{
		return IMAGE_WAV;
	}
}


void CWaveFile::Checkout(bool updatestateicons /*= true*/)
{
	VSS_Checkout( GetFileName(), updatestateicons );
}

void CWaveFile::Checkin(bool updatestateicons /*= true*/)
{
	VSS_Checkin( GetFileName(), updatestateicons );
}


void CWaveFile::MoveChildUp( ITreeItem *child )
{
}

void CWaveFile::MoveChildDown( ITreeItem *child )
{
}

void CWaveFile::SetDirty( bool dirty )
{
	if ( GetOwnerVCDFile() )
	{
		GetOwnerVCDFile()->SetDirty( dirty );
	}
}

bool CWaveFile::IsChildFirst( ITreeItem *child )
{
	return false;
}

bool CWaveFile::IsChildLast( ITreeItem *child )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sentence - 
//-----------------------------------------------------------------------------
void CWaveFile::SetThreadLoadedSentence( CSentence& sentence )
{
	if ( m_bSentenceLoaded )
		return;

	m_bSentenceLoaded = true;
	m_Sentence = sentence;
}

#define WORD_DATA_EXTENSION	".txt"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *tempfile - 
//-----------------------------------------------------------------------------
void CWaveFile::ExportValveDataChunk( char const *tempfile )
{
	EnsureSentence();

	if ( m_Sentence.m_Words.Count() <= 0 )
	{
		Con_ColorPrintf( ERROR_R, ERROR_G, ERROR_B, "CWaveFile::ExportValveDataChunk:  Sentence has no word data\n" );
		return;
	}

	SafeCreatePath( va( "%s%s", SceneManager_GetGameDirectory(), (char *)tempfile ) );

	FileHandle_t fh = filesystem->Open( tempfile, "wb" );
	if ( !fh )
	{
		Con_ColorPrintf( ERROR_R, ERROR_G, ERROR_B, "CWaveFile::ExportValveDataChunk:  Unable to write to %s (read-only?)\n", tempfile );
		return;
	}
	else
	{
		// Buffer and dump data
		CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

		m_Sentence.SaveToBuffer( buf );

		filesystem->Write( buf.Base(), buf.TellPut(), fh );
		filesystem->Close(fh);

		Con_Printf( "Exported %i words to %s\n", m_Sentence.m_Words.Count(), tempfile );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *tempfile - 
//-----------------------------------------------------------------------------
void CWaveFile::ImportValveDataChunk( char const *tempfile )
{
	EnsureSentence();

	FileHandle_t fh = filesystem->Open( tempfile, "rb" );
	if ( !fh )
	{
		Con_ColorPrintf( ERROR_R, ERROR_G, ERROR_B, "CWaveFile::ImportValveDataChunk:  Unable to read from %s\n", tempfile );
		return;
	}

	int len = filesystem->Size( fh );
	if ( len <= 4 )
	{
		Con_ColorPrintf( ERROR_R, ERROR_G, ERROR_B, "CWaveFile::ImportValveDataChunk:  File %s has length 0\n", tempfile );
		return;
	}

	CSentence newSentence;

	unsigned char *buf = new unsigned char[ len + 1 ];

	filesystem->Read( buf, len, fh );
	filesystem->Close( fh );

	newSentence.InitFromDataChunk( (void *)( buf ), len );

	delete[] buf;

	// See if we can write it out...
	if ( !IsCheckedOut() )
	{
		int retval = MultipleRequest( va( "Check out '%s'?", GetFileName() ) );
		if ( retval != 0 )
			return;

		VSS_Checkout( GetFileName() );
	}

	if ( !IsCheckedOut() )
	{
		MakeFileWriteable( GetFileName() );
		Con_Printf( "Unable to check out %s, forcing it to be writable instead!\n", GetFileName() );
	}

	Con_Printf( "Imported %i words from %s\n", newSentence.m_Words.Count(), tempfile );

	m_Sentence = newSentence;

	SceneManager_SaveSentenceToWavFile( GetFileName(), m_Sentence );
}

void CWaveFile::GetPhonemeExportFile( char *path, int maxlen )
{
	char relative[ 512 ];
	strcpy( relative, GetFileName() );
	Q_StripExtension( relative, relative, sizeof( relative ) );
	Q_DefaultExtension( relative, WORD_DATA_EXTENSION, sizeof( relative ) );

	Q_snprintf( path, maxlen, "phonemes/%s", relative );
	Q_FixSlashes( path );
}

