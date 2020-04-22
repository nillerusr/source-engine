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
class CImportSFMV8 : public CSFMBaseImporter
{
	typedef CSFMBaseImporter BaseClass;
public:
	CImportSFMV8( const char *formatName, const char *nextFormatName );

private:
	virtual bool DoFixup( CDmElement *pSourceRoot );


	void FixupElement( CDmElement *pElement );
	// Fixes up all elements
	void BuildList( CDmElement *pElement, CUtlRBTree< CDmElement *, int >& list );
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CImportSFMV8 s_ImportSFMV8( "sfm_v8", "sfm_v9" );

void InstallSFMV8Importer( IDataModel *pFactory )
{
	pFactory->AddLegacyUpdater( &s_ImportSFMV8 );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CImportSFMV8::CImportSFMV8( const char *formatName, const char *nextFormatName ) : 
	BaseClass( formatName, nextFormatName )	
{
}


//-----------------------------------------------------------------------------
// Fixes up all elements
//-----------------------------------------------------------------------------
void CImportSFMV8::FixupElement( CDmElement *pElement )
{
	if ( !pElement )
		return;

	const char *pType = pElement->GetTypeString();

	if ( !V_stricmp( pType, "DmeAnimationSet" ) )
	{
		// Remove 'midpoint' from all controls, and 
		// Add 'defaultBalance' and 'defaultMultilevel' to all non-transform controls
		CDmrElementArray<> srcControls( pElement, "controls" );
		if ( srcControls.IsValid() )
		{
			int nCount = srcControls.Count();
			for ( int i = 0; i < nCount; ++i )
			{
				CDmElement *pControl = srcControls[i];
				if ( pControl )
				{
					if ( !pControl->GetValue<bool>( "transform" ) )
					{
						pControl->InitValue( "defaultBalance", 0.5f );
						pControl->InitValue( "defaultMultilevel", 0.5f );
						pControl->RemoveAttribute( "midpoint" );
					}
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Fixes up all elements
//-----------------------------------------------------------------------------
void CImportSFMV8::BuildList( CDmElement *pElement, CUtlRBTree< CDmElement *, int >& list )
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

bool CImportSFMV8::DoFixup( CDmElement *pSourceRoot )
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
