//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Definition of many useful tilegen expressions which can be 
// evaluated to return values.
//
//===============================================================================

#ifndef TILEGEN_EXPRESSIONS_H
#define TILEGEN_EXPRESSIONS_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "utldict.h"
#include "stringpool.h"
#include "tilegen_core.h"
#include "tilegen_class_interfaces.h"
#include "tilegen_class_factories.h"

class CRoom;
class CRoomTemplate;
class CRoomCandidate;
class CExit;

//-----------------------------------------------------------------------------
// A dictionary that maps variable names to void * values 
// which represent "free variable" state used by expressions ("closures").
//-----------------------------------------------------------------------------
class CFreeVariableMap : public CUtlDict< void *, int >
{
public:
	//-----------------------------------------------------------------------------
	// Gets the value of a free variable and returns NULL if not present.
	//-----------------------------------------------------------------------------
	void *GetFreeVariableOrNULL( const char *pName ) const;

	//-----------------------------------------------------------------------------
	// Gets the value of a free variable and reports an error if 
	// not present or NULL valued.
	//-----------------------------------------------------------------------------
	void *GetFreeVariable( const char *pName ) const;

	//-----------------------------------------------------------------------------
	// Gets the value of a free variable and reports an error if 
	// not present or NULL valued.
	//-----------------------------------------------------------------------------
	void *GetFreeVariableDisallowNULL( const char *pName ) const;

	//-----------------------------------------------------------------------------
	// Sets the value of an existing free variable or creates a 
	// new one if none is found.
	//-----------------------------------------------------------------------------
	void SetOrCreateFreeVariable( const char *pName, void *pValue );

	// Actions/expressions that operate on string variables can ignore
	// memory management and toss their data in the string pool.
	CStringPool m_StringPool;
};

//-----------------------------------------------------------------------------
// A binary expression which evaluates two sub-expressions of the same type
// and returns a value of another type.
//-----------------------------------------------------------------------------
template< typename TReturn, typename TParam >
class CTilegenExpression_Binary : public ITilegenExpression< TReturn >
{
public:
	explicit CTilegenExpression_Binary( ITilegenExpression< TParam > *pExpressionL = NULL, ITilegenExpression< TParam > *pExpressionR = NULL )
	{
		m_pExpression[0] = pExpressionL;
		m_pExpression[1] = pExpressionR;
	}

	virtual ~CTilegenExpression_Binary() { delete m_pExpression[0]; delete m_pExpression[1]; }

	// Used to initialize expressions by setting parameters in a general fashion
	virtual bool LoadFromKeyValues( KeyValues *pKeyValues )
	{ 
		bool bSuccess = true;
		bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "param0", GetTypeName(), &m_pExpression[0] );
		bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "param1", GetTypeName(), &m_pExpression[1] );
		return bSuccess;
	}

	virtual TReturn Evaluate( CFreeVariableMap *pContext ) { return DirectEvaluate( pContext, m_pExpression[0]->Evaluate( pContext ), m_pExpression[1]->Evaluate( pContext ) ); }
	virtual TReturn DirectEvaluate( CFreeVariableMap *pContext, const TParam &param1, const TParam &param2 ) = 0;

protected:
	ITilegenExpression< TParam > *m_pExpression[2];
};

//-----------------------------------------------------------------------------
// Define common binary operators here.
//-----------------------------------------------------------------------------
#define DEFINE_BINARY_OPERATOR( OperatorName, ReturnType, ParameterType, Operator ) \
class OperatorName : public CTilegenExpression_Binary< ReturnType, ParameterType > \
{ \
public: \
	explicit OperatorName( ITilegenExpression< ParameterType > *pL = NULL, ITilegenExpression< ParameterType > *pR = NULL ) : CTilegenExpression_Binary( pL, pR ) { } \
	virtual ReturnType Evaluate( CFreeVariableMap *pContext ) { return m_pExpression[0]->Evaluate( pContext ) Operator m_pExpression[1]->Evaluate( pContext ); } \
	virtual ReturnType DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &param1, const ParameterType &param2 ) { return param1 Operator param2; } \
	virtual const char *GetTypeName(); \
}

