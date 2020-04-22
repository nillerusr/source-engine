//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds the CTFDualSummary
//
// $NoKeywords: $
//=============================================================================//

#ifndef TFDUELSUMMARY_H
#define TFDUELSUMMARY_H
#ifdef _WIN32
#pragma once
#endif

#include "gcsdk/protobufsharedobject.h"
#include "tf_gcmessages.h"


// do not re-order, stored in DB
enum eDuelStatus
{
	kDuelStatus_Loss,
	kDuelStatus_Tie,
	kDuelStatus_Win,
};

// do not re-order, stored in DB
enum eDuelEndReason
{
	kDuelEndReason_DuelOver,
	kDuelEndReason_PlayerDisconnected,
	kDuelEndReason_PlayerSwappedTeams,
	kDuelEndReason_LevelShutdown,
	kDuelEndReason_ScoreTiedAtZero,
	kDuelEndReason_PlayerKicked,
	kDuelEndReason_PlayerForceSwappedTeams,
	kDuelEndReason_ScoreTied,
	kDuelEndReason_Cancelled
};

const char *PchNameFromeDuelEndReason( eDuelEndReason eReason );

const uint32 kWinsPerLevel = 10;

//---------------------------------------------------------------------------------
// Purpose: 
//---------------------------------------------------------------------------------
class CTFDuelSummary : public GCSDK::CProtoBufSharedObject< CSOTFDuelSummary, k_EEconTypeDuelSummary >
{
#ifdef GC
	DECLARE_CLASS_MEMPOOL( CTFDuelSummary );
#endif

public:
#ifdef GC
	virtual bool BYieldingAddInsertToTransaction( GCSDK::CSQLAccess & sqlAccess );
	virtual bool BYieldingAddWriteToTransaction( GCSDK::CSQLAccess & sqlAccess, const CUtlVector< int > &fields );
	virtual bool BYieldingAddRemoveToTransaction( GCSDK::CSQLAccess & sqlAccess );

	void WriteToRecord( CSchDuelSummary *pDuelSummary ) const;
	void ReadFromRecord( const CSchDuelSummary & duelSummary );
#endif
};

#endif //TFDUELSUMMARY_H
