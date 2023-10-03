//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Declarations for the rule- and state-based level generation system.
//
//===============================================================================

#ifndef TILEGEN_LAYOUT_SYSTEM_H
#define TILEGEN_LAYOUT_SYSTEM_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "utlvector.h"
#include "vstdlib/random.h"
#include "tilegen_class_interfaces.h"
#include "tilegen_expressions.h"
#include "RoomTemplate.h"

class CMapLayout;
class CTilegenState;
class CLayoutSystem;

//-----------------------------------------------------------------------------
// Adds all known tilegen listeners to the given layout system.
//-----------------------------------------------------------------------------
void AddListeners( CLayoutSystem *pLayoutSystem );

//-----------------------------------------------------------------------------
// A list of states in a state layout system state machine.
// Each state may contain a nested state list.
// A state list is owned either by a state or by the root-level layout system.
//-----------------------------------------------------------------------------
class CTilegenStateList
{
public:
	explicit CTilegenStateList( CLayoutSystem *pLayoutSystem = NULL ) : m_pOwnerState( NULL ), m_pLayoutSystem( pLayoutSystem ) { }
	~CTilegenStateList() { m_States.PurgeAndDeleteElements(); }

	//-----------------------------------------------------------------------------
	// Finds a state with the given name (either in this class or in one of 
	// its children). The search is depth-first.
	// Returns NULL if not found.
	//-----------------------------------------------------------------------------
	CTilegenState *FindState( const char *pStateName );

	//-----------------------------------------------------------------------------
	// Gets the next state in the list after the specified state.
	//-----------------------------------------------------------------------------
	CTilegenState *GetNextState( CTilegenState *pState );

	int GetStateCount() const { return m_States.Count(); }
	CTilegenState *GetState( int i ) { return m_States[i]; }

	void AddState( CTilegenState *pState ) { m_States.AddToTail( pState ); }

	CLayoutSystem *GetLayoutSystem() { return m_pLayoutSystem; }
	void SetLayoutSystem( CLayoutSystem *pLayoutSystem ) { m_pLayoutSystem = pLayoutSystem; }
	CTilegenStateList *GetParentStateList();
	void SetOwnerState( CTilegenState *pParent ) { m_pOwnerState = pParent; }

	//-----------------------------------------------------------------------------
	// Called when beginning generation of a new map.
	//-----------------------------------------------------------------------------
	void OnBeginGeneration();

private:
	// A list of contained states.
	CUtlVector< CTilegenState * > m_States;

	// This is NULL if the state list is owned by the layout system.
	CTilegenState *m_pOwnerState;

	CLayoutSystem *m_pLayoutSystem;
};

struct ActionConditionPair_t
{
	ActionConditionPair_t( ITilegenAction *pAction, ITilegenExpression< bool > *pCondition ) :
	m_pCondition( pCondition ),
	m_pAction( pAction)
	{
	}

	ITilegenAction *m_pAction;
	ITilegenExpression< bool > *m_pCondition;
};

//-----------------------------------------------------------------------------
// A node in the layout system state machine which contains a list of 
// actions (and optional conditions) to execute in linear order. 
// States may contain nested state lists.
//-----------------------------------------------------------------------------
class CTilegenState
{
public:
	CTilegenState( CLayoutSystem *pLayoutSystem, CTilegenState *pParentState ) :
		m_ChildStates( pLayoutSystem ),
		m_pParentState( pParentState )
	{
		m_StateName[0] = '\0';
		m_ChildStates.SetOwnerState( this );
	}

	~CTilegenState();

	//-----------------------------------------------------------------------------
	// Parses a state from KeyValues data.
	//-----------------------------------------------------------------------------
	bool LoadFromKeyValues( KeyValues *pKeyValues );

	//-----------------------------------------------------------------------------
	// Executes a single iteration of logic in this state of the state machine.
	//-----------------------------------------------------------------------------
	void ExecuteIteration( CLayoutSystem *pLayoutSystem );

	//-----------------------------------------------------------------------------
	// Called when beginning generation of a new map.
	//-----------------------------------------------------------------------------
	void OnBeginGeneration( CLayoutSystem *pLayoutSystem );

	//-----------------------------------------------------------------------------
	// Called when changing to this state.
	//-----------------------------------------------------------------------------
	void OnStateChanged( CLayoutSystem *pLayoutSystem );

	void AddAction( ITilegenAction *pAction, ITilegenExpression< bool > *pCondition = NULL ) { m_Actions.AddToTail( ActionConditionPair_t( pAction, pCondition ) ); }
	int GetActionCount() { return m_Actions.Count(); }

