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
class CImportSFMV4 : public CSFMBaseImporter
{
	typedef CSFMBaseImporter BaseClass;
public:
	CImportSFMV4( char const *formatName, char const *nextFormatName );

private:
	virtual bool DoFixup( CDmElement *pSourceRoot );


	void FixupElement( CDmElement *pElement );
	// Fixes up all elements
	void BuildList( CDmElement *pElement, CUtlRBTree< CDmElement *, int >& list );
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CImportSFMV4 s_ImportSFMV4( "sfm_v4", "sfm_v5" );

void InstallSFMV4Importer( IDataModel *pFactory )
{
	pFactory->AddLegacyUpdater( &s_ImportSFMV4 );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CImportSFMV4::CImportSFMV4( char const *formatName, char const *nextFormatName ) : 
	BaseClass( formatName, nextFormatName )	
{
}

// Fixes up all elements
//-----------------------------------------------------------------------------
void CImportSFMV4::FixupElement( CDmElement *pElement )
{
	if ( !pElement )
		return;

	const char *pType = pElement->GetTypeString();

	if ( !V_stricmp( pType, "DmeCamera" ) )
	{
		CDmAttribute *pOldToneMapScaleAttr = pElement->GetAttribute( "toneMapScale" );
		float fNewBloomScale = pOldToneMapScaleAttr->GetValue<float>( );

		Assert( !pElement->HasAttribute("bloomScale") );

		CDmAttribute *pNewBloomScaleAttr = pElement->AddAttribute( "bloomScale", AT_FLOAT );
		pNewBloomScaleAttr->SetValue( fNewBloomScale );
		pOldToneMapScaleAttr->SetValue( 1.0f );
	}
}

// Fixes up all elements
//-----------------------------------------------------------------------------
void CImportSFMV4::BuildList( CDmElement *pElement, CUtlRBTree< CDmElement *, int >& list )
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

bool CImportSFMV4::DoFixup( CDmElement *pSourceRoot )
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
