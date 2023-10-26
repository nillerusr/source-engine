//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Definitions of many useful tilegen actions which can be executed
// sequentially by the layout system.
//
//===============================================================================

#include "LevelTheme.h"
#include "MapLayout.h"
#include "Room.h"
#include "tilegen_layout_system.h"
#include "tilegen_ranges.h"
#include "tilegen_actions.h"


//////////////////////////////////////////////////////////////////////////
// Forward Declarations
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// Adds the given room template and position configuration to the candidate 
// list if it does not already exist in the list.
//-----------------------------------------------------------------------------
static void TryAddRoomCandidate( const CRoomCandidate &roomCandidate, CUtlVector< CRoomCandidate > *pRoomCandidateList );

//-----------------------------------------------------------------------------
// Builds a list of all room templates in a theme 
// which satisfy a filter condition.
//-----------------------------------------------------------------------------
static void BuildRoomTemplateList(
	CLayoutSystem *pLayoutSystem,
	CLevelTheme *pTheme,
	ITilegenExpression< bool > *pRoomTemplateFilter,
	bool bExcludeGlobalFilters,
	CUtlVector< const CRoomTemplate * > *pRoomTemplateList );

//-----------------------------------------------------------------------------
// Builds a list of all possible room candidates given a list of valid 
// room templates, a layout system (which contains all of the current 
// layout information), and filter predicates (plus implicit global filters).
//-----------------------------------------------------------------------------
static void BuildRoomCandidateList( 
	CLayoutSystem *pLayoutSystem, 
	const CRoomTemplate **ppRoomTemplates, 
	int nNumRoomTemplates, 
	ITilegenExpression< bool > *pExitFilter,
	ITilegenExpression< bool > *pRoomCandidateFilter,
	ITilegenAction *pRoomCandidateFilterAction,
	ITilegenExpression< bool > *pRoomCandidateFilterCondition,
	bool bExcludeGlobalFilters );

//-----------------------------------------------------------------------------
// Computes a score for coordinates based on a preferred direction.
//-----------------------------------------------------------------------------
static int ComputeScore( ExitDirection_t direction, int nX, int nY );

//////////////////////////////////////////////////////////////////////////
// Public Implementation
//////////////////////////////////////////////////////////////////////////

CTilegenAction_NestedActions::CTilegenAction_NestedActions() :
	m_pWhileCondition( NULL )
{
}

CTilegenAction_NestedActions::~CTilegenAction_NestedActions()
{
	for ( int i = 0; i < m_NestedActions.Count(); ++ i )
	{
		delete m_NestedActions[i].m_pAction;
		delete m_NestedActions[i].m_pCondition;
	}
	delete m_pWhileCondition;
}

void CTilegenAction_NestedActions::OnBeginGeneration( CLayoutSystem *pLayoutSystem )
{
	for ( int i = 0; i < m_NestedActions.Count(); ++ i )
	{
		m_NestedActions[i].m_pAction->OnBeginGeneration( pLayoutSystem );
	}
}

void CTilegenAction_NestedActions::OnStateChanged( CLayoutSystem *pLayoutSystem )
{
	for ( int i = 0; i < m_NestedActions.Count(); ++ i )
	{
		m_NestedActions[i].m_pAction->OnStateChanged( pLayoutSystem );
	}
}

bool CTilegenAction_NestedActions::LoadFromKeyValues( KeyValues *pKeyValues )
{
	for ( KeyValues *pSubKey = pKeyValues->GetFirstSubKey(); pSubKey != NULL; pSubKey = pSubKey->GetNextKey() )
	{
		if ( Q_stricmp( pSubKey->GetName(), "action" ) == 0 )
		{
			ITilegenAction *pAction;
			ITilegenExpression< bool > *pCondition;

			if ( !CreateActionAndCondition( pSubKey, &pAction, &pCondition ) )
			{
				Log_Warning( LOG_TilegenLayoutSystem, "Error creating nested action/condition pair in CTilegenAction_NestedActions.\n" );
				return false;
			}

			m_NestedActions.AddToTail( ActionConditionPair_t( pAction, pCondition ) );
		}
	}

	// Load up an optional "while" loop condition
	return CreateExpressionFromKeyValuesBlock( pKeyValues, "while", GetTypeName(), &m_pWhileCondition, true );
}

void CTilegenAction_NestedActions::Execute( CLayoutSystem *pLayoutSystem )
{
	while ( m_pWhileCondition == NULL || m_pWhileCondition->Evaluate( pLayoutSystem->GetFreeVariables() ) )
	{
		for ( int i = 0; i < m_NestedActions.Count(); ++ i )
		{
			if ( pLayoutSystem->ShouldStopProcessingActions() )
			{
				return;
			}

			pLayoutSystem->ExecuteAction( m_NestedActions[i].m_pAction, m_NestedActions[i].m_pCondition );
		}

		// If no 'while' clause is specified, treat like an 'if' 
		if ( m_pWhileCondition == NULL )
		{
			break;
		}
	}
}

CTilegenAction_AddRoomCandidates::CTilegenAction_AddRoomCandidates() :
	m_pLevelTheme( NULL ),
	m_pThemeNameExpression( NULL ),
	m_pExitFilter( NULL ),
	m_pRoomTemplateFilter( NULL ),
	m_pRoomCandidateFilter( NULL ),
	m_pRoomCandidateFilterAction( NULL ),
	m_pRoomCandidateFilterCondition( NULL ),
	m_bExcludeGlobalFilters( false )
{
}

CTilegenAction_AddRoomCandidates::~CTilegenAction_AddRoomCandidates() 
{ 
	delete m_pThemeNameExpression; 
	delete m_pExitFilter; 
	delete m_pRoomTemplateFilter; 
	delete m_pRoomCandidateFilter;
	delete m_pRoomCandidateFilterAction;
	delete m_pRoomCandidateFilterCondition;
}

bool CTilegenAction_AddRoomCandidates::LoadFromKeyValues( KeyValues *pKeyValues )
{
	bool bSuccess = true;
	bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "theme", GetTypeName(), &m_pThemeNameExpression );
	bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "exit_filter", GetTypeName(), &m_pExitFilter, true );
	bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "room_template_filter", GetTypeName(), &m_pRoomTemplateFilter, true  );
	bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "room_candidate_filter", GetTypeName(), &m_pRoomCandidateFilter, true );
	if ( pKeyValues->FindKey( "room_candidate_filter_action" ) != NULL )
	{
		bSuccess &= CreateActionAndConditionFromKeyValuesBlock( pKeyValues, "room_candidate_filter_action", GetTypeName(), &m_pRoomCandidateFilterAction, &m_pRoomCandidateFilterCondition );
	}
	m_bExcludeGlobalFilters = pKeyValues->GetBool( "exclude_global_filters", false );
	return bSuccess;
}

