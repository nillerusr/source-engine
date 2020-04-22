//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "netmessages.h"
#include "bitbuf.h"
#include "const.h"
#include "../engine/net_chan.h"
#include "mathlib/mathlib.h"
#include "networkstringtabledefs.h"
#include "../engine/event_system.h"
#include "../engine/dt.h"
#include "tier0/vprof.h"
#include "convar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// XXX(JohnS): This is no longer used, but we're keeping it so we can check if old network streams set it to work around
//             missing information in the older SVC_VoiceInit packet. See SVC_VoiceInit::ReadFromBuffer
ConVar sv_use_steam_voice( "sv_use_steam_voice", "0", FCVAR_HIDDEN | FCVAR_REPLICATED,
                           "Deprecated - placeholder convar for handling old network streams that "
                           "had an incomplete SVC_VoiceInit packet.  Use \"sv_voicecodec steam\"" );

static char s_text[1024];

const char *CLC_VoiceData::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: %i bytes", GetName(), Bits2Bytes(m_nLength) );
	return s_text;
}

bool CLC_VoiceData::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	
	m_nLength = m_DataOut.GetNumBitsWritten();

	buffer.WriteWord( m_nLength );	// length in bits

	//Send this client's XUID (only needed on the 360)
#if defined ( _X360 )
	buffer.WriteLongLong( m_xuid );
#endif
	
	return buffer.WriteBits( m_DataOut.GetBasePointer(), m_nLength );
}

bool CLC_VoiceData::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "CLC_VoiceData::ReadFromBuffer" );

	m_nLength = buffer.ReadWord();	// length in bits

#if defined ( _X360 )
	m_xuid	= buffer.ReadLongLong();
#endif

	m_DataIn = buffer;

	return buffer.SeekRelative( m_nLength );
}

const char *CLC_Move::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: backup %i, new %i, bytes %i", GetName(), 
		m_nNewCommands, m_nBackupCommands, Bits2Bytes(m_nLength) );
	return s_text;
}

bool CLC_Move::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	m_nLength = m_DataOut.GetNumBitsWritten();
	
	buffer.WriteUBitLong( m_nNewCommands, NUM_NEW_COMMAND_BITS );
	buffer.WriteUBitLong( m_nBackupCommands, NUM_BACKUP_COMMAND_BITS );
	
	buffer.WriteWord( m_nLength );	

	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
}

bool CLC_Move::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "CLC_Move::ReadFromBuffer" );

	m_nNewCommands = buffer.ReadUBitLong( NUM_NEW_COMMAND_BITS );
	m_nBackupCommands = buffer.ReadUBitLong( NUM_BACKUP_COMMAND_BITS );
	m_nLength = buffer.ReadWord();
	m_DataIn = 	buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *CLC_ClientInfo::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: SendTableCRC %i", GetName(), 
		m_nSendTableCRC );
	return s_text;
}

bool CLC_ClientInfo::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	buffer.WriteLong( m_nServerCount );
	buffer.WriteLong( m_nSendTableCRC );
	buffer.WriteOneBit( m_bIsHLTV?1:0 );
	buffer.WriteLong( m_nFriendsID );
	buffer.WriteString( m_FriendsName );
	
	for ( int i=0; i<MAX_CUSTOM_FILES; i++ )
	{
		if ( m_nCustomFiles[i] != 0 )
		{
			buffer.WriteOneBit( 1 );
			buffer.WriteUBitLong( m_nCustomFiles[i], 32 );
		}
		else
		{
			buffer.WriteOneBit( 0 );
		}
	}
		
#if defined( REPLAY_ENABLED )
	buffer.WriteOneBit( m_bIsReplay?1:0 );
#endif

	return !buffer.IsOverflowed();
}

bool CLC_ClientInfo::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "CLC_ClientInfo::ReadFromBuffer" );

	m_nServerCount = buffer.ReadLong();
	m_nSendTableCRC = buffer.ReadLong();
	m_bIsHLTV = buffer.ReadOneBit()!=0;
	m_nFriendsID = buffer.ReadLong();
	buffer.ReadString( m_FriendsName, sizeof(m_FriendsName) );
	
	for ( int i=0; i<MAX_CUSTOM_FILES; i++ )
	{
		if ( buffer.ReadOneBit() != 0 )
		{
			m_nCustomFiles[i] = buffer.ReadUBitLong( 32 );
		}
		else
		{
			m_nCustomFiles[i] = 0;
		}
	}

#if defined( REPLAY_ENABLED )
	m_bIsReplay = buffer.ReadOneBit()!=0;
#endif

	return !buffer.IsOverflowed();
}

bool CLC_BaselineAck::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteLong( m_nBaselineTick );
	buffer.WriteUBitLong( m_nBaselineNr, 1 );
	return !buffer.IsOverflowed();
}

bool CLC_BaselineAck::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "CLC_BaselineAck::ReadFromBuffer" );

	m_nBaselineTick = buffer.ReadLong();
	m_nBaselineNr = buffer.ReadUBitLong( 1 );
	return !buffer.IsOverflowed();
}

const char *CLC_BaselineAck::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: tick %i", GetName(), m_nBaselineTick );
	return s_text;
}

bool CLC_ListenEvents::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	int count = MAX_EVENT_NUMBER / 32;
	for ( int i = 0; i < count; ++i )
	{
		buffer.WriteUBitLong( m_EventArray.GetDWord( i ), 32 );
	}

	return !buffer.IsOverflowed();
}

bool CLC_ListenEvents::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "CLC_ListenEvents::ReadFromBuffer" );

	int count = MAX_EVENT_NUMBER / 32;
	for ( int i = 0; i < count; ++i )
	{
		m_EventArray.SetDWord( i, buffer.ReadUBitLong( 32 ) );
	}

	return !buffer.IsOverflowed();
}

const char *CLC_ListenEvents::ToString(void) const
{
	int count = 0;

	for ( int i = 0; i<MAX_EVENT_NUMBER; i++ )
	{
		if ( m_EventArray.Get( i ) )
			count++;
	}

	Q_snprintf(s_text, sizeof(s_text), "%s: registered events %i", GetName(), count );
	return s_text;
}


bool CLC_RespondCvarValue::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	buffer.WriteSBitLong( m_iCookie, 32 );
	buffer.WriteSBitLong( m_eStatusCode, 4 );
	
	buffer.WriteString( m_szCvarName );
	buffer.WriteString( m_szCvarValue );

	return !buffer.IsOverflowed();
}

bool CLC_RespondCvarValue::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "CLC_RespondCvarValue::ReadFromBuffer" );

	m_iCookie = buffer.ReadSBitLong( 32 );
	m_eStatusCode = (EQueryCvarValueStatus)buffer.ReadSBitLong( 4 );
	
	// Read the name.
	buffer.ReadString( m_szCvarNameBuffer, sizeof( m_szCvarNameBuffer ) );
	m_szCvarName = m_szCvarNameBuffer;
	
	// Read the value.
	buffer.ReadString( m_szCvarValueBuffer, sizeof( m_szCvarValueBuffer ) );
	m_szCvarValue = m_szCvarValueBuffer;
	
	return !buffer.IsOverflowed();
}

const char *CLC_RespondCvarValue::ToString(void) const
{
	Q_snprintf( s_text, sizeof(s_text), "%s: status: %d, value: %s, cookie: %d", GetName(), m_eStatusCode, m_szCvarValue, m_iCookie );
	return s_text;
}



const char *g_MostCommonPathIDs[] =
{
	"GAME",
	"MOD"
};

const char *g_MostCommonPrefixes[] =
{
	"materials",
	"models",
	"sounds",
	"scripts"
};

static int FindCommonPathID( const char *pPathID )
{
	for ( int i=0; i < ARRAYSIZE( g_MostCommonPathIDs ); i++ )
	{
		if ( V_stricmp( pPathID, g_MostCommonPathIDs[i] ) == 0 )
			return i;
	}
	return -1;
}