DEFINE_BINARY_OPERATOR( CTilegenExpression_Greater, bool, int, > );
DEFINE_BINARY_OPERATOR( CTilegenExpression_GreaterOrEqual, bool, int, >= );
DEFINE_BINARY_OPERATOR( CTilegenExpression_Equal, bool, int, == );
DEFINE_BINARY_OPERATOR( CTilegenExpression_NotEqual, bool, int, != );
DEFINE_BINARY_OPERATOR( CTilegenExpression_Less, bool, int, < );
DEFINE_BINARY_OPERATOR( CTilegenExpression_LessOrEqual, bool, int, <= );
DEFINE_BINARY_OPERATOR( CTilegenExpression_Subtract, int, int, - );

#undef DEFINE_BINARY_OPERATOR

//-----------------------------------------------------------------------------
// Tests two strings for equality (case insensitive).
//-----------------------------------------------------------------------------
class CTilegenExpression_StringEqual : public CTilegenExpression_Binary< bool, const char * >
{
public:
	typedef const char * ParameterType;
	CTilegenExpression_StringEqual( ITilegenExpression< const char * > *pL = NULL, ITilegenExpression< const char * > *pR = NULL ) : CTilegenExpression_Binary( pL, pR ) { }
	virtual bool DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &param1, const ParameterType &param2 ) { return Q_stricmp( param1, param2 ) == 0; }
	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Tests two strings for inequality (case insensitive).
//-----------------------------------------------------------------------------
class CTilegenExpression_StringNotEqual : public CTilegenExpression_Binary< bool, const char * >
{
public:
	typedef const char * ParameterType;
	CTilegenExpression_StringNotEqual( ITilegenExpression< const char * > *pL = NULL, ITilegenExpression< const char * > *pR = NULL ) : CTilegenExpression_Binary( pL, pR ) { }
	virtual bool DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &param1, const ParameterType &param2 ) { return Q_stricmp( param1, param2 ) != 0; }
	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Concatenates two strings, returns the result.
// The result is cached & stored in the string pool.
//-----------------------------------------------------------------------------
class CTilegenExpression_StringConcatenate : public CTilegenExpression_Binary< const char *, const char * >
{
public:
	typedef const char * ParameterType;
	CTilegenExpression_StringConcatenate( ITilegenExpression< const char * > *pL = NULL, ITilegenExpression< const char * > *pR = NULL ) : CTilegenExpression_Binary( pL, pR ) { }
	virtual const char *DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &param1, const ParameterType &param2 );
	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Define common binary operators here.
//-----------------------------------------------------------------------------
template< typename TParam  >
class CTilegenExpression_Multi : public ITilegenExpression< TParam >
{
public:
	CTilegenExpression_Multi() { }

	virtual ~CTilegenExpression_Multi() { m_Expressions.PurgeAndDeleteElements(); }

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues )
	{
		for ( KeyValues *pSubKey = pKeyValues->GetFirstSubKey(); pSubKey != NULL; pSubKey = pSubKey->GetNextKey() )
		{
			if ( Q_stricmp( pSubKey->GetName(), "param" ) == 0 )
			{
				ITilegenExpression< TParam > *pExpression = CreateFromKeyValues< ITilegenExpression< TParam > >( pSubKey );
				if ( !pExpression )
				{
					return false;
				}
				m_Expressions.AddToTail( pExpression );
			}
		}
		return true;
	}

	virtual TParam Evaluate( CFreeVariableMap *pContext )
	{
		if ( m_Expressions.Count() == 0 )
		{
			return GetIdentityValue();
		}
		else if ( m_Expressions.Count() == 1 )
		{
			return m_Expressions[0]->Evaluate( pContext );
		}
		else
		{
			TParam currentValue = DirectEvaluate( pContext, m_Expressions[0]->Evaluate( pContext ), m_Expressions[1]->Evaluate( pContext ) );
			for ( int i = 2; i < m_Expressions.Count(); ++ i )
			{
				currentValue = DirectEvaluate( pContext, currentValue, m_Expressions[i]->Evaluate( pContext ) );
			}
			return currentValue;
		}
	}

	virtual TParam DirectEvaluate( CFreeVariableMap *pContext, const TParam &param1, const TParam &param2 ) = 0;
	virtual TParam GetIdentityValue() = 0;

private:
	CUtlVector< ITilegenExpression< TParam > * > m_Expressions;
};

//-----------------------------------------------------------------------------
// Define common multi operators here.
//-----------------------------------------------------------------------------
#define DEFINE_MULTI_OPERATOR( OperatorName, ParameterType, Operator, IdentityValue ) \
class OperatorName : public CTilegenExpression_Multi< ParameterType > \
{ \
public: \
	OperatorName() { } \
	virtual ParameterType DirectEvaluate( CFreeVariableMap *pContext, const TExpressionValue &param1, const TExpressionValue &param2 ) { return param1 Operator param2; } \
	virtual TExpressionValue GetIdentityValue() { return IdentityValue; } \
	virtual const char *GetTypeName(); \
}

