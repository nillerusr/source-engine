//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dmxloader/dmxelement.h"
#include "tier1/utlbuffer.h"
#include "tier2/tier2.h"
#include "tier2/utlstreambuffer.h"
#include "filesystem.h"
#include "datamodel/idatamodel.h"	// for the file format #defines
#include "dmxserializationdictionary.h"
#include "tier1/memstack.h"


//-----------------------------------------------------------------------------
// DMX elements/attributes can only be accessed inside a dmx context
//-----------------------------------------------------------------------------
static int s_bInDMXContext;
CMemoryStack s_DMXAllocator;
static bool s_bAllocatorInitialized;

void BeginDMXContext( )
{
	Assert( !s_bInDMXContext );

	if ( !s_bAllocatorInitialized )
	{
		s_DMXAllocator.Init( 2 * 1024 * 1024, 0, 0, 4 );
		s_bAllocatorInitialized = true;
	}

	s_bInDMXContext = true;
}

void EndDMXContext( bool bDecommitMemory )
{
	Assert( s_bInDMXContext );
	s_bInDMXContext = false;
	s_DMXAllocator.FreeAll( bDecommitMemory );
}

void DecommitDMXMemory()
{
	s_DMXAllocator.FreeAll( true );
}


//-----------------------------------------------------------------------------
// Used for allocation. All will be freed when we leave the DMX context
//-----------------------------------------------------------------------------
void* DMXAlloc( size_t size )
{
	Assert( s_bInDMXContext );
	if ( !s_bInDMXContext )
		return 0;
	return s_DMXAllocator.Alloc( size, false );
}


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
bool UnserializeTextDMX( const char *pFileName, CUtlBuffer &buf, CDmxElement **ppRoot );
bool SerializeTextDMX( const char *pFileName, CUtlBuffer &buf, CDmxElement *pRoot );


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
class CDmxSerializer
{
public:
	bool Unserialize( CUtlBuffer &buf, int nEncodingVersion, CDmxElement **ppRoot );
	bool Serialize( CUtlBuffer &buf, CDmxElement *pRoot, const char *pFileName );

private:
	// Methods related to serialization
	bool ShouldWriteAttribute( const char *pAttributeName, CDmxAttribute *pAttribute );
	void SerializeElementIndex( CUtlBuffer& buf, CDmxSerializationDictionary& list, CDmxElement *pElement );
	void SerializeElementAttribute( CUtlBuffer& buf, CDmxSerializationDictionary& list, CDmxAttribute *pAttribute );
	void SerializeElementArrayAttribute( CUtlBuffer& buf, CDmxSerializationDictionary& list, CDmxAttribute *pAttribute );
	bool SaveElementDict( CUtlBuffer& buf, CUtlRBTree< const char* > &stringTable, CDmxElement *pElement );
	bool SaveElement( CUtlBuffer& buf, CDmxSerializationDictionary& dict, CUtlRBTree< const char* > &stringTable, CDmxElement *pElement);

	// Methods related to unserialization
	CDmxElement* UnserializeElementIndex( CUtlBuffer &buf, CUtlVector<CDmxElement*> &elementList );
	void UnserializeElementAttribute( CUtlBuffer &buf, CDmxAttribute *pAttribute, CUtlVector<CDmxElement*> &elementList );
	void UnserializeElementArrayAttribute( CUtlBuffer &buf, CDmxAttribute *pAttribute, CUtlVector<CDmxElement*> &elementList );
	bool UnserializeAttributes( CUtlBuffer &buf, CDmxElement *pElement, CUtlVector<CDmxElement*> &elementList, int nStrings, int *offsetTable, char *stringTable );
	int GetStringOffsetTable( CUtlBuffer &buf, int *offsetTable, int nStrings );
};