	CTilegenStateList *GetStateList() { return &m_ChildStates; }
	const char *GetStateName() const { return m_StateName; }
	CTilegenState *GetNextState() { return m_ChildStates.GetParentStateList()->GetNextState( this ); }
	
	CTilegenState *GetParentState() const { return m_pParentState; }
	CLayoutSystem *GetLayoutSystem() { return m_ChildStates.GetLayoutSystem(); }

private:
	bool ParseAction( KeyValues *pKeyValues );

	// A list of conditional actions in execution-order (e.g. m_Actions[0] runs first)
	CUtlVector< ActionConditionPair_t > m_Actions;

	CTilegenStateList m_ChildStates;

	// The parent state, or NULL if this is owned directly by the layout system.
	CTilegenState *m_pParentState;

	char m_StateName[MAX_TILEGEN_IDENTIFIER_LENGTH];
};

//-----------------------------------------------------------------------------
// A state machine which iteratively executes actions in states
// in order to populate a map layout (CMapLayout).
//-----------------------------------------------------------------------------
class CLayoutSystem
{
public:
	CLayoutSystem();
	~CLayoutSystem();

	//-----------------------------------------------------------------------------
	// Parses a full state machine from KeyValues data.
	//-----------------------------------------------------------------------------
	bool LoadFromKeyValues( KeyValues *pKeyValues );

	CMapLayout *GetMapLayout() { return m_pMapLayout; }
	const CMapLayout *GetMapLayout() const { return m_pMapLayout; }
	CUtlVector< CRoomCandidate > *GetRoomCandidateList() const { return m_CurrentIterationState.m_pRoomCandidateList; }
	CTilegenStateList *GetStateList() { return &m_States; }
	CTilegenState *GetCurrentState() const { return m_pCurrentState; }
	CTilegenState *GetGlobalActionState() const { return m_pGlobalActionState; }

	void AddListener( ITilegenListener *pListener ) { m_TilegenListeners.AddToTail( pListener ); }
	
	//-----------------------------------------------------------------------------
	// Gets a list of currently open (un-matched) exits.
	// This pointer should never be cached; map layout changes may invalidate it.
	//-----------------------------------------------------------------------------
	const CExit * GetOpenExits() const { return m_OpenExits.Base(); }
	int GetNumOpenExits() const { return m_OpenExits.Count(); }

	//-----------------------------------------------------------------------------
	// Gets the dictionary of variable values.
	//-----------------------------------------------------------------------------
	CFreeVariableMap *GetFreeVariables() { return &m_FreeVariables; }

	//-----------------------------------------------------------------------------
	// Instructs the system to no longer process further actions this iteration.
	//-----------------------------------------------------------------------------
	void StopProcessingActions() { m_CurrentIterationState.m_bStopIteration = true; }

	//-----------------------------------------------------------------------------
	// Gets a value indicating whether to continue processing further 
	// actions this iteration.
	//-----------------------------------------------------------------------------
	bool ShouldStopProcessingActions() const { return m_CurrentIterationState.m_bStopIteration || m_bLayoutError; }

	//-----------------------------------------------------------------------------
	// Gets a value indicating whether the layout system is in the process of
	// generating a level or not.
	//-----------------------------------------------------------------------------
	bool IsGenerating() const { return m_bGenerating; }

	//-----------------------------------------------------------------------------
	// Gets a value indicating whether an error occurred during the layout process.
	//-----------------------------------------------------------------------------
	bool GenerationErrorOccurred() const { return m_bLayoutError; }

	//-----------------------------------------------------------------------------
	// Gets a value indicating whether this map is randomly generated or
	// fixed to a particular seed.
	// A seed of 0 implies that a new seed is chosen for each generation while
	// a non-zero seed implies that the same seed should be chosen each time.
	//-----------------------------------------------------------------------------
	bool IsRandomlyGenerated() const { return m_nRandomSeed == 0; }

	//-----------------------------------------------------------------------------
	// Attempts to place a room in the map layout.
	//
	// Returns true on success, false on failure.
	//
	// Reasons for failure include: room out of bounds, room overlapping
	// with existing room, room exit mismatch.
	//-----------------------------------------------------------------------------
	bool TryPlaceRoom( const CRoomCandidate *pRoomCandidate );

	//-----------------------------------------------------------------------------
	// Generates a random integer value X in the range [nMin, nMax], inclusive.
	//
	// All actions or rules which require randomness must use CLayoutSystem
	// random functions so that "randomness" is reproducible from a given seed.
	//-----------------------------------------------------------------------------
	int GetRandomInt( int nMin, int nMax ) { return m_Random.RandomInt( nMin, nMax ); }

