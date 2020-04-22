//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dmxloader/dmxelement.h"
#include "dmxloader/dmxattribute.h"
#include "tier1/utlbuffer.h"
#include "mathlib/ssemath.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------
CUtlSymbolTableMT CDmxElement::s_TypeSymbols;


//-----------------------------------------------------------------------------
// Creates a dmx element
//-----------------------------------------------------------------------------
CDmxElement* CreateDmxElement( const char *pType )
{
	return new CDmxElement( pType );
}


//-----------------------------------------------------------------------------
// Constructor, destructor 
//-----------------------------------------------------------------------------
CDmxElement::CDmxElement( const char *pType )
{
	m_Type = s_TypeSymbols.AddString( pType );
	m_nLockCount = 0;
	m_bResortNeeded = false;
	m_bIsMarkedForDeletion = false;
	CreateUniqueId( &m_Id );
}

CDmxElement::~CDmxElement()
{
	CDmxElementModifyScope modify( this );
	RemoveAllAttributes();
}


//-----------------------------------------------------------------------------
// Utility method for getting at the type
//-----------------------------------------------------------------------------
CUtlSymbol CDmxElement::GetType()  const
{
	return m_Type;
}

const char* CDmxElement::GetTypeString() const
{
	return s_TypeSymbols.String( m_Type );
}

const char* CDmxElement::GetName() const
{
	return GetValue< CUtlString >( "name" );
}

const DmObjectId_t &CDmxElement::GetId() const
{
	return m_Id;
}


//-----------------------------------------------------------------------------
// Sets the object id
//-----------------------------------------------------------------------------
void CDmxElement::SetId( const DmObjectId_t &id )
{
	CopyUniqueId( id, &m_Id );
}


//-----------------------------------------------------------------------------
// Sorts the vector when a change has occurred
//-----------------------------------------------------------------------------
void CDmxElement::Resort( )	const
{
	if ( m_bResortNeeded )
	{
		AttributeList_t *pAttributes = const_cast< AttributeList_t *>( &m_Attributes );
		pAttributes->RedoSort();
		m_bResortNeeded = false;

		// NOTE: This checks for duplicate attribute names
		int nCount = m_Attributes.Count();
		for ( int i = nCount; --i >= 1; )
		{
			if ( m_Attributes[i]->GetNameSymbol() == m_Attributes[i-1]->GetNameSymbol() )
			{
				Warning( "Duplicate attribute name %s encountered!\n", m_Attributes[i]->GetName() );
				pAttributes->Remove(i);
				Assert( 0 );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Enables modification of the DmxElement
//-----------------------------------------------------------------------------
void CDmxElement::LockForChanges( bool bLock )
{
	if ( bLock )
	{
		++m_nLockCount;
	}
	else
	{
		if ( --m_nLockCount == 0 )
		{
			Resort();
		}
		Assert( m_nLockCount >= 0 );
	}
}


//-----------------------------------------------------------------------------
// Adds, removes attributes
//-----------------------------------------------------------------------------
CDmxAttribute *CDmxElement::AddAttribute( const char *pAttributeName )
{
	int nIndex = FindAttribute( pAttributeName );
	if ( nIndex >= 0 )
		return m_Attributes[nIndex];

	CDmxElementModifyScope modify( this );
	m_bResortNeeded = true;
	CDmxAttribute *pAttribute = new CDmxAttribute( pAttributeName );
	m_Attributes.InsertNoSort( pAttribute );
	return pAttribute;
}

void CDmxElement::RemoveAttribute( const char *pAttributeName )
{
	CDmxElementModifyScope modify( this );
	int idx = FindAttribute( pAttributeName );
	if ( idx >= 0 )
	{
		delete m_Attributes[idx];
		m_Attributes.Remove( idx );
	}
}

void CDmxElement::RemoveAttributeByPtr( CDmxAttribute *pAttribute )
{	
	CDmxElementModifyScope modify( this );
	int nCount = m_Attributes.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_Attributes[i] != pAttribute )
			continue;

		delete pAttribute;
		m_Attributes.Remove( i );
		break;
	}
}

void CDmxElement::RemoveAllAttributes()
{
	int nCount = m_Attributes.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		delete m_Attributes[i];
	}
	m_Attributes.RemoveAll();
	m_bResortNeeded = false;
}


//-----------------------------------------------------------------------------
// Rename an attribute
//-----------------------------------------------------------------------------
void CDmxElement::RenameAttribute( const char *pAttributeName, const char *pNewName )
{
	CDmxElementModifyScope modify( this );

	// No change...
	if ( !Q_stricmp( pAttributeName, pNewName ) )
		return;

	int idx = FindAttribute( pAttributeName );
	if ( idx < 0 )
		return;

	if ( HasAttribute( pNewName ) )
	{
		Warning( "Tried to rename from \"%s\" to \"%s\", but \"%s\" already exists!\n",
			pAttributeName, pNewName, pNewName );
		return;
	}

	m_bResortNeeded = true;
	m_Attributes[ idx ]->SetName( pNewName );
}


//-----------------------------------------------------------------------------
// Find an attribute by name-based lookup
//-----------------------------------------------------------------------------
int CDmxElement::FindAttribute( const char *pAttributeName ) const
{
	// FIXME: The cost here is O(log M) + O(log N)
	// where log N is the binary search for the symbol match
	// and log M is the binary search for the attribute name->symbol
	// We can eliminate log M by using a hash table in the symbol lookup
	Resort();
	CDmxAttribute search( pAttributeName );
	return m_Attributes.Find( &search );
}


//-----------------------------------------------------------------------------
// Find an attribute by name-based lookup
//-----------------------------------------------------------------------------
int CDmxElement::FindAttribute( CUtlSymbol attributeName ) const
{
	Resort();
	CDmxAttribute search( attributeName );
	return m_Attributes.Find( &search );
}


//-----------------------------------------------------------------------------
// Attribute finding
//-----------------------------------------------------------------------------
bool CDmxElement::HasAttribute( const char *pAttributeName ) const
{
	int idx = FindAttribute( pAttributeName );
	return ( idx >= 0 );
}

CDmxAttribute *CDmxElement::GetAttribute( const char *pAttributeName )
{
	int idx = FindAttribute( pAttributeName );
	if ( idx >= 0 )
		return m_Attributes[ idx ];
	return NULL;
}

const CDmxAttribute *CDmxElement::GetAttribute( const char *pAttributeName ) const
{
	int idx = FindAttribute( pAttributeName );
	if ( idx >= 0 )
		return m_Attributes[ idx ];
	return NULL;
}


//-----------------------------------------------------------------------------
// Attribute interation
//-----------------------------------------------------------------------------
int CDmxElement::AttributeCount() const
{
	return m_Attributes.Count();
}

CDmxAttribute *CDmxElement::GetAttribute( int nIndex )
{
	return m_Attributes[ nIndex ];
}

const CDmxAttribute *CDmxElement::GetAttribute( int nIndex ) const
{
	return m_Attributes[ nIndex ];
}


//-----------------------------------------------------------------------------
// Removes all elements recursively
//-----------------------------------------------------------------------------
void CDmxElement::AddElementsToDelete( CUtlVector< CDmxElement * >& elementsToDelete )
{
	if ( m_bIsMarkedForDeletion )
		return;

	m_bIsMarkedForDeletion = true;
	elementsToDelete.AddToTail( this );

	int nCount = AttributeCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxAttribute *pAttribute = GetAttribute(i);
		if ( pAttribute->GetType() == AT_ELEMENT )
		{
			CDmxElement *pElement = pAttribute->GetValue< CDmxElement* >();
			if ( pElement )
			{
				pElement->AddElementsToDelete( elementsToDelete );
			}
			continue;
		}

		if ( pAttribute->GetType() == AT_ELEMENT_ARRAY )
		{
			const CUtlVector< CDmxElement * > &elements = pAttribute->GetArray< CDmxElement* >();
			int nElementCount = elements.Count();
			for ( int j = 0; j < nElementCount; ++j )
			{
				if ( elements[j] )
				{
					elements[j]->AddElementsToDelete( elementsToDelete );
				}
			}
			continue;
		}
	}
}


