//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Definitions of useful tilegen listeners which can be used to update
// free variable state based on events.
//
//===============================================================================

#include "MapLayout.h"
#include "Room.h"
#include "tilegen_layout_system.h"
#include "tilegen_listeners.h"

void CTilegenListener_NumTilesPlaced::OnBeginGeneration( const CLayoutSystem *pLayoutSystem, CFreeVariableMap *pFreeVariables )
{
	m_nTotalArea = 0; 
	m_nTotalAreaThisState = 0;
	pFreeVariables->SetOrCreateFreeVariable( "NumTilesPlaced", ( void * )m_nTotalArea );
	pFreeVariables->SetOrCreateFreeVariable( "NumTilesPlacedThisState", ( void * )m_nTotalAreaThisState );
}

void CTilegenListener_NumTilesPlaced::OnRoomPlaced( const CLayoutSystem *pLayoutSystem, CFreeVariableMap *pFreeVariables  )
{
	int nArea = pLayoutSystem->GetMapLayout()->GetLastPlacedRoom()->m_pRoomTemplate->GetArea();
	m_nTotalArea += nArea;
	m_nTotalAreaThisState += nArea;
	pFreeVariables->SetOrCreateFreeVariable( "NumTilesPlaced", ( void * )m_nTotalArea );
	pFreeVariables->SetOrCreateFreeVariable( "NumTilesPlacedThisState", ( void * )m_nTotalAreaThisState );
}

void CTilegenListener_NumTilesPlaced::OnStateChanged( const CLayoutSystem *pLayoutSystem, const CTilegenState *pOldState, CFreeVariableMap *pFreeVariables  )
{
	m_nTotalAreaThisState = 0;
	pFreeVariables->SetOrCreateFreeVariable( "NumTilesPlacedThisState", ( void * )m_nTotalAreaThisState );
}