static int FindCommonPrefix( const char *pStr )
{
	for ( int i=0; i < ARRAYSIZE( g_MostCommonPrefixes ); i++ )
	{
		if ( V_stristr( pStr, g_MostCommonPrefixes[i] ) == pStr  )
		{
			int iNextChar = V_strlen( g_MostCommonPrefixes[i] );
			if ( pStr[iNextChar] == '/' || pStr[iNextChar] == '\\' )
				return i;
		}
	}
	return -1;
}


bool CLC_FileCRCCheck::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	// Reserved for future use.
	buffer.WriteOneBit( 0 );
	
	// Just write a couple bits for the path ID if it's one of the common ones.
	int iCode = FindCommonPathID( m_szPathID );
	if ( iCode == -1 )
	{
		buffer.WriteUBitLong( 0, 2 );
		buffer.WriteString( m_szPathID );
	}
	else
	{
		buffer.WriteUBitLong( iCode+1, 2 );
	}

	iCode = FindCommonPrefix( m_szFilename );
	if ( iCode == -1 )
	{
		buffer.WriteUBitLong( 0, 3 );
		buffer.WriteChar( 1 ); // so we can detect the new message version
		buffer.WriteString( m_szFilename );
	}
	else
	{
		buffer.WriteUBitLong( iCode+1, 3 );
		buffer.WriteChar( 1 ); // so we can detect the new message version
		buffer.WriteString( &m_szFilename[ V_strlen(g_MostCommonPrefixes[iCode])+1 ] );
	}

	buffer.WriteBits( &m_MD5.bits, sizeof(m_MD5.bits)*8 );
	buffer.WriteUBitLong( m_CRCIOs, 32 );
	buffer.WriteUBitLong( m_eFileHashType, 32 );
	buffer.WriteUBitLong( m_cbFileLen, 32 );
	buffer.WriteUBitLong( m_nPackFileNumber, 32 );
	buffer.WriteUBitLong( m_PackFileID, 32 );
	buffer.WriteUBitLong( m_nFileFraction, 32 );
	return !buffer.IsOverflowed();
}

bool CLC_FileCRCCheck::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "CLC_FileCRCCheck::ReadFromBuffer" );

	// Reserved for future use.
	buffer.ReadOneBit();
	
	// Read the path ID.
	int iCode = buffer.ReadUBitLong( 2 );
	if ( iCode == 0 )
	{
		buffer.ReadString( m_szPathID, sizeof( m_szPathID ) );
	}
	else if ( (iCode-1) < ARRAYSIZE( g_MostCommonPathIDs ) )
	{
		V_strncpy( m_szPathID, g_MostCommonPathIDs[iCode-1], sizeof( m_szPathID ) );
	}
	else
	{
		Assert( !"Invalid path ID code in CLC_FileCRCCheck" );
		return false;
	}

	// Prefix string
	iCode = buffer.ReadUBitLong( 3 );

	// Read filename, and check for the new message format version?
	char szTemp[ MAX_PATH ];
	int c = buffer.ReadChar();
	bool bNewVersion = false;
	if ( c == 1 )
	{
		bNewVersion = true;
		buffer.ReadString( szTemp, sizeof( szTemp ) );
	}
	else
	{
		szTemp[0] = (char)c;
		buffer.ReadString( szTemp+1, sizeof( szTemp)-1 );
	}
	if ( iCode == 0 )
	{
		V_strcpy_safe( m_szFilename, szTemp );
	}
	else if ( (iCode-1) < ARRAYSIZE( g_MostCommonPrefixes ) )
	{
		V_sprintf_safe( m_szFilename, "%s%c%s", g_MostCommonPrefixes[iCode-1], CORRECT_PATH_SEPARATOR, szTemp );
	}
	else
	{
		Assert( !"Invalid prefix code in CLC_FileCRCCheck." );
		return false;
	}

	if ( bNewVersion )
	{
		buffer.ReadBits( &m_MD5.bits, sizeof(m_MD5.bits)*8 );
		m_CRCIOs = buffer.ReadUBitLong( 32 );
		m_eFileHashType = buffer.ReadUBitLong( 32 );
		m_cbFileLen = buffer.ReadUBitLong( 32 );
		m_nPackFileNumber = buffer.ReadUBitLong( 32 );
		m_PackFileID = buffer.ReadUBitLong( 32 );
		m_nFileFraction = buffer.ReadUBitLong( 32 );
	}
	else
	{
		/* m_CRC */ buffer.ReadUBitLong( 32 );
		m_CRCIOs = buffer.ReadUBitLong( 32 );
		m_eFileHashType = buffer.ReadUBitLong( 32 );
	}
	
	return !buffer.IsOverflowed();
}

const char *CLC_FileCRCCheck::ToString(void) const
{
	V_snprintf( s_text, sizeof(s_text), "%s: path: %s, file: %s", GetName(), m_szPathID, m_szFilename );
	return s_text;
}

bool CLC_FileMD5Check::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	// Reserved for future use.
	buffer.WriteOneBit( 0 );

	// Just write a couple bits for the path ID if it's one of the common ones.
	int iCode = FindCommonPathID( m_szPathID );
	if ( iCode == -1 )
	{
		buffer.WriteUBitLong( 0, 2 );
		buffer.WriteString( m_szPathID );
	}
	else
	{
		buffer.WriteUBitLong( iCode+1, 2 );
	}

	iCode = FindCommonPrefix( m_szFilename );
	if ( iCode == -1 )
	{
		buffer.WriteUBitLong( 0, 3 );
		buffer.WriteString( m_szFilename );
	}
	else
	{
		buffer.WriteUBitLong( iCode+1, 3 );
		buffer.WriteString( &m_szFilename[ V_strlen(g_MostCommonPrefixes[iCode])+1 ] );
	}

	buffer.WriteBytes( m_MD5.bits, MD5_DIGEST_LENGTH );
	return !buffer.IsOverflowed();
}

bool CLC_FileMD5Check::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "CLC_FileMD5Check::ReadFromBuffer" );

	// Reserved for future use.
	buffer.ReadOneBit();

	// Read the path ID.
	int iCode = buffer.ReadUBitLong( 2 );
	if ( iCode == 0 )
	{
		buffer.ReadString( m_szPathID, sizeof( m_szPathID ) );
	}
	else if ( (iCode-1) < ARRAYSIZE( g_MostCommonPathIDs ) )
	{
		V_strncpy( m_szPathID, g_MostCommonPathIDs[iCode-1], sizeof( m_szPathID ) );
	}
	else
	{
		Assert( !"Invalid path ID code in CLC_FileMD5Check" );
		return false;
	}

	// Read the filename.
	iCode = buffer.ReadUBitLong( 3 );
	if ( iCode == 0 )
	{
		buffer.ReadString( m_szFilename, sizeof( m_szFilename ) );
	}
	else if ( (iCode-1) < ARRAYSIZE( g_MostCommonPrefixes ) )
	{
		char szTemp[MAX_PATH];
		buffer.ReadString( szTemp, sizeof( szTemp ) );
		V_snprintf( m_szFilename, sizeof( m_szFilename ), "%s%c%s", g_MostCommonPrefixes[iCode-1], CORRECT_PATH_SEPARATOR, szTemp );
	}
	else
	{
		Assert( !"Invalid prefix code in CLC_FileMD5Check." );
		return false;
	}

	buffer.ReadBytes( m_MD5.bits, MD5_DIGEST_LENGTH );

	return !buffer.IsOverflowed();
}

const char *CLC_FileMD5Check::ToString(void) const
{
	V_snprintf( s_text, sizeof(s_text), "%s: path: %s, file: %s", GetName(), m_szPathID, m_szFilename );
	return s_text;
}

#if defined( REPLAY_ENABLED )
bool CLC_SaveReplay::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteString( m_szFilename );
	buffer.WriteUBitLong( m_nStartSendByte, sizeof( m_nStartSendByte ) );
	buffer.WriteFloat( m_flPostDeathRecordTime );
	return !buffer.IsOverflowed();
}

