//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Definitions for the rule- and state-based level generation system.
//
//===============================================================================

#include "MapLayout.h"
#include "Room.h"
#include "tilegen_class_factories.h"
#include "tilegen_mission_preprocessor.h"
#include "tilegen_listeners.h"
#include "tilegen_ranges.h"
#include "tilegen_layout_system.h"
#include "asw_npcs.h"

ConVar tilegen_break_on_iteration( "tilegen_break_on_iteration", "-1", FCVAR_CHEAT, "If set to a non-negative value, tilegen will break at the start of iteration #N if a debugger is attached." );
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_TilegenLayoutSystem, "TilegenLayoutSystem", 0, LS_MESSAGE, Color( 192, 255, 192, 255 ) );

void AddListeners( CLayoutSystem *pLayoutSystem )
{
	// @TODO: need a better mechanism to detect which of these listeners are required.  For now, add them all.
	pLayoutSystem->AddListener( new CTilegenListener_NumTilesPlaced() );
}

CTilegenState *CTilegenStateList::FindState( const char *pStateName )
{
	for ( int i = 0; i < m_States.Count(); ++ i )
	{
		if ( Q_stricmp( pStateName, m_States[i]->GetStateName() ) == 0 )
		{
			return m_States[i];
		}
		CTilegenState *pState = m_States[i]->GetStateList()->FindState( pStateName );
		if ( pState != NULL )
		{
			return pState;
		}
	}
	return NULL;
}

CTilegenState *CTilegenStateList::GetNextState( CTilegenState *pState )
{
	int i;
	for ( i = 0; i < m_States.Count(); ++ i )
	{
		if ( m_States[i] == pState )
			break;
	}

	if ( i < ( m_States.Count() - 1 ) )
	{
		return m_States[i + 1];
	}
	else if ( i == m_States.Count() )
	{
		// This should never happen unless there's a bug in the layout code
		Log_Warning( LOG_TilegenLayoutSystem, "State %s not found in state list.\n", pState->GetStateName() );
		m_pLayoutSystem->OnError();
		return NULL;
	}
	else if ( i == ( m_States.Count() - 1 ) )
	{
		// We're on the last state
		CTilegenStateList *pParentStateList = GetParentStateList();
		if ( pParentStateList == NULL )
		{
			// No more states left in the layout system.
			return NULL;
		}
		else
		{
			Assert( pState->GetParentState() != NULL );
			return pParentStateList->GetNextState( pState->GetParentState() );
		}
	}
	UNREACHABLE();
	return NULL;
}

CTilegenStateList *CTilegenStateList::GetParentStateList()
{
	if ( m_pOwnerState == NULL )
	{
		// No parent state implies that this state list is owned by the layout system, so there
		// is no parent state list.
		return NULL;
	}
	else if ( m_pOwnerState->GetParentState() == NULL )
	{
		// A parent state with a NULL parent state implies that this list is owned by a top-level
		// state, so the parent state list belongs to the layout system.
		return m_pLayoutSystem->GetStateList();
	}
	else
	{
		return m_pOwnerState->GetParentState()->GetStateList();
	}
}

void CTilegenStateList::OnBeginGeneration()
{
	for ( int i = 0; i < m_States.Count(); ++ i )
	{
		m_States[i]->OnBeginGeneration( m_pLayoutSystem );
	}
}

CTilegenState::~CTilegenState()
{
	for ( int i = 0; i < m_Actions.Count(); ++ i )
	{
		delete m_Actions[i].m_pAction;
		delete m_Actions[i].m_pCondition;
	}
}