DEFINE_MULTI_OPERATOR( CTilegenExpression_Add, int, +, 0 );

// Floating point operators are in the minority, so they are preceded by "Float" (integers are the default)
DEFINE_MULTI_OPERATOR( CTilegenExpression_FloatMultiply, float, *, 0.0f );

DEFINE_MULTI_OPERATOR( CTilegenExpression_And, bool, &&, true );
DEFINE_MULTI_OPERATOR( CTilegenExpression_Or, bool, ||, false );

#undef DEFINE_MULTI_OPERATOR

//-----------------------------------------------------------------------------
// A unary expression which evaluates one sub-expression of some type
// and returns a value of another type.
//-----------------------------------------------------------------------------
template< typename TReturn, typename TParam >
class CTilegenExpression_Unary : public ITilegenExpression< TReturn >
{
public:
	explicit CTilegenExpression_Unary( ITilegenExpression< TParam > *pExpression = NULL ) :
	m_pExpression( pExpression )
	{
	}

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues )
	{ 
		return CreateExpressionFromKeyValuesBlock( pKeyValues, "param", GetTypeName(), &m_pExpression );
	}

	virtual TReturn Evaluate( CFreeVariableMap *pContext ) { return DirectEvaluate( pContext, m_pExpression->Evaluate( pContext ) ); }
	virtual TReturn DirectEvaluate( CFreeVariableMap *pContext, const TParam &param ) = 0;

	virtual ~CTilegenExpression_Unary() { delete m_pExpression; }

protected:
	ITilegenExpression< TParam > *m_pExpression;
};

//-----------------------------------------------------------------------------
// Define common unary operators here.
//-----------------------------------------------------------------------------
#define DEFINE_UNARY_OPERATOR( OperatorName, ReturnType, ParameterType, Operator ) \
class OperatorName : public CTilegenExpression_Unary< ReturnType, ParameterType > \
{ \
public: \
	explicit OperatorName( ITilegenExpression< ParameterType > *pExpression = NULL ) : CTilegenExpression_Unary( pExpression ) { } \
	virtual ReturnType DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &param ) { return Operator param; } \
	virtual const char *GetTypeName(); \
}

DEFINE_UNARY_OPERATOR( CTilegenExpression_Not, bool, bool, ! );
DEFINE_UNARY_OPERATOR( CTilegenExpression_BoolToInt, int, bool, (int) );
DEFINE_UNARY_OPERATOR( CTilegenExpression_IntToBool, bool, int, !! );
DEFINE_UNARY_OPERATOR( CTilegenExpression_FloatToInt, int, float, (int) );
DEFINE_UNARY_OPERATOR( CTilegenExpression_IntToFloat, float, int, (float) );

#undef DEFINE_UNARY_OPERATOR

//-----------------------------------------------------------------------------
// Returns the room name string from a room template.
//-----------------------------------------------------------------------------
class CTilegenExpression_RoomName : public CTilegenExpression_Unary< const char *, const CRoomTemplate * >
{
public:
	typedef const CRoomTemplate * ParameterType;
	CTilegenExpression_RoomName( ITilegenExpression< const CRoomTemplate * > *pExpression = NULL ) : CTilegenExpression_Unary( pExpression ) { }
	virtual const char *DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &pRoomTemplate );
	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Returns the area of a room template.
//-----------------------------------------------------------------------------
class CTilegenExpression_RoomArea : public CTilegenExpression_Unary< int, const CRoomTemplate * >
{
public:
	typedef const CRoomTemplate * ParameterType;
	CTilegenExpression_RoomArea( ITilegenExpression< const CRoomTemplate * > *pExpression = NULL ) : CTilegenExpression_Unary( pExpression ) { }
	virtual int DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &pRoomTemplate );
	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Returns the room name string from a joint "Theme\RoomName" string
//-----------------------------------------------------------------------------
class CTilegenExpression_ExtractRoomName : public CTilegenExpression_Unary< const char *, const char * >
{
public:
	typedef const char * ParameterType;
	CTilegenExpression_ExtractRoomName( ITilegenExpression< const char * > *pExpression = NULL ) : CTilegenExpression_Unary( pExpression ) { }
	virtual const char *DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &pFullRoomName );
	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Returns the theme name string from a joint "Theme\RoomName" string