bool CLC_SaveReplay::ReadFromBuffer( bf_read &buffer )
{
	buffer.ReadString( m_szFilename, sizeof( m_szFilename ) );
	m_nStartSendByte = buffer.ReadUBitLong( sizeof( m_nStartSendByte ) );
	m_flPostDeathRecordTime = buffer.ReadFloat();
	return !buffer.IsOverflowed();
}

const char *CLC_SaveReplay::ToString() const
{
	V_snprintf( s_text, sizeof( s_text ), "%s: filename: %s, start byte: %i, post death record time: %f", GetName(), m_szFilename, m_nStartSendByte, m_flPostDeathRecordTime );
	return s_text;
}
#endif


//
// CmdKeyValues message
//

Base_CmdKeyValues::Base_CmdKeyValues( KeyValues *pKeyValues /* = NULL */ ) :
	m_pKeyValues( pKeyValues )
{
}

Base_CmdKeyValues::~Base_CmdKeyValues()
{
	if ( m_pKeyValues )
		m_pKeyValues->deleteThis();
	m_pKeyValues = NULL;
}

bool Base_CmdKeyValues::WriteToBuffer( bf_write &buffer )
{
	Assert( m_pKeyValues );
	if ( !m_pKeyValues )
		return false;

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	CUtlBuffer bufData;
	if ( !m_pKeyValues->WriteAsBinary( bufData ) )
	{
		Assert( false );
		return false;
	}

	// Note how many we're sending
	int numBytes = bufData.TellMaxPut();
	buffer.WriteLong( numBytes );
	buffer.WriteBits( bufData.Base(), numBytes * 8 );

	return !buffer.IsOverflowed();
}

bool Base_CmdKeyValues::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "Base_CmdKeyValues::ReadFromBuffer" );

	if ( !m_pKeyValues )
		m_pKeyValues = new KeyValues( "" );

	m_pKeyValues->Clear();

	int numBytes = buffer.ReadLong();
	if ( numBytes <= 0 || numBytes > buffer.GetNumBytesLeft() )
	{
		return false; // don't read past the end of the buffer
	}

	void *pvBuffer = malloc( numBytes );
	if ( !pvBuffer )
	{
		return false;
	}

	buffer.ReadBits( pvBuffer, numBytes * 8 );

	CUtlBuffer bufRead( pvBuffer, numBytes, CUtlBuffer::READ_ONLY );
	if ( !m_pKeyValues->ReadAsBinary( bufRead ) )
	{
		Assert( false );
		free( pvBuffer );
		return false;
	}

	free( pvBuffer );
	return !buffer.IsOverflowed();
}

const char * Base_CmdKeyValues::ToString(void) const
{
	Q_snprintf( s_text, sizeof(s_text), "%s: %s", 
		GetName(), m_pKeyValues ? m_pKeyValues->GetName() : "<<null>>" );
	return s_text;
}

CLC_CmdKeyValues::CLC_CmdKeyValues( KeyValues *pKeyValues /* = NULL */ ) : Base_CmdKeyValues( pKeyValues )
{
}

bool CLC_CmdKeyValues::WriteToBuffer( bf_write &buffer )
{
	return Base_CmdKeyValues::WriteToBuffer( buffer );
}

bool CLC_CmdKeyValues::ReadFromBuffer( bf_read &buffer )
{
	return Base_CmdKeyValues::ReadFromBuffer( buffer );
}

const char *CLC_CmdKeyValues::ToString(void) const
{
	return Base_CmdKeyValues::ToString();
}

SVC_CmdKeyValues::SVC_CmdKeyValues( KeyValues *pKeyValues /* = NULL */ ) : Base_CmdKeyValues( pKeyValues )
{
}

bool SVC_CmdKeyValues::WriteToBuffer( bf_write &buffer )
{
	return Base_CmdKeyValues::WriteToBuffer( buffer );
}

bool SVC_CmdKeyValues::ReadFromBuffer( bf_read &buffer )
{
	return Base_CmdKeyValues::ReadFromBuffer( buffer );
}

const char *SVC_CmdKeyValues::ToString(void) const
{
	return Base_CmdKeyValues::ToString();
}


//
// SVC_Print message
//

bool SVC_Print::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	return buffer.WriteString( m_szText ? m_szText : " svc_print NULL" );
}

bool SVC_Print::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_Print::ReadFromBuffer" );

	m_szText = m_szTextBuffer;
	
	return buffer.ReadString(m_szTextBuffer, sizeof(m_szTextBuffer) );
}

const char *SVC_Print::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: \"%s\"", GetName(), m_szText );
	return s_text;
}

bool NET_StringCmd::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	return buffer.WriteString( m_szCommand ? m_szCommand : " NET_StringCmd NULL" );
}

bool NET_StringCmd::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "NET_StringCmd::ReadFromBuffer" );

	m_szCommand = m_szCommandBuffer;
	
	return buffer.ReadString(m_szCommandBuffer, sizeof(m_szCommandBuffer) );
}

const char *NET_StringCmd::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: \"%s\"", GetName(), m_szCommand );
	return s_text;
}

bool SVC_ServerInfo::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteShort ( m_nProtocol );
	buffer.WriteLong  ( m_nServerCount );
	buffer.WriteOneBit( m_bIsHLTV?1:0);
	buffer.WriteOneBit( m_bIsDedicated?1:0);
	buffer.WriteLong  ( 0xffffffff );  // Used to be client.dll CRC.  This was far before signed binaries, VAC, and cross-platform play
	buffer.WriteWord  ( m_nMaxClasses );
	buffer.WriteBytes( m_nMapMD5.bits, MD5_DIGEST_LENGTH );		// To prevent cheating with hacked maps
	buffer.WriteByte  ( m_nPlayerSlot );
	buffer.WriteByte  ( m_nMaxClients );
	buffer.WriteFloat ( m_fTickInterval );
	buffer.WriteChar  ( m_cOS );
	buffer.WriteString( m_szGameDir );
	buffer.WriteString( m_szMapName );
	buffer.WriteString( m_szSkyName );
	buffer.WriteString( m_szHostName );

#if defined( REPLAY_ENABLED )
	buffer.WriteOneBit( m_bIsReplay?1:0);
#endif

	return !buffer.IsOverflowed();
}

bool SVC_ServerInfo::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_ServerInfo::ReadFromBuffer" );

	m_szGameDir = m_szGameDirBuffer;
	m_szMapName = m_szMapNameBuffer;
	m_szSkyName = m_szSkyNameBuffer;
	m_szHostName = m_szHostNameBuffer;

	m_nProtocol		= buffer.ReadShort();
	m_nServerCount	= buffer.ReadLong();
	m_bIsHLTV		= buffer.ReadOneBit()!=0;
	m_bIsDedicated	= buffer.ReadOneBit()!=0;
	buffer.ReadLong();  // Legacy client CRC.
	m_nMaxClasses	= buffer.ReadWord();

	// Prevent cheating with hacked maps
	if ( m_nProtocol > PROTOCOL_VERSION_17 )
	{
		buffer.ReadBytes( m_nMapMD5.bits, MD5_DIGEST_LENGTH );
	}
	else
	{
		m_nMapCRC	= buffer.ReadLong();
	}

	m_nPlayerSlot	= buffer.ReadByte();
	m_nMaxClients	= buffer.ReadByte();
	m_fTickInterval	= buffer.ReadFloat();
	m_cOS			= buffer.ReadChar();
	buffer.ReadString( m_szGameDirBuffer, sizeof(m_szGameDirBuffer) );
	buffer.ReadString( m_szMapNameBuffer, sizeof(m_szMapNameBuffer) );
	buffer.ReadString( m_szSkyNameBuffer, sizeof(m_szSkyNameBuffer) );
	buffer.ReadString( m_szHostNameBuffer, sizeof(m_szHostNameBuffer) );

