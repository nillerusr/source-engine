//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Unit test program  for DMX testing
//
// $NoKeywords: $
//=============================================================================//

#include "unitlib/unitlib.h"
#include "dmxloader/dmxloader.h"
#include "dmxloader/dmxelement.h"

struct TestStruct_t
{
	DmObjectId_t m_nId;
	bool m_bBool;
	int m_nInt;
	float m_flFloat;
	Color m_Color;
	Vector2D m_Vector2D;
	Vector m_Vector3D;
	Vector4D m_Vector4D;
	QAngle m_Angles;
	Quaternion m_Quaternion;
	VMatrix m_Matrix;
	char m_pStringBuf[256];
};

BEGIN_DMXELEMENT_UNPACK( TestStruct_t ) 
	DMXELEMENT_UNPACK_FIELD( "id_test", NULL, DmObjectId_t, m_nId )
	DMXELEMENT_UNPACK_FIELD( "bool_test", "1", bool, m_bBool )
	DMXELEMENT_UNPACK_FIELD( "int_test", "5", int, m_nInt )
	DMXELEMENT_UNPACK_FIELD( "float_test", "4.0", float, m_flFloat )
	DMXELEMENT_UNPACK_FIELD( "color_test", "200 200 200 200", Color, m_Color )
	DMXELEMENT_UNPACK_FIELD( "vector2d_test", "5.0 1.0", Vector2D, m_Vector2D )
	DMXELEMENT_UNPACK_FIELD( "vector3d_test", "5.0 1.0 -3.0", Vector, m_Vector3D )
	DMXELEMENT_UNPACK_FIELD( "vector4d_test", "5.0 1.0 -4.0 2.0", Vector4D, m_Vector4D )
	DMXELEMENT_UNPACK_FIELD( "qangle_test", "5.0 1.0 -3.0", QAngle, m_Angles )
	DMXELEMENT_UNPACK_FIELD( "quat_test", "5.0 1.0 -4.0 2.0", Quaternion, m_Quaternion )
	DMXELEMENT_UNPACK_FIELD( "vmatrix_test", NULL, VMatrix, m_Matrix )
	DMXELEMENT_UNPACK_FIELD_STRING( "string_test", "default", m_pStringBuf )
END_DMXELEMENT_UNPACK( TestStruct_t, s_TestStructUnpack )

