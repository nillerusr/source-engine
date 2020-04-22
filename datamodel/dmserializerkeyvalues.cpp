//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dmserializerkeyvalues.h"
#include "datamodel/idatamodel.h"
#include "datamodel.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"
#include "dmattributeinternal.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlvector.h"
#include <limits.h>
#include "DmElementFramework.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CUtlBuffer;
class CBaseSceneObject;


//-----------------------------------------------------------------------------
// Used to remap keyvalues names
//-----------------------------------------------------------------------------
struct AttributeRemap_t
{
	const char *m_pKeyValuesName;
	const char *m_pDmeName;
};

static AttributeRemap_t s_pAttributeRemap[] = 
{
	{ "type", "_type" }, // FIXME - remove this once we've made type no longer be an attribute
	{ "name", "_name" },
	{ "id", "_id" }, // FIXME - remove this once we've made id no longer be an attribute
	{ NULL, NULL }
};


//-----------------------------------------------------------------------------
// Serialization class for Key Values
//-----------------------------------------------------------------------------
class CDmSerializerKeyValues : public IDmSerializer
{
public:
	// Inherited from IDMSerializer
	virtual const char *GetName() const { return "keyvalues"; }
	virtual const char *GetDescription() const { return "KeyValues"; }
	virtual bool StoresVersionInFile() const { return false; }
	virtual bool IsBinaryFormat() const { return false; }
	virtual int GetCurrentVersion() const { return 0; } // doesn't store a version
	virtual bool Serialize( CUtlBuffer &buf, CDmElement *pRoot );
	virtual bool Unserialize( CUtlBuffer &buf, const char *pEncodingName, int nEncodingVersion,
							  const char *pSourceFormatName, int nSourceFormatVersion,
							  DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot );

private:
	// Methods related to serialization
	void SerializeSubKeys( CUtlBuffer& buf, CDmAttribute *pSubKeys );
	bool SerializeAttributes( CUtlBuffer& buf, CDmElement *pElement );
	bool SerializeElement( CUtlBuffer& buf, CDmElement *pElement );

	// Methods related to unserialization
	DmElementHandle_t UnserializeElement( KeyValues *pKeyValues, int iNestingLevel );
	void UnserializeAttribute( CDmElement *pElement, KeyValues *pKeyValues );
	DmElementHandle_t CreateDmElement( const char *pElementType, const char *pElementName );
	CDmElement* UnserializeFromKeyValues( KeyValues *pKeyValues );

	// Deterimines the attribute type of a keyvalue
	DmAttributeType_t DetermineAttributeType( KeyValues *pKeyValues );

	// For unserialization
	CUtlVector<DmElementHandle_t> m_ElementList;
	DmElementHandle_t m_hRoot;
	DmFileId_t m_fileid;
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CDmSerializerKeyValues s_DMSerializerKeyValues;

void InstallKeyValuesSerializer( IDataModel *pFactory )
{
	pFactory->AddSerializer( &s_DMSerializerKeyValues );
}


//-----------------------------------------------------------------------------
// Serializes a single element attribute
//-----------------------------------------------------------------------------
void CDmSerializerKeyValues::SerializeSubKeys( CUtlBuffer& buf, CDmAttribute *pSubKeys )
{
	CDmrElementArray<> array( pSubKeys );
	int c = array.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmElement *pChild = array[i];
		if ( pChild )
		{
			SerializeElement( buf, pChild );
		}
	}
}


//-----------------------------------------------------------------------------
// Serializes all attributes in an element
//-----------------------------------------------------------------------------
bool CDmSerializerKeyValues::SerializeAttributes( CUtlBuffer& buf, CDmElement *pElement )
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
	for ( int i = nAttributes - 1; i >= 0; --i )
	{
		CDmAttribute *pAttribute = ppAttributes[ i ];
		Assert( pAttribute );

		const char *pName = pAttribute->GetName();

		// Rename "_type", "_name", or "_id" fields, since they are special fields
		for ( int iAttr = 0; s_pAttributeRemap[i].m_pKeyValuesName; ++i )
		{
			if ( !Q_stricmp( pName, s_pAttributeRemap[iAttr].m_pDmeName ) )
			{
				pName = s_pAttributeRemap[iAttr].m_pKeyValuesName;
				break;
			}
		}

  		DmAttributeType_t nAttrType = pAttribute->GetType();
		if ( ( nAttrType ==  AT_ELEMENT_ARRAY ) && !Q_stricmp( pName, "subkeys" ) )
		{
			SerializeSubKeys( buf, pAttribute );
			continue;
		}

		buf.Printf( "\"%s\" ", pName );

		switch( nAttrType )
		{
		case AT_VOID:
		case AT_STRING_ARRAY:
		case AT_VOID_ARRAY:
		case AT_ELEMENT:
		case AT_ELEMENT_ARRAY:
			Warning("KeyValues: Can't serialize attribute of type %s into KeyValues files!\n", 
				g_pDataModel->GetAttributeNameForType( nAttrType ) );
			buf.PutChar( '\"' );
			buf.PutChar( '\"' );
			break;

		case AT_FLOAT:
		case AT_INT:
		case AT_BOOL:
			pAttribute->Serialize( buf );
			break;

		case AT_VECTOR4:
		case AT_VECTOR3:
		case AT_VECTOR2:
		case AT_STRING:
		default:
			buf.PutChar( '\"' );
			buf.PushTab();
			pAttribute->Serialize( buf );
			buf.PopTab();
			buf.PutChar( '\"' );
			break;
		}

 		buf.PutChar( '\n' );
	}

	return true;
}