	//-----------------------------------------------------------------------------
	// Generates a random float value X in the range [nMin, nMax], inclusive.
	//
	// All actions or rules which require randomness must use CLayoutSystem
	// random functions so that "randomness" is reproducible from a given seed.
	//-----------------------------------------------------------------------------
	float GetRandomFloat( float flMin, float flMax ) { return m_Random.RandomFloat( flMin, flMax ); }

	//-----------------------------------------------------------------------------
	// Changes the current state to the state with the given name and 
	// stops processing further actions.
	//-----------------------------------------------------------------------------
	void TransitionToState( const char *pStateName );

	//-----------------------------------------------------------------------------
	// Changes the current state to the specified state object and stops 
	// processing further actions.
	//-----------------------------------------------------------------------------
	void TransitionToState( CTilegenState *pState );

	//-----------------------------------------------------------------------------
	// Notifies the system taht an error occurred during level generation 
	// to signal a failure and stop further processing.
	//-----------------------------------------------------------------------------
	void OnError();

	//-----------------------------------------------------------------------------
	// Notifies the system that level generation is complete and stops
	// further processing.
	//-----------------------------------------------------------------------------
	void OnFinished();

	//-----------------------------------------------------------------------------
	// Executes the specified action with a given condition.  
	// If pCondition is NULL, the action is always executed.
	//-----------------------------------------------------------------------------
	void ExecuteAction( ITilegenAction *pAction, ITilegenExpression< bool > *pCondition );

	//-----------------------------------------------------------------------------
	// Resets the layout system and begins generating a new map.
	// Once this is called, the system must be ticked by calling ExecuteIteration
	// until IsGeneration() returns false.
	//-----------------------------------------------------------------------------
	void BeginGeneration( CMapLayout *pMapLayout );

	//-----------------------------------------------------------------------------
	// Executes a single pass through the actions in the global & current state.
	// Executing an iteration may have a number of side-effects on the layout 
	// system such as placing rooms, setting variables, changing states, etc.
	//-----------------------------------------------------------------------------
	void ExecuteIteration();

private:
	void OnActionExecuted( const ITilegenAction *pAction );
	void OnRoomPlaced();
	void OnStateChanged( const CTilegenState *pOldState );

	void AddOpenExitsFromRoom( CRoom *pRoom );
	void AddOpenExit( CRoom *pSourceRoom, int nX, int nY, ExitDirection_t exitDirection, const char *pExitTag, bool bChokepointGrowSource );

	// A random number generator which must be used by any actions/conditions which need randomness.
	CUniformRandomStream m_Random;
	// Starting random seed of the map.  If set to 0, pick a completely arbitrary one using the global random generator.
	int m_nRandomSeed;

	// A set of top-level states.
	CTilegenStateList m_States;

	// A special/sentinel state which contains global actions that get executed on every iteration.
	CTilegenState *m_pGlobalActionState;

	// Current state in the state machine.
	// Points to a member of m_States or a nested state contained within.
	CTilegenState *m_pCurrentState;

	// Exits which are currently open.
	CUtlVector< CExit > m_OpenExits;

	// Stores a name-value map of data which can be accessed by expressions
	CFreeVariableMap m_FreeVariables;

	// A list of objects listening for tilegen events which can affect free variable state.
	CUtlVector< ITilegenListener * > m_TilegenListeners;

	// The map layout to be built over time (not owned by this class)
	CMapLayout *m_pMapLayout;

	struct ActionData_t
	{
		int m_nNumTimesExecuted;
	};

	CUtlMap< ITilegenAction *, ActionData_t > m_ActionData;

	bool m_bLayoutError;
	bool m_bGenerating;

	// Number of iterations since beginning level generation.
	int m_nIterations;

	// State for the current iteration
	struct CurrentIterationState_t
	{
		CurrentIterationState_t() :
			m_bStopIteration( false ),
			m_pRoomCandidateList( NULL )
		{
		}

		// True if iteration should stop after execution of the current rule, false otherwise.
		bool m_bStopIteration;	

		// A list of all valid combinations of room placement.
		// This is essentially the outer product of the set of all possible room
		// choices and the set of all open exits, filtered down to valid combinations.
		CUtlVector< CRoomCandidate > *m_pRoomCandidateList;
	} m_CurrentIterationState;
};

#endif // TILEGEN_LAYOUT_SYSTEM_H