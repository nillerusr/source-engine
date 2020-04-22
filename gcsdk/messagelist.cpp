//====== Copyright , Valve Corporation, All rights reserved. =======
//
// Purpose: Provides names for GC message types
//
//=============================================================================


#include "stdafx.h"



// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


namespace GCSDK
{

//-----------------------------------------------------------------------------
// Purpose: allow global initializers for a list of message infos. The message
//			list will assemble them all into a single list when it initializes.
//-----------------------------------------------------------------------------
class CMessageListRegistration
{
public:
	CMessageListRegistration( MsgInfo_t *pMsgInfo, int cMsgInfo, void *pExtra = NULL );

	static CMessageListRegistration *sm_pFirst;
	CMessageListRegistration *m_pNext;
	MsgInfo_t *m_pMsgInfo;
	int m_cMsgInfo;
};


DECLARE_GC_EMIT_GROUP( g_EGMessages, messages );

//function called when a message is tallied but not registered properly. Used to help consolidate behavior for untracked messages
static void HandleUntrackedMsg( const char* pszMsgOperation, uint32 nMsgID )
{
	//for now output this as verbose so that we can clean up the initial spam. Then promote this to warnings in the future
	EG_VERBOSE( g_EGMessages, "Found %s message with ID %d that was not properly registered. Unable to tally information\n", pszMsgOperation, nMsgID );
}


CMessageListRegistration::CMessageListRegistration( MsgInfo_t *pMsgInfo, int cMsgInfo, void *pExtra )
: m_pMsgInfo( pMsgInfo ),
	m_cMsgInfo( cMsgInfo )
{
	m_pNext = sm_pFirst;
	sm_pFirst = this;
}

CMessageListRegistration *CMessageListRegistration::sm_pFirst = NULL;

//-----------------------------------------------------------------------------
// Purpose: Returns the name of a message type
//-----------------------------------------------------------------------------
const char *PchMsgNameFromEMsg( MsgType_t eMsg )
{
	const char *pchMsgName = k_rgchUnknown;
	g_theMessageList.GetMessage( eMsg, &pchMsgName, MT_GC );
	return pchMsgName;
}


//-----------------------------------------------------------------------------
void MsgRegistrationFromEnumDescriptor( const ::google::protobuf::EnumDescriptor *pEnumDescriptor, int nTypeMask )
{
	// build the struct list for messages
	MsgInfo_t *pMsgInfos = new MsgInfo_t[ pEnumDescriptor->value_count() ];
	memset( pMsgInfos, 0, sizeof( MsgInfo_t ) * pEnumDescriptor->value_count() );

	for ( int i = 0; i < pEnumDescriptor->value_count(); ++i )
	{
		const ::google::protobuf::EnumValueDescriptor *pEnumValueDescriptor = pEnumDescriptor->value(i);
		pMsgInfos[ i ].eMsg = pEnumValueDescriptor->number();
		pMsgInfos[ i ].pchMsgName = pEnumValueDescriptor->name().c_str();
		pMsgInfos[ i ].nFlags = nTypeMask;
	}

	new CMessageListRegistration( pMsgInfos, pEnumDescriptor->value_count() );
}

//-----------------------------------------------------------------------------

CMessageList g_theMessageList;

//-----------------------------------------------------------------------------
// CMessageList
//
// builds a hash of the MsgInfo_t table so that information about messages
// can be found without searching.
//-----------------------------------------------------------------------------

CMessageList::CMessageList( ) : m_bProfiling( false ), m_ulProfileMicrosecs( 0 )
{
}


//-----------------------------------------------------------------------------
// CMessageList
//
// builds a hash of the MsgInfo_t table so that information about messages
// can be found without searching.
//-----------------------------------------------------------------------------
bool CMessageList::BInit(  )
{
	m_bProfiling = false;
	m_ulProfileMicrosecs = 0;

	// figure out our message count
	int cMessageInfos = 0;
	for( CMessageListRegistration *pReg = CMessageListRegistration::sm_pFirst; pReg != NULL; pReg = pReg->m_pNext)
	{
		cMessageInfos += pReg->m_cMsgInfo;
	}

	// message indexes should fit in a short
	Assert( cMessageInfos < SHRT_MAX );

	m_vecMsgInfo.EnsureCapacity( cMessageInfos );
	m_vecMsgInfo.RemoveAll();
	m_vecMessageInfoBuckets.RemoveAll();

	int nIndex = 0;
	for( CMessageListRegistration *pReg = CMessageListRegistration::sm_pFirst; pReg != NULL; pReg = pReg->m_pNext)
	{
		for ( int nRegIndex = 0; nRegIndex < pReg->m_cMsgInfo; nRegIndex++ )
		{
			nIndex = m_vecMsgInfo.AddToTail( pReg->m_pMsgInfo[nRegIndex] );

			int nSlot;
			int nBucket = HashMessage( pReg->m_pMsgInfo[nRegIndex].eMsg, nSlot );

			AssureBucket( nBucket );
			
			if ( m_vecMessageInfoBuckets[nBucket][nSlot] != -1 )
			{
				int otherIndex = m_vecMessageInfoBuckets[nBucket][nSlot];
				MsgInfo_t &otherMsg = m_vecMsgInfo[ otherIndex ];
				AssertFatalMsg2( false, "Message collision: %s redefined as %s", 
					otherMsg.pchMsgName,
					pReg->m_pMsgInfo[nRegIndex].pchMsgName);
			}
			else
			{
				m_vecMessageInfoBuckets[nBucket][nSlot] = (short) nIndex;
			}
		}
	}

	//start our window
	ResetWindow();
	//and start our global timer
	m_sCollectTime[ MsgInfo_t::k_EStatsGroupGlobal ].SetToJobTime();

	return true;
}


//-----------------------------------------------------------------------------
// Tally the sending of a message
//-----------------------------------------------------------------------------
void CMessageList::TallySendMessage( MsgType_t eMsgType, uint32 unMsgSize, uint32 nSourceMask )
{
	short nIndex = GetMessageIndex( eMsgType );
	if ( nIndex == - 1 )
	{
		HandleUntrackedMsg( "send", eMsgType );
		return;
	}

	TallyMessageInternal( m_vecMsgInfo[nIndex],MsgInfo_t::k_EStatsTypeSent, unMsgSize, nSourceMask );
}


//-----------------------------------------------------------------------------
// Tally the receiving of a message
//-----------------------------------------------------------------------------
void CMessageList::TallyReceiveMessage( MsgType_t eMsgType, uint32 unMsgSize, uint32 nSourceMask )
{
	short nIndex = 	GetMessageIndex( eMsgType );
	if ( nIndex == - 1 )
	{
		HandleUntrackedMsg( "receive", eMsgType );
		return;
	}

	TallyMessageInternal( m_vecMsgInfo[nIndex], MsgInfo_t::k_EStatsTypeReceived, unMsgSize, nSourceMask );
}


//-----------------------------------------------------------------------------
// Tally the receiving of a message
//-----------------------------------------------------------------------------
void CMessageList::TallyMultiplexedMessage( MsgType_t eMsgType, uint32 uSent, uint32 cRecipients, uint32 uMsgSize, uint32 nSourceMask )
{
	short nIndex = 	GetMessageIndex( eMsgType );
	if ( nIndex == - 1 )
	{
		HandleUntrackedMsg( "multiplex", eMsgType );
		return;
	}

	MsgInfo_t &msgInfo = m_vecMsgInfo[nIndex];
	TallyMessageInternal( msgInfo, MsgInfo_t::k_EStatsTypeMultiplexedSends, uSent, nSourceMask, 1 );
	TallyMessageInternal( msgInfo, MsgInfo_t::k_EStatsTypeMultiplexedSendsRaw, uMsgSize * cRecipients, nSourceMask, cRecipients );
}


//-----------------------------------------------------------------------------
// Tally the receiving of a message
//-----------------------------------------------------------------------------
void CMessageList::TallyMessageInternal( MsgInfo_t &msgInfo, MsgInfo_t::EStatsType eBucket, uint32 unMsgSize, uint32 nSourceMask, uint32 cMessages )
{
	//log this for global stats
	msgInfo.stats[ MsgInfo_t::k_EStatsGroupGlobal ][ eBucket ].nCount += cMessages;
	msgInfo.stats[ MsgInfo_t::k_EStatsGroupGlobal ][ eBucket ].uBytes += unMsgSize;
	msgInfo.stats[ MsgInfo_t::k_EStatsGroupGlobal ][ eBucket ].nSourceMask |= nSourceMask;

	//track this in our current timing window
	msgInfo.stats[ MsgInfo_t::k_EStatsGroupWindow ][ eBucket ].nCount += cMessages;
	msgInfo.stats[ MsgInfo_t::k_EStatsGroupWindow ][ eBucket ].uBytes += unMsgSize;
	msgInfo.stats[ MsgInfo_t::k_EStatsGroupWindow ][ eBucket ].nSourceMask |= nSourceMask;

	//track our window totals as well
	m_WindowTotals[ eBucket ].nCount += cMessages;
	m_WindowTotals[ eBucket ].uBytes += unMsgSize;
	m_WindowTotals[ eBucket ].nSourceMask |= nSourceMask;

	//and if we are profiling, track the data into our profiling window
	if ( m_bProfiling )
	{
		msgInfo.stats[ MsgInfo_t::k_EStatsGroupProfile ][ eBucket ].nCount += cMessages;
		msgInfo.stats[ MsgInfo_t::k_EStatsGroupProfile ][ eBucket ].uBytes += unMsgSize;
		msgInfo.stats[ MsgInfo_t::k_EStatsGroupProfile ][ eBucket ].nSourceMask |= nSourceMask;
	}
}

//called to obtain the totals for the timing window
const MsgInfo_t::Stats_t& CMessageList::GetWindowTotal( MsgInfo_t::EStatsType eType ) const
{
	return m_WindowTotals[ eType ];
}

//called to reset the window timings
void CMessageList::ResetWindow()
{
	//reset when our window started
	m_sCollectTime[ MsgInfo_t::k_EStatsGroupWindow ].SetToJobTime();

	// starting a new window... clear everything
	FOR_EACH_VEC( m_vecMsgInfo, nIndex )
	{
		for( uint32 nType = 0; nType < MsgInfo_t::k_EStatsType_Count; nType++ )
		{
			m_vecMsgInfo[ nIndex ].stats[ MsgInfo_t::k_EStatsGroupWindow ][ nType ].nCount = 0;
			m_vecMsgInfo[ nIndex ].stats[ MsgInfo_t::k_EStatsGroupWindow ][ nType ].uBytes = 0;
			m_vecMsgInfo[ nIndex ].stats[ MsgInfo_t::k_EStatsGroupWindow ][ nType ].nSourceMask = 0;
		}
	}

	//and clear our totals
	for( uint32 nType = 0; nType < MsgInfo_t::k_EStatsType_Count; nType++ )
	{
		m_WindowTotals[ nType ].nCount = 0;
		m_WindowTotals[ nType ].uBytes = 0;
	}
}


//-----------------------------------------------------------------------------
// Turns snapshot profiling on and off
//-----------------------------------------------------------------------------
void CMessageList::EnableProfiling( bool bEnableProfiling )
{
	m_bProfiling = bEnableProfiling;
	if ( m_bProfiling )
	{
		m_sCollectTime[ MsgInfo_t::k_EStatsGroupProfile ].SetToJobTime();

		// starting a new profile... clear everything
		FOR_EACH_VEC( m_vecMsgInfo, nIndex )
		{
			for( uint32 nType = 0; nType < MsgInfo_t::k_EStatsType_Count; nType++ )
			{
				m_vecMsgInfo[ nIndex ].stats[ MsgInfo_t::k_EStatsGroupProfile ][ nType ].nCount = 0;
				m_vecMsgInfo[ nIndex ].stats[ MsgInfo_t::k_EStatsGroupProfile ][ nType ].uBytes = 0;
				m_vecMsgInfo[ nIndex ].stats[ MsgInfo_t::k_EStatsGroupProfile ][ nType ].nSourceMask = 0;
			}
		}
	}
	else
	{
		m_ulProfileMicrosecs = m_sCollectTime[ MsgInfo_t::k_EStatsGroupProfile ].CServerMicroSecsPassed();
	}
}


uint64 CMessageList::GetGroupDuration( MsgInfo_t::EStatsGroup eGroup ) const
{
	//handle the special case of it being a profile, where if we are no longer profiling, we want to use our cached value
	if( ( eGroup == MsgInfo_t::k_EStatsGroupProfile ) && !m_bProfiling )
	{
		return m_ulProfileMicrosecs;
	}

	//otherwise we can just use the timer directly
	return m_sCollectTime[ eGroup ].CServerMicroSecsPassed();
}

//-----------------------------------------------------------------------------
// print statistics bout each message we handle
//-----------------------------------------------------------------------------
void CMessageList::PrintStats( bool bShowAll, bool bSortByFrequency, MsgInfo_t::EStatsGroup eGroup, MsgInfo_t::EStatsType eType, uint32 nSourceMask ) const
{
	// Figure out which time value we should use for rate calcs
	uint64 ulMicroseconds = GetGroupDuration( eGroup );

	// work out a sorted list
	CUtlMap<uint64, uint32> mapValueToMsg( DefLessFunc( uint64 ) );
	uint64 unTotalBytes = 0;
	uint32 unTotalMessages = 0;

	FOR_EACH_VEC( m_vecMsgInfo, n )
	{
		//see if we are looking for a particular source, and if so, if this has that source set
		if( nSourceMask && ( ( m_vecMsgInfo[n].stats[ eGroup ][ eType ].nSourceMask & nSourceMask ) == 0 ) )
			continue;

		uint32 nCount = m_vecMsgInfo[n].stats[ eGroup ][ eType ].nCount;
		uint64 uBytes = m_vecMsgInfo[n].stats[ eGroup ][ eType ].uBytes;

		unTotalMessages += nCount;
		unTotalBytes += uBytes;

		if ( nCount == 0 && !bShowAll )
			continue;

		mapValueToMsg.Insert( bSortByFrequency ? (uint64)nCount : uBytes, n );
	}

	double fSeconds = ulMicroseconds / ( 1000.0 * 1000.0 );
	
	// 5, 46, 10, 6, 14, 6, 10, 10
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%s", "EMsg  MessageName                                         Count      %          Bytes      %     MsgPS    Avg/Msg       KBPS\n" );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%s", "----- ---------------------------------------------- ---------- ------ -------------- ------ --------- ---------- ----------\n" );