bool CDmSerializerKeyValues::SerializeElement( CUtlBuffer& buf, CDmElement *pElement )
{
	buf.Printf( "\"%s\"\n{\n", pElement->GetName() );
	buf.PushTab();
	SerializeAttributes( buf, pElement );
	buf.PopTab();
	buf.Printf( "}\n" );
	return true;
}

bool CDmSerializerKeyValues::Serialize( CUtlBuffer &outBuf, CDmElement *pRoot )
{
	if ( !pRoot )
		return true;

	CDmAttribute* pSubKeys = pRoot->GetAttribute( "subkeys" );
	if ( !pSubKeys )
		return true;

	//SetSerializationDelimiter( GetCStringCharConversion() );
	SerializeSubKeys( outBuf, pSubKeys );
	//SetSerializationDelimiter( NULL );
	return true;
}


//-----------------------------------------------------------------------------
// Creates a scene object, adds it to the element dictionary
//-----------------------------------------------------------------------------
DmElementHandle_t CDmSerializerKeyValues::CreateDmElement( const char *pElementType, const char *pElementName )
{
	// See if we can create an element of that type
	DmElementHandle_t hElement = g_pDataModel->CreateElement( pElementType, pElementName, m_fileid );
	if ( hElement == DMELEMENT_HANDLE_INVALID )
	{
		Warning("KeyValues: Element uses unknown element type %s\n", pElementType );
		return DMELEMENT_HANDLE_INVALID;
	}

	m_ElementList.AddToTail( hElement );

	CDmElement *pElement = g_pDataModel->GetElement( hElement );
	CDmeElementAccessor::MarkBeingUnserialized( pElement, true );
	return hElement;
}


//-----------------------------------------------------------------------------
// Deterimines the attribute type of a keyvalue
//-----------------------------------------------------------------------------
DmAttributeType_t CDmSerializerKeyValues::DetermineAttributeType( KeyValues *pKeyValues )
{
	// FIXME: Add detection of vectors/matrices?
	switch( pKeyValues->GetDataType() )
	{
	default:
	case KeyValues::TYPE_NONE:
		Assert( 0 );
		return AT_UNKNOWN;

	case KeyValues::TYPE_STRING:
		{
			float f1, f2, f3, f4;
			if ( sscanf( pKeyValues->GetString(), "%f %f %f %f", &f1, &f2, &f3, &f4 ) == 4 )
				return AT_VECTOR4;
			if ( sscanf( pKeyValues->GetString(), "%f %f %f",&f1, &f2, &f3 ) == 3 )
				return AT_VECTOR3;
			if ( sscanf( pKeyValues->GetString(), "%f %f", &f1, &f2 ) == 2 )
				return AT_VECTOR2;

			int i = pKeyValues->GetInt( NULL, INT_MAX );
			if ( ( sscanf( pKeyValues->GetString(), "%d", &i ) == 1 ) && 
				 ( !strchr( pKeyValues->GetString(), '.' ) ) )
				return AT_INT;

			if ( sscanf( pKeyValues->GetString(), "%f", &f1 ) == 1 )
				return AT_FLOAT;

			return AT_STRING;
		}

	case KeyValues::TYPE_INT:
		return AT_INT;

	case KeyValues::TYPE_FLOAT:
		return AT_FLOAT;

	case KeyValues::TYPE_PTR:
		return AT_VOID;

	case KeyValues::TYPE_COLOR:
		return AT_COLOR;
	}
}


