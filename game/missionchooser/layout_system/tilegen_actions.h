//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Definitions of many useful tilegen actions which can be executed
// sequentially by the layout system.
//
//===============================================================================

#ifndef TILEGEN_ACTIONS_H
#define TILEGEN_ACTIONS_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "tilegen_class_interfaces.h"
#include "tilegen_class_factories.h"
#include "tilegen_layout_system.h"
#include "MapLayout.h" // for StringReplace_t

class CLevelTheme;

//-----------------------------------------------------------------------------
// Recursively executes a conditional action.
//-----------------------------------------------------------------------------
class CTilegenAction_NestedActions : public ITilegenAction
{
public:
	CTilegenAction_NestedActions();
	~CTilegenAction_NestedActions();

	virtual void OnBeginGeneration( CLayoutSystem *pLayoutSystem );
	virtual void OnStateChanged( CLayoutSystem *pLayoutSystem );
	virtual bool LoadFromKeyValues( KeyValues *pKeyValues );
	virtual void Execute( CLayoutSystem *pLayoutSystem );
	virtual const char *GetTypeName();
	
private:
	CUtlVector< ActionConditionPair_t > m_NestedActions;
	ITilegenExpression< bool > *m_pWhileCondition;
};

class CTilegenAction_SetVariable : public ITilegenAction
{
public:
	CTilegenAction_SetVariable( ITilegenExpression< const char * > *pVariableName = NULL ) :
	m_pVariableName( pVariableName ),
	m_bFireOnBeginGeneration( false ),
	m_bFireOnChangeState( false )
	{
	}

	~CTilegenAction_SetVariable()
	{
		delete m_pVariableName;
	}

	virtual void OnBeginGeneration( CLayoutSystem *pLayoutSystem )
	{
		if ( m_bFireOnBeginGeneration ) 
		{
			InternalExecute( pLayoutSystem );
		}
	}

	virtual void OnStateChanged( CLayoutSystem *pLayoutSystem )
	{
		if ( m_bFireOnChangeState ) 
		{
			InternalExecute( pLayoutSystem );
		}
	}

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues )
	{
		bool bSuccess = true;
		bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "variable", GetTypeName(), &m_pVariableName );
		m_bFireOnBeginGeneration = pKeyValues->GetBool( "on_begin_generation", false );
		m_bFireOnChangeState = pKeyValues->GetBool( "on_begin_state", false );
		return bSuccess;
	}

	virtual void Execute( CLayoutSystem *pLayoutSystem )
	{
		if ( !m_bFireOnBeginGeneration && !m_bFireOnChangeState ) 
		{
			InternalExecute( pLayoutSystem );
		}
	}

protected:
	virtual void InternalExecute( CLayoutSystem *pLayoutSystem ) = 0;

	ITilegenExpression< const char * > *m_pVariableName;
	// This indicates that the expressions should be evaluated & the variable set
	// either exactly once (OnBeginGeneration) or every time the state containing the variable is transitioned to.
	// If either is set, this action will be skipped during normal execution.
	bool m_bFireOnBeginGeneration;
	bool m_bFireOnChangeState;
};

//-----------------------------------------------------------------------------
// Sets a named free variable to the result of an expression
//-----------------------------------------------------------------------------
template< typename TVariableType >
class CTilegenAction_SetVariableT : public CTilegenAction_SetVariable
{
public:
	CTilegenAction_SetVariableT( ITilegenExpression< const char * > *pVariableName = NULL, ITilegenExpression< TVariableType > *pExpression = NULL ) :
	CTilegenAction_SetVariable( pVariableName ),
	m_pExpression( pExpression )
	{
	}

	~CTilegenAction_SetVariableT()
	{
		delete m_pExpression;
	}

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues )
	{
		bool bSuccess = true;
		bSuccess &= CTilegenAction_SetVariable::LoadFromKeyValues( pKeyValues );
		bSuccess &= CreateExpressionFromKeyValuesBlock< TVariableType >( pKeyValues, "value", GetTypeName(), &m_pExpression );
		return bSuccess;
	}

