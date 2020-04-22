//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dmelementdictionary.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmattributevar.h"
#include "datamodel/idatamodel.h"
#include "datamodel.h"


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CDmElementDictionary::CDmElementDictionary()
	: m_idmap( 1024, 0, 0, DmIdPair_t::Compare, DmIdPair_t::HashKey )
{
}


//-----------------------------------------------------------------------------
// Clears the dictionary
//-----------------------------------------------------------------------------
void CDmElementDictionary::Clear()
{
	m_Dict.Purge();
	m_Attributes.Purge();
	m_ArrayAttributes.Purge();
	m_elementsToDelete.Purge();
}

	
//-----------------------------------------------------------------------------
// Inserts an element into the table
//-----------------------------------------------------------------------------
DmElementDictHandle_t CDmElementDictionary::InsertElement( CDmElement *pElement )
{
	// Insert it into the reconnection table
	return m_Dict.AddToTail( pElement ? pElement->GetHandle() : DMELEMENT_HANDLE_INVALID );
}


//-----------------------------------------------------------------------------
// Returns a particular element
//-----------------------------------------------------------------------------
CDmElement *CDmElementDictionary::GetElement( DmElementDictHandle_t handle )
{
	if ( handle == ELEMENT_DICT_HANDLE_INVALID )
		return NULL;

	return g_pDataModel->GetElement( m_Dict[ handle ] );
}


//-----------------------------------------------------------------------------
// Adds an attribute to the fixup list
//-----------------------------------------------------------------------------
void CDmElementDictionary::AddAttribute( CDmAttribute *pAttribute, const DmObjectId_t &objectId )
{
	if ( m_elementsToDelete.Find( pAttribute->GetOwner()->GetHandle() ) != m_elementsToDelete.InvalidIndex() )
		return; // don't add attributes if their element is being deleted

	int i = m_Attributes.AddToTail();
	m_Attributes[i].m_nType = AT_OBJECTID;
	m_Attributes[i].m_pAttribute = pAttribute;
	CopyUniqueId( objectId, &m_Attributes[i].m_ObjectId );
}


//-----------------------------------------------------------------------------
// Adds an element of an attribute array to the fixup list
//-----------------------------------------------------------------------------
void CDmElementDictionary::AddArrayAttribute( CDmAttribute *pAttribute, DmElementDictHandle_t hElement )
{
	if ( m_elementsToDelete.Find( pAttribute->GetOwner()->GetHandle() ) != m_elementsToDelete.InvalidIndex() )
		return; // don't add attributes if their element is being deleted

	int i = m_ArrayAttributes.AddToTail();
	m_ArrayAttributes[i].m_nType = AT_ELEMENT;
	m_ArrayAttributes[i].m_pAttribute = pAttribute;
	m_ArrayAttributes[i].m_hElement = hElement;
}