	for ( uint16 iValue = mapValueToMsg.LastInorder(); iValue != mapValueToMsg.InvalidIndex(); iValue = mapValueToMsg.PrevInorder( iValue ) )
	{
		uint32 n = mapValueToMsg[iValue];

		//see if we are looking for a particular source, and if so, if this has that source set
		if( nSourceMask && ( ( m_vecMsgInfo[n].stats[ eGroup ][ eType ].nSourceMask & nSourceMask ) == 0 ) )
			continue;

		uint32 nCount = m_vecMsgInfo[n].stats[ eGroup ][ eType].nCount;
		uint64 uBytes = m_vecMsgInfo[n].stats[ eGroup ][ eType].uBytes;

		uint32 uAvgMsg = 0;
		if ( nCount > 0 )
			uAvgMsg = uBytes / nCount;

		float flCountPerc = 0;
		if ( unTotalMessages > 0 )
			flCountPerc = nCount * 100.0f / unTotalMessages;
		float flBytesPerc = 0;
		if ( unTotalBytes > 0 )
			flBytesPerc = uBytes * 100.0f / unTotalBytes;
		double fMsgPS = 0.0;
		if ( ulMicroseconds > 0 )
			fMsgPS = nCount / fSeconds;
		double fKBPS = 0.0f;
		if ( ulMicroseconds > 0 )
			fKBPS = uBytes / ( fSeconds * 1000.0 );


		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%5u %-46s %10u %5.1f%% %14llu %5.1f%% %9.1f %10u %10.2f\n",
			m_vecMsgInfo[n].eMsg,
			m_vecMsgInfo[n].pchMsgName,
			nCount,
			flCountPerc,
			uBytes,
			flBytesPerc,
			fMsgPS,
			uAvgMsg,
			fKBPS );
	}

	uint32 uAvgMsgTotal = 0;
	if ( unTotalMessages > 0 )
		uAvgMsgTotal = unTotalBytes / unTotalMessages;
	double fMsgPS = 0.0;
	if ( ulMicroseconds > 0 )
		fMsgPS = unTotalMessages / fSeconds;
	double fKBPSTotal = 0.0;
	if ( ulMicroseconds > 0 )
		fKBPSTotal = unTotalBytes / ( fSeconds * 1000.0 );

	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "----- ---------------------------------------------- ---------- ------ -------------- ------ --------- ---------- ----------\n" );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "Totals                                               %10u 100.0%% %14llu 100.0%% %9.1f %10u %10.2f\n",
		unTotalMessages,
		unTotalBytes,
		fMsgPS,
		uAvgMsgTotal,
		fKBPSTotal );
}