void CTilegenAction_AddRoomCandidates::Execute( CLayoutSystem *pLayoutSystem )
{
	if ( m_pLevelTheme == NULL )
	{
		const char *pThemeName = m_pThemeNameExpression->Evaluate( pLayoutSystem->GetFreeVariables() );
		m_pLevelTheme = CLevelTheme::FindTheme( pThemeName );
		if ( m_pLevelTheme == NULL )
		{
			Log_Warning( LOG_TilegenLayoutSystem, "Theme %s not found.\n", pThemeName );
			pLayoutSystem->OnError();
			return;
		}
	}

	CUtlVector< const CRoomTemplate * > validRoomTemplates;
	BuildRoomTemplateList( pLayoutSystem, m_pLevelTheme, m_pRoomTemplateFilter, m_bExcludeGlobalFilters, &validRoomTemplates );

	const CRoomTemplate **ppRoomTemplates = const_cast< const CRoomTemplate ** >( validRoomTemplates.Base() );
	BuildRoomCandidateList( 
		pLayoutSystem, 
		ppRoomTemplates, 
		validRoomTemplates.Count(), 
		m_pExitFilter, 
		m_pRoomCandidateFilter, 
		m_pRoomCandidateFilterAction,
		m_pRoomCandidateFilterCondition,
		m_bExcludeGlobalFilters );
}

CTilegenAction_AddRoomCandidatesAtLocation::CTilegenAction_AddRoomCandidatesAtLocation( 
	ITilegenExpression< const char * > *pThemeNameExpression, 
	ITilegenExpression< int > *pXExpression,
	ITilegenExpression< int > *pYExpression,
	ITilegenExpression< bool > *pRoomTemplateFilter ) :
	m_pLevelTheme( NULL ),
	m_pThemeNameExpression( pThemeNameExpression ),
	m_pXExpression( pXExpression ),
	m_pYExpression( pYExpression ),
	m_pRoomTemplateFilter( pRoomTemplateFilter )
{	
}

CTilegenAction_AddRoomCandidatesAtLocation::~CTilegenAction_AddRoomCandidatesAtLocation() 
{ 
	delete m_pThemeNameExpression; 
	delete m_pXExpression; 
	delete m_pYExpression; 
	delete m_pRoomTemplateFilter; 
}

bool CTilegenAction_AddRoomCandidatesAtLocation::LoadFromKeyValues( KeyValues *pKeyValues )
{
	bool bSuccess = true;
	bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "theme", GetTypeName(), &m_pThemeNameExpression );
	bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "x", GetTypeName(), &m_pXExpression );
	bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "y", GetTypeName(), &m_pYExpression );
	bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "room_template_filter", GetTypeName(), &m_pRoomTemplateFilter, true );
	return bSuccess;
}

void CTilegenAction_AddRoomCandidatesAtLocation::Execute( CLayoutSystem *pLayoutSystem )
{	
	if ( m_pLevelTheme == NULL )
	{
		const char *pThemeName = m_pThemeNameExpression->Evaluate( pLayoutSystem->GetFreeVariables() );
		if ( pThemeName == NULL )
		{
			Log_Warning( LOG_TilegenLayoutSystem, "No theme name specified.\n" );
			pLayoutSystem->OnError();
			return;
		}
		m_pLevelTheme = CLevelTheme::FindTheme( pThemeName );
		if ( m_pLevelTheme == NULL )
		{
			Log_Warning( LOG_TilegenLayoutSystem, "Theme %s not found.\n", pThemeName );
			pLayoutSystem->OnError();
			return;
		}
	}

	int nX = m_pXExpression->Evaluate( pLayoutSystem->GetFreeVariables() );
	int nY = m_pYExpression->Evaluate( pLayoutSystem->GetFreeVariables() );

	for ( int i = 0; i < m_pLevelTheme->m_RoomTemplates.Count(); ++ i )
	{
		if ( pLayoutSystem->GetMapLayout()->TemplateFits( m_pLevelTheme->m_RoomTemplates[i], nX, nY, false ) )
		{
			pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( "RoomTemplate", m_pLevelTheme->m_RoomTemplates[i] );
			if ( m_pRoomTemplateFilter == NULL || m_pRoomTemplateFilter->Evaluate( pLayoutSystem->GetFreeVariables() ) )
			{
				TryAddRoomCandidate( CRoomCandidate( m_pLevelTheme->m_RoomTemplates[i], nX, nY, NULL ), pLayoutSystem->GetRoomCandidateList() );
			}
			pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( "RoomTemplate", NULL );
		}
	}
}

bool CTilegenAction_ChooseCandidate::LoadFromKeyValues( KeyValues *pKeyValues )
{
	m_bStopProcessingActionsOnSuccess = pKeyValues->GetBool( "stop_processing", false );
	return true;
}

void CTilegenAction_ChooseCandidate::Execute( CLayoutSystem *pLayoutSystem )
{
	CUtlVector< CRoomCandidate > *pRoomCandidateList = pLayoutSystem->GetRoomCandidateList();
	if ( pRoomCandidateList->Count() == 0 )
	{
		Log_Msg( LOG_TilegenLayoutSystem, "No more room candidates to choose from.\n" );
		pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( "ChoseCandidate", ( void * )0 );
		return;
	}

	int i;
	float flChance = 0.0f;
	for ( i = 0; i < pRoomCandidateList->Count(); ++ i )
	{
		flChance += pRoomCandidateList->Element( i ).m_flCandidateChance;
	}

	float flRandom = pLayoutSystem->GetRandomFloat( 0.0f, 1.0f ) * flChance;

	for ( i = 0; i < pRoomCandidateList->Count(); ++ i )
	{
		flRandom -= pRoomCandidateList->Element( i ).m_flCandidateChance;
		if ( flRandom <= 0.0f )
		{
			break;
		}
	}
	
	if ( i == pRoomCandidateList->Count() ) 
	{
		i = pRoomCandidateList->Count() - 1;
	}
	
	const CRoomCandidate *pCandidate = &pRoomCandidateList->Element( i );

	// This should always succeed since it's in the candidate list to begin with
	bool bSuccess = pLayoutSystem->TryPlaceRoom( pCandidate );
	Assert( bSuccess );
	bSuccess;
	Log_Msg( LOG_TilegenLayoutSystem, "Chose room candidate %s at position (%d, %d).\n", pCandidate->m_pRoomTemplate->GetFullName(), pCandidate->m_iXPos, pCandidate->m_iYPos );
	
	// Empty the room candidate list for future actions
	pRoomCandidateList->RemoveAll();

	if ( m_bStopProcessingActionsOnSuccess )
	{
		pLayoutSystem->StopProcessingActions();
	}

	pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( "ChoseCandidate", ( void * )1 );
}

