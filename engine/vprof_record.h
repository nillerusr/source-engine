//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef VPROF_RECORD_H
#define VPROF_RECORD_H
#ifdef _WIN32
#pragma once
#endif


void VProfRecord_Shutdown();

// Take a snapshot of the current vprof state (and maybe write it to the file).
void VProfRecord_Snapshot();

// Execute any CVProfile::Start/Stop commands (you can only do them at certain times).
void VProfRecord_StartOrStop();

bool VProfRecord_IsPlayingBack();

// Which tick are we in the playback (-1 if not playing back).
int VProfPlayback_GetCurrentTick();
float VProfPlayback_GetCurrentPercent();

// These functions return 0 on error, 1 on success, and 2 means that it succeeded
// but that the nodes changed (so any tree views attached to it should be reset).
int VProfPlayback_SetPlaybackTick( int iTick );	// Note: this might take a long time if it has to seek a long way.
void VProfPlayback_Step();
int VProfPlayback_StepBack();					// Note: this might take a long time if it has to seek a long way.
int VProfPlayback_SeekToPercent( float percent );		// Seek to a percent of the way through the file.


class CVProfile;
extern CVProfile *g_pVProfileForDisplay;


#endif // VPROF_RECORD_H