bool CTilegenState::LoadFromKeyValues( KeyValues *pKeyValues )
{
	const char *pStateName = pKeyValues->GetString( "name", NULL );
	if ( pStateName == NULL )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "No state name found in State key values.\n" );
		return false;
	}
	
	m_StateName[0] = '\0';
	// @TODO: support nested state names?
	// 	if ( pParentStateName != NULL )
	// 	{
	// 		Q_strncat( m_StateName, pParentStateName, MAX_TILEGEN_IDENTIFIER_LENGTH );
	// 		Q_strncat( m_StateName, ".", MAX_TILEGEN_IDENTIFIER_LENGTH );
	// 	}
	Q_strncat( m_StateName, pStateName, MAX_TILEGEN_IDENTIFIER_LENGTH );

	for ( KeyValues *pSubKey = pKeyValues->GetFirstSubKey(); pSubKey != NULL; pSubKey = pSubKey->GetNextKey() )
	{
		if ( Q_stricmp( pSubKey->GetName(), "action" ) == 0 )
		{
			if ( !ParseAction( pSubKey ) )
			{
				return false;
			}
		}
		else if ( Q_stricmp( pSubKey->GetName(), "state") == 0 )
		{
			// Nested state
			CTilegenState *pNewState = new CTilegenState( GetLayoutSystem(), this );
			if ( !pNewState->LoadFromKeyValues( pSubKey ) )
			{
				delete pNewState;
				return false;
			}
			m_ChildStates.AddState( pNewState );
		}
	}
	return true;
}

void CTilegenState::ExecuteIteration( CLayoutSystem *pLayoutSystem )
{
	if ( m_Actions.Count() == 0 )
	{
		// No actions in this state, go to the first nested state if one exists
		// or move on to the next sibling state.
		if ( m_ChildStates.GetStateCount() > 0 )
		{
			Log_Msg( LOG_TilegenLayoutSystem, "No actions found in state %s, transitioning to first child state.\n", GetStateName() );
			pLayoutSystem->TransitionToState( m_ChildStates.GetState( 0 ) );
		}
		else
		{
			// Try to switch to the next sibling state.
			CTilegenState *pNextState = m_ChildStates.GetParentStateList()->GetNextState( this );
			if ( pNextState != NULL )
			{
				Log_Msg( LOG_TilegenLayoutSystem, "No actions or child states found in state %s, transitioning to next state.\n", GetStateName() );
				pLayoutSystem->TransitionToState( pNextState );
			}
			else
			{
				// This must be the last state in the entire layout system.
				Log_Msg( LOG_TilegenLayoutSystem, "No more states to which to transition." );
				pLayoutSystem->OnFinished();
			}
		}
	}
	else
	{
		pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( "CurrentState", this );
		for ( int i = 0; i < m_Actions.Count(); ++ i )
		{
			if ( pLayoutSystem->ShouldStopProcessingActions() )
			{
				break;
			}

			pLayoutSystem->ExecuteAction( m_Actions[i].m_pAction, m_Actions[i].m_pCondition );
		}
		pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( "CurrentState", NULL );
	}
}

void CTilegenState::OnBeginGeneration( CLayoutSystem *pLayoutSystem )
{
	for ( int i = 0; i < m_Actions.Count(); ++ i )
	{
		m_Actions[i].m_pAction->OnBeginGeneration( pLayoutSystem );
	}

	m_ChildStates.OnBeginGeneration();
}

void CTilegenState::OnStateChanged( CLayoutSystem *pLayoutSystem )
{
	for ( int i = 0; i < m_Actions.Count(); ++ i )
	{
		m_Actions[i].m_pAction->OnStateChanged( pLayoutSystem );
	}
}

bool CTilegenState::ParseAction( KeyValues *pKeyValues )
{
	ITilegenAction *pNewAction = NULL;
	ITilegenExpression< bool > *pCondition = NULL;
	if ( CreateActionAndCondition( pKeyValues, &pNewAction, &pCondition ) )
	{
		AddAction( pNewAction, pCondition );
		return true;
	}
	else
	{
		return false;
	}
}

CLayoutSystem::CLayoutSystem() :
	m_nRandomSeed( 0 ),
	m_pGlobalActionState( NULL ),
	m_pCurrentState( NULL ),
	m_pMapLayout( NULL ),
	m_ActionData( DefLessFunc( ITilegenAction *) ),
	m_bLayoutError( false ),
	m_bGenerating( false ),
	m_nIterations( 0 )
{
	m_States.SetLayoutSystem( this );
}

CLayoutSystem::~CLayoutSystem()
{
	m_TilegenListeners.PurgeAndDeleteElements();
	delete m_pGlobalActionState;
}

