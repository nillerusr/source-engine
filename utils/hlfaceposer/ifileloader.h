//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef IFILELOADER_H
#define IFILELOADER_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"

class CWaveFile;

class IFileLoader
{
public:
	virtual void			AddWaveFilesToThread( CUtlVector< CWaveFile * >& wavefiles ) = 0;

	virtual int				ProcessCompleted() = 0;

	virtual	void			Start() = 0;

	virtual int				GetPendingLoadCount() = 0;
};

extern IFileLoader *fileloader;

#endif // IFILELOADER_H
