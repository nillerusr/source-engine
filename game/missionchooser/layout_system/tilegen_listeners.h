//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Definitions of useful tilegen listeners which can be used to update
// free variable state based on events.
//
//===============================================================================

#ifndef TILEGEN_LISTENERS_H
#define TILEGEN_LISTENERS_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "tilegen_class_interfaces.h"

class CLayoutSystem;
class CTilegenState;

//-----------------------------------------------------------------------------
// Keeps track of the total area of tiles that has been placed 
// both in this current state and in total.
// Exposes the variables "NumTilesPlaced" and "NumTilesPlacedThisState".
//-----------------------------------------------------------------------------
class CTilegenListener_NumTilesPlaced : public ITilegenListener
{
public:
	CTilegenListener_NumTilesPlaced() : 
		m_nTotalArea( 0 ), 
		m_nTotalAreaThisState( 0 ) 
	{ 
	}

	virtual void OnBeginGeneration( const CLayoutSystem *pLayoutSystem, CFreeVariableMap *pFreeVariables );
	virtual void OnRoomPlaced( const CLayoutSystem *pLayoutSystem, CFreeVariableMap *pFreeVariables );
	virtual void OnStateChanged( const CLayoutSystem *pLayoutSystem, const CTilegenState *pOldState, CFreeVariableMap *pFreeVariables );
	

private:
	int m_nTotalArea;
	int m_nTotalAreaThisState;
};

#endif // TILEGEN_LISTENERS_H