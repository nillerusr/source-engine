//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "toolutils/attributeelementchoicelist.h"
#include "datamodel/dmelement.h"

	
typedef CUtlRBTree< CDmElement *, int > ElementDict_t;


//-----------------------------------------------------------------------------
// returns the choice string that AddElementsRecursively would have returned
//-----------------------------------------------------------------------------
const char *GetChoiceString( CDmElement *pElement )
{
	return pElement->GetName();
}

//-----------------------------------------------------------------------------
// Recursively adds all elements referred to this element into the list of elements
//-----------------------------------------------------------------------------
void AddElementsRecursively_R( CDmElement *pElement, ElementChoiceList_t &list, ElementDict_t &dict, const char *pElementType )
{
	if ( !pElement )
		return;

	if ( dict.Find( pElement ) != dict.InvalidIndex() )
		return;

	dict.Insert( pElement );

	if ( pElement->IsA( pElementType ) )
	{
		int nIndex = list.AddToTail( );
		ElementChoice_t &entry = list[nIndex];
		entry.m_pValue = pElement;
		entry.m_pChoiceString = GetChoiceString( pElement );
	}

	for ( CDmAttribute *pAttribute = pElement->FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
	{
		char const *attributeName = pAttribute->GetName( );
		DmAttributeType_t attrType = pAttribute->GetType( );
		if ( attrType == AT_ELEMENT )
		{
			CDmElement *pChild = pElement->GetValueElement< CDmElement >( attributeName );
			AddElementsRecursively_R( pChild, list, dict, pElementType );
		}
		else if ( attrType == AT_ELEMENT_ARRAY )
		{
			const CDmrElementArray<CDmElement> children( pElement, attributeName );
			uint n = children.Count();
			for ( uint i = 0; i < n; ++i )
			{
				CDmElement *pChild = children[ i ];
				AddElementsRecursively_R( pChild, list, dict, pElementType );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Recursively adds all elements referred to this element into the list of elements
//-----------------------------------------------------------------------------
void AddElementsRecursively_R( CDmElement *pElement, DmeHandleVec_t &list, ElementDict_t &dict, const char *pElementType )
{
	if ( !pElement )
		return;

	if ( dict.Find( pElement ) != dict.InvalidIndex() )
		return;

	dict.Insert( pElement );

	if ( pElement->IsA( pElementType ) )
	{
		int nIndex = list.AddToTail( );
		list[nIndex] = pElement;
	}

	for ( CDmAttribute *pAttribute = pElement->FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
	{
		char const *attributeName = pAttribute->GetName( );
		DmAttributeType_t attrType = pAttribute->GetType( );
		if ( attrType == AT_ELEMENT )
		{
			CDmElement *pChild = pElement->GetValueElement< CDmElement >( attributeName );
			AddElementsRecursively_R( pChild, list, dict, pElementType );
		}
		else if ( attrType == AT_ELEMENT_ARRAY )
		{
			const CDmrElementArray<CDmElement> children( pElement, attributeName );
			uint n = children.Count();
			for ( uint i = 0; i < n; ++i )
			{
				CDmElement *pChild = children[ i ];
				AddElementsRecursively_R( pChild, list, dict, pElementType );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Recursively adds all elements referred to this element into the list of elements
//-----------------------------------------------------------------------------
void AddElementsRecursively( CDmElement *obj, ElementChoiceList_t &list, const char *pElementType )
{
	if ( !pElementType )
	{
		pElementType = g_pDataModel->GetString( CDmElement::GetStaticTypeSymbol() );
	}

	ElementDict_t dict( 0, 0, DefLessFunc( CDmElement * ) );
	AddElementsRecursively_R( obj, list, dict, pElementType );
}


//-----------------------------------------------------------------------------
// Recursively adds all elements of the specified type under pElement into the vector
//-----------------------------------------------------------------------------
void AddElementsRecursively( CDmElement *pElement, DmeHandleVec_t &list, const char *pElementType )
{
	if ( !pElementType )
	{
		pElementType = g_pDataModel->GetString( CDmElement::GetStaticTypeSymbol() );
	}

	ElementDict_t dict( 0, 0, DefLessFunc( CDmElement * ) );
	AddElementsRecursively_R( pElement, list, dict, pElementType );
}