bool CLayoutSystem::LoadFromKeyValues( KeyValues *pKeyValues )
{
	// Make sure all tilegen class factories have been registered.
	RegisterAllTilegenClasses();

	for ( KeyValues *pSubKey = pKeyValues->GetFirstSubKey(); pSubKey != NULL; pSubKey = pSubKey->GetNextKey() )
	{
		if ( Q_stricmp( pSubKey->GetName(), "state" ) == 0 )
		{
			CTilegenState *pNewState = new CTilegenState( this, NULL );
			if ( !pNewState->LoadFromKeyValues( pSubKey ) )
			{
				delete pNewState;
				return false;
			}
			m_States.AddState( pNewState );
		} 
		else if ( Q_stricmp( pSubKey->GetName(), "action" ) == 0 )
		{
			// Global actions, executed at the beginning of every state
			ITilegenAction *pNewAction = NULL;
			ITilegenExpression< bool > *pCondition = NULL;
			if ( CreateActionAndCondition( pSubKey, &pNewAction, &pCondition ) )
			{
				if ( m_pGlobalActionState == NULL )
				{
					m_pGlobalActionState = new CTilegenState( this, NULL );
				}
				m_pGlobalActionState->AddAction( pNewAction, pCondition );
			}
			else
			{
				return false;
			}
		}
		else if ( Q_stricmp( pSubKey->GetName(), "mission_settings" ) == 0 )
		{
			m_nRandomSeed = pSubKey->GetInt( "RandomSeed", 0 );
		}
	}
	return true;
}

// @TODO: add rotation/mirroring support here by adding rotation argument
bool CLayoutSystem::TryPlaceRoom( const CRoomCandidate *pRoomCandidate )
{
	int nX = pRoomCandidate->m_iXPos;
	int nY = pRoomCandidate->m_iYPos;
	const CRoomTemplate *pRoomTemplate = pRoomCandidate->m_pRoomTemplate;
	CRoom *pSourceRoom = pRoomCandidate->m_pExit ? pRoomCandidate->m_pExit->pSourceRoom : NULL;

	if ( m_pMapLayout->TemplateFits( pRoomTemplate, nX, nY, false ) )
	{
		// This has the side-effect of attaching itself to the layout; no need to keep track of it.
		CRoom *pRoom = new CRoom( m_pMapLayout, pRoomTemplate, nX, nY );

		// Remove exits covered up by the room
		for ( int i = m_OpenExits.Count() - 1; i >= 0; -- i )
		{
			CExit *pExit = &m_OpenExits[i];
			if ( pExit->X >= nX && pExit->Y >= nY &&
				 pExit->X < nX + pRoomTemplate->GetTilesX() &&
				 pExit->Y < nY + pRoomTemplate->GetTilesY() )
			{
				m_OpenExits.FastRemove( i );
			}
		}

		if ( pSourceRoom != NULL )
		{
			++ pSourceRoom->m_iNumChildren;
		}

		AddOpenExitsFromRoom( pRoom );
		OnRoomPlaced();

		GetFreeVariables()->SetOrCreateFreeVariable( "LastPlacedRoomTemplate", ( void * )pRoomTemplate );
		return true;
	}

	return false;
}

void CLayoutSystem::TransitionToState( const char *pStateName )
{
	CTilegenState *pState = m_States.FindState( pStateName );
	if ( pState == NULL )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Tilegen state %s not found.\n", pStateName );
		OnError();
	}
	else
	{
		TransitionToState( pState );
	}
}

void CLayoutSystem::TransitionToState( CTilegenState *pState )
{	
	CTilegenState *pOldState = m_pCurrentState;
	m_pCurrentState = pState;
	m_CurrentIterationState.m_bStopIteration = true;
	OnStateChanged( pOldState );
	Log_Msg( LOG_TilegenLayoutSystem, "Transitioning to state %s.\n", pState->GetStateName() );
	StopProcessingActions();
}

void CLayoutSystem::OnError()
{
	m_bLayoutError = true;
	m_bGenerating = false;
	Log_Warning( LOG_TilegenLayoutSystem, "An error occurred during level generation.\n" );
}

void CLayoutSystem::OnFinished()
{
	m_bGenerating = false;
	m_CurrentIterationState.m_bStopIteration = true;
	Log_Msg( LOG_TilegenLayoutSystem, "CLayoutSystem: finished layout generation.\n" );

	// Temp hack to setup fixed alien spawns
	// TODO: Move this into a required rule
	CASWMissionChooserNPCs::InitFixedSpawns( this, m_pMapLayout );
}

