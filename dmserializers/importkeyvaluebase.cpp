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
#include <limits.h>


//-----------------------------------------------------------------------------
// Default serialization method
//-----------------------------------------------------------------------------
bool CImportKeyValueBase::Serialize( CUtlBuffer &outBuf, CDmElement *pRoot )
{
	Warning( "Serialization not supported for importing from keyvalues files\n");
	return false;
}


//-----------------------------------------------------------------------------
// Creates a new element
//-----------------------------------------------------------------------------
CDmElement* CImportKeyValueBase::CreateDmElement( const char *pElementType, const char *pElementName, DmObjectId_t *pId )
{
	// See if we can create an element of that type
	DmElementHandle_t hElement = g_pDataModel->CreateElement( pElementType, pElementName, DMFILEID_INVALID, pId );
	if ( hElement == DMELEMENT_HANDLE_INVALID )
	{
		Warning("%s: Element uses unknown element type %s\n", m_pFileName, pElementType );
		return NULL;
	}

	return g_pDataModel->GetElement( hElement );
}


//-----------------------------------------------------------------------------
// Used to output typed attributes to keyvalues
//-----------------------------------------------------------------------------
void CImportKeyValueBase::PrintBoolAttribute( CDmElement* pElement, CUtlBuffer &outBuf, const char *pKeyName )
{
	if ( pElement->HasAttribute( pKeyName ) )
	{
		CDmAttribute *pAttribute = pElement->GetAttribute( pKeyName );
		if ( pAttribute->GetType() == AT_BOOL )
		{
			outBuf.Printf("\"%s\" \"%d\"\n", pKeyName, pAttribute->GetValue<bool>( ) );
		}
	}
}

void CImportKeyValueBase::PrintIntAttribute( CDmElement* pElement, CUtlBuffer &outBuf, const char *pKeyName )
{
	if ( pElement->HasAttribute( pKeyName ) )
	{
		CDmAttribute *pAttribute = pElement->GetAttribute( pKeyName );
		if ( pAttribute->GetType() == AT_INT )
		{
			outBuf.Printf("\"%s\" \"%d\"\n", pKeyName, pAttribute->GetValue<int>( ) );
		}
	}
}

void CImportKeyValueBase::PrintFloatAttribute( CDmElement* pElement, CUtlBuffer &outBuf, const char *pKeyName )
{
	if ( pElement->HasAttribute( pKeyName ) )
	{
		CDmAttribute *pAttribute = pElement->GetAttribute( pKeyName );
		if ( pAttribute->GetType() == AT_FLOAT )
		{
			outBuf.Printf("\"%s\" \"%.10f\"\n", pKeyName, pAttribute->GetValue<float>( ) );
		}
	}
}

void CImportKeyValueBase::PrintStringAttribute( CDmElement* pElement, CUtlBuffer &outBuf, const char *pKeyName, bool bSkipEmptryStrings, bool bPrintValueOnly )
{
	if ( pElement->HasAttribute( pKeyName ) )
	{
		CDmAttribute *pAttribute = pElement->GetAttribute( pKeyName );
		if ( pAttribute->GetType() == AT_STRING )
		{
			const char *pValue = pAttribute->GetValueString();
			if ( !bSkipEmptryStrings || pValue[0] )
			{
				if ( !bPrintValueOnly )
				{
					outBuf.Printf("\"%s\" \"%s\"\n", pKeyName, pValue );
				}
				else
				{
					outBuf.Printf("\"%s\"\n", pValue );
				}
			}
		}
	}
}

	
//-----------------------------------------------------------------------------
// Used to add typed attributes from keyvalues
//-----------------------------------------------------------------------------
bool CImportKeyValueBase::AddBoolAttribute( CDmElement* pElement, KeyValues *pKeyValues, const char *pKeyName, bool *pDefault )
{
	KeyValues *pKey = pKeyValues->FindKey( pKeyName );
	bool bValue;
	if ( pKey )
	{
		bValue = pKey->GetInt() != 0;
	}
	else
	{
		if ( !pDefault )
			return true;
		bValue = *pDefault;
	}

	return pElement->SetValue( pKeyName, bValue ) != NULL;
}

	
//-----------------------------------------------------------------------------
// Used to add typed attributes from keyvalues
//-----------------------------------------------------------------------------
bool CImportKeyValueBase::AddIntAttribute( CDmElement* pElement, KeyValues *pKeyValues, const char *pKeyName, int *pDefault )
{
	KeyValues *pKey = pKeyValues->FindKey( pKeyName );
	int nValue;
	if ( pKey )
	{
		nValue = pKey->GetInt();
	}
	else
	{
		if ( !pDefault )
			return true;
		nValue = *pDefault;
	}

	return pElement->SetValue( pKeyName, nValue ) != NULL;
}