//-----------------------------------------------------------------------------
// Should we write out the attribute?
//-----------------------------------------------------------------------------
bool CDmxSerializer::ShouldWriteAttribute( const char *pAttributeName, CDmxAttribute *pAttribute )
{
	if ( !pAttribute )
		return false;

	// These are already written in the initial element dictionary
	if ( !Q_stricmp( pAttributeName, "name" ) )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Write out the index of the element to avoid looks at read time
//-----------------------------------------------------------------------------
void CDmxSerializer::SerializeElementIndex( CUtlBuffer& buf, CDmxSerializationDictionary& list, CDmxElement *pElement )
{
	if ( !pElement )
	{
		buf.PutInt( ELEMENT_INDEX_NULL ); // invalid handle
		return;
	}

	buf.PutInt( list.Find( pElement ) );
}


//-----------------------------------------------------------------------------
// Writes out element attributes
//-----------------------------------------------------------------------------
void CDmxSerializer::SerializeElementAttribute( CUtlBuffer& buf, CDmxSerializationDictionary& list, CDmxAttribute *pAttribute )
{
	SerializeElementIndex( buf, list, pAttribute->GetValue<CDmxElement*>() );
}


//-----------------------------------------------------------------------------
// Writes out element array attributes
//-----------------------------------------------------------------------------
void CDmxSerializer::SerializeElementArrayAttribute( CUtlBuffer& buf, CDmxSerializationDictionary& list, CDmxAttribute *pAttribute )
{
	const CUtlVector<CDmxElement*> &vec = pAttribute->GetArray<CDmxElement*>();

	int nCount = vec.Count();
	buf.PutInt( nCount );
	for ( int i = 0; i < nCount; ++i )
	{
		SerializeElementIndex( buf, list, vec[ i ] );
	}
}


//-----------------------------------------------------------------------------
// Writes out all attributes
//-----------------------------------------------------------------------------
bool CDmxSerializer::SaveElement( CUtlBuffer& buf, CDmxSerializationDictionary& list, CUtlRBTree< const char* > &stringTable, CDmxElement *pElement )
{
	int nAttributesToSave = 0;

	// Count the attributes...
	int nCount = pElement->AttributeCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxAttribute *pAttribute = pElement->GetAttribute( i );
		const char *pName = pAttribute->GetName( );
		if ( !ShouldWriteAttribute( pName, pAttribute ) )
			continue;

		++nAttributesToSave;
	}

	// Now write them all out.
	buf.PutInt( nAttributesToSave );
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxAttribute *pAttribute = pElement->GetAttribute( i );
		const char *pName = pAttribute->GetName();
		if ( !ShouldWriteAttribute( pName, pAttribute ) )
			continue;

		unsigned short sym = stringTable.Find( pName );
		if ( sym == stringTable.InvalidIndex() )
			return false;

		buf.PutShort( sym );
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

bool CDmxSerializer::SaveElementDict( CUtlBuffer& buf, CUtlRBTree< const char* > &stringTable, CDmxElement *pElement )
{
	unsigned short sym = stringTable.Find( pElement->GetTypeString() );
	if ( sym == stringTable.InvalidIndex() )
		return false;

	buf.PutShort( sym );
	buf.PutString( pElement->GetName() );
	buf.Put( &pElement->GetId(), sizeof(DmObjectId_t) );
	return buf.IsValid();
}

//-----------------------------------------------------------------------------
// Main entry point for serialization
//-----------------------------------------------------------------------------
bool CDmxSerializer::Serialize( CUtlBuffer &buf, CDmxElement *pRoot, const char *pFileName )
{
	// Save elements, attribute links
	CDmxSerializationDictionary dict;
	dict.BuildElementList( pRoot, true );

	// collect list of attribute names and element types into string table
	CUtlRBTree< const char* > stringTable( CaselessStringLessThan );
	DmxSerializationHandle_t i;
	for ( i = dict.FirstRootElement(); i != DMX_SERIALIZATION_HANDLE_INVALID; i = dict.NextRootElement(i) )
	{
		CDmxElement *pElement = dict.GetRootElement( i );
		if ( !pElement )
			return false;
		stringTable.InsertIfNotFound( pElement->GetTypeString() );
		int nAttributes = pElement->AttributeCount();
		for ( int ai = 0; ai < nAttributes; ++ai )
		{
			CDmxAttribute *pAttr = pElement->GetAttribute( ai );
			if ( !pAttr )
				return false;
			stringTable.InsertIfNotFound( pAttr->GetName() );
		}
	}

	// write out the string table
	int nStrings = stringTable.Count();
	if ( nStrings > 65535 )
		return false;
	buf.PutShort( nStrings );
	for ( int si = 0; si < nStrings; ++si )
	{
		buf.PutString( stringTable[ si ] );
	}

	// First write out the dictionary of all elements (to avoid later stitching up in unserialize)
	buf.PutInt( dict.RootElementCount() );
	for ( i = dict.FirstRootElement(); i != DMX_SERIALIZATION_HANDLE_INVALID; i = dict.NextRootElement(i) )
	{
		if ( !SaveElementDict( buf, stringTable, dict.GetRootElement( i ) ) )
			return false;
	}

	// Now write out the attributes of each of those elements
	for ( i = dict.FirstRootElement(); i != DMX_SERIALIZATION_HANDLE_INVALID; i = dict.NextRootElement(i) )
	{
		if ( !SaveElement( buf, dict, stringTable, dict.GetRootElement( i ) ) )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Reads an element index and converts it to a handle (local or external)
//-----------------------------------------------------------------------------
CDmxElement* CDmxSerializer::UnserializeElementIndex( CUtlBuffer &buf, CUtlVector<CDmxElement*> &elementList )
{
	int nElementIndex = buf.GetInt();
	if ( nElementIndex == ELEMENT_INDEX_EXTERNAL )
	{
		Warning( "Reading externally referenced elements is not supported!\n" );
		return NULL;
	}

	Assert( nElementIndex < elementList.Count() );
	Assert( nElementIndex >= 0 || nElementIndex == ELEMENT_INDEX_NULL );
	if ( nElementIndex < 0 || !elementList[ nElementIndex ] )
		return NULL;

	return elementList[ nElementIndex ];
}


//-----------------------------------------------------------------------------
// Reads an element attribute
//-----------------------------------------------------------------------------
void CDmxSerializer::UnserializeElementAttribute( CUtlBuffer &buf, CDmxAttribute *pAttribute, CUtlVector<CDmxElement*> &elementList )
{
	CDmxElement *pElement = UnserializeElementIndex( buf, elementList );
	pAttribute->SetValue( pElement );
}


//-----------------------------------------------------------------------------
// Reads an element array attribute
//-----------------------------------------------------------------------------
void CDmxSerializer::UnserializeElementArrayAttribute( CUtlBuffer &buf, CDmxAttribute *pAttribute, CUtlVector<CDmxElement*> &elementList )
{
	int nElementCount = buf.GetInt();
	CUtlVector< CDmxElement* >& elementArray = pAttribute->GetArrayForEdit< CDmxElement* >();
	elementArray.EnsureCapacity( nElementCount );

	for ( int i = 0; i < nElementCount; ++i )
	{
		CDmxElement *pElement = UnserializeElementIndex( buf, elementList );
		elementArray.AddToTail( pElement );
	}
}


//-----------------------------------------------------------------------------
// Reads a single element
//-----------------------------------------------------------------------------
bool CDmxSerializer::UnserializeAttributes( CUtlBuffer &buf, CDmxElement *pElement, CUtlVector<CDmxElement*> &elementList, int nStrings, int *offsetTable, char *stringTable )
{
	CDmxElementModifyScope modify( pElement );

	char nameBuf[ 1024 ];
	int nAttributeCount = buf.GetInt();
	for ( int i = 0; i < nAttributeCount; ++i )
	{
		const char *pName = NULL;
		if ( stringTable )
		{
			int si = buf.GetShort();
			if ( si >= nStrings )
				return false;
			pName = stringTable + offsetTable[ si ];
		}
		else
		{
			buf.GetString( nameBuf );
			pName = nameBuf;
		}
		DmAttributeType_t nAttributeType = (DmAttributeType_t)buf.GetChar();

		CDmxAttribute *pAttribute = pElement->AddAttribute( pName );
		if ( !pAttribute )
			return false;

		switch( nAttributeType )
		{
		default:
			pAttribute->Unserialize( nAttributeType, buf );
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


int CDmxSerializer::GetStringOffsetTable( CUtlBuffer &buf, int *offsetTable, int nStrings )
{
	int nBytes = buf.GetBytesRemaining();
	char *pBegin = ( char* )buf.PeekGet();
	char *pBytes = pBegin;
	for ( int i = 0; i < nStrings; ++i )
	{
		offsetTable[ i ] = pBytes - pBegin;

		do
		{
			// grow/shift utlbuffer if it hasn't loaded the entire string table into memory
			if ( pBytes - pBegin >= nBytes )
			{
				pBegin = ( char* )buf.PeekGet( nBytes + 1, 0 );
				if ( !pBegin )
					return false;
				pBytes = pBegin + nBytes;
				nBytes = buf.GetBytesRemaining();
			}
		}
		while ( *pBytes++ );
	}

	return pBytes - pBegin;
}

//-----------------------------------------------------------------------------
// Main entry point for the unserialization
//-----------------------------------------------------------------------------
bool CDmxSerializer::Unserialize( CUtlBuffer &buf, int nEncodingVersion, CDmxElement **ppRoot )
{
	if ( nEncodingVersion < 0 || nEncodingVersion > 2 )
		return false;

	bool bReadStringTable = nEncodingVersion >= 2;

	// Keep reading until we read a NULL terminator
	while( buf.GetChar() != 0 )
	{
		if ( !buf.IsValid() )
			return false;
	}

	// Read string table
	int nStrings = 0;
	int *offsetTable = NULL;
	char *stringTable = NULL;
	if ( bReadStringTable )
	{
		nStrings = buf.GetShort();
		if ( nStrings > 0 )
		{
			offsetTable = ( int* )stackalloc( nStrings * sizeof( int ) );

			// this causes entire string table to be mapped in memory at once
			int nStringMemoryUsage = GetStringOffsetTable( buf, offsetTable, nStrings );
			stringTable = ( char* )stackalloc( nStringMemoryUsage * sizeof( char ) );
			buf.Get( stringTable, nStringMemoryUsage );
		}
	}

	// Read in the element count.
	int nElementCount = buf.GetInt();
	if ( !nElementCount )
	{
		// Empty (but valid) file
		return true;
	}

	if ( nElementCount < 0 || ( bReadStringTable && !stringTable ) )
	{
		// Invalid file. Non-empty files with a string table need at least one to associate with elements.
		return false;
	}

	char pTypeBuf[256];
	char pName[2048];
	DmObjectId_t id;

	// Read + create all elements
	CUtlVector<CDmxElement*> elementList( 0, nElementCount );
	for ( int i = 0; i < nElementCount; ++i )
	{
		const char *pType = NULL;
		if ( stringTable )
		{
			int si = buf.GetShort();
			if ( si >= nStrings )
				return false;
			pType = stringTable + offsetTable[ si ];
		}
		else
		{
			buf.GetString( pTypeBuf );
			pType = pTypeBuf;
		}
		buf.GetString( pName );
		buf.Get( &id, sizeof(DmObjectId_t) );

		CDmxElement *pElement = new CDmxElement( pType );
		{
			CDmxElementModifyScope modify( pElement );
			CDmxAttribute *pAttribute = pElement->AddAttribute( "name" );
			pAttribute->SetValue( (char const *) pName );
			pElement->SetId( id );
		}
		elementList.AddToTail( pElement );
	}

	// The root is the 0th element
	*ppRoot = elementList[ 0 ];

	// Now read all attributes
	for ( int i = 0; i < nElementCount; ++i )
	{
		UnserializeAttributes( buf, elementList[ i ], elementList, nStrings, offsetTable, stringTable );
	}

	return buf.IsValid();
}


//-----------------------------------------------------------------------------
// Serialization main entry point
//-----------------------------------------------------------------------------
bool SerializeDMX( CUtlBuffer &buf, CDmxElement *pRoot, const char *pFileName )
{
	// Write the format name into the file using XML format so that 
	// 3rd-party XML readers can read the file without fail
	const char *pEncodingName = buf.IsText() ? "keyvalues2" : "binary";
	int nEncodingVersion = buf.IsText() ? 1 : 2; // HACK - we should have some way of automatically updating this when the encoding version changes!
	const char *pFormatName = GENERIC_DMX_FORMAT;
	int nFormatVersion = 1; // HACK - we should have some way of automatically updating this when the encoding version changes!
	buf.Printf( "%s encoding %s %d format %s %d %s\n", DMX_VERSION_STARTING_TOKEN, pEncodingName, nEncodingVersion, pFormatName, nFormatVersion, DMX_VERSION_ENDING_TOKEN );

	if ( buf.IsText() )
		return SerializeTextDMX( pFileName ? pFileName : "<no file>", buf, pRoot );

	CDmxSerializer dmxSerializer;
	return dmxSerializer.Serialize( buf, pRoot, pFileName );
}

bool SerializeDMX( const char *pFileName, const char *pPathID, bool bTextMode, CDmxElement *pRoot )
{
	// NOTE: This guarantees full path names for pathids
	char pBuf[MAX_PATH];
	const char *pFullPath = pFileName;
	if ( !Q_IsAbsolutePath( pFullPath ) && !pPathID )
	{
		char pDir[MAX_PATH];
		if ( g_pFullFileSystem->GetCurrentDirectory( pDir, sizeof(pDir) ) )
		{
			Q_ComposeFileName( pDir, pFileName, pBuf, sizeof(pBuf) );
			Q_RemoveDotSlashes( pBuf );
			pFullPath = pBuf;
		}
	}

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER |  CUtlBuffer::READ_ONLY );
	g_pFullFileSystem->ReadFile( pFullPath, pPathID, buf );

	if ( !buf.IsValid() )
	{
		Warning( "SerializeDMX: Unable to open file \"%s\"\n", pFullPath );
		return DMFILEID_INVALID;
	}

	return SerializeDMX( buf, pRoot, pFullPath );
}


//-----------------------------------------------------------------------------
// Read the header, return the version (or false if it's not a DMX file)
//-----------------------------------------------------------------------------
bool ReadDMXHeader( CUtlBuffer &buf, char *pEncodingName, int nEncodingNameLen, int &nEncodingVersion, char *pFormatName, int nFormatNameLen, int &nFormatVersion )
{
	// Make the buffer capable of being read as text
	bool bBufIsText = buf.IsText();
	bool bBufHasCRLF = buf.ContainsCRLF();
	buf.SetBufferType( true, !bBufIsText || bBufHasCRLF );

	char header[ DMX_MAX_HEADER_LENGTH ] = { 0 };
	bool bOk = buf.ParseToken( DMX_VERSION_STARTING_TOKEN, DMX_VERSION_ENDING_TOKEN, header, sizeof( header ) );
	if ( bOk )
	{
#ifdef _WIN32
		int nAssigned = sscanf_s( header, "encoding %s %d format %s %d\n", pEncodingName, nEncodingNameLen, &nEncodingVersion, pFormatName, nFormatNameLen, &nFormatVersion );
#else
		// sscanf considered harmful. We don't have POSIX 2008 support on OS X and "C11 Annex K" is optional... (optional specs considered useful)
		char pTmpEncodingName[ sizeof( header ) ] = { 0 };
		char pTmpFormatName  [ sizeof( header ) ] = { 0 };
		int nAssigned = sscanf( header, "encoding %s %d format %s %d\n", pTmpEncodingName, &nEncodingVersion, pTmpFormatName, &nFormatVersion );
		bOk = ( V_strlen( pTmpEncodingName ) < nEncodingNameLen ) && ( V_strlen( pTmpFormatName ) < nFormatNameLen );
		V_strncpy( pEncodingName, pTmpEncodingName, nEncodingNameLen );
		V_strncpy( pFormatName, pTmpFormatName, nFormatNameLen );
#endif
		bOk = bOk && ( nAssigned == 4 );
		if ( bOk )
		{
			bOk = !V_stricmp( pEncodingName, bBufIsText ? "keyvalues2" : "binary" );
		}
	}

	// TODO - retire legacy format version reading
	if ( !bOk )
	{
		buf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
		bOk = buf.ParseToken( DMX_LEGACY_VERSION_STARTING_TOKEN, DMX_LEGACY_VERSION_ENDING_TOKEN, pFormatName, sizeof( pFormatName ) );
		if ( bOk )
		{
			nEncodingVersion = 0;
			nFormatVersion = 0; // format version is encoded in the format name string

			if ( !V_stricmp( pFormatName, "binary_v1" ) || !V_stricmp( pFormatName, "binary_v2" ) )
			{
				bOk = !bBufIsText;
				V_strncpy( pEncodingName, "binary", nEncodingNameLen );
			}
			else if ( !V_stricmp( pFormatName, "keyvalues2_v1" ) || !V_stricmp( pFormatName, "keyvalues2_flat_v1" ) )
			{
				bOk = bBufIsText;
				V_strncpy( pEncodingName, "keyvalues2", nEncodingNameLen );
			}
			else
			{
				bOk = false;
			}
		}
	}

	// Restore the buffer type
	buf.SetBufferType( bBufIsText, bBufHasCRLF );
	return bOk && buf.IsValid();
}


//-----------------------------------------------------------------------------
// Unserialization main entry point
//-----------------------------------------------------------------------------
bool UnserializeDMX( CUtlBuffer &buf, CDmxElement **ppRoot, const char *pFileName )
{
	// NOTE: Checking the format name string for a version check here is how you'd do it
	*ppRoot = NULL;

	// Read the standard buffer header
	int nEncodingVersion, nFormatVersion;
	char pEncodingName[ DMX_MAX_FORMAT_NAME_MAX_LENGTH ];
	char pFormatName  [ DMX_MAX_FORMAT_NAME_MAX_LENGTH ];
	if ( !ReadDMXHeader( buf,
		pEncodingName, sizeof( pEncodingName ), nEncodingVersion,
		pFormatName, sizeof( pFormatName ), nFormatVersion ) )
		return false;

	// TODO - retire legacy format version reading
	if ( nFormatVersion == 0 ) // legacy formats store format version in their format name string
	{
		Warning( "reading file '%s' of legacy format '%s' - dmxconvert this file to a newer format!\n", pFileName ? pFileName : "<no file>", pFormatName );
	}

	// Only allow binary protocol files
	bool bIsBinary = ( buf.GetFlags() & CUtlBuffer::TEXT_BUFFER ) == 0;
	if ( bIsBinary )
	{
		CDmxSerializer dmxUnserializer;
		return dmxUnserializer.Unserialize( buf, nEncodingVersion, ppRoot );
	}
	return UnserializeTextDMX( pFileName ? pFileName : "<no file>", buf, ppRoot );
}

bool UnserializeDMX( const char *pFileName, const char *pPathID, bool bTextMode, CDmxElement **ppRoot )
{
	// NOTE: This guarantees full path names for pathids
	char pBuf[MAX_PATH];
	const char *pFullPath = pFileName;
	if ( !Q_IsAbsolutePath( pFullPath ) && !pPathID )
	{
		char pDir[MAX_PATH];
		if ( g_pFullFileSystem->GetCurrentDirectory( pDir, sizeof(pDir) ) )
		{
			Q_ComposeFileName( pDir, pFileName, pBuf, sizeof(pBuf) );
			Q_RemoveDotSlashes( pBuf );
			pFullPath = pBuf;
		}
	}

	int nFlags = CUtlBuffer::READ_ONLY;
	if ( bTextMode )
	{
		nFlags |= CUtlBuffer::TEXT_BUFFER;
	}

	CUtlBuffer buf( 0, 0, nFlags );
	g_pFullFileSystem->ReadFile( pFullPath, pPathID, buf );

	if ( !buf.IsValid() )
	{
		Warning( "UnserializeDMX: Unable to open file \"%s\"\n", pFullPath );
		return false;
	}

	return UnserializeDMX( buf, ppRoot, pFullPath );
}


//-----------------------------------------------------------------------------
// Cleans up read-in elements
//-----------------------------------------------------------------------------
void CleanupDMX( CDmxElement *pRoot )
{
	if ( pRoot )
	{
		pRoot->RemoveAllElementsRecursive();
	}
}
