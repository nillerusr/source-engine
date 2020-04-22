//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IBSPLIGHTING_THREAD_H
#define IBSPLIGHTING_THREAD_H
#ifdef _WIN32
#pragma once
#endif


#include "ivraddll.h"


class IBSPLightingThread
{
public:
	enum
	{
		STATE_IDLE=0,
		STATE_LIGHTING=1,
		STATE_FINISHED=2
	};

	virtual				~IBSPLightingThread() {}	

	virtual void		Release() = 0;

	// Start processing light in the background thread. 
	//
	// Goes to STATE_LIGHTING, then if it's successful, it goes to STATE_FINISHED.
	// If unsuccessful or interrupted, it goes to STATE_IDLE.
	//
	// If this is called while it's already lighting, it stops the current lighting
	// process and restarts.
	virtual void		StartLighting( char const *pVMFFileWithEntities ) = 0;

	// Returns one of the STATE_ defines.
	virtual int			GetCurrentState() = 0;

	// If lighting is in progress, make it stop.
	virtual void		Interrupt() = 0;

	// Returns IVRadDLL::GetPercentComplete if it's lighting.
	virtual float		GetPercentComplete() = 0;
};

IBSPLightingThread* CreateBSPLightingThread( IVRadDLL *pDLL );


#endif // IBSPLIGHTING_THREAD_H