//-----------------------------------------------------------------------------
// Removes all elements recursively
//-----------------------------------------------------------------------------
void CDmxElement::RemoveAllElementsRecursive()
{
	CUtlVector< CDmxElement * > elementsToDelete; 
	AddElementsToDelete( elementsToDelete );
	int nCount = elementsToDelete.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		delete elementsToDelete[i];
	}
}


//-----------------------------------------------------------------------------
// Method to unpack data into a structure
//-----------------------------------------------------------------------------
void CDmxElement::UnpackIntoStructure( void *pData, size_t DestSizeInBytes, const DmxElementUnpackStructure_t *pUnpack ) const
{
	void *pDataEnd = ( char * )pData + DestSizeInBytes;

	for ( ; pUnpack->m_AttributeType != AT_UNKNOWN; ++pUnpack )
	{
		char *pDest = (char*)pData + pUnpack->m_nOffset;

		// NOTE: This does not work with array data at the moment
		if ( IsArrayType( pUnpack->m_AttributeType ) )
		{
			AssertMsg( 0, ( "CDmxElement::UnpackIntoStructure: Array attribute types not currently supported!\n" ) );
			continue;
		}

		if ( pUnpack->m_AttributeType == AT_VOID )
		{
			AssertMsg( 0, ( "CDmxElement::UnpackIntoStructure: Binary blob attribute types not currently supported!\n" ) );
			continue;
		}

		CDmxAttribute temp( NULL );
		const CDmxAttribute *pAttribute = GetAttribute( pUnpack->m_pAttributeName );
		if ( !pAttribute )
		{
			if ( !pUnpack->m_pDefaultString )
				continue;

			// Convert the default string into the target
			int nLen = Q_strlen( pUnpack->m_pDefaultString );
			if ( nLen > 0 )
			{
				CUtlBuffer buf( pUnpack->m_pDefaultString, nLen, CUtlBuffer::READ_ONLY | CUtlBuffer::TEXT_BUFFER );
				temp.Unserialize( pUnpack->m_AttributeType, buf );
			}
			else
			{
				CUtlBuffer buf;
				temp.Unserialize( pUnpack->m_AttributeType, buf );				
			}
			pAttribute = &temp;
		}

		if ( pUnpack->m_AttributeType != pAttribute->GetType() ) 
		{
			Warning( "CDmxElement::UnpackIntoStructure: Mismatched attribute type in attribute \"%s\"!\n", pUnpack->m_pAttributeName );
			continue;
		}

		if ( pAttribute->GetType() == AT_STRING )
		{
			if ( pDest + pUnpack->m_nSize > pDataEnd )
			{
				Warning( "ERROR Memory corruption: CDmxElement::UnpackIntoStructure string buffer overrun!\n" );
				continue;
			}

			// Strings get special treatment: they are stored as in-line arrays of chars
			Q_strncpy( pDest, pAttribute->GetValueString(), pUnpack->m_nSize );
			continue;
		}

		// special case - if data type is float, but dest size == 16, we are unpacking into simd by
		// replication
		if ( ( pAttribute->GetType() == AT_FLOAT ) && ( pUnpack->m_nSize == sizeof( fltx4 ) ) )
		{
			if ( pDest + 4 * sizeof( float ) > pDataEnd )
			{
				Warning( "ERROR Memory corruption: CDmxElement::UnpackIntoStructure float buffer overrun!\n" );
				continue;
			}

			memcpy( pDest + 0 * sizeof( float ) , pAttribute->m_pData, sizeof( float ) );
			memcpy( pDest + 1 * sizeof( float ) , pAttribute->m_pData, sizeof( float ) );
			memcpy( pDest + 2 * sizeof( float ) , pAttribute->m_pData, sizeof( float ) );
			memcpy( pDest + 3 * sizeof( float ) , pAttribute->m_pData, sizeof( float ) );
		}
		else
		{
			if ( pDest + pUnpack->m_nSize > pDataEnd )
			{
				Warning( "ERROR Memory corruption: CDmxElement::UnpackIntoStructure memcpy buffer overrun!\n" );
				continue;
			}

			AssertMsg( pUnpack->m_nSize == CDmxAttribute::AttributeDataSize( pAttribute->GetType() ), 
					   "CDmxElement::UnpackIntoStructure: Incorrect size to unpack data into in attribute \"%s\"!\n", pUnpack->m_pAttributeName );
			memcpy( pDest, pAttribute->m_pData, pUnpack->m_nSize );
		}
	}
}