//-----------------------------------------------------------------------------
// print statistics bout each message we handle
//-----------------------------------------------------------------------------
void CMessageList::PrintMultiplexStats( MsgInfo_t::EStatsGroup eGroup, bool bSortByFrequency, uint32 nSourceMask ) const
{
	// Figure out which time value we should use for rate calcs
	uint64 ulMicroseconds = GetGroupDuration( eGroup );

	MsgInfo_t::Stats_t rgTotals[MsgInfo_t::k_EStatsType_Count];

	const MsgInfo_t::EStatsType eMultiType = MsgInfo_t::k_EStatsTypeMultiplexedSends;
	const MsgInfo_t::EStatsType eRawType = MsgInfo_t::k_EStatsTypeMultiplexedSendsRaw;		

	// work out a sorted list
	CUtlMap<uint64, uint32> mapValueToMsg( DefLessFunc( uint64 ) );
	FOR_EACH_VEC( m_vecMsgInfo, n )
	{
		const MsgInfo_t &msgInfo = m_vecMsgInfo[n];

		//see if we are looking for a particular source, and if so, if this has that source set
		if( nSourceMask && ( msgInfo.stats[ eGroup ][ eMultiType ].nSourceMask & nSourceMask ) == 0 )
			continue;

		rgTotals[eMultiType].nCount += msgInfo.stats[ eGroup ][ eMultiType ].nCount;
		rgTotals[eMultiType].uBytes += msgInfo.stats[ eGroup ][ eMultiType ].uBytes;
		rgTotals[eRawType].nCount += msgInfo.stats[ eGroup ][ eRawType ].nCount;
		rgTotals[eRawType].uBytes += msgInfo.stats[ eGroup ][ eRawType ].uBytes;

		if ( msgInfo.stats[ eGroup ][ eMultiType ].nCount == 0 )
			continue;

		uint32 nCount = msgInfo.stats[ eGroup ][ eMultiType ].nCount;
		uint64 uBytes = msgInfo.stats[ eGroup ][ eMultiType ].uBytes;
		mapValueToMsg.Insert( bSortByFrequency ? (uint64)nCount : uBytes, n );
	}

	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%s", "EMsg  MessageName                                         Count      %             KB      %    Avg/Msg       KBPS  Msg Saved      %       KB Saved      %\n" );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%s", "----- ---------------------------------------------- ---------- ------ -------------- ------ ---------- ---------- ---------- ------ -------------- ------\n" );

	uint32 unTotalMessages = 0;
	uint64 unTotalBytes = 0;

	for ( uint16 iValue = mapValueToMsg.LastInorder(); iValue != mapValueToMsg.InvalidIndex(); iValue = mapValueToMsg.PrevInorder( iValue ) )
	{
		const MsgInfo_t &msgInfo = m_vecMsgInfo[ mapValueToMsg[iValue] ];

		//see if we are looking for a particular source, and if so, if this has that source set
		if( nSourceMask && ( msgInfo.stats[ eGroup ][ eMultiType ].nSourceMask & nSourceMask ) == 0 )
			continue;

		uint32 nCount = msgInfo.stats[ eGroup ][ eMultiType ].nCount;
		uint64 uBytes = msgInfo.stats[ eGroup ][ eMultiType ].uBytes;

		unTotalMessages += nCount;
		unTotalBytes += uBytes;

		int32 nMessagesSaved = msgInfo.stats[ eGroup ][ eRawType ].nCount - msgInfo.stats[ eGroup ][ eMultiType ].nCount;
		int64 uBytesSaved =	   msgInfo.stats[ eGroup ][ eRawType ].uBytes - msgInfo.stats[ eGroup ][ eMultiType ].uBytes;

		uint32 nMessagesIfNotMultiplex = msgInfo.stats[ eGroup ][ eRawType ].nCount;
		uint64 uBytesIfNotMultiplex =    msgInfo.stats[ eGroup ][ eRawType ].uBytes;

		uint32 uAvgMsg = 0;
		if ( nCount > 0 )
			uAvgMsg = uBytes / nCount;

		float flCountPerc = 0;
		if ( rgTotals[eMultiType].nCount > 0 )
			flCountPerc = nCount * 100.0f / rgTotals[eMultiType].nCount;
		float flBytesPerc = 0;
		if ( rgTotals[eMultiType].uBytes > 0 )
			flBytesPerc = uBytes * 100.0f / rgTotals[eMultiType].uBytes;
		float fKBPS = 0.0f;
		if ( ulMicroseconds > 0 )
			fKBPS = uBytes * 1000.0f / ulMicroseconds;

		float flMessagesSavedPerc = 0.0f;
		if ( nMessagesIfNotMultiplex > 0 )
			flMessagesSavedPerc = nMessagesSaved * 100.0f / nMessagesIfNotMultiplex;
		float flBytesSavedPerc = 0.0f;
		if ( uBytesIfNotMultiplex > 0 )
			flBytesSavedPerc = uBytesSaved * 100.0f / uBytesIfNotMultiplex;

		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "%5u %-46s %10u %5.1f%% %14llu %5.1f%% %10u %10.2f %10d %5.1f%% %14lld %5.1f%%\n",
			msgInfo.eMsg,
			msgInfo.pchMsgName,
			nCount,
			flCountPerc,
			uBytes / 1024,
			flBytesPerc,
			uAvgMsg,
			fKBPS,
			nMessagesSaved,
			flMessagesSavedPerc,
			uBytesSaved / 1024,
			flBytesSavedPerc );
	}

	uint32 uAvgMsgTotal = 0;
	if ( unTotalMessages > 0 )
		uAvgMsgTotal = unTotalBytes / unTotalMessages;

	float fKBPSTotal = 0.0f;
	if ( ulMicroseconds > 0 )
		fKBPSTotal = unTotalBytes * 1000.0f / ulMicroseconds;

	float flTotalMessagesPct = 0.0f;
	if ( rgTotals[eMultiType].nCount > 0 )
		flTotalMessagesPct = unTotalMessages * 100.0f / rgTotals[eMultiType].nCount;

	float flTotalBytesPct = 0.0f;
	if ( rgTotals[eMultiType].uBytes > 0 )
		flTotalBytesPct = unTotalBytes * 100.0f / rgTotals[eMultiType].uBytes;

	int32 nTotalMessagesSaved = rgTotals[eRawType].nCount - rgTotals[eMultiType].nCount;
	int64 nTotalBytesSaved = rgTotals[eRawType].uBytes - rgTotals[eMultiType].uBytes;
	
	int32 nTotalMessagesIfNotMultiplex = unTotalMessages - rgTotals[eMultiType].nCount + rgTotals[eRawType].nCount;
	int64 uTotalBytesIfNotMultiplex = unTotalBytes - rgTotals[eMultiType].uBytes + rgTotals[eRawType].uBytes;

	float flTotalMessagesSavedPct = 0.0f;
	if ( nTotalMessagesIfNotMultiplex > 0 )
		flTotalMessagesSavedPct = nTotalMessagesSaved * 100.0f / nTotalMessagesIfNotMultiplex;

	float flTotalBytesSavedPct = 0.0f;
	if ( uTotalBytesIfNotMultiplex > 0 )
		flTotalBytesSavedPct = nTotalBytesSaved * 100.0f / uTotalBytesIfNotMultiplex;
	
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "----- ---------------------------------------------- ---------- ------ -------------- ------ ---------- ---------- ---------- ------ -------------- ------\n" );
	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "Totals                                               %10u %5.1f%% %14llu %5.1f%% %10u %10.2f %10d %5.1f%% %14lld %5.1f%%\n",
		unTotalMessages,
		flTotalMessagesPct,
		unTotalBytes / 1024,
		flTotalBytesPct,
		uAvgMsgTotal,
		fKBPSTotal,
		nTotalMessagesSaved,
		flTotalMessagesSavedPct,
		nTotalBytesSaved / 1024,
		flTotalBytesSavedPct );
}


