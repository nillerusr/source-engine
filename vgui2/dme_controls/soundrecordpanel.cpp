//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dme_controls/soundrecordpanel.h"
#include "filesystem.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/TextEntry.h"
#include "dme_controls/dmecontrols.h"
#include "vgui_controls/messagebox.h"
#include "soundemittersystem/isoundemittersystembase.h"
#include "vgui/IVGui.h"
#include "mathlib/mathlib.h"

// FIXME: Move sound code out of the engine + into a library!
#include "toolframework/ienginetool.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
//
// Sound Record Dialog
//
//-----------------------------------------------------------------------------
CSoundRecordPanel::CSoundRecordPanel( vgui::Panel *pParent, const char *pTitle ) : 
	BaseClass( pParent, "SoundRecordPanel" )
{
	m_bIsRecording = false;
	m_nPlayingSound = 0;
	SetDeleteSelfOnClose( true );
	m_pOkButton = new Button( this, "OkButton", "#FileOpenDialog_Open", this, "Ok" );
	m_pCancelButton = new Button( this, "CancelButton", "#FileOpenDialog_Cancel", this, "Cancel" );
	m_pPlayButton = new Button( this, "PlayButton", "Play", this, "Play" );
	m_pRecordButton = new Button( this, "Record", "Record", this, "ToggleRecord" );
	m_pRecordTime = new TextEntry( this, "RecordTime" );
	m_pFileName = new TextEntry( this, "FileName" );

	LoadControlSettingsAndUserConfig( "resource/soundrecordpanel.res" );

	SetTitle( pTitle, false );
}

CSoundRecordPanel::~CSoundRecordPanel()
{
	StopSoundPreview();
}


//-----------------------------------------------------------------------------
// Purpose: Activate the dialog
//-----------------------------------------------------------------------------
void CSoundRecordPanel::DoModal( const char *pFileName )
{
	Assert( EngineTool() );

	char pRelativeWAVPath[MAX_PATH];
	g_pFullFileSystem->FullPathToRelativePath( pFileName, pRelativeWAVPath, sizeof(pRelativeWAVPath) );

	// Check to see if this file is not hidden owing to search paths
	bool bBadDirectory = false;
	char pRelativeDir[MAX_PATH];
	Q_strncpy( pRelativeDir, pRelativeWAVPath, sizeof( pRelativeDir ) );
	Q_StripFilename( pRelativeDir );
	char pFoundFullPath[MAX_PATH];
	g_pFullFileSystem->RelativePathToFullPath( pRelativeDir, "MOD", pFoundFullPath, sizeof( pFoundFullPath ) );
	if ( StringHasPrefix( pFileName, pFoundFullPath ) )
	{
		// Strip 'sound/' prefix
		m_FileName = pRelativeWAVPath;
		const char *pSoundName = StringAfterPrefix( pRelativeWAVPath, "sound\\" );
		if ( !pSoundName )
		{
			pSoundName = pRelativeWAVPath;
			bBadDirectory = true;
		}
		m_EngineFileName = pSoundName;
	}
	else
	{
		bBadDirectory = true;
	}

	if ( bBadDirectory )
	{
		char pBuf[1024];
		Q_snprintf( pBuf, sizeof(pBuf), "File %s is in a bad directory!\nAudio must be recorded into your mod's sound/ directory.\n", pFileName ); 
		vgui::MessageBox *pMessageBox = new vgui::MessageBox( "Bad Save Directory!\n", pBuf, GetParent() );
		pMessageBox->DoModal( );
		return;
	}

	m_pFileName->SetText( pFileName );
	m_pOkButton->SetEnabled( false );
	m_pPlayButton->SetEnabled( false );
	BaseClass::DoModal();
}


//-----------------------------------------------------------------------------
// Stop sound preview
//-----------------------------------------------------------------------------
void CSoundRecordPanel::StopSoundPreview( )
{
	if ( m_nPlayingSound != 0 )
	{
		EngineTool()->StopSoundByGuid( m_nPlayingSound );
		m_nPlayingSound = 0;
	}
}


//-----------------------------------------------------------------------------
// Plays a wav file
//-----------------------------------------------------------------------------
void CSoundRecordPanel::PlaySoundPreview( )
{
	StopSoundPreview();
	m_nPlayingSound = EngineTool()->StartSound( 0, true, -1, CHAN_STATIC, m_EngineFileName, 
		VOL_NORM, SNDLVL_NONE, vec3_origin, vec3_origin, 0, PITCH_NORM, false, 0 );
}


//-----------------------------------------------------------------------------
// Updates sound record time during recording
//-----------------------------------------------------------------------------
void CSoundRecordPanel::UpdateTimeRecorded()
{ 
	float flTime = Plat_FloatTime() - m_flRecordStartTime;
	char pTimeBuf[64];
	Q_snprintf( pTimeBuf, sizeof(pTimeBuf), "%.3f", flTime );
	m_pRecordTime->SetText( pTimeBuf );
}


//-----------------------------------------------------------------------------
// Updates sound record time during recording
//-----------------------------------------------------------------------------
void CSoundRecordPanel::OnTick()
{
	BaseClass::OnTick();

	// Update the amount of time recorded
	UpdateTimeRecorded();
}

	
//-----------------------------------------------------------------------------
// On command
//-----------------------------------------------------------------------------
void CSoundRecordPanel::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "ToggleRecord" ) )
	{
		if ( !m_bIsRecording )
		{
			StopSoundPreview();
			g_pFullFileSystem->RemoveFile( m_FileName, "MOD" );
			EngineTool()->StartRecordingVoiceToFile( m_FileName, "MOD" );
			m_pRecordButton->SetText( "Stop Recording" );
			m_pPlayButton->SetEnabled( false );
			m_pOkButton->SetEnabled( false );
			m_pCancelButton->SetEnabled( true );
			m_flRecordStartTime = Plat_FloatTime();
			vgui::ivgui()->AddTickSignal( GetVPanel(), 0 );	
		}
		else
		{
			EngineTool()->StopRecordingVoiceToFile();
			EngineTool()->ReloadSound( m_EngineFileName );
			ivgui()->RemoveTickSignal( GetVPanel() );
			UpdateTimeRecorded();
			m_pOkButton->SetEnabled( true );
			m_pCancelButton->SetEnabled( true );
			m_pPlayButton->SetEnabled( true );
			m_pRecordButton->SetText( "Record" );
		}
		m_bIsRecording = !m_bIsRecording;
		return;
	}

	if ( !Q_stricmp( pCommand, "Play" ) )
	{
		PlaySoundPreview();
		return;
	}

	if ( !Q_stricmp( pCommand, "Ok" ) )
	{
		PostActionSignal( new KeyValues( "SoundRecorded", "relativepath", m_EngineFileName.Get() ) );
		CloseModal();
		return;
	}

	if ( !Q_stricmp( pCommand, "Cancel" ) )
	{
		g_pFullFileSystem->RemoveFile( m_FileName, "MOD" );
		CloseModal();
		return;
	}

	BaseClass::OnCommand( pCommand );
}

	
