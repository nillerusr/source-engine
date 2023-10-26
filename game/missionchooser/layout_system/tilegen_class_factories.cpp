//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Boilerplate code to generically register tilegen classes
// so they can be instantiated and parsed by name.
//
//===============================================================================

#include "tilegen_class_interfaces.h"
#include "tilegen_expressions.h"
#include "tilegen_actions.h"
#include "tilegen_ranges.h"
#include "tilegen_class_factories.h"

class CExit;
class CRoomTemplate;
class CRoomCandidate;

//-----------------------------------------------------------------------------
// Provide storage for the static expression class registries
// (one for each return type).
//-----------------------------------------------------------------------------
#define DEFINE_TILEGEN_EXPRESSION_FACTORY( ExpressionType ) \
	CUtlVector< ITilegenClassFactory< ITilegenExpression< ExpressionType > > * > CTilegenClassRegistry< ITilegenExpression< ExpressionType > >::m_FactoryList; \
	const char *CTilegenClassRegistry< ITilegenExpression< ExpressionType > >::m_pFactoryName = "Expression Registry (" #ExpressionType ")"

DEFINE_TILEGEN_EXPRESSION_FACTORY( bool );
DEFINE_TILEGEN_EXPRESSION_FACTORY( int );
DEFINE_TILEGEN_EXPRESSION_FACTORY( float );
DEFINE_TILEGEN_EXPRESSION_FACTORY( const char * );
DEFINE_TILEGEN_EXPRESSION_FACTORY( const CExit * );
DEFINE_TILEGEN_EXPRESSION_FACTORY( const CRoomTemplate * );
DEFINE_TILEGEN_EXPRESSION_FACTORY( const CRoom * );
DEFINE_TILEGEN_EXPRESSION_FACTORY( const CRoomCandidate * );
DEFINE_TILEGEN_EXPRESSION_FACTORY( ITilegenRange< const CExit * > * );
DEFINE_TILEGEN_EXPRESSION_FACTORY( const CTilegenState * );

//-----------------------------------------------------------------------------
// Provide storage for the static action class registry.
//-----------------------------------------------------------------------------
CUtlVector< ITilegenClassFactory< ITilegenAction > * > CTilegenClassRegistry< ITilegenAction >::m_FactoryList;
const char *CTilegenClassRegistry< ITilegenAction >::m_pFactoryName = "Action Registry";

//-----------------------------------------------------------------------------
// Provide storage for the static range class registries
// (one for each return type).
//-----------------------------------------------------------------------------
#define DEFINE_TILEGEN_RANGE_FACTORY( ExpressionType ) \
	CUtlVector< ITilegenClassFactory< ITilegenRange< ExpressionType > > * > CTilegenClassRegistry< ITilegenRange< ExpressionType > >::m_FactoryList; \
	const char *CTilegenClassRegistry< ITilegenRange< ExpressionType > >::m_pFactoryName = "Range Registry (" #ExpressionType ")"

DEFINE_TILEGEN_RANGE_FACTORY( const CExit * );

//-----------------------------------------------------------------------------
// Macros to implement virtual functions and create & register static instances
// of each class factory for expressions, actions, and ranges.
//-----------------------------------------------------------------------------
#define IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( ClassName, OperatorString ) \
	const char *CTilegenClassFactory< ITilegenExpression< ClassName::TExpressionValue >, ClassName >::GetName() { return OperatorString; } \
	const char *ClassName::GetTypeName() { return #ClassName; } \
	static CTilegenClassFactory< ITilegenExpression< ClassName::TExpressionValue >, ClassName > s_##ClassName##_Factory;

#define IMPLEMENT_TILEGEN_EXPRESSION( ClassName ) IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( ClassName, #ClassName )

#define REGISTER_TILEGEN_EXPRESSION( ClassName ) \
	CTilegenClassRegistry< ITilegenExpression< ClassName::TExpressionValue > >::AddFactory( &s_##ClassName##_Factory );

#define IMPLEMENT_TILEGEN_ACTION( ClassName ) \
	const char *CTilegenClassFactory< ITilegenAction, ClassName >::GetName() { return #ClassName; } \
	const char *ClassName::GetTypeName() { return #ClassName; } \
	static CTilegenClassFactory< ITilegenAction, ClassName > s_##ClassName##_Factory;

#define REGISTER_TILEGEN_ACTION( ClassName ) \
	CTilegenClassRegistry< ITilegenAction >::AddFactory( &s_##ClassName##_Factory );

#define IMPLEMENT_TILEGEN_RANGE( ClassName ) \
	const char *CTilegenClassFactory< ITilegenRange< ClassName::TElementValue >, ClassName >::GetName() { return #ClassName; } \
	static CTilegenClassFactory< ITilegenRange< ClassName::TElementValue >, ClassName > s_##ClassName##_Factory;

#define REGISTER_TILEGEN_RANGE( ClassName ) \
	CTilegenClassRegistry< ITilegenRange< ClassName::TElementValue > >::AddFactory( &s_##ClassName##_Factory );

//-----------------------------------------------------------------------------
// Every new expression, action, and range must be both implemented
// and registered here.
//-----------------------------------------------------------------------------

IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_Add, "+" );
IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_Subtract, "-" );
IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_FloatMultiply, "fmul" );
IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_And, "&&" );
IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_Or, "||" );
IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_Not, "!" );

IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_BoolToInt );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_IntToBool );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_FloatToInt );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_IntToFloat );

IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_Greater, ">" );
IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_GreaterOrEqual, ">=" );
IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_Equal, "==" );
IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_NotEqual, "!=" );
IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_Less, "<" );
IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_LessOrEqual, "<=" );

IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_LiteralInt );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_LiteralBool );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_LiteralFloat );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_LiteralString );
IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_StringEqual, "streq" );
IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_StringNotEqual, "!streq" );
IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_StringConcatenate, "concat" );

IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_RoomName );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_RoomArea );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_ExtractRoomName );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_ExtractThemeName );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_LastPlacedRoom );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_SourceRoomFromExit );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_SourceRoomTemplateFromExit );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_ChokepointGrowSource );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_RoomChildCount );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_RoomTemplateFromName );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_XPosition );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_YPosition );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_HasTag );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_CanPlaceRandomly );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_NumTimesPlaced );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_ExitTag );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_ExitDirection );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_ParentState );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_StateName );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_MapReduceExits );
IMPLEMENT_TILEGEN_EXPRESSION( CTilegenExpression_CountExits );

IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_VariableInt, "var_int" );
IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_VariableString, "var_string" );
IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_VariableExit, "var_exit" );
IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_VariableRoomCandidate, "var_room_candidate" );
IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_VariableRoomTemplate, "var_room_template" );
IMPLEMENT_TILEGEN_EXPRESSION_OVERRIDE_NAME( CTilegenExpression_VariableState, "var_state" );

IMPLEMENT_TILEGEN_ACTION( CTilegenAction_NestedActions );
IMPLEMENT_TILEGEN_ACTION( CTilegenAction_SetVariableInt );
IMPLEMENT_TILEGEN_ACTION( CTilegenAction_SetVariableString );
IMPLEMENT_TILEGEN_ACTION( CTilegenAction_SetVariableBoolExpression );
IMPLEMENT_TILEGEN_ACTION( CTilegenAction_SetVariableAction );
IMPLEMENT_TILEGEN_ACTION( CTilegenAction_AddRoomCandidates );
IMPLEMENT_TILEGEN_ACTION( CTilegenAction_AddRoomCandidatesAtLocation );
IMPLEMENT_TILEGEN_ACTION( CTilegenAction_ChooseCandidate );
IMPLEMENT_TILEGEN_ACTION( CTilegenAction_FilterCandidatesByDirection );
IMPLEMENT_TILEGEN_ACTION( CTilegenAction_FilterCandidatesForLinearGrowth );
IMPLEMENT_TILEGEN_ACTION( CTilegenAction_SwitchState );
IMPLEMENT_TILEGEN_ACTION( CTilegenAction_FinishGeneration );
IMPLEMENT_TILEGEN_ACTION( CTilegenAction_EpicFail );
IMPLEMENT_TILEGEN_ACTION( CTilegenAction_EnsureRoomExists );
IMPLEMENT_TILEGEN_ACTION( CTilegenAction_AddConnectorRoomCandidates );
IMPLEMENT_TILEGEN_ACTION( CTilegenAction_PlaceComponent );
IMPLEMENT_TILEGEN_ACTION( CTilegenAction_AddInstances );
IMPLEMENT_TILEGEN_ACTION( CTilegenAction_AddInstanceToRoom );
IMPLEMENT_TILEGEN_ACTION( CTilegenAction_LoadLayout );

IMPLEMENT_TILEGEN_RANGE( CTilegenRange_NewOpenExits );
IMPLEMENT_TILEGEN_RANGE( CTilegenRange_ClosedExits );

