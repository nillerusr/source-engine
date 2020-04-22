//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef INOTIFYUI_H
#define INOTIFYUI_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"
#include "datamodel/idatamodel.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;


//-----------------------------------------------------------------------------
// Interface used to allow tools to deal with dynamic choice lists
//-----------------------------------------------------------------------------
struct IntChoice_t
{
	int m_nValue;
	const char *m_pChoiceString;
};

struct StringChoice_t
{
	const char *m_pValue;
	const char *m_pChoiceString;
};

struct ElementChoice_t
{
	ElementChoice_t() {}
	ElementChoice_t( CDmElement *pValue, const char *pChoiceString = NULL ) : m_pValue( pValue ), m_pChoiceString( pChoiceString ) {}
	CDmElement *m_pValue;
	const char *m_pChoiceString;
};

typedef CUtlVector<IntChoice_t> IntChoiceList_t;
typedef CUtlVector<StringChoice_t> StringChoiceList_t;
typedef CUtlVector<ElementChoice_t> ElementChoiceList_t;


//-----------------------------------------------------------------------------
// Interface used to call back out of the element properties panels
// to communicate with external systems
//-----------------------------------------------------------------------------
enum DmeControlsNotifySource_t
{
	NOTIFY_SOURCE_PROPERTIES_TREE = NOTIFY_SOURCE_FIRST_DME_CONTROL_SOURCE,
	NOTIFY_SOURCE_FILE_LIST_MANAGER,
	NOTIFY_SOURCE_PRESET_GROUP_EDITOR,
};


//-----------------------------------------------------------------------------
// Utility scope guards
//-----------------------------------------------------------------------------
DEFINE_SOURCE_UNDO_SCOPE_GUARD( ElementTree, NOTIFY_SOURCE_PROPERTIES_TREE );
DEFINE_SOURCE_NOTIFY_SCOPE_GUARD( ElementTree, NOTIFY_SOURCE_PROPERTIES_TREE );


class IElementPropertiesChoices
{
public:
	// For boolean choice lists. Return false if it's an unknown choice list type
	// Element, attribute specifies the attribute we're editing. 
	// bArray element is true if the attribute is an array attribute and we're editing one of its elements
	virtual bool GetBoolChoiceList( const char *pChoiceListType, CDmElement *pElement, 
		const char *pAttributeName, bool bArrayElement, const char *pChoiceStrings[2] ) = 0;

	// For integer choice lists. Return false if it's an unknown choice list type
	virtual bool GetIntChoiceList( const char *pChoiceListType, CDmElement *pElement, 
		const char *pAttributeName, bool bArrayElement, IntChoiceList_t &list ) = 0;

	// For string choice lists. Return false if it's an unknown choice list type
	virtual bool GetStringChoiceList( const char *pChoiceListType, CDmElement *pElement, 
		const char *pAttributeName, bool bArrayElement, StringChoiceList_t &list ) = 0;

	// For element choice lists. Return false if it's an unknown choice list type
	virtual bool GetElementChoiceList( const char *pChoiceListType, CDmElement *pElement, 
		const char *pAttributeName, bool bArrayElement, ElementChoiceList_t &list ) = 0;

	virtual const char *GetElementChoiceString( const char *pChoiceListType, CDmElement *pElement, 
		const char *pAttributeName, bool bArrayElement, CDmElement *pValue ) = 0;
};


//-----------------------------------------------------------------------------
// Default implementation of IElementPropertiesChoices
//-----------------------------------------------------------------------------
class CBaseElementPropertiesChoices : public IElementPropertiesChoices
{
public:
	virtual bool GetBoolChoiceList( const char *pChoiceListType, CDmElement *pElement, 
		const char *pAttributeName, bool bArrayElement, const char *pChoiceStrings[2] )
	{
		return false;
	}

	virtual bool GetIntChoiceList( const char *pChoiceListType, CDmElement *pElement, 
		const char *pAttributeName, bool bArrayElement, IntChoiceList_t &list )
	{
		return false;
	}

	virtual bool GetStringChoiceList( const char *pChoiceListType, CDmElement *pElement, 
		const char *pAttributeName, bool bArrayElement, StringChoiceList_t &list )
	{
		return false;
	}

	virtual bool GetElementChoiceList( const char *pChoiceListType, CDmElement *pElement, 
		const char *pAttributeName, bool bArrayElement, ElementChoiceList_t &list )
	{
		return false;
	}
	virtual const char *GetElementChoiceString( const char *pChoiceListType, CDmElement *pElement, 
		const char *pAttributeName, bool bArrayElement, CDmElement *pValue )
	{
		return NULL;
	}
};

#endif // INOTIFYUI_H