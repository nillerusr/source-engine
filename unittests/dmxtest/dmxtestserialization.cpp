//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Unit test program  for DMX testing
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

bool AssertEqualElementHierarchies( bool quiet, DmElementHandle_t src1, DmElementHandle_t src2 );
bool AssertUnEqualElementHierarchies( DmElementHandle_t src1, DmElementHandle_t src2 )
{
	bool equal = AssertEqualElementHierarchies( true, src1, src2 );
	if ( equal )
	{
		AssertMsg( 0, "Hierarchies equal, expecting mismatch\n" );
	}
	return !equal;
}


void CreateTestScene( CUtlVector< DmElementHandle_t >& handles, DmFileId_t fileid )
{
	DmObjectId_t id;
	CreateUniqueId( &id );

	VMatrix mat, mat2;
	MatrixBuildRotateZ( mat, 45 );
	MatrixBuildRotateZ( mat2, 30 );

	int i;
	unsigned char buf[256];
	unsigned char buf2[256];
	for ( i = 0; i < 256; ++i )
	{
		buf[i] = i;
		buf2[i] = 255 - i;
	}

	CDmElement *pElement = CreateElement<CDmElement>( "root", fileid );
	Assert( pElement );
	CDmElement *pElement2 = CreateElement<CDmElement>( "shared_child", fileid );
	Assert( pElement2 );
	CDmElement *pElement3 = CreateElement<CDmElement>( "unique_child", fileid );
	Assert( pElement3 );
	CDmElement *pElement4 = CreateElement<CDmElement>( "shared_array_element", fileid );
	Assert( pElement4 );
	CDmElement *pElement5 = CreateElement<CDmElement>( "unique_array_element", fileid );
	Assert( pElement5 );
	CDmElement *pElement6 = CreateElement<CDmElement>( "shared_element", fileid );
	Assert( pElement6 );
	CDmElement *pElement7 = CreateElement<CDmElement>( "unique_element", fileid );
	Assert( pElement7 );

	g_pDataModel->SetFileRoot( fileid, pElement->GetHandle() );

	handles.AddToTail( pElement->GetHandle() );
	handles.AddToTail( pElement2->GetHandle() );
	handles.AddToTail( pElement3->GetHandle() );
	handles.AddToTail( pElement4->GetHandle() );
	handles.AddToTail( pElement5->GetHandle() );
	handles.AddToTail( pElement6->GetHandle() );
	handles.AddToTail( pElement7->GetHandle() );

	pElement->SetValue( "id_test", id );
	pElement->SetValue( "bool_test", true );
	pElement->SetValue( "int_test", 2 );
	pElement->SetValue( "float_test", 3.0f );
	pElement->SetValue( "color_test", Color( 0, 64, 128, 255 ) );
	pElement->SetValue( "vector2d_test", Vector2D( 1.0f, -1.0f ) );
	pElement->SetValue( "vector3d_test", Vector( 1.0f, -1.0f, 0.0f ) );
	pElement->SetValue( "vector4d_test", Vector4D( 1.0f, -1.0f, 0.0f, 2.0f ) );
	pElement->SetValue( "qangle_test", QAngle( 0.0f, 90.0f, -90.0f ) );
	pElement->SetValue( "quat_test", Quaternion( 1.0f, -1.0f, 0.0f, 2.0f ) );
	pElement->SetValue( "vmatrix_test", mat );
	pElement->SetValue( "string_test", "test" );
	pElement->SetValue( "binary_test", buf, 256 );

	// Test DONTSAVE
//	pElement->SetValue( "dontsave", true );
//	CDmAttribute *pAttribute = pElement->GetAttribute( "dontsave" );
//	pAttribute->AddFlag( FATTRIB_DONTSAVE );

	CDmrArray< bool > boolVec( pElement2, "bool_array_test", true );
	boolVec.AddToTail( false );
	boolVec.AddToTail( true );

	CDmrArray< int > intVec( pElement2, "int_array_test", true );
	intVec.AddToTail( 0 );
	intVec.AddToTail( 1 );
	intVec.AddToTail( 2 );

	CDmrArray< float > floatVec( pElement2, "float_array_test", true );
	floatVec.AddToTail( -1.0f );
	floatVec.AddToTail( 0.0f );
	floatVec.AddToTail( 1.0f );

	CDmrArray< Color > colorVec( pElement3, "color_array_test", true );
	colorVec.AddToTail( Color( 0, 0, 0, 255 ) );
	colorVec.AddToTail( Color( 64, 64, 64, 255 ) );
	colorVec.AddToTail( Color( 128, 128, 128, 255 ) );

	CDmrArray< Vector2D > vector2DVec( pElement3, "vector2d_array_test", true );
	vector2DVec.AddToTail( Vector2D( -1.0f, -1.0f ) );
	vector2DVec.AddToTail( Vector2D( 1.0f, 1.0f ) );

	CDmrArray< Vector > vector3DVec( pElement3, "vector3d_array_test", true );
	vector3DVec.AddToTail( Vector( 1.0f, -1.0f, 0.0f ) );
	vector3DVec.AddToTail( Vector( 2.0f, -2.0f, 0.0f ) );

	CDmrArray< Vector4D > vector4DVec( pElement4, "vector4d_array_test", true );
	vector4DVec.AddToTail( Vector4D( 1.0f, -1.0f, 0.0f, 2.0f ) );
	vector4DVec.AddToTail( Vector4D( 2.0f, -2.0f, 0.0f, 4.0f ) );

	CDmrArray< QAngle > angleVec( pElement4, "qangle_array_test", true );
	angleVec.AddToTail( QAngle( 1.0f, -1.0f, 0.0f ) );
	angleVec.AddToTail( QAngle( 2.0f, -2.0f, 0.0f ) );

	CDmrArray< Quaternion > quatVec( pElement4, "quat_array_test", true );
	quatVec.AddToTail( Quaternion( 1.0f, -1.0f, 0.0f, 2.0f ) );
	quatVec.AddToTail( Quaternion( 2.0f, -2.0f, 0.0f, 4.0f ) );

	CDmrArray< VMatrix > matVec( pElement5, "vmatrix_array_test", true );
	matVec.AddToTail( mat );
	matVec.AddToTail( mat2 );

	CDmrStringArray stringVec( pElement5, "string_array_test", true );
	stringVec.AddToTail( "string1" );
	stringVec.AddToTail( "string2" );
	stringVec.AddToTail( "string3" );

	CDmrArray< CUtlBinaryBlock > binaryVec( pElement5, "binary_array_test", true );
	CUtlBinaryBlock block( (const void *)buf, 256 );
	i = binaryVec.AddToTail( block );
	CUtlBinaryBlock block2( (const void *)buf2, 256 );
	i = binaryVec.AddToTail( block2);

	CDmrArray< DmObjectId_t > idVec( pElement6, "elementid_array_test", true );
	i = idVec.AddToTail( pElement6->GetId() );
	i = idVec.AddToTail( pElement5->GetId() );
	i = idVec.AddToTail( pElement4->GetId() );

	CDmrElementArray< > elementVec( pElement6, "element_array_test", true );
	elementVec.AddToTail( pElement4 );
	elementVec.AddToTail( pElement5 );

	CDmrElementArray< > elementVec2( pElement7, "element_array_test", true );
	elementVec2.AddToTail( pElement2 );
	elementVec2.AddToTail( pElement4 );
	
	pElement->SetValue( "element_test", pElement7 );
	pElement->SetValue( "shared_element_test", pElement6 );
	CDmrElementArray<> children( pElement, "children", true );
	children.InsertBefore( 0, pElement2 );
	children.InsertBefore( 1, pElement3 );

	pElement7->SetValue( "shared_element_test", pElement6 );
	CDmrElementArray<> children2( pElement7, "children", true );
	children2.InsertBefore( 0, pElement2 );
}

