//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds WarData
//
// $NoKeywords: $
//=============================================================================//

#ifndef TFLADDERDATA_H
#define TFLADDERDATA_H
#ifdef _WIN32
#pragma once
#endif

#include "gcsdk/protobufsharedobject.h"
#include "tf_gcmessages.h"
#if defined (CLIENT_DLL) || defined (GAME_DLL)
#include "gc_clientsystem.h"
#endif

#ifdef GC
#include "tf_gc.h"
#else
#include "tf_matchmaking_shared.h"
#endif


//---------------------------------------------------------------------------------
// Purpose: The shared object that contains a ladder player's stats		
//---------------------------------------------------------------------------------
class CSOTFLadderData : public GCSDK::CProtoBufSharedObject< CSOTFLadderPlayerStats, k_EEConTypeLadderData >
{
public:
	CSOTFLadderData();
	CSOTFLadderData( uint32 unAccountID, EMatchGroup eMatchGroup );
#ifdef GC
	DECLARE_CLASS_MEMPOOL( CSOTFLadderData );

	virtual bool BIsKeyLess( const CSharedObject & soRHS ) const OVERRIDE;

	virtual bool BYieldingAddInsertToTransaction( GCSDK::CSQLAccess & sqlAccess ) OVERRIDE;
	virtual bool BYieldingAddWriteToTransaction( GCSDK::CSQLAccess & sqlAccess, const CUtlVector< int > &fields ) OVERRIDE;
	virtual bool BYieldingAddRemoveToTransaction( GCSDK::CSQLAccess & sqlAccess ) OVERRIDE;

	void WriteToRecord( CSchLadderData *pLadderData ) const;
	void ReadFromRecord( const CSchLadderData &ladderData );
#endif // GC
};


CSOTFLadderData *YieldingGetPlayerLadderDataBySteamID( const CSteamID &steamID, EMatchGroup nMatchGroup );
#ifndef GC
CSOTFLadderData *GetLocalPlayerLadderData( EMatchGroup nMatchGroup );	// TODO: GetSeasonID()
#endif // !GC

//---------------------------------------------------------------------------------
// Purpose: The shared object that contains stats from a specific match - for match history on the client
//---------------------------------------------------------------------------------
class CSOTFMatchResultPlayerInfo : public GCSDK::CProtoBufSharedObject< CSOTFMatchResultPlayerStats, k_EEConTypeMatchResultPlayerInfo >
{
public:
	CSOTFMatchResultPlayerInfo();
#ifdef GC
	DECLARE_CLASS_MEMPOOL( CSOTFMatchResultPlayerInfo );
	CSOTFMatchResultPlayerInfo( uint32 unAccountID );

	virtual bool BIsKeyLess( const CSharedObject & soRHS ) const OVERRIDE;

	virtual bool BYieldingAddInsertToTransaction( GCSDK::CSQLAccess & sqlAccess ) OVERRIDE;
	virtual bool BYieldingAddWriteToTransaction( GCSDK::CSQLAccess & sqlAccess, const CUtlVector< int > &fields ) OVERRIDE;
	virtual bool BYieldingAddRemoveToTransaction( GCSDK::CSQLAccess & sqlAccess ) OVERRIDE;

	void WriteToRecord( CSchMatchResultPlayerInfo *pMatchInfo ) const;
	void ReadFromRecord( const CSchMatchResultPlayerInfo &matchInfo );
#endif // GC
};

#ifndef GC
void GetLocalPlayerMatchHistory( EMatchGroup nMatchGroup, CUtlVector < CSOTFMatchResultPlayerStats > &vecMatchesOut );
#endif // !GC

#endif // TFLADDERDATA_H