//-----------------------------------------------------------------------------
// Reads an attribute for an element
//-----------------------------------------------------------------------------
void CDmSerializerKeyValues::UnserializeAttribute( CDmElement *pElement, KeyValues *pKeyValues )
{
	// It's an attribute
	const char *pAttributeName = pKeyValues->GetName();
	const char *pAttributeValue = pKeyValues->GetString();

	// Convert to lower case
	CUtlString pLowerName = pAttributeName;
	pLowerName.ToLower();

	// Rename "type", "name", or "id" fields, since they are special fields
	for ( int i = 0; s_pAttributeRemap[i].m_pKeyValuesName; ++i )
	{
		if ( !Q_stricmp( pLowerName, s_pAttributeRemap[i].m_pKeyValuesName ) )
		{
			pLowerName = s_pAttributeRemap[i].m_pDmeName;
			break;
		}
	}

	// Element types are stored out by GUID, we need to hang onto the guid and 
	// link it back up once all elements have been loaded from the file
	DmAttributeType_t type = DetermineAttributeType( pKeyValues );

	// In this case, we have an inlined element or element array attribute
	if ( type == AT_UNKNOWN )
	{
		// Assume this is an empty attribute or attribute array element
		Warning("Dm Unserialize: Attempted to read an attribute (\"%s\") of an inappropriate type!\n", pLowerName.Get() );
		return;
	}

	CDmAttribute *pAttribute = pElement->AddAttribute( pLowerName, type );
	if ( !pAttribute )
	{
		Warning("Dm Unserialize: Attempted to read an attribute (\"%s\") of an inappropriate type!\n", pLowerName.Get() );
		return;
	}

	switch( type )
	{
	case AT_STRING:
		{
			// Strings have different delimiter rules for KeyValues, 
			// so let's just directly copy the string instead of going through unserialize
			pAttribute->SetValue( pAttributeValue );
		}
		break;
	
	default:
		{
			int nLen = Q_strlen( pAttributeValue );
			CUtlBuffer buf( pAttributeValue, nLen, CUtlBuffer::TEXT_BUFFER | CUtlBuffer::READ_ONLY ); 
			pAttribute->Unserialize( buf );
		}
		break;
	}
}


//-----------------------------------------------------------------------------
// Reads a single element
//-----------------------------------------------------------------------------
DmElementHandle_t CDmSerializerKeyValues::UnserializeElement( KeyValues *pKeyValues, int iNestingLevel )
{
	const char *pElementName = pKeyValues->GetName( );
	const char *pszKeyValuesElement = g_pDataModel->GetKeyValuesElementName( pElementName, iNestingLevel );
	if ( !pszKeyValuesElement )
	{
		pszKeyValuesElement = "DmElement";
	}

	DmElementHandle_t handle = CreateDmElement( pszKeyValuesElement, pElementName );
	Assert( handle != DMELEMENT_HANDLE_INVALID );

	iNestingLevel++;

	CDmElement *pElement = g_pDataModel->GetElement( handle );
	CDmrElementArray<> subKeys;
	for ( KeyValues *pSub = pKeyValues->GetFirstSubKey(); pSub != NULL ; pSub = pSub->GetNextKey() )
	{
		// Read in a subkey
		if ( pSub->GetDataType() == KeyValues::TYPE_NONE )
		{
			if ( !subKeys.IsValid() )
			{
				subKeys.Init( pElement->AddAttribute( "subkeys", AT_ELEMENT_ARRAY ) );
			}

			DmElementHandle_t hChild = UnserializeElement( pSub, iNestingLevel );
			if ( hChild != DMELEMENT_HANDLE_INVALID )
			{
				subKeys.AddToTail( hChild );
			}
		}
		else
		{
			UnserializeAttribute( pElement, pSub );
		}
	}

	return handle;
}


//-----------------------------------------------------------------------------
// Main entry point for the unserialization
//-----------------------------------------------------------------------------
CDmElement* CDmSerializerKeyValues::UnserializeFromKeyValues( KeyValues *pKeyValues )
{
	m_ElementList.RemoveAll();

	m_hRoot = CreateDmElement( "DmElement", "root" );
 	CDmElement *pRoot = g_pDataModel->GetElement( m_hRoot );
	CDmrElementArray<> subkeys( pRoot->AddAttribute( "subkeys", AT_ELEMENT_ARRAY ) );

	int iNestingLevel = 0;

	for ( KeyValues *pElementKey = pKeyValues; pElementKey != NULL; pElementKey = pElementKey->GetNextKey() )
	{
		DmElementHandle_t hChild = UnserializeElement( pElementKey, iNestingLevel );
		if ( hChild != DMELEMENT_HANDLE_INVALID )
		{
			subkeys.AddToTail( hChild );
		}
	}

	// mark all unserialized elements as done unserializing, and call Resolve()
	int c = m_ElementList.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmElement *pElement = g_pDataModel->GetElement( m_ElementList[i] );
		CDmeElementAccessor::MarkBeingUnserialized( pElement, false );
	}

	g_pDmElementFrameworkImp->RemoveCleanElementsFromDirtyList( );
	m_ElementList.RemoveAll();
    return pRoot;
}

	
//-----------------------------------------------------------------------------
// Main entry point for the unserialization
//-----------------------------------------------------------------------------
bool CDmSerializerKeyValues::Unserialize( CUtlBuffer &buf, const char *pEncodingName, int nEncodingVersion,
										  const char *pSourceFormatName, int nSourceFormatVersion,
										  DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot )
{
	Assert( !V_stricmp( pEncodingName, "keyvalues" ) );

	*ppRoot = NULL;

	KeyValues *kv = new KeyValues( "keyvalues file" );
	if ( !kv )
		return false;

	m_fileid = fileid;

	bool bOk = kv->LoadFromBuffer( "keyvalues file", buf );
	if ( bOk )
	{
		//SetSerializationDelimiter( GetCStringCharConversion() );
		*ppRoot = UnserializeFromKeyValues( kv );
		//SetSerializationDelimiter( NULL );
	}

	m_fileid = DMFILEID_INVALID;

	kv->deleteThis();
	return bOk;
}
