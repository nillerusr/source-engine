//========= Copyright Valve Corporation, All rights reserved. ============//
//
// NOTE: This is a cut-and-paste hack job to get animation set construction
// working from a commandline tool. It came from tools/ifm/createsfmanimation.cpp
// This file needs to die almost immediately + be replaced with a better solution
// that can be used both by the sfm + sfmgen.
//
//=============================================================================

#ifndef SFMANIMATIONSETUTILS_H
#define SFMANIMATIONSETUTILS_H
#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// movieobjects
//-----------------------------------------------------------------------------
class CDmeFilmClip;
class CDmeGameModel;
class CDmeAnimationSet;


//-----------------------------------------------------------------------------
// Creates an animation set
//-----------------------------------------------------------------------------
CDmeAnimationSet *CreateAnimationSet( CDmeFilmClip *pMovie, CDmeFilmClip *pShot, 
	CDmeGameModel *pGameModel, const char *pAnimationSetName, int nSequenceToUse, bool bAttachToGameRecording );


#endif // SFMANIMATIONSETUTILS_H