//-----------------------------------------------------------------------------
// Destroys a message list by deallocating everything it's got
//-----------------------------------------------------------------------------
CMessageList::~CMessageList()
{
	int nUsedBuckets = 0;
	for ( int nIndex = 0; nIndex < m_vecMessageInfoBuckets.Count(); nIndex++ )
	{
		if ( m_vecMessageInfoBuckets[nIndex] != NULL )
		{
			FreePv( m_vecMessageInfoBuckets[nIndex] );
			m_vecMessageInfoBuckets[nIndex] = NULL;
			nUsedBuckets++;
		}
	}

	m_vecMessageInfoBuckets.RemoveAll();
}

//-----------------------------------------------------------------------------
// assure that we've got the slots in the given bucket allocated
//-----------------------------------------------------------------------------
void CMessageList::AssureBucket( int nBucket )
{
	// if this bucket is bigger then the array, extend the array
	if ( nBucket >= m_vecMessageInfoBuckets.Count() )
	{
		int nOldCount = m_vecMessageInfoBuckets.Count();
		// message ID "clumps" are usually 100 apart, so we'll try
		// to grow by 100 each time. Divide 100 by the bucket size and add
		// one for truncation to figure out our grow-by.
		int nNewCount = nBucket + (1 + (100 / m_kcBucketSize));

		// get that count
		m_vecMessageInfoBuckets.EnsureCount( nNewCount );

		// initialize the new ones to NULL
		for ( int nIndex = nOldCount; nIndex < nNewCount; nIndex++ )
		{
			m_vecMessageInfoBuckets[nIndex] = NULL;
		}
	}

	// is the bucket we want allocated?
	if ( m_vecMessageInfoBuckets[nBucket] == NULL )
	{
		// nope; get one and initialize it
		m_vecMessageInfoBuckets[nBucket] = (short*) PvAlloc( sizeof(short) * m_kcBucketSize );
		for ( int nIndex = 0; nIndex < m_kcBucketSize; nIndex++)
			m_vecMessageInfoBuckets[nBucket][nIndex] = -1;
	}

	return;
}