protected:
	virtual void InternalExecute( CLayoutSystem *pLayoutSystem )
	{
		const char *pVariableName = m_pVariableName->Evaluate( pLayoutSystem->GetFreeVariables() );
		pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( pVariableName, ( void * )m_pExpression->Evaluate( pLayoutSystem->GetFreeVariables() ) );
	}

	ITilegenExpression< TVariableType > *m_pExpression;
	
};

class CTilegenAction_SetVariableInt : public CTilegenAction_SetVariableT< int >
{
public:
	virtual const char *GetTypeName();
};

class CTilegenAction_SetVariableString : public CTilegenAction_SetVariableT< const char * >
{
public:
	virtual const char *GetTypeName();
};

// This is somewhat tricky; this sets variables of type ITilegenExpression< bool > but otherwise behaves 
// just like an instance of CTilegenAction_SetVariable< bool >, not CTilegenAction_SetVariable< ITilegenExpresion< bool > >.
class CTilegenAction_SetVariableBoolExpression : public CTilegenAction_SetVariableT< bool >
{
public:
	virtual const char *GetTypeName();

protected:
	virtual void InternalExecute( CLayoutSystem *pLayoutSystem )
	{
		const char *pVariableName = m_pVariableName->Evaluate( pLayoutSystem->GetFreeVariables() );
		pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( pVariableName, m_pExpression );
	}
};

class CTilegenAction_SetVariableAction : public CTilegenAction_SetVariable
{
public:
	CTilegenAction_SetVariableAction( ITilegenExpression< const char * > *pVariableName = NULL, ITilegenAction *pAction = NULL ) :
	CTilegenAction_SetVariable( pVariableName ),
	m_pAction( pAction )
	{
	}

	~CTilegenAction_SetVariableAction()
	{
		delete m_pAction;
	}

	virtual const char *GetTypeName();

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues )
	{
		bool bSuccess = true;
		bSuccess &= CTilegenAction_SetVariable::LoadFromKeyValues( pKeyValues );
		ITilegenExpression< bool > *pDummyCondition;
		bSuccess &= CreateActionAndConditionFromKeyValuesBlock( pKeyValues, "value", GetTypeName(), &m_pAction, &pDummyCondition );
		if ( pDummyCondition != NULL )
		{
			delete pDummyCondition;
			Warning( "'CTilegenAction_SetVariableAction' cannot set actions which have associated conditions.\n" );
			bSuccess = false;
		}
		return bSuccess;
	}

protected:
	virtual void InternalExecute( CLayoutSystem *pLayoutSystem )
	{
		const char *pVariableName = m_pVariableName->Evaluate( pLayoutSystem->GetFreeVariables() );
		pLayoutSystem->GetFreeVariables()->SetOrCreateFreeVariable( pVariableName, m_pAction );
	}

	ITilegenAction *m_pAction;
};

//-----------------------------------------------------------------------------
// Builds up a list of room candidates from a given theme and set of open exits 
// in the level. The list can be filtered with the specified filter class.
//-----------------------------------------------------------------------------
class CTilegenAction_AddRoomCandidates : public ITilegenAction
{
public:
	//-----------------------------------------------------------------------------
	// Generates room candidates based on allowed room templates and 
	// currently open exits.
	//
	// NOTE: the theme name expression is only evaluated once for performance 
	// reasons. We can always add a boolean value to control this behavior if 
	// something else is desired (unlikely).
	//-----------------------------------------------------------------------------

	CTilegenAction_AddRoomCandidates();
	virtual ~CTilegenAction_AddRoomCandidates();

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues );
	virtual const char *GetTypeName();
	virtual void Execute( CLayoutSystem *pLayoutSystem );

private:
	CLevelTheme *m_pLevelTheme;
	ITilegenExpression< const char * > *m_pThemeNameExpression;
	ITilegenExpression< bool > *m_pExitFilter;
	ITilegenExpression< bool > *m_pRoomTemplateFilter;
	ITilegenExpression< bool > *m_pRoomCandidateFilter;
	ITilegenAction *m_pRoomCandidateFilterAction;
	ITilegenExpression< bool > *m_pRoomCandidateFilterCondition;
	bool m_bExcludeGlobalFilters;
};

