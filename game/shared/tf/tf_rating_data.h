//========= Copyright Valve Corporation, All rights reserved. ============//

#ifndef TF_RATING_DATA_H
#define TF_RATING_DATA_H
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
// Purpose: The shared object that contains a specific MM rating
//---------------------------------------------------------------------------------
class CTFRatingData : public GCSDK::CProtoBufSharedObject< CSOTFRatingData, k_EProtoObjectTFRatingData, /* bPublicMutable */ false >
{
public:
	CTFRatingData();
	CTFRatingData( uint32 unAccountID, EMMRating eRatingType, const MMRatingData_t &ratingData );

#ifdef GC
	CTFRatingData( uint32 unAccountID, EMMRating eRatingType );	// Fills with Default rating for backend
#endif

	// Helpers
	static CTFRatingData *YieldingGetPlayerRatingDataBySteamID( const CSteamID &steamID, EMMRating eRatingType );

#ifdef GC
	DECLARE_CLASS_MEMPOOL( CTFRatingData );

	// Mutability is purposefully private, only CTFSharedObjectCache::BYieldingUpdatePlayerRating should be generating
	// rating changes.
	friend class CTFSharedObjectCache;

	// Direct database & client shared SO
	// (Some ratings may not be shared with clients, though)

	virtual bool BIsNetworked() const OVERRIDE { return true; }
	virtual bool BIsDatabaseBacked() const OVERRIDE { return true; }

	virtual bool BIsKeyLess( const CSharedObject & soRHS ) const OVERRIDE;

	// We purposefully pass false to our template to make Obj() immutable -- these calls currently are just hooked up to
	// assert.  All database writes should go through BYieldingUpdateRatingAndAddToTransaction, or it's SOCache helper.
	// If we have a need to do arbitrary writes of this type of object we should hook it up in a way that doesn't
	// encourage mis-use that bypasses audit record creation.
	virtual bool BYieldingAddInsertToTransaction( GCSDK::CSQLAccess & sqlAccess ) OVERRIDE;
	virtual bool BYieldingAddWriteToTransaction( GCSDK::CSQLAccess & sqlAccess, const CUtlVector< int > &fields ) OVERRIDE;
	virtual bool BYieldingAddRemoveToTransaction( GCSDK::CSQLAccess & sqlAccess ) OVERRIDE;

	void WriteToRecord( CSchRatingData *pRatingData ) const;
	void ReadFromRecord( const CSchRatingData &ladderData );

	// CTFSubscriberMessageFilter allows this type to filter flags on a per-object basis, so we can not send certain
	// rating types down.
	uint32_t FilterSendFlags( uint32_t unTypeFlags ) const;

private:
	// Updates this rating object and stages writes, including audit entries, to the given transaction.  We purposefully
	// make ourselves not publically mutable to ensure callers go through this path to generate audit records.  See
	// CTFSharedObjectCache::BYieldingUpdatePlayerRating for the SOCache path to calling this.
	bool BYieldingUpdateRatingAndAddToTransaction( CSQLAccess &transaction,
	                                               const MMRatingData_t &newData,
	                                               EMMRatingSource eAdjustmentSource,
	                                               uint64_t unSourceEventID,
	                                               RTime32 rtTimestamp );
#endif // GC

	// Private on the GC to encourage callers to go through the proper lookup in CTFSharedObjectCache (GC) or
	// GetLocalPlayerRatingData (client)
	MMRatingData_t GetRatingData();
};

#endif // TF_RATING_DATA_H
