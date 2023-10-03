//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Definitions of useful tilegen array types which can be iterated.
//
//===============================================================================

#ifndef TILEGEN_RANGES_H
#define TILEGEN_RANGES_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "utlvector.h"
#include "tilegen_class_interfaces.h"

class CExit;
class CRoomCandidate;

//-----------------------------------------------------------------------------
// A range which allows iteration over all hypothetically new exits which 
// would be created as a result of the placement of a given room candidate.
//-----------------------------------------------------------------------------
class CTilegenRange_NewOpenExits : public ITilegenRange< const CExit * >
{
public:
	CTilegenRange_NewOpenExits( ITilegenExpression< const CRoomCandidate * > *pRoomCandidateExpression = NULL );
	~CTilegenRange_NewOpenExits();

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues );
	virtual void Initialize( CFreeVariableMap *pContext );
	virtual void Reset();
	virtual bool MoveNext();
	virtual const CExit * GetCurrent();

private:
	ITilegenExpression< const CRoomCandidate * > *m_pRoomCandidateExpression;
	CUtlVector< CExit > m_Exits;
	int m_nCurrentExit;
};

//-----------------------------------------------------------------------------
// A range which allows iteration over all exits which would be 
// hypothetically closed  as a result of the placement of a 
// given room candidate.
//-----------------------------------------------------------------------------
class CTilegenRange_ClosedExits : public ITilegenRange< const CExit * >
{
public:
	CTilegenRange_ClosedExits( ITilegenExpression< const CRoomCandidate * > *pRoomCandidateExpression = NULL );
	~CTilegenRange_ClosedExits();

  virtual bool LoadFromKeyValues( KeyValues *pKeyValues );
  virtual void Initialize( CFreeVariableMap *pContext );
  virtual void Reset();
  virtual bool MoveNext();
  virtual const CExit * GetCurrent();

private:
	ITilegenExpression< const CRoomCandidate * > *m_pRoomCandidateExpression;
	CUtlVector< CExit > m_Exits;
	int m_nCurrentExit;
};

//-----------------------------------------------------------------------------
// Gets the coordinates of the specified exit from the given room placement.
// Returns true if the exit is in bounds, false otherwise.
//-----------------------------------------------------------------------------
bool GetExitPosition( const CRoomTemplate *pTemplate, int nX, int nY, int nExitIndex, int *pExitX, int *pExitY );

//-----------------------------------------------------------------------------
// Builds a list of new open exits that would exist if this room candidate
// were placed in the world.
//-----------------------------------------------------------------------------
void BuildOpenExitList( const CRoomCandidate &roomCandidate, const CMapLayout *pMapLayout, CUtlVector< CExit > *pNewExitList );

#endif // TILEGEN_RANGES_H