//-----------------------------------------------------------------------------
// Builds up a list of room candidates from a given theme at a specific
// location (ignoring open exits).
//-----------------------------------------------------------------------------
class CTilegenAction_AddRoomCandidatesAtLocation : public ITilegenAction
{
public:
	//-----------------------------------------------------------------------------
	// Generates room candidates at the given location.  
	// NOTE: the theme name expression is only evaluated once for performance 
	// reasons. We can always add a boolean value to control this behavior if 
	// something else is desired (unlikely).
	//-----------------------------------------------------------------------------
	CTilegenAction_AddRoomCandidatesAtLocation( 
		ITilegenExpression< const char * > *pThemeNameExpression = NULL, 
		ITilegenExpression< int > *pXExpression = NULL,
		ITilegenExpression< int > *pYExpression = NULL,
		ITilegenExpression< bool > *pRoomTemplateFilter = NULL );

	virtual ~CTilegenAction_AddRoomCandidatesAtLocation();

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues );
	virtual const char *GetTypeName();
	virtual void Execute( CLayoutSystem *pLayoutSystem );

private:
	CLevelTheme *m_pLevelTheme;
	ITilegenExpression< const char * > *m_pThemeNameExpression;
	ITilegenExpression< int > *m_pXExpression;
	ITilegenExpression< int > *m_pYExpression;
	ITilegenExpression< bool > *m_pRoomTemplateFilter;
};

//-----------------------------------------------------------------------------
// Chooses a candidate at random from the room candidate list for the 
// current iteration.
//-----------------------------------------------------------------------------
class CTilegenAction_ChooseCandidate : public ITilegenAction
{
public:
	CTilegenAction_ChooseCandidate( bool bStopProcessingActionsOnSuccess = false ) : m_bStopProcessingActionsOnSuccess( bStopProcessingActionsOnSuccess ) { }

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues );
	virtual const char *GetTypeName();
	virtual void Execute( CLayoutSystem *pLayoutSystem );

private:
	bool m_bStopProcessingActionsOnSuccess;
};

//-----------------------------------------------------------------------------
// Filters the candidate list by purging exitsmost strongly in a particular 
// direction from the room candidate list for the current iteration.
//-----------------------------------------------------------------------------
class CTilegenAction_FilterCandidatesByDirection : public ITilegenAction
{
public:
	CTilegenAction_FilterCandidatesByDirection( 
		ITilegenExpression< const char * > *pDirectionExpression = NULL,
		ITilegenExpression< int > *pThresholdExpression = NULL );

	~CTilegenAction_FilterCandidatesByDirection();

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues );
	virtual const char *GetTypeName();
	virtual void Execute( CLayoutSystem *pLayoutSystem );

private:
	ITilegenExpression< const char * > *m_pDirectionExpression;
	ITilegenExpression< int > *m_pThresholdExpression;
};

//-----------------------------------------------------------------------------
// Filters the candidate list by culling the list down to candidates
// with the fewest number of children, which encourages linear growth.
//-----------------------------------------------------------------------------
class CTilegenAction_FilterCandidatesForLinearGrowth : public ITilegenAction
{
public:
	CTilegenAction_FilterCandidatesForLinearGrowth() { }
	~CTilegenAction_FilterCandidatesForLinearGrowth() { }

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues );
	virtual const char *GetTypeName();
	virtual void Execute( CLayoutSystem *pLayoutSystem );

private:
	ITilegenExpression< int > *m_pThresholdExpression;
};

//-----------------------------------------------------------------------------
// Switches the state machine's current state to the specified state.
//-----------------------------------------------------------------------------
class CTilegenAction_SwitchState : public ITilegenAction
{
public:
	CTilegenAction_SwitchState();
	~CTilegenAction_SwitchState();

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues );
	virtual const char *GetTypeName();
	virtual void Execute( CLayoutSystem *pLayoutSystem );

