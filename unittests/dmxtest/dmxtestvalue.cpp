//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Unit test program for DMX testing (testing the single-value operations)
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


DEFINE_TESTCASE_NOSUITE( DmxValueTest )
{
	Msg( "Running dmx single value tests...\n" );

	CDisableUndoScopeGuard sg;
	DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( "<RunValueTests>" );

	CDmElement *pElement = CreateElement< CDmElement >( "root", fileid );

	CDmElement *pElement2 = CreateElement<CDmElement>( "element1", fileid );
	Assert( pElement2 );
	CDmElement *pElement3 = CreateElement<CDmElement>( "element2", fileid );
	Assert( pElement3 );
	CDmeShape *pElement4 = CreateElement<CDmeShape>( "shape", fileid );
	Assert( pElement4 );

	CDmAttribute *pIntAttribute = pElement->SetValue( "int_test", 5 );
	CDmAttribute *pFloatAttribute = pElement->SetValue( "float_test", 4.5f );
	CDmAttribute *pBoolAttribute = pElement->SetValue( "bool_test", true );

	CDmAttribute *pAttribute = pElement->AddAttribute( "int_convert_test", AT_INT );

	// Type conversion set test
	pAttribute->SetValue( 5 );
	Shipping_Assert( pAttribute->GetValue<int>() == 5 );
	pAttribute->SetValue( 4.5f );
	Shipping_Assert( pAttribute->GetValue<int>() == 4 );
	pAttribute->SetValue( true );
	Shipping_Assert( pAttribute->GetValue<int>() == 1 );
	pAttribute->SetValue( pIntAttribute );
	Shipping_Assert( pAttribute->GetValue<int>() == 5 );
	pAttribute->SetValue( pFloatAttribute );
	Shipping_Assert( pAttribute->GetValue<int>() == 4 );
	pAttribute->SetValue( pBoolAttribute );
	Shipping_Assert( pAttribute->GetValue<int>() == 1 );

	pAttribute = pElement->AddAttribute( "bool_convert_test", AT_BOOL );

	// Type conversion set test
	pAttribute->SetValue( 5 );
	Shipping_Assert( pAttribute->GetValue<bool>() == true );
	pAttribute->SetValue( 4.5f );
	Shipping_Assert( pAttribute->GetValue<bool>() == true );
	pAttribute->SetValue( false );
	Shipping_Assert( pAttribute->GetValue<bool>() == false );
	pAttribute->SetValue( pIntAttribute );
	Shipping_Assert( pAttribute->GetValue<bool>() == true );
	pAttribute->SetValue( pFloatAttribute );
	Shipping_Assert( pAttribute->GetValue<bool>() == true );
	pAttribute->SetValue( pBoolAttribute );
	Shipping_Assert( pAttribute->GetValue<bool>() == true );

	pAttribute = pElement->AddAttribute( "float_convert_test", AT_FLOAT );

	// Type conversion set test
	pAttribute->SetValue( 5 );
	Shipping_Assert( pAttribute->GetValue<float>() == 5.0f );
	pAttribute->SetValue( 4.5f );
	Shipping_Assert( pAttribute->GetValue<float>() == 4.5f );
	pAttribute->SetValue( true );
	Shipping_Assert( pAttribute->GetValue<float>() == 1.0f );
	pAttribute->SetValue( pIntAttribute );
	Shipping_Assert( pAttribute->GetValue<float>() == 5.0f );
	pAttribute->SetValue( pFloatAttribute );
	Shipping_Assert( pAttribute->GetValue<float>() == 4.5f );
	pAttribute->SetValue( pBoolAttribute );
	Shipping_Assert( pAttribute->GetValue<float>() == 1.0f );

	// Type conversion set test
	QAngle angles( 90, 0, 0 );
	Quaternion quat;
	AngleQuaternion( angles, quat );

	pAttribute = pElement->AddAttribute( "qangle_convert_test", AT_QANGLE );
	pAttribute->SetValue( angles );
	Shipping_Assert( pAttribute->GetValue<QAngle>() == angles );
	pAttribute->SetValue( quat );
	Shipping_Assert( pAttribute->GetValue<QAngle>() == angles );

	pAttribute = pElement->AddAttribute( "quat_convert_test", AT_QUATERNION );
	pAttribute->SetValue( angles );
	Shipping_Assert( pAttribute->GetValue<Quaternion>() == quat );
	pAttribute->SetValue( quat );
	Shipping_Assert( pAttribute->GetValue<Quaternion>() == quat );

	g_pDataModel->RemoveFileId( fileid );
}