//-----------------------------------------------------------------------------
class CTilegenExpression_ExtractThemeName : public CTilegenExpression_Unary< const char *, const char * >
{
public:
	typedef const char * ParameterType;
	CTilegenExpression_ExtractThemeName( ITilegenExpression< const char * > *pExpression = NULL ) : CTilegenExpression_Unary( pExpression ) { }
	virtual const char *DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &pFullRoomName );
	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Returns the last placed room.
//-----------------------------------------------------------------------------
class CTilegenExpression_LastPlacedRoom : public ITilegenExpression< const CRoom * >
{
public:
	CTilegenExpression_LastPlacedRoom() { }
	virtual bool LoadFromKeyValues( KeyValues *pKeyValues ) { return true; }
	virtual const CRoom *Evaluate( CFreeVariableMap *pContext );
	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Returns the source room from an exit.
//-----------------------------------------------------------------------------
class CTilegenExpression_SourceRoomFromExit : public CTilegenExpression_Unary< const CRoom *, const CExit * >
{
public:
	typedef const CExit * ParameterType;
	CTilegenExpression_SourceRoomFromExit( ITilegenExpression< const CExit * > *pExpression = NULL ) : CTilegenExpression_Unary( pExpression ) { }
	virtual const CRoom *DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &pExit );
	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Returns the source room template from an exit.
//-----------------------------------------------------------------------------
class CTilegenExpression_SourceRoomTemplateFromExit : public CTilegenExpression_Unary< const CRoomTemplate *, const CExit * >
{
public:
	typedef const CExit * ParameterType;
	CTilegenExpression_SourceRoomTemplateFromExit( ITilegenExpression< const CExit * > *pExpression = NULL ) : CTilegenExpression_Unary( pExpression ) { }
	virtual const CRoomTemplate *DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &pExit );
	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Returns a value indicating whether this room is a chokepoint grow source.
//-----------------------------------------------------------------------------
class CTilegenExpression_ChokepointGrowSource : public CTilegenExpression_Unary< bool, const CExit * >
{
public:
	typedef const CExit * ParameterType;
	CTilegenExpression_ChokepointGrowSource( ITilegenExpression< const CExit * > *pExpression = NULL ) : CTilegenExpression_Unary( pExpression ) { }
	virtual bool DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &pExit );
	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Returns the number of rooms which have sprouted from a given room.
//-----------------------------------------------------------------------------
class CTilegenExpression_RoomChildCount : public CTilegenExpression_Unary< int, const CRoom * >
{
public:
	typedef const CRoom * ParameterType;
	CTilegenExpression_RoomChildCount( ITilegenExpression< const CRoom * > *pExpression = NULL ) : CTilegenExpression_Unary( pExpression ) { }
	virtual int DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &pRoom );
	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Gets a room template by name.
//-----------------------------------------------------------------------------
class CTilegenExpression_RoomTemplateFromName : public CTilegenExpression_Unary< const CRoomTemplate *, const char * >
{
public:
	typedef const char * ParameterType;
	CTilegenExpression_RoomTemplateFromName( ITilegenExpression< const char * > *pExpression = NULL ) : CTilegenExpression_Unary( pExpression ) { }
	virtual const CRoomTemplate *DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &pName );
	virtual const char *GetTypeName();
};

class CTilegenExpression_XPosition : public CTilegenExpression_Unary< int, const CRoomCandidate * >
{
public:
	typedef const CRoomCandidate * ParameterType;
	CTilegenExpression_XPosition( ITilegenExpression< const CRoomCandidate * > *pExpression = NULL ) : CTilegenExpression_Unary( pExpression ) { }
	virtual int DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &pRoomCandidate );
	virtual const char *GetTypeName();
};

class CTilegenExpression_YPosition : public CTilegenExpression_Unary< int, const CRoomCandidate * >
{
public:
	typedef const CRoomCandidate * ParameterType;
	CTilegenExpression_YPosition( ITilegenExpression< const CRoomCandidate * > *pExpression = NULL ) : CTilegenExpression_Unary( pExpression ) { }
	virtual int DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &pRoomCandidate );
	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Returns a boolean value indicating whether a given room template
// has a given string tag.
//-----------------------------------------------------------------------------
class CTilegenExpression_HasTag : public ITilegenExpression< bool >
{
public:
	CTilegenExpression_HasTag( ITilegenExpression< const CRoomTemplate * > *pRoomTemplateExpression = NULL, ITilegenExpression< const char * > *pTagExpression = NULL );

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues );
	virtual bool Evaluate( CFreeVariableMap *pContext );
	virtual const char *GetTypeName();

