//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Base class for objects that are kept in synch between client and server
//
//=============================================================================
#include "stdafx.h"
#include "gcsdk_gcmessages.pb.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{

//----------------------------------------------------------------------------
// Purpose: Map of all the factory functions for all CSharedObject classes
//----------------------------------------------------------------------------
	CUtlMap<int, CSharedObject::SharedObjectInfo_t> CSharedObject::sm_mapFactories(DefLessFunc(int));


//----------------------------------------------------------------------------
// Purpose: Registers a new CSharedObject class
//----------------------------------------------------------------------------
void CSharedObject::RegisterFactory( int nTypeID, SOCreationFunc_t fnFactory, uint32 unFlags, const char *pchClassName )
{
	SharedObjectInfo_t info;
	info.m_pFactoryFunction = fnFactory;
	info.m_unFlags = unFlags;
	info.m_pchClassName = pchClassName;
	info.m_sBuildCacheSubNodeName.Format( "BuildCacheSubscribed(%s)", pchClassName );
	info.m_sCreateNodeName.Format( "Create(%s)", pchClassName );
	info.m_sUpdateNodeName.Format( "Update(%s)", pchClassName );
	sm_mapFactories.InsertOrReplace( nTypeID, info );

	//register this class with our SO stats as well
	#ifdef GC
		g_SharedObjectStats.RegisterSharedObjectType( nTypeID, pchClassName );
	#endif //GC
}


//----------------------------------------------------------------------------
// Purpose: Creates a new CSharedObject instance of the specified type ID
//----------------------------------------------------------------------------
CSharedObject *CSharedObject::Create( int nTypeID )
{
	int nIndex = sm_mapFactories.Find( nTypeID );
	AssertMsg1( sm_mapFactories.IsValidIndex( nIndex ), "Probably failed to set object type (%d) on the server/client.\n", nTypeID );
	if( sm_mapFactories.IsValidIndex( nIndex ) ) 
	{
		return sm_mapFactories[nIndex].m_pFactoryFunction();
	}
	else
	{
		return NULL;
	}
}

//----------------------------------------------------------------------------
// Purpose: Various accessors for static class data
//----------------------------------------------------------------------------
uint32 CSharedObject::GetTypeFlags( int nTypeID )
{
	int nIndex = sm_mapFactories.Find( nTypeID );
	if( !sm_mapFactories.IsValidIndex( nIndex ) ) 
		return 0;
	else
		return sm_mapFactories[nIndex].m_unFlags;
}

const char *CSharedObject::PchClassName( int nTypeID )
{
	int nIndex = sm_mapFactories.Find( nTypeID );
	if( !sm_mapFactories.IsValidIndex( nIndex ) ) 
		return 0;
	else
		return sm_mapFactories[nIndex].m_pchClassName;

}

const char *CSharedObject::PchClassBuildCacheNodeName( int nTypeID )
{
	int nIndex = sm_mapFactories.Find( nTypeID );
	if( !sm_mapFactories.IsValidIndex( nIndex ) ) 
		return 0;
	else
		return sm_mapFactories[nIndex].m_sBuildCacheSubNodeName.Get();
}

const char *CSharedObject::PchClassCreateNodeName( int nTypeID )
{
	int nIndex = sm_mapFactories.Find( nTypeID );
	if( !sm_mapFactories.IsValidIndex( nIndex ) ) 
		return 0;
	else
		return sm_mapFactories[nIndex].m_sCreateNodeName.Get();
}

const char *CSharedObject::PchClassUpdateNodeName( int nTypeID )
{
	int nIndex = sm_mapFactories.Find( nTypeID );
	if( !sm_mapFactories.IsValidIndex( nIndex ) ) 
		return 0;
	else
		return sm_mapFactories[nIndex].m_sUpdateNodeName.Get();
}


//----------------------------------------------------------------------------
// Purpose: Figures out if the primary keys on these two objects are the same
//			using the subclass-defined less function
//----------------------------------------------------------------------------
bool CSharedObject::BIsKeyEqual( const CSharedObject & soRHS ) const
{
	// Make sure they are the same type.
	if ( GetTypeID() != soRHS.GetTypeID() )
		return false;

	return !BIsKeyLess( soRHS ) && !soRHS.BIsKeyLess( *this );
}



#ifdef GC

