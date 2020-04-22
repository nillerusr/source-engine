//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SYS_MAINWIND_H
#define SYS_MAINWIND_H
#ifdef _WIN32
#pragma once
#endif


int MapEngineKeyToVirtualKey( int keyCode );


// Pause VCR playback.
void VCR_EnterPausedState();


// During VCR playback, this can be adjusted to slow down the playback.
extern int g_iVCRPlaybackSleepInterval;

// During VCR playback, if this is true, then it'll pause at the end of each frame.
extern bool g_bVCRSingleStep;

extern bool g_bShowVCRPlaybackDisplay;


#endif // SYS_MAINWIND_H