private:
	ITilegenExpression< const CRoomTemplate * > *m_pRoomTemplateExpression;
	ITilegenExpression< const char * > *m_pTagExpression;
};

//-----------------------------------------------------------------------------
// Tests for the presence of several known tags which imply
// that a room template should not be placed randomly.
//-----------------------------------------------------------------------------
class CTilegenExpression_CanPlaceRandomly : public ITilegenExpression< bool >
{
public:
	CTilegenExpression_CanPlaceRandomly( ITilegenExpression< const CRoomTemplate * > *pRoomTemplateExpression = NULL );

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues );
	virtual bool Evaluate( CFreeVariableMap *pContext );
	virtual const char *GetTypeName();

private:
	ITilegenExpression< const CRoomTemplate * > *m_pRoomTemplateExpression;
};

//-----------------------------------------------------------------------------
// Returns the number of times a room template has already been placed.
//-----------------------------------------------------------------------------
class CTilegenExpression_NumTimesPlaced : public CTilegenExpression_Unary< int, const CRoomTemplate * >
{
public:
	typedef const CRoomTemplate * ParameterType;
	CTilegenExpression_NumTimesPlaced( ITilegenExpression< const CRoomTemplate * > *pExpression = NULL ) : CTilegenExpression_Unary( pExpression ) { }
	virtual int DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &pRoomTemplate );
	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Returns the exit tag string from an exit.
//-----------------------------------------------------------------------------
class CTilegenExpression_ExitTag : public CTilegenExpression_Unary< const char *, const CExit * >
{
public:
	typedef const CExit * ParameterType;

	CTilegenExpression_ExitTag( ITilegenExpression< const CExit * > *pExpression = NULL ) : CTilegenExpression_Unary( pExpression ) { }

	virtual const char *DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &pExit );
	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Returns the exit direction (integer enumeration) from an exit.
//-----------------------------------------------------------------------------
class CTilegenExpression_ExitDirection : public CTilegenExpression_Unary< int, const CExit * >
{
public:
	typedef const CExit * ParameterType;

	CTilegenExpression_ExitDirection( ITilegenExpression< const CExit * > *pExpression = NULL ) : CTilegenExpression_Unary( pExpression ) { }

	virtual int DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &pExit );
	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Gets the parent of the given state object.
//-----------------------------------------------------------------------------
class CTilegenExpression_ParentState : public CTilegenExpression_Unary< const CTilegenState *, const CTilegenState * >
{
public:
	typedef const CTilegenState * ParameterType;

	CTilegenExpression_ParentState( ITilegenExpression< const CTilegenState * > *pExpression = NULL ) : CTilegenExpression_Unary( pExpression ) { }

	virtual const CTilegenState *DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &pState );
	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Gets the name of the given state object.
//-----------------------------------------------------------------------------
class CTilegenExpression_StateName : public CTilegenExpression_Unary< const char *, const CTilegenState * >
{
public:
	typedef const CTilegenState * ParameterType;
	CTilegenExpression_StateName( ITilegenExpression< const CTilegenState * > *pExpression = NULL ) : CTilegenExpression_Unary( pExpression ) { }

	virtual const char *DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &pState );
	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Returns a constant value.
//-----------------------------------------------------------------------------
template< typename TValue >
class CTilegenExpression_Literal : public ITilegenExpression< TValue >
{
public:
	explicit CTilegenExpression_Literal( TValue value ) : 
		m_Value( value )
	{
	}

	virtual const char *GetTypeName() { Assert( 0 ); return NULL; }

	virtual TValue Evaluate( CFreeVariableMap *pContext ) { return m_Value; }

protected:
	TValue m_Value;
};

//-----------------------------------------------------------------------------
// Returns a constant integer value.
//-----------------------------------------------------------------------------
class CTilegenExpression_LiteralInt : public CTilegenExpression_Literal< int >
{
public: 
	explicit CTilegenExpression_LiteralInt( int nValue = 0 ) : CTilegenExpression_Literal< int >( nValue ) { }
	
