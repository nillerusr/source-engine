//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dmserializers.h"
#include "dmebaseimporter.h"
#include "datamodel/idatamodel.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmelement.h"

#include <math.h>

//-----------------------------------------------------------------------------
// Format converter
//-----------------------------------------------------------------------------
class CImportSFMV1 : public CSFMBaseImporter
{
	typedef CSFMBaseImporter BaseClass;
public:
	CImportSFMV1( char const *formatName, char const *nextFormatName );

private:
	virtual bool DoFixup( CDmElement *pSourceRoot );

	// Fixes up a single time attribute - converting from float seconds to int tenths-of-a-millisecond
	void ConvertTimeAttribute( CDmElement *pElementInternal, const char *pOldName, const char *pNewName );

	// Fixes up a single timeframe
	void FixupTimeframe( CDmElement *pElementInternal );

	// Fixes up a single log - converting from int milliseconds to int tenths-of-a-millisecond
	void FixupLog( CDmElement *pElementInternal );

	CUtlRBTree< CDmElement*, int > m_fixedElements;
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CImportSFMV1 s_ImportDmxV1( "sfm_v1", "sfm_v2" );

void InstallSFMV1Importer( IDataModel *pFactory )
{
	pFactory->AddLegacyUpdater( &s_ImportDmxV1 );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CImportSFMV1::CImportSFMV1( char const *formatName, char const *nextFormatName ) : 
	BaseClass( formatName, nextFormatName )	
{
	m_fixedElements.SetLessFunc( DefLessFunc( CDmElement * ) );
}


//-----------------------------------------------------------------------------
// Fixes up all elements
//-----------------------------------------------------------------------------
bool CImportSFMV1::DoFixup( CDmElement *pElementInternal )
{
	if ( !pElementInternal )
		return true;

	if ( m_fixedElements.Find( pElementInternal ) != m_fixedElements.InvalidIndex() )
		return true;

	m_fixedElements.Insert( pElementInternal );

	const char *pType = pElementInternal->GetTypeString();
	if ( !Q_strcmp( pType, "DmeTimeFrame" ) )
	{
		FixupTimeframe( pElementInternal );
	}
	else if ( !Q_strcmp( pType, "DmeLog" ) ||
			!Q_strcmp( pType, "DmeIntLog" ) ||
			!Q_strcmp( pType, "DmeFloatLog" ) ||
			!Q_strcmp( pType, "DmeBoolLog" ) ||
			!Q_strcmp( pType, "DmeColorLog" ) ||
			!Q_strcmp( pType, "DmeVector2Log" ) ||
			!Q_strcmp( pType, "DmeVector3Log" ) ||
			!Q_strcmp( pType, "DmeVector4Log" ) ||
			!Q_strcmp( pType, "DmeQAngleLog" ) ||
			!Q_strcmp( pType, "DmeQuaternionLog" ) ||
			!Q_strcmp( pType, "DmeVMatrixLog" ) )
	{
		FixupLog( pElementInternal );
	}


	for ( CDmAttribute *pAttribute = pElementInternal->FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
	{
		if ( pAttribute->GetType() == AT_ELEMENT )
		{
			CDmElement *pElement = pAttribute->GetValueElement<CDmElement>( );
			DoFixup( pElement );
			continue;
		}

		if ( pAttribute->GetType() == AT_ELEMENT_ARRAY )
		{
			CDmrElementArray<> array( pAttribute );
			int nCount = array.Count();
			for ( int i = 0; i < nCount; ++i )
			{
				CDmElement *pChild = array[ i ];
				DoFixup( pChild );
			}
			continue;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Fixes up a single time attribute - converting from float seconds to int tenths-of-a-millisecond
//-----------------------------------------------------------------------------
void CImportSFMV1::ConvertTimeAttribute( CDmElement *pElementInternal, const char *pOldName, const char *pNewName )
{
	float time = 0.0f;
	CDmAttribute *pOldAttr = pElementInternal->GetAttribute( pOldName );
	if ( !pOldAttr )
	{
		Warning( "*** Problem in file encountered!\n" );
		Warning( "*** TimeFrame \"%s\" is missing attribute \"%s\"!\n", pElementInternal->GetName(), pOldName );
		Warning( "*** Setting new attribute \"%s\" to 0\n", pNewName );
	}
	else if ( pOldAttr->GetType() != AT_FLOAT )
	{
		Warning( "*** Problem in file encountered!\n" );
		Warning( "*** TimeFrame \"%s\" has attribute \"%s\" with an unexpected type (expected float)!\n", pElementInternal->GetName(), pOldName );
	}
	else
	{
		time = pOldAttr->GetValue< float >();
		pElementInternal->RemoveAttribute( pOldName );
	}

	CDmAttribute *pNewAttr = NULL;

	// this is disabled because even dmxconvert installs *some* movieobjects factories, when it probably shouldn't
	// the method of installing movieobjects factories will change at some point in the future, and we can turn on this safety check then
#if 0
	int i = g_pDataModel->GetFirstFactory();
	if ( g_pDataModel->IsValidFactory( i ) )
	{
		// factories installed - most likely from within movieobjects.lib
		// ie there may be different ways of allocating attributes, so it's not safe to add them here
		pNewAttr = pElementInternal->GetAttribute( pNewName );
		if ( !pNewAttr || pNewAttr->GetType() != AT_INT )
		{
			Assert( 0 );
			Warning( "*** Converter error - expected element \"%s\" to contain int attribute \"%s\"!\n", pElementInternal->GetName(), pNewName );
			Warning( "***                 - if you get this error, the converter is out of sync with the element library!\n" );
			return;
		}
	}
	else
	{
#endif
		// no factories installed - most likely from within dmxconvert.exe
		// ie we're just working with CDmElement subclasses, so it's safe to add attributes
		pNewAttr = pElementInternal->AddAttribute( pNewName, AT_INT );
		if ( !pNewAttr )
		{
			Assert( 0 );
			Warning( "*** Converter error - element \"%s\" already has a non-int attribute \"%s\"!\n", pElementInternal->GetName(), pNewName );
			return;
		}
#if 0
	}
#endif

	pNewAttr->SetValue< int >( floor( time * 10000 + 0.5f ) );
}

//-----------------------------------------------------------------------------
// Fixes up a single timeframe
//-----------------------------------------------------------------------------
void CImportSFMV1::FixupTimeframe( CDmElement *pElementInternal )
{
	ConvertTimeAttribute( pElementInternal, "start", "startTime" );
	ConvertTimeAttribute( pElementInternal, "duration", "durationTime" );
	ConvertTimeAttribute( pElementInternal, "offset", "offsetTime" );
}

//-----------------------------------------------------------------------------
// Fixes up a single log - converting from int milliseconds to int tenths-of-a-millisecond
//-----------------------------------------------------------------------------
void CImportSFMV1::FixupLog( CDmElement *pElementInternal )
{
	CDmAttribute *pAttr = pElementInternal->GetAttribute( "times" );
	if ( !pAttr )
	{
		Warning( "*** Problem in file encountered!\n" );
		Warning( "*** Log \"%s\" is missing attribute \"%s\"!\n", pElementInternal->GetName(), "times" );
		return;
	}

	if ( pAttr->GetType() != AT_INT_ARRAY )
	{
		Warning( "*** Problem in file encountered!\n" );
		Warning( "*** Log \"%s\" has attribute \"%s\" with an unexpected type (expected int array)!\n", pElementInternal->GetName(), "times" );
		return;
	}

	CDmrArray<int> array( pAttr );
	int c = array.Count();
	for ( int i = 0; i < c; ++i )
	{
		// convert all log times from int milliseconds to int tenths-of-a-millisecond
		array.Set( i, 10 * array[i] );
	}
}