private:
	// m_pNewStateExpression can be NULL, in which case, simply go to the next sequential state.
	ITilegenExpression< const char * > *m_pNewStateExpression;
};

//-----------------------------------------------------------------------------
// Signals the end of level generation.
//-----------------------------------------------------------------------------
class CTilegenAction_FinishGeneration : public ITilegenAction
{
public:
	CTilegenAction_FinishGeneration() { }

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues ) { return true; }
	virtual const char *GetTypeName();
	virtual void Execute( CLayoutSystem *pLayoutSystem );
};

//-----------------------------------------------------------------------------
// Signals a fatal error during level generation.
//-----------------------------------------------------------------------------
class CTilegenAction_EpicFail : public ITilegenAction
{
public:
	CTilegenAction_EpicFail() { }

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues ) { return true; }
	virtual const char *GetTypeName();
	virtual void Execute( CLayoutSystem *pLayoutSystem );
};

class CTilegenAction_EnsureRoomExists : public ITilegenAction
{
public:
	CTilegenAction_EnsureRoomExists() { }

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues );
	virtual const char *GetTypeName();
	virtual void Execute( CLayoutSystem *pLayoutSystem );

private:
	ITilegenExpression< const char * > *m_pRoomNameExpression;
};

//-----------------------------------------------------------------------------
// Places a piece which assists in connecting to another piece.
//-----------------------------------------------------------------------------
class CTilegenAction_AddConnectorRoomCandidates : public ITilegenAction
{
public:
	CTilegenAction_AddConnectorRoomCandidates( ITilegenAction *pAddRoomCandidates = NULL );
	~CTilegenAction_AddConnectorRoomCandidates();

	void SetRoomTemplate( const CRoomTemplate *pTargetRoomTemplate );

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues );
	virtual const char *GetTypeName();
	virtual void Execute( CLayoutSystem *pLayoutSystem );

private:
	// Theme and room template filter to find the room we're trying to connect to.
	CLevelTheme *m_pLevelTheme;
	ITilegenExpression< const char * > *m_pTargetThemeNameExpression;
	ITilegenExpression< bool > *m_pTargetRoomTemplateFilter;

	// When instantiated programmatically, the theme & room template filter are NULL but the room template is specified directly.
	const CRoomTemplate *m_pTargetRoomTemplate;

	// A nested action which is used to add the connector piece candidates (typically CTilegenAction_AddRoomCandidates).
	ITilegenAction *m_pAddConnectorCandidates;
};

//-----------------------------------------------------------------------------
// Places a mission component/objective at a specified time.
//-----------------------------------------------------------------------------
class CTilegenAction_PlaceComponent : public ITilegenAction
{
public:
	CTilegenAction_PlaceComponent();
	~CTilegenAction_PlaceComponent();

	virtual void OnBeginGeneration( CLayoutSystem *pLayoutSystem );
	virtual bool LoadFromKeyValues( KeyValues *pKeyValues );
	virtual const char *GetTypeName();
	virtual void Execute( CLayoutSystem *pLayoutSystem );

private:
	struct RoomPlacementInfo_t
	{
		CRoomTemplate *m_pRoomTemplate;
		// The fraction of the way through generating either the current state or the whole map.
		// If less than 0.0, implies random placement.
		float m_flPlacementFraction;
	};
	
	struct RoomPlacementInstance_t
	{
		RoomPlacementInfo_t *pInfo;
		// The fraction of the way through generating either the current state or the whole map.
		// If the original placement info indicated random placement, this will be in the range
		// [0.05f to 0.95f]; in general, it is always clamped to the range [0.0f, 1.0f].
		float m_flPlacementFraction;
		// Whether or not the room has been placed yet
		bool m_bPlaced;
	};

	// The maximum number of optional rooms allowed to be parsed
	static const int m_nMaxTotalOptionalRooms = 128;

