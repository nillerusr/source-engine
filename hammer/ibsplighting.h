//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IBSPLIGHTING_H
#define IBSPLIGHTING_H
#ifdef _WIN32
#pragma once
#endif


class IBSPLighting
{
public:
		
	virtual					~IBSPLighting() {}
	virtual void			Release() = 0;

	// Init for incremental lighting.
	// - load the VRAD DLL
	// - load the BSP file into it
	// - start the incremental lighting thread
	// - start lighting in the background if need be
	virtual bool			Load( char const *pFilename ) = 0;
	
	// Shutdown everything (but keep the object around).
	virtual void			Term() = 0;

	// Serialize the r0 and bsp files if any new lighting has been completed.
	// Note: this will return false if a lighting pass is currently in progress
	//       (don't worry though - Term() will stop an active lighting pass and
	//       serialize the lighting).
	virtual bool			Serialize() = 0;

	// Start lighting in the background thread using the current state of the
	// entities in the editor's memory.
	virtual void			StartLighting( char const *pVMFFileWithEnts ) = 0;

	// Returns a 0-1 value describing how far along the lighting is, or -1 if not lighting.
	virtual float			GetPercentComplete() = 0;

	// Stop the lighting process.
	virtual void			Interrupt() = 0;

	// This should be called during idle time. If it returns true, then it has
	// update all the lightmaps and the views should be redrawn.
	virtual bool			CheckForNewLightmaps() = 0;

	// Render the current lightmaps.	
	virtual void			Draw() = 0;
};

IBSPLighting* CreateBSPLighting();


#endif // IBSPLIGHTING_H
