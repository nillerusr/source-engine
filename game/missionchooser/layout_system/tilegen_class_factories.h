//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Boilerplate template code to generically instantiate tilegen
// classes by name and parse them from KeyValues files.
// 
//===============================================================================

#ifndef TILEGEN_CLASS_FACTORIES_H
#define TILEGEN_CLASS_FACTORIES_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "utlvector.h"
#include "KeyValues.h"
#include "tilegen_class_interfaces.h"

//-----------------------------------------------------------------------------
// This function must be called at least once before any tilegen 
// expression, action, or range classes are parsed.
// Subsequent calls are ignored.
//-----------------------------------------------------------------------------
void RegisterAllTilegenClasses();

//-----------------------------------------------------------------------------
// The most generic, abstract base form of a tilegen class factory.
// Each derived template instantiation is capable of instantiating a single 
// kind of tilegen class derived from TTilegenInterface.
//
// TTilegenInterface - the type of interface this class factory returns,
//   e.g. ITilegenExpression< int >, ITilegenRange< const CExit * >, 
//        ITilegenAction
//-----------------------------------------------------------------------------
template< typename TTilegenInterface >
class ITilegenClassFactory
{
public:
	//-----------------------------------------------------------------------------
	// Creates an instance of the tilegen class this factory is templated on.
	//-----------------------------------------------------------------------------
	virtual TTilegenInterface * CreateInstance() = 0;

	//-----------------------------------------------------------------------------
	// Gets the friendly string name of the tilegen class this factory produces.
	//-----------------------------------------------------------------------------
	virtual const char *GetName() = 0;

	static TTilegenInterface *ReadLiteralValue( KeyValues *pKeyValues )
	{
		// Only some tilegen classes can be loaded this way.
		Log_Warning( LOG_TilegenLayoutSystem, "Tilegen class does not support literal values.\n" );
		return NULL; 
	}
};

// Annoying workaround due to template specialization rules.
// I want these implemented in a .cpp elsewhere but if I simply do a template prototype and define the function elsewhere,
// it won't specialize.  So I need a fully implemented template specialization to call these functions.
ITilegenExpression< int > *ReadLiteralIntValue( KeyValues *pKeyValues );
ITilegenExpression< bool > *ReadLiteralBoolValue( KeyValues *pKeyValues );
ITilegenExpression< float > *ReadLiteralFloatValue( KeyValues *pKeyValues );
ITilegenExpression< const char * > *ReadLiteralStringValue( KeyValues *pKeyValues );

// Class factory specializations to handle parsing literal values...

ITilegenExpression< int > *ITilegenClassFactory< ITilegenExpression< int > >::ReadLiteralValue( KeyValues *pKeyValues )
{
	return ReadLiteralIntValue( pKeyValues );
}

ITilegenExpression< bool > *ITilegenClassFactory< ITilegenExpression< bool > >::ReadLiteralValue( KeyValues *pKeyValues )
{
	return ReadLiteralBoolValue( pKeyValues );
}

ITilegenExpression< float > *ITilegenClassFactory< ITilegenExpression< float > >::ReadLiteralValue( KeyValues *pKeyValues )
{
	return ReadLiteralFloatValue( pKeyValues );
}

ITilegenExpression< char const * > *ITilegenClassFactory< ITilegenExpression< char const * > >::ReadLiteralValue( KeyValues *pKeyValues )
{
	return ReadLiteralStringValue( pKeyValues );
}

//-----------------------------------------------------------------------------
// Implementation of a generic tilegen class factory which
// can be specialized to implement either expression, action, and range class
// factories.
//
// TTilegenInterface - the type of interface this class factory produces,
//   e.g. ITilegenExpression< int >, ITilegenRange< const CExit * >, 
//        ITilegenAction
// TTilegenClass - the specific class (which must derive from 
//   TTilegenInterface) that this factory actually produduces,
//   e.g. CTilegenExpression_Add, CTilegenRange_NewOpenExits,
//        CTilegenAction_SwitchState
//-----------------------------------------------------------------------------
template< typename TTilegenInterface, typename TTilegenClass >
class CTilegenClassFactory : public ITilegenClassFactory< TTilegenInterface >
{
public:
	virtual TTilegenInterface * CreateInstance() { return new TTilegenClass(); }
	virtual const char *GetName();
};