DmElementHandle_t CreateTestScene( DmFileId_t fileid )
{
	CUtlVector< DmElementHandle_t > handles;
	CreateTestScene( handles, fileid );
	return handles[ 0 ]; 
}

DmElementHandle_t CreateKeyValuesTestScene( DmFileId_t fileid )
{
	CDmElement *pElement = CreateElement<CDmElement>( "root", fileid );
	Assert( pElement );
	CDmElement *pElement2 = CreateElement<CDmElement>( "shared_child", fileid );
	Assert( pElement2 );
	CDmElement *pElement3 = CreateElement<CDmElement>( "unique_child", fileid );
	Assert( pElement3 );
	CDmElement *pElement4 = CreateElement<CDmElement>( "shared_array_element", fileid );
	Assert( pElement4 );
	CDmElement *pElement5 = CreateElement<CDmElement>( "unique_array_element", fileid );
	Assert( pElement5 );
	CDmElement *pElement6 = CreateElement<CDmElement>( "shared_element", fileid );
	Assert( pElement6 );
	CDmElement *pElement7 = CreateElement<CDmElement>( "unique_element", fileid );
	Assert( pElement7 );

	g_pDataModel->SetFileRoot( fileid, pElement->GetHandle() );

	pElement->SetValue( "int_test", 2 );
	pElement->SetValue( "float_test", 3.0f );
	pElement->SetValue( "string_test", "test" );

	CDmrElementArray<> eVec( pElement6, "element_array_test", true );
	eVec.AddToTail( pElement4 );
	eVec.AddToTail( pElement5 );

	CDmrElementArray<> eVec2( pElement7, "element_array_test", true );
	eVec2.AddToTail( pElement2 );
	eVec2.AddToTail( pElement4 );

	pElement->SetValue( "element_test", pElement7 );
	pElement->SetValue( "shared_element_test", pElement6 );
	CDmrElementArray<> children( pElement, "children", true );
	children.InsertBefore( 0, pElement2 );
	children.InsertBefore( 1, pElement3 );

	pElement7->SetValue( "shared_element_test", pElement6 );
	CDmrElementArray<> children2( pElement7, "children", true );
	children2.InsertBefore( 0, pElement2 );

	return pElement->GetHandle();
}

