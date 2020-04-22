//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Unit test program  for DMX testing
//
// $NoKeywords: $
//=============================================================================//

#include "unitlib/unitlib.h"
#include "datamodel/dmelement.h"
#include "datamodel/idatamodel.h"
#include "movieobjects/movieobjects.h"
#include "tier1/utlbuffer.h"
#include "filesystem.h"


// From dmxtestserialization.cpp

bool AssertEqualElementHierarchies( bool quiet, DmElementHandle_t src1, DmElementHandle_t src2 );
bool AssertUnEqualElementHierarchies( DmElementHandle_t src1, DmElementHandle_t src2 );

void CreateTestScene( CUtlVector< DmElementHandle_t >& handles, DmFileId_t fileid );

bool AssertEqualElementHierarchies( bool quiet, CDmElement *src1, CDmElement *src2 )
{
	DmElementHandle_t h1 = src1->GetHandle();
	DmElementHandle_t h2 = src2->GetHandle();

	return AssertEqualElementHierarchies( quiet, h1, h2 );
}

bool AssertUnEqualElementHierarchies( CDmElement *src1, CDmElement *src2 )
{
	return AssertUnEqualElementHierarchies( src1->GetHandle(), src2->GetHandle() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void DescribeUndo()
{
	CUtlVector< UndoInfo_t > list;
	g_pDataModel->GetUndoInfo( list );

	for ( int i = list.Count() - 1; i >= 0; --i )
	{
		UndoInfo_t& entry = list[ i ];
		if ( entry.terminator )
		{
			Msg( "[ '%s' ] -- %i operations\n", entry.undo, entry.numoperations );
		}

		Msg( "   +[ '%s' ]\n", entry.desc );
	}
}

template< class T >
void RunSingleSetAttributeUndoTest( CDmElement *pTest, CDmElement *pOriginal, CDmElement *pElement, const char *attribute, const T& newVal )
{
	{
		CUndoScopeGuard guard( attribute, attribute );
		pElement->SetValue( attribute, newVal );
	}
	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );

	g_pDataModel->Redo();
	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );
}

void RunSingleSetAttributeUndoTestBinary( CDmElement *pTest, CDmElement *pOriginal, CDmElement *pElement, const char *attribute, const void *data, size_t size )
{
	{
		CUndoScopeGuard guard( attribute, attribute );
		pElement->SetValue( attribute, data, size );
	}

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );

	g_pDataModel->Redo();

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );
}

template< class T >
void RunSingleSetAttributeUndoTestArray( CDmElement *pTest, CDmElement *pOriginal, CDmElement *pElement, const char *attribute, int slot, const T& newVal )
{
	{
		CUndoScopeGuard guard( attribute, attribute );
		CDmrArray< T > array( pElement, attribute );
		array.Set( slot, newVal );
	}

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );

	g_pDataModel->Redo();

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );
}

template<> void RunSingleSetAttributeUndoTestArray<DmElementHandle_t>( CDmElement *pTest, CDmElement *pOriginal, CDmElement *pElement, const char *attribute, int slot, const DmElementHandle_t& newVal )
{
	{
		CUndoScopeGuard guard( attribute, attribute );
		CDmrElementArray<> array( pElement, attribute );
		array.SetHandle( slot, newVal );
	}

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );

	g_pDataModel->Redo();

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );
}

template< class T >
void RunSingleAttributeUndoTestArrayAddToHead( CDmElement *pTest, CDmElement *pOriginal, CDmElement *pElement, const char *attribute, const T& newVal )
{
	{
		CUndoScopeGuard guard( attribute, attribute );
		CDmrArray< T > array( pElement, attribute );
		array.InsertBefore( 0, newVal );
	}

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );

	g_pDataModel->Redo();

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );
}

template< class T >
void RunSingleAttributeUndoTestArrayAddToTail( CDmElement *pTest, CDmElement *pOriginal, CDmElement *pElement, const char *attribute, const T& newVal )
{
	{
		CUndoScopeGuard guard( attribute, attribute );
		CDmrArray< T > array( pElement, attribute );
		array.AddToTail( newVal );
	}

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );

	g_pDataModel->Redo();

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );
}

template< class T >
void RunSingleAttributeUndoTestArrayInsertBefore( CDmElement *pTest, CDmElement *pOriginal, CDmElement *pElement, const char *attribute, int slot, const T& newVal )
{
	{
		CUndoScopeGuard guard( attribute, attribute );
		CDmrArray< T > array( pElement, attribute );
		array.InsertBefore( slot, newVal );
	}

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );

	g_pDataModel->Redo();

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );
}


