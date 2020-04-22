//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef EXPORTFACIALANIMATION_H
#define EXPORTFACIALANIMATION_H
#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeFilmClip;
class CDmeAnimationSet;


//-----------------------------------------------------------------------------
// Exports an .fac file
//-----------------------------------------------------------------------------
bool ExportFacialAnimation( const char *pFileName, CDmeFilmClip *pMovie, CDmeFilmClip *pShot, CDmeAnimationSet *pAnimationSet );


#endif // EXPORTFACIALANIMATION_H