template< class T >
bool AssertEqualsTest( bool quiet, const T& src1, const T& src2 )
{
	if ( !( src1 == src2 ))
	{
		if ( !quiet )
		{
			AssertMsg( 0, "Results not equal, expecting equal\n" );
		}
		return false;
	}
	return true;
}

template< class T >
bool AssertEqualsUtlVector( bool quiet, const CUtlVector<T> &src1, const CUtlVector<T> &src2 )
{
	bool retval = true;
	if ( src1.Count() != src2.Count() )
	{
		if ( !quiet )
		{
			AssertEqualsTest( quiet, src1.Count(), src2.Count() );
		}
		retval = false;
	}
	
	for ( int i = 0; i < src1.Count(); ++i )
	{
		if ( !src2.IsValidIndex( i ) )
			continue;

		if ( !( src1[i] == src2[i] ) )
		{
			if ( !quiet )
			{
				AssertEqualsTest( quiet, src1[i], src2[i] );
			}
			retval = false;
		}
	}
	return retval;
}

template< class T >
bool AssertEqualsUtlVector( bool quiet, CDmAttribute *pAttribute1, CDmAttribute *pAttribute2 )
{
	CDmrArray<T> src1( pAttribute1 );
	CDmrArray<T> src2( pAttribute2 );
	return AssertEqualsUtlVector( quiet, src1.Get(), src2.Get() );
}