CTilegenAction_FilterCandidatesByDirection::CTilegenAction_FilterCandidatesByDirection( 
	ITilegenExpression< const char * > *pDirectionExpression,
	ITilegenExpression< int > *pThresholdExpression ) :
m_pDirectionExpression( pDirectionExpression ),
m_pThresholdExpression( pThresholdExpression )
{
}

CTilegenAction_FilterCandidatesByDirection::~CTilegenAction_FilterCandidatesByDirection()
{
	delete m_pDirectionExpression;
	delete m_pThresholdExpression;
}

bool CTilegenAction_FilterCandidatesByDirection::LoadFromKeyValues( KeyValues *pKeyValues )
{
	bool bSuccess = true;
	bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "direction", GetTypeName(), &m_pDirectionExpression );
	bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "threshold", GetTypeName(), &m_pThresholdExpression );
	return bSuccess;
}

void CTilegenAction_FilterCandidatesByDirection::Execute( CLayoutSystem *pLayoutSystem )
{
	CUtlVector< CRoomCandidate > *pRoomCandidateList = pLayoutSystem->GetRoomCandidateList();
	if ( pRoomCandidateList->Count() == 0 )
	{
		return;
	}

	const char *pDirection = m_pDirectionExpression->Evaluate( pLayoutSystem->GetFreeVariables() );
	int nThreshold = m_pThresholdExpression->Evaluate( pLayoutSystem->GetFreeVariables() );
	nThreshold = MAX( 0, nThreshold );
	ExitDirection_t direction = GetDirectionFromString( pDirection );
	if ( direction < EXITDIR_BEGIN || direction >= EXITDIR_END )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Invalid direction specified: %s.\n", pDirection );
		return;
	}

	// First go through and figure out the highest score
	int nHighScore = INT_MIN;
	for ( int i = 0; i < pRoomCandidateList->Count(); ++ i )
	{
		const CRoomCandidate *pCandidate = &pRoomCandidateList->Element( i );
		int nScore = ComputeScore( direction, pCandidate->m_iXPos, pCandidate->m_iYPos );
		if ( nScore > nHighScore )
		{
			nHighScore = nScore;
		}
	}

	// Now go through and set the chance of each candidate to 1.0f for any with that score or 0.0f for those with a lower score
	// @TODO: allow for specifying a numerical range in which candidates are chosen
	for ( int i = pRoomCandidateList->Count() - 1; i >= 0; -- i )
	{
		const CRoomCandidate *pCandidate = &pRoomCandidateList->Element( i );
		if ( ComputeScore( direction, pCandidate->m_iXPos, pCandidate->m_iYPos ) < ( nHighScore - nThreshold ) )
		{
			pRoomCandidateList->FastRemove( i );
		}
	}
}

bool CTilegenAction_FilterCandidatesForLinearGrowth::LoadFromKeyValues( KeyValues *pKeyValues )
{
	bool bSuccess = true;
	bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "threshold", GetTypeName(), &m_pThresholdExpression );
	return bSuccess;
}

void CTilegenAction_FilterCandidatesForLinearGrowth::Execute( CLayoutSystem *pLayoutSystem )
{
	CUtlVector< CRoomCandidate > *pRoomCandidateList = pLayoutSystem->GetRoomCandidateList();
	if ( pRoomCandidateList->Count() == 0 )
	{
		return;
	}

	int nThreshold = m_pThresholdExpression->Evaluate( pLayoutSystem->GetFreeVariables() );
	if ( nThreshold < 0 ) nThreshold = INT_MAX;
	
	// First go through and find the most recently placed source room.
	int nHighestPlacementIndex = -1;
	for ( int i = 0; i < pRoomCandidateList->Count(); ++ i )
	{
		const CRoomCandidate *pCandidate = &pRoomCandidateList->Element( i );
		const CRoom *pSourceRoom = pCandidate->m_pExit->pSourceRoom;
		if ( pSourceRoom->m_nPlacementIndex > nHighestPlacementIndex )
		{
			nHighestPlacementIndex = pSourceRoom->m_nPlacementIndex;
		}
	}

	CMapLayout *pMapLayout = pLayoutSystem->GetMapLayout();
	int nMinimumPlacementIndex = pMapLayout->m_PlacedRooms.Count() - 1 - nThreshold;

	// Now go through and remove any candidates not within the threshold of the most recently placed source room.
	for ( int i = pRoomCandidateList->Count() - 1; i >= 0; -- i )
	{
		const CRoomCandidate *pCandidate = &pRoomCandidateList->Element( i );
		if ( pCandidate->m_pExit->pSourceRoom->m_nPlacementIndex < nHighestPlacementIndex || pCandidate->m_pExit->pSourceRoom->m_nPlacementIndex < nMinimumPlacementIndex )
		{
			pRoomCandidateList->FastRemove( i );
		}
	}
}

CTilegenAction_SwitchState::CTilegenAction_SwitchState() : 
m_pNewStateExpression( NULL ) 
{ 
}

CTilegenAction_SwitchState::~CTilegenAction_SwitchState()
{
	delete m_pNewStateExpression;
}

bool CTilegenAction_SwitchState::LoadFromKeyValues( KeyValues *pKeyValues )
{
	CreateExpressionFromKeyValuesBlock( pKeyValues, "new_state", "CTilegenAction_SwitchState", &m_pNewStateExpression, true );
	// m_pNewStateExpression can be NULL, in which case, simply go to the next sequential state.
	return true;
}

void CTilegenAction_SwitchState::Execute( CLayoutSystem *pLayoutSystem )
{
	if ( m_pNewStateExpression == NULL )
	{
		// Advance to next state
		CTilegenState *pCurrentState = pLayoutSystem->GetCurrentState();
		CTilegenState *pNextState = pCurrentState->GetNextState();
		if ( pNextState == NULL )
		{
			// Must be the last state
			pLayoutSystem->OnFinished();
			return;
		}
		pLayoutSystem->TransitionToState( pNextState );
	}
	else
	{
		pLayoutSystem->TransitionToState( m_pNewStateExpression->Evaluate( pLayoutSystem->GetFreeVariables() ) ); 
	}
}

void CTilegenAction_FinishGeneration::Execute( CLayoutSystem *pLayoutSystem ) 
{ 
	pLayoutSystem->OnFinished(); 
}

void CTilegenAction_EpicFail::Execute( CLayoutSystem *pLayoutSystem )
{
	pLayoutSystem->OnError();
}

bool CTilegenAction_EnsureRoomExists::LoadFromKeyValues( KeyValues *pKeyValues )
{
	return CreateExpressionFromKeyValuesBlock( pKeyValues, "roomname", GetTypeName(), &m_pRoomNameExpression );
}

