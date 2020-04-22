//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef DMATTRIBUTEINTERNAL_H
#define DMATTRIBUTEINTERNAL_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmattribute.h"
#include "wchar.h"


//-----------------------------------------------------------------------------
// Forward declarations: 
//-----------------------------------------------------------------------------
class IDataModelFactory;
class CUtlBuffer;
class Vector;
class Color;
class CUtlCharConversion;
class CDmElement;


//-----------------------------------------------------------------------------
// Utility class to allow datamodel objects to access private members of CDmAttribute
//-----------------------------------------------------------------------------
class CDmAttributeAccessor
{
public:
	static void OnChanged( CDmAttribute *pAttribute, bool bArrayCountChanged = false, bool bIsTopological = false )
	{
		pAttribute->OnChanged( bArrayCountChanged, bIsTopological );
	}

	static void DestroyAttribute( CDmAttribute *pOldAttribute )
	{
		CDmAttribute::DestroyAttribute( pOldAttribute );
	}

	static bool MarkDirty( CDmAttribute *pAttribute )
	{
		return pAttribute->MarkDirty();
	}
};

//-----------------------------------------------------------------------------
// For serialization, set the delimiter rules
//-----------------------------------------------------------------------------
void SetSerializationDelimiter( CUtlCharConversion *pConv );
void SetSerializationArrayDelimiter( const char *pDelimiter );


//-----------------------------------------------------------------------------
// Skip unserialization for an attribute type (unserialize into a dummy variable)
//-----------------------------------------------------------------------------
bool SkipUnserialize( CUtlBuffer &buf, DmAttributeType_t type );


//-----------------------------------------------------------------------------
// Attribute names/types
//-----------------------------------------------------------------------------
const char *AttributeTypeName( DmAttributeType_t type );
DmAttributeType_t AttributeType( const char *pAttributeType );


//-----------------------------------------------------------------------------
// returns the number of attributes currently allocated
//-----------------------------------------------------------------------------
int GetAllocatedAttributeCount();

#endif // DMATTRIBUTEINTERNAL_H
