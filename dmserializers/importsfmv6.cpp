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
class CImportSFMV6 : public CSFMBaseImporter
{
	typedef CSFMBaseImporter BaseClass;
public:
	CImportSFMV6( char const *formatName, char const *nextFormatName );

private:
	virtual bool DoFixup( CDmElement *pSourceRoot );

	Quaternion DirectionToOrientation( const Vector &dir );

	void FixupElement( CDmElement *pElement );
	// Fixes up all elements
	void BuildList( CDmElement *pElement, CUtlRBTree< CDmElement *, int >& list );
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CImportSFMV6 s_ImportSFMV6( "sfm_v6", "sfm_v7" );

void InstallSFMV6Importer( IDataModel *pFactory )
{
	pFactory->AddLegacyUpdater( &s_ImportSFMV6 );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CImportSFMV6::CImportSFMV6( char const *formatName, char const *nextFormatName ) : 
	BaseClass( formatName, nextFormatName )	
{
}

Quaternion CImportSFMV6::DirectionToOrientation( const Vector &dir )
{
	Vector up( 0, 0, 1 );
	Vector right = CrossProduct( dir, up );
	if ( right.IsLengthLessThan( 0.001f ) )
	{
		up.Init( 1, 0, 0 );
		right = CrossProduct( dir, up );
	}
	right.NormalizeInPlace();
	up = CrossProduct( right, dir );

	Quaternion q;
	BasisToQuaternion( dir, right, up, q );
	return q;
}

//-----------------------------------------------------------------------------
// Fixes up all elements
//-----------------------------------------------------------------------------
void CImportSFMV6::FixupElement( CDmElement *pElement )
{
	if ( !pElement )
		return;

	const char *pType = pElement->GetTypeString();

	if ( !V_stricmp( pType, "DmeProjectedLight" ) )
	{
		Vector vDir = pElement->GetValue<Vector>( "direction" );
		pElement->RemoveAttribute( "direction" );
		Quaternion q = DirectionToOrientation( vDir );
		pElement->SetValue<Quaternion>( "orientation", q );
	}
}


//-----------------------------------------------------------------------------
// Fixes up all elements
//-----------------------------------------------------------------------------
void CImportSFMV6::BuildList( CDmElement *pElement, CUtlRBTree< CDmElement *, int >& list )
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

bool CImportSFMV6::DoFixup( CDmElement *pSourceRoot )
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