void CTilegenAction_EnsureRoomExists::Execute( CLayoutSystem *pLayoutSystem )
{
	const char *pFullRoomName = m_pRoomNameExpression ? m_pRoomNameExpression->Evaluate( pLayoutSystem->GetFreeVariables() ) : NULL;
	if ( !pFullRoomName )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Room name expression invalid.\n" );
		pLayoutSystem->OnError();
		return;
	}
	char themeName[MAX_TILEGEN_IDENTIFIER_LENGTH];
	char roomName[MAX_TILEGEN_IDENTIFIER_LENGTH];
	if ( !CLevelTheme::SplitThemeAndRoom( pFullRoomName, themeName, MAX_TILEGEN_IDENTIFIER_LENGTH, roomName, MAX_TILEGEN_IDENTIFIER_LENGTH ) )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Could not split theme name from room (full name: %s).\n", pFullRoomName );
		pLayoutSystem->OnError();
		return;
	}
	CLevelTheme *pLevelTheme = CLevelTheme::FindTheme( themeName );
	if ( pLevelTheme == NULL )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Theme %s not found.\n", themeName );
		pLayoutSystem->OnError();
		return;
	}
	if ( pLevelTheme->FindRoom( roomName ) == NULL )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Room %s not found.\n", roomName );
		pLayoutSystem->OnError();
		return;
	}
}

CTilegenAction_AddConnectorRoomCandidates::CTilegenAction_AddConnectorRoomCandidates( ITilegenAction *pAddRoomCandidates ) :
m_pLevelTheme( NULL ),
m_pTargetThemeNameExpression( NULL ),
m_pTargetRoomTemplateFilter( NULL ),
m_pTargetRoomTemplate( NULL ),
m_pAddConnectorCandidates( pAddRoomCandidates )
{
}

CTilegenAction_AddConnectorRoomCandidates::~CTilegenAction_AddConnectorRoomCandidates()
{
	delete m_pTargetThemeNameExpression;
	delete m_pTargetRoomTemplateFilter;
	delete m_pAddConnectorCandidates;
}

void CTilegenAction_AddConnectorRoomCandidates::SetRoomTemplate( const CRoomTemplate *pTargetRoomTemplate )
{
	Assert( m_pAddConnectorCandidates != NULL && m_pTargetRoomTemplateFilter == NULL && m_pTargetThemeNameExpression == NULL );
	m_pTargetRoomTemplate = pTargetRoomTemplate;
}

bool CTilegenAction_AddConnectorRoomCandidates::LoadFromKeyValues( KeyValues *pKeyValues )
{
	bool bSuccess = true;
	bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "theme", GetTypeName(), &m_pTargetThemeNameExpression );
	bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "room_template_filter", GetTypeName(), &m_pTargetRoomTemplateFilter );
	
	ITilegenExpression< bool > *pCondition;
	KeyValues *pAddConnectorRoomCandidatesKV = pKeyValues->FindKey( "connector_room_candidates" );
	if ( pAddConnectorRoomCandidatesKV == NULL )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Key 'connector_room_candidates' not found when parsing 'CTilegenAction_AddConnectorRoomCandidates'.\n" );
		return false;
	}

	bSuccess &= CreateActionAndConditionFromKeyValuesBlock( pAddConnectorRoomCandidatesKV, "action", GetTypeName(), &m_pAddConnectorCandidates, &pCondition );
	if ( pCondition != NULL )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Action specified in 'connector_room_candidates' block of 'CTilegenAction_AddConnectorRoomCandidates' must not have a nested condition.\n" );
		return false;
	}
	
	return bSuccess;
}

void CTilegenAction_AddConnectorRoomCandidates::Execute( CLayoutSystem *pLayoutSystem )
{
	CUtlVector< const CRoomTemplate * > roomTemplateList;
	if ( m_pTargetRoomTemplate != NULL )
	{
		Assert( m_pTargetRoomTemplateFilter == NULL && m_pTargetThemeNameExpression == NULL );
		roomTemplateList.AddToTail( m_pTargetRoomTemplate );
	}
	else
	{
		Assert( m_pTargetRoomTemplateFilter != NULL	&& m_pTargetThemeNameExpression != NULL );

		if ( m_pLevelTheme == NULL )
		{
			const char *pThemeName = m_pTargetThemeNameExpression->Evaluate( pLayoutSystem->GetFreeVariables() );
			m_pLevelTheme = CLevelTheme::FindTheme( pThemeName );
			if ( m_pLevelTheme == NULL )
			{
				Log_Warning( LOG_TilegenLayoutSystem, "Theme %s not found.\n", pThemeName );
				pLayoutSystem->OnError();
				return;
			}
		}

		BuildRoomTemplateList( pLayoutSystem, m_pLevelTheme, m_pTargetRoomTemplateFilter, true, &roomTemplateList );
	}

	// Build a list of exit types we're looking for
	CUtlVector< CExit > desiredMatchingExits;
	for ( int i = 0; i < roomTemplateList.Count(); ++ i )
	{
		const CRoomTemplate *pTemplate = roomTemplateList[i];
		for ( int j = 0; j < pTemplate->m_Exits.Count(); ++ j )
		{
			desiredMatchingExits.AddToTail( CExit( 
				0, 0, CRoomTemplateExit::GetOppositeDirection( pTemplate->m_Exits[j]->m_ExitDirection ), 
				pTemplate->m_Exits[j]->m_szExitTag, NULL, false ) );
		}
	}

	// Build up a room of connector candidates
	pLayoutSystem->ExecuteAction( m_pAddConnectorCandidates, NULL );

	// Filter the set down by eliminating candidates which don't connect to the desired direction.
	CUtlVector< CRoomCandidate > *pRoomCandidateList = pLayoutSystem->GetRoomCandidateList();
	CUtlVector< CExit > newOpenExits;
	for ( int i = pRoomCandidateList->Count() - 1; i >= 0; -- i )
	{
		bool bMatch = false;
		newOpenExits.RemoveAll();

		// Figure out which new exits would be open as a result of placing this room candidate.
		BuildOpenExitList( pRoomCandidateList->Element( i ), pLayoutSystem->GetMapLayout(), &newOpenExits );

		// For every new open exit potentially created by this candidate,
		// see if one of them could connect to one of our desired exits.
		for ( int j = 0; j < newOpenExits.Count(); ++ j )
		{
			const CExit *pNewOpenExit = &newOpenExits[j];
			for ( int k = 0; k < desiredMatchingExits.Count(); ++ k )
			{
				const CExit *pDesiredMatchingExit = &desiredMatchingExits[k];
				if ( pNewOpenExit->ExitDirection == pDesiredMatchingExit->ExitDirection && 
					 Q_stricmp( pNewOpenExit->m_szExitTag, pDesiredMatchingExit->m_szExitTag ) == 0 )
				{
					// Found a match!
					bMatch = true;
					break;
				}
			}
			if ( bMatch ) break;
		}

		if ( !bMatch )
		{
			pRoomCandidateList->FastRemove( i );
		}
	}
}

