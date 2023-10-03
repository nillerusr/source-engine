//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Definitions of useful tilegen array types which can be iterated.
//
//===============================================================================

#include "utlvector.h"
#include "RoomTemplate.h"
#include "MapLayout.h"

#include "tilegen_ranges.h"
#include "tilegen_class_factories.h"
#include "tilegen_expressions.h"

//////////////////////////////////////////////////////////////////////////
// Public Implementation
//////////////////////////////////////////////////////////////////////////

void BuildOpenExitList( const CRoomCandidate &roomCandidate, const CMapLayout *pMapLayout, CUtlVector< CExit > *pNewExitList )
{
	Assert( pMapLayout );
	const CRoomTemplate *pTemplate = roomCandidate.m_pRoomTemplate;
	for ( int i = 0; i < pTemplate->m_Exits.Count(); ++ i )
	{
		int nExitX, nExitY;
		if ( GetExitPosition( pTemplate, roomCandidate.m_iXPos, roomCandidate.m_iYPos, i, &nExitX, &nExitY ) )
		{
			if ( !pMapLayout->GetRoom( nExitX, nExitY ) )
			{
				pNewExitList->AddToTail( CExit( nExitX, nExitY, pTemplate->m_Exits[i]->m_ExitDirection, pTemplate->m_Exits[i]->m_szExitTag, NULL, false ) );
			}
		}
	}
}

bool GetExitPosition( const CRoomTemplate *pTemplate, int nX, int nY, int nExitIndex, int *pExitX, int *pExitY )
{
	int nDeltaX, nDeltaY;
	CRoomTemplateExit::GetExitOffset( pTemplate->m_Exits[nExitIndex]->m_ExitDirection, &nDeltaX, &nDeltaY );
	*pExitX = nX + pTemplate->m_Exits[nExitIndex]->m_iXPos + nDeltaX;
	// @TODO: fix the flipped coordinate frame for exits
	*pExitY = nY + ( ( pTemplate->GetTilesY() - 1 ) - ( pTemplate->m_Exits[nExitIndex]->m_iYPos + nDeltaY ) );

	// Exit goes off the edge of the map, but the room fits, so pretend the exit doesn't exist.
	// @TODO: does this mean that the room abuts the bounding-box around the world? or do we need to cap it somehow?
	if ( *pExitX < 0 || *pExitY < 0 || *pExitX >= MAP_LAYOUT_TILES_WIDE || *pExitY >= MAP_LAYOUT_TILES_WIDE )
	{
		return false;
	}

	return true;
}

CTilegenRange_NewOpenExits::CTilegenRange_NewOpenExits( ITilegenExpression< const CRoomCandidate * > *pRoomCandidateExpression ) :
	m_pRoomCandidateExpression( pRoomCandidateExpression ),
	m_nCurrentExit( 0 )
{
}

CTilegenRange_NewOpenExits::~CTilegenRange_NewOpenExits() 
{ 
	delete m_pRoomCandidateExpression; 
}

bool CTilegenRange_NewOpenExits::LoadFromKeyValues( KeyValues *pKeyValues ) 
{ 
	return CreateExpressionFromKeyValuesBlock( pKeyValues, "expression", "CTilegenRange_NewOpenExits", &m_pRoomCandidateExpression ); 
}

void CTilegenRange_NewOpenExits::Initialize( CFreeVariableMap *pContext )
{
	m_Exits.RemoveAll();
	const CRoomCandidate *pRoomCandidate = m_pRoomCandidateExpression->Evaluate( pContext );
	if ( pRoomCandidate != NULL )
	{
		BuildOpenExitList( *pRoomCandidate, ( const CMapLayout * )pContext->GetFreeVariableDisallowNULL( "MapLayout" ), &m_Exits );
	}
	m_nCurrentExit = m_Exits.Count();
}

void CTilegenRange_NewOpenExits::Reset()
{
	m_nCurrentExit = -1;
}

bool CTilegenRange_NewOpenExits::MoveNext()
{
	++ m_nCurrentExit;
	return m_nCurrentExit < m_Exits.Count();
}

const CExit * CTilegenRange_NewOpenExits::GetCurrent()
{
	return &m_Exits[m_nCurrentExit];
}

CTilegenRange_ClosedExits::CTilegenRange_ClosedExits( ITilegenExpression< const CRoomCandidate * > *pRoomCandidateExpression) :
	m_pRoomCandidateExpression( pRoomCandidateExpression ),
	m_nCurrentExit( 0 )
{
}

CTilegenRange_ClosedExits::~CTilegenRange_ClosedExits()
{ 
	delete m_pRoomCandidateExpression; 
}

bool CTilegenRange_ClosedExits::LoadFromKeyValues( KeyValues *pKeyValues )
{ 
	return CreateExpressionFromKeyValuesBlock( pKeyValues, "expression", "CTilegenRange_ClosedExits", &m_pRoomCandidateExpression ); 
}

void CTilegenRange_ClosedExits::Initialize( CFreeVariableMap *pContext )
{
	m_Exits.RemoveAll();
	const CRoomCandidate *pRoomCandidate = m_pRoomCandidateExpression->Evaluate( pContext );
	if ( pRoomCandidate != NULL )
	{
		CUtlVector< CRoomTemplateExit * > matchingExits;
		( ( const CMapLayout * )pContext->GetFreeVariableDisallowNULL( "MapLayout" ) )->CheckExits( pRoomCandidate->m_pRoomTemplate, pRoomCandidate->m_iXPos, pRoomCandidate->m_iYPos, &matchingExits );
		for ( int i = 0; i < matchingExits.Count(); ++ i )
		{
			m_Exits.AddToTail( CExit( matchingExits[i]->m_iXPos + pRoomCandidate->m_iXPos, pRoomCandidate->m_pRoomTemplate->GetTilesY() - 1 - matchingExits[i]->m_iYPos + pRoomCandidate->m_iYPos, matchingExits[i]->m_ExitDirection, matchingExits[i]->m_szExitTag, NULL, false ) );
		}
	}
	m_nCurrentExit = m_Exits.Count();
}

void CTilegenRange_ClosedExits::Reset()
{
	m_nCurrentExit = -1;
}

bool CTilegenRange_ClosedExits::MoveNext()
{
	++ m_nCurrentExit;
	return m_nCurrentExit < m_Exits.Count();
}

const CExit * CTilegenRange_ClosedExits::GetCurrent()
{
	return &m_Exits[m_nCurrentExit];
}