void CLayoutSystem::ExecuteAction( ITilegenAction *pAction, ITilegenExpression< bool > *pCondition )
{
	Assert( pAction != NULL );

	// Since actions can be nested, only expose the inner-most nested action.
	ITilegenAction *pOldAction = ( ITilegenAction * )GetFreeVariables()->GetFreeVariableOrNULL( "Action" );
	GetFreeVariables()->SetOrCreateFreeVariable( "Action", pAction );
	
	// Get data associated with the current action and set it to free variables
	int nIndex = m_ActionData.Find( pAction );
	if ( nIndex == m_ActionData.InvalidIndex() )
	{
		ActionData_t actionData = { 0 };
		actionData.m_nNumTimesExecuted = 0;
		nIndex = m_ActionData.Insert( pAction, actionData );
		Assert( nIndex != m_ActionData.InvalidIndex() );
	}
	GetFreeVariables()->SetOrCreateFreeVariable( "NumTimesExecuted", ( void * )m_ActionData[nIndex].m_nNumTimesExecuted );

	// Execute the action if the condition is met
	if ( pCondition == NULL || pCondition->Evaluate( GetFreeVariables() ) )
	{
		// Log_Msg( LOG_TilegenLayoutSystem, "Executing action %s.\n", pAction->GetTypeName() );
		pAction->Execute( this );
		++ m_ActionData[nIndex].m_nNumTimesExecuted;
		OnActionExecuted( pAction );
	}

	// Restore free variable state
	GetFreeVariables()->SetOrCreateFreeVariable( "Action", pOldAction );
	nIndex = m_ActionData.Find( pOldAction );
	Assert( nIndex != m_ActionData.InvalidIndex() );
	GetFreeVariables()->SetOrCreateFreeVariable( "NumTimesExecuted", ( void * )m_ActionData[nIndex].m_nNumTimesExecuted );
}

void CLayoutSystem::OnActionExecuted( const ITilegenAction *pAction )
{
	for ( int i = 0; i < m_TilegenListeners.Count(); ++ i )
	{
		m_TilegenListeners[i]->OnActionExecuted( this, pAction, GetFreeVariables() );
	}
}

void CLayoutSystem::OnRoomPlaced()
{
	for ( int i = 0; i < m_TilegenListeners.Count(); ++ i )
	{
		m_TilegenListeners[i]->OnRoomPlaced( this, GetFreeVariables() );
	}
}

void CLayoutSystem::OnStateChanged( const CTilegenState *pOldState )
{
	for ( int i = 0; i < m_TilegenListeners.Count(); ++ i )
	{
		m_TilegenListeners[i]->OnStateChanged( this, pOldState, GetFreeVariables() );
	}

	m_pCurrentState->OnStateChanged( this );
}

void CLayoutSystem::BeginGeneration( CMapLayout *pMapLayout ) 
{ 
	if ( m_States.GetStateCount() == 0 )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "No states in layout system!\n" );
		OnError();
		return;
	}

	// Reset random generator
	int nSeed;
	m_Random = CUniformRandomStream();
	if ( m_nRandomSeed != 0 )
	{
		nSeed = m_nRandomSeed;
	}
	else
	{
		// @TODO: this is rather unscientific, but it gets us desired randomness for now.
		nSeed = RandomInt( 1, 1000000000 );
	}

	m_Random.SetSeed( nSeed );
	Log_Msg( LOG_TilegenLayoutSystem, "Beginning generation with random seed " );
	Log_Msg( LOG_TilegenLayoutSystem, Color( 255, 255, 0, 255 ), "%d.\n", nSeed );

	m_bLayoutError = false;
	m_bGenerating = true; 
	m_nIterations = 0;
	m_pMapLayout = pMapLayout; 
	m_pCurrentState = m_States.GetState( 0 );
	m_OpenExits.RemoveAll();
	m_ActionData.RemoveAll();
	m_FreeVariables.RemoveAll();

	// Initialize with sentinel value
	ActionData_t nullActionData = { 0 };
	m_ActionData.Insert( NULL, nullActionData );
	
	for ( int i = 0; i < m_TilegenListeners.Count(); ++ i )
	{
		m_TilegenListeners[i]->OnBeginGeneration( this, GetFreeVariables() );
	}

	if ( m_pGlobalActionState != NULL )
	{
		m_pGlobalActionState->OnBeginGeneration( this );
	}
	
	m_States.OnBeginGeneration();
	
	GetFreeVariables()->SetOrCreateFreeVariable( "LayoutSystem", this );
	GetFreeVariables()->SetOrCreateFreeVariable( "MapLayout", GetMapLayout() );
}

