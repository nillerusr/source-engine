//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef VOICE_H
#define VOICE_H
#pragma once


#include "ivoicetweak.h"


/*! @defgroup Voice Voice
Defines the engine's interface to the voice code.
@{
*/

// Voice_Init will pick a sample rate, it must be within RATE_MAX
#define VOICE_OUTPUT_SAMPLE_RATE_LOW		11025	// Sample rate that we feed to the mixer.
#define VOICE_OUTPUT_SAMPLE_RATE_HIGH		22050	// Sample rate that we feed to the mixer.
#define VOICE_OUTPUT_SAMPLE_RATE_MAX		22050	// Sample rate that we feed to the mixer.


//! Returned on error from certain voice functions.
#define VOICE_CHANNEL_ERROR			-1
#define VOICE_CHANNEL_IN_TWEAK_MODE	-2	// Returned by AssignChannel if currently in tweak mode (not an error).


//! Initialize the voice code.
bool Voice_Init( const char *pCodec, int nSampleRate );

//! Inits voice with defaults if it is not initialized normally, e.g. for local mixer use.
void Voice_ForceInit();

//! Get the default sample rate to use for this codec
inline int Voice_GetDefaultSampleRate( const char *pCodec ) // Inline for DEDICATED builds
{
	// Use legacy lower rate for speex
	if ( Q_stricmp( pCodec, "vaudio_speex" ) == 0 )
	{
		return VOICE_OUTPUT_SAMPLE_RATE_LOW;
	}
	else if ( Q_stricmp( pCodec, "steam" ) == 0 )
	{
		return 0; // For the steam codec, 0 passed to voice_init means "use optimal steam voice rate"
	}

	// Use high sample rate by default for other codecs.
	return VOICE_OUTPUT_SAMPLE_RATE_HIGH;
}

//! Shutdown the voice code.
void Voice_Deinit();

//! Returns true if the client has voice enabled
bool Voice_Enabled( void );

//! The codec voice was initialized with. Empty string if voice is not initialized.
const char *Voice_ConfiguredCodec();

//! The sample rate voice was initialized with. -1 if voice is not initialized.
int Voice_ConfiguredSampleRate();

//! Returns true if the user can hear themself speak.
bool Voice_GetLoopback();


//! This is called periodically by the engine when the server acks the local player talking.
//! This tells the client DLL that the local player is talking and fades after about 200ms.
void Voice_LocalPlayerTalkingAck();


//! Call every frame to update the voice stuff.
void Voice_Idle(float frametime);


//! Returns true if mic input is currently being recorded.
bool Voice_IsRecording();

//! Begin recording input from the mic.
bool Voice_RecordStart(
	//! Filename to store incoming mic data, or NULL if none.
	const char *pUncompressedFile,	
	
	//! Filename to store the output of compression and decompressiong with the codec, or NULL if none.
	const char *pDecompressedFile,	
	
	//! If this is non-null, the voice manager will use this file for input instead of the mic.
	const char *pMicInputFile		
	);

// User wants to stop recording
void Voice_UserDesiresStop();

//! Stop recording from the mic.
bool Voice_RecordStop();


//! Get the most recent N bytes of compressed data. If nCount is less than the number of
//! available bytes, it discards the first bytes and gives you the last ones.
//! Set bFinal to true on the last call to this (it will flush out any stored voice data).
int Voice_GetCompressedData(char *pchData, int nCount, bool bFinal);



//! Pass incoming data from the server into here.
//! The data should have been compressed and gotten through a Voice_GetCompressedData call.
int Voice_AddIncomingData(
	//! Channel index.
	int nChannel, 
	//! Compressed data to add to the channel.
	const char *pchData, 
	//! Number of bytes in pchData.
	int nCount,
	//! Sequence number. If a packet is missed, it adds padding so the time isn't squashed.
	int iSequenceNumber
	);

//! Call this to reserve a voice channel for the specified entity to talk into.
//! \return A channel index for use with Voice_AddIncomingData or VOICE_CHANNEL_ERROR on error.
int Voice_AssignChannel(int nEntity, bool bProximity );

//! Call this to get the channel index that the specified entity is talking into.
//! \return A channel index for use with Voice_AddIncomingData or VOICE_CHANNEL_ERROR if the entity isn't talking.
int Voice_GetChannel(int nEntity);

#if !defined( NO_VOICE )
extern IVoiceTweak g_VoiceTweakAPI;
extern bool g_bUsingSteamVoice;
#endif

/*! @} */


#endif // VOICE_H
