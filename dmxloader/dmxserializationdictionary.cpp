//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dmxserializationdictionary.h"
#include "dmxloader/dmxelement.h"
#include "dmxloader/dmxattribute.h"


//-----------------------------------------------------------------------------
//
// Element dictionary used in serialization
//
//-----------------------------------------------------------------------------
CDmxSerializationDictionary::CDmxSerializationDictionary( int nElementsHint /* = 0 */ ) :
	m_Dict( 0, nElementsHint, CDmxSerializationDictionary::LessFunc )
{
}


//-----------------------------------------------------------------------------
// Used to sort the list of elements
//-----------------------------------------------------------------------------
bool CDmxSerializationDictionary::LessFunc( const DmxElementInfo_t &lhs, const DmxElementInfo_t &rhs )
{
	return lhs.m_pElement < rhs.m_pElement;
}


//-----------------------------------------------------------------------------
// Finds the handle of the element
//-----------------------------------------------------------------------------
DmxSerializationHandle_t CDmxSerializationDictionary::Find( CDmxElement *pElement )
{
	DmxElementInfo_t find;
	find.m_pElement = pElement;
	return m_Dict.Find( find );
}

	
//-----------------------------------------------------------------------------
// Creates the list of all things to serialize
//-----------------------------------------------------------------------------
void CDmxSerializationDictionary::BuildElementList_R( CDmxElement *pElement, bool bFlatMode, bool bIsRoot )
{
	if ( !pElement )
		return;

	// FIXME: Right here we should ask the element if it's an external
	// file reference and exit immediately if so.

	// This means we've already encountered this guy.
	// Therefore, he can never be a root element
	DmxSerializationHandle_t h = Find( pElement );
	if ( h != m_Dict.InvalidIndex() )
	{
		m_Dict[h].m_bRoot = true;
		return;
	}

	DmxElementInfo_t info;
	info.m_bRoot = bFlatMode || bIsRoot;
	info.m_pElement = pElement;
	m_Dict.Insert( info );

	int nCount = pElement->AttributeCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxAttribute *pAttribute = pElement->GetAttribute(i);
		switch( pAttribute->GetType() )
		{
		case AT_ELEMENT:
			{
				CDmxElement *pChild = pAttribute->GetValue<CDmxElement*>();
				if ( !pChild )
					break;

				BuildElementList_R( pChild, bFlatMode, false );
			}
			break;

		case AT_ELEMENT_ARRAY:
			{
				const CUtlVector<CDmxElement*> &array = pAttribute->GetArray<CDmxElement*>( );
				int nCountArray = array.Count();
				for ( int j = 0; j < nCountArray; ++j )
				{
					CDmxElement *pChild = array[ j ];
					if ( !pChild )
						break;

					BuildElementList_R( pChild, bFlatMode, false );
				}
			}
			break;
		}
	}
}

void CDmxSerializationDictionary::BuildElementList( CDmxElement *pElement, bool bFlatMode )
{
	BuildElementList_R( pElement, bFlatMode, true );
}


//-----------------------------------------------------------------------------
// Should I inline the serialization of this element?
//-----------------------------------------------------------------------------
bool CDmxSerializationDictionary::ShouldInlineElement( CDmxElement *pElement )
{
	// This means we've already encountered this guy.
	// Therefore, he can never be a root element
	DmxSerializationHandle_t h = Find( pElement );
	if ( h != m_Dict.InvalidIndex() )
		return !m_Dict[h].m_bRoot;

	// If we didn't find the element, it means it's a reference to an external
	// element (or it's NULL), so don't inline ie.
	return false;
}


//-----------------------------------------------------------------------------
// Clears the dictionary
//-----------------------------------------------------------------------------
void CDmxSerializationDictionary::Clear()
{
	m_Dict.RemoveAll();
}


//-----------------------------------------------------------------------------
// How many root elements do we have?
//-----------------------------------------------------------------------------
int CDmxSerializationDictionary::RootElementCount() const
{
	int nCount = 0;
	DmxSerializationHandle_t h = m_Dict.FirstInorder();
	while( h != m_Dict.InvalidIndex() )
	{
		if ( m_Dict[h].m_bRoot )
		{
			++nCount;
		}
		h = m_Dict.NextInorder( h );
	}
	return nCount;
}

	
//-----------------------------------------------------------------------------
// Iterates over all root elements to serialize
//-----------------------------------------------------------------------------
DmxSerializationHandle_t CDmxSerializationDictionary::FirstRootElement() const
{
	// NOTE: I don't have to use First/NextInorder here because there
	// are guaranteed to be no removals from the dictionary.
	// Also, using inorder traversal won't get my actual root element to be first in the file
	int nCount = m_Dict.Count();
	for ( DmxSerializationHandle_t h = 0; h < nCount; ++h )
	{
		if ( m_Dict[h].m_bRoot )
			return h;
	}
	return DMX_SERIALIZATION_HANDLE_INVALID;
}

DmxSerializationHandle_t CDmxSerializationDictionary::NextRootElement( DmxSerializationHandle_t h ) const
{
	++h;
	int nCount = m_Dict.Count();
	for ( ; h < nCount; ++h )
	{
		if ( m_Dict[h].m_bRoot )
			return h;
	}
	return DMX_SERIALIZATION_HANDLE_INVALID;
}

CDmxElement *CDmxSerializationDictionary::GetRootElement( DmxSerializationHandle_t h )
{
	Assert( m_Dict[h].m_bRoot );
	return m_Dict[h].m_pElement;
}


