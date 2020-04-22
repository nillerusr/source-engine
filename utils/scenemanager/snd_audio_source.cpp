//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include <stdio.h>
#include "snd_audio_source.h"
#include "iscenemanagersound.h"

extern CAudioSource *Audio_CreateMemoryWave( const char *pName );

//-----------------------------------------------------------------------------
// Purpose: Simple wrapper to crack naming convention and create the proper wave source
// Input  : *pName - WAVE filename
// Output : CAudioSource
//-----------------------------------------------------------------------------
CAudioSource *AudioSource_Create( const char *pName )
{
	if ( !pName )
		return NULL;

//	if ( pName[0] == '!' )		// sentence
		;

	// Names that begin with "*" are streaming.
	// Skip over the * and create a streamed source
	if ( pName[0] == '*' )
	{

		return NULL;
	}

	// These are loaded into memory directly
	return Audio_CreateMemoryWave( pName );
}

CAudioSource::~CAudioSource( void )
{
	CAudioMixer *mixer;
	
	while ( 1 )
	{
		mixer = sound->FindMixer( this );
		if ( !mixer )
			break;

		sound->StopSound( mixer );
	}
}

CAudioSource::CAudioSource( void )
{
}