//----------------------------------------------------------------------------
// Purpose: Sends a create message for this shared object to the specified 
//			steam ID.
//----------------------------------------------------------------------------
bool CSharedObject::BSendCreateToSteamID( const CSteamID & steamID, const CSteamID & steamIDOwner, uint64 ulVersion ) const
{
	// Don't send these while shutting down.  We'll use higher-level
	// connection-oriented messages instead
	if ( GGCBase()->GetIsShuttingDown() )
		return false;

	CProtoBufMsg< CMsgSOSingleObject > msg( k_ESOMsg_Create );
	msg.Body().set_owner( steamIDOwner.ConvertToUint64() );
	msg.Body().set_type_id( GetTypeID() );
	msg.Body().set_version( ulVersion );
	if( !BAddToMessage( msg.Body().mutable_object_data() ) )
	{
		EmitWarning( SPEW_SHAREDOBJ, SPEW_ALWAYS, "Failed to add create fields to message in create" );
		return false;
	}

	return GGCBase()->BSendGCMsgToClient( steamID, msg );
}


//----------------------------------------------------------------------------
// Purpose: Sends a destroy message for this object to the specified steam ID
//----------------------------------------------------------------------------
bool CSharedObject::BSendDestroyToSteamID( const CSteamID & steamID, const CSteamID & steamIDOwner, uint64 ulVersion ) const
{
	// Don't send these while shutting down.  We'll use higher-level
	// connection-oriented messages instead
	if ( GGCBase()->GetIsShuttingDown() )
		return false;

	CProtoBufMsg< CMsgSOSingleObject > msg( k_ESOMsg_Destroy );
	msg.Body().set_owner( steamIDOwner.ConvertToUint64() );
	msg.Body().set_type_id( GetTypeID() );
	msg.Body().set_version( ulVersion );
	if( !BAddDestroyToMessage( msg.Body().mutable_object_data() ) )
	{
		EmitWarning( SPEW_SHAREDOBJ, SPEW_ALWAYS, "Failed to add key to message in create" );
		return false;
	}
	return GGCBase()->BSendGCMsgToClient( steamID, msg );
}


//----------------------------------------------------------------------------
// Purpose: Wraps BYieldingAddInsertToTransaction with a transaction and a 
//			commit.
//----------------------------------------------------------------------------
bool CSharedObject::BYieldingAddToDatabase()
{
	CSQLAccess sqlAccess;
	sqlAccess.BBeginTransaction( CFmtStr( "CSharedObject::BYieldingAddToDatabase Type %d", GetTypeID() ) );
	if( !BYieldingAddInsertToTransaction( sqlAccess ) )
	{
		sqlAccess.RollbackTransaction();
		return false;
	}

	return sqlAccess.BCommitTransaction();
}


//----------------------------------------------------------------------------
// Purpose: Wraps BYieldingAddWriteToTransaction with a transaction and a 
//			commit.
//----------------------------------------------------------------------------
bool CSharedObject::BYieldingWriteToDatabase( const CUtlVector< int > &fields )
{
	CSQLAccess sqlAccess;
	sqlAccess.BBeginTransaction( CFmtStr( "CSharedObject::BYieldingWriteToDatabase Type %d", GetTypeID() ) );
	if( !BYieldingAddWriteToTransaction( sqlAccess, fields ) )
	{
		sqlAccess.RollbackTransaction();
		return false;
	}

	if( sqlAccess.BCommitTransaction() )
	{
		return true;
	}
	else
	{
		return false;
	}
}


//----------------------------------------------------------------------------
// Purpose: Wraps BYieldingAddRemoveToTransaction with a transaction and a 
//			commit.
//----------------------------------------------------------------------------
bool CSharedObject::BYieldingRemoveFromDatabase()
{
	CSQLAccess sqlAccess;
	sqlAccess.BBeginTransaction( CFmtStr( "CSharedObject::BYieldingRemoveFromDatabase Type %d", GetTypeID() ) );
	if( !BYieldingAddRemoveToTransaction( sqlAccess ) )
	{
		sqlAccess.RollbackTransaction();
		return false;
	}

	return sqlAccess.BCommitTransaction();
}

//--------------------------------------------------------------------------------------------------------------------------
// CSharedObjectStats
//--------------------------------------------------------------------------------------------------------------------------

//our global stats for our SO objects
CSharedObjectStats g_SharedObjectStats;


CSharedObjectStats::CSharedObjectStats()
	: m_bCollectingStats( false )
	, m_nMicroSElapsed( 0 )
{
}

CSharedObjectStats::SOStats_t::SOStats_t()
	: m_nNumActive( 0 )
{
	ResetStats();
}