//-----------------------------------------------------------------------------
// Class factory type to produce tilegen expressions.
//
// TExpressionValue - the type the expression class returns, e.g. bool, int
// TExpressionClass - the specific type of the expression, e.g.
//   CTilegenExpression_Add
//
// TExpressionClass must derive from ITilegenExpression< TExpressionValue >
//-----------------------------------------------------------------------------
template< typename TExpressionValue, typename TExpressionClass >
class CTilegenExpressionClassFactory : public CTilegenClassFactory< ITilegenExpression< TExpressionValue >, TExpressionClass >
{
public:
	// Must be implemented for each specialization of CTilegenExpressionClassFactory 
	// (see macro IMPLEMENT_TILEGEN_EXPRESSION in tilegen_class_factories.cpp).
	virtual const char *GetName();
};

//-----------------------------------------------------------------------------
// Class factory type to produce tilegen actions.
//
// TActionClass - the specific type of the action, e.g.
//   CTilegenAction_SwitchState
//
// TActionClass must derive from ITilegenAction
//-----------------------------------------------------------------------------
template< typename TActionClass >
class CTilegenActionClassFactory : public CTilegenClassFactory< ITilegenAction, TActionClass >
{
public:
	// Must be implemented for each specialization of CTilegenActionClassFactory
	// (see macro IMPLEMENT_TILEGEN_ACTION in tilegen_class_factories.cpp).
	virtual const char *GetName();
};

//-----------------------------------------------------------------------------
// Class factory type to produce tilegen ranges.
//
// TElementValue - the type of each element in the range, e.g. const CExit *
// TRangeClass - the specific type of the range, e.g.
//   CTilegenRange_NewOpenExits, CTilegenRange_ClosedExits
//
// TRangeClass must derive from ITilegenRange< TElementValue >
//-----------------------------------------------------------------------------
template< typename TElementValue, typename TRangeClass >
class CTilegenRangeClassFactory : public CTilegenClassFactory< ITilegenRange< TElementValue >, TRangeClass >
{
public:
	// Must be implemented for each specialization of CTilegenRangeClassFactory
	// (see macro IMPLEMENT_TILEGEN_RANGE in tilegen_class_factories.cpp).
	virtual const char *GetName();
};

//-----------------------------------------------------------------------------
// A class registry which is capable of instantiating any class
// which has a class factory instance registered with it.
// A list of class factories is tracked and searched by string when 
// an instance of a particular type is requested.
//
// TTilegenInterface - base interface class which is returned when a
//   new instance is created.
//-----------------------------------------------------------------------------
template< typename TTilegenInterface >
class CTilegenClassRegistry
{
	typedef ITilegenClassFactory< TTilegenInterface > TTilegenClassFactoryInterface;
public:
	// @TODO: do we need cleanup the CUtlVector for each instance of this class?  
	// If so, we're better off making implicit linked lists, since
	// each factory class is statically allocated..

	//-----------------------------------------------------------------------------
	// Registers a new factory instance which can produce a specific type of
	// tilegen class.
	//-----------------------------------------------------------------------------
	static void AddFactory( TTilegenClassFactoryInterface *pFactory )
	{
		m_FactoryList.AddToTail( pFactory );
	}

	//-----------------------------------------------------------------------------
	// Creates an instance of the class specified by pClassName
	//-----------------------------------------------------------------------------
	static TTilegenInterface *CreateInstance( const char *pClassName )
	{
		for ( int i = 0; i < m_FactoryList.Count(); ++ i )
		{
			if ( Q_stricmp( m_FactoryList[i]->GetName(), pClassName ) == 0 )
			{
				return m_FactoryList[i]->CreateInstance();
			}
		}
		Log_Warning( LOG_TilegenLayoutSystem, "Class %s not found in %s. Check the class name spelling and the context in which it is used.\n", pClassName, m_pFactoryName );
		return NULL;
	}

private:
	static CUtlVector< TTilegenClassFactoryInterface * > m_FactoryList;
	static const char *m_pFactoryName;
};