template< class T >
void RunSingleAttributeUndoTestArrayFastRemove( CDmElement *pTest, CDmElement *pOriginal, CDmElement *pElement, const char *attribute, int slot, T typeDeducer )
{
	{
		CUndoScopeGuard guard( attribute, attribute );
		CDmrArray< T > array( pElement, attribute );
		array.FastRemove( slot );
	}

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );

	g_pDataModel->Redo();

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );
}


void RunSingleAttributeUndoTestElementArrayRemove( CDmElement *pTest, CDmElement *pOriginal, CDmElement *pElement, const char *attribute, int slot )
{
	{
		CUndoScopeGuard guard( attribute, attribute );
		CDmrElementArray<> array( pElement, attribute );
		array.Remove( slot );
	}

	DescribeUndo();

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );

	g_pDataModel->Redo();

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );
}

template< class T >
void RunSingleAttributeUndoTestArrayRemove( CDmElement *pTest, CDmElement *pOriginal, CDmElement *pElement, const char *attribute, int slot, T typeDeducer )
{
	{
		CUndoScopeGuard guard( attribute, attribute );
		CDmrArray<T> array( pElement, attribute );
		array.Remove( slot );
	}

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );

	g_pDataModel->Redo();

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );
}

template< class T >
void RunSingleAttributeUndoTestArrayRemoveMultiple( CDmElement *pTest, CDmElement *pOriginal, CDmElement *pElement, const char *attribute, int slot, int count, T typeDeducer )
{
	{
		CUndoScopeGuard guard( attribute, attribute );
		CDmrArray< T > array( pElement, attribute );
		array.RemoveMultiple( slot, count );
	}

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );

	g_pDataModel->Redo();

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );
}

template< class T >
void RunSingleAttributeUndoTestArrayRemoveAll( CDmElement *pTest, CDmElement *pOriginal, CDmElement *pElement, const char *attribute, bool purge )
{
	{
		CUndoScopeGuard guard( attribute, attribute );
		CDmrArray< T > array( pElement, attribute );
		if( purge )
		{
			array.Purge();
		}
		else
		{
			array.RemoveAll();
		}
	}

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );

	g_pDataModel->Redo();

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );
}

template< class T >
void RunSingleAttributeUndoTestArrayCopyFrom( CDmElement *pTest, CDmElement *pOriginal, CDmElement *pElement, const char *attribute, const CUtlVector<T>& list )
{
	{
		CUndoScopeGuard guard( attribute, attribute );
		CDmrArray<T> array( pElement, attribute );
		array.CopyArray( list.Base(), list.Count() );
	}

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );

	g_pDataModel->Redo();

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );
}


void RunSingleSetAttributeUndoTestArrayBinary( CDmElement *pTest, CDmElement *pOriginal, CDmElement *pElement, const char *attribute, int slot, const void *data, size_t size )
{
	{
		CUndoScopeGuard guard( attribute, attribute );
		CUtlBinaryBlock block( (const void *)data, size );
		CDmrArray< CUtlBinaryBlock > array( pElement, attribute );
		array.Set( slot, block );
	}

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );

	g_pDataModel->Redo();

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );
}

template< class T >
void RunSingleSetAttributeUndoTestArrayWhole( CDmElement *pTest, CDmElement *pOriginal, CDmElement *pElement, const char *attribute, CUtlVector< T >& newVal )
{
	{
		CUndoScopeGuard guard( attribute, attribute );
		IDmAttributeArray< T >	*pArray = static_cast< IDmAttributeArray< T > * >( pElement->GetAttribute( "attribute" ) );
		pArray->SetValue( newVal );
	}

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );

	g_pDataModel->Redo();

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );
}

template< class T >
void RunSingleAddAttributeUndoTest( CDmElement *pTest, CDmElement *pOriginal, CDmElement *pElement, const char *attribute, const T& newVal )
{
	{
		CUndoScopeGuard guard( attribute, attribute );
		pElement->SetValue( attribute, newVal );
	}

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );

	g_pDataModel->Redo();

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );
}

void RunSingleRemoveAttributeUndoTest( CDmElement *pTest, CDmElement *pOriginal, CDmElement *pElement, const char *attribute )
{
	AssertEqualElementHierarchies( false, pTest, pOriginal );

	{
		CUndoScopeGuard guard( attribute, attribute );
		pElement->RemoveAttribute( attribute );
	}

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );

	g_pDataModel->Redo();

	AssertUnEqualElementHierarchies( pTest, pOriginal );

	g_pDataModel->Undo();

	AssertEqualElementHierarchies( false, pTest, pOriginal );
}


