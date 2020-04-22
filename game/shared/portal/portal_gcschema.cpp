//========= Copyright Valve Corporation, All rights reserved. ============//
// -------------------------------------------------------
// DO NOT EDIT
//   This file was generated from portal\portal_gcschema.sch by SchemaCompiler.EXE
//		on Mon Feb 22 13:22:55 2010
// -------------------------------------------------------

#include "cbase.h"
#include "portal_gcschema.h"

CSchGameAccount::CSchGameAccount()
{
	memset( PubRecordFixed(), 0, CubRecordFixed() );
}
int CSchGameAccount::GetITable() const { return k_iTable; }
CSchGameAccount::CSchGameAccount( const CSchGameAccount &that ) { *this = that; }
void CSchGameAccount::operator=( const CSchGameAccount &that ) { CRecordBase::operator =( that ); }


CSchGameAccountClient::CSchGameAccountClient()
{
	memset( PubRecordFixed(), 0, CubRecordFixed() );
}
int CSchGameAccountClient::GetITable() const { return k_iTable; }
CSchGameAccountClient::CSchGameAccountClient( const CSchGameAccountClient &that ) { *this = that; }
void CSchGameAccountClient::operator=( const CSchGameAccountClient &that ) { CRecordBase::operator =( that ); }


// statics for index IDs

int CSchGameAccount::m_nPrimaryKeyID;
int CSchGameAccountClient::m_nPrimaryKeyID;

// other initializers


// run-time initializer

namespace PORTAL_GCSCHEMA
{
void GenerateIntrinsicSQLSchema( GCSDK::CSchemaFull &schemaFull )
{
	GCSDK::CSchema *pSchema;
	pSchema = schemaFull.AddNewSchema();
	schemaFull.SetITable( pSchema, CSchGameAccount::k_iTable ); // 0
	pSchema->SetESchemaCatalog( GCSDK::k_ESchemaCatalogMain );
	pSchema->SetName( "GameAccount" );
	pSchema->EnsureFieldCount( CSchGameAccount::k_iFieldMax );
	pSchema->SetReportingInterval( 0 );
	pSchema->AddField( "unAccountID", "AccountID", k_EGCSQLType_int32, sizeof( uint32 ), 0, true, 0 );
	pSchema->AddField( "unRewardPoints", "RewardPoints", k_EGCSQLType_int32, sizeof( uint32 ), 0, true, 0 );
	pSchema->AddField( "unPointCap", "PointCap", k_EGCSQLType_int32, sizeof( uint32 ), 0, true, 0 );
	pSchema->AddField( "unLastCapRollover", "LastCapRollover", k_EGCSQLType_int32, sizeof( RTime32 ), 0, true, 0 );
	CSchGameAccount::m_nPrimaryKeyID = pSchema->PrimaryKey( true, 100, "unAccountID" );
	pSchema->SetTestWipePolicy( GCSDK::k_EWipePolicyWipeForAllTests );
	pSchema->SetBAllowWipeTableInProd( false );
	pSchema->CalcOffsets();
	schemaFull.CheckSchema( pSchema, CSchGameAccount::k_iFieldMax, sizeof( CSchGameAccount ) - sizeof( GCSDK::CRecordBase ) );
	pSchema->PrepareForUse();

	pSchema = schemaFull.AddNewSchema();
	schemaFull.SetITable( pSchema, CSchGameAccountClient::k_iTable ); // 1
	pSchema->SetESchemaCatalog( GCSDK::k_ESchemaCatalogMain );
	pSchema->SetName( "GameAccountClient" );
	pSchema->EnsureFieldCount( CSchGameAccountClient::k_iFieldMax );
	pSchema->SetReportingInterval( 0 );
	pSchema->AddField( "unAccountID", "AccountID", k_EGCSQLType_int32, sizeof( uint32 ), 0, true, 0 );
	CSchGameAccountClient::m_nPrimaryKeyID = pSchema->PrimaryKey( true, 80, "unAccountID" );
	pSchema->SetTestWipePolicy( GCSDK::k_EWipePolicyWipeForAllTests );
	pSchema->SetBAllowWipeTableInProd( false );
	pSchema->CalcOffsets();
	schemaFull.CheckSchema( pSchema, CSchGameAccountClient::k_iFieldMax, sizeof( CSchGameAccountClient ) - sizeof( GCSDK::CRecordBase ) );
	pSchema->PrepareForUse();


	schemaFull.FinishInit();
}
} // namespace 