void TestReadFile( CDmxElement *pRoot )
{
	VMatrix mattest, mat2test;
	MatrixBuildRotateZ( mattest, 45 );
	MatrixBuildRotateZ( mat2test, 30 );

	int i;
	unsigned char buftest[256];
	unsigned char buf2test[256];
	for ( i = 0; i < 256; ++i )
	{
		buftest[i] = i;
		buf2test[i] = 255 - i;
	}


	// Make sure everything was read in ok.
	AssertEquals( pRoot->GetValue<bool>( "bool_test" ), true );
	AssertEquals( pRoot->GetValue<int>( "int_test" ), 2 );
	AssertFloatEquals( pRoot->GetValue<float>( "float_test" ), 3.0f, 1e-3 );
	const Color& color = pRoot->GetValue<Color>( "color_test" );
	Shipping_Assert( color.r() == 0 && color.g() == 64 && color.b() == 128 && color.a() == 255 );
	const Vector2D& vec2D = pRoot->GetValue<Vector2D>( "vector2d_test" );
	Shipping_Assert( vec2D.x == 1.0f && vec2D.y == -1.0f );
	const Vector& vec3D = pRoot->GetValue<Vector>( "vector3d_test" );
	Shipping_Assert( vec3D.x == 1.0f && vec3D.y == -1.0f && vec3D.z == 0.0f );
	const Vector4D& vec4D = pRoot->GetValue<Vector4D>( "vector4d_test" );
	Shipping_Assert( vec4D.x == 1.0f && vec4D.y == -1.0f && vec4D.z == 0.0f && vec4D.w == 2.0f );
	const QAngle& ang = pRoot->GetValue<QAngle>( "qangle_test" );
	Shipping_Assert( ang.x == 0.0f && ang.y == 90.0f && ang.z == -90.0f );
	const Quaternion& quat = pRoot->GetValue<Quaternion>( "quat_test" );
	Shipping_Assert( quat.x == 1.0f && quat.y == -1.0f && quat.z == 0.0f && quat.w == 2.0f );

	const VMatrix& mat = pRoot->GetValue<VMatrix>( "vmatrix_test" );
	Shipping_Assert( MatricesAreEqual( mat, mattest, 1e-3 ) );

	Shipping_Assert( !Q_stricmp( pRoot->GetValueString( "string_test" ), "test" ) );
	const CUtlBinaryBlock& blob = pRoot->GetValue<CUtlBinaryBlock>( "binary_test" );
	Shipping_Assert( blob.Length() == 256 );
	Shipping_Assert( !memcmp( blob.Get(), buftest, 256 ) );

	CDmxElement *pElement7 = pRoot->GetValue<CDmxElement*>( "element_test" );
	Shipping_Assert( pElement7 != NULL );
	CDmxElement *pElement6 = pRoot->GetValue<CDmxElement*>( "shared_element_test" );
	Shipping_Assert( pElement6 != NULL );
	const CUtlVector< CDmxElement* >& elementList = pRoot->GetArray<CDmxElement*>( "children" );
	Shipping_Assert( elementList.Count() == 2 );
	CDmxElement *pElement2 = elementList[0];
	CDmxElement *pElement3 = elementList[1];
	Shipping_Assert( pElement2 != NULL && pElement3 != NULL );
	Shipping_Assert( pElement7->GetValue<CDmxElement*>( "shared_element_test" ) == pElement6 );
	const CUtlVector< CDmxElement* >& elementList3 = pElement6->GetArray<CDmxElement*>( "element_array_test" );
	CDmxElement *pElement4 = elementList3[0];
	CDmxElement *pElement5 = elementList3[1];

	const CUtlVector< bool > &boolVec = pElement2->GetArray<bool>( "bool_array_test" );
	Shipping_Assert( boolVec.Count() == 2 && boolVec[0] == false && boolVec[1] == true );

	const CUtlVector< int > &intVec = pElement2->GetArray<int>( "int_array_test" );
	Shipping_Assert( intVec.Count() == 3 && intVec[0] == 0 && intVec[1] == 1 && intVec[2] == 2 );

	const CUtlVector< float > &floatVec = pElement2->GetArray<float>( "float_array_test" );
	Shipping_Assert( floatVec.Count() == 3 && floatVec[0] == -1.0f && floatVec[1] == 0.0f && floatVec[2] == 1.0f );

	const CUtlVector< Color > &colorVec = pElement3->GetArray<Color>( "color_array_test" );
	Shipping_Assert( colorVec.Count() == 3 && colorVec[0].r() == 0 && colorVec[1].r() == 64 && colorVec[2].r() == 128 );

	const CUtlVector< Vector2D > &vec2DVec = pElement3->GetArray<Vector2D>( "vector2d_array_test" );
	Shipping_Assert( vec2DVec.Count() == 2 && vec2DVec[0].x == -1.0f && vec2DVec[1].x == 1.0f );

	const CUtlVector< Vector > &vec3DVec = pElement3->GetArray<Vector>( "vector3d_array_test" );
	Shipping_Assert( vec3DVec.Count() == 2 && vec3DVec[0].x == 1.0f && vec3DVec[1].x == 2.0f );

	const CUtlVector< Vector4D > &vec4DVec = pElement4->GetArray<Vector4D>( "vector4d_array_test" );
	Shipping_Assert( vec4DVec.Count() == 2 && vec4DVec[0].x == 1.0f && vec4DVec[1].x == 2.0f );

	const CUtlVector< QAngle > &angVec = pElement4->GetArray<QAngle>( "qangle_array_test" );
	Shipping_Assert( angVec.Count() == 2 && angVec[0].x == 1.0f && angVec[1].x == 2.0f );

	const CUtlVector< Quaternion > &quatVec = pElement4->GetArray<Quaternion>( "quat_array_test" );
	Shipping_Assert( quatVec.Count() == 2 && quatVec[0].x == 1.0f && quatVec[1].x == 2.0f );

	const CUtlVector< VMatrix > &matVec = pElement5->GetArray<VMatrix>( "vmatrix_array_test" );
	Shipping_Assert( matVec.Count() == 2  );
	Shipping_Assert( MatricesAreEqual( matVec[0], mattest, 1e-3 ) );
	Shipping_Assert( MatricesAreEqual( matVec[1], mat2test, 1e-3 ) );

	const CUtlVector< CUtlString > &stringVec = pElement5->GetArray<CUtlString>( "string_array_test" );
	Shipping_Assert( stringVec.Count() == 3 && !Q_stricmp( stringVec[2], "string3" ) );

	const CUtlVector< CUtlBinaryBlock > &binaryVec = pElement5->GetArray<CUtlBinaryBlock>( "binary_array_test" );
	Shipping_Assert( binaryVec.Count() == 2 && !memcmp( binaryVec[1].Get(), buf2test, 256 ) );

	const CUtlVector< DmObjectId_t > &idVec = pElement6->GetArray<DmObjectId_t>( "elementid_array_test" );
	Shipping_Assert( idVec.Count() == 3 );

	TestStruct_t testStruct;
	pRoot->UnpackIntoStructure( &testStruct, sizeof( testStruct ), s_TestStructUnpack );

	Shipping_Assert( testStruct.m_bBool == true );
	Shipping_Assert( testStruct.m_nInt == 2 );
	AssertFloatEquals( testStruct.m_flFloat, 3.0f, 1e-3 );
	Shipping_Assert( testStruct.m_Color.r() == 0 && testStruct.m_Color.g() == 64 && testStruct.m_Color.b() == 128 && testStruct.m_Color.a() == 255 );
	Shipping_Assert( testStruct.m_Vector2D.x == 1.0f && testStruct.m_Vector2D.y == -1.0f );
	Shipping_Assert( testStruct.m_Vector3D.x == 1.0f && testStruct.m_Vector3D.y == -1.0f && testStruct.m_Vector3D.z == 0.0f );
	Shipping_Assert( testStruct.m_Vector4D.x == 1.0f && testStruct.m_Vector4D.y == -1.0f && testStruct.m_Vector4D.z == 0.0f && testStruct.m_Vector4D.w == 2.0f );
	Shipping_Assert( testStruct.m_Angles.x == 0.0f && testStruct.m_Angles.y == 90.0f && testStruct.m_Angles.z == -90.0f );
	Shipping_Assert( testStruct.m_Quaternion.x == 1.0f && testStruct.m_Quaternion.y == -1.0f && testStruct.m_Quaternion.z == 0.0f && testStruct.m_Quaternion.w == 2.0f );
	Shipping_Assert( MatricesAreEqual( testStruct.m_Matrix, mattest, 1e-3 ) );
	Shipping_Assert( !Q_stricmp( testStruct.m_pStringBuf, "test" ) );

	pElement6->UnpackIntoStructure( &testStruct, sizeof( testStruct ), s_TestStructUnpack );

	Shipping_Assert( testStruct.m_bBool == true );
	Shipping_Assert( testStruct.m_nInt == 5 );
	AssertFloatEquals( testStruct.m_flFloat, 4.0f, 1e-3 );
	Shipping_Assert( testStruct.m_Color.r() == 200 && testStruct.m_Color.g() == 200 && testStruct.m_Color.b() == 200 && testStruct.m_Color.a() == 200 );
	Shipping_Assert( testStruct.m_Vector2D.x == 5.0f && testStruct.m_Vector2D.y == 1.0f );
	Shipping_Assert( testStruct.m_Vector3D.x == 5.0f && testStruct.m_Vector3D.y == 1.0f && testStruct.m_Vector3D.z == -3.0f );
	Shipping_Assert( testStruct.m_Vector4D.x == 5.0f && testStruct.m_Vector4D.y == 1.0f && testStruct.m_Vector4D.z == -4.0f && testStruct.m_Vector4D.w == 2.0f );
	Shipping_Assert( testStruct.m_Angles.x == 5.0f && testStruct.m_Angles.y == 1.0f && testStruct.m_Angles.z == -3.0f );
	Shipping_Assert( testStruct.m_Quaternion.x == 5.0f && testStruct.m_Quaternion.y == 1.0f && testStruct.m_Quaternion.z == -4.0f && testStruct.m_Quaternion.w == 2.0f );
	Shipping_Assert( !Q_stricmp( testStruct.m_pStringBuf, "default" ) );
}