void RunUndoTestsArray( CUtlVector< DmElementHandle_t >& handles, CDmElement *pTest, CDmElement *pOriginal )
{
	DmObjectId_t id;
	CreateUniqueId( &id );

	VMatrix mat, mat2;
	MatrixBuildRotateZ( mat, 55 );
	MatrixBuildRotateZ( mat2, -55 );
	int i;
	unsigned char buf[256];
	unsigned char buf2[256];
	for ( i = 0; i < 256; ++i )
	{
		buf[i] = i+55500;
		buf2[i] = 55000 + 255 - i;
	}

	Assert( handles.Count() == 7 );

	//CDmElement *pElement = pTest;
	CDmElement *pElement2 = GetElement< CDmElement >( handles[ 1 ] );
	CDmElement *pElement3 = GetElement< CDmElement >( handles[ 2 ] );
	CDmElement *pElement4 = GetElement< CDmElement >( handles[ 3 ] );
	CDmElement *pElement5 = GetElement< CDmElement >( handles[ 4 ] );
	CDmElement *pElement6 = GetElement< CDmElement >( handles[ 5 ] );
	CDmElement *pElement7 = GetElement< CDmElement >( handles[ 6 ] );

	RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement2, "int_array_test", 0, 55 );
	RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement2, "float_array_test", 0, 55.0f );
	RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement2, "bool_array_test", 0, true );

	RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement3, "color_array_test", 0, Color( 55, 55, 55, 55 ) );
	RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement3, "vector2d_array_test", 0, Vector2D( 55.0f, -55.0f ) );
	RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement3, "vector3d_array_test", 0, Vector( 55.0f, -55.0f, 55.0f ) );

	RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement4, "vector4d_array_test", 0, Vector4D( 55.0f, -55.0f, 55.0f, 55.0f ) );
	RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement4, "qangle_array_test", 0, QAngle( 55.0f, 55.0f, -55.0f ) );
	RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement4, "quat_array_test", 0, Quaternion( 1.0f, -1.0f, 0.0f, 55.0f ) );

	RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement5, "vmatrix_array_test", 0, mat );
	CUtlString newString( "55test" );
	RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement5, "string_array_test", 0, newString );
	RunSingleSetAttributeUndoTestArrayBinary( pTest, pOriginal, pElement5, "binary_array_test", 0, buf, 256 );

//	RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement6, "elementid_array_test", 0, id );
	RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement6, "element_array_test", 0, pElement2->GetHandle() );

	RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement7, "element_array_test", 0, pElement5->GetHandle() );

	{
		RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement2, "int_array_test", 0, 55 );
		RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement2, "float_array_test", 0, 55.0f );
		RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement2, "bool_array_test", 0, true );

		RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement3, "color_array_test", 0, Color( 55, 55, 55, 55 ) );
		RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement3, "vector2d_array_test", 0, Vector2D( 55.0f, -55.0f ) );
		RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement3, "vector3d_array_test", 0, Vector( 55.0f, -55.0f, 55.0f ) );

		RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement4, "vector4d_array_test", 0, Vector4D( 55.0f, -55.0f, 55.0f, 55.0f ) );
		RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement4, "qangle_array_test", 0, QAngle( 55.0f, 55.0f, -55.0f ) );
		RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement4, "quat_array_test", 0, Quaternion( 1.0f, -1.0f, 0.0f, 55.0f ) );

		RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement5, "vmatrix_array_test", 0, mat );
		CUtlString newString( "55test" );
		RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement5, "string_array_test", 0, newString );
		RunSingleSetAttributeUndoTestArrayBinary( pTest, pOriginal, pElement5, "binary_array_test", 0, buf, 256 );

//		RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement6, "elementid_array_test", 0, id );
		RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement6, "element_array_test", 0, pElement2->GetHandle() );

		RunSingleSetAttributeUndoTestArray( pTest, pOriginal, pElement7, "element_array_test", 0, pElement5->GetHandle() );
	}

	RunSingleAttributeUndoTestArrayAddToHead( pTest, pOriginal, pElement2, "int_array_test", 55 );
	RunSingleAttributeUndoTestArrayAddToTail( pTest, pOriginal, pElement2, "int_array_test", 55 );
	RunSingleAttributeUndoTestArrayInsertBefore( pTest, pOriginal, pElement2, "int_array_test", 0, 55 );
	RunSingleAttributeUndoTestArrayFastRemove( pTest, pOriginal, pElement2, "int_array_test", 0, (int)0 );
	RunSingleAttributeUndoTestArrayRemove( pTest, pOriginal, pElement2, "int_array_test", 0, (int)0 );
	RunSingleAttributeUndoTestArrayRemoveMultiple( pTest, pOriginal, pElement2, "int_array_test", 0, 2, (int)0 );
	RunSingleAttributeUndoTestElementArrayRemove( pTest, pOriginal, pElement7, "children", 0 );

	// RemoveAll
	RunSingleAttributeUndoTestArrayRemoveAll<int>( pTest, pOriginal, pElement2, "int_array_test", false );
	// Purge
	RunSingleAttributeUndoTestArrayRemoveAll<int>( pTest, pOriginal, pElement2, "int_array_test", true );

	CUtlVector< int > foo;
	foo.AddToTail( 55 );
	foo.AddToTail( 56 );
	foo.AddToTail( 57 );
	foo.AddToTail( 58 );
	foo.AddToTail( 59 );

	RunSingleAttributeUndoTestArrayCopyFrom( pTest, pOriginal, pElement2, "int_array_test", foo );
}