//-----------------------------------------------------------------------------
// Purpose: Hash an MsgType_t and return the index into m_vecMsgInfo where it can be found
//			returns -1 if the MsgType_t is unknown
//-----------------------------------------------------------------------------
short CMessageList::GetMessageIndex( MsgType_t eMsg )
{
	// find the slot and bucket for this message in our hash
	int nSlot;
	int nBucket = HashMessage( eMsg, nSlot );

	// taller than the hash?
	if ( nBucket >= m_vecMessageInfoBuckets.Count() )
		return -1;

	// not a bucket?
	if ( m_vecMessageInfoBuckets[nBucket] == NULL )
		return -1;

	// get the index back to the global array
	short nIndex = m_vecMessageInfoBuckets[nBucket][nSlot];
	return nIndex;
}


//-----------------------------------------------------------------------------
// Tests to see if a message is valid; if so, return a pointer to its name.
// A message must match at least one of the nTypeMask flags to be considered
// valid.  If provided, ppMsgName is set to k_rgchUnknown if no match,
// or the message name if matched.
//-----------------------------------------------------------------------------

bool CMessageList::GetMessage( MsgType_t eMsg, const char **ppMsgName, int nTypeMask )
{
	VPROF_BUDGET( "GetMessage", VPROF_BUDGETGROUP_JOBS_COROUTINES );

	// if an out variable for the name is provided,
	// initialize it with a pointer to "unknown" string
	if ( ppMsgName != NULL )
		*ppMsgName = k_rgchUnknown;

	short nIndex = GetMessageIndex( eMsg );

	// good index?
	if ( nIndex == -1 )
	{
		if ( ppMsgName != NULL )
		{
			*ppMsgName = "Unknown MsgType - Not Found";
		}
		return false;
	}

	const MsgInfo_t &msgInfo = m_vecMsgInfo[nIndex];

	// get the string out
	if ( ppMsgName != NULL )
		*ppMsgName = msgInfo.pchMsgName;

	// it's good if it matches the flags
	return ( 0 != ( msgInfo.nFlags & nTypeMask ) );
}


} // namespace GCSDK