DEFINE_TESTCASE_NOSUITE( DmxLoaderTest )
{
	Msg( "Running dmx loader tests...\n" );
	
	CDmxElement *pRoot;
	bool bOk = UnserializeDMX( "dmxtestloader.dmx", NULL, false, &pRoot );
	Shipping_Assert( bOk );
	Shipping_Assert( pRoot );
	if ( pRoot )
	{
		TestReadFile( pRoot );
		CleanupDMX( pRoot );
	}

	bOk = UnserializeDMX( "dmxtestloadertext.dmx", NULL, true, &pRoot );
	Shipping_Assert( bOk );
	Shipping_Assert( pRoot );
	if ( pRoot )
	{
		TestReadFile( pRoot );
		CleanupDMX( pRoot );
	}

	// Test serialization
	bOk = UnserializeDMX( "dmxtestloader.dmx", NULL, false, &pRoot );
	Shipping_Assert( bOk );
	Shipping_Assert( pRoot );
	if ( pRoot )
	{
		bOk = SerializeDMX( "dmxtestscratch.dmx", NULL, false, pRoot );
		Shipping_Assert( bOk );
		CleanupDMX( pRoot );
	}
	CleanupDMX( pRoot );

	bOk = UnserializeDMX( "dmxtestscratch.dmx", NULL, false, &pRoot );
	Shipping_Assert( bOk );
	Shipping_Assert( pRoot );
	if ( pRoot )
	{
		TestReadFile( pRoot );
		CleanupDMX( pRoot );
	}
}
