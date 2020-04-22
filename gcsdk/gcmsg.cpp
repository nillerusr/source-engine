//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds function bodies for GCSDK
//
//=============================================================================

#include "stdafx.h"
#include "gcmsg.h"
#include "msgprotobuf.h"

#include "tier0/memdbgoff.h"

#ifdef GC
namespace GCSDK
{
	IMPLEMENT_CLASS_MEMPOOL( CStructNetPacket, 1000, UTLMEMORYPOOL_GROW_SLOW );
}
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{

CUtlString GCMsgHdr_t::GetHeaderDescription( )
{
	CUtlString desc;
	desc.Format( "gc msg %s, SteamID %llu", PchMsgNameFromEMsg( m_eMsg ), m_ulSteamID );
	return desc;
}

CUtlString GCMsgHdrEx_t::GetHeaderDescription( )
{
	CUtlString desc;
	desc.Format( "gc msg %s, SteamID %llu, version %hd, job source %llu, job target %llu", PchMsgNameFromEMsg( m_eMsg ), m_ulSteamID, m_nHdrVersion, m_JobIDSource, m_JobIDTarget );
	return desc;
}

CUtlString GCMsgInterHdr_t::GetHeaderDescription( )
{
	CUtlString desc;
	desc.Format( "gc inter msg %s, SourceAppID %d, version %hd, job source %llu, job target %llu", PchMsgNameFromEMsg( m_eMsg ), m_unSourceAppId, m_nHdrVersion, m_JobIDSource, m_JobIDTarget );
	return desc;
}

//-----------------------------------------------------------------------------
// Purpose: Takes a pNetPacket, then converts it to the appropriate actual message
// type and return the IMsgNetPacket interface.
//-----------------------------------------------------------------------------
IMsgNetPacket *IMsgNetPacketFromCNetPacket( CNetPacket *pNetPacket )
{
	uint32 cubPkt = pNetPacket->CubData();
	uint8* pubPkt = pNetPacket->PubData();

	if ( cubPkt >= sizeof( ProtoBufMsgHeader_t ) && ( ( ( ProtoBufMsgHeader_t *) pubPkt )->m_EMsgFlagged & k_EMsgProtoBufFlag ) )
	{
		CProtoBufNetPacket *pMsgNetPacket = new CProtoBufNetPacket( pNetPacket, GCProtoBufMsgSrc_Unspecified, CSteamID(), 0, 0x7FFFFFFF & ( ( ProtoBufMsgHeader_t *) pubPkt )->m_EMsgFlagged );
		if ( !pMsgNetPacket->IsValid() )
		{
			pMsgNetPacket->Release();
			return NULL;
		}

		return pMsgNetPacket;
	}
	else
	{
		if ( cubPkt >= sizeof( GCMsgHdrEx_t ) )
		{
			CStructNetPacket *pMsgNetPacket = (CStructNetPacket*)malloc( sizeof( CStructNetPacket ) );
			Construct( pMsgNetPacket, pNetPacket );
			return pMsgNetPacket;
		}
	}

	EmitError( SPEW_GC, "IMsgNetPacketFromCNetPacket: malformed packet, size %d bytes\n", cubPkt );
	for ( uint32 i = 0 ; i < cubPkt ; i += 16 )
	{
		char buf[512];
		char *d = buf;
		d += Q_snprintf(d, 12, "%08X:", i );
		for ( uint32 j = i ; j < i+16 && j < cubPkt ; ++j )
		{
			d += Q_snprintf(d, 8, " %02X", pubPkt[j] );
		}
		*d = '\0';
		EmitError( SPEW_GC, "%s\n", buf );
	}

	AssertMsg( false, "IMsgNetPacketFromCNetPacket couldn't detect any appropriate msg format, returning NULL." );
	return NULL;
}
} // namespace GCSDK
