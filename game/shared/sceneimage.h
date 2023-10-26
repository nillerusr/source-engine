//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SCENEIMAGE_H
#define SCENEIMAGE_H
#ifdef _WIN32
#pragma once
#endif

class ISceneTokenProcessor;

class ISceneCompileStatus
{
public:
	virtual void UpdateStatus( char const *pchSceneName, bool bQuiet, int nIndex, int nCount ) = 0;
};

class CUtlBuffer;
class CUtlString;

class ISceneImage
{
public:
	virtual bool CreateSceneImageFile( CUtlBuffer &targetBuffer, char const *pchModPath, bool bLittleEndian, bool bQuiet, ISceneCompileStatus *Status ) = 0;
	// This will update the scenes.image file, or create a new one if it doesn't exist.  
	// The caller should pass in the existing .image file in targetBuffer!!!
	virtual bool UpdateSceneImageFile( CUtlBuffer &targetBuffer, char const *pchModPath, bool bLittleEndian, bool bQuiet, ISceneCompileStatus *Status, CUtlString *pFilesToUpdate, int nUpdateCount ) = 0;
};

extern ISceneImage *g_pSceneImage;
extern ISceneTokenProcessor *tokenprocessor;

#endif // SCENEIMAGE_H
