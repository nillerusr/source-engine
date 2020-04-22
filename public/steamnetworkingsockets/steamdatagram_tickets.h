//====== Copyright Valve Corporation, All rights reserved. ====================
//
// Types and utilities for handling steam datagram tickets.  These are
// useful for both the client and the backend ticket generating authority.
//
//=============================================================================

#ifndef STEAMDATAGRAM_TICKETS_H
#define STEAMDATAGRAM_TICKETS_H
#ifdef _WIN32
#pragma once
#endif

#ifndef assert
	#include <assert.h>
#endif

#include "steamnetworkingtypes.h"

#pragma pack(push)
#pragma pack(8)

#if defined( STEAMDATAGRAM_TICKETGEN_FOREXPORT ) || defined( STEAMDATAGRAMLIB_FOREXPORT )
	#define STEAMDATAGRAM_TICKET_INTERFACE DLL_EXPORT
#elif defined( STEAMDATAGRAMLIB_STATIC_LINK )
	#define STEAMDATAGRAM_TICKET_INTERFACE extern "C"
#else
	#define STEAMDATAGRAM_TICKET_INTERFACE DLL_IMPORT
#endif

/// Max length of serialized auth ticket.  This is important so that we
/// can ensure that we always fit into a single UDP datagram (along with
/// other certs and signatures) and kep the implementation simple.
const size_t k_cbSteamDatagramMaxSerializedTicket = 512;

/// Network-routable identifier for a service.  In general, clients should
/// treat this as an opaque structure.  The only thing that is important
/// is that this contains everything the system needs to route packets to a
/// service.
struct SteamDatagramServiceNetID
{
	// Just use the private LAN address to identify the service
	uint32 m_unIP;
	uint16 m_unPort;
	uint16 m__nPad1;

	void Clear() { *(uint64 *)this = 0; }

	uint64 ConvertToUint64() const { return ( uint64(m_unIP) << 16U ) | uint64(m_unPort); }
	void SetFromUint64( uint64 x )
	{
		m_unIP = uint32(x >> 16U);
		m_unPort = uint16(x);
		m__nPad1 = 0;
	}

	inline bool operator==( const SteamDatagramServiceNetID &x ) const { return m_unIP == x.m_unIP && m_unPort == x.m_unPort; }
};

/// Ticket used to gain access to the relay network.
struct SteamDatagramRelayAuthTicket
{
	SteamDatagramRelayAuthTicket() { Clear(); }

	/// Reset all fields
	void Clear() { memset( this, 0, sizeof(*this) ); }

	/// Steam ID of the gameserver we want to talk to
	CSteamID m_steamIDGameserver;

	/// Steam ID of the person who was authorized.
	CSteamID m_steamIDAuthorizedSender;

	/// SteamID is authorized to send from a particular public IP.  If this
	/// is 0, then the sender is not restricted to a particular IP.
	uint32 m_unPublicIP;

	/// Time when the ticket expires.
	RTime32 m_rtimeTicketExpiry;

	/// Routing information
	SteamDatagramServiceNetID m_routing;

	/// App ID this is for
	uint32 m_nAppID;

	struct ExtraField
	{
		enum EType
		{
			k_EType_String,
			k_EType_Int, // For most small integral values.  Uses google protobuf sint64, so it's small on the wire.  WARNING: In some places this value may be transmitted in JSON, in which case precision may be lost in backend analytics.  Don't use this afor an "identifier", use it for a scalar quantity.
			k_EType_Fixed64, // 64 arbitrary bits.  This value is treated as an "identifier".  In places where JSON format is used, it will be serialized as a string.  No aggregation / analytics can be performed on this value.
		};
		int /* EType */ m_eType;
		char m_szName[28];

		union {
			char m_szStringValue[128];
			int64 m_nIntValue;
			uint64 m_nFixed64Value;
		};
	};
	enum { k_nMaxExtraFields = 16 };
	int m_nExtraFields;
	ExtraField m_vecExtraFields[ k_nMaxExtraFields ];

	/// Helper to add an extra field in a single call
	void AddExtraField_Int( const char *pszName, int64 val )
	{
		ExtraField *p = AddExtraField( pszName, ExtraField::k_EType_Int );
		if ( p )
			p->m_nIntValue = val;
	}
	void AddExtraField_Fixed64( const char *pszName, uint64 val )
	{
		ExtraField *p = AddExtraField( pszName, ExtraField::k_EType_Fixed64 );
		if ( p )
			p->m_nFixed64Value = val;
	}
	void AddExtraField_String( const char *pszName, const char *val )
	{
		ExtraField *p = AddExtraField( pszName, ExtraField::k_EType_Fixed64 );
		if ( p )
			V_strcpy_safe( p->m_szStringValue, val );
	}

private:
	ExtraField *AddExtraField( const char *pszName, ExtraField::EType eType )
	{
		if ( m_nExtraFields >= k_nMaxExtraFields )
		{
			assert( false );
			return nullptr;
		}
		ExtraField *p = &m_vecExtraFields[ m_nExtraFields++ ];
		p->m_eType = eType;
		V_strcpy_safe( p->m_szName, pszName );
		return p;
	}
};

/// Unpack signed ticket.  Does not check the signature!
/// Note that it's assumed that you won't need to link with both the client
/// lib and the ticket generating lib in any one project, since this symbol
/// is exposed in both.
STEAMDATAGRAM_TICKET_INTERFACE bool SteamDatagramRelayAuthTicket_Parse( const void *pvTicket, int cbTicket, SteamDatagramRelayAuthTicket *pOutTicket, SteamDatagramErrMsg &errMsg );

#pragma pack(pop)

#endif // STEAMDATAGRAM_TICKETS_H
