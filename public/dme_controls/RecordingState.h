//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef RECORDINGSTATE_H
#define RECORDINGSTATE_H
#ifdef _WIN32
#pragma once
#endif

// Animation set editor recording states
enum RecordingState_t
{
	AS_OFF = 0,
	AS_PREVIEW,
	AS_RECORD,
	AS_PLAYBACK,

	NUM_AS_RECORDING_STATES,
};

#endif // RECORDINGSTATE_H