#if defined( REPLAY_ENABLED )
	// Only attempt to read the 'replay' bit if the net channel's protocol
	// version is greater or equal than the protocol version for replay's release.
	// INetChannel::GetProtocolVersion() will return PROTOCOL_VERSION for
	// a regular net channel, or the network protocol version from the demo
	// file, if we're playing back a demo.
	if ( m_NetChannel->GetProtocolVersion() >= PROTOCOL_VERSION_REPLAY )
	{
		m_bIsReplay = buffer.ReadOneBit() != 0;
	}
#endif

	return !buffer.IsOverflowed();
}

const char *SVC_ServerInfo::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: game \"%s\", map \"%s\", max %i", GetName(), m_szGameDir, m_szMapName, m_nMaxClients );
	return s_text;
}

bool NET_SignonState::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteByte( m_nSignonState );
	buffer.WriteLong( m_nSpawnCount );

	return !buffer.IsOverflowed();
}

bool NET_SignonState::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "NET_SignonState::ReadFromBuffer" );

	m_nSignonState = buffer.ReadByte();
	m_nSpawnCount = buffer.ReadLong();

	return !buffer.IsOverflowed();
}

const char *NET_SignonState::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: state %i, count %i", GetName(), m_nSignonState, m_nSpawnCount );
	return s_text;
}

bool SVC_BSPDecal::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteBitVec3Coord( m_Pos );
	buffer.WriteUBitLong( m_nDecalTextureIndex, MAX_DECAL_INDEX_BITS );

	if ( m_nEntityIndex != 0)
	{
		buffer.WriteOneBit( 1 );
		buffer.WriteUBitLong( m_nEntityIndex, MAX_EDICT_BITS );
		buffer.WriteUBitLong( m_nModelIndex, SP_MODEL_INDEX_BITS );
	}
	else
	{
		buffer.WriteOneBit( 0 );
	}
	buffer.WriteOneBit( m_bLowPriority ? 1 : 0 );

	return !buffer.IsOverflowed();
}

bool SVC_BSPDecal::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_BSPDecal::ReadFromBuffer" );

	buffer.ReadBitVec3Coord( m_Pos );
	m_nDecalTextureIndex = buffer.ReadUBitLong( MAX_DECAL_INDEX_BITS );

	if ( buffer.ReadOneBit() != 0 )
	{
		m_nEntityIndex = buffer.ReadUBitLong( MAX_EDICT_BITS );
		m_nModelIndex = buffer.ReadUBitLong( SP_MODEL_INDEX_BITS );
	}
	else
	{
		m_nEntityIndex = 0;
		m_nModelIndex = 0;
	}
	m_bLowPriority = buffer.ReadOneBit() ? true : false;
	
	return !buffer.IsOverflowed();
}

const char *SVC_BSPDecal::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: tex %i, ent %i, mod %i lowpriority %i", 
		GetName(), m_nDecalTextureIndex, m_nEntityIndex, m_nModelIndex, m_bLowPriority ? 1 : 0 );
	return s_text;
}

bool SVC_SetView::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteUBitLong( m_nEntityIndex, MAX_EDICT_BITS );
	return !buffer.IsOverflowed();
}

bool SVC_SetView::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_SetView::ReadFromBuffer" );

	m_nEntityIndex = buffer.ReadUBitLong( MAX_EDICT_BITS );
	return !buffer.IsOverflowed();
}

const char *SVC_SetView::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: view entity %i", GetName(), m_nEntityIndex );
	return s_text;
}

bool SVC_FixAngle::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteOneBit( m_bRelative ? 1 : 0 );
	buffer.WriteBitAngle( m_Angle.x, 16 );
	buffer.WriteBitAngle( m_Angle.y, 16 );
	buffer.WriteBitAngle( m_Angle.z, 16 );
	return !buffer.IsOverflowed();
}

bool SVC_FixAngle::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_FixAngle::ReadFromBuffer" );

	m_bRelative = buffer.ReadOneBit() != 0;
	m_Angle.x = buffer.ReadBitAngle( 16 );
	m_Angle.y = buffer.ReadBitAngle( 16 );
	m_Angle.z = buffer.ReadBitAngle( 16 );
	return !buffer.IsOverflowed();
}

const char *SVC_FixAngle::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: %s %.1f %.1f %.1f ", GetName(), m_bRelative?"relative":"absolute",
		m_Angle[0], m_Angle[1], m_Angle[2] );
	return s_text;
}

bool SVC_CrosshairAngle::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteBitAngle( m_Angle.x, 16 );
	buffer.WriteBitAngle( m_Angle.y, 16 );
	buffer.WriteBitAngle( m_Angle.z, 16 );
	return !buffer.IsOverflowed();
}

bool SVC_CrosshairAngle::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_CrosshairAngle::ReadFromBuffer" );

	m_Angle.x = buffer.ReadBitAngle( 16 );
	m_Angle.y = buffer.ReadBitAngle( 16 );
	m_Angle.z = buffer.ReadBitAngle( 16 );
	return !buffer.IsOverflowed();
}

const char *SVC_CrosshairAngle::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: (%.1f %.1f %.1f)", GetName(), m_Angle[0], m_Angle[1], m_Angle[2] );
	return s_text;
}

bool SVC_VoiceInit::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteString( m_szVoiceCodec );
	buffer.WriteByte( /* Legacy Quality Field */ 255 );
	buffer.WriteShort( m_nSampleRate );
	return !buffer.IsOverflowed();
}

bool SVC_VoiceInit::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_VoiceInit::ReadFromBuffer" );

	buffer.ReadString( m_szVoiceCodec, sizeof(m_szVoiceCodec) );
	unsigned char nLegacyQuality = buffer.ReadByte();
	if ( nLegacyQuality == 255 )
	{
		// v2 packet
		m_nSampleRate = buffer.ReadShort();
	}
	else
	{
		// v1 packet
		//
		// Hacky workaround for v1 packets not actually indicating if we were using steam voice -- we've kept the steam
		// voice separate convar that was in use at the time as replicated&hidden, and if whatever network stream we're
		// interpreting sets it, lie about the subsequent voice init's codec & sample rate.
		if ( sv_use_steam_voice.GetBool() )
		{
			Msg( "Legacy SVC_VoiceInit - got a set for sv_use_steam_voice convar, assuming Steam voice\n" );
			V_strncpy( m_szVoiceCodec, "steam", sizeof( m_szVoiceCodec ) );
			// Legacy steam voice can always be parsed as auto sample rate.
			m_nSampleRate = 0;
		}
		else if ( V_strncasecmp( m_szVoiceCodec, "vaudio_celt", sizeof( m_szVoiceCodec ) ) == 0 )
		{
			// Legacy rate vaudio_celt always selected during v1 packet era
			m_nSampleRate = 22050;
		}
		else
		{
			// Legacy rate everything but CELT always selected during v1 packet era
			m_nSampleRate = 11025;
		}
	}

	return !buffer.IsOverflowed();
}

const char *SVC_VoiceInit::ToString(void) const
{
	Q_snprintf( s_text, sizeof(s_text), "%s: codec \"%s\", sample rate %i", GetName(), m_szVoiceCodec, m_nSampleRate );
	return s_text;
}

bool SVC_VoiceData::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteByte( m_nFromClient );
	buffer.WriteByte( m_bProximity );
	buffer.WriteWord( m_nLength );

	if ( IsX360() )
	{
		buffer.WriteLongLong( m_xuid );
	}

	return buffer.WriteBits( m_DataOut, m_nLength );
}

bool SVC_VoiceData::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_VoiceData::ReadFromBuffer" );

	m_nFromClient = buffer.ReadByte();
	m_bProximity = !!buffer.ReadByte();
	m_nLength = buffer.ReadWord();

	if ( IsX360() )
	{
		m_xuid =  buffer.ReadLongLong();
	}

	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_VoiceData::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: client %i, bytes %i", GetName(), m_nFromClient, Bits2Bytes(m_nLength) );
	return s_text;
}

