//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Definition of many useful tilegen expressions which can be 
// evaluated to return values.
//
//===============================================================================

#include "Room.h"
#include "LevelTheme.h"
#include "MapLayout.h"
#include "tilegen_layout_system.h"
#include "tilegen_expressions.h"

void *CFreeVariableMap::GetFreeVariableOrNULL( const char *pName ) const
{ 
	int nIndex = Find( pName );
	if ( nIndex == InvalidIndex() )
	{
		return NULL;
	}
	return Element( nIndex );
}

void *CFreeVariableMap::GetFreeVariable( const char *pName ) const
{
	int nIndex = Find( pName );
	if ( nIndex == InvalidIndex() )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Free variable '%s' not found.\n", pName );
		void *pLayoutSystem = GetFreeVariableOrNULL( "LayoutSystem" );
		if ( pLayoutSystem != NULL ) 
		{
			( ( CLayoutSystem * ) pLayoutSystem )->OnError();
		}
		return NULL;
	}
	return Element( nIndex );
}

void *CFreeVariableMap::GetFreeVariableDisallowNULL( const char *pName ) const
{
	int nIndex = Find( pName );
	if ( nIndex == InvalidIndex() )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Free variable '%s' not found.\n", pName );
		void *pLayoutSystem = GetFreeVariableOrNULL( "LayoutSystem" );
		if ( pLayoutSystem != NULL ) 
		{
			( ( CLayoutSystem * ) pLayoutSystem )->OnError();
		}
		return NULL;
	}
	void *pPtr = Element( nIndex );
	if ( pPtr == NULL )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Free variable '%s' found, but value is NULL or 0.\n", pName );
		void *pLayoutSystem = GetFreeVariableOrNULL( "LayoutSystem" );
		if ( pLayoutSystem != NULL ) 
		{
			( ( CLayoutSystem * ) pLayoutSystem )->OnError();
		}
	}
	return pPtr;
}

void CFreeVariableMap::SetOrCreateFreeVariable( const char *pName, void *pValue )
{
	int nIndex = Find( pName );
	if ( nIndex != InvalidIndex() )
	{
		Element( nIndex ) = pValue;
	}
	else
	{
		Insert( pName, pValue );
	}
}

const char *CTilegenExpression_StringConcatenate::DirectEvaluate( CFreeVariableMap *pContext, const CTilegenExpression_StringConcatenate::TExpressionValue &param1, const CTilegenExpression_StringConcatenate::TExpressionValue &param2 )
{
	char buf[MAX_TILEGEN_IDENTIFIER_LENGTH];
	Q_strncpy( buf, param1, MAX_TILEGEN_IDENTIFIER_LENGTH );
	Q_strncat( buf, param2, MAX_TILEGEN_IDENTIFIER_LENGTH );
	buf[MAX_TILEGEN_IDENTIFIER_LENGTH - 1] = '\0';
	return pContext->m_StringPool.Allocate( buf );
}

const char *CTilegenExpression_RoomName::DirectEvaluate( CFreeVariableMap *pContext, const CTilegenExpression_RoomName::ParameterType &pRoomTemplate ) 
{ 
	return pRoomTemplate ? pRoomTemplate->GetFullName() : ""; 
}

int CTilegenExpression_RoomArea::DirectEvaluate( CFreeVariableMap *pContext, const CTilegenExpression_RoomName::ParameterType &pRoomTemplate ) 
{ 
	return pRoomTemplate ? pRoomTemplate->GetArea() : 1;
}

const char *CTilegenExpression_ExtractRoomName::DirectEvaluate( CFreeVariableMap *pContext, const CTilegenExpression_ExtractRoomName::ParameterType &pFullRoomName )
{
	char themeName[MAX_TILEGEN_IDENTIFIER_LENGTH];
	char roomName[MAX_TILEGEN_IDENTIFIER_LENGTH];
	if ( !CLevelTheme::SplitThemeAndRoom( pFullRoomName, themeName, MAX_TILEGEN_IDENTIFIER_LENGTH, roomName, MAX_TILEGEN_IDENTIFIER_LENGTH ) )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Could not split theme name from room (full name: %s).\n", pFullRoomName );
		return NULL;
	}
	return pContext->m_StringPool.Allocate( roomName );
}

