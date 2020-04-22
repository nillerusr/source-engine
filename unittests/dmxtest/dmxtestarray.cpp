//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Unit test program for DMX testing (testing the Array operations)
//
// $NoKeywords: $
//=============================================================================//

#include "unitlib/unitlib.h"
#include "datamodel/dmelement.h"
#include "datamodel/idatamodel.h"
#include "tier1/utlbuffer.h"
#include "filesystem.h"
#include "datamodel/dmehandle.h"
#include "tier2/tier2.h"
#include "movieobjects/dmeshape.h"


DEFINE_TESTCASE_NOSUITE( DmxArrayTest )
{
	Msg( "Running dmx array tests...\n" );

	CDisableUndoScopeGuard sg;
	DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( "<RunArrayTests>" );

	CDmElement *pElement = CreateElement< CDmElement >( "root", fileid );

	CDmElement *pElement2 = CreateElement<CDmElement>( "element1", fileid );
	Assert( pElement2 );
	CDmElement *pElement3 = CreateElement<CDmElement>( "element2", fileid );
	Assert( pElement3 );
	CDmeShape *pElement4 = CreateElement<CDmeShape>( "shape", fileid );
	Assert( pElement4 );

	CDmrStringArray stringVec( pElement, "string_array_test", true );
	stringVec.AddToTail( "string1" );
	stringVec.AddToTail( "string2" );
	stringVec.AddToTail( "string3" );

	CDmrArray< float > floatVec( pElement, "float_array_test", true );
	floatVec.AddToTail( -1.0f );
	floatVec.AddToTail( 0.0f );
	floatVec.AddToTail( 1.0f );

	CDmrElementArray< > elementVec( pElement, "element_array_test", true );
	elementVec.AddToTail( pElement2 );
	elementVec.AddToTail( pElement3 );
	elementVec.AddToTail( pElement4 );

	CDmrStringArray stringVec2( pElement, "string_array_test2", true );
	stringVec2 = stringVec;
	Shipping_Assert( stringVec2.Count() == 3 );
			 
	CDmrArray< float > floatVec2( pElement, "float_array_test2", true );
	floatVec2 = floatVec;
	Shipping_Assert( floatVec2.Count() == 3 );

	CDmrElementArray< > elementVec2( pElement, "element_array_test2", true );
	elementVec2 = elementVec;
	Shipping_Assert( elementVec2.Count() == 3 );

	CDmrElementArray< CDmeShape > elementVec3( pElement, "element_array_test3", true );
	elementVec3 = elementVec2;
	Shipping_Assert( elementVec3.Count() == 1 );

	CUtlVector<DmElementHandle_t> val;
	val.AddToTail( pElement2->GetHandle() );
	val.AddToTail( pElement4->GetHandle() );

	elementVec2 = val;
	Shipping_Assert( elementVec2.Count() == 2 );

	elementVec3 = val;
	Shipping_Assert( elementVec3.Count() == 1 );

	g_pDataModel->RemoveFileId( fileid );
}