#define NET_TICK_SCALEUP	100000.0f

bool NET_Tick::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteLong( m_nTick );
#if PROTOCOL_VERSION > 10
	buffer.WriteUBitLong( clamp( (int)( NET_TICK_SCALEUP * m_flHostFrameTime ), 0, 65535 ), 16 );
	buffer.WriteUBitLong( clamp( (int)( NET_TICK_SCALEUP * m_flHostFrameTimeStdDeviation ), 0, 65535 ), 16 );
#endif
	return !buffer.IsOverflowed();
}

bool NET_Tick::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "NET_Tick::ReadFromBuffer" );

	m_nTick = buffer.ReadLong();
#if PROTOCOL_VERSION > 10
	m_flHostFrameTime				= (float)buffer.ReadUBitLong( 16 ) / NET_TICK_SCALEUP;
	m_flHostFrameTimeStdDeviation	= (float)buffer.ReadUBitLong( 16 ) / NET_TICK_SCALEUP;
#endif
	return !buffer.IsOverflowed();
}

const char *NET_Tick::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: tick %i", GetName(), m_nTick );
	return s_text;
}

bool SVC_UserMessage::WriteToBuffer( bf_write &buffer )
{
	m_nLength = m_DataOut.GetNumBitsWritten();

	Assert( m_nLength < (1 << NETMSG_LENGTH_BITS) );
	if ( m_nLength >= (1 << NETMSG_LENGTH_BITS) )
		return false;

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteByte( m_nMsgType );
	buffer.WriteUBitLong( m_nLength, NETMSG_LENGTH_BITS );  // max 256 * 8 bits, see MAX_USER_MSG_DATA
	
	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
}

bool SVC_UserMessage::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_UserMessage::ReadFromBuffer" );
	m_nMsgType = buffer.ReadByte();
	m_nLength = buffer.ReadUBitLong( NETMSG_LENGTH_BITS ); // max 256 * 8 bits, see MAX_USER_MSG_DATA
	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_UserMessage::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: type %i, bytes %i", GetName(), m_nMsgType, Bits2Bytes(m_nLength) );
	return s_text;
}

bool SVC_SetPause::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteOneBit( m_bPaused?1:0 );
	return !buffer.IsOverflowed();
}

bool SVC_SetPause::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_SetPause::ReadFromBuffer" );

	m_bPaused = buffer.ReadOneBit() != 0;
	return !buffer.IsOverflowed();
}

const char *SVC_SetPause::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: %s", GetName(), m_bPaused?"paused":"unpaused" );
	return s_text;
}

bool SVC_SetPauseTimed::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteOneBit( m_bPaused ? 1 : 0 );
	buffer.WriteFloat( m_flExpireTime );
	return !buffer.IsOverflowed();
}

bool SVC_SetPauseTimed::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_SetPauseTimed::ReadFromBuffer" );

	m_bPaused = buffer.ReadOneBit() != 0;
	m_flExpireTime = buffer.ReadFloat();
	return !buffer.IsOverflowed();
}

const char *SVC_SetPauseTimed::ToString( void ) const
{
	Q_snprintf( s_text, sizeof( s_text ), "%s: %s", GetName(), m_bPaused ? "paused" : "unpaused" );
	return s_text;
}

bool NET_SetConVar::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	int numvars = m_ConVars.Count();

	// Note how many we're sending
	buffer.WriteByte( numvars );

	for (int i=0; i< numvars; i++ )
	{
		cvar_t * var = &m_ConVars[i];
		buffer.WriteString( var->name  );
		buffer.WriteString( var->value );
	}

	return !buffer.IsOverflowed();
}

bool NET_SetConVar::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "NET_SetConVar::ReadFromBuffer" );

	int numvars = buffer.ReadByte();

	m_ConVars.RemoveAll();

	for (int i=0; i< numvars; i++ )
	{
		cvar_t var;
		buffer.ReadString( var.name, sizeof(var.name) );
		buffer.ReadString( var.value, sizeof(var.value) );
		m_ConVars.AddToTail( var );

	}
	return !buffer.IsOverflowed();
}

const char *NET_SetConVar::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: %i cvars, \"%s\"=\"%s\"", 
		GetName(), m_ConVars.Count(), 
		m_ConVars[0].name, m_ConVars[0].value );
	return s_text;
}

bool SVC_UpdateStringTable::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	m_nLength = m_DataOut.GetNumBitsWritten();

	buffer.WriteUBitLong( m_nTableID, Q_log2( MAX_TABLES ) );	// TODO check bounds

	if ( m_nChangedEntries == 1 )
	{
		buffer.WriteOneBit( 0 ); // only one entry changed
	}
	else
	{
		buffer.WriteOneBit( 1 );
		buffer.WriteWord( m_nChangedEntries );	// more entries changed
	}

	buffer.WriteUBitLong( m_nLength, 20 );
	
	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
}

bool SVC_UpdateStringTable::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_UpdateStringTable::ReadFromBuffer" );

	m_nTableID = buffer.ReadUBitLong( Q_log2( MAX_TABLES ) );

	if ( buffer.ReadOneBit() != 0 )
	{
		m_nChangedEntries = buffer.ReadWord();
	}
	else
	{
		m_nChangedEntries = 1;
	}
	
	m_nLength = buffer.ReadUBitLong( 20 );
	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_UpdateStringTable::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: table %i, changed %i, bytes %i", GetName(), m_nTableID, m_nChangedEntries, Bits2Bytes(m_nLength) );
	return s_text;
}

SVC_CreateStringTable::SVC_CreateStringTable()
{

}

bool SVC_CreateStringTable::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	m_nLength = m_DataOut.GetNumBitsWritten();

	/*
	JASON: this code is no longer needed; the ':' is prepended to the table name at table creation time.
	if ( m_bIsFilenames )
	{
		// identifies a table that hosts filenames
		buffer.WriteByte( ':' );
	}
	*/

	buffer.WriteString( m_szTableName );
	buffer.WriteWord( m_nMaxEntries );
	int encodeBits = Q_log2( m_nMaxEntries );
	buffer.WriteUBitLong( m_nNumEntries, encodeBits+1 );
	buffer.WriteVarInt32( m_nLength ); // length in bits

	buffer.WriteOneBit( m_bUserDataFixedSize ? 1 : 0 );
	if ( m_bUserDataFixedSize )
	{
		buffer.WriteUBitLong( m_nUserDataSize, 12 );
		buffer.WriteUBitLong( m_nUserDataSizeBits, 4 );
	}
	
	buffer.WriteOneBit( m_bDataCompressed ? 1 : 0 );
	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
}

bool SVC_CreateStringTable::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_CreateStringTable::ReadFromBuffer" );

	char prefix = buffer.PeekUBitLong( 8 );
	if ( prefix == ':' )
	{
		// table hosts filenames
		m_bIsFilenames = true;
		buffer.ReadByte();
	}
	else
	{
		m_bIsFilenames = false;
	}

	m_szTableName = m_szTableNameBuffer;
	buffer.ReadString( m_szTableNameBuffer, sizeof(m_szTableNameBuffer) );
	m_nMaxEntries = buffer.ReadWord();
	int encodeBits = Q_log2( m_nMaxEntries );
	m_nNumEntries = buffer.ReadUBitLong( encodeBits+1 );
	if ( m_NetChannel->GetProtocolVersion() > PROTOCOL_VERSION_23 )
		m_nLength = buffer.ReadVarInt32();
	else
		m_nLength = buffer.ReadUBitLong( NET_MAX_PAYLOAD_BITS_V23 + 3 );

	m_bUserDataFixedSize = buffer.ReadOneBit() ? true : false;
	if ( m_bUserDataFixedSize )
	{
		m_nUserDataSize = buffer.ReadUBitLong( 12 );
		m_nUserDataSizeBits = buffer.ReadUBitLong( 4 );
	}
	else
	{
		m_nUserDataSize = 0;
		m_nUserDataSizeBits = 0;
	}

	if ( m_pMessageHandler->GetDemoProtocolVersion() > PROTOCOL_VERSION_14 )
	{
		m_bDataCompressed = buffer.ReadOneBit() != 0;
	}
	else
	{
		m_bDataCompressed = false;
	}

	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_CreateStringTable::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: table %s, entries %i, bytes %i userdatasize %i userdatabits %i", 
		GetName(), m_szTableName, m_nNumEntries, Bits2Bytes(m_nLength), m_nUserDataSize, m_nUserDataSizeBits );
	return s_text;
}