void RegisterAllTilegenClasses()
{
	static bool bRegistered = false;

	if ( !bRegistered )
	{
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_Add );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_Subtract );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_FloatMultiply );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_And );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_Or );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_Not );

		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_BoolToInt );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_IntToBool );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_FloatToInt );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_IntToFloat );
		
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_Greater );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_GreaterOrEqual );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_Equal );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_NotEqual );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_Less );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_LessOrEqual );
		
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_LiteralInt );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_LiteralBool );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_LiteralFloat );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_LiteralString );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_StringEqual );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_StringNotEqual );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_StringConcatenate );

		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_RoomName );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_RoomArea );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_ExtractRoomName );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_ExtractThemeName );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_LastPlacedRoom );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_SourceRoomFromExit );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_SourceRoomTemplateFromExit );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_ChokepointGrowSource );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_RoomChildCount );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_RoomTemplateFromName );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_XPosition );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_YPosition );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_HasTag );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_CanPlaceRandomly );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_NumTimesPlaced );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_ExitTag );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_ExitDirection );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_ParentState );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_StateName );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_MapReduceExits );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_CountExits );

		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_VariableInt );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_VariableString );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_VariableExit );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_VariableRoomCandidate );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_VariableRoomTemplate );
		REGISTER_TILEGEN_EXPRESSION( CTilegenExpression_VariableState );

		REGISTER_TILEGEN_ACTION( CTilegenAction_NestedActions );
		REGISTER_TILEGEN_ACTION( CTilegenAction_SetVariableInt );
		REGISTER_TILEGEN_ACTION( CTilegenAction_SetVariableString );
		REGISTER_TILEGEN_ACTION( CTilegenAction_SetVariableBoolExpression );
		REGISTER_TILEGEN_ACTION( CTilegenAction_SetVariableAction );
		REGISTER_TILEGEN_ACTION( CTilegenAction_AddRoomCandidates );
		REGISTER_TILEGEN_ACTION( CTilegenAction_AddRoomCandidatesAtLocation );
		REGISTER_TILEGEN_ACTION( CTilegenAction_ChooseCandidate );
		REGISTER_TILEGEN_ACTION( CTilegenAction_FilterCandidatesByDirection );
		REGISTER_TILEGEN_ACTION( CTilegenAction_FilterCandidatesForLinearGrowth );
		REGISTER_TILEGEN_ACTION( CTilegenAction_SwitchState );
		REGISTER_TILEGEN_ACTION( CTilegenAction_FinishGeneration );
		REGISTER_TILEGEN_ACTION( CTilegenAction_EpicFail );
		REGISTER_TILEGEN_ACTION( CTilegenAction_EnsureRoomExists );
		REGISTER_TILEGEN_ACTION( CTilegenAction_AddConnectorRoomCandidates );
		REGISTER_TILEGEN_ACTION( CTilegenAction_PlaceComponent );
		REGISTER_TILEGEN_ACTION( CTilegenAction_AddInstances );
		REGISTER_TILEGEN_ACTION( CTilegenAction_AddInstanceToRoom );
		REGISTER_TILEGEN_ACTION( CTilegenAction_LoadLayout );

		REGISTER_TILEGEN_RANGE( CTilegenRange_NewOpenExits );
		REGISTER_TILEGEN_RANGE( CTilegenRange_ClosedExits );

		bRegistered = true;
	}
}

bool CreateActionAndCondition( KeyValues *pKeyValues, ITilegenAction **ppAction, ITilegenExpression< bool > **ppCondition )
{
	*ppCondition = NULL;
	*ppAction = CreateFromKeyValues< ITilegenAction >( pKeyValues );
	if ( *ppAction == NULL )
		return false;

	KeyValues *pConditionKey = pKeyValues->FindKey( "condition" );
	if ( pConditionKey != NULL )
	{
		*ppCondition = CreateFromKeyValues< ITilegenExpression< bool > >( pConditionKey );
		if ( *ppCondition == NULL )
			return false;
	}
	return true;
}

bool CreateActionAndConditionFromKeyValuesBlock( KeyValues *pParentKV, const char *pKeyName, const char *pParentClassName, ITilegenAction **ppAction, ITilegenExpression< bool > **ppCondition )
{
	*ppAction = NULL;
	*ppCondition = NULL;
	KeyValues *pSubKey = pParentKV->FindKey( pKeyName );
	if ( pSubKey == NULL )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Could not load sub-key '%s' for parent class '%s'.\n", pKeyName, pParentClassName );
		return false;
	}
	return CreateActionAndCondition( pSubKey, ppAction, ppCondition );
}