	virtual bool LoadFromKeyValues( KeyValues *pKeyValues )
	{ 
		m_Value = pKeyValues->GetInt( "value", 0 );
		return true;
	}

	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Returns a constant integer value.
//-----------------------------------------------------------------------------
class CTilegenExpression_LiteralBool : public CTilegenExpression_Literal< bool >
{
public: 
	explicit CTilegenExpression_LiteralBool( bool bValue = false ) : CTilegenExpression_Literal< bool >( bValue ) { }

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues )
	{ 
		m_Value = pKeyValues->GetBool( "value", false );
		return true;
	}

	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Returns a constant float value.
//-----------------------------------------------------------------------------
class CTilegenExpression_LiteralFloat : public CTilegenExpression_Literal< float >
{
public: 
	explicit CTilegenExpression_LiteralFloat( float flValue = 0.0f ) : CTilegenExpression_Literal< float >( flValue ) { }

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues )
	{ 
		m_Value = pKeyValues->GetFloat( "value", 0 );
		return true;
	}

	virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Returns a constant string value.
//
// @TODO: use a string pool so we can create new strings from 
// script operations and not worry about lifetime.
//-----------------------------------------------------------------------------
class CTilegenExpression_LiteralString : public CTilegenExpression_Literal< const char * >
{
public: 
	explicit CTilegenExpression_LiteralString( const char *pValue = NULL ) :
	CTilegenExpression_Literal< const char * >( NULL )
	{
		if ( pValue == NULL ) pValue = "";
		Q_strncpy( m_StringData, pValue, MAX_TILEGEN_IDENTIFIER_LENGTH );
		m_Value = m_StringData;
	}

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues )
	{ 
		const char *pValue = pKeyValues->GetString( "value", NULL );
		if ( pValue == NULL )
		{
			Log_Warning( LOG_TilegenLayoutSystem, "No value specified for CTilegenExpression_Literal in key values.\n" );
			return false;
		}
		Q_strncpy( m_StringData, pValue, MAX_TILEGEN_IDENTIFIER_LENGTH );
		m_Value = m_StringData;
		return true;
	}

	virtual const char *GetTypeName();

protected:
	char m_StringData[MAX_TILEGEN_IDENTIFIER_LENGTH];
};

//-----------------------------------------------------------------------------
// Returns the value of a named free variable.
//-----------------------------------------------------------------------------
template< typename TValue >
class CTilegenExpression_Variable : public ITilegenExpression< TValue >
{
public:
	CTilegenExpression_Variable( ITilegenExpression< const char * > *pVariableNameExpression = NULL, bool bDisallowNULL = true ) :
	  m_pVariableNameExpression( pVariableNameExpression ),
	  m_bDisallowNULL( bDisallowNULL )
	  {	  
	  }

	  virtual TValue Evaluate( CFreeVariableMap *pContext ) 
	  { 
		  const char *pVariableName = m_pVariableNameExpression->Evaluate( pContext );
		  if ( m_bDisallowNULL ) 
		  {
			  return ( TValue )pContext->GetFreeVariableDisallowNULL( pVariableName );
		  }
		  else
		  {
			  return ( TValue )pContext->GetFreeVariableOrNULL( pVariableName );
		  }
	  }

	  virtual bool LoadFromKeyValues( KeyValues *pKeyValues )
	  { 
		  if ( !CreateExpressionFromKeyValuesBlock( pKeyValues, "variable", GetTypeName(), &m_pVariableNameExpression ) )
		  {
			  Log_Warning( LOG_TilegenLayoutSystem, "No variable specified for CTilegenExpression_Variable in key values.\n" );
			  return false;
		  }
		  m_bDisallowNULL = pKeyValues->GetBool( "disallow_null", true );
		  return true;
	  }

private:
	ITilegenExpression< const char * > *m_pVariableNameExpression;
	bool m_bDisallowNULL;
};

//-----------------------------------------------------------------------------
// Define some common variable types.
//-----------------------------------------------------------------------------
#define DEFINE_EXPRESSION_VARIABLE( CType, TypeName ) \
class CTilegenExpression_Variable##TypeName : public CTilegenExpression_Variable< CType > \
{ \
public: \
	CTilegenExpression_Variable##TypeName( ITilegenExpression< const char * > *pVariableNameExpression = NULL, bool bDisallowNULL = true ) : CTilegenExpression_Variable< CType >( pVariableNameExpression, bDisallowNULL ) { } \
	virtual const char *GetTypeName(); \
}