void CLayoutSystem::ExecuteIteration()
{
	Assert( m_pCurrentState != NULL && m_pMapLayout != NULL );

	const int nMaxIterations = 200;
	if ( m_nIterations > nMaxIterations )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Exceeded %d iterations, may be in an infinite loop.\n", nMaxIterations );
		OnError();
		return;
	}
	
	// Debugging assistant
	int nBreakOnIteration = tilegen_break_on_iteration.GetInt();
	if ( nBreakOnIteration >= 0 )
	{
		Log_Msg( LOG_TilegenLayoutSystem, "Iteration #%d\n", m_nIterations );
	}
	if ( m_nIterations == nBreakOnIteration )
	{
		DebuggerBreakIfDebugging();
	}

	// Initialize state scoped to the current iteration
	m_CurrentIterationState.m_bStopIteration = false;
	CUtlVector< CRoomCandidate > roomCandidateList;
	m_CurrentIterationState.m_pRoomCandidateList = &roomCandidateList;

	// Execute global actions first, if any are present.
	if ( m_pGlobalActionState != NULL )
	{
		m_pGlobalActionState->ExecuteIteration( this );
	}

	// Now process actions in the current state
	if ( !ShouldStopProcessingActions() )
	{
		// Executing an iteration may have a number of side-effects on the layout system,
		// such as placing rooms, changing state, etc.
		m_pCurrentState->ExecuteIteration( this );
	}

	++ m_nIterations;

	m_CurrentIterationState.m_pRoomCandidateList = NULL;
}

// This function adds an open exit for every un-mated exit in the given room.
// The exits are added to the grid tile where a new room would connect
// (e.g., a north exit from a room tile at (x, y) is actually added to location (x, y+1)
void CLayoutSystem::AddOpenExitsFromRoom( CRoom *pRoom )
{
	const CRoomTemplate *pTemplate = pRoom->m_pRoomTemplate;

	// Go through each exit in the room.
	for ( int i = 0; i < pTemplate->m_Exits.Count(); ++ i )
	{
		const CRoomTemplateExit *pExit = pTemplate->m_Exits[i];
		int nExitX, nExitY;
		if ( GetExitPosition( pTemplate, pRoom->m_iPosX, pRoom->m_iPosY, i, &nExitX, &nExitY ) )
		{
			// If no room exists where the exit interface goes, then consider the exit open.
			// NOTE: this function assumes that the caller has already verified that the template fits, which means
			// that if a room exists where this exit goes, it must be a matching exit (and thus not open).
			if ( !m_pMapLayout->GetRoom( nExitX, nExitY ) )
			{
				AddOpenExit( pRoom, nExitX, nExitY, pExit->m_ExitDirection, pExit->m_szExitTag, pExit->m_bChokepointGrowSource );
			}
		}
	}
}

void CLayoutSystem::AddOpenExit( CRoom *pSourceRoom, int nX, int nY, ExitDirection_t exitDirection, const char *pExitTag, bool bChokepointGrowSource )
{
	// Check to make sure exit isn't already in the list.
	for ( int i = 0; i < m_OpenExits.Count(); ++ i )
	{
		const CExit *pExit = &m_OpenExits[i];
		if ( pExit->X == nX && pExit->Y == nY && pExit->ExitDirection == exitDirection )
		{
			// Exit already exists.
			Assert( pExit->pSourceRoom == pSourceRoom && Q_stricmp( pExitTag, pExit->m_szExitTag ) == 0 );
			return;
		}
	}

	m_OpenExits.AddToTail( CExit( nX, nY, exitDirection, pExitTag, pSourceRoom, bChokepointGrowSource ) );
}
