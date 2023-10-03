//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Interfaces used by the tilegen layout system:
// ITilegenAction, ITilegenListener, ITilegenExpression<>, ITilegenRange<>
//
//===============================================================================

#ifndef TILEGEN_CLASS_INTERFACES_H
#define TILEGEN_CLASS_INTERFACES_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

class KeyValues;

// These classes come from tilegen_layout_system.h
class CFreeVariableMap;
class CLayoutSystem;
class CTilegenState;

//////////////////////////////////////////////////////////////////////////
// Interface Definitions
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// Base interface for all level generation actions.
//-----------------------------------------------------------------------------
class ITilegenAction
{
public:
	virtual ~ITilegenAction() { }

	//-----------------------------------------------------------------------------
	// Called on all action objects by the layout system when 
	// beginning generation of a new map.
	//-----------------------------------------------------------------------------
	virtual void OnBeginGeneration( CLayoutSystem *pLayoutSystem ) { pLayoutSystem; }

	//-----------------------------------------------------------------------------
	// Called on all action objects in a state when transitioning to that state.
	//-----------------------------------------------------------------------------
	virtual void OnStateChanged( CLayoutSystem *pLayoutSystem ) { pLayoutSystem; }

	//-----------------------------------------------------------------------------
	// Initializes the class from KeyValues data.
	//-----------------------------------------------------------------------------
	virtual bool LoadFromKeyValues( KeyValues *pKeyValues ) = 0;

	//-----------------------------------------------------------------------------
	// Gets the name of the derived class.
	// NOTE: this is defined for each class by a macro in 
	// tilegen_class_factories.cpp
	//-----------------------------------------------------------------------------
	virtual const char *GetTypeName() = 0;

	//-----------------------------------------------------------------------------
	// Executes an action, operating on the given layout system.
	//-----------------------------------------------------------------------------
	virtual void Execute( CLayoutSystem *pLayoutSystem ) = 0;
};

//-----------------------------------------------------------------------------
// Derived classes can listen and respond to events by updating 
// free variables.
//-----------------------------------------------------------------------------
class ITilegenListener
{
public:
	virtual ~ITilegenListener() { }

	//-----------------------------------------------------------------------------
	// Invoked when beginning generation of a new map.
	//-----------------------------------------------------------------------------
	virtual void OnBeginGeneration( const CLayoutSystem *pLayoutSystem, CFreeVariableMap *pFreeVariables ) { pLayoutSystem; pFreeVariables; }

	//-----------------------------------------------------------------------------
	// Invoked when a room is placed as the result of an action.
	//-----------------------------------------------------------------------------
	virtual void OnRoomPlaced( const CLayoutSystem *pLayoutSystem, CFreeVariableMap *pFreeVariables ) { pLayoutSystem; pFreeVariables; }

	//-----------------------------------------------------------------------------
	// Invoked when the current state is changed as the result of an action.
	//-----------------------------------------------------------------------------
	virtual void OnStateChanged( const CLayoutSystem *pLayoutSystem, const CTilegenState *pOldState, CFreeVariableMap *pFreeVariables ) { pLayoutSystem; pOldState; pFreeVariables; }

	//-----------------------------------------------------------------------------
	// Invoked when a an action is executed.
	//-----------------------------------------------------------------------------
	virtual void OnActionExecuted( const CLayoutSystem *pLayoutSystem, const ITilegenAction *pAction, CFreeVariableMap *pFreeVariables ) { pLayoutSystem; pAction; pFreeVariables; }
};


//-----------------------------------------------------------------------------
// Derived classes are nodes in an experession tree which can be recursively
// evalauted to produce a value.
//-----------------------------------------------------------------------------
template< typename TValue >
class ITilegenExpression
{
public:
	typedef TValue TExpressionValue;

	virtual ~ITilegenExpression() { }

	//-----------------------------------------------------------------------------
	// Initializes the class from KeyValues data.
	//-----------------------------------------------------------------------------
	virtual bool LoadFromKeyValues( KeyValues *pKeyValues ) = 0;

	//-----------------------------------------------------------------------------
	// Gets the name of the derived class.
	//-----------------------------------------------------------------------------
	virtual const char *GetTypeName() = 0;	

	//-----------------------------------------------------------------------------
	// Recursively evaluates this expression, given the current free variable
	// state, and returns the value.
	//
	// pContext - A dictionary of all known variables in the system.
	//-----------------------------------------------------------------------------
	virtual TValue Evaluate( CFreeVariableMap *pContext ) = 0;
};


//-----------------------------------------------------------------------------
// A class which encapsulates a range of iterateable elements.
// 
// The iterator pattern for this class is:
//
//     // Initialize if this range's values may have changed
//     pRange->Initialize( pFreeVariables ); 
//
//     pRange->Reset();
//     while ( pRange->MoveNext() )
//     {
//         DoStuff( pRange->GetCurrent() );
//     }
//-----------------------------------------------------------------------------
template< typename TValue >
class ITilegenRange
{
public:
	typedef TValue TElementValue;

	virtual ~ITilegenRange() { }

	//-----------------------------------------------------------------------------
	// Initializes the class from KeyValues data.
	//-----------------------------------------------------------------------------
	virtual bool LoadFromKeyValues( KeyValues *pKeyValues ) = 0;

	//-----------------------------------------------------------------------------
	// Called before a range is used.  Function is responsible for ensuring
	// that any cached internal state is up-to-date.
	//-----------------------------------------------------------------------------
	virtual void Initialize( CFreeVariableMap *pContext ) = 0;

	//-----------------------------------------------------------------------------
	// Resets the current item pointer to one-before the start of the range.
	// GetCurrent() is invalid to call immediately after Reset().
	//-----------------------------------------------------------------------------
	virtual void Reset() = 0;
	
	//-----------------------------------------------------------------------------
	// Advances the current item forward by 1.
	// Returns false if past the end of the range, true otherwise.
	// GetCurrent() is only valid to call after a call to MoveNext() which
	// has returned true.
	//-----------------------------------------------------------------------------
	virtual bool MoveNext() = 0;

	//-----------------------------------------------------------------------------
	// Returns the current element in the range.
	//-----------------------------------------------------------------------------
	virtual TElementValue GetCurrent() = 0;
};

#endif // TILEGEN_CLASS_INTERFACES_H