DEFINE_EXPRESSION_VARIABLE( int, Int );
DEFINE_EXPRESSION_VARIABLE( const char *, String );
DEFINE_EXPRESSION_VARIABLE( const CExit *, Exit );
DEFINE_EXPRESSION_VARIABLE( const CRoomCandidate *, RoomCandidate );
DEFINE_EXPRESSION_VARIABLE( const CRoomTemplate *, RoomTemplate );
DEFINE_EXPRESSION_VARIABLE( const CTilegenState *, State );

#undef DEFINE_EXPRESSION_VARIABLE

//-----------------------------------------------------------------------------
// Applies a functional map and reduce operator to every element of a range.
//-----------------------------------------------------------------------------
template< typename TMapOutput, typename TMapInput >
class CTilegenExpression_MapReduce : public ITilegenExpression< TMapOutput >
{
public:
	CTilegenExpression_MapReduce() :
		m_pInputRangeExpression( NULL ),
		m_pInputRange( NULL ),
		m_pMapFunction( NULL ),
		m_pReduceFunction( NULL )
	{
		m_IteratorName[0] = '\0';
	}

	CTilegenExpression_MapReduce( ITilegenRange< TMapInput > *pRange, ITilegenExpression< TMapOutput > *pMapFunction, CTilegenExpression_Multi< TMapOutput > *pReduceFunction, const char *pIteratorName ) :
		m_pInputRangeExpression( NULL ),
		m_pInputRange( pRange ),
		m_pMapFunction( pMapFunction ),
		m_pReduceFunction( pReduceFunction )
	{
		if ( pIteratorName == NULL ) pIteratorName = "";
		Q_strncpy( m_IteratorName, pIteratorName, MAX_TILEGEN_IDENTIFIER_LENGTH );
	}

	CTilegenExpression_MapReduce( ITilegenExpression< ITilegenRange< TMapInput > * > *pRangeExpression, ITilegenExpression< TMapOutput > *pMapFunction, CTilegenExpression_Multi< TMapOutput > *pReduceFunction, const char *pIteratorName ) :
		m_pInputRangeExpression( pRangeExpression ),
		m_pInputRange( NULL ),
		m_pMapFunction( pMapFunction ),
		m_pReduceFunction( pReduceFunction )
	{	
		if ( pIteratorName == NULL ) pIteratorName = "";
		Q_strncpy( m_IteratorName, pIteratorName, MAX_TILEGEN_IDENTIFIER_LENGTH );
	}

	~CTilegenExpression_MapReduce() 
	{
		// They can't both be non-NULL
		Assert( m_pInputRangeExpression == NULL || m_pInputRange == NULL );
		delete m_pInputRange; 
		delete m_pInputRangeExpression;
		delete m_pMapFunction;
		delete m_pReduceFunction; 
	}

	virtual bool LoadFromKeyValues( KeyValues *pKeyValues )
	{
		  // Allow derived class to partially specify members of this class and partially load some from disk
		  if ( m_pInputRangeExpression == NULL && m_pInputRange == NULL )
		  {
			  KeyValues *pRangeExpressionKV = pKeyValues->FindKey( "range_expression" );
			  KeyValues *pRangeKV = pKeyValues->FindKey( "range" );
			  if ( pRangeExpressionKV == NULL && pRangeKV == NULL )
			  {
				  Log_Warning( LOG_TilegenLayoutSystem, "Could not find 'range' or 'range_expression' subkey in CTilegenExpression_MapReduce in KeyValues file.\n" );
				  return false;
			  } 
			  else if ( pRangeExpressionKV != NULL && pRangeKV != NULL )
			  {
				  Log_Warning( LOG_TilegenLayoutSystem, "Found both a 'range' and 'range_expression' subkey in CTilegenExpression_MapReduce in KeyValues file.\n" );
				  return false;
			  }

			  if ( pRangeExpressionKV != NULL )
			  {
				  if ( !CreateExpressionFromKeyValuesBlock( pKeyValues, "range_expression", GetTypeName(), &m_pInputRangeExpression ) )
					  return false;
			  }
			  else if ( pRangeKV != NULL)
			  {
				  if ( !CreateRangeFromKeyValuesBlock( pKeyValues, "range", GetTypeName(), &m_pInputRange ) )
					  return false;
			  }
		  }

		  if ( m_pMapFunction == NULL )
		  {
			  if ( !CreateExpressionFromKeyValuesBlock< TMapOutput >( pKeyValues, "map", GetTypeName(), &m_pMapFunction ) )
				  return false;
		  }

		  if ( m_pReduceFunction == NULL )
		  {
			  ITilegenExpression< TMapOutput > *pReduceFunction = NULL;
			  // Create an empty instance of the binary function since we will never call Evaluate on it (just DirectEvaluate)
			  CreateExpressionFromKeyValuesBlock< TMapOutput >( pKeyValues, "reduce", GetTypeName(), &pReduceFunction, false, true );
			  m_pReduceFunction = static_cast< CTilegenExpression_Multi< TMapOutput > * >( pReduceFunction );
		  }

		  if ( m_IteratorName[0] == '\0' )
		  {
			  const char *pIteratorName = pKeyValues->GetString( "iterator", NULL );
			  if ( pIteratorName == NULL )
			  {
				  Log_Warning( LOG_TilegenLayoutSystem, "No iterator name specified for CTilegenExpression_MapReduce in Key Values.\n" );
				  return false;
			  }
			  Q_strncpy( m_IteratorName, pIteratorName, MAX_TILEGEN_IDENTIFIER_LENGTH );
		  }
		  return true;
	}