bool SVC_Sounds::WriteToBuffer( bf_write &buffer )
{
	m_nLength = m_DataOut.GetNumBitsWritten();

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	Assert( m_nNumSounds > 0 );
	
	if ( m_bReliableSound )
	{
		// as single sound message is 32 bytes long maximum
		buffer.WriteOneBit( 1 );
		buffer.WriteUBitLong( m_nLength, 8 );
	}
	else
	{
		// a bunch of unreliable messages
		buffer.WriteOneBit( 0 );
		buffer.WriteUBitLong( m_nNumSounds, 8 );
		buffer.WriteUBitLong( m_nLength, 16  );
	}
	
	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
}

bool SVC_Sounds::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_Sounds::ReadFromBuffer" );

	m_bReliableSound = buffer.ReadOneBit() != 0;

	if ( m_bReliableSound )
	{
		m_nNumSounds = 1;
		m_nLength = buffer.ReadUBitLong( 8 );

	}
	else
	{
		m_nNumSounds = buffer.ReadUBitLong( 8 );
		m_nLength = buffer.ReadUBitLong( 16 );
	}
		
	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_Sounds::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: number %i,%s bytes %i", 
		GetName(), m_nNumSounds, m_bReliableSound?" reliable,":"", Bits2Bytes(m_nLength) );
	return s_text;
} 

bool SVC_Prefetch::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	// Don't write type until we have more thanone
	// buffer.WriteUBitLong( m_fType, 1 );
	buffer.WriteUBitLong( m_nSoundIndex, MAX_SOUND_INDEX_BITS );
	return !buffer.IsOverflowed();
}

bool SVC_Prefetch::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_Prefetch::ReadFromBuffer" );

	m_fType = SOUND; // buffer.ReadUBitLong( 1 );
	if( m_pMessageHandler->GetDemoProtocolVersion() > 22 )
	{
		m_nSoundIndex = buffer.ReadUBitLong( MAX_SOUND_INDEX_BITS );
	}
	else
	{
		m_nSoundIndex = buffer.ReadUBitLong( 13 );
	}

	return !buffer.IsOverflowed();
}

const char *SVC_Prefetch::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: type %i index %i", 
		GetName(), 
		(int)m_fType, 
		(int)m_nSoundIndex );
	return s_text;
} 

bool SVC_TempEntities::WriteToBuffer( bf_write &buffer )
{
	Assert( m_nNumEntries > 0 );

	m_nLength = m_DataOut.GetNumBitsWritten();

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteUBitLong( m_nNumEntries, CEventInfo::EVENT_INDEX_BITS );
	buffer.WriteVarInt32( m_nLength );
	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
}

bool SVC_TempEntities::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_TempEntities::ReadFromBuffer" );

	m_nNumEntries = buffer.ReadUBitLong( CEventInfo::EVENT_INDEX_BITS );
	if ( m_pMessageHandler->GetDemoProtocolVersion() > PROTOCOL_VERSION_23 )
		m_nLength = buffer.ReadVarInt32();
	else
		m_nLength = buffer.ReadUBitLong( NET_MAX_PAYLOAD_BITS_V23 );

	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_TempEntities::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: number %i, bytes %i", GetName(), m_nNumEntries, Bits2Bytes(m_nLength) );
	return s_text;
} 

bool SVC_ClassInfo::WriteToBuffer( bf_write &buffer )
{
	if ( !m_bCreateOnClient )
	{
		m_nNumServerClasses = m_Classes.Count();	// use number from list list	
	}
	
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	buffer.WriteShort( m_nNumServerClasses );

	int serverClassBits = Q_log2( m_nNumServerClasses ) + 1;

	buffer.WriteOneBit( m_bCreateOnClient?1:0 );

	if ( m_bCreateOnClient )
		return !buffer.IsOverflowed();

	for ( int i=0; i< m_nNumServerClasses; i++ )
	{
		class_t * serverclass = &m_Classes[i];

		buffer.WriteUBitLong( serverclass->classID, serverClassBits );
		buffer.WriteString( serverclass->classname );
		buffer.WriteString( serverclass->datatablename );
	}

	return !buffer.IsOverflowed();
}

bool SVC_ClassInfo::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_ClassInfo::ReadFromBuffer" );

	m_Classes.RemoveAll();

	m_nNumServerClasses = buffer.ReadShort();

	int nServerClassBits = Q_log2( m_nNumServerClasses ) + 1;

	m_bCreateOnClient = buffer.ReadOneBit() != 0;

	if ( m_bCreateOnClient )
	{
		return !buffer.IsOverflowed(); // stop here
	}

	for ( int i=0; i<m_nNumServerClasses; i++ )
	{
		class_t serverclass;

		serverclass.classID = buffer.ReadUBitLong( nServerClassBits );
		buffer.ReadString( serverclass.classname, sizeof(serverclass.classname) );
		buffer.ReadString( serverclass.datatablename, sizeof(serverclass.datatablename) );

		m_Classes.AddToTail( serverclass );
	}
	
	return !buffer.IsOverflowed();
}

const char *SVC_ClassInfo::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: num %i, %s", GetName(), 
		m_nNumServerClasses, m_bCreateOnClient?"use client classes":"full update" );
	return s_text;
} 

/*
bool SVC_SpawnBaseline::WriteToBuffer( bf_write &buffer )
{
	m_nLength = m_DataOut.GetNumBitsWritten();

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	buffer.WriteUBitLong( m_nEntityIndex, MAX_EDICT_BITS );

	buffer.WriteUBitLong( m_nClassID, MAX_SERVER_CLASS_BITS );

	buffer.WriteUBitLong( m_nLength, Q_log2(MAX_PACKEDENTITY_DATA<<3) ); // TODO see below

	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
}

bool SVC_SpawnBaseline::ReadFromBuffer( bf_read &buffer )
{
	m_nEntityIndex = buffer.ReadUBitLong( MAX_EDICT_BITS );

	m_nClassID = buffer.ReadUBitLong( MAX_SERVER_CLASS_BITS );

	m_nLength = buffer.ReadUBitLong( Q_log2(MAX_PACKEDENTITY_DATA<<3) ); // TODO wrong, check bounds

	m_DataIn = buffer;

	return buffer.SeekRelative( m_nLength );
}

const char *SVC_SpawnBaseline::ToString(void) const
{
	static char text[256];
	Q_snprintf(text, sizeof(text), "%s: ent %i, class %i, bytes %i", 
		GetName(), m_nEntityIndex, m_nClassID, Bits2Bytes(m_nLength) );
	return text;
} */

bool SVC_GameEvent::WriteToBuffer( bf_write &buffer )
{
	m_nLength = m_DataOut.GetNumBitsWritten();
	Assert( m_nLength < (1 << NETMSG_LENGTH_BITS) );
	if ( m_nLength >= (1 << NETMSG_LENGTH_BITS) )
		return false;

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteUBitLong( m_nLength, NETMSG_LENGTH_BITS );  // max 8 * 256 bits
	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
	
}

bool SVC_GameEvent::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_GameEvent::ReadFromBuffer" );

	m_nLength = buffer.ReadUBitLong( NETMSG_LENGTH_BITS ); // max 8 * 256 bits
	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_GameEvent::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: bytes %i", GetName(), Bits2Bytes(m_nLength) );
	return s_text;
} 