void CSharedObjectStats::SOStats_t::ResetStats()
{
	m_nNumCreated = 0;
	m_nNumDestroyed = 0;
	m_nNumSends = 0;
	m_nRawBytesSent = 0;
	m_nMultiplexedBytesSent = 0;
	m_nNumSubOwner = 0;
	m_nNumSubOtherUsers = 0;
	m_nNumSubGameServer = 0;
}

void CSharedObjectStats::StartCollectingStats()
{
	//do nothing if already collecting
	if( m_bCollectingStats )
		return;

	m_bCollectingStats = true;
	m_CollectTime.SetToJobTime();
}

void CSharedObjectStats::StopCollectingStats()
{
	if( !m_bCollectingStats )
		return;

	m_bCollectingStats = false;
	m_nMicroSElapsed = m_CollectTime.CServerMicroSecsPassed();
}

void CSharedObjectStats::RegisterSharedObjectType( int nTypeID, const char* pszTypeName )
{
	if( nTypeID < 0 )
	{
		AssertMsg2( false, "Error registering shared object type %d (%s), negative type ID's are not supported", nTypeID, pszTypeName );
		return;
	}

	//see if we need to grow our list to accommodate this type id
	if( nTypeID >= m_vTypeToIndex.Count() )
	{
		//make sure people aren't getting carried away with index ranges
		AssertMsg( nTypeID < 8 * 1024, "Warning: Using a very large type ID which can be inefficient for the shared object stats. Try to keep values to a smaller range" );
		int nOldSize = m_vTypeToIndex.Count();
		m_vTypeToIndex.AddMultipleToTail( nTypeID + 1 - nOldSize );
		for( int nCurrFill = nOldSize; nCurrFill < nTypeID; nCurrFill++ )
		{
			m_vTypeToIndex[ nCurrFill ] = knInvalidIndex;
		}
	}
	else if( m_vTypeToIndex[ nTypeID ] != knInvalidIndex )
	{
		//we have already registered something in this slot
		AssertMsg2( false, "Error registering shared object type %d (%s), may have multiple registrations of this type, check for conflicts", nTypeID, pszTypeName );
		return;
	}

	//we need to register this type by adding a new record, and updating our index
	int nNewIndex = m_Stats.AddToTail();
	m_Stats[ nNewIndex ].m_sName	= pszTypeName;
	m_Stats[ nNewIndex ].m_nTypeID	= nTypeID;
	m_vTypeToIndex[ nTypeID ] = nNewIndex;
}

void CSharedObjectStats::TrackSharedObjectLifetime( int nTypeID, int32 nCount )
{
	uint16 nIndex = ( m_vTypeToIndex.IsValidIndex( nTypeID ) ) ? m_vTypeToIndex[ nTypeID ] : knInvalidIndex;
	if( nIndex != knInvalidIndex )
	{
		m_Stats[ nIndex ].m_nNumActive += nCount;
	}
}

void CSharedObjectStats::TrackSharedObjectSendCreate( int nTypeID, uint32 nCount )
{
	uint16 nIndex = ( m_vTypeToIndex.IsValidIndex( nTypeID ) ) ? m_vTypeToIndex[ nTypeID ] : knInvalidIndex;
	if( nIndex != knInvalidIndex )
	{
		m_Stats[ nIndex ].m_nNumCreated += nCount;
	}
}

void CSharedObjectStats::TrackSharedObjectSendDestroy( int nTypeID, uint32 nCount )
{
	uint16 nIndex = (m_vTypeToIndex.IsValidIndex( nTypeID ) ) ? m_vTypeToIndex[ nTypeID ] : knInvalidIndex;
	if( nIndex != knInvalidIndex )
	{
		m_Stats[ nIndex ].m_nNumDestroyed += nCount;
	}
}

void CSharedObjectStats::TrackSubscription( int nTypeID, uint32 nFlags, uint32 nNumSubscriptions )
{
	if( !m_bCollectingStats )
		return;

	uint16 nIndex = ( m_vTypeToIndex.IsValidIndex( nTypeID ) ) ? m_vTypeToIndex[ nTypeID ] : knInvalidIndex;
	if( nIndex != knInvalidIndex )
	{
		if( nFlags & k_ESOFlag_SendToOwner )
			m_Stats[ nIndex ].m_nNumSubOwner += nNumSubscriptions;
		if( nFlags & k_ESOFlag_SendToOtherUsers )
			m_Stats[ nIndex ].m_nNumSubOtherUsers += nNumSubscriptions;
		if( nFlags & k_ESOFlag_SendToOtherGameservers )
			m_Stats[ nIndex ].m_nNumSubGameServer += nNumSubscriptions;
	}

}

