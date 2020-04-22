//========= Copyright Valve Corporation, All rights reserved. ============//

#include "cbase.h"

#include "tf_rating_data.h"
#include "gcsdk/enumutils.h"

#ifdef CLIENT_DLL
#include "tf_matchmaking_shared.h"
#endif

using namespace GCSDK;

#if defined( GC )
#include "tf_matchmaking_rating.h"

IMPLEMENT_CLASS_MEMPOOL( CTFRatingData, 1000, UTLMEMORYPOOL_GROW_SLOW );
// Make sure you visit all these if you add new fields to rating data
#define ASSERT_LAST_FIELD( foo ) COMPILE_TIME_ASSERT( CSchRatingData::k_iFieldMax == CSchRatingData::k_iField_##foo + 1 )
#else // defined( GC )
#define ASSERT_LAST_FIELD( foo ) // No Sch on client. As long as it fails somewhere.
#endif // defined( GC )

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Get player's rating data by steamID.  Returns NULL if it doesn't exist
//-----------------------------------------------------------------------------
CTFRatingData *CTFRatingData::YieldingGetPlayerRatingDataBySteamID( const CSteamID &steamID, EMMRating eRatingType )
{
#ifdef GC_DLL
	GCSDK::CGCSharedObjectCache *pSOCache = GGCEcon()->YieldingFindOrLoadSOCache( steamID );
#else
	GCSDK::CGCClientSharedObjectCache *pSOCache = GCClientSystem()->GetSOCache( steamID );
#endif
	if ( pSOCache )
	{
		auto *pTypeCache = pSOCache->FindTypeCache( CTFRatingData::k_nTypeID );
		if ( pTypeCache )
		{
			for ( uint32 i = 0; i < pTypeCache->GetCount(); ++i )
			{
				CTFRatingData *pRatingData = (CTFRatingData*)pTypeCache->GetObject( i );
				if ( eRatingType == (EMMRating)pRatingData->Obj().rating_type() )
				{
					return pRatingData;
				}
			}
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
#ifdef GC
CTFRatingData::CTFRatingData( uint32 unAccountID, EMMRating eRatingType )
	: CTFRatingData( unAccountID, eRatingType, ITFMMRatingBackend::GetRatingBackend( eRatingType ).GetDefaultRatingData() )
{}
#endif

//-----------------------------------------------------------------------------
CTFRatingData::CTFRatingData()
{}

//-----------------------------------------------------------------------------
CTFRatingData::CTFRatingData( uint32 unAccountID, EMMRating eRatingType, const MMRatingData_t &ratingData )
{
	MutObj().set_account_id( unAccountID );
	MutObj().set_rating_type( eRatingType );

	MutObj().set_rating_primary( ratingData.unRatingPrimary );
	MutObj().set_rating_secondary( ratingData.unRatingSecondary );
	MutObj().set_rating_tertiary( ratingData.unRatingTertiary );
	// MMRatingData_t fields
	ASSERT_LAST_FIELD( unRatingTertiary );
}

#ifdef GC

//-----------------------------------------------------------------------------
bool CTFRatingData::BIsKeyLess( const CSharedObject & soRHS ) const
{
	Assert( GetTypeID() == soRHS.GetTypeID() );
	const CSOTFRatingData &obj = Obj();
	const CSOTFRatingData &rhs = ( static_cast< const CTFRatingData & >( soRHS ) ).Obj();

	if ( obj.account_id() < rhs.account_id() ) return true;
	if ( obj.account_id() > rhs.account_id() ) return false;

	return obj.rating_type() < rhs.rating_type();
}

//-----------------------------------------------------------------------------
// Purpose: This is a database backed object, but, all mutations to these
//          should go through BYieldingUpdateRatingAndAddToTransaction and its
//          CTFSharedObjectCache user.
//
// Not implementing to prevent mis-use until there's a path that legitimately
// needs them to work.
//-----------------------------------------------------------------------------
bool CTFRatingData::BYieldingAddInsertToTransaction( GCSDK::CSQLAccess & sqlAccess )
	{ Assert( false ); return false; }
bool CTFRatingData::BYieldingAddWriteToTransaction( GCSDK::CSQLAccess & sqlAccess, const CUtlVector< int > &fields )
	{ Assert( false ); return false; }
bool CTFRatingData::BYieldingAddRemoveToTransaction( GCSDK::CSQLAccess & sqlAccess )
	{ Assert( false ); return false; }

//-----------------------------------------------------------------------------
void CTFRatingData::WriteToRecord( CSchRatingData *pRatingData ) const
{
	pRatingData->m_unAccountID = Obj().account_id();
	pRatingData->m_nRatingType = Obj().rating_type();
	pRatingData->m_unRatingPrimary = Obj().rating_primary();
	pRatingData->m_unRatingSecondary = Obj().rating_secondary();
	pRatingData->m_unRatingTertiary = Obj().rating_tertiary();
	ASSERT_LAST_FIELD( unRatingTertiary );
}

//-----------------------------------------------------------------------------
void CTFRatingData::ReadFromRecord( const CSchRatingData &ratingData )
{
	MutObj().set_account_id( ratingData.m_unAccountID );
	MutObj().set_rating_type( ratingData.m_nRatingType );
	MutObj().set_rating_primary( ratingData.m_unRatingPrimary );
	MutObj().set_rating_secondary( ratingData.m_unRatingSecondary );
	MutObj().set_rating_tertiary( ratingData.m_unRatingTertiary );
	ASSERT_LAST_FIELD( unRatingTertiary );
}

//-----------------------------------------------------------------------------
// Purpose: Hook for CTFSubscriberMessageFilter to allow us to stop certain
//          rating types from going to certain subscribers.
//-----------------------------------------------------------------------------
uint32_t CTFRatingData::FilterSendFlags( uint32_t unTypeFlags ) const
{
	auto &backend = ITFMMRatingBackend::GetRatingBackend( (EMMRating)Obj().rating_type() );
	ITFMMRatingBackend::ERatingFlags eBackendFlags = backend.GetRatingFlags();

	bool bNetworked = ( eBackendFlags & ITFMMRatingBackend::eRatingFlag_Networked );
	bool bOwnerOnly = ( eBackendFlags & ITFMMRatingBackend::eRatingFlag_OwnerOnly );

	uint32_t unRemoveFlags = 0u;

	if ( !bNetworked || bOwnerOnly )
	{
		unRemoveFlags |= k_ESOFlag_SendToOtherGameservers;
		unRemoveFlags |= k_ESOFlag_SendToOtherUsers;
	}

	if ( !bNetworked )
	{
		unRemoveFlags |= k_ESOFlag_SendToOwner;
	}

	return unTypeFlags & ~unRemoveFlags;
}

//-----------------------------------------------------------------------------
// Purpose: Updates the rating fields from an MMRatingData_t, add a write or
//          insert to the given transaction including an audit entry.
//-----------------------------------------------------------------------------
bool CTFRatingData::BYieldingUpdateRatingAndAddToTransaction( CSQLAccess &transaction,
                                                              const MMRatingData_t &newData,
                                                              EMMRatingSource eAdjustmentSource,
                                                              uint64_t unSourceEventID,
                                                              RTime32 rtTimestamp )
{
	// MMRatingData_t fields
	ASSERT_LAST_FIELD( unRatingTertiary );
	CSchRatingHistory schHistory;
	schHistory.m_unAccountID          = Obj().account_id();
	schHistory.m_nRatingType          = Obj().rating_type();
	schHistory.m_nRatingSource        = eAdjustmentSource;
	schHistory.m_unEventID            = unSourceEventID;
	schHistory.m_rtime32Timestamp     = rtTimestamp;
	schHistory.m_unOldRatingPrimary   = Obj().rating_primary();
	schHistory.m_unOldRatingSecondary = Obj().rating_secondary();
	schHistory.m_unOldRatingTertiary  = Obj().rating_tertiary();
	schHistory.m_unNewRatingPrimary   = newData.unRatingPrimary;
	schHistory.m_unNewRatingSecondary = newData.unRatingSecondary;
	schHistory.m_unNewRatingTertiary  = newData.unRatingTertiary;

	// Update ourself
	// MMRatingData_t fields
	ASSERT_LAST_FIELD( unRatingTertiary );
	MutObj().set_rating_primary( newData.unRatingPrimary );
	MutObj().set_rating_secondary( newData.unRatingSecondary );
	MutObj().set_rating_tertiary( newData.unRatingTertiary );

	// Create record
	CSchRatingData schRatingData;
	WriteToRecord( &schRatingData );

	// Add to transaction
	return transaction.BYieldingInsertOrUpdateOnPK( &schRatingData ) &&
	       transaction.BYieldingInsertRecord( &schHistory );
}

#endif	// GC

//-----------------------------------------------------------------------------
// Purpose: Reads out the rating fields for rating systems to use
//-----------------------------------------------------------------------------
MMRatingData_t CTFRatingData::GetRatingData()
{
	MMRatingData_t rvoData;
	rvoData.unRatingPrimary = Obj().rating_primary();
	rvoData.unRatingSecondary = Obj().rating_secondary();
	rvoData.unRatingTertiary = Obj().rating_tertiary();

	// If you expanded MMRatingData_t
	ASSERT_LAST_FIELD( unRatingTertiary );

	return rvoData;
}