const char *CTilegenExpression_ExtractThemeName::DirectEvaluate( CFreeVariableMap *pContext, const CTilegenExpression_ExtractRoomName::ParameterType &pFullRoomName )
{
	char themeName[MAX_TILEGEN_IDENTIFIER_LENGTH];
	char roomName[MAX_TILEGEN_IDENTIFIER_LENGTH];
	if ( !CLevelTheme::SplitThemeAndRoom( pFullRoomName, themeName, MAX_TILEGEN_IDENTIFIER_LENGTH, roomName, MAX_TILEGEN_IDENTIFIER_LENGTH ) )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Could not split theme name from room (full name: %s).\n", pFullRoomName );
		return NULL;
	}
	return pContext->m_StringPool.Allocate( themeName );
}

const CRoom *CTilegenExpression_LastPlacedRoom::Evaluate( CFreeVariableMap *pContext )
{ 
	CMapLayout *pMapLayout = ( CMapLayout * )pContext->GetFreeVariableDisallowNULL( "MapLayout" );
	if ( pMapLayout->m_PlacedRooms.Count() > 0 )
	{
		return pMapLayout->m_PlacedRooms[pMapLayout->m_PlacedRooms.Count() - 1];
	}
	else
	{
		return NULL;
	}
}

const CRoom *CTilegenExpression_SourceRoomFromExit::DirectEvaluate( CFreeVariableMap *pContext, const CTilegenExpression_SourceRoomFromExit::ParameterType &pExit ) 
{ 
	return pExit ? pExit->pSourceRoom : NULL; 
}

const CRoomTemplate *CTilegenExpression_SourceRoomTemplateFromExit::DirectEvaluate( CFreeVariableMap *pContext, const CTilegenExpression_SourceRoomTemplateFromExit::ParameterType &pExit ) 
{ 
	return pExit ? pExit->pSourceRoom->m_pRoomTemplate : NULL; 
}

bool CTilegenExpression_ChokepointGrowSource::DirectEvaluate( CFreeVariableMap *pContext, const CTilegenExpression_ChokepointGrowSource::ParameterType &pExit ) 
{ 
	return pExit ? pExit->m_bChokepointGrowSource : false; 
}

int CTilegenExpression_RoomChildCount::DirectEvaluate( CFreeVariableMap *pContext, const CTilegenExpression_RoomChildCount::ParameterType &pRoom ) 
{ 
	return pRoom ? pRoom->m_iNumChildren : 0;
}

const CRoomTemplate *CTilegenExpression_RoomTemplateFromName::DirectEvaluate( CFreeVariableMap *pContext, const CTilegenExpression_RoomTemplateFromName::ParameterType &pName )
{
	char themeName[MAX_TILEGEN_IDENTIFIER_LENGTH];
	char roomName[MAX_TILEGEN_IDENTIFIER_LENGTH];
	if ( !CLevelTheme::SplitThemeAndRoom( pName, themeName, MAX_TILEGEN_IDENTIFIER_LENGTH, roomName, MAX_TILEGEN_IDENTIFIER_LENGTH ) )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Could not split theme name from room (full name: %s).\n", pName );
		return NULL;
	}
	CLevelTheme *pTheme = CLevelTheme::FindTheme( themeName );
	if ( pTheme == NULL )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Could not find theme '%s'.\n", themeName );
		return NULL;
	}
	CRoomTemplate *pTemplate = pTheme->FindRoom( roomName );
	if ( pTheme == NULL )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Could not find room '%s' in theme '%s'.\n", roomName, themeName );
		return NULL;
	}
	return pTemplate;
}

int CTilegenExpression_XPosition::DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &pRoomCandidate )
{
	return pRoomCandidate ? pRoomCandidate->m_iXPos : -1;
}

int CTilegenExpression_YPosition::DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &pRoomCandidate )
{
	return pRoomCandidate ? pRoomCandidate->m_iYPos : -1;
}

CTilegenExpression_HasTag::CTilegenExpression_HasTag( 
	ITilegenExpression< const CRoomTemplate * > *pRoomTemplateExpression, 
	ITilegenExpression< const char * > *pTagExpression ) :
m_pRoomTemplateExpression( pRoomTemplateExpression ),
m_pTagExpression( pTagExpression )
{
}