bool CImportKeyValueBase::AddFloatAttribute( CDmElement* pElement, KeyValues *pKeyValues, const char *pKeyName, float *pDefault )
{
	KeyValues *pKey = pKeyValues->FindKey( pKeyName );
	float flValue;
	if ( pKey )
	{
		flValue = pKey->GetFloat();
	}
	else
	{
		if ( !pDefault )
			return true;
		flValue = *pDefault;
	}

	return pElement->SetValue( pKeyName, flValue ) != NULL;
}

bool CImportKeyValueBase::AddStringAttribute( CDmElement* pElement, KeyValues *pKeyValues, const char *pKeyName, const char *pDefault )
{
	KeyValues *pKey = pKeyValues->FindKey( pKeyName );
	const char *pValue = "";
	if ( pKey )
	{
		pValue = pKey->GetString();
	}
	else
	{
		if ( !pDefault )
			return true;
		pValue = pDefault;
	}

	return pElement->SetValue( pKeyName, pValue ) != NULL;
}


//-----------------------------------------------------------------------------
// Used to add typed attributes from keyvalues
//-----------------------------------------------------------------------------
bool CImportKeyValueBase::AddBoolAttributeFlags( CDmElement* pElement, KeyValues *pKeyValue, const char *pKeyName, int nFlags, bool *pDefault )
{
	if ( !AddBoolAttribute( pElement, pKeyValue, pKeyName, pDefault )  )
		return false;

	CDmAttribute *pAttribute = pElement->GetAttribute( pKeyName );
	pAttribute->AddFlag( nFlags );
	return true;
}

bool CImportKeyValueBase::AddIntAttributeFlags( CDmElement* pElement, KeyValues *pKeyValue, const char *pKeyName, int nFlags, int *pDefault )
{
	if ( !AddIntAttribute( pElement, pKeyValue, pKeyName, pDefault ) )
		return false;

	CDmAttribute *pAttribute = pElement->GetAttribute( pKeyName );
	pAttribute->AddFlag( nFlags );
	return true;
}

bool CImportKeyValueBase::AddFloatAttributeFlags( CDmElement* pElement, KeyValues *pKeyValue, const char *pKeyName, int nFlags, float *pDefault )
{
	if ( !AddFloatAttribute( pElement, pKeyValue, pKeyName, pDefault ) )
		return false;

	CDmAttribute *pAttribute = pElement->GetAttribute( pKeyName );
	pAttribute->AddFlag( nFlags );
	return true;
}

bool CImportKeyValueBase::AddStringAttributeFlags( CDmElement* pElement, KeyValues *pKeyValue, const char *pKeyName, int nFlags, const char *pDefault )
{
	if ( !AddStringAttribute( pElement, pKeyValue, pKeyName, pDefault ) )
		return false;

	CDmAttribute *pAttribute = pElement->GetAttribute( pKeyName );
	pAttribute->AddFlag( nFlags );
	return true;
}

	
//-----------------------------------------------------------------------------
// Recursively resolves all attributes pointing to elements
//-----------------------------------------------------------------------------
void CImportKeyValueBase::RecursivelyResolveElement( CDmElement* pElement )
{
	if ( !pElement )
		return;

	pElement->Resolve();

	CDmAttribute *pAttribute = pElement->FirstAttribute();
	while ( pAttribute )
	{
		switch ( pAttribute->GetType() )
		{
		case AT_ELEMENT:
			{
				CDmElement *pElementAt = pAttribute->GetValueElement<CDmElement>();
				RecursivelyResolveElement( pElementAt );
			}
			break;

		case AT_ELEMENT_ARRAY:
			{
				CDmrElementArray<> array( pAttribute );
				int nCount = array.Count();
				for ( int i = 0; i < nCount; ++i )
				{
					CDmElement *pElementAt = array[ i ];
					RecursivelyResolveElement( pElementAt );
				}
			}
			break;
		}

		pAttribute = pAttribute->NextAttribute( );
	}
}

	
//-----------------------------------------------------------------------------
// Main entry point for the unserialization
//-----------------------------------------------------------------------------
bool CImportKeyValueBase::Unserialize( CUtlBuffer &buf, const char *pEncodingName, int nEncodingVersion,
									   const char *pSourceFormatName, int nSourceFormatVersion,
									   DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot )
{
	*ppRoot = NULL;
	m_pFileName = g_pDataModel->GetFileName( fileid );

	KeyValues *kv = new KeyValues( "dmx file" );
	if ( !kv )
		return false;

	bool bOk = kv->LoadFromBuffer( "dmx file", buf );
	if ( bOk )
	{
		*ppRoot = UnserializeFromKeyValues( kv );
	}

	kv->deleteThis();
	return bOk;
}