bool AssertEqualAttributes( bool quiet, CDmAttribute *pAttribute1, CDmAttribute *pAttribute2 )
{
	// Always follow ptrs to elements...
	if ( pAttribute1->GetType() != AT_ELEMENT_ARRAY &&
		 pAttribute1->GetType() != AT_ELEMENT )
	{
		// Dirty flag checking here is to avoid infinite recursive loops
		if ( !pAttribute1->IsFlagSet( FATTRIB_DIRTY ) && !pAttribute2->IsFlagSet( FATTRIB_DIRTY ) )
			return true;
	}

	if ( !pAttribute1 )
	{
		if ( !quiet )
		{
			AssertMsg( 0, "AssertEqualAttributes:  pAttribute1 is NULL\n" );
		}
        return false;
	}


	if ( !pAttribute2 )
	{
		if ( !quiet )
		{
			AssertMsg( 0, "AssertEqualAttributes:  pAttribute2 is NULL\n" );
		}
		return false;
	}

	bool retval = true;

	pAttribute1->RemoveFlag( FATTRIB_DIRTY );
	pAttribute2->RemoveFlag( FATTRIB_DIRTY );

	if ( pAttribute1->GetType() != pAttribute2->GetType() )
	{
		if ( !quiet )
		{
			AssertMsg( 0, "pAttribute1->GetType() == pAttribute2->GetType()" );
		}
		retval = false;
	}

	switch( pAttribute1->GetType() )
	{
	case AT_INT:
		return AssertEqualsTest( quiet, pAttribute1->GetValue<int>(  ), pAttribute2->GetValue<int>( ) );

	case AT_FLOAT:
		return AssertEqualsTest( quiet, pAttribute1->GetValue<float>( ), pAttribute2->GetValue<float>( ) );

	case AT_BOOL:
		return AssertEqualsTest( quiet, pAttribute1->GetValue<bool>( ), pAttribute2->GetValue<bool>( ) );

	case AT_STRING:
		return AssertEqualsTest( quiet, pAttribute1->GetValue<CUtlString>( ), pAttribute2->GetValue<CUtlString>( ) );

	case AT_VOID:
		return AssertEqualsTest( quiet, pAttribute1->GetValue<CUtlBinaryBlock>( ), pAttribute2->GetValue<CUtlBinaryBlock>( ) );

	case AT_OBJECTID:
		return true; // skip this for now - two elements can't have the same id, and CreateTestScene currently creates random test_id's each time...
/*
		{
			if ( !g_pDataModel->IsEqual( pAttribute1->GetValue<DmObjectId_t>( ), pAttribute2->GetValue<DmObjectId_t>( ) ) )
			{
				if ( !quiet )
				{
					Assert( g_pDataModel->IsEqual( pAttribute1->GetValue<DmObjectId_t>( ), pAttribute2->GetValue<DmObjectId_t>( ) ) );
				}
				return false;
			}
			return true;
		}
		break;
*/

	case AT_COLOR:
		return AssertEqualsTest( quiet, pAttribute1->GetValue<Color>( ), pAttribute2->GetValue<Color>( ) );

	case AT_VECTOR2:
		return AssertEqualsTest( quiet, pAttribute1->GetValue<Vector2D>( ), pAttribute2->GetValue<Vector2D>( ) );

	case AT_VECTOR3:
		return AssertEqualsTest( quiet, pAttribute1->GetValue<Vector>( ), pAttribute2->GetValue<Vector>( ) );

	case AT_VECTOR4:
		return AssertEqualsTest( quiet, pAttribute1->GetValue<Vector4D>( ), pAttribute2->GetValue<Vector4D>( ) );

	case AT_QANGLE:
		return AssertEqualsTest( quiet, pAttribute1->GetValue<QAngle>( ), pAttribute2->GetValue<QAngle>( ) );

	case AT_QUATERNION:
		return AssertEqualsTest( quiet, pAttribute1->GetValue<Quaternion>( ), pAttribute2->GetValue<Quaternion>( ) );

	case AT_VMATRIX:
		return AssertEqualsTest( quiet, pAttribute1->GetValue<VMatrix>( ), pAttribute2->GetValue<VMatrix>( ) );

	case AT_ELEMENT:
		return AssertEqualElementHierarchies( quiet, pAttribute1->GetValue<DmElementHandle_t>( ), pAttribute2->GetValue<DmElementHandle_t>( ) );

	case AT_ELEMENT_ARRAY:
		{
			const CDmrElementArray< CDmElement > src1( pAttribute1 );
			const CDmrElementArray< CDmElement > src2( pAttribute2 );

			bool differs = !AssertEqualsTest( quiet, src1.Count(), src2.Count() );
			bool differs2 = false;
			for ( int i = 0; i < src1.Count(); ++i )
			{
				differs2 |= !AssertEqualElementHierarchies( quiet, src1[ i ]->GetHandle(), src2[ i ]->GetHandle() );
			}

			return ( !differs && !differs2 );
		}
		break;

	case AT_INT_ARRAY:
		return AssertEqualsUtlVector<int>( quiet, pAttribute1, pAttribute2 );

	case AT_FLOAT_ARRAY:
		return AssertEqualsUtlVector<float>( quiet, pAttribute1, pAttribute2 );

	case AT_BOOL_ARRAY:
		return AssertEqualsUtlVector<bool>( quiet, pAttribute1, pAttribute2 );

	case AT_STRING_ARRAY:
		return AssertEqualsUtlVector<CUtlString>( quiet, pAttribute1, pAttribute2 );

	case AT_VOID_ARRAY:
		return AssertEqualsUtlVector<CUtlBinaryBlock>( quiet, pAttribute1, pAttribute2 );

	case AT_OBJECTID_ARRAY:
		{
			const CDmrArray<DmObjectId_t> src1( pAttribute1 );
			const CDmrArray<DmObjectId_t> src2( pAttribute2 );

			bool differs = AssertEqualsTest( quiet, src1.Count(), src2.Count() );
			return differs; // skip this for now - CreateTestScene currently creates random ids each time...
/*
			bool differs2 = false;
			for ( int i = 0; i < src1.Count(); ++i )
			{
				if ( !g_pDataModel->IsEqual( src1[i], src2[i] ) )
				{
					differs2 = true;
					if ( !quiet )
					{
						Assert( g_pDataModel->IsEqual( src1[i], src2[i] ) );
					}
				}
			}

			return ( !differs && !differs2 );
*/
		}
		break;

	case AT_COLOR_ARRAY:
		return AssertEqualsUtlVector<Color>( quiet, pAttribute1, pAttribute2 );

	case AT_VECTOR2_ARRAY:
		return AssertEqualsUtlVector<Vector2D>( quiet, pAttribute1, pAttribute2 );

	case AT_VECTOR3_ARRAY:
		return AssertEqualsUtlVector<Vector>( quiet, pAttribute1, pAttribute2 );

	case AT_VECTOR4_ARRAY:
		return AssertEqualsUtlVector<Vector4D>( quiet, pAttribute1, pAttribute2 );

	case AT_QANGLE_ARRAY:
		return AssertEqualsUtlVector<QAngle>( quiet, pAttribute1, pAttribute2 );

	case AT_QUATERNION_ARRAY:
		return AssertEqualsUtlVector<Quaternion>( quiet, pAttribute1, pAttribute2 );

	case AT_VMATRIX_ARRAY:
		return AssertEqualsUtlVector<VMatrix>( quiet, pAttribute1, pAttribute2 );
	}

	return retval;
}