CTilegenAction_PlaceComponent::CTilegenAction_PlaceComponent() :
m_nMinOptionalRooms( 0 ),
m_nMaxOptionalRooms( 0 ),
m_pExitFilter( NULL ),
m_pRoomCandidateFilter( NULL ),
m_pRoomCandidateFilterAction( NULL ),
m_pRoomCandidateFilterCondition( NULL ),
m_pAddConnectorRoomCandidates( NULL ),
m_pChooseCandidate( NULL ),
m_bExcludeGlobalFilters( false )
{
}

CTilegenAction_PlaceComponent::~CTilegenAction_PlaceComponent()
{
	delete m_pExitFilter;
	delete m_pRoomCandidateFilter;
	delete m_pRoomCandidateFilterAction;
	delete m_pRoomCandidateFilterCondition;
	delete m_pAddConnectorRoomCandidates;
	delete m_pChooseCandidate;
}

void CTilegenAction_PlaceComponent::OnBeginGeneration( CLayoutSystem *pLayoutSystem )
{
	int nNumOptionalRooms = pLayoutSystem->GetRandomInt( m_nMinOptionalRooms, m_nMaxOptionalRooms );
	nNumOptionalRooms = MIN( nNumOptionalRooms, m_OptionalRooms.Count() );
	nNumOptionalRooms = MAX( nNumOptionalRooms, 0 );

	m_RoomsToPlace.RemoveAll();
	for ( int i = 0; i < m_MandatoryRooms.Count(); ++ i )
	{
		AddRoomPlacementInstance( pLayoutSystem, &m_MandatoryRooms[i] );
	}

	bool isRoomChosen[m_nMaxTotalOptionalRooms];
	Q_memset( isRoomChosen, 0, sizeof( isRoomChosen ) );

	// Simplest but probably not the most efficient way to randomly choose N rooms from a list of X rooms.
	int nNumOptionalRoomsChosen = 0;
	while ( nNumOptionalRoomsChosen < nNumOptionalRooms )
	{
		int nRoom = pLayoutSystem->GetRandomInt( 0, nNumOptionalRooms - 1 );
		if ( !isRoomChosen[nRoom] )
		{
			isRoomChosen[nRoom] = true;
			AddRoomPlacementInstance( pLayoutSystem, &m_OptionalRooms[nRoom] );
			++ nNumOptionalRoomsChosen;
		}
	}
}

bool CTilegenAction_PlaceComponent::LoadFromKeyValues( KeyValues *pKeyValues )
{
	KeyValues *pMandatoryRoomsKV = pKeyValues->FindKey( "mandatory_rooms" );
	if ( pMandatoryRoomsKV != NULL )
	{
		LoadRoomPlacementsFromKeyValues( pMandatoryRoomsKV, &m_MandatoryRooms );
	}

	KeyValues *pOptionalRoomsKV = pKeyValues->FindKey( "optional_rooms" );
	if ( pOptionalRoomsKV != NULL )
	{
		LoadRoomPlacementsFromKeyValues( pOptionalRoomsKV, &m_OptionalRooms );
	}

	if ( m_OptionalRooms.Count() >= m_nMaxTotalOptionalRooms )
	{
		return false;
	}

	m_nMinOptionalRooms = pKeyValues->GetInt( "min", 0 );
	m_nMaxOptionalRooms = pKeyValues->GetInt( "max", 0 );
	m_bExcludeGlobalFilters = pKeyValues->GetBool( "exclude_global_filters", false );

	// Load optional exit & room candidate filters.
	bool bSuccess = true;
	bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "exit_filter", GetTypeName(), &m_pExitFilter, true );
	bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "room_candidate_filter", GetTypeName(), &m_pRoomCandidateFilter, true );
	if ( pKeyValues->FindKey( "room_candidate_filter_action" ) != NULL )
	{
		bSuccess &= CreateActionAndConditionFromKeyValuesBlock( pKeyValues, "room_candidate_filter_action", GetTypeName(), &m_pRoomCandidateFilterAction, &m_pRoomCandidateFilterCondition );
	}

	// Load up the nested "connector_room_candidates" block.
	// This is the same block found in a CTilegenAction_AddConnectorRoomCandidates instance.
	// This block contains logic to add room candidates 
	KeyValues *pAddConnectorRoomCandidatesKV = pKeyValues->FindKey( "connector_room_candidates" );
	if ( pAddConnectorRoomCandidatesKV != NULL )
	{	
		ITilegenAction *pAction;
		ITilegenExpression< bool > *pCondition;
		bSuccess &= CreateActionAndConditionFromKeyValuesBlock( pAddConnectorRoomCandidatesKV, "action", GetTypeName(), &pAction, &pCondition );
		if ( pCondition != NULL )
		{
			Log_Warning( LOG_TilegenLayoutSystem, "Action specified in 'connector_room_candidates' block of %s must not have a nested condition.\n", GetTypeName() );
			return false;
		}

		m_pAddConnectorRoomCandidates = new CTilegenAction_AddConnectorRoomCandidates( pAction );
	}

	m_pChooseCandidate = new CTilegenAction_ChooseCandidate( false );

	return bSuccess;
}

void CTilegenAction_PlaceComponent::Execute( CLayoutSystem *pLayoutSystem )
{
	int nTotalArea;
	int nNumTilesPlaced;
	CFreeVariableMap *pFreeVariables = pLayoutSystem->GetFreeVariables();
	bool bInGlobalActionState = ( pFreeVariables->GetFreeVariableDisallowNULL( "CurrentState" ) == pLayoutSystem->GetGlobalActionState() );
	const char *pNumTilesPlacedVariableName;
	const char *pTotalAreaVariableName;
	if ( bInGlobalActionState )
	{
		// Fractions are portions of the entire map's area.  This relies on the "TotalGenerationArea" variable to have been set.
		// The CTilegenListener_NumTilesPlaced listener must also be registered.
		pTotalAreaVariableName = "TotalGenerationArea";
		pNumTilesPlacedVariableName = "NumTilesPlaced";
	}
	else
	{
		// Fractions are portions of this step's total area.  This relies on the "StepGenerationArea" variable to have been set for the current state.
		// The CTilegenListener_NumTilesPlaced listener must also be registered.
		pTotalAreaVariableName = "StepGenerationArea";
		pNumTilesPlacedVariableName = "NumTilesPlacedThisState";
	}

	nTotalArea = ( int )pFreeVariables->GetFreeVariableOrNULL( pTotalAreaVariableName );
	nNumTilesPlaced = ( int )pFreeVariables->GetFreeVariableOrNULL( pNumTilesPlacedVariableName );

	if ( nTotalArea <= 0 )
	{
		// Nothing has been placed yet, do not attempt to place component pieces
		Log_Msg( LOG_TilegenLayoutSystem, "Ignoring PlaceComponent action since no tiles have been placed yet.\n" );
		pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( "PlacedComponent", ( void * )0 );
		return;
	}

	// Go through the sorted list of rooms to place and attempt to place 1 room
	for ( int i = 0; i < m_RoomsToPlace.Count(); ++ i )
	{
		if ( !m_RoomsToPlace[i].m_bPlaced )
		{
			int nTileThreshold = ( int )( m_RoomsToPlace[i].m_flPlacementFraction * ( float )nTotalArea );
			if ( nNumTilesPlaced >= nTileThreshold )
			{
				if ( PlaceRoom( pLayoutSystem, m_RoomsToPlace[i].pInfo->m_pRoomTemplate ) )
				{
					m_RoomsToPlace[i].m_bPlaced = true;
					pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( "PlacedComponent", ( void * )1 );
					return;
				}
				else
				{
					pLayoutSystem->OnError();
					return;
				}
			}
		}
	}
	pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( "PlacedComponent", ( void * )0 );
}

