//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "importkeyvaluebase.h"
#include "dmserializers.h"
#include "datamodel/idatamodel.h"
#include "datamodel/dmelement.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "datamodel/dmattribute.h"


//-----------------------------------------------------------------------------
// Serialization class for VMF files (map files)
//-----------------------------------------------------------------------------
class CImportVMF : public CImportKeyValueBase
{
public:
	virtual const char *GetName() const { return "vmf"; }
	virtual const char *GetDescription() const { return "Valve Map File"; }
	virtual int GetCurrentVersion() const { return 0; } // doesn't store a version

	bool Serialize( CUtlBuffer &outBuf, CDmElement *pRoot );
	CDmElement* UnserializeFromKeyValues( KeyValues *pKeyValues );

private:
	// Reads a single entity
	bool UnserializeEntityKey( CDmAttribute *pEntities, KeyValues *pKeyValues );

	// Reads entity editor keys
	bool UnserializeEntityEditorKey( CDmAttribute *pEditor, KeyValues *pKeyValues );

	// Reads keys that we currently do nothing with
	bool UnserializeUnusedKeys( DmElementHandle_t hOther, KeyValues *pKeyValues );

	// Writes out all everything other than entities
	bool SerializeOther( CUtlBuffer &buf, CDmAttribute *pOther, const char **ppFilter = 0 );

	// Writes out all entities
	bool SerializeEntities( CUtlBuffer &buf, CDmAttribute *pEntities );

	// Writes out a single attribute recursively
	bool SerializeAttribute( CUtlBuffer &buf, CDmAttribute *pAttribute, bool bElementArrays );

	// Writes entity editor keys
	bool SerializeEntityEditorKey( CUtlBuffer &buf, DmElementHandle_t hEditor );

	// Updates the max hammer id
	void UpdateMaxHammerId( KeyValues *pKeyValue );