void RunUndoTests( CUtlVector< DmElementHandle_t >& handles, CDmElement *pTest, CDmElement *pOriginal )
{
	DmObjectId_t id;
	CreateUniqueId( &id );

	VMatrix mat, mat2;
	MatrixBuildRotateZ( mat, 55 );
	MatrixBuildRotateZ( mat2, -55 );
	int i;
	unsigned char buf[256];
	unsigned char buf2[256];
	for ( i = 0; i < 256; ++i )
	{
		buf[i] = i+100;
		buf2[i] = 127 + 255 - i;
	}

	Assert( handles.Count() == 7 );
	CDmElement *pElement = pTest;
	CDmElement *pElement2 = GetElement<CDmElement>( handles[ 1 ] );

	RunSingleSetAttributeUndoTest( pTest, pOriginal, pElement, "element_test", (CDmElement *)NULL );
	RunSingleSetAttributeUndoTest( pTest, pOriginal, pElement, "int_test", 55 );
	RunSingleSetAttributeUndoTest( pTest, pOriginal, pElement, "float_test", 55.0f );
	RunSingleSetAttributeUndoTest( pTest, pOriginal, pElement, "bool_test", false );
//	RunSingleSetAttributeUndoTest( pTest, pOriginal, pElement, "id_test", id );
	RunSingleSetAttributeUndoTest( pTest, pOriginal, pElement, "string_test", "55test" );
	RunSingleSetAttributeUndoTestBinary( pTest, pOriginal, pElement, "binary_test", buf, 256 );
	RunSingleSetAttributeUndoTest( pTest, pOriginal, pElement, "color_test", Color( 55, 55, 55, 55 ) );
	RunSingleSetAttributeUndoTest( pTest, pOriginal, pElement, "vector2d_test", Vector2D( 55.0f, -55.0f ) );
	RunSingleSetAttributeUndoTest( pTest, pOriginal, pElement, "vector3d_test", Vector( 55.0f, -55.0f, 55.0f ) );
	RunSingleSetAttributeUndoTest( pTest, pOriginal, pElement, "vector4d_test", Vector4D( 55.0f, -55.0f, 55.0f, 55.0f ) );
	RunSingleSetAttributeUndoTest( pTest, pOriginal, pElement, "qangle_test", QAngle( 55.0f, 55.0f, -55.0f ) );
	RunSingleSetAttributeUndoTest( pTest, pOriginal, pElement, "quat_test", Quaternion( 1.0f, -1.0f, 0.0f, 55.0f ) );
	RunSingleSetAttributeUndoTest( pTest, pOriginal, pElement, "vmatrix_test", mat );

	// Now run a single test with a bunch of operations occurring at the same time
	{
		{
			CUndoScopeGuard guard( "biggish", "smallish" );
			pElement->SetValue( "bool_test", false );
			pElement->SetValue( "int_test", 55 );
			pElement->SetValue( "float_test", 55.0f );
			pElement->SetValue( "color_test", Color( 55, 55, 55, 55 ) );
			pElement->SetValue( "vector2d_test", Vector2D( 55.0f, -55.0f ) );
			pElement->SetValue( "vector3d_test", Vector( 55.0f, -55.0f, 55.0f ) );
			pElement->SetValue( "vector4d_test", Vector4D( 55.0f, -55.0f, 55.0f, 55.0f ) );
			pElement->SetValue( "qangle_test", QAngle( 55.0f, 55.0f, -55.0f ) );
			pElement->SetValue( "quat_test", Quaternion( 1.0f, -1.0f, 0.0f, 55.0f ) );
			pElement->SetValue( "vmatrix_test", mat );
			pElement->SetValue( "string_test", "55test" );
			pElement->SetValue( "binary_test", buf, 256 );
		}

		g_pDataModel->Undo();

		AssertEqualElementHierarchies( false, pTest, pOriginal );

		g_pDataModel->Redo();
		g_pDataModel->Undo();

		AssertEqualElementHierarchies( false, pTest, pOriginal );
	}
	
	// Now run a test with multiple items in the stack
	{
		// Push 1
		{
			CUndoScopeGuard guard( "biggish1", "smallish1" );
			pElement->SetValue( "bool_test", false );
			pElement->SetValue( "int_test", 55 );
			pElement->SetValue( "float_test", 55.0f );
			pElement->SetValue( "color_test", Color( 55, 55, 55, 55 ) );
		}
		// Push 2
		{
			CUndoScopeGuard guard( "biggish2", "smallish2" );
			pElement->SetValue( "vector2d_test", Vector2D( 55.0f, -55.0f ) );
			pElement->SetValue( "vector3d_test", Vector( 55.0f, -55.0f, 55.0f ) );
			pElement->SetValue( "vector4d_test", Vector4D( 55.0f, -55.0f, 55.0f, 55.0f ) );
			pElement->SetValue( "qangle_test", QAngle( 55.0f, 55.0f, -55.0f ) );
			pElement->SetValue( "quat_test", Quaternion( 1.0f, -1.0f, 0.0f, 55.0f ) );
		}
		// Push 3
		{
			CUndoScopeGuard guard( "biggish3", "smallish3" );
			pElement->SetValue( "vmatrix_test", mat );
			pElement->SetValue( "string_test", "55test" );
			pElement->SetValue( "binary_test", buf, 256 );
		}
		// Tests
		{
			g_pDataModel->Undo();
			g_pDataModel->Undo();
			g_pDataModel->Undo();

			AssertEqualElementHierarchies( false, pTest, pOriginal );

			g_pDataModel->Redo();
			g_pDataModel->Redo();
			g_pDataModel->Redo();

			g_pDataModel->Undo();
			g_pDataModel->Undo();
			g_pDataModel->Undo();

			AssertEqualElementHierarchies( false, pTest, pOriginal );
		}
	}

	// Now run "add" tests on a subset -- add adds an attribute where one didn't already exist
	RunSingleAddAttributeUndoTest( pTest, pOriginal, pElement2, "int_test_added", 55 );

	// Now run "remove" tests on a subset of attributes -- removes an existing attribute by name
	RunSingleRemoveAttributeUndoTest( pTest, pOriginal, pTest, "int_test" );
}