bool AssertEqualElementHierarchies( bool quiet, DmElementHandle_t src1, DmElementHandle_t src2 )
{
	CDmElement *pSrc1 = g_pDataModel->GetElement( src1 );
	CDmElement *pSrc2 = g_pDataModel->GetElement( src2 );

	if ( !pSrc1 || !pSrc2 )
		return false;

	// Assume equality
	bool retval = true;

	if ( pSrc1->GetType() != pSrc2->GetType() )
	{
		if ( !quiet )
		{
			AssertMsg( 0, "pSrc1->GetType() == pSrc2->GetType()" );
		}
		retval = false;
	}

	if ( Q_strcmp( pSrc1->GetName(), pSrc2->GetName() ) )
	{
		if ( !quiet )
		{
			AssertMsg2( 0, "Q_strcmp( %s, %s )", pSrc1->GetName(), pSrc2->GetName() );
		}
		retval = false;
	}

	if ( pSrc1->AttributeCount() != pSrc2->AttributeCount() )
	{
		if ( !quiet )
		{
			AssertMsg( 0, "pSrc1->NumAttributes() == pSrc2->NumAttributes()" );
		}
		retval = false;
	}

	for ( CDmAttribute *pAttribute1 = pSrc1->FirstAttribute(); pAttribute1; pAttribute1 = pAttribute1->NextAttribute() )
	{
		const char *pName = pAttribute1->GetName();
		if ( !pSrc2->HasAttribute( pName ) )
		{
			if ( !quiet )
			{
				AssertMsg1( 0, "pSrc2->HasAttribute( %s ) failed\n", pName );
			}
			retval = false;
		}
		else
		{
			CDmAttribute *pAttribute2 = pSrc2->GetAttribute( pName );

			bool differs = !AssertEqualAttributes( quiet, pAttribute1, pAttribute2 );
			if ( differs )
			{
				retval = false;
			}
		}
	}

	return retval;
}