void CSharedObjectStats::TrackSharedObjectSend( int nTypeID, uint32 nNumClients, uint32 nMsgSize )
{
	if( !m_bCollectingStats )
		return;

	uint16 nIndex = ( m_vTypeToIndex.IsValidIndex( nTypeID ) ) ? m_vTypeToIndex[ nTypeID ] : knInvalidIndex;
	if( nIndex != knInvalidIndex )
	{
		m_Stats[ nIndex ].m_nNumSends++;
		m_Stats[ nIndex ].m_nRawBytesSent += nMsgSize;
		m_Stats[ nIndex ].m_nMultiplexedBytesSent += nMsgSize * nNumClients;
	}
}

void CSharedObjectStats::ResetStats()
{
	FOR_EACH_VEC( m_Stats, nCurrSO )
	{
		m_Stats[ nCurrSO ].ResetStats();
	}
}

bool CSharedObjectStats::SortSOStatsSent( const SOStats_t* pLhs, const SOStats_t* pRhs )
{
	//highest bandwidth ones go up top
	if( pLhs->m_nMultiplexedBytesSent != pRhs->m_nMultiplexedBytesSent )
		return pLhs->m_nMultiplexedBytesSent > pRhs->m_nMultiplexedBytesSent;

	//otherwise sort by the name
	return pLhs->m_nTypeID < pRhs->m_nTypeID;
}

bool CSharedObjectStats::SortSOStatsSubscribe( const SOStats_t* pLhs, const SOStats_t* pRhs )
{
	//sort based on total number of subscriptions
	uint32 nLhsSub = pLhs->m_nNumSubOwner + pLhs->m_nNumSubOtherUsers + pLhs->m_nNumSubGameServer;
	uint32 nRhsSub = pRhs->m_nNumSubOwner + pRhs->m_nNumSubOtherUsers + pRhs->m_nNumSubGameServer;
	if( nLhsSub != nRhsSub )
		return nLhsSub > nRhsSub;	

	//otherwise sort by the name
	return pLhs->m_nTypeID < pRhs->m_nTypeID;
}

