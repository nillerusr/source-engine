//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dmserializerbinary.h"
#include "datamodel/idatamodel.h"
#include "datamodel.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"
#include "dmattributeinternal.h"
#include "dmelementdictionary.h"
#include "tier1/utlbuffer.h"
#include "DmElementFramework.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CUtlBuffer;
class CBaseSceneObject;


//-----------------------------------------------------------------------------
// special element indices
//-----------------------------------------------------------------------------
enum
{
	ELEMENT_INDEX_NULL = -1,
	ELEMENT_INDEX_EXTERNAL = -2,
};


//-----------------------------------------------------------------------------
// Serialization class for Binary output
//-----------------------------------------------------------------------------
class CDmSerializerBinary : public IDmSerializer
{
public:
	CDmSerializerBinary() {}
	// Inherited from IDMSerializer
	virtual const char *GetName() const { return "binary"; }
	virtual const char *GetDescription() const { return "Binary"; }
	virtual bool StoresVersionInFile() const { return true; }
	virtual bool IsBinaryFormat() const { return true; }
	virtual int GetCurrentVersion() const { return 2; }
	virtual bool Serialize( CUtlBuffer &buf, CDmElement *pRoot );
	virtual bool Unserialize( CUtlBuffer &buf, const char *pEncodingName, int nEncodingVersion,
							  const char *pSourceFormatName, int nFormatVersion,
							  DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot );
    
private:
	// Methods related to serialization
	void SerializeElementIndex( CUtlBuffer& buf, CDmElementSerializationDictionary& list, DmElementHandle_t hElement, DmFileId_t fileid );
	void SerializeElementAttribute( CUtlBuffer& buf, CDmElementSerializationDictionary& list, CDmAttribute *pAttribute );
	void SerializeElementArrayAttribute( CUtlBuffer& buf, CDmElementSerializationDictionary& list, CDmAttribute *pAttribute );
	bool SerializeAttributes( CUtlBuffer& buf, CDmElementSerializationDictionary& list, unsigned short *symbolToIndexMap, CDmElement *pElement );
	bool SaveElementDict( CUtlBuffer& buf, unsigned short *symbolToIndexMap, CDmElement *pElement );
	bool SaveElement( CUtlBuffer& buf, CDmElementSerializationDictionary& dict, unsigned short *symbolToIndexMap, CDmElement *pElement);

	// Methods related to unserialization
	DmElementHandle_t UnserializeElementIndex( CUtlBuffer &buf, CUtlVector<CDmElement*> &elementList );
	void UnserializeElementAttribute( CUtlBuffer &buf, CDmAttribute *pAttribute, CUtlVector<CDmElement*> &elementList );
	void UnserializeElementArrayAttribute( CUtlBuffer &buf, CDmAttribute *pAttribute, CUtlVector<CDmElement*> &elementList );
	bool UnserializeAttributes( CUtlBuffer &buf, CDmElement *pElement, CUtlVector<CDmElement*> &elementList, UtlSymId_t *symbolTable );
	bool UnserializeElements( CUtlBuffer &buf, DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot, UtlSymId_t *symbolTable );
};
   

//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CDmSerializerBinary s_DMSerializerBinary;

void InstallBinarySerializer( IDataModel *pFactory )
{
	pFactory->AddSerializer( &s_DMSerializerBinary );
}


