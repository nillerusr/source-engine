//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: fixed "color" attribute of lights to be of type Color, rather than Vector4
// this should have been put in a *long* time ago, but I somehow missed creating the updater between 3 and 4
// fortunately, since all updates happen on untyped elements, it's reasonably safe to do this out of order
//
//=============================================================================

#include "dmserializers.h"
#include "dmebaseimporter.h"
#include "datamodel/idatamodel.h"
#include "datamodel/dmelement.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlmap.h"
#include <limits.h>


//-----------------------------------------------------------------------------
// Format converter
//-----------------------------------------------------------------------------
class CImportSFMV9 : public CSFMBaseImporter
{
	typedef CSFMBaseImporter BaseClass;
public:
	CImportSFMV9( const char *formatName, const char *nextFormatName );

private:
	virtual bool DoFixup( CDmElement *pSourceRoot );


	void FixupElement( CDmElement *pElement );
	// Fixes up all elements
	void BuildList( CDmElement *pElement, CUtlRBTree< CDmElement *, int >& list );
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CImportSFMV9 s_ImportSFMV9( "sfm_v9", "sfm_v10" );

void InstallSFMV9Importer( IDataModel *pFactory )
{
	pFactory->AddLegacyUpdater( &s_ImportSFMV9 );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CImportSFMV9::CImportSFMV9( const char *formatName, const char *nextFormatName ) : 
BaseClass( formatName, nextFormatName )	
{
}


//-----------------------------------------------------------------------------
// Fixes up all elements
//-----------------------------------------------------------------------------
void CImportSFMV9::FixupElement( CDmElement *pElement )
{
	if ( !pElement )
		return;

	const char *pType = pElement->GetTypeString();
	if ( !V_stricmp( pType, "DmeLight" ) ||
		!V_stricmp( pType, "DmeDirectionalLight" ) ||
		!V_stricmp( pType, "DmeProjectedLight" ) ||
		!V_stricmp( pType, "DmePointLight" ) ||
		!V_stricmp( pType, "DmeSpotLight" ) ||
		!V_stricmp( pType, "DmeAmbientLight" ) )
	{
		const CDmAttribute *pOldAttr = pElement->GetAttribute( "color", AT_VECTOR4 );
		if ( !pOldAttr )
			return;

		Color color;

		{ // scoping this section of code since vecColor is invalid after RemoveAttribute
			const Vector4D &vecColor = pOldAttr->GetValue< Vector4D >();
			for ( int i = 0; i < 4; ++i )
			{
				color[ i ] = ( int )clamp( vecColor[ i ], 0.0f, 255.0f );
			}

			pElement->RemoveAttribute( "color" );
		}

		CDmAttribute *pNewAttr = pElement->AddAttribute( "color", AT_COLOR );
		pNewAttr->SetValue( color );
	}
}


//-----------------------------------------------------------------------------
// Fixes up all elements
//-----------------------------------------------------------------------------
void CImportSFMV9::BuildList( CDmElement *pElement, CUtlRBTree< CDmElement *, int >& list )
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

bool CImportSFMV9::DoFixup( CDmElement *pSourceRoot )
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