	virtual TMapOutput Evaluate( CFreeVariableMap *pContext )
	{
		  TMapOutput reducedValue = TMapOutput( 0 );
		  int nValues = 0;
		  ITilegenRange< TMapInput > *pInputRange = m_pInputRange;
		  if ( m_pInputRangeExpression )
		  {
			  // Assume that the expression has already initialized the range.
			  pInputRange = m_pInputRangeExpression->Evaluate( pContext );
		  }
		  else
		  {
			  pInputRange->Initialize( pContext );
		  }

		  pInputRange->Reset();
		  while ( pInputRange->MoveNext() )
		  {
			  // The map function must use a CTilegenExpression_Variable (initialized to the range iterator variable name) to get at the current range value.
			  TMapInput currentValue = pInputRange->GetCurrent();
			  pContext->SetOrCreateFreeVariable( m_IteratorName, ( void * )currentValue );
			  TMapOutput mappedValue = m_pMapFunction->Evaluate( pContext );
			  if ( nValues == 0 )
			  {
				  reducedValue = mappedValue;
			  }
			  else
			  {
				  reducedValue = m_pReduceFunction->DirectEvaluate( pContext, reducedValue, mappedValue );
			  }

			  ++ nValues;
		  }
		  return reducedValue;
	}

protected:
	ITilegenExpression< ITilegenRange< TMapInput > * > *m_pInputRangeExpression;
	ITilegenRange< TMapInput > *m_pInputRange;
	ITilegenExpression< TMapOutput > *m_pMapFunction;
	CTilegenExpression_Multi< TMapOutput > *m_pReduceFunction;
	char m_IteratorName[MAX_TILEGEN_IDENTIFIER_LENGTH];
};

//-----------------------------------------------------------------------------
// Applies a functional map-reduce operator to a range of exits, 
// returning an integer.
//-----------------------------------------------------------------------------
class CTilegenExpression_MapReduceExits : public CTilegenExpression_MapReduce< int, const CExit * > 
{ 
public:
	CTilegenExpression_MapReduceExits( ITilegenRange< const CExit * > *pRange = NULL, ITilegenExpression< int > *pMapFunction = NULL, CTilegenExpression_Multi< int > *pReduceFunction = NULL, const char *pIteratorName = NULL ) :
	  CTilegenExpression_MapReduce< int, const CExit * >( pRange, pMapFunction, pReduceFunction, pIteratorName )
	  {
	  }

	  virtual const char *GetTypeName();
};

//-----------------------------------------------------------------------------
// Counts the number of elements in a range.
//-----------------------------------------------------------------------------
template< typename TInput >
class CTilegenExpression_CountRange : public CTilegenExpression_MapReduce< int, TInput >
{
public:
	CTilegenExpression_CountRange( ITilegenRange< TInput > *pRange = NULL ) :
	  CTilegenExpression_MapReduce( pRange, new CTilegenExpression_LiteralInt( 1 ), new CTilegenExpression_Add(), "$$unused" )
	  {
	  }
};

//-----------------------------------------------------------------------------
// Counts the number of exits in a range of exits.
//-----------------------------------------------------------------------------
class CTilegenExpression_CountExits : public CTilegenExpression_CountRange< const CExit * >
{
public:
	explicit CTilegenExpression_CountExits( ITilegenRange< const CExit * > *pRange = NULL ) :
	CTilegenExpression_CountRange< const CExit * >( pRange )
	{
	}

	virtual const char *GetTypeName();
};

#endif // TILEGEN_EXPRESSIONS_H