void CTilegenAction_PlaceComponent::AddRoomPlacementInstance( CLayoutSystem *pLayoutSystem, RoomPlacementInfo_t *pRoomPlacementInfo )
{
	RoomPlacementInstance_t instance;
	instance.pInfo = pRoomPlacementInfo;
	instance.m_bPlaced = false;

	if ( pRoomPlacementInfo->m_flPlacementFraction < 0.0f )
	{
		// Randomly place level between 5% and 95% of the way through the generation process
		instance.m_flPlacementFraction = pLayoutSystem->GetRandomFloat( 0.05, 0.95 );
	}
	else
	{
		instance.m_flPlacementFraction = MAX( 0.0f, MIN( 1.0f, pRoomPlacementInfo->m_flPlacementFraction ) );
	}

	m_RoomsToPlace.Insert( instance );
}

bool CTilegenAction_PlaceComponent::LoadRoomPlacementsFromKeyValues( KeyValues *pKeyValues, CUtlVector< RoomPlacementInfo_t > *pRooms )
{
	for ( KeyValues *pRoomKV = pKeyValues->GetFirstSubKey(); pRoomKV != NULL; pRoomKV = pRoomKV->GetNextKey() )
	{
		if ( Q_stricmp( pRoomKV->GetName(), "room" ) == 0 )
		{
			const char *pFullRoomName = pRoomKV->GetString( "room_name", NULL );
			if ( pFullRoomName == NULL )
			{
				return false;
			}

			// A negative or omitted value means random placement
			float flFraction = pRoomKV->GetFloat( "fraction", -1.0f );

			pRooms->AddToTail();
			RoomPlacementInfo_t *pRoom = &pRooms->Element( pRooms->Count() - 1 );

			pRoom->m_flPlacementFraction = flFraction;

			char themeName[MAX_TILEGEN_IDENTIFIER_LENGTH];
			char roomName[MAX_TILEGEN_IDENTIFIER_LENGTH];
			if ( !CLevelTheme::SplitThemeAndRoom( pFullRoomName, themeName, MAX_TILEGEN_IDENTIFIER_LENGTH, roomName, MAX_TILEGEN_IDENTIFIER_LENGTH ) )
			{
				Log_Warning( LOG_TilegenLayoutSystem, "Could not split theme name from room (full name: %s).\n", pFullRoomName );
				return false;
			}

			CLevelTheme *pTheme = CLevelTheme::FindTheme( themeName );
			if ( pTheme == NULL )
			{
				Log_Warning( LOG_TilegenLayoutSystem, "Theme %s not found.\n", themeName );
				return false;
			}

			pRoom->m_pRoomTemplate = pTheme->FindRoom( roomName );
			if ( pRoom->m_pRoomTemplate == NULL )
			{
				Log_Warning( LOG_TilegenLayoutSystem, "Room %s not found.\n", roomName );
				return false;
			}
		}
	}
	return true;
}

bool CTilegenAction_PlaceComponent::PlaceRoom( CLayoutSystem *pLayoutSystem, const CRoomTemplate *pRoomTemplate )
{
	int nNumTries = 0;

	while ( nNumTries < 10 )
	{
		BuildRoomCandidateList( 
			pLayoutSystem, 
			&pRoomTemplate,
			1,
			m_pExitFilter, 
			m_pRoomCandidateFilter, 
			m_pRoomCandidateFilterAction,
			m_pRoomCandidateFilterCondition,
			m_bExcludeGlobalFilters );

		CUtlVector< CRoomCandidate > *pRoomCandidateList = pLayoutSystem->GetRoomCandidateList();
		if ( pRoomCandidateList->Count() > 0 )
		{
			pLayoutSystem->ExecuteAction( m_pChooseCandidate, NULL );
			return true;
		}
		else
		{
			Log_Msg( LOG_TilegenLayoutSystem, "Unable to place component room %s, attempting to place connector room.\n", pRoomTemplate->GetFullName() );
			if ( !PlaceConnectorRoom( pLayoutSystem, pRoomTemplate ) )
			{
				Log_Warning( LOG_TilegenLayoutSystem, "Unable to place connector piece to connect to component room %s.\n", pRoomTemplate->GetFullName() );
				return false;
			}
		}

		++ nNumTries;
	}

	Log_Warning( LOG_TilegenLayoutSystem, "Unable to place component room %s after %d tries.\n", pRoomTemplate->GetFullName(), nNumTries );
	return false;
}

bool CTilegenAction_PlaceComponent::PlaceConnectorRoom( CLayoutSystem *pLayoutSystem, const CRoomTemplate *pRoomTemplate )
{
	if ( m_pAddConnectorRoomCandidates == NULL )
	{
		// No sub-rule specified to handle populating room candidates for a connector piece
		return false;
	}

	m_pAddConnectorRoomCandidates->SetRoomTemplate( pRoomTemplate );

	Assert( pLayoutSystem->GetRoomCandidateList()->Count() == 0 );
	pLayoutSystem->ExecuteAction( m_pAddConnectorRoomCandidates, NULL );

	int nNumPlacedRooms = pLayoutSystem->GetMapLayout()->m_PlacedRooms.Count();
	if ( pLayoutSystem->GetRoomCandidateList()->Count() > 0 )
	{
		pLayoutSystem->ExecuteAction( m_pChooseCandidate, NULL );
		Assert( pLayoutSystem->GetMapLayout()->m_PlacedRooms.Count() == ( nNumPlacedRooms + 1 ) );
		nNumPlacedRooms;
		return true;
	}
	else
	{
		return false;
	}
}