void TestDeleteOldCR( const char *pSerializationType )
{
	DmFileId_t testFileID = g_pDataModel->FindOrCreateFileId( "<TestDeleteOldCR>" );
	DmElementHandle_t hRoot = CreateTestScene( testFileID );

	int nTestElements = g_pDataModel->NumElementsInFile( testFileID );

	const char *pFileName = "DeleteOld.dmx";
	CDmElement *pRoot = static_cast< CDmElement* >( g_pDataModel->GetElement( hRoot ) );
	bool bOk = g_pDataModel->SaveToFile( pFileName, NULL, pSerializationType, "dmx", pRoot );
	Shipping_Assert( bOk );

	CDmElement *pReadInRoot = NULL;
	DmFileId_t readFileID = g_pDataModel->RestoreFromFile( pFileName, NULL, NULL, &pReadInRoot, CR_DELETE_OLD );
	Shipping_Assert( readFileID != DMFILEID_INVALID );

	if ( pReadInRoot )
	{
		Shipping_Assert( pReadInRoot->GetHandle() == hRoot );
		Shipping_Assert( g_pDataModel->GetElement( hRoot ) == pReadInRoot );
		Shipping_Assert( g_pDataModel->NumElementsInFile( testFileID ) == 0 );
		Shipping_Assert( g_pDataModel->NumElementsInFile( readFileID ) == nTestElements );

		CDmeHandle< CDmElement > rootHandle( hRoot ); // keeps a reference to root around, even after the file is unloaded
		g_pDataModel->UnloadFile( readFileID );

		Shipping_Assert( g_pDataModel->NumElementsInFile( readFileID ) == 0 );
		Shipping_Assert( g_pDataModel->GetElement( hRoot ) == NULL );

		DmFileId_t readFileID2 = g_pDataModel->RestoreFromFile( pFileName, NULL, NULL, &pReadInRoot, CR_DELETE_OLD );
		Shipping_Assert( readFileID2 == readFileID );

		Shipping_Assert( pReadInRoot->GetHandle() == hRoot );
		Shipping_Assert( g_pDataModel->GetElement( hRoot ) == pReadInRoot );
		Shipping_Assert( g_pDataModel->NumElementsInFile( testFileID ) == 0 );
		Shipping_Assert( g_pDataModel->NumElementsInFile( readFileID ) == nTestElements );

		g_pDataModel->RemoveFileId( readFileID );
	}
	else
	{
		Msg( "Failed to load %s back from disk!!!", pFileName );
	}

	g_pDataModel->RemoveFileId( testFileID );
}

void TestDeleteNewCR( const char *pSerializationType )
{
	DmFileId_t testFileID = g_pDataModel->FindOrCreateFileId( "<TestDeleteNewCR>" );
	DmElementHandle_t hRoot = CreateTestScene( testFileID );

	int nTestElements = g_pDataModel->NumElementsInFile( testFileID );

	const char *pFileName = "DeleteNew.dmx";
	CDmElement *pRoot = static_cast< CDmElement* >( g_pDataModel->GetElement( hRoot ) );
	bool bOk = g_pDataModel->SaveToFile( pFileName, NULL, pSerializationType, "dmx", pRoot );
	Shipping_Assert( bOk );

	CDmElement *pReadInRoot = NULL;
	DmFileId_t readFileID = g_pDataModel->RestoreFromFile( pFileName, NULL, NULL, &pReadInRoot, CR_DELETE_NEW );
	Shipping_Assert( readFileID != DMFILEID_INVALID );

	Shipping_Assert( g_pDataModel->GetElement( hRoot ) == pRoot );
	Shipping_Assert( pRoot->GetHandle() == hRoot );
	Shipping_Assert( pReadInRoot == pRoot ); // RestoreFromFile now returns the old element when the new root is deleted
	Shipping_Assert( g_pDataModel->NumElementsInFile( testFileID ) == nTestElements );
	Shipping_Assert( g_pDataModel->NumElementsInFile( readFileID ) == 0 );

	g_pDataModel->UnloadFile( readFileID );

	Shipping_Assert( g_pDataModel->NumElementsInFile( readFileID ) == 0 );

	DmFileId_t readFileID2 = g_pDataModel->RestoreFromFile( pFileName, NULL, NULL, &pReadInRoot, CR_DELETE_NEW );
	Shipping_Assert( readFileID2 == readFileID );

	Shipping_Assert( g_pDataModel->GetElement( hRoot ) == pRoot );
	Shipping_Assert( pRoot->GetHandle() == hRoot );
	Shipping_Assert( pReadInRoot == pRoot ); // RestoreFromFile now returns the old element when the new root is deleted
	Shipping_Assert( g_pDataModel->NumElementsInFile( testFileID ) == nTestElements );
	Shipping_Assert( g_pDataModel->NumElementsInFile( readFileID ) == 0 );

	g_pDataModel->RemoveFileId( readFileID );

	g_pDataModel->RemoveFileId( testFileID );
}