	void AddRoomPlacementInstance( CLayoutSystem *pLayoutSystem, RoomPlacementInfo_t *pRoomPlacementInfo );
	bool LoadRoomPlacementsFromKeyValues( KeyValues *pKeyValues, CUtlVector< RoomPlacementInfo_t > *pRooms );
	bool PlaceRoom( CLayoutSystem *pLayoutSystem, const CRoomTemplate *pRoomTemplate );
	bool PlaceConnectorRoom( CLayoutSystem *pLayoutSystem, const CRoomTemplate *pRoomTemplate );

	int m_nMinOptionalRooms, m_nMaxOptionalRooms;

	ITilegenExpression< bool > *m_pExitFilter;
	ITilegenExpression< bool > *m_pRoomCandidateFilter;
	ITilegenAction *m_pRoomCandidateFilterAction;
	ITilegenExpression< bool > *m_pRoomCandidateFilterCondition;

	// An action used to add connector pieces if it proves impossible to place a given component at a specified point in time.
	CTilegenAction_AddConnectorRoomCandidates *m_pAddConnectorRoomCandidates;
	// An action used to actually place connector pieces if needed.
	CTilegenAction_ChooseCandidate *m_pChooseCandidate;

	bool m_bExcludeGlobalFilters;

	CUtlVector< RoomPlacementInfo_t > m_MandatoryRooms;
	CUtlVector< RoomPlacementInfo_t > m_OptionalRooms;

	class CRoomPlacementInstanceLessFunc
	{
	public:
		bool Less( const RoomPlacementInstance_t &lhs, const RoomPlacementInstance_t &rhs, void *pContext )
		{
			return lhs.m_flPlacementFraction < rhs.m_flPlacementFraction;
		}
	};
	CUtlSortVector< RoomPlacementInstance_t, CRoomPlacementInstanceLessFunc > m_RoomsToPlace;
};
 
//-----------------------------------------------------------------------------
// Adds instance(s) to previous placed rooms meeting some criteria 
// during the VMF instancing fix-up phase.
// (This actually schedules a fix-up to occur at a later time by adding
// entries to the map layout file).
//-----------------------------------------------------------------------------
class CTilegenAction_AddInstances : public ITilegenAction
{
public:
	CTilegenAction_AddInstances();
	~CTilegenAction_AddInstances();

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues );
	virtual const char *GetTypeName();
	virtual void Execute( CLayoutSystem *pLayoutSystem );

private:
	// A "template" instance spawn definition, which will be effectively repeated N times (where N is the instance count)
	// for various rooms
	CInstanceSpawn m_InstanceSpawn;
	
	// # of instances to spawn
	ITilegenExpression< int > *m_pInstanceCount;

	// Filter used to eliminate placed rooms from consideration
	ITilegenExpression< bool > *m_pRoomTemplateFilter;
};


//-----------------------------------------------------------------------------
// Adds an instance to a specific CRoom during the VMF instancing fix-up phase.
// (This actually schedules a fix-up to occur at a later time by adding
// entries to the map layout file).
//-----------------------------------------------------------------------------
class CTilegenAction_AddInstanceToRoom : public ITilegenAction
{
public:
	CTilegenAction_AddInstanceToRoom();
	~CTilegenAction_AddInstanceToRoom();

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues );
	virtual const char *GetTypeName();
	virtual void Execute( CLayoutSystem *pLayoutSystem );

private:
	// A spawn definition for the
	CInstanceSpawn m_InstanceSpawn;

	ITilegenExpression< const CRoom * > *m_pRoomExpression;
};

//-----------------------------------------------------------------------------
// Loads the rooms from a .layout file directly into this layout.
//-----------------------------------------------------------------------------
class CTilegenAction_LoadLayout : public ITilegenAction
{
public:
	CTilegenAction_LoadLayout();
	
	virtual bool LoadFromKeyValues( KeyValues *pKeyValues );

	virtual const char *GetTypeName();
	virtual void Execute( CLayoutSystem *pLayoutSystem );

private:
	char m_LayoutFilename[MAX_PATH];
};

#endif // TILEGEN_ACTIONS_H