CTilegenAction_AddInstances::CTilegenAction_AddInstances() :
	m_pInstanceCount( NULL ),
	m_pRoomTemplateFilter( NULL )
{
}

CTilegenAction_AddInstances::~CTilegenAction_AddInstances()
{
	delete m_pInstanceCount;
	delete m_pRoomTemplateFilter;
}

bool CTilegenAction_AddInstances::LoadFromKeyValues( KeyValues *pKeyValues )
{
	if ( !m_InstanceSpawn.LoadFromKeyValues( pKeyValues ) )
	{
		return false;
	}

	bool bSuccess = true;
	bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "instance_count", GetTypeName(), &m_pInstanceCount );
	bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "room_template_filter", GetTypeName(), &m_pRoomTemplateFilter, true );
	return bSuccess;
}

void CTilegenAction_AddInstances::Execute( CLayoutSystem *pLayoutSystem )
{
	CFreeVariableMap *pFreeVariables = pLayoutSystem->GetFreeVariables();
	int nInstanceCount = m_pInstanceCount->Evaluate( pFreeVariables );
	CMapLayout *pMapLayout = pLayoutSystem->GetMapLayout();

	CUtlVector< int > validCandidates;
	for ( int i = 0; i < pMapLayout->m_PlacedRooms.Count(); ++ i )
	{
		pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( "RoomTemplate", ( void * )pMapLayout->m_PlacedRooms[i]->m_pRoomTemplate );
		if ( m_pRoomTemplateFilter == NULL || m_pRoomTemplateFilter->Evaluate( pFreeVariables ) )
		{
			validCandidates.AddToTail( i );
		}
		pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( "RoomTemplate", NULL );
	}

	if ( validCandidates.Count() == 0 )
	{
		// No valid candidates found, bail
		Log_Warning( LOG_TilegenLayoutSystem, "No valid candidates found for rule %s.\n", GetTypeName() );
		return;
	}

	for ( int i = 0; i < nInstanceCount; ++ i )
	{
		CInstanceSpawn *pInstanceSpawn = &pMapLayout->m_InstanceSpawns[pMapLayout->m_InstanceSpawns.AddToTail( m_InstanceSpawn )];
		char indexString[15];

		Q_snprintf( indexString, _countof( indexString ), "%02d", i );
		pInstanceSpawn->FixupValues( "%N", indexString );
		pInstanceSpawn->SetPlacedRoomIndex( validCandidates[pLayoutSystem->GetRandomInt( 0, validCandidates.Count() - 1 )] );
		pInstanceSpawn->SetRandomSeed( pLayoutSystem->GetRandomInt( 1, 1000000000 ) );
	}
}

CTilegenAction_AddInstanceToRoom::CTilegenAction_AddInstanceToRoom() :
m_pRoomExpression( NULL )
{
}

CTilegenAction_AddInstanceToRoom::~CTilegenAction_AddInstanceToRoom()
{
	delete m_pRoomExpression;
}

bool CTilegenAction_AddInstanceToRoom::LoadFromKeyValues( KeyValues *pKeyValues )
{
	if ( !m_InstanceSpawn.LoadFromKeyValues( pKeyValues ) )
	{
		return false;
	}

	return CreateExpressionFromKeyValuesBlock( pKeyValues, "room", GetTypeName(), &m_pRoomExpression );
}

void CTilegenAction_AddInstanceToRoom::Execute( CLayoutSystem *pLayoutSystem )
{
	const CRoom *pRoom = m_pRoomExpression->Evaluate( pLayoutSystem->GetFreeVariables() );
	if ( pRoom != NULL )
	{
		CMapLayout *pMapLayout = pLayoutSystem->GetMapLayout();
		CInstanceSpawn *pInstanceSpawn = &pMapLayout->m_InstanceSpawns[pMapLayout->m_InstanceSpawns.AddToTail( m_InstanceSpawn )];
		pInstanceSpawn->SetPlacedRoomIndex( pRoom->m_nPlacementIndex );
		pInstanceSpawn->SetRandomSeed( pLayoutSystem->GetRandomInt( 1, 1000000000 ) );
	}
}

CTilegenAction_LoadLayout::CTilegenAction_LoadLayout()
{
	m_LayoutFilename[0] = '\0';
}

bool CTilegenAction_LoadLayout::LoadFromKeyValues( KeyValues *pKeyValues )
{
	const char *pFilename = pKeyValues->GetString( "filename", NULL );
	if ( pFilename == NULL )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "No 'filename' specified in %s.\n", GetTypeName() );
		return false;
	}
	Q_strncpy( m_LayoutFilename, pFilename, _countof( m_LayoutFilename ) );
	return true;
}

void CTilegenAction_LoadLayout::Execute( CLayoutSystem *pLayoutSystem )
{
	CMapLayout *pMapLayout = new CMapLayout();
	if ( !pMapLayout->LoadMapLayout( m_LayoutFilename ) )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Unable to load map layout '%s'.\n", m_LayoutFilename );
		pLayoutSystem->OnError();
		return;
	}

	for ( int i = 0; i < pMapLayout->m_PlacedRooms.Count(); ++ i )
	{
		CRoom *pRoom = pMapLayout->m_PlacedRooms[i];
		CRoomCandidate roomCandidate( pRoom->m_pRoomTemplate, pRoom->m_iPosX, pRoom->m_iPosY, NULL );
		if ( !pLayoutSystem->TryPlaceRoom( &roomCandidate ) )
		{
			Log_Warning( LOG_TilegenLayoutSystem, "Unable to place room template '%s' at position (%d, %d) based on data from layout file '%s'.\n", 
				pRoom->m_pRoomTemplate->GetFullName(), 
				pRoom->m_iPosX,
				pRoom->m_iPosY,
				m_LayoutFilename );
			pLayoutSystem->OnError();
			return;
		}
		Log_Msg( LOG_TilegenLayoutSystem, "Chose room candidate %s at position (%d, %d).\n", pRoom->m_pRoomTemplate->GetFullName(), pRoom->m_iPosX, pRoom->m_iPosY );
	}
}

//////////////////////////////////////////////////////////////////////////
// Helper function implementations
//////////////////////////////////////////////////////////////////////////

void TryAddRoomCandidate( const CRoomCandidate &roomCandidate, CUtlVector< CRoomCandidate > *pRoomCandidateList )
{
	int i;
	for ( i = 0; i < pRoomCandidateList->Count(); ++ i )
	{
		const CRoomCandidate *pRoomCandidate = &(*pRoomCandidateList)[i];
		if ( pRoomCandidate->m_pRoomTemplate == roomCandidate.m_pRoomTemplate && pRoomCandidate->m_iXPos == roomCandidate.m_iXPos && pRoomCandidate->m_iYPos == roomCandidate.m_iYPos && pRoomCandidate->m_pExit == roomCandidate.m_pExit )
		{
			// Already exists in list
			return;
		}
	}

	pRoomCandidateList->AddToTail( roomCandidate );
	// Start with a default (equal) chance of 1.0
	pRoomCandidateList->Element( i ).m_flCandidateChance = 1.0f;
}

void BuildRoomTemplateList( 
	CLayoutSystem *pLayoutSystem, 
	CLevelTheme *pTheme,
	ITilegenExpression< bool > *pRoomTemplateFilter, 
	bool bExcludeGlobalFilters, 
	CUtlVector< const CRoomTemplate * > *pRoomTemplateList )
{
	// Build a list of valid room templates from the theme's full list.
	for ( int i = 0; i < pTheme->m_RoomTemplates.Count(); ++ i )
	{
		// Set the room template to the global variable "RoomTemplate", which gets used by room_template_filter rules.
		pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( "RoomTemplate", pTheme->m_RoomTemplates[i] );

		// Filter the room templates against the action's room_template_filter and global filter (if not being excluded).
		ITilegenExpression< bool > *pGlobalRoomTemplateFilter = 
			bExcludeGlobalFilters ? NULL : ( ITilegenExpression< bool > * )pLayoutSystem->GetFreeVariables()->GetFreeVariableOrNULL( "GlobalRoomTemplateFilters" );
		if ( ( pRoomTemplateFilter == NULL || pRoomTemplateFilter->Evaluate( pLayoutSystem->GetFreeVariables() ) ) &&
			( pGlobalRoomTemplateFilter == NULL || pGlobalRoomTemplateFilter->Evaluate( pLayoutSystem->GetFreeVariables() ) ) )
		{
			pRoomTemplateList->AddToTail( pTheme->m_RoomTemplates[i] );
		}
		pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( "RoomTemplate", NULL );
	}
}

void BuildRoomCandidateList( 
	CLayoutSystem *pLayoutSystem, 
	const CRoomTemplate **ppRoomTemplates, 
	int nNumRoomTemplates, 
	ITilegenExpression< bool > *pExitFilter,
	ITilegenExpression< bool > *pRoomCandidateFilter,
	ITilegenAction *pRoomCandidateFilterAction,
	ITilegenExpression< bool > *pRoomCandidateFilterCondition,
	bool bExcludeGlobalFilters )
{
	CUtlVector< CRoomCandidate > *pRoomCandidateList = pLayoutSystem->GetRoomCandidateList();

	// For every exit, test every given room template in every position.
	for ( int i = 0; i < pLayoutSystem->GetNumOpenExits(); ++ i )
	{
		const CExit *pExit = pLayoutSystem->GetOpenExits() + i;
		
		// Set the exit to the global variable "Exit", which gets used by exit_filter rules.
		pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( "Exit", ( void * )pExit );
		
		// Actions/rules can choose to ignore the global exit/room filters.
		ITilegenExpression< bool > *pGlobalExitFilter = 
			bExcludeGlobalFilters ? NULL : ( ITilegenExpression< bool > * )pLayoutSystem->GetFreeVariables()->GetFreeVariableOrNULL( "GlobalExitFilters" );

		// Test this exit with the action's exit filter and the global exit filter.
		if ( ( pExitFilter == NULL || pExitFilter->Evaluate( pLayoutSystem->GetFreeVariables() ) ) && 
			 ( pGlobalExitFilter == NULL || pGlobalExitFilter->Evaluate( pLayoutSystem->GetFreeVariables() ) ) )
		{
			// Go through each room template to see if it fits at this exit.
			for ( int j = 0; j < nNumRoomTemplates; ++ j )
			{
				const CRoomTemplate *pRoomTemplate = ppRoomTemplates[j];
				
				// Set the room template to the global variable "RoomTemplate", which gets used by room_template_filter / room_candidate_filter rules.
				pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( "RoomTemplate", ( void * )pRoomTemplate );

				// @TODO: Add rotation support here by testing 4 directions if tile is rotateable

				// Test this template in all possible locations that overlap the exit square.
				for ( int x = 0; x < pRoomTemplate->GetTilesX(); ++ x )
				{
					for ( int y = 0; y < pRoomTemplate->GetTilesY(); ++ y )
					{
						// If the template fits, test it against the filters.
						if ( pLayoutSystem->GetMapLayout()->TemplateFits( pRoomTemplate, pExit->X - x, pExit->Y - y, false ) )
						{
							CRoomCandidate roomCandidate( pRoomTemplate, pExit->X - x, pExit->Y - y, pExit );

							// Set the room candidate to the global variable "RoomCandidate", which gets used by room_candidate_filter rules.
							pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( "RoomCandidate", &roomCandidate );

							// Actions/rules can choose to ignore the global exit/room filters.
							ITilegenExpression< bool > *pGlobalRoomCandidateFilter = 
								bExcludeGlobalFilters ? NULL : ( ITilegenExpression< bool > * )pLayoutSystem->GetFreeVariables()->GetFreeVariableOrNULL( "GlobalRoomCandidateFilters" );

							if ( ( pRoomCandidateFilter == NULL || pRoomCandidateFilter->Evaluate( pLayoutSystem->GetFreeVariables() ) ) &&
								 ( pGlobalRoomCandidateFilter == NULL || pGlobalRoomCandidateFilter->Evaluate( pLayoutSystem->GetFreeVariables() ) ) )
							{
								TryAddRoomCandidate( roomCandidate, pRoomCandidateList );
							}
							pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( "RoomCandidate", NULL );
						}
					}
				}

				pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( "RoomTemplate", NULL );
			}
		}
		pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( "Exit", NULL );
	}

	// Pare the list of room candidates down by applying room candidate filter actions (a more general version of room candidate filters).
	ITilegenAction *pGlobalCandidateFilterAction = bExcludeGlobalFilters ? NULL : ( ITilegenAction * )pLayoutSystem->GetFreeVariables()->GetFreeVariableOrNULL( "GlobalRoomCandidateFilterActions" );
	if ( pGlobalCandidateFilterAction != NULL )
	{
		pGlobalCandidateFilterAction->Execute( pLayoutSystem );
	}

	if ( pRoomCandidateFilterAction != NULL && 
		( pRoomCandidateFilterCondition == NULL || pRoomCandidateFilterCondition->Evaluate( pLayoutSystem->GetFreeVariables() ) ) )
	{
		pRoomCandidateFilterAction->Execute( pLayoutSystem );
	}
}

int ComputeScore( ExitDirection_t direction, int nX, int nY )
{
	switch( direction )
	{
	case EXITDIR_NORTH:
		return nY;
	case EXITDIR_SOUTH:
		return -nY;
	case EXITDIR_EAST:
		return nX;
	case EXITDIR_WEST:
		return -nX;
	}
	return INT_MIN;
}