#include "datamodel/dmehandle.h"

void RunUndoTestCreateElement()
{
	DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( "<RunUndoTestCreateElement>" );

	CDmElement *element = NULL;
	{
		CUndoScopeGuard guard( "create" );
		element = CreateElement< CDmElement >( "test", fileid );
	}

	CDmeHandle< CDmElement >	hChannel;

	hChannel = element;

	Assert( hChannel.Get() );

	g_pDataModel->Undo();

	Assert( !hChannel.Get() );

	g_pDataModel->Redo();

	Assert( hChannel.Get() );

	g_pDataModel->Undo();   // It's on the redo stack, but marked as kill and all ptrs are NULL to it

	Assert( !hChannel.Get() );

	g_pDataModel->ClearUndo();
	g_pDataModel->ClearRedo();

	g_pDataModel->RemoveFileId( fileid );
}

DEFINE_TESTCASE_NOSUITE( DmxUndoRedoTest )
{
	Msg( "Running undo tests...\n" );

#ifdef _DEBUG
	int nStartingCount = g_pDataModel->GetAllocatedElementCount();
#endif

	CUndoScopeGuard sg( "Create Test Scenes" );

	DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( "<DmxUndoRedoTest>" );
	CUtlVector< DmElementHandle_t > handles;
	CreateTestScene( handles, fileid );
	DmElementHandle_t hOriginal = handles[ 0 ]; 
	handles.RemoveAll();
	CreateTestScene( handles, fileid );
	DmElementHandle_t hTest = handles[ 0 ];

	sg.Release();

	CDmElement *pOriginal = static_cast<CDmElement*>( g_pDataModel->GetElement( hOriginal ) );
	CDmElement *pTest = static_cast<CDmElement*>(g_pDataModel->GetElement( hTest ) );

	AssertEqualElementHierarchies( false, pTest, pOriginal );

	RunUndoTests( handles, pTest, pOriginal );
	RunUndoTestsArray( handles, pTest, pOriginal );
	RunUndoTestCreateElement();

	g_pDataModel->ClearUndo();

	g_pDataModel->RemoveFileId( fileid );

#ifdef _DEBUG
	int nEndingCount = g_pDataModel->GetAllocatedElementCount();
	AssertEquals( nEndingCount, nStartingCount );
	if ( nEndingCount != nStartingCount )
	{
		for ( DmElementHandle_t hElement = g_pDataModel->FirstAllocatedElement() ;
			hElement != DMELEMENT_HANDLE_INVALID;
			hElement = g_pDataModel->NextAllocatedElement( hElement ) )
		{
			CDmElement *pElement = g_pDataModel->GetElement( hElement );
			Assert( pElement );
			if ( !pElement )
				return;

			Msg( "[%s : %s] in memory\n", pElement->GetName(), pElement->GetTypeString() );
		}
	}
#endif
}

#include "datamodel/dmelementfactoryhelper.h"