bool SVC_SendTable::WriteToBuffer( bf_write &buffer )
{
	m_nLength = m_DataOut.GetNumBitsWritten();

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteOneBit( m_bNeedsDecoder?1:0 );
	buffer.WriteShort( m_nLength );
	buffer.WriteBits( m_DataOut.GetData(), m_nLength );

	return !buffer.IsOverflowed();
}

bool SVC_SendTable::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_SendTable::ReadFromBuffer" );

	m_bNeedsDecoder = buffer.ReadOneBit() != 0;
	m_nLength = buffer.ReadShort();		// TODO do we have a maximum length ? check that

	m_DataIn = buffer;

	return buffer.SeekRelative( m_nLength );
}

const char *SVC_SendTable::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: needs Decoder %s,bytes %i", 
		GetName(), m_bNeedsDecoder?"yes":"no", Bits2Bytes(m_nLength) );
	return s_text;
} 

bool SVC_EntityMessage::WriteToBuffer( bf_write &buffer )
{
	m_nLength = m_DataOut.GetNumBitsWritten();
	Assert( m_nLength < (1 << NETMSG_LENGTH_BITS) );
	if ( m_nLength >= (1 << NETMSG_LENGTH_BITS) )
		return false;

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteUBitLong( m_nEntityIndex, MAX_EDICT_BITS );
	buffer.WriteUBitLong( m_nClassID, MAX_SERVER_CLASS_BITS );
	buffer.WriteUBitLong( m_nLength, NETMSG_LENGTH_BITS );  // max 8 * 256 bits
	
	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
}

bool SVC_EntityMessage::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_EntityMessage::ReadFromBuffer" );

	m_nEntityIndex = buffer.ReadUBitLong( MAX_EDICT_BITS );
	m_nClassID = buffer.ReadUBitLong( MAX_SERVER_CLASS_BITS );
	m_nLength = buffer.ReadUBitLong( NETMSG_LENGTH_BITS );  // max 8 * 256 bits
	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_EntityMessage::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: entity %i, class %i, bytes %i",
		GetName(), m_nEntityIndex, m_nClassID, Bits2Bytes(m_nLength) );
	return s_text;
}

bool SVC_PacketEntities::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	buffer.WriteUBitLong( m_nMaxEntries, MAX_EDICT_BITS );
	
	buffer.WriteOneBit( m_bIsDelta?1:0 );

	if ( m_bIsDelta )
	{
		buffer.WriteLong( m_nDeltaFrom );
	}

	buffer.WriteUBitLong( m_nBaseline, 1 );

	buffer.WriteUBitLong( m_nUpdatedEntries, MAX_EDICT_BITS );

	buffer.WriteUBitLong( m_nLength, DELTASIZE_BITS );

	buffer.WriteOneBit( m_bUpdateBaseline?1:0 ); 

	buffer.WriteBits( m_DataOut.GetData(), m_nLength );

	return !buffer.IsOverflowed();
}

bool SVC_PacketEntities::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_PacketEntities::ReadFromBuffer" );

	m_nMaxEntries = buffer.ReadUBitLong( MAX_EDICT_BITS );
	
	m_bIsDelta = buffer.ReadOneBit()!=0;

	if ( m_bIsDelta )
	{
		m_nDeltaFrom = buffer.ReadLong();
	}
	else
	{
		m_nDeltaFrom = -1;
	}

	m_nBaseline = buffer.ReadUBitLong( 1 );

	m_nUpdatedEntries = buffer.ReadUBitLong( MAX_EDICT_BITS );

	m_nLength = buffer.ReadUBitLong( DELTASIZE_BITS );

	m_bUpdateBaseline = buffer.ReadOneBit() != 0;

	m_DataIn = buffer;
	
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_PacketEntities::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: delta %i, max %i, changed %i,%s bytes %i",
		GetName(), m_nDeltaFrom, m_nMaxEntries, m_nUpdatedEntries, m_bUpdateBaseline?" BL update,":"", Bits2Bytes(m_nLength) );
	return s_text;
} 

SVC_Menu::SVC_Menu( DIALOG_TYPE type, KeyValues *data )
{
	m_bReliable = true;

	m_Type = type;
	m_MenuKeyValues = data->MakeCopy();
	m_iLength = -1;
}

SVC_Menu::~SVC_Menu()
{
	if ( m_MenuKeyValues )
	{
		m_MenuKeyValues->deleteThis();
	}
}

bool SVC_Menu::WriteToBuffer( bf_write &buffer )
{
	if ( !m_MenuKeyValues )
	{
		return false;
	}

	CUtlBuffer buf;
	m_MenuKeyValues->WriteAsBinary( buf );

	if ( buf.TellPut() > 4096 )
	{
		Msg( "Too much menu data (4096 bytes max)\n" );
		return false;
	}

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteShort( m_Type );
	buffer.WriteWord( buf.TellPut() );
	buffer.WriteBytes( buf.Base(), buf.TellPut() );

	return !buffer.IsOverflowed();
}

bool SVC_Menu::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_Menu::ReadFromBuffer" );

	m_Type = (DIALOG_TYPE)buffer.ReadShort();
	m_iLength = buffer.ReadWord();

	CUtlBuffer buf( 0, m_iLength );
	buffer.ReadBytes( buf.Base(), m_iLength );
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, m_iLength );

	if ( m_MenuKeyValues ) 
	{
		m_MenuKeyValues->deleteThis();
	}
	m_MenuKeyValues = new KeyValues( "menu" );
	Assert( m_MenuKeyValues );
	
	m_MenuKeyValues->ReadAsBinary( buf );

	return !buffer.IsOverflowed();
}

const char *SVC_Menu::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: %i \"%s\" (len:%i)", GetName(),
		m_Type, m_MenuKeyValues ? m_MenuKeyValues->GetName() : "No KeyValues", m_iLength );
	return s_text;
} 

bool SVC_GameEventList::WriteToBuffer( bf_write &buffer )
{
	Assert( m_nNumEvents > 0 );

	m_nLength = m_DataOut.GetNumBitsWritten();

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteUBitLong( m_nNumEvents, MAX_EVENT_BITS );
	buffer.WriteUBitLong( m_nLength, 20  );
	return buffer.WriteBits( m_DataOut.GetData(), m_nLength );
}

bool SVC_GameEventList::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_GameEventList::ReadFromBuffer" );

	m_nNumEvents = buffer.ReadUBitLong( MAX_EVENT_BITS );
	m_nLength = buffer.ReadUBitLong( 20 );
	m_DataIn = buffer;
	return buffer.SeekRelative( m_nLength );
}

const char *SVC_GameEventList::ToString(void) const
{
	Q_snprintf(s_text, sizeof(s_text), "%s: number %i, bytes %i", GetName(), m_nNumEvents, Bits2Bytes(m_nLength) );
	return s_text;
} 

///////////////////////////////////////////////////////////////////////////////////////
// Matchmaking messages:
///////////////////////////////////////////////////////////////////////////////////////

bool MM_Heartbeat::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	return !buffer.IsOverflowed();
}

bool MM_Heartbeat::ReadFromBuffer( bf_read &buffer )
{
	return true;
}

const char *MM_Heartbeat::ToString( void ) const
{
	Q_snprintf( s_text, sizeof( s_text ), "Heartbeat" );
	return s_text;
}

bool MM_ClientInfo::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteBytes( &m_xnaddr, sizeof( m_xnaddr ) );
	buffer.WriteLongLong( m_id );			// 64 bit
	buffer.WriteByte( m_cPlayers );
	buffer.WriteByte( m_bInvited );
	for ( int i = 0; i < m_cPlayers; ++i )
	{
		buffer.WriteLongLong( m_xuids[i] );	// 64 bit
		buffer.WriteBytes( &m_cVoiceState, sizeof( m_cVoiceState ) );
		buffer.WriteLong( m_iTeam[i] );
		buffer.WriteByte( m_iControllers[i] );
		buffer.WriteString( m_szGamertags[i] );
	}

	return !buffer.IsOverflowed();
}

