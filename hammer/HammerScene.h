//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef HAMMERSCENE_H
#define HAMMERSCENE_H
#ifdef _WIN32
#pragma once
#endif


class CChoreoScene;


// Load the specified vcd file.
CChoreoScene* HammerLoadScene( const char *pFilename );

// Load the VCD file and get the first sound in it.
bool GetFirstSoundInScene( const char *pSceneFilename, char *pSoundName, int soundNameLen );


#endif // HAMMERSCENE_H