//-----------------------------------------------------------------------------
// CDmeExternal - element class used for testing external attributes
//-----------------------------------------------------------------------------

class CDmeExternal : public CDmElement
{
	DEFINE_ELEMENT( CDmeExternal, CDmElement );

public:
	CDmaVar< float > m_External;
};

IMPLEMENT_ELEMENT_FACTORY( DmeExternal, CDmeExternal );

void CDmeExternal::OnConstruction()
{
	m_External   .InitAndSet( this, "external", 99.9f );
}

void CDmeExternal::OnDestruction()
{
}

USING_ELEMENT_FACTORY( DmeExternal );


DEFINE_TESTCASE_NOSUITE( DmxExternalAttributeTest )
{
	Msg( "Running external attribute test...\n" );

	DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( "<DmxExternalAttributeTest>" );

	CDmeExternal *pOriginal = 0;

#ifdef _DEBUG
	int nStartingCount = g_pDataModel->GetAllocatedElementCount();
#endif

	{
		CUndoScopeGuard guard( "External" );
		pOriginal = CreateElement<CDmeExternal>( "foo", fileid );

		for ( int m = 0; m < 1; ++m )
		{
			CDmeExternal *discard = CreateElement<CDmeExternal>( "foo", fileid );
			g_pDataModel->DestroyElement( discard->GetHandle() );
		}
	}

	// Now mess with vars

	g_pDataModel->Undo();
	g_pDataModel->Redo();

	{
		CUndoScopeGuard guard( "External2" );
		pOriginal->m_External.Set( 100.0f );
	}

	g_pDataModel->ClearUndo();

	g_pDataModel->RemoveFileId( fileid );

#ifdef _DEBUG
	int nEndingCount = g_pDataModel->GetAllocatedElementCount();
	AssertEquals( nEndingCount, nStartingCount );
	if ( nEndingCount != nStartingCount )
	{
		for ( DmElementHandle_t hElement = g_pDataModel->FirstAllocatedElement() ;
			hElement != DMELEMENT_HANDLE_INVALID;
			hElement = g_pDataModel->NextAllocatedElement( hElement ) )
		{
			CDmElement *pElement = g_pDataModel->GetElement( hElement );
			Assert( pElement );
			if ( !pElement )
				return;

			Msg( "[%s : %s] in memory\n", pElement->GetName(), pElement->GetTypeString() );
		}
	}
#endif
}

void Assert_InvalidAndNULL( CDmeHandle< CDmElement > &hElement )
{
	Shipping_Assert( !hElement.Get() );
	if ( !hElement.Get() )
	{
		g_pDataModel->MarkHandleValid( hElement.GetHandle() );
		Shipping_Assert( !hElement.Get() );
		if ( hElement.Get() )
		{
			g_pDataModel->MarkHandleInvalid( hElement.GetHandle() );
		}
	}
}

void Assert_InvalidButNonNULL( CDmeHandle< CDmElement > &hElement )
{
	Shipping_Assert( !hElement.Get() );
	if ( !hElement.Get() )
	{
		g_pDataModel->MarkHandleValid( hElement.GetHandle() );
		Shipping_Assert( hElement.Get() );
		if ( hElement.Get() )
		{
			g_pDataModel->MarkHandleInvalid( hElement.GetHandle() );
		}
	}
}

void Assert_ValidAndNonNULL( CDmeHandle< CDmElement > &hElement )
{
	Shipping_Assert( hElement.Get() );
}