	// Max id read from the file
	int m_nMaxHammerId;
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CImportVMF s_ImportVMF;

void InstallVMFImporter( IDataModel *pFactory )
{
	pFactory->AddSerializer( &s_ImportVMF );
}


//-----------------------------------------------------------------------------
// Deals with poorly-named key values for the DME system
//-----------------------------------------------------------------------------
static const char *s_pKeyRemapNames[][2] = 
{
	{ "id", "__id" },
	{ "name", "__name" },
	{ "type", "__type" },
	{ NULL, NULL },
};


//-----------------------------------------------------------------------------
// Gets remap name for unserialization/serailzation
//-----------------------------------------------------------------------------
static const char *GetRemapName( const char *pName, bool bSerialization )
{
	for ( int i = 0; s_pKeyRemapNames[i][0]; ++i )
	{
		if ( !Q_stricmp( pName, s_pKeyRemapNames[i][bSerialization] ) )
			return s_pKeyRemapNames[i][1 - bSerialization];
	}
	return pName;
}


//-----------------------------------------------------------------------------
// Writes out a single attribute recursively
//-----------------------------------------------------------------------------
bool CImportVMF::SerializeAttribute( CUtlBuffer &buf, CDmAttribute *pAttribute, bool bElementArrays )
{
	if ( pAttribute->IsFlagSet( FATTRIB_STANDARD | FATTRIB_DONTSAVE ) )
		return true;

	const char *pFieldName = GetRemapName( pAttribute->GetName(), true );
	if ( !Q_stricmp( pFieldName, "editorType" ) )
		return true;

	if ( !IsArrayType( pAttribute->GetType() ) )
	{
		if ( !bElementArrays )
		{
			buf.Printf( "\"%s\" ", pFieldName );
			if ( pAttribute->GetType() != AT_STRING )
			{
				buf.Printf( "\"" );
			}
			g_pDataModel->SetSerializationDelimiter( GetCStringCharConversion() );
			pAttribute->Serialize( buf );
			g_pDataModel->SetSerializationDelimiter( NULL );
			if ( pAttribute->GetType() != AT_STRING )
			{
				buf.Printf( "\"" );
			}
			buf.Printf( "\n" );
		}
	}
	else
	{
		if ( bElementArrays )
		{
			Assert( pAttribute->GetType() == AT_ELEMENT_ARRAY );
			if ( !SerializeOther( buf, pAttribute ) )
				return false;
		}
	}

	return true;
}

	
//-----------------------------------------------------------------------------
// Writes out all everything other than entities
//-----------------------------------------------------------------------------
bool CImportVMF::SerializeOther( CUtlBuffer &buf, CDmAttribute *pOther, const char **ppFilter )
{
	CDmrElementArray<> array( pOther );
	int nCount = array.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmElement *pElement = array[i];
		const char *pElementName = pElement->GetName();
		if ( ppFilter )
		{
			int j;
			for ( j = 0; ppFilter[j]; ++j )
			{
				if ( !Q_stricmp( pElementName, ppFilter[j] ) )
					break;
			}

			if ( !ppFilter[j] )
				continue;
		}

		int nLen = Q_strlen( pElementName ) + 1;
		char *pTemp = (char*)_alloca( nLen );
		Q_strncpy( pTemp, pElementName, nLen );
		Q_strlower( pTemp );
		buf.Printf( "%s\n", pTemp );
		buf.Printf( "{\n" );
		buf.PushTab();

		// Normal attributes first
	    for( CDmAttribute *pAttribute = pElement->FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
		{
			if ( !SerializeAttribute( buf, pAttribute, false ) )
				return false;
		}

		// Subkeys later
	    for( CDmAttribute *pAttribute = pElement->FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
		{
			if ( !SerializeAttribute( buf, pAttribute, true ) )
				return false;
		}

		buf.PopTab();
		buf.Printf( "}\n" );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Writes entity editor keys
//-----------------------------------------------------------------------------
bool CImportVMF::SerializeEntityEditorKey( CUtlBuffer &buf, DmElementHandle_t hEditor )
{
	CDmElement *pEditorElement = g_pDataModel->GetElement( hEditor );
	if ( !pEditorElement )
		return true;

	buf.Printf( "editor\n" );
	buf.Printf( "{\n" );
	buf.PushTab();

	{
		CDmAttribute *pAttribute = pEditorElement->GetAttribute( "color" );
		if ( pAttribute )
		{
			Color c = pAttribute->GetValue<Color>();
			buf.Printf( "\"color\" \"%d %d %d\"\n", c.r(), c.g(), c.b() );
		}
	}
	PrintIntAttribute( pEditorElement, buf, "id" ); // FIXME - id is a DmObjectId_t!!! This should never print anything!
	PrintStringAttribute( pEditorElement, buf, "comments" );
	PrintBoolAttribute( pEditorElement, buf, "visgroupshown" );
	PrintBoolAttribute( pEditorElement, buf, "visgroupautoshown" );

	for ( CDmAttribute *pAttribute = pEditorElement->FirstAttribute(); pAttribute != NULL; pAttribute = pAttribute->NextAttribute() )
	{
		if ( pAttribute->IsFlagSet( FATTRIB_STANDARD | FATTRIB_DONTSAVE ) )
			continue;

		const char *pKeyName = pAttribute->GetName();
		if ( Q_stricmp( pKeyName, "color" ) && Q_stricmp( pKeyName, "id" ) && 
			Q_stricmp( pKeyName, "comments" ) && Q_stricmp( pKeyName, "visgroupshown" ) &&
			Q_stricmp( pKeyName, "visgroupautoshown" ) )
		{
			PrintStringAttribute( pEditorElement, buf, pKeyName );
		}
	}

	buf.PopTab();
	buf.Printf( "}\n" );

	return true;
}


//-----------------------------------------------------------------------------
// Writes out all entities
//-----------------------------------------------------------------------------
bool CImportVMF::SerializeEntities( CUtlBuffer &buf, CDmAttribute *pEntities )
{
	// FIXME: Make this serialize in the order in which it appears in the FGD
	// to minimize diffs
	CDmrElementArray<> array( pEntities );

	int nCount = array.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmElement *pElement = array[i];
		buf.Printf( "entity\n" );
		buf.Printf( "{\n" );
		buf.PushTab();
		buf.Printf( "\"id\" \"%s\"\n", pElement->GetName() );

	    for( CDmAttribute *pAttribute = pElement->FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
		{
			// Do 'editor' at the end to preserve ordering and not make terrible diffs
			if ( !Q_stricmp( pAttribute->GetName(), "editor" ) )
				continue;

			if ( !SerializeAttribute( buf, pAttribute, false ) )
				return false;
		}

	    for( CDmAttribute *pAttribute = pElement->FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
		{
			// Do 'editor' at the end to preserve ordering and not make terrible diffs
			if ( !Q_stricmp( pAttribute->GetName(), "editor" ) )
				continue;

			if ( !SerializeAttribute( buf, pAttribute, true ) )
				return false;
		}

		// Do the 'editor'
		CDmAttribute *pEditor = pElement->GetAttribute( "editor" );
		if ( pEditor )
		{
			SerializeEntityEditorKey( buf, pEditor->GetValue<DmElementHandle_t>() );
		}

		buf.PopTab();
		buf.Printf( "}\n" );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Writes out a new VMF file
//-----------------------------------------------------------------------------
bool CImportVMF::Serialize( CUtlBuffer &buf, CDmElement *pRoot )
{
	// This is done in this strange way (namely, serializing other twice) to minimize diffs
	const char *pOtherFilter1[] = 
	{
		"versioninfo", "visgroups", "viewsettings", "world", NULL
	};

	const char *pOtherFilter2[] = 
	{
		"cameras", "cordon", "hidden", NULL
	};

	CDmAttribute *pOther = pRoot->GetAttribute( "other" );
	if ( pOther && pOther->GetType() == AT_ELEMENT_ARRAY )
	{
		if ( !SerializeOther( buf, pOther, pOtherFilter1 ) )
			return false;
	}

	// Serialize entities
	CDmAttribute *pEntities = pRoot->GetAttribute( "entities" );
	if ( pEntities && pEntities->GetType() == AT_ELEMENT_ARRAY )
	{
		if ( !SerializeEntities( buf, pEntities ) )
			return false;
	}

	if ( pOther && pOther->GetType() == AT_ELEMENT_ARRAY )
	{
		if ( !SerializeOther( buf, pOther, pOtherFilter2 ) )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Updates the max hammer id
//-----------------------------------------------------------------------------
void CImportVMF::UpdateMaxHammerId( KeyValues *pField )
{
	if ( !Q_stricmp( pField->GetName(), "id" ) )
	{
		int nId = atoi( pField->GetString() );
		if ( nId > m_nMaxHammerId )
		{
			m_nMaxHammerId = nId;
		}
	}
}


//-----------------------------------------------------------------------------
// Reads entity editor keys
//-----------------------------------------------------------------------------
bool CImportVMF::UnserializeEntityEditorKey( CDmAttribute *pEditorAttribute, KeyValues *pKeyValues )
{
	CDmElement *pEditor;
	DmElementHandle_t hEditor = pEditorAttribute->GetValue<DmElementHandle_t>();
	if ( hEditor == DMELEMENT_HANDLE_INVALID )
	{
		pEditor = CreateDmElement( "DmElement", "editor", NULL );;
		if ( !pEditor )
			return false;
		hEditor = pEditor->GetHandle();
		pEditorAttribute->SetValue( hEditor );
	}
	else
	{
		pEditor = g_pDataModel->GetElement( hEditor );
	}

	int r, g, b;
	if ( sscanf( pKeyValues->GetString( "color", "" ), "%d %d %d", &r, &g, &b ) == 3 )
	{
		Color c( r, g, b, 255 );
		if ( !pEditor->SetValue( "color", c ) )
			return false;
	}
	KeyValues *pIdKey = pKeyValues->FindKey( "id" );
	if ( pIdKey )
	{
		UpdateMaxHammerId( pIdKey );
	}
	AddIntAttribute( pEditor, pKeyValues, "id" );
	AddStringAttribute( pEditor, pKeyValues, "comments" );
	AddBoolAttribute( pEditor, pKeyValues, "visgroupshown" );
	AddBoolAttribute( pEditor, pKeyValues, "visgroupautoshown" );

	for ( KeyValues *pUserKey = pKeyValues->GetFirstValue(); pUserKey != NULL; pUserKey = pUserKey->GetNextValue() )
	{
		const char *pKeyName = pUserKey->GetName();
		if ( Q_stricmp( pKeyName, "color" ) && Q_stricmp( pKeyName, "id" ) && 
			Q_stricmp( pKeyName, "comments" ) && Q_stricmp( pKeyName, "visgroupshown" ) &&
			Q_stricmp( pKeyName, "visgroupautoshown" ) )
		{
			AddStringAttribute( pEditor, pKeyValues, pKeyName );
		}
	}

	return true;
}

	
//-----------------------------------------------------------------------------
// Reads a single entity
//-----------------------------------------------------------------------------
bool CImportVMF::UnserializeEntityKey( CDmAttribute *pEntities, KeyValues *pKeyValues )
{
	CDmElement *pEntity = CreateDmElement( "DmeVMFEntity", pKeyValues->GetString( "id", "-1" ), NULL );
	if ( !pEntity )
		return false;

	CDmrElementArray<> array( pEntities );
	array.AddToTail( pEntity );

	// Each act busy needs to have an editortype associated with it so it displays nicely in editors
	pEntity->SetValue( "editorType", "vmfEntity" );

	const char *pClassName = pKeyValues->GetString( "classname", NULL );
	if ( !pClassName )
		return false;

	// Read the actual fields
	for ( KeyValues *pField = pKeyValues->GetFirstValue(); pField != NULL; pField = pField->GetNextValue() )
	{
		// FIXME: Knowing the FGD here would be useful for type determination.
		// Look up the field by name based on class name
		// In the meantime, just use the keyvalues type?
		char pFieldName[512];
		Q_strncpy( pFieldName, pField->GetName(), sizeof(pFieldName) );
		Q_strlower( pFieldName );

		// Don't do id: it's used as the name
		// Not to mention it's a protected name
		if ( !Q_stricmp( pFieldName, "id" ) )
		{
			UpdateMaxHammerId( pField );
			continue;
		}

		// Type, name, and editortype are protected names
		Assert( Q_stricmp( pFieldName, "type" ) && Q_stricmp( pFieldName, "name" ) && Q_stricmp( pFieldName, "editortype" ) );

		switch( pField->GetDataType() )
		{
		case KeyValues::TYPE_INT:
			if ( !AddIntAttributeFlags( pEntity, pKeyValues, pFieldName, FATTRIB_USERDEFINED ) )
				return false;
			break;

		case KeyValues::TYPE_FLOAT:
			if ( !AddFloatAttributeFlags( pEntity, pKeyValues, pFieldName, FATTRIB_USERDEFINED ) )
				return false;
			break;

		case KeyValues::TYPE_STRING:
			{
				const char* pString = pField->GetString();
				if (!pString || !pString[0])
					return false;

				// Look for vectors
				Vector4D v;
				if ( sscanf( pString, "%f %f %f %f", &v.x, &v.y, &v.z, &v.w ) == 4 )
				{
					if ( !pEntity->SetValue( pFieldName, v ) )
						return false;
					CDmAttribute *pAttribute = pEntity->GetAttribute( pFieldName );
					pAttribute->AddFlag( FATTRIB_USERDEFINED );
				}
				else if ( sscanf( pString, "%f %f %f", &v.x, &v.y, &v.z ) == 3 )
				{
					if ( !pEntity->SetValue( pFieldName, v.AsVector3D() ) )
					{
						QAngle ang( v.x, v.y, v.z );
						if ( !pEntity->SetValue( pFieldName, ang ) )
							return false;
					}
					CDmAttribute *pAttribute = pEntity->GetAttribute( pFieldName );
					pAttribute->AddFlag( FATTRIB_USERDEFINED );
				}
				else
				{
					if ( !AddStringAttributeFlags( pEntity, pKeyValues, pFieldName, FATTRIB_USERDEFINED ) )
						return false;
				}
			}
			break;
		}
	}

	// Read the subkeys
	CDmAttribute *pEditor = pEntity->AddAttribute( "editor", AT_ELEMENT );
	CDmrElementArray<> otherKeys( pEntity->AddAttribute( "other", AT_ELEMENT_ARRAY ) );
	for ( KeyValues *pSubKey = pKeyValues->GetFirstTrueSubKey(); pSubKey != NULL; pSubKey = pSubKey->GetNextTrueSubKey() )
	{
		bool bOk = false;
		if ( !Q_stricmp( pSubKey->GetName(), "editor" ) )
		{
			bOk = UnserializeEntityEditorKey( pEditor, pSubKey );
		}
		else
		{
			// We don't currently do anything with the other keys
			CDmElement *pOther = CreateDmElement( "DmElement", pSubKey->GetName(), NULL );
			otherKeys.AddToTail( pOther );
			bOk = UnserializeUnusedKeys( pOther->GetHandle(), pSubKey );
		}

		if ( !bOk )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Reads keys that we currently do nothing with
//-----------------------------------------------------------------------------
bool CImportVMF::UnserializeUnusedKeys( DmElementHandle_t hOther, KeyValues *pKeyValues )
{
	CDmElement *pOther = g_pDataModel->GetElement( hOther );

	// Read the actual fields
	for ( KeyValues *pField = pKeyValues->GetFirstValue(); pField != NULL; pField = pField->GetNextValue() )
	{
		UpdateMaxHammerId( pField );
		const char *pFieldName = GetRemapName( pField->GetName(), false );
		pOther->SetValue( pFieldName, pField->GetString() );
	}

	// Read the subkeys
	CDmrElementArray<> subKeys( pOther->AddAttribute( "subkeys", AT_ELEMENT_ARRAY ) );
	for ( KeyValues *pSubKey = pKeyValues->GetFirstTrueSubKey(); pSubKey != NULL; pSubKey = pSubKey->GetNextTrueSubKey() )
	{
		CDmElement *pSubElement = CreateDmElement( "DmElement", pSubKey->GetName(), NULL );
		subKeys.AddToTail( pSubElement );
		if ( !UnserializeUnusedKeys( pSubElement->GetHandle(), pSubKey ) )
			return false;
	}
	return true;
}

	
/*
//-----------------------------------------------------------------------------
// Reads the cordon data
//-----------------------------------------------------------------------------
bool CImportVMF::UnserializeCordonKey( IDmAttributeElement *pCordon, KeyValues *pKeyValues )
{
	DmElementHandle_t hCordon = pCordon->GetValue().Get();
	if ( hCordon == DMELEMENT_HANDLE_INVALID )
	{
		hCordon = CreateDmElement( "DmElement", "cordon", NULL );
		if ( hCordon == DMELEMENT_HANDLE_INVALID )
			return false;
		pCordon->SetValue( hCordon );
	}

	AddBoolAttribute( hCordon, pKeyValues, "active" );

	Vector v;
	if ( sscanf( pKeyValues->GetString( "mins", "" ), "(%f %f %f)", &v.x, &v.y, &v.z ) == 3 )
	{
		if ( !DmElementAddAttribute( hCordon, "mins", v ) )
			return false;
	}
	if ( sscanf( pKeyValues->GetString( "maxs", "" ), "(%f %f %f)", &v.x, &v.y, &v.z ) == 3 )
	{
		if ( !DmElementAddAttribute( hCordon, "maxs", v ) )
			return false;
	}
	return true;
}
*/


//-----------------------------------------------------------------------------
// Main entry point for the unserialization
//-----------------------------------------------------------------------------
CDmElement* CImportVMF::UnserializeFromKeyValues( KeyValues *pKeyValues )
{
	m_nMaxHammerId = 0;

	// Create the main element
	CDmElement *pElement = CreateDmElement( "DmElement", "VMF", NULL );
	if ( !pElement )
		return NULL;

	// Each vmf needs to have an editortype associated with it so it displays nicely in editors
	pElement->SetValue( "editorType", "VMF" );

	// The VMF is a series of keyvalue blocks; either 
	// 'entity', 'cameras', 'cordon', 'world', 'versioninfo', or 'viewsettings' 
	CDmAttribute *pEntityArray = pElement->AddAttribute( "entities", AT_ELEMENT_ARRAY );

	// All main keys are root keys
	CDmrElementArray<> otherKeys( pElement->AddAttribute( "other", AT_ELEMENT_ARRAY ) );
	for ( ; pKeyValues != NULL; pKeyValues = pKeyValues->GetNextKey() )
	{
		bool bOk = false;
		if ( !Q_stricmp( pKeyValues->GetName(), "entity" ) )
		{
			bOk = UnserializeEntityKey( pEntityArray, pKeyValues );
		}
		else
		{
			// We don't currently do anything with 
			CDmElement *pOther = CreateDmElement( "DmElement", pKeyValues->GetName(), NULL );
			otherKeys.AddToTail( pOther );
			bOk = UnserializeUnusedKeys( pOther->GetHandle(), pKeyValues );
		}

		if ( !bOk )
		{
			Warning( "Error importing VMF element %s\n", pKeyValues->GetName() );
			return NULL;
		}
	}

	// Resolve all element references recursively
	RecursivelyResolveElement( pElement );

	// Add the max id read in from the file to the root entity
	pElement->SetValue( "maxHammerId", m_nMaxHammerId );

	return pElement;
}