//-----------------------------------------------------------------------------
// Write out the index of the element to avoid looks at read time
//-----------------------------------------------------------------------------
void CDmSerializerBinary::SerializeElementIndex( CUtlBuffer& buf, CDmElementSerializationDictionary& list, DmElementHandle_t hElement, DmFileId_t fileid )
{
	if ( hElement == DMELEMENT_HANDLE_INVALID )
	{
		buf.PutInt( ELEMENT_INDEX_NULL ); // invalid handle
	}
	else
	{
		CDmElement *pElement = g_pDataModel->GetElement( hElement );
		if ( pElement )
		{
			if ( pElement->GetFileId() == fileid )
			{
				buf.PutInt( list.Find( pElement ) );
			}
			else
			{
				buf.PutInt( ELEMENT_INDEX_EXTERNAL );
				char idstr[ 40 ];
				UniqueIdToString( pElement->GetId(), idstr, sizeof( idstr ) );
				buf.PutString( idstr );
			}
		}
		else
		{
			DmObjectId_t *pId = NULL;
			DmElementReference_t *pRef = g_pDataModelImp->FindElementReference( hElement, &pId );
			if ( pRef && pId )
			{
				buf.PutInt( ELEMENT_INDEX_EXTERNAL );
				char idstr[ 40 ];
				UniqueIdToString( *pId, idstr, sizeof( idstr ) );
				buf.PutString( idstr );
			}
			else
			{
				buf.PutInt( ELEMENT_INDEX_NULL ); // invalid handle
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Writes out element attributes
//-----------------------------------------------------------------------------
void CDmSerializerBinary::SerializeElementAttribute( CUtlBuffer& buf, CDmElementSerializationDictionary& list, CDmAttribute *pAttribute )
{
	SerializeElementIndex( buf, list, pAttribute->GetValue< DmElementHandle_t >(), pAttribute->GetOwner()->GetFileId() );
}


//-----------------------------------------------------------------------------
// Writes out element array attributes
//-----------------------------------------------------------------------------
void CDmSerializerBinary::SerializeElementArrayAttribute( CUtlBuffer& buf, CDmElementSerializationDictionary& list, CDmAttribute *pAttribute )
{
	DmFileId_t fileid = pAttribute->GetOwner()->GetFileId();
	CDmrElementArray<> vec( pAttribute );

	int nCount = vec.Count();
	buf.PutInt( nCount );
	for ( int i = 0; i < nCount; ++i )
	{
		SerializeElementIndex( buf, list, vec.GetHandle(i), fileid );
	}
}


//-----------------------------------------------------------------------------
// Writes out all attributes
//-----------------------------------------------------------------------------
bool CDmSerializerBinary::SerializeAttributes( CUtlBuffer& buf, CDmElementSerializationDictionary& list, unsigned short *symbolToIndexMap, CDmElement *pElement )
{
	// Collect the attributes to be written
	CDmAttribute **ppAttributes = ( CDmAttribute** )_alloca( pElement->AttributeCount() * sizeof( CDmAttribute* ) );
	int nAttributes = 0;
	for ( CDmAttribute *pAttribute = pElement->FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
	{
		if ( pAttribute->IsFlagSet( FATTRIB_DONTSAVE | FATTRIB_STANDARD ) )
			continue;

		ppAttributes[ nAttributes++ ] = pAttribute;
	}

	// Now write them all out in reverse order, since FirstAttribute is actually the *last* attribute for perf reasons
	buf.PutInt( nAttributes );
	for ( int i = nAttributes - 1; i >= 0; --i )
	{
		CDmAttribute *pAttribute = ppAttributes[ i ];
		Assert( pAttribute );

		buf.PutShort( symbolToIndexMap[ pAttribute->GetNameSymbol() ] );
		buf.PutChar( pAttribute->GetType() );
		switch( pAttribute->GetType() )
		{
		default:
 			pAttribute->Serialize( buf );
			break;

		case AT_ELEMENT:
			SerializeElementAttribute( buf, list, pAttribute );
			break;

		case AT_ELEMENT_ARRAY:
			SerializeElementArrayAttribute( buf, list, pAttribute );
			break;
		}
	}

	return buf.IsValid();
}


bool CDmSerializerBinary::SaveElement( CUtlBuffer& buf, CDmElementSerializationDictionary& list, unsigned short *symbolToIndexMap, CDmElement *pElement )
{
	SerializeAttributes( buf, list, symbolToIndexMap, pElement );
	return buf.IsValid();
}

bool CDmSerializerBinary::SaveElementDict( CUtlBuffer& buf, unsigned short *symbolToIndexMap, CDmElement *pElement )
{
	buf.PutShort( symbolToIndexMap[ pElement->GetType() ] );
	buf.PutString( pElement->GetName() );
	buf.Put( &pElement->GetId(), sizeof(DmObjectId_t) );
	return buf.IsValid();
}

void MarkSymbol( UtlSymId_t *indexToSymbolMap, unsigned short *symbolToIndexMap, unsigned short &nSymbols, UtlSymId_t sym )
{
	if ( symbolToIndexMap[ sym ] != 0xffff )
		return;

	symbolToIndexMap[ sym ] = nSymbols;
	indexToSymbolMap[ nSymbols ] = sym;
	nSymbols++;
}

void MarkSymbols( UtlSymId_t *indexToSymbolMap, unsigned short *symbolToIndexMap, unsigned short &nSymbols, CDmElement *pElement )
{
	MarkSymbol( indexToSymbolMap, symbolToIndexMap, nSymbols, pElement->GetType() );
	for ( CDmAttribute *pAttr = pElement->FirstAttribute(); pAttr; pAttr = pAttr->NextAttribute() )
	{
		MarkSymbol( indexToSymbolMap, symbolToIndexMap, nSymbols, pAttr->GetNameSymbol() );
	}
}

bool CDmSerializerBinary::Serialize( CUtlBuffer &outBuf, CDmElement *pRoot )
{
	// Save elements, attribute links
	CDmElementSerializationDictionary dict;
	dict.BuildElementList( pRoot, true );

	// TODO - consider allowing dmxconvert to skip the collection step, since the only datamodel symbols will be the ones from the file

	unsigned short nTotalSymbols = g_pDataModelImp->GetSymbolCount();
	UtlSymId_t     *indexToSymbolMap = ( UtlSymId_t    * )stackalloc( nTotalSymbols * sizeof( UtlSymId_t ) );
	unsigned short *symbolToIndexMap = ( unsigned short* )stackalloc( nTotalSymbols * sizeof( unsigned short ) );
	V_memset( indexToSymbolMap, 0xff, nTotalSymbols * sizeof( UtlSymId_t ) );
	V_memset( symbolToIndexMap, 0xff, nTotalSymbols * sizeof( unsigned short ) );

	// collect list of attribute names and element types into string table
	unsigned short nUsedSymbols = 0;
	DmElementDictHandle_t i;
	for ( i = dict.FirstRootElement(); i != ELEMENT_DICT_HANDLE_INVALID; i = dict.NextRootElement(i) )
	{
		MarkSymbols( indexToSymbolMap, symbolToIndexMap, nUsedSymbols, dict.GetRootElement( i ) );
	}
	Assert( nUsedSymbols <= nTotalSymbols );

	// write out the symbol table for this file (may be significantly smaller than datamodel's full symbol table)
	outBuf.PutShort( nUsedSymbols );
	for ( int si = 0; si < nUsedSymbols; ++si )
	{
		UtlSymId_t sym = indexToSymbolMap[ si ];
		const char *pStr = g_pDataModel->GetString( sym );
		outBuf.PutString( pStr );
	}

	// First write out the dictionary of all elements (to avoid later stitching up in unserialize)
	outBuf.PutInt( dict.RootElementCount() );
	for ( i = dict.FirstRootElement(); i != ELEMENT_DICT_HANDLE_INVALID; i = dict.NextRootElement(i) )
	{
		SaveElementDict( outBuf, symbolToIndexMap, dict.GetRootElement( i ) );
	}

	// Now write out the attributes of each of those elements
	for ( i = dict.FirstRootElement(); i != ELEMENT_DICT_HANDLE_INVALID; i = dict.NextRootElement(i) )
	{
		SaveElement( outBuf, dict, symbolToIndexMap, dict.GetRootElement( i ) );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Reads an element index and converts it to a handle (local or external)
//-----------------------------------------------------------------------------
DmElementHandle_t CDmSerializerBinary::UnserializeElementIndex( CUtlBuffer &buf, CUtlVector<CDmElement*> &elementList )
{
	int nElementIndex = buf.GetInt();
	Assert( nElementIndex < elementList.Count() );
	if ( nElementIndex == ELEMENT_INDEX_EXTERNAL )
	{
		char idstr[ 40 ];
		buf.GetString( idstr );
		DmObjectId_t id;
		UniqueIdFromString( &id, idstr, sizeof( idstr ) );
		return g_pDataModelImp->FindOrCreateElementHandle( id );
	}

	Assert( nElementIndex >= 0 || nElementIndex == ELEMENT_INDEX_NULL );
	if ( nElementIndex < 0 || !elementList[ nElementIndex ] )
		return DMELEMENT_HANDLE_INVALID;

	return elementList[ nElementIndex ]->GetHandle();
}


//-----------------------------------------------------------------------------
// Reads an element attribute
//-----------------------------------------------------------------------------
void CDmSerializerBinary::UnserializeElementAttribute( CUtlBuffer &buf, CDmAttribute *pAttribute, CUtlVector<CDmElement*> &elementList )
{
	DmElementHandle_t hElement = UnserializeElementIndex( buf, elementList );
	if ( !pAttribute )
		return;

	pAttribute->SetValue( hElement );
}


//-----------------------------------------------------------------------------
// Reads an element array attribute
//-----------------------------------------------------------------------------
void CDmSerializerBinary::UnserializeElementArrayAttribute( CUtlBuffer &buf, CDmAttribute *pAttribute, CUtlVector<CDmElement*> &elementList )
{
	int nElementCount = buf.GetInt();

	if ( !pAttribute || pAttribute->GetType() != AT_ELEMENT_ARRAY )
	{
		// Parse past the data
		for ( int i = 0; i < nElementCount; ++i )
		{
			UnserializeElementIndex( buf, elementList );
		}
		return;
	}

	CDmrElementArray<> array( pAttribute );
	array.RemoveAll();
	array.EnsureCapacity( nElementCount );
	for ( int i = 0; i < nElementCount; ++i )
	{
		DmElementHandle_t hElement = UnserializeElementIndex( buf, elementList );
		array.AddToTail( hElement );
	}
}


//-----------------------------------------------------------------------------
// Reads a single element
//-----------------------------------------------------------------------------
bool CDmSerializerBinary::UnserializeAttributes( CUtlBuffer &buf, CDmElement *pElement, CUtlVector<CDmElement*> &elementList, UtlSymId_t *symbolTable )
{
	char nameBuf[ 1024 ];

	int nAttributeCount = buf.GetInt();
	for ( int i = 0; i < nAttributeCount; ++i )
	{
		const char *pName = NULL;
		if ( symbolTable )
		{
			unsigned short nName = buf.GetShort();
			pName = g_pDataModel->GetString( symbolTable[ nName ] );
		}
		else
		{
			buf.GetString( nameBuf );
			pName = nameBuf;
		}
		DmAttributeType_t nAttributeType = (DmAttributeType_t)buf.GetChar();

		Assert( pName != NULL && pName[ 0 ] != '\0' );
		Assert( nAttributeType != AT_UNKNOWN );

		CDmAttribute *pAttribute = pElement ? pElement->AddAttribute( pName, nAttributeType ) : NULL;
		if ( pElement && !pAttribute )
		{
			Warning("Dm: Attempted to read an attribute (\"%s\") of an inappropriate type!\n", pName );
			return false;
		}

		switch( nAttributeType )
		{
		default:
			if ( !pAttribute )
			{
				SkipUnserialize( buf, nAttributeType );
			}
			else
			{
				pAttribute->Unserialize( buf );
			}
			break;

		case AT_ELEMENT:
			UnserializeElementAttribute( buf, pAttribute, elementList );
			break;

		case AT_ELEMENT_ARRAY:
			UnserializeElementArrayAttribute( buf, pAttribute, elementList );
			break;
		}
	}

	return buf.IsValid();
}

struct DmIdPair_t
{
	DmObjectId_t m_oldId;
	DmObjectId_t m_newId;
	DmIdPair_t &operator=( const DmIdPair_t &that )
	{
		CopyUniqueId( that.m_oldId, &m_oldId );
		CopyUniqueId( that.m_newId, &m_newId );
		return *this;
	}
	static unsigned int HashKey( const DmIdPair_t& that )
	{
		return *( unsigned int* )&that.m_oldId.m_Value;
	}
	static bool Compare( const DmIdPair_t& a, const DmIdPair_t& b )
	{
		return IsUniqueIdEqual( a.m_oldId, b.m_oldId );
	}
};

DmElementHandle_t CreateElementWithFallback( const char *pType, const char *pName, DmFileId_t fileid, const DmObjectId_t &id )
{
	DmElementHandle_t hElement = g_pDataModel->CreateElement( pType, pName, fileid, &id );
	if ( hElement == DMELEMENT_HANDLE_INVALID )
	{
		Warning("Binary: Element uses unknown element type %s\n", pType );
		hElement = g_pDataModel->CreateElement( "DmElement", pName, fileid, &id );
		Assert( hElement != DMELEMENT_HANDLE_INVALID );
	}
	return hElement;
}

//-----------------------------------------------------------------------------
// Main entry point for the unserialization
//-----------------------------------------------------------------------------
bool CDmSerializerBinary::Unserialize( CUtlBuffer &buf, const char *pEncodingName, int nEncodingVersion,
									   const char *pSourceFormatName, int nSourceFormatVersion,
									   DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot )
{
	Assert( !V_stricmp( pEncodingName, GetName() ) );
	if ( V_stricmp( pEncodingName, GetName() ) != 0 )
		return false;

	Assert( nEncodingVersion >= 0 && nEncodingVersion <= 2 );
	if ( nEncodingVersion < 0 || nEncodingVersion > 2 )
		return false;

	bool bReadSymbolTable = nEncodingVersion >= 2;

	// Read string table
	unsigned short nStrings = 0;
	UtlSymId_t *symbolTable = NULL;
	if ( bReadSymbolTable )
	{
		char stringBuf[ 256 ];

		nStrings = buf.GetShort();
		symbolTable = ( UtlSymId_t* )stackalloc( nStrings * sizeof( UtlSymId_t ) );
		for ( int i = 0; i < nStrings; ++i )
		{
			buf.GetString( stringBuf );
			symbolTable[ i ] = g_pDataModel->GetSymbol( stringBuf );
		}
	}

	bool bSuccess = UnserializeElements( buf, fileid, idConflictResolution, ppRoot, symbolTable );
	if ( !bSuccess )
		return false;

	return g_pDataModel->UpdateUnserializedElements( pSourceFormatName, nSourceFormatVersion, fileid, idConflictResolution, ppRoot );
}

bool CDmSerializerBinary::UnserializeElements( CUtlBuffer &buf, DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot, UtlSymId_t *symbolTable )
{
	*ppRoot = NULL;

	// Read in the element count.
	int nElementCount = buf.GetInt();
	if ( !nElementCount )
		return true;

	int nMaxIdConflicts = min( nElementCount, g_pDataModel->GetAllocatedElementCount() );
	int nExpectedIdCopyConflicts = ( idConflictResolution == CR_FORCE_COPY || idConflictResolution == CR_COPY_NEW ) ? nMaxIdConflicts : 0;
	int nBuckets = min( 0x10000, max( 16, nExpectedIdCopyConflicts / 16 ) ); // CUtlHash can only address up to 65k buckets
	CUtlHash< DmIdPair_t > idmap( nBuckets, 0, 0, DmIdPair_t::Compare, DmIdPair_t::HashKey );

	// Read + create all elements
	CUtlVector<CDmElement*> elementList( 0, nElementCount );
	for ( int i = 0; i < nElementCount; ++i )
	{
		char pName[2048];
		DmObjectId_t id;

		char typeBuf[ 256 ];
		const char *pType = NULL;
		if ( symbolTable )
		{
			unsigned short nType = buf.GetShort();
			pType = g_pDataModel->GetString( symbolTable[ nType ] );
		}
		else
		{
			buf.GetString( typeBuf );
			pType = typeBuf;
		}

		buf.GetString( pName );
		buf.Get( &id, sizeof(DmObjectId_t) );

		if ( idConflictResolution == CR_FORCE_COPY )
		{
			DmIdPair_t idpair;
			CopyUniqueId( id, &idpair.m_oldId );
			CreateUniqueId( &idpair.m_newId );
			idmap.Insert( idpair );

			CopyUniqueId( idpair.m_newId, &id );
		}

		DmElementHandle_t hElement = DMELEMENT_HANDLE_INVALID;
		DmElementHandle_t hExistingElement = g_pDataModel->FindElement( id );
		if ( hExistingElement != DMELEMENT_HANDLE_INVALID )
		{
			// id is already in use - need to resolve conflict

			if ( idConflictResolution == CR_DELETE_NEW )
			{
				elementList.AddToTail( g_pDataModel->GetElement( hExistingElement ) );
				continue; // just don't create this element
			}
			else if ( idConflictResolution == CR_DELETE_OLD )
			{
				g_pDataModelImp->DeleteElement( hExistingElement, HR_NEVER ); // keep the handle around until CreateElementWithFallback
				hElement = CreateElementWithFallback( pType, pName, fileid, id );
				Assert( hElement == hExistingElement );
			}
			else if ( idConflictResolution == CR_COPY_NEW )
			{
				DmIdPair_t idpair;
				CopyUniqueId( id, &idpair.m_oldId );
				CreateUniqueId( &idpair.m_newId );
				idmap.Insert( idpair );

				hElement = CreateElementWithFallback( pType, pName, fileid, idpair.m_newId );
			}
			else
				Assert( 0 );
		}

		// if not found, then create it
		if ( hElement == DMELEMENT_HANDLE_INVALID )
		{
			hElement = CreateElementWithFallback( pType, pName, fileid, id );
		}

		CDmElement *pElement = g_pDataModel->GetElement( hElement );
		CDmeElementAccessor::MarkBeingUnserialized( pElement, true );
		elementList.AddToTail( pElement );
	}

	// The root is the 0th element
	*ppRoot = elementList[ 0 ];

	// Now read all attributes
	for ( int i = 0; i < nElementCount; ++i )
	{
		CDmElement *pInternal = elementList[ i ];
		UnserializeAttributes( buf, pInternal->GetFileId() == fileid ? pInternal : NULL, elementList, symbolTable );
	}

	for ( int i = 0; i < nElementCount; ++i )
	{
		CDmElement *pElement = elementList[ i ];
		if ( pElement->GetFileId() == fileid )
		{
			// mark all unserialized elements as done unserializing, and call Resolve()
			CDmeElementAccessor::MarkBeingUnserialized( pElement, false );
		}
	}

	g_pDmElementFrameworkImp->RemoveCleanElementsFromDirtyList( );
	return buf.IsValid();
}
