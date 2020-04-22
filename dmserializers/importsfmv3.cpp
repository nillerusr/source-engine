//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dmserializers.h"
#include "dmebaseimporter.h"
#include "datamodel/idatamodel.h"
#include "datamodel/dmelement.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlmap.h"
#include <limits.h>


//-----------------------------------------------------------------------------
// Format converter
//-----------------------------------------------------------------------------
class CImportSFMV3 : public CSFMBaseImporter
{
	typedef CSFMBaseImporter BaseClass;
public:
	CImportSFMV3( char const *formatName, char const *nextFormatName );

private:
	virtual bool DoFixup( CDmElement *pSourceRoot );


	void FixupElement( CDmElement *pElement );
	// Fixes up all elements
	void BuildList( CDmElement *pElement, CUtlRBTree< CDmElement *, int >& list );
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CImportSFMV3 s_ImportSFMV3( "sfm_v3", "sfm_v4" );

void InstallSFMV3Importer( IDataModel *pFactory )
{
	pFactory->AddLegacyUpdater( &s_ImportSFMV3 );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CImportSFMV3::CImportSFMV3( char const *formatName, char const *nextFormatName ) : 
	BaseClass( formatName, nextFormatName )	
{
}


struct LogToCurveInfoTypeMap_t
{
	const char *pLogType;
	const char *pLogLayerType;
	const char *pCurveInfoType;
};

LogToCurveInfoTypeMap_t g_typeMap[] =
{
	{ "DmeIntLog",			"DmeIntLogLayer",		"DmeIntCurveInfo" },
	{ "DmeFloatLog",		"DmeFloatLogLayer",		"DmeFloatCurveInfo" },
	{ "DmeBoolLog",			"DmeBoolLogLayer",		"DmeBoolCurveInfo" },
	// string,
	// void,
	// objectid,
	{ "DmeColorLog",		"DmeColorLogLayer",		"DmeColorCurveInfo" },
	{ "DmeVector2Log",		"DmeVector2LogLayer",	"DmeVector2CurveInfo" },
	{ "DmeVector3Log",		"DmeVector3LogLayer",	"DmeVector3CurveInfo" },
	{ "DmeVector4Log",		"DmeVector4LogLayer",	"DmeVector4CurveInfo" },
	{ "DmeQAngleLog",		"DmeQAngleLogLayer",	"DmeQAngleCurveInfo" },
	{ "DmeQuaternionLog",	"DmeQuaternionLogLayer","DmeQuaternionCurveInfo" },
	{ "DmeVMatrixLog",		"DmeVMatrixLogLayer",	"DmeVMatrixCurveInfo" },
};

const char *GetCurveInfoTypeFromLogType( const char *pLogType )
{
	int c = ARRAYSIZE( g_typeMap );
	for ( int i = 0; i < c; ++i )
	{
		if ( !Q_stricmp( pLogType, g_typeMap[ i ].pLogType ) )
			return g_typeMap[ i ].pCurveInfoType;
	}
	return NULL;
}

bool IsLogLayerType( const char *pLogLayerType )
{
	int c = ARRAYSIZE( g_typeMap );
	for ( int i = 0; i < c; ++i )
	{
		if ( !Q_stricmp( pLogLayerType, g_typeMap[ i ].pLogLayerType ) )
			return true;
	}
	return false;
}

void MoveAttribute( CDmElement *pFromElement, const char *pFromAttrName, CDmElement *pToElement = NULL, const char *pToAttrName = NULL, DmAttributeType_t toType = AT_UNKNOWN )
{
	if ( !pToAttrName )
	{
		pToAttrName = pFromAttrName;
	}

	if ( pToElement )
	{
		CDmAttribute *pFromAttr = pFromElement->GetAttribute( pFromAttrName );
		const void *pValue = pFromAttr->GetValueUntyped();
		DmAttributeType_t fromType = pFromAttr->GetType();
		if ( toType == AT_UNKNOWN )
		{
			toType = fromType;
		}

		CDmAttribute *pToAttr = pToElement->AddAttribute( pToAttrName, toType );
		if ( !pToAttr )
		{
			Warning( "*** Problem in converter encountered!\n" );
			Warning( "*** Unable to find or add attribute \"%s\" to element \"%s\"!\n", pToAttrName, pToElement->GetName() );
		}
		else if ( fromType != toType )
		{
			Warning( "*** Problem in file encountered!\n" );
			Warning( "*** Element \"%s\" has attribute \"%s\" with an unexpected type!\n", pFromElement->GetName(), pFromAttrName );
		}
		else
		{
			pToAttr->SetValue( toType, pValue );
		}
	}

	pFromElement->RemoveAttribute( pFromAttrName );
}

// Fixes up all elements
//-----------------------------------------------------------------------------
void CImportSFMV3::FixupElement( CDmElement *pElement )
{
	if ( !pElement )
		return;

	const char *pType = pElement->GetTypeString();

	// log layer
	if ( IsLogLayerType( pType ) )
	{
		pElement->RemoveAttribute( "ownerlog" );
		return;
	}

	// log
	const char *pCurveInfoType = GetCurveInfoTypeFromLogType( pType );
	if ( !pCurveInfoType )
		return;

	CDmElement *pCurveInfo = NULL;
	CDmAttribute *pUseCurveTypeAttr = pElement->GetAttribute( "usecurvetypes" );
	if ( pUseCurveTypeAttr && pUseCurveTypeAttr->GetValue<bool>() )
	{
		DmElementHandle_t hElement = g_pDataModel->CreateElement( "curve info", pCurveInfoType, pElement->GetFileId() );
		pCurveInfo = g_pDataModel->GetElement( hElement );
	}
	pElement->RemoveAttribute( "usecurvetypes" );

	MoveAttribute( pElement, "defaultcurvetype",	pCurveInfo, "defaultCurveType",		AT_INT );
	MoveAttribute( pElement, "defaultedgezerovalue",pCurveInfo, "defaultEdgeZeroValue" );
	MoveAttribute( pElement, "useedgeinfo",			pCurveInfo, "useEdgeInfo",			AT_BOOL );
	MoveAttribute( pElement, "rightedgetime",		pCurveInfo, "rightEdgeTime",		AT_INT );
	MoveAttribute( pElement, "left_edge_active",	pCurveInfo, "leftEdgeActive",		AT_BOOL );
	MoveAttribute( pElement, "right_edge_active",	pCurveInfo, "rightEdgeActive",		AT_BOOL );
	MoveAttribute( pElement, "left_edge_curvetype", pCurveInfo, "leftEdgeCurveType",	AT_INT );
	MoveAttribute( pElement, "right_edge_curvetype",pCurveInfo, "rightEdgeCurveType",	AT_INT );
	MoveAttribute( pElement, "left_edge_value",		pCurveInfo, "leftEdgeValue" );
	MoveAttribute( pElement, "right_edge_value",	pCurveInfo, "rightEdgeValue" );
}

// Fixes up all elements
//-----------------------------------------------------------------------------
void CImportSFMV3::BuildList( CDmElement *pElement, CUtlRBTree< CDmElement *, int >& list )
{
	if ( !pElement )
		return;

	if ( list.Find( pElement ) != list.InvalidIndex() )
		return;

	list.Insert( pElement );

	// Descend to bottom of tree, then do fixup coming back up the tree
	for ( CDmAttribute *pAttribute = pElement->FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
	{
		if ( pAttribute->GetType() == AT_ELEMENT )
		{
			CDmElement *pElementAt = pAttribute->GetValueElement<CDmElement>( );
			BuildList( pElementAt, list );
			continue;
		}

		if ( pAttribute->GetType() == AT_ELEMENT_ARRAY )
		{
			CDmrElementArray<> array( pAttribute );
			int nCount = array.Count();
			for ( int i = 0; i < nCount; ++i )
			{
				CDmElement *pChild = array[ i ];
				BuildList( pChild, list );
			}
			continue;
		}
	}
}

bool CImportSFMV3::DoFixup( CDmElement *pSourceRoot )
{
	CUtlRBTree< CDmElement *, int >	fixlist( 0, 0, DefLessFunc( CDmElement * ) );
	BuildList( pSourceRoot, fixlist );
	for ( int i = fixlist.FirstInorder(); i != fixlist.InvalidIndex() ; i = fixlist.NextInorder( i ) )
	{
		// Search and replace in the entire tree!
		FixupElement( fixlist[ i ] );
	}
	return true;
}