void CDmElementDictionary::AddArrayAttribute( CDmAttribute *pAttribute, const DmObjectId_t &objectId )
{
	if ( m_elementsToDelete.Find( pAttribute->GetOwner()->GetHandle() ) != m_elementsToDelete.InvalidIndex() )
		return; // don't add attributes if their element is being deleted

	int i = m_ArrayAttributes.AddToTail();
	m_ArrayAttributes[i].m_nType = AT_OBJECTID;
	m_ArrayAttributes[i].m_pAttribute = pAttribute;
	CopyUniqueId( objectId, &m_ArrayAttributes[i].m_ObjectId );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CDmElementDictionary::RemoveAttributeInfosOfElement( AttributeList_t &attributes, DmElementHandle_t hElement )
{
	while ( attributes.Count() > 0 && attributes.Tail().m_pAttribute->GetOwner()->GetHandle() == hElement )
	{
		attributes.Remove( attributes.Count() - 1 );
	}
}

DmElementHandle_t CDmElementDictionary::SetElementId( DmElementDictHandle_t hDictHandle, const DmObjectId_t &newId, DmConflictResolution_t idConflictResolution )
{
	DmElementHandle_t hElement = m_Dict[ hDictHandle ];
	CDmElement *pElement = g_pDataModel->GetElement( hElement );
	Assert( pElement );
	if ( !pElement )
		return DMELEMENT_HANDLE_INVALID;

	const DmObjectId_t &oldId = pElement->GetId();

	if ( idConflictResolution == CR_FORCE_COPY )
	{
		m_idmap.Insert( DmIdPair_t( newId, oldId ) ); // map the newId back to the old id, and keep the old id
		return hElement;
	}

	DmElementHandle_t newHandle = g_pDataModelImp->ChangeElementId( hElement, oldId, newId );
	if ( newHandle != DMELEMENT_HANDLE_INVALID )
	{
		// if ChangeElementId returns a handle, the id has been changed
		if ( newHandle != hElement )
		{
			int i = m_Dict.Find( hElement );
			if ( i != m_Dict.InvalidIndex() )
			{
				m_Dict[ i ] = newHandle;
			}
		}
		return newHandle; // either keeping the old handle, with the new id, or found a new handle associated with that new id
	}

	// id not changed because that id is already in use
	if ( idConflictResolution == CR_DELETE_NEW )
	{
		DmElementHandle_t hExistingElement = g_pDataModel->FindElement( newId );

		int i = m_elementsToDelete.AddToTail( );
		m_elementsToDelete[i].m_hDictHandle = hDictHandle;
		m_elementsToDelete[i].m_hElementToDelete = hElement;
		m_elementsToDelete[i].m_hReplacementElement = hExistingElement;

		// remove all element ref attributes read in before the id (typically none)
		RemoveAttributeInfosOfElement( m_Attributes, hElement );
		RemoveAttributeInfosOfElement( m_ArrayAttributes, hElement );

		return DMELEMENT_HANDLE_INVALID;
	}

	if ( idConflictResolution == CR_DELETE_OLD )
	{
		DmElementHandle_t hExistingElement = g_pDataModel->FindElement( newId );
		Assert( hExistingElement != DMELEMENT_HANDLE_INVALID );
		if ( hExistingElement == DMELEMENT_HANDLE_INVALID )
			return DMELEMENT_HANDLE_INVALID; // unexpected error in ChangeElementId (failed due to something other than a conflict)

		g_pDataModelImp->DeleteElement( hExistingElement, HR_NEVER ); // need to keep the handle around until ChangeElemendId
		newHandle = g_pDataModelImp->ChangeElementId( hElement, oldId, newId );
		Assert( newHandle == hExistingElement );

		int i = m_Dict.Find( hElement );
		if ( i != m_Dict.InvalidIndex() )
		{
			m_Dict[ i ] = newHandle;
		}

		return newHandle;
	}

	if ( idConflictResolution == CR_COPY_NEW )
	{
		m_idmap.Insert( DmIdPair_t( newId, oldId ) ); // map the newId back to the old id, and keep the old id
		return hElement;
	}

	Assert( 0 );
	return DMELEMENT_HANDLE_INVALID;
}


//-----------------------------------------------------------------------------
// Finds an element into the table
//-----------------------------------------------------------------------------
DmElementDictHandle_t CDmElementDictionary::FindElement( CDmElement *pElement )
{
	return m_Dict.Find( pElement ? pElement->GetHandle() : DMELEMENT_HANDLE_INVALID );
}


//-----------------------------------------------------------------------------
// Hook up all element references (which were unserialized as object ids)
//-----------------------------------------------------------------------------
void CDmElementDictionary::HookUpElementAttributes()
{
	int n = m_Attributes.Count();
	for ( int i = 0; i < n; ++i )
	{
		Assert( m_Attributes[i].m_pAttribute->GetType() == AT_ELEMENT );
		Assert( m_Attributes[i].m_nType == AT_OBJECTID );

		UtlHashHandle_t h = m_idmap.Find( DmIdPair_t( m_Attributes[i].m_ObjectId ) );
		DmObjectId_t &id = h == m_idmap.InvalidHandle() ? m_Attributes[i].m_ObjectId : m_idmap[ h ].m_newId;

		// search id->handle table (both loaded and unloaded) for id, and if not found, create a new handle, map it to the id and return it
		DmElementHandle_t hElement = g_pDataModelImp->FindOrCreateElementHandle( id );
		m_Attributes[i].m_pAttribute->SetValue<DmElementHandle_t>( hElement );
	}
}


//-----------------------------------------------------------------------------
// Hook up all element array references
//-----------------------------------------------------------------------------
void CDmElementDictionary::HookUpElementArrayAttributes()
{
	// Find unique array attributes; we need to clear them all before adding stuff.
	// This clears them of stuff added during their construction phase.
	int n = m_ArrayAttributes.Count();
	CUtlRBTree< CDmAttribute*, unsigned short >	lookup( 0, n, DefLessFunc(CDmAttribute*) );
	for ( int i = 0; i < n; ++i )
	{
		Assert( m_ArrayAttributes[i].m_pAttribute->GetType() == AT_ELEMENT_ARRAY );
		CDmAttribute *pElementArray = m_ArrayAttributes[i].m_pAttribute;
		CDmrElementArray<> array( pElementArray );
		if ( lookup.Find( pElementArray ) == lookup.InvalidIndex() )
		{
			array.RemoveAll();
			lookup.Insert( pElementArray );
		}
	}

	for ( int i = 0; i < n; ++i )
	{
		Assert( m_ArrayAttributes[i].m_pAttribute->GetType() == AT_ELEMENT_ARRAY );

		CDmrElementArray<> array( m_ArrayAttributes[i].m_pAttribute );

		if ( m_ArrayAttributes[i].m_nType == AT_ELEMENT )
		{
			CDmElement *pElement = GetElement( m_ArrayAttributes[i].m_hElement );
			array.AddToTail( pElement );
		}
		else
		{
			UtlHashHandle_t h = m_idmap.Find( DmIdPair_t( m_ArrayAttributes[i].m_ObjectId ) );
			DmObjectId_t &id = ( h == m_idmap.InvalidHandle() ) ? m_ArrayAttributes[i].m_ObjectId : m_idmap[ h ].m_newId;

			// search id->handle table (both loaded and unloaded) for id, and if not found, create a new handle, map it to the id and return it
			DmElementHandle_t hElement = g_pDataModelImp->FindOrCreateElementHandle( id );
			int nIndex = array.AddToTail();
			array.SetHandle( nIndex, hElement );
		}
	}
}


//-----------------------------------------------------------------------------
// Hook up all element references (which were unserialized as object ids)
//-----------------------------------------------------------------------------
void CDmElementDictionary::HookUpElementReferences()
{
	int nElementsToDelete = m_elementsToDelete.Count();
	for ( int i = 0; i < nElementsToDelete; ++i )
	{
		DmElementDictHandle_t hDictIndex = m_elementsToDelete[i].m_hDictHandle;
		DmElementHandle_t hElement = m_Dict[ hDictIndex ];
		g_pDataModelImp->DeleteElement( hElement );
		m_Dict[ hDictIndex ] = m_elementsToDelete[i].m_hReplacementElement;
	}

	HookUpElementArrayAttributes();
	HookUpElementAttributes();
}



//-----------------------------------------------------------------------------
//
// Element dictionary used in serialization
//
//-----------------------------------------------------------------------------
CDmElementSerializationDictionary::CDmElementSerializationDictionary() :
	m_Dict( 1024, 0, CDmElementSerializationDictionary::LessFunc )
{
}


//-----------------------------------------------------------------------------
// Used to sort the list of elements
//-----------------------------------------------------------------------------
bool CDmElementSerializationDictionary::LessFunc( const ElementInfo_t &lhs, const ElementInfo_t &rhs )
{
	return lhs.m_pElement < rhs.m_pElement;
}


//-----------------------------------------------------------------------------
// Finds the handle of the element
//-----------------------------------------------------------------------------
DmElementDictHandle_t CDmElementSerializationDictionary::Find( CDmElement *pElement )
{
	ElementInfo_t find;
	find.m_pElement = pElement;
	return m_Dict.Find( find );
}

	
//-----------------------------------------------------------------------------
// Creates the list of all things to serialize
//-----------------------------------------------------------------------------
void CDmElementSerializationDictionary::BuildElementList_R( CDmElement *pElement, bool bFlatMode, bool bIsRoot )
{
	if ( !pElement )
		return;

	// FIXME: Right here we should ask the element if it's an external
	// file reference and exit immediately if so.

	// This means we've already encountered this guy.
	// Therefore, he can never be a root element
	DmElementDictHandle_t h = Find( pElement );
	if ( h != m_Dict.InvalidIndex() )
	{
		m_Dict[h].m_bRoot = true;
		return;
	}

	ElementInfo_t info;
	info.m_bRoot = bFlatMode || bIsRoot;
	info.m_pElement = pElement;
	m_Dict.Insert( info );

	for ( CDmAttribute *pAttribute = pElement->FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
	{
		if ( pAttribute->IsFlagSet( FATTRIB_DONTSAVE ) )
			continue;

		switch( pAttribute->GetType() )
		{
		case AT_ELEMENT:
			{
				CDmElement *pChild = pAttribute->GetValueElement<CDmElement>();
				if ( !pChild || pChild->GetFileId() != pElement->GetFileId() )
					break;

				BuildElementList_R( pChild, bFlatMode, false );
			}
			break;

		case AT_ELEMENT_ARRAY:
			{
				CDmrElementArray<> array( pAttribute );
				int nCount = array.Count();
				for ( int i = 0; i < nCount; ++i )
				{
					CDmElement *pChild = array[i];
					if ( !pChild || pChild->GetFileId() != pElement->GetFileId() )
						break;

					BuildElementList_R( pChild, bFlatMode, false );
				}
			}
			break;
		}
	}
}

void CDmElementSerializationDictionary::BuildElementList( CDmElement *pElement, bool bFlatMode )
{
	BuildElementList_R( pElement, bFlatMode, true );
}


//-----------------------------------------------------------------------------
// Should I inline the serialization of this element?
//-----------------------------------------------------------------------------
bool CDmElementSerializationDictionary::ShouldInlineElement( CDmElement *pElement )
{
	// This means we've already encountered this guy.
	// Therefore, he can never be a root element
	DmElementDictHandle_t h = Find( pElement );
	if ( h != m_Dict.InvalidIndex() )
		return !m_Dict[h].m_bRoot;

	// If we didn't find the element, it means it's a reference to an external
	// element (or it's NULL), so don't inline ie.
	return false;
}


//-----------------------------------------------------------------------------
// Clears the dictionary
//-----------------------------------------------------------------------------
void CDmElementSerializationDictionary::Clear()
{
	m_Dict.RemoveAll();
}


//-----------------------------------------------------------------------------
// How many root elements do we have?
//-----------------------------------------------------------------------------
int CDmElementSerializationDictionary::RootElementCount() const
{
	int nCount = 0;
	DmElementDictHandle_t h = m_Dict.FirstInorder();
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
DmElementDictHandle_t CDmElementSerializationDictionary::FirstRootElement() const
{
	// NOTE - this code only works with BlockMemory or Memory (NOT FixedMemory)

	// NOTE: I don't have to use First/NextInorder here because there
	// are guaranteed to be no removals from the dictionary.
	// Also, using inorder traversal won't get my actual root element to be first in the file
	int nCount = m_Dict.Count();
	for ( DmElementDictHandle_t h = 0; h < nCount; ++h )
	{
		if ( m_Dict[h].m_bRoot )
			return h;
	}
	return ELEMENT_DICT_HANDLE_INVALID;
}

DmElementDictHandle_t CDmElementSerializationDictionary::NextRootElement( DmElementDictHandle_t h ) const
{
	// NOTE - this code only works with BlockMemory or Memory (NOT FixedMemory)

	// NOTE: I don't have to use First/NextInorder here because there
	// are guaranteed to be no removals from the dictionary.
	// Also, using inorder traversal won't get my actual root element to be first in the file
	++h;
	int nCount = m_Dict.Count();
	for ( ; h < nCount; ++h )
	{
		if ( m_Dict[h].m_bRoot )
			return h;
	}
	return ELEMENT_DICT_HANDLE_INVALID;
}

CDmElement *CDmElementSerializationDictionary::GetRootElement( DmElementDictHandle_t h )
{
	Assert( m_Dict[h].m_bRoot );
	return m_Dict[h].m_pElement;
}


