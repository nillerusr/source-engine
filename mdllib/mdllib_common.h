//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef MDLLIB_COMMON_H
#define MDLLIB_COMMON_H

#ifdef _WIN32
#pragma once
#endif

#include "mdllib/mdllib.h"

#include "platform.h"
#pragma warning( disable : 4018 )
#pragma warning( disable : 4389 )



//-----------------------------------------------------------------------------
// Purpose: Interface to accessing P4 commands
//-----------------------------------------------------------------------------
class CMdlLib : public CBaseAppSystem< IMdlLib >
{
public:
	// Destructor
	virtual ~CMdlLib();

	//////////////////////////////////////////////////////////////////////////
	//
	// Methods of IAppSystem
	//
	//////////////////////////////////////////////////////////////////////////
public:
	virtual bool Connect( CreateInterfaceFn factory );
	virtual InitReturnVal_t Init();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual void Shutdown();
	virtual void Disconnect();


	//////////////////////////////////////////////////////////////////////////
	//
	// Methods of IMdlLib
	//
	//////////////////////////////////////////////////////////////////////////
public:
	//
	// StripModelBuffers
	//	The main function that strips the model buffers
	//		mdlBuffer			- mdl buffer, updated, no size change
	//		vvdBuffer			- vvd buffer, updated, size reduced
	//		vtxBuffer			- vtx buffer, updated, size reduced
	//		ppStripInfo			- if nonzero on return will be filled with the stripping info
	//
	virtual bool StripModelBuffers( CUtlBuffer &mdlBuffer, CUtlBuffer &vvdBuffer, CUtlBuffer &vtxBuffer, IMdlStripInfo **ppStripInfo );

	//
	// CreateNewStripInfo
	//	Creates an empty strip info so that it can be reused.
	//
	virtual bool CreateNewStripInfo( IMdlStripInfo **ppStripInfo );
};

#endif // #ifndef MDLLIB_COMMON_H