//-----------------------------------------------------------------------------
// Creates attributes based on the unpack structure
//-----------------------------------------------------------------------------
void CDmxElement::AddAttributesFromStructure_Internal( const void *pData, size_t byteCount, const DmxElementUnpackStructure_t *pUnpack )
{
	for ( ; pUnpack->m_AttributeType != AT_UNKNOWN; ++pUnpack )
	{
		const char *pSrc = (const char*)pData + pUnpack->m_nOffset;

		// NOTE: This does not work with array data at the moment
		if ( IsArrayType( pUnpack->m_AttributeType ) )
		{
			AssertMsg( 0, "CDmxElement::AddAttributesFromStructure: Array attribute types not currently supported!\n" );
			continue;
		}

		if ( pUnpack->m_AttributeType == AT_VOID )
		{
			AssertMsg( 0, "CDmxElement::AddAttributesFromStructure: Binary blob attribute types not currently supported!\n" );
			continue;
		}

		if ( HasAttribute( pUnpack->m_pAttributeName ) )
		{
			AssertMsg( 0, "CDmxElement::AddAttributesFromStructure: Attribute %s already exists!\n", pUnpack->m_pAttributeName );
			continue;
		}

		{
			if ( (size_t)(pUnpack->m_nOffset + pUnpack->m_nSize) > byteCount )
			{
				Msg( "Buffer underread! Mismatched type/type-descriptor.\n" );
			}
			CDmxElementModifyScope modify( this );
			CDmxAttribute *pAttribute = AddAttribute( pUnpack->m_pAttributeName );
			if ( pUnpack->m_AttributeType == AT_STRING )
			{
				pAttribute->SetValue( pSrc );
			}
			else
			{
				int nSize = pUnpack->m_nSize;

				// handle float attrs stored as replicated fltx4's
				if ( ( pUnpack->m_AttributeType == AT_FLOAT ) && ( nSize == sizeof( fltx4 ) ) )
				{
					nSize = sizeof( float );
				}

				AssertMsg( nSize == CDmxAttribute::AttributeDataSize( pUnpack->m_AttributeType ), 
						   "CDmxElement::UnpackIntoStructure: Incorrect size to unpack data into in attribute \"%s\"!\n", pUnpack->m_pAttributeName );
				pAttribute->SetValue( pUnpack->m_AttributeType, pSrc, nSize );
			}
		}
	}
}