DEFINE_TESTCASE_NOSUITE( DmxReferenceCountingTest )
{
	Msg( "Running external attribute test...\n" );

	DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( "<DmxReferenceCountingTest>" );

	CDmeHandle< CDmElement > hRoot;
	CDmeHandle< CDmElement > hChild1, hChild2, hChild3, hChild4, hChild5;
	CDmeHandle< CDmElement > hGrandChild1, hGrandChild2, hGrandChild3, hGrandChild4, hGrandChild5, hGrandChild6, hGrandChild7;
	CDmeCountedHandle hStrong6, hStrong7;

	g_pDmElementFramework->BeginEdit();

	int nStartingCount = g_pDataModel->GetAllocatedElementCount();
	Shipping_Assert( nStartingCount == 0 );

	{
		CUndoScopeGuard guard( "Create RefCountTest Elements" );

		hRoot = CreateElement<CDmElement>( "root", fileid );
		CDmrElementArray<> arrayRoot( hRoot, "children", true );
		g_pDataModel->SetFileRoot( fileid, hRoot->GetHandle() );

		hChild1 = CreateElement<CDmElement>( "child1", fileid );
		CDmrElementArray<> arrayChild1( hChild1, "children", true );
		arrayRoot.AddToTail( hChild1.Get() );

		hChild2 = CreateElement<CDmElement>( "child2", fileid );
		hChild2->AddAttributeElement<CDmElement>( "child" );
		arrayRoot.AddToTail( hChild2.Get() );

		hChild3 = CreateElement<CDmElement>( "child3", fileid );
		hChild3->AddAttributeElement<CDmElement>( "child" );
		arrayRoot.AddToTail( hChild3.Get() );

		hChild4 = CreateElement<CDmElement>( "child4", fileid );
		hChild4->AddAttributeElement<CDmElement>( "child" );
		arrayRoot.AddToTail( hChild4.Get() );

		hChild5 = CreateElement<CDmElement>( "child5", fileid );
		CDmrElementArray<> arrayChild5( hChild5, "children", true );
		arrayRoot.AddToTail( hChild5.Get() );

		hGrandChild1 = CreateElement<CDmElement>( "grandchild1", fileid );
		arrayChild1.AddToTail( hGrandChild1.Get() );

		hGrandChild2 = CreateElement<CDmElement>( "grandchild2", fileid );
		arrayChild1.AddToTail( hGrandChild2.Get() );
		hChild2->SetValue( "child", hGrandChild2.GetHandle() );

		hGrandChild3 = CreateElement<CDmElement>( "grandchild3", fileid );
		arrayChild1.AddToTail( hGrandChild3.Get() );
		hChild3->SetValue( "child", hGrandChild3.GetHandle() );

		hGrandChild4 = CreateElement<CDmElement>( "grandchild4", fileid );
		arrayChild1.AddToTail( hGrandChild4.Get() );
		hChild4->SetValue( "child", hGrandChild4.GetHandle() );

		hGrandChild5 = CreateElement<CDmElement>( "grandchild5", fileid );
		arrayChild1.AddToTail( hGrandChild5.Get() );
		arrayChild5.AddToTail( hGrandChild5.Get() );

		hGrandChild6 = CreateElement<CDmElement>( "grandchild6", fileid );
		arrayChild1.AddToTail( hGrandChild6.Get() );
		hStrong6 = hGrandChild6;

		hGrandChild7 = CreateElement<CDmElement>( "grandchild7", fileid );
		arrayChild1.AddToTail( hGrandChild7.Get() );
		hStrong7 = hGrandChild7;
	}

	g_pDmElementFramework->BeginEdit();

	int nCreateCount = g_pDataModel->GetAllocatedElementCount();
	Assert( nCreateCount == nStartingCount + 13 );

	Assert_ValidAndNonNULL  ( hRoot );
	Assert_ValidAndNonNULL  ( hChild1 );
	Assert_ValidAndNonNULL  ( hChild2 );
	Assert_ValidAndNonNULL  ( hChild3 );
	Assert_ValidAndNonNULL  ( hChild4 );
	Assert_ValidAndNonNULL  ( hChild5 );
	Assert_ValidAndNonNULL  ( hGrandChild1 );
	Assert_ValidAndNonNULL  ( hGrandChild2 );
	Assert_ValidAndNonNULL  ( hGrandChild3 );
	Assert_ValidAndNonNULL  ( hGrandChild4 );
	Assert_ValidAndNonNULL  ( hGrandChild5 );
	Assert_ValidAndNonNULL  ( hGrandChild6 );
	Assert_ValidAndNonNULL  ( hGrandChild7 );

	g_pDataModel->ClearUndo();

	g_pDmElementFramework->BeginEdit();

	int nPostCreateCount = g_pDataModel->GetAllocatedElementCount();
	Shipping_Assert( nPostCreateCount == nCreateCount );

	Assert_ValidAndNonNULL  ( hRoot );
	Assert_ValidAndNonNULL  ( hChild1 );
	Assert_ValidAndNonNULL  ( hChild2 );
	Assert_ValidAndNonNULL  ( hChild3 );
	Assert_ValidAndNonNULL  ( hChild4 );
	Assert_ValidAndNonNULL  ( hChild5 );
	Assert_ValidAndNonNULL  ( hGrandChild1 );
	Assert_ValidAndNonNULL  ( hGrandChild2 );
	Assert_ValidAndNonNULL  ( hGrandChild3 );
	Assert_ValidAndNonNULL  ( hGrandChild4 );
	Assert_ValidAndNonNULL  ( hGrandChild5 );
	Assert_ValidAndNonNULL  ( hGrandChild6 );
	Assert_ValidAndNonNULL  ( hGrandChild7 );

	{
		CUndoScopeGuard guard( "Edit RefCountTest Elements" );

		CDmrGenericArray children( hChild1, "children" );
		children.RemoveAll(); // grandchild1 unref'ed to 0, grandchild2..6 unref'ed to 1

		hChild2->SetValue( "child", DMELEMENT_HANDLE_INVALID ); // grandchild2 unref'ed to 0

		g_pDataModel->DestroyElement( hChild3 ); // grandchild3 is unref'ed to 0

		CDmrElementArray<> children5( hChild5, "children" );
		children5.Set( 0, NULL ); // grandchild5 is unref'ed to 0

		hStrong6 = DMELEMENT_HANDLE_INVALID; // grandchild6 is unref'ed to 0
	}

	g_pDmElementFramework->BeginEdit();

	int nEditCount = g_pDataModel->GetAllocatedElementCount();
	Shipping_Assert( nEditCount == nCreateCount - 1 );

	Assert_ValidAndNonNULL  ( hRoot );
	Assert_ValidAndNonNULL  ( hChild1 );
	Assert_ValidAndNonNULL  ( hChild2 );
	Assert_InvalidButNonNULL( hChild3 );
	Assert_ValidAndNonNULL  ( hChild4 );
	Assert_ValidAndNonNULL  ( hChild5 );
	Assert_ValidAndNonNULL  ( hGrandChild1 );
	Assert_ValidAndNonNULL  ( hGrandChild2 );
	Assert_ValidAndNonNULL  ( hGrandChild3 );
	Assert_ValidAndNonNULL  ( hGrandChild4 );
	Assert_ValidAndNonNULL  ( hGrandChild5 );
	Assert_ValidAndNonNULL  ( hGrandChild6 );
	Assert_ValidAndNonNULL  ( hGrandChild7 );

	g_pDataModel->Undo();

	g_pDmElementFramework->BeginEdit();

	int nUndoCount = g_pDataModel->GetAllocatedElementCount();
	Shipping_Assert( nUndoCount == nCreateCount );

	Assert_ValidAndNonNULL  ( hRoot );
	Assert_ValidAndNonNULL  ( hChild1 );
	Assert_ValidAndNonNULL  ( hChild2 );
	Assert_ValidAndNonNULL  ( hChild3 );
	Assert_ValidAndNonNULL  ( hChild4 );
	Assert_ValidAndNonNULL  ( hChild5 );
	Assert_ValidAndNonNULL  ( hGrandChild1 );
	Assert_ValidAndNonNULL  ( hGrandChild2 );
	Assert_ValidAndNonNULL  ( hGrandChild3 );
	Assert_ValidAndNonNULL  ( hGrandChild4 );
	Assert_ValidAndNonNULL  ( hGrandChild5 );
	Assert_ValidAndNonNULL  ( hGrandChild6 );
	Assert_ValidAndNonNULL  ( hGrandChild7 );

	g_pDataModel->Redo();

	g_pDmElementFramework->BeginEdit();

	int nRedoCount = g_pDataModel->GetAllocatedElementCount();
	Shipping_Assert( nRedoCount == nEditCount );

	Assert_ValidAndNonNULL  ( hRoot );
	Assert_ValidAndNonNULL  ( hChild1 );
	Assert_ValidAndNonNULL  ( hChild2 );
	Assert_InvalidButNonNULL( hChild3 );
	Assert_ValidAndNonNULL  ( hChild4 );
	Assert_ValidAndNonNULL  ( hChild5 );
	Assert_ValidAndNonNULL  ( hGrandChild1 );
	Assert_ValidAndNonNULL  ( hGrandChild2 );
	Assert_ValidAndNonNULL  ( hGrandChild3 );
	Assert_ValidAndNonNULL  ( hGrandChild4 );
	Assert_ValidAndNonNULL  ( hGrandChild5 );
	Assert_ValidAndNonNULL  ( hGrandChild6 );
	Assert_ValidAndNonNULL  ( hGrandChild7 );

	g_pDataModel->ClearUndo();

	g_pDmElementFramework->BeginEdit();

	int nClearUndoCount = g_pDataModel->GetAllocatedElementCount();
	Shipping_Assert( nClearUndoCount == nCreateCount - 6 );

	Assert_ValidAndNonNULL  ( hRoot );
	Assert_ValidAndNonNULL  ( hChild1 );
	Assert_ValidAndNonNULL  ( hChild2 );
	Assert_InvalidAndNULL   ( hChild3 );
	Assert_ValidAndNonNULL  ( hChild4 );
	Assert_ValidAndNonNULL  ( hChild5 );
	Assert_InvalidAndNULL   ( hGrandChild1 );
	Assert_InvalidAndNULL   ( hGrandChild2 );
	Assert_InvalidAndNULL   ( hGrandChild3 );
	Assert_ValidAndNonNULL  ( hGrandChild4 );
	Assert_InvalidAndNULL   ( hGrandChild5 );
	Assert_InvalidAndNULL   ( hGrandChild6 );
	Assert_ValidAndNonNULL  ( hGrandChild7 );

	g_pDataModel->RemoveFileId( fileid );
}
