//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmesound.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "movieobjects_interfaces.h"
#include "tier2/tier2.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSound, CDmeSound );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeSound::OnConstruction()
{
	m_SoundName.Init( this, "soundname" );
	m_GameSoundName.Init( this, "gameSoundName" );
}

void CDmeSound::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// For sounds that are relative paths (instead of GameSound names), get full path
//-----------------------------------------------------------------------------
bool CDmeSound::ComputeSoundFullPath( char *pBuf, int nBufLen )
{
	if ( !m_SoundName[0] )
	{
		pBuf[0] = 0;
		return false;
	}

	// Compute the full path of the sound
	char pRelativePath[MAX_PATH];
	Q_snprintf( pRelativePath, sizeof(pRelativePath), "sound\\%s", m_SoundName.Get() );
	return g_pFullFileSystem->RelativePathToFullPath( pRelativePath, "GAME", pBuf, nBufLen ) != NULL;
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeGameSound, CDmeGameSound );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeGameSound::OnConstruction()
{
	m_Volume	.Init( this, "volume" );
	m_Level		.Init( this, "level" );
	m_Pitch		.Init( this, "pitch" );

	m_IsStatic	.Init( this, "static" );
	m_Channel	.Init( this, "channel" );
	m_Flags		.Init( this, "flags" );

//	m_Source	.Init( this, "source" );
//	m_FollowSource.Init( this, "followsource" );
	m_Origin	.Init( this, "origin" );
	m_Direction	.Init( this, "direction" );
}

void CDmeGameSound::OnDestruction()
{
}

CDmElement *CDmeGameSound::FindOrAddPhonemeExtractionSettings()
{
	if ( HasAttribute( "PhonemeExtractionSettings" ) )
		return GetValueElement< CDmElement >( "PhonemeExtractionSettings" );

	CDmElement *settings = CreateElement< CDmElement >( "PhonemeExtractionSettings", GetFileId() );
	if ( !settings )
		return NULL;

	SetValue( "PhonemeExtractionSettings", settings );
	return settings;
}