bool CTilegenExpression_HasTag::LoadFromKeyValues( KeyValues *pKeyValues )
{
	bool bSuccess = true;
	bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "room_template", GetTypeName(), &m_pRoomTemplateExpression );
	bSuccess &= CreateExpressionFromKeyValuesBlock( pKeyValues, "tag", GetTypeName(), &m_pTagExpression );
	return bSuccess;
}

bool CTilegenExpression_HasTag::Evaluate( CFreeVariableMap *pContext )
{  
	const CRoomTemplate *pRoomTemplate = m_pRoomTemplateExpression->Evaluate( pContext );
	if ( pRoomTemplate != NULL )
	{
		return pRoomTemplate->HasTag( m_pTagExpression->Evaluate( pContext ) ); 
	}
	else
	{
		return false;
	}
}

CTilegenExpression_CanPlaceRandomly::CTilegenExpression_CanPlaceRandomly(
	ITilegenExpression< const CRoomTemplate * > *pRoomTemplateExpression ) :
m_pRoomTemplateExpression( pRoomTemplateExpression )
{
}

bool CTilegenExpression_CanPlaceRandomly::LoadFromKeyValues( KeyValues *pKeyValues )
{
	return CreateExpressionFromKeyValuesBlock( pKeyValues, "room_template", GetTypeName(), &m_pRoomTemplateExpression );
}

bool CTilegenExpression_CanPlaceRandomly::Evaluate( CFreeVariableMap *pContext )
{
	return !m_pRoomTemplateExpression->Evaluate( pContext )->ShouldOnlyPlaceByRequest();
}

int CTilegenExpression_NumTimesPlaced::DirectEvaluate( CFreeVariableMap *pContext, const CTilegenExpression_NumTimesPlaced::ParameterType &pRoomTemplate ) 
{ 
	CMapLayout *pMapLayout = ( CMapLayout * )pContext->GetFreeVariableDisallowNULL( "MapLayout" );
	int nPlacementCount = 0;
	for ( int i = 0; i < pMapLayout->m_PlacedRooms.Count(); ++ i )
	{
		if ( pMapLayout->m_PlacedRooms[i]->m_pRoomTemplate == pRoomTemplate )
		{
			++ nPlacementCount;
		}
	}
	return nPlacementCount;
}

const char *CTilegenExpression_ExitTag::DirectEvaluate( CFreeVariableMap *pContext, const CTilegenExpression_ExitTag::ParameterType &pExit )
{ 
	return pExit ? pExit->m_szExitTag : ""; 
}

int CTilegenExpression_ExitDirection::DirectEvaluate( CFreeVariableMap *pContext, const CTilegenExpression_ExitDirection::ParameterType &pExit ) 
{ 
	return pExit ? pExit->ExitDirection : -1; 
}

const CTilegenState *CTilegenExpression_ParentState::DirectEvaluate( CFreeVariableMap *pContext, const ParameterType &pState )
{
	return pState->GetParentState();
}

const char *CTilegenExpression_StateName::DirectEvaluate( CFreeVariableMap *pContext, const CTilegenExpression_StateName::ParameterType &pState )
{
	return pState->GetStateName();
}

//-----------------------------------------------------------------------------
// Template specialization to properly handle parsing literal integers.
//-----------------------------------------------------------------------------
ITilegenExpression< int > *ReadLiteralIntValue( KeyValues *pKeyValues )
{
	return new CTilegenExpression_LiteralInt( pKeyValues->GetInt() );
}

//-----------------------------------------------------------------------------
// Template specialization to properly handle parsing literal booleans.
//-----------------------------------------------------------------------------
ITilegenExpression< bool > *ReadLiteralBoolValue( KeyValues *pKeyValues )
{
	return new CTilegenExpression_LiteralBool( pKeyValues->GetBool() );
}

//-----------------------------------------------------------------------------
// Template specialization to properly handle parsing literal floats.
//-----------------------------------------------------------------------------
ITilegenExpression< float > *ReadLiteralFloatValue( KeyValues *pKeyValues )
{
	return new CTilegenExpression_LiteralFloat( pKeyValues->GetFloat() );
}

//-----------------------------------------------------------------------------
// Template specialization to properly handle parsing literal strings.
//-----------------------------------------------------------------------------
ITilegenExpression< const char * > *ReadLiteralStringValue( KeyValues *pKeyValues )
{
	return new CTilegenExpression_LiteralString( pKeyValues->GetString() );
}