void CSharedObjectStats::ReportCollectedStats() const
{
	//build up a list of our stats, so we can sort them appropriately (also total how many bytes we send)
	uint64 nTotalRawBytes = 0;
	uint64 nTotalMultiplexBytes = 0;
	uint32 nTotalActive = 0;
	uint32 nTotalCreates = 0;
	uint32 nTotalFrees = 0;
	uint32 nTotalSends = 0;
	uint32 nTotalSubOwner = 0;
	uint32 nTotalSubOtherUsers = 0;
	uint32 nTotalSubGameServer = 0;

	CUtlVector< const SOStats_t* > sortedStats( 0, m_Stats.Count() );
	FOR_EACH_VEC( m_Stats, nCurrSO )
	{
		sortedStats.AddToTail( &( m_Stats[ nCurrSO ] ) );

		nTotalRawBytes			+= m_Stats[ nCurrSO ].m_nRawBytesSent;
		nTotalMultiplexBytes	+= m_Stats[ nCurrSO ].m_nMultiplexedBytesSent;
		nTotalActive			+= m_Stats[ nCurrSO ].m_nNumActive;
		nTotalCreates			+= m_Stats[ nCurrSO ].m_nNumCreated;
		nTotalFrees				+= m_Stats[ nCurrSO ].m_nNumDestroyed;
		nTotalSends				+= m_Stats[ nCurrSO ].m_nNumSends;
		nTotalSubOwner			+= m_Stats[ nCurrSO ].m_nNumSubOwner;
		nTotalSubOtherUsers		+= m_Stats[ nCurrSO ].m_nNumSubOtherUsers;
		nTotalSubGameServer		+= m_Stats[ nCurrSO ].m_nNumSubGameServer;
	}

	//determine the time scale to normalize this into per second measurements
	uint64 nMicroSElapsed = ( m_bCollectingStats ) ? m_CollectTime.CServerMicroSecsPassed() : m_nMicroSElapsed;
	double fSeconds = nMicroSElapsed / 1000000.0;
	double fToS = ( nMicroSElapsed > 0 ) ? 1000000.0 / nMicroSElapsed : 1.0;		

	//-----------------------------------------
	// Update stats

	std::sort( sortedStats.begin(), sortedStats.end(), SortSOStatsSent );

	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\n" );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "SO Cache Transmits - %.2f second capture\n", fSeconds );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\n" );

	//now run through and display our report
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%s", "Type Name                           SendKB  %      SendKB/S MPlex  Create  C/S     Destroy D/S     Update  U/S     Active    U/S (%)\n" );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%s", "---- ------------------------------ ------- ------ -------- ------ ------- ------- ------- ------- ------- ------- --------- -------\n" );

	FOR_EACH_VEC( sortedStats, nCurrSO )
	{
		const SOStats_t& stats = *sortedStats[ nCurrSO ];

		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%4d %-30s %7u %5.1f%% %8.1f %6.2f %7u %7.0f %7u %7.0f %7u %7.0f %9u %6.3f%%\n",
					stats.m_nTypeID,
					stats.m_sName.Get(), 
					( uint32 )( stats.m_nRawBytesSent / 1024 ),
					100.0 * ( double )stats.m_nRawBytesSent / ( double )MAX( 1, nTotalRawBytes ),
					( double )( stats.m_nRawBytesSent / 1024 ) * fToS,
					( double )stats.m_nMultiplexedBytesSent / MAX( 1, stats.m_nRawBytesSent ),
					stats.m_nNumCreated,
					stats.m_nNumCreated * fToS,
					stats.m_nNumDestroyed,
					stats.m_nNumDestroyed * fToS,
					stats.m_nNumSends,
					stats.m_nNumSends * fToS,
					stats.m_nNumActive,
					100.0 * stats.m_nNumSends * fToS / MAX( 1, stats.m_nNumActive ) );
	}

	//close it out with the totals
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%s", "---- ------------------------------ ------- ------ -------- ------ ------- ------- ------- ------- ------- ------- --------- -------\n" );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS,       "     Totals                         %7u %5.1f%% %8.1f %6.2f %7u %7.0f %7u %7.0f %7u %7.0f %9u %6.3f%%\n",
		( uint32 )( nTotalRawBytes / 1024 ),
		100.0,
		( double )( nTotalRawBytes / 1024 ) * fToS,
		( double )nTotalMultiplexBytes / ( double )MAX( 1, nTotalRawBytes ),
		nTotalCreates,
		nTotalCreates * fToS,
		nTotalFrees,
		nTotalFrees * fToS,
		nTotalSends,
		nTotalSends * fToS,
		nTotalActive,
		100.0 * nTotalSends * fToS / MAX( 1, nTotalActive ) );

	//-----------------------------------------
	// Subscription stats

	std::sort( sortedStats.begin(), sortedStats.end(), SortSOStatsSubscribe );

	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\n" );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "SO Cache Subscriptions Sends - %.2f second capture\n", fSeconds );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\n" );

	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%s", "Type Name                           Owner    Owner/S  Other    Other/S  Server   Server/S\n" );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%s", "---- ------------------------------ -------- -------- -------- -------- -------- --------\n" );

	FOR_EACH_VEC( sortedStats, nCurrSO )
	{
		const SOStats_t& stats = *sortedStats[ nCurrSO ];

		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%4d %-30s %8u %8.0f %8u %8.0f %8u %8.0f\n",
			stats.m_nTypeID,
			stats.m_sName.Get(), 
			stats.m_nNumSubOwner,
			stats.m_nNumSubOwner * fToS,
			stats.m_nNumSubOtherUsers,
			stats.m_nNumSubOtherUsers * fToS,
			stats.m_nNumSubGameServer,
			stats.m_nNumSubGameServer * fToS );
	}

	//close it out with the totals
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%s", "---- ------------------------------ -------- -------- -------- -------- -------- --------\n" );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS,       "     Totals                         %8u %8.0f %8u %8.0f %8u %8.0f\n",
		nTotalSubOwner,
		nTotalSubOwner * fToS,
		nTotalSubOtherUsers,
		nTotalSubOtherUsers * fToS,
		nTotalSubGameServer,
		nTotalSubGameServer * fToS );

}

#endif // GC


//----------------------------------------------------------------------------
// Purpose: Claims all the memory for the shared object
//----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CSharedObject::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
}


//----------------------------------------------------------------------------
// Purpose: Claims all the static memory for the shared object (i.e. the
//			factory function map.
//----------------------------------------------------------------------------
void CSharedObject::ValidateStatics( CValidator & validator )
{
	CValidateAutoPushPop validatorAutoPushPop( validator, NULL, "CSharedObject::ValidateStatics", "CSharedObject::ValidateStatics" );	
	ValidateObj( sm_mapFactories );
}

#endif


} // namespace GCSDK