void TestCopyNewCR( const char *pSerializationType )
{
	DmFileId_t testFileID = g_pDataModel->FindOrCreateFileId( "<TestCopyNewCR>" );
	DmElementHandle_t hRoot = CreateTestScene( testFileID );

	int nTestElements = g_pDataModel->NumElementsInFile( testFileID );

	const char *pFileName = "CopyNew.dmx";
	CDmElement *pRoot = g_pDataModel->GetElement( hRoot );
	bool bOk = g_pDataModel->SaveToFile( pFileName, NULL, pSerializationType, "dmx", pRoot );
	Shipping_Assert( bOk );

	CDmElement *pReadInRoot = NULL;
	DmFileId_t readFileID = g_pDataModel->RestoreFromFile( pFileName, NULL, NULL, &pReadInRoot, CR_COPY_NEW );
	Shipping_Assert( readFileID != DMFILEID_INVALID );

	if ( pReadInRoot )
	{
		DmElementHandle_t hReadInRoot = pReadInRoot->GetHandle();

		Shipping_Assert( g_pDataModel->GetElement( hRoot ) == pRoot );
		Shipping_Assert( pRoot->GetHandle() == hRoot );
		Shipping_Assert( pReadInRoot->GetHandle() != hRoot );
		Shipping_Assert( !IsUniqueIdEqual( pRoot->GetId(), pReadInRoot->GetId() ) );
		Shipping_Assert( g_pDataModel->NumElementsInFile( testFileID ) == nTestElements );
		Shipping_Assert( g_pDataModel->NumElementsInFile( readFileID ) == nTestElements );

		g_pDataModel->UnloadFile( readFileID );

		Shipping_Assert( g_pDataModel->NumElementsInFile( readFileID ) == 0 );
		Shipping_Assert( g_pDataModel->GetElement( hReadInRoot ) == NULL );

		DmFileId_t readFileID2 = g_pDataModel->RestoreFromFile( pFileName, NULL, NULL, &pReadInRoot, CR_COPY_NEW );
		Shipping_Assert( readFileID2 == readFileID );

		Shipping_Assert( g_pDataModel->GetElement( hRoot ) == pRoot );
		Shipping_Assert( pRoot->GetHandle() == hRoot );
		Shipping_Assert( pReadInRoot->GetHandle() != hRoot );
		Shipping_Assert( !IsUniqueIdEqual( pRoot->GetId(), pReadInRoot->GetId() ) );
		Shipping_Assert( g_pDataModel->NumElementsInFile( testFileID ) == nTestElements );
		Shipping_Assert( g_pDataModel->NumElementsInFile( readFileID ) == nTestElements );

		g_pDataModel->RemoveFileId( readFileID );
	}
	else
	{
		Msg( "Failed to load %s back from disk!!!", pFileName );
	}

	g_pDataModel->RemoveFileId( testFileID );
}