bool MM_ClientInfo::ReadFromBuffer( bf_read &buffer )
{
	buffer.ReadBytes( &m_xnaddr, sizeof( m_xnaddr ) );
	m_id = buffer.ReadLongLong();			// 64 bit
	m_cPlayers = buffer.ReadByte();
	m_bInvited = (buffer.ReadByte() != 0);
	for ( int i = 0; i < m_cPlayers; ++i )
	{
		m_xuids[i] = buffer.ReadLongLong();	// 64 bit
		buffer.ReadBytes( &m_cVoiceState, sizeof( m_cVoiceState ) );
		m_iTeam[i] = buffer.ReadLong();
		m_iControllers[i] = buffer.ReadByte();
		buffer.ReadString( m_szGamertags[i], sizeof( m_szGamertags[i] ), true );
	}

	return !buffer.IsOverflowed();
}

const char *MM_ClientInfo::ToString( void ) const
{
	Q_snprintf( s_text, sizeof( s_text ), "Client Info: ID: %llu, Players: %d", m_id, m_cPlayers );
	return s_text;
}

bool MM_RegisterResponse::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	return !buffer.IsOverflowed();
}

bool MM_RegisterResponse::ReadFromBuffer( bf_read &buffer )
{
	return true;
}

const char *MM_RegisterResponse::ToString( void ) const
{
	Q_snprintf( s_text, sizeof( s_text ), "Register Response" );
	return s_text;
}

bool MM_Mutelist::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	buffer.WriteLongLong( m_id );
	buffer.WriteByte( m_cPlayers );
	for ( int i = 0; i < m_cPlayers; ++i )
	{
		buffer.WriteByte( m_cRemoteTalkers[i] );
		buffer.WriteLongLong( m_xuid[i] ); // 64 bit
		buffer.WriteByte( m_cMuted[i] );
		for ( int j = 0; j < m_cMuted[i]; ++j )
		{
			buffer.WriteLongLong( m_Muted[i][j] );
		}
	}
	return !buffer.IsOverflowed();
}

bool MM_Mutelist::ReadFromBuffer( bf_read &buffer )
{
	m_id = buffer.ReadLongLong();
	m_cPlayers = buffer.ReadByte();
	for ( int i = 0; i < m_cPlayers; ++i )
	{
		m_cRemoteTalkers[i] = buffer.ReadByte();
		m_xuid[i] = buffer.ReadLongLong();	// 64 bit
		m_cMuted[i] = buffer.ReadByte();
		for ( int j = 0; j < m_cMuted[i]; ++j )
		{
			m_Muted[i].AddToTail( buffer.ReadLongLong() );
		}
	}
	return !buffer.IsOverflowed();
}

const char *MM_Mutelist::ToString( void ) const
{
	Q_snprintf( s_text, sizeof( s_text ), "Mutelist" );
	return s_text;
}

bool MM_Checkpoint::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteByte( m_Checkpoint );
	return !buffer.IsOverflowed();
}

bool MM_Checkpoint::ReadFromBuffer( bf_read &buffer )
{
	m_Checkpoint = buffer.ReadByte();
	return !buffer.IsOverflowed();
}

const char *MM_Checkpoint::ToString( void ) const
{
	Q_snprintf( s_text, sizeof( s_text ), "Checkpoint: %d", m_Checkpoint );
	return s_text;
}

// NOTE: This message is not network-endian compliant, due to the
// transmission of structures instead of their component parts
bool MM_JoinResponse::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteLong( m_ResponseType );		
	buffer.WriteLongLong( m_id );		// 64 bit
	buffer.WriteLongLong( m_Nonce );	// 64 bit
	buffer.WriteLong( m_SessionFlags );
	buffer.WriteLong( m_nOwnerId );
	buffer.WriteLong( m_iTeam );
	buffer.WriteLong( m_nTotalTeams );
	buffer.WriteByte( m_PropertyCount );
	buffer.WriteByte( m_ContextCount );
	for ( int i = 0; i < m_PropertyCount; ++i )
	{
		buffer.WriteBytes( &m_SessionProperties[i], sizeof( XUSER_PROPERTY ) );
	}
	for ( int i = 0; i < m_ContextCount; ++i )
	{
		buffer.WriteBytes( &m_SessionContexts[i], sizeof( XUSER_CONTEXT ) );
	}

	return !buffer.IsOverflowed();
}

bool MM_JoinResponse::ReadFromBuffer( bf_read &buffer )
{
	m_ResponseType = buffer.ReadLong();
	m_id = buffer.ReadLongLong();		// 64 bit
	m_Nonce = buffer.ReadLongLong();	// 64 bit
	m_SessionFlags = buffer.ReadLong();
	m_nOwnerId = buffer.ReadLong();
	m_iTeam = buffer.ReadLong();
	m_nTotalTeams = buffer.ReadLong();
	m_PropertyCount = buffer.ReadByte();
	m_ContextCount = buffer.ReadByte();
	XUSER_PROPERTY prop;
	m_SessionProperties.RemoveAll();
	for ( int i = 0; i < m_PropertyCount; ++i )
	{
		buffer.ReadBytes( &prop, sizeof( XUSER_PROPERTY ) );
		m_SessionProperties.AddToTail( prop );
	}
	XUSER_CONTEXT ctx;
	m_SessionContexts.RemoveAll();
	for ( int i = 0; i < m_ContextCount; ++i )
	{
		buffer.ReadBytes( &ctx, sizeof( XUSER_CONTEXT ) );
		m_SessionContexts.AddToTail( ctx );
	}

	return !buffer.IsOverflowed();
}

const char *MM_JoinResponse::ToString( void ) const
{
	Q_snprintf( s_text, sizeof( s_text ), "ID: %llu, Nonce: %llu, Flags: %u", m_id, m_Nonce, m_SessionFlags );
	return s_text;
}

// NOTE: This message is not network-endian compliant, due to the
// transmission of structures instead of their component parts
bool MM_Migrate::WriteToBuffer( bf_write &buffer )
{
	Assert( IsX360() );

	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );
	buffer.WriteByte( m_MsgType );
	buffer.WriteLongLong( m_Id );
	buffer.WriteBytes( &m_sessionId, sizeof( m_sessionId ) );
	buffer.WriteBytes( &m_xnaddr, sizeof( m_xnaddr ) );
	buffer.WriteBytes( &m_key, sizeof( m_key ) );
	return !buffer.IsOverflowed();
}

bool MM_Migrate::ReadFromBuffer( bf_read &buffer )
{
	Assert( IsX360() );

	m_MsgType = buffer.ReadByte();
	m_Id = buffer.ReadLongLong();
	buffer.ReadBytes( &m_sessionId, sizeof( m_sessionId ) );
	buffer.ReadBytes( &m_xnaddr, sizeof( m_xnaddr ) );
	buffer.ReadBytes( &m_key, sizeof( m_key ) );
	return !buffer.IsOverflowed();
}

const char *MM_Migrate::ToString( void ) const
{
	Q_snprintf( s_text, sizeof( s_text ), "Migrate Message" );
	return s_text;
}


bool SVC_GetCvarValue::WriteToBuffer( bf_write &buffer )
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	buffer.WriteSBitLong( m_iCookie, 32 );
	buffer.WriteString( m_szCvarName );

	return !buffer.IsOverflowed();
}

bool SVC_GetCvarValue::ReadFromBuffer( bf_read &buffer )
{
	VPROF( "SVC_GetCvarValue::ReadFromBuffer" );

	m_iCookie = buffer.ReadSBitLong( 32 );
	buffer.ReadString( m_szCvarNameBuffer, sizeof( m_szCvarNameBuffer ) );
	m_szCvarName = m_szCvarNameBuffer;
	
	return !buffer.IsOverflowed();
}

const char *SVC_GetCvarValue::ToString(void) const
{
	Q_snprintf( s_text, sizeof(s_text), "%s: cvar: %s, cookie: %d", GetName(), m_szCvarName, m_iCookie );
	return s_text;
} 

