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
class CImportSFMV2 : public CSFMBaseImporter
{
	typedef CSFMBaseImporter BaseClass;
public:
	CImportSFMV2( char const *formatName, char const *nextFormatName );

private:
	virtual bool DoFixup( CDmElement *pSourceRoot );


	void FixupElement( CDmElement *pElement );
	// Fixes up all elements
	void BuildList( CDmElement *pElement, CUtlRBTree< CDmElement *, int >& list );
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CImportSFMV2 s_ImportSFMV2( "sfm_v2", "sfm_v3" );

void InstallSFMV2Importer( IDataModel *pFactory )
{
	pFactory->AddLegacyUpdater( &s_ImportSFMV2 );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CImportSFMV2::CImportSFMV2( char const *formatName, char const *nextFormatName ) : 
	BaseClass( formatName, nextFormatName )	
{
}


struct LayerType_t
{
	char const *loglayertype;
	int datatype;
	char const *logtype;
};

static LayerType_t g_LayerTypes[] = 
{
	{ "DmeIntLogLayer",	AT_INT_ARRAY, "DmeIntLog" },
	{ "DmeFloatLogLayer", AT_FLOAT_ARRAY, "DmeFloatLog" },
	{ "DmeBoolLogLayer", AT_BOOL_ARRAY, "DmeBoolLog" },
	// AT_STRING_ARRAY,
	// AT_VOID_ARRAY,
	// AT_OBJECTID_ARRAY,
	{ "DmeColorLogLayer", AT_COLOR_ARRAY, "DmeColorLog" },
	{ "DmeVector2LogLayer", AT_VECTOR2_ARRAY, "DmeVector2Log" },
	{ "DmeVector3LogLayer", AT_VECTOR3_ARRAY, "DmeVector3Log" },
	{ "DmeVector4LogLayer", AT_VECTOR4_ARRAY, "DmeVector4Log" },
	{ "DmeQAngleLogLayer", AT_QANGLE_ARRAY, "DmeQAngleLog" },
	{ "DmeQuaternionLogLayer", AT_QUATERNION_ARRAY, "DmeQuaternionLog" },
	{ "DmeVMatrixLogLayer", AT_VMATRIX_ARRAY, "DmeVMatrixLog" },
	// AT_ELEMENT_ARRAY
	// NO ARRAY TYPES EITHER!!!
};

int GetLogType( char const *type )
{
	int c = ARRAYSIZE( g_LayerTypes );
	for ( int i = 0; i < c; ++i )
	{
		if ( !Q_stricmp( type, g_LayerTypes[ i ].logtype ) )
			return g_LayerTypes[ i ].datatype;
	}
	return AT_UNKNOWN;
}

char const *GetLogLayerType( int nDataType )
{
	int c = ARRAYSIZE( g_LayerTypes );
	for ( int i = 0; i < c; ++i )
	{
		if ( nDataType == g_LayerTypes[ i ].datatype )
			return g_LayerTypes[ i ].loglayertype;
	}
	return NULL;
}

char const *GetLogLayerType( char const *logType )
{
	int c = ARRAYSIZE( g_LayerTypes );
	for ( int i = 0; i < c; ++i )
	{
		if ( !Q_stricmp( logType, g_LayerTypes[ i ].logtype ) )
			return g_LayerTypes[ i ].loglayertype;
	}
	return NULL;
}

template< class T >
void CopyValues( int layerType, CDmElement *pElement, CDmElement *pLayer, CDmAttribute *pInTimeAttribute, CDmAttribute *pInCurveTypeAttribute )
{
	CDmAttribute *pInValueAttribute = pElement->GetAttribute( "values" );
	if ( !pInValueAttribute )
	{
		Assert( 0 );
		return;
	}

	CDmrArray<T> outValues( pLayer->AddAttribute( "values", (DmAttributeType_t)layerType ) );
	CDmrArray<int> outTimes( pLayer->AddAttribute( "times", AT_INT_ARRAY ) );
	CDmrArray<int> outCurveTypes;
	if ( pInCurveTypeAttribute )
	{
		outCurveTypes.Init( pLayer->AddAttribute( "curvetypes", AT_INT_ARRAY ) );
	}

	CDmrArray<T> inValues( pInValueAttribute );
	CDmrArray<int> inTimes( pInTimeAttribute );
	CDmrArray<int> inCurveTypes( pInCurveTypeAttribute );

	Assert( inValues.Count() == inTimes.Count() );
	int c = inValues.Count();
	for ( int i = 0; i < c; ++i )
	{
		outTimes.AddToTail( inTimes[ i ] );
		outValues.AddToTail( inValues[ i ] );
		if ( outCurveTypes.IsValid() )
		{
			outCurveTypes.AddToTail( inCurveTypes[ i ] );
		}
	}
}


//-----------------------------------------------------------------------------
// Fixes up all elements
//-----------------------------------------------------------------------------
void CImportSFMV2::FixupElement( CDmElement *pElement )
{
	if ( !pElement )
		return;

	// Perform the fixup
	const char *pType = pElement->GetTypeString();
	int layerType = GetLogType( pType );
	if ( layerType != AT_UNKNOWN )
	{
		/*
		char buf[ 128 ];
		g_pDataModel->ToString( pElement->GetId(), buf, sizeof( buf ) );

		Msg( "Processing %s %s id %s\n",
			pElement->GetTypeString(), pElement->GetName(), buf );
		*/

		// Find attribute arrays for times, values and curvetypes
		CDmAttribute *pTimes = pElement->GetAttribute( "times" );
		CDmAttribute *pCurveTypes = NULL;

		// FIX
		CDmAttribute *pAttr = pElement->AddAttribute( "usecurvetypes", AT_BOOL );
		if ( pAttr->GetValue<bool>() )
		{
			pCurveTypes = pElement->GetAttribute( "curvetypes" );
		}

		// Get the default layer (added when the new style log is created)
		CDmrElementArray<> layers( pElement->AddAttribute( "layers", AT_ELEMENT_ARRAY ) );
		CDmElement *layer = NULL;
		if ( layers.Count() == 0 )
		{
			DmElementHandle_t hElement = g_pDataModel->CreateElement( GetLogLayerType( layerType ), GetLogLayerType( layerType ), pElement->GetFileId() );
			layer = g_pDataModel->GetElement( hElement );
			layers.AddToTail( layer );
		}
		else
		{
			Assert( layers.Count() == 1 );
			layer = layers[ 0 ];
		}

		// Copy data
		switch ( layerType )
		{
		default:
		case AT_UNKNOWN:
			break;
		case AT_FLOAT_ARRAY:
			CopyValues< float >( layerType, pElement, layer, pTimes, pCurveTypes );
			break;
		case AT_INT_ARRAY:
			CopyValues< int >( layerType, pElement, layer, pTimes, pCurveTypes );
			break;
		case AT_BOOL_ARRAY:
			CopyValues< bool >( layerType, pElement, layer, pTimes, pCurveTypes );
			break;
		case AT_COLOR_ARRAY:
			CopyValues< Color >( layerType, pElement, layer, pTimes, pCurveTypes );
			break;
		case AT_VECTOR2_ARRAY:
			CopyValues< Vector2D >( layerType, pElement, layer, pTimes, pCurveTypes );
			break;
		case AT_VECTOR3_ARRAY:
			CopyValues< Vector >( layerType, pElement, layer, pTimes, pCurveTypes );
			break;
		case AT_VECTOR4_ARRAY:
			CopyValues< Vector4D >( layerType, pElement, layer, pTimes, pCurveTypes );
			break;
		case AT_QANGLE_ARRAY:
			CopyValues< QAngle >( layerType, pElement, layer, pTimes, pCurveTypes );
			break;
		case AT_QUATERNION_ARRAY:
			CopyValues< Quaternion >( layerType, pElement, layer, pTimes, pCurveTypes );
			break;
		case AT_VMATRIX_ARRAY:
			CopyValues< VMatrix >( layerType, pElement, layer, pTimes, pCurveTypes );
			break;
		}

		// Set the back pointer
		CDmAttribute *ownerLog = layer->AddAttribute( "ownerlog", AT_ELEMENT );
		Assert( ownerLog );
		ownerLog->SetValue( pElement->GetHandle() );

		// Delete the base attributes
		pElement->RemoveAttribute( "times" );
		pElement->RemoveAttribute( "values" );
		pElement->RemoveAttribute( "curvetypes" );
	}
}

// Fixes up all elements
//-----------------------------------------------------------------------------
void CImportSFMV2::BuildList( CDmElement *pElement, CUtlRBTree< CDmElement *, int >& list )
{
	if ( !pElement )
		return;

	if ( list.Find( pElement ) != list.InvalidIndex() )
		return;

	list.Insert( pElement );

	// Descene to bottom of tree, then do fixup coming back up the tree
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

bool CImportSFMV2::DoFixup( CDmElement *pSourceRoot )
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