void TestForceCopyCR( const char *pSerializationType )
{
	DmFileId_t testFileID = g_pDataModel->FindOrCreateFileId( "<TestForceCopyCR>" );
	DmElementHandle_t hRoot = CreateTestScene( testFileID );

	int nTestElements = g_pDataModel->NumElementsInFile( testFileID );

	const char *pFileName = "ForceCopy.dmx";
	CDmElement *pRoot = static_cast< CDmElement* >( g_pDataModel->GetElement( hRoot ) );
	bool bOk = g_pDataModel->SaveToFile( pFileName, NULL, pSerializationType, "dmx", pRoot );
	Shipping_Assert( bOk );

	CDmElement *pReadInRoot = NULL;
	DmFileId_t readFileID = g_pDataModel->RestoreFromFile( pFileName, NULL, NULL, &pReadInRoot, CR_FORCE_COPY );
	Shipping_Assert( readFileID != DMFILEID_INVALID );

	if ( pReadInRoot )
	{
		DmElementHandle_t hReadInRoot = pReadInRoot->GetHandle();

		Shipping_Assert( g_pDataModel->GetElement( hRoot ) == pRoot );
		Shipping_Assert( pRoot->GetHandle() == hRoot );
		Shipping_Assert( pReadInRoot->GetHandle() != hRoot );
		Shipping_Assert( !IsUniqueIdEqual( pRoot->GetId(), pReadInRoot->GetId() ) );
		Shipping_Assert( g_pDataModel->NumElementsInFile( testFileID ) == nTestElements );
		Shipping_Assert( g_pDataModel->NumElementsInFile( readFileID ) == nTestElements );

		g_pDataModel->UnloadFile( readFileID );

		Shipping_Assert( g_pDataModel->NumElementsInFile( readFileID ) == 0 );
		Shipping_Assert( g_pDataModel->GetElement( hReadInRoot ) == NULL );

		DmFileId_t readFileID2 = g_pDataModel->RestoreFromFile( pFileName, NULL, NULL, &pReadInRoot, CR_FORCE_COPY );
		Shipping_Assert( readFileID2 == readFileID );

		Shipping_Assert( g_pDataModel->GetElement( hRoot ) == pRoot );
		Shipping_Assert( pRoot->GetHandle() == hRoot );
		Shipping_Assert( pReadInRoot->GetHandle() != hRoot );
		Shipping_Assert( !IsUniqueIdEqual( pRoot->GetId(), pReadInRoot->GetId() ) );
		Shipping_Assert( g_pDataModel->NumElementsInFile( testFileID ) == nTestElements );
		Shipping_Assert( g_pDataModel->NumElementsInFile( readFileID ) == nTestElements );

		g_pDataModel->RemoveFileId( readFileID );
	}
	else
	{
		Msg( "Failed to load %s back from disk!!!", pFileName );
	}

	g_pDataModel->RemoveFileId( testFileID );
}

void TestConflictResolution( const char *pSerializationType )
{
	TestDeleteOldCR( pSerializationType );
	TestDeleteNewCR( pSerializationType );
	TestCopyNewCR( pSerializationType );
	TestForceCopyCR( pSerializationType );
}

void TestSerializationMethod( const char *pSerializationType )
{
	DmFileId_t testFileID = g_pDataModel->FindOrCreateFileId( "<CreateTestScene>" );
	DmElementHandle_t hRoot = CreateTestScene( testFileID );

	const char *pFileName = "dmxtest.dmx";
	CDmElement *pRoot = static_cast<CDmElement*>(g_pDataModel->GetElement(hRoot));
	bool bOk = g_pDataModel->SaveToFile( pFileName, NULL, pSerializationType, "dmx", pRoot );
	Shipping_Assert( bOk );

	CDmElement *pReadInRoot = NULL;
	DmFileId_t dmxFileID = g_pDataModel->RestoreFromFile( pFileName, NULL, NULL, &pReadInRoot, CR_FORCE_COPY );
	Shipping_Assert( dmxFileID != DMFILEID_INVALID );

	if ( pReadInRoot )
	{
		AssertEqualElementHierarchies( false, hRoot, pReadInRoot->GetHandle() );
		g_pDataModel->RemoveFileId( dmxFileID );
	}
	else
	{
		Msg( "Failed to load dmxtest.dmx back from disk!!!" );
	}

	g_pDataModel->RemoveFileId( testFileID );

	TestConflictResolution( pSerializationType );
}

DEFINE_TESTCASE_NOSUITE( DmxSerializationTest )
{
	Msg( "Running dmx serialization tests...\n" );
	CDisableUndoScopeGuard sg;

	TestSerializationMethod( "keyvalues2" );
	TestSerializationMethod( "keyvalues2_flat" );
	TestSerializationMethod( "xml" );
	TestSerializationMethod( "xml_flat" );
	TestSerializationMethod( "binary" );

	int nEndingCount = g_pDataModel->GetAllocatedElementCount();
	AssertEqualsTest( false, 0, nEndingCount );
}