//-----------------------------------------------------------------------------
// Parses KeyValues data and instantiates the appropriate class
//
// bCreateEmptyInstance is used in the rare case where the caller needs
// the object created but not actually loaded from KeyValues.
// (One example of this is CTilegenExpression_MapReduce, which uses binary 
// operators to fold/reduce values together but does not need any state
// in the operators to function.
//-----------------------------------------------------------------------------
template< typename TTilegenInterface >
static TTilegenInterface *CreateFromKeyValues( KeyValues *pKeyValues, bool bCreateEmptyInstance = false )
{
	if ( pKeyValues->GetFirstSubKey() == NULL )
	{
		// Code to handle literal/inline values
		return ITilegenClassFactory< TTilegenInterface >::ReadLiteralValue( pKeyValues );
	}

	const char *pClassName = pKeyValues->GetString( "class", NULL );
	if ( pClassName == NULL )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "No class name specified for class instantiation in key values.\n" );
		return NULL;
	}

	TTilegenInterface *pNewClass = CTilegenClassRegistry< TTilegenInterface >::CreateInstance( pClassName );
	if ( pNewClass != NULL && !bCreateEmptyInstance && !pNewClass->LoadFromKeyValues( pKeyValues ) )
	{
		delete pNewClass;
		pNewClass = NULL;
	}
	return pNewClass;
}

//-----------------------------------------------------------------------------
// Creates the appropriate tilegen class from the specified sub-key
// of KeyValues data.
//-----------------------------------------------------------------------------
template< typename TTilegenClass >
bool CreateFromKeyValuesBlock( KeyValues *pParentKV, const char *pKeyName, const char *pParentClassName, TTilegenClass **ppClass, bool bOptional = false, bool bCreateEmptyInstance = false )
{
	KeyValues *pSubKey = pParentKV->FindKey( pKeyName );
	*ppClass = NULL;	
	if ( pSubKey == NULL )
	{
		if ( bOptional )
		{
			return true;
		}
		Log_Warning( LOG_TilegenLayoutSystem, "Could not load sub-key '%s' for parent class '%s'.\n", pKeyName, pParentClassName );
		return false;
	}

	*ppClass = CreateFromKeyValues< TTilegenClass >( pSubKey, bCreateEmptyInstance );
	if ( *ppClass == NULL )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Could not create class for sub-key '%s' for parent class '%s'.\n", pKeyName, pParentClassName );
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Creates the appropriate tilegen expression class from the specified sub-key
// of KeyValues data.
//-----------------------------------------------------------------------------
template< typename TExpressionValue >
bool CreateExpressionFromKeyValuesBlock( KeyValues *pParentKV, const char *pKeyName, const char *pParentClassName, ITilegenExpression< TExpressionValue > **ppClass, bool bOptional = false, bool bCreateEmptyInstance = false )
{
	return CreateFromKeyValuesBlock< ITilegenExpression< TExpressionValue > >( pParentKV, pKeyName, pParentClassName, ppClass, bOptional, bCreateEmptyInstance );
}

//-----------------------------------------------------------------------------
// Creates the appropriate tilegen action and boolean expression class
// (if present) from the specified KeyValues data.
//-----------------------------------------------------------------------------
bool CreateActionAndCondition( KeyValues *pKeyValues, ITilegenAction **ppAction, ITilegenExpression< bool > **ppCondition );

//-----------------------------------------------------------------------------
// Creates the appropriate tilegen action and boolean expression class
// (if present) from the specified sub-key of KeyValues data.
//-----------------------------------------------------------------------------
bool CreateActionAndConditionFromKeyValuesBlock( KeyValues *pParentKV, const char *pKeyName, const char *pParentClassName, ITilegenAction **ppAction, ITilegenExpression< bool > **ppCondition );

//-----------------------------------------------------------------------------
// Creates the appropriate tilegen range class from the specified sub-key
// of KeyValues data.
//-----------------------------------------------------------------------------
template< typename TElementValue >
bool CreateRangeFromKeyValuesBlock( KeyValues *pParentKV, const char *pKeyName, const char *pParentClassName, ITilegenRange< TElementValue > **ppClass )
{
	return CreateFromKeyValuesBlock< ITilegenRange< TElementValue > >( pParentKV, pKeyName, pParentClassName, ppClass );
}

#endif // TILEGEN_CLASS_FACTORIES_H