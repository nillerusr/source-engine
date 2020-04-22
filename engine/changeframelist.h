//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CHANGEFRAMELIST_H
#define CHANGEFRAMELIST_H
#ifdef _WIN32
#pragma once
#endif


#include "bitbuf.h"


// This class holds the last tick (from host_tickcount) that each property in 
// a datatable changed at.
//
// It provides fast access to a list of properties that changed within a certain frame range.
//
// These are created once per entity per frame. Since usually a very small percentage of an
// entity's properties actually change each frame, this allows you to get a small set of 
// properties to delta for each client.
abstract_class IChangeFrameList
{
public:
	
	// Call this to delete the object.
	virtual void	Release() = 0;

	// This just returns the value you passed into AllocChangeFrameList().
	virtual int		GetNumProps() = 0;

	// Sets the change frames for the specified properties to iFrame.
	virtual void	SetChangeTick( const int *pPropIndices, int nPropIndices, const int iTick ) = 0;

	// Get a list of all properties with a change frame > iFrame.
	virtual int		GetPropsChangedAfterTick( int iTick, int *iOutProps, int nMaxOutProps ) = 0;

	virtual IChangeFrameList* Copy() = 0; // return a copy of itself


protected:
	// Use Release to delete these.
	virtual			~IChangeFrameList() {}
};				


// Call to initialize. Pass in the number of properties this CChangeFrameList will hold.
// All properties will be initialized to iCurFrame.
IChangeFrameList* AllocChangeFrameList( int nProperties, int iCurFrame );


#endif // CHANGEFRAMELIST_H
