//========= Copyright Valve Corporation, All rights reserved. ============//
// -------------------------------------------------------
// DO NOT EDIT
//   This file was generated from portal\portal_gcschema.sch by SchemaCompiler.EXE
//		on Mon Feb 22 13:22:55 2010
// -------------------------------------------------------
#ifndef PORTAL_GCSCHEMA_H
#define PORTAL_GCSCHEMA_H
#ifdef _WIN32
#pragma once
#endif

#include "gcsdk/gcschema.h"
#pragma pack(push, 1)

//-----------------------------------------------------------------------------
// GameAccount
//
//-----------------------------------------------------------------------------

class CSchGameAccount : public GCSDK::CRecordBase
{
public:
	const static int k_iTable = 0;
	CSchGameAccount();
	int GetITable() const;
	CSchGameAccount( const CSchGameAccount &that );
	void operator=( const CSchGameAccount &that );

	uint32 m_unAccountID;	// Account ID of the user
	uint32 m_unRewardPoints;	// number of timed reward points (coplayed minutes) for this user
	uint32 m_unPointCap;	// Current maximum number of points
	RTime32 m_unLastCapRollover;	// Last time the player's cap was adjusted

	static int m_nPrimaryKeyID;

	const static int k_iField_unAccountID = 0;
	const static int k_iField_unRewardPoints = 1;
	const static int k_iField_unPointCap = 2;
	const static int k_iField_unLastCapRollover = 3;
	const static int k_iFieldMax = 4;
};

//-----------------------------------------------------------------------------
// GameAccountClient
//
//-----------------------------------------------------------------------------

class CSchGameAccountClient : public GCSDK::CRecordBase
{
public:
	const static int k_iTable = 1;
	CSchGameAccountClient();
	int GetITable() const;
	CSchGameAccountClient( const CSchGameAccountClient &that );
	void operator=( const CSchGameAccountClient &that );

	uint32 m_unAccountID;	// Item Owner

	static int m_nPrimaryKeyID;

	const static int k_iField_unAccountID = 0;
	const static int k_iFieldMax = 1;
};

namespace PORTAL_GCSCHEMA
{
// ITABLE_STATS_BEGIN is the number of the first stats table;
// this should be one more than the number of the last data table.
const int ITABLE_STATS_BEGIN = 2;

const int k_iTableStatsFirst = -1;
const int k_iTableStatsMax = -1;
const int NUM_BASE_STATS_TABLES = 0;

extern void GenerateIntrinsicSQLSchema( GCSDK::CSchemaFull &schemaFull );

}
#pragma pack(pop)
#endif // PORTAL_GCSCHEMA_H
