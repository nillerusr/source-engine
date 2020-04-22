//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifdef _WIN32
#if !defined( _X360 )
#include <windows.h>
#endif
#elif defined(POSIX)
#include <sys/socket.h>
#include <netinet/in.h>
#include <pwd.h>
#include <sys/types.h>
#else
#error
#endif

#include "host.h"
#include "quakedef.h"
#include "net.h"
#include "bitbuf.h"
#include "tier0/icommandline.h"
#include "cserserverprotocol_engine.h"
#include "host_phonehome.h"
#include "mathlib/IceKey.H"
#include "tier0/vcrmode.h"
#include "blockingudpsocket.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PHONE_HOME_TIMEOUT	1.5f
#define PHONE_HOME_RETRIES	3

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to a function, given a module
// Input  : pModuleName - module name
//			*pName - proc name
//-----------------------------------------------------------------------------
static char const *g_pszExitMsg = "Renderer:  Out of memory, message code %i";

class CPhoneHome : public IPhoneHome
{
public:
	CPhoneHome() :
		m_bPhoneHome( false ),
		m_uSessionID( 0 ),
		m_pSocket( 0 )
	{
		  Q_memset( &m_cserIP, 0, sizeof( m_cserIP ) );
		  Q_memset( m_szBuildIdentifier, 0, sizeof( m_szBuildIdentifier ) );
	}

	virtual void Shutdown()
	{
		delete m_pSocket;
		m_pSocket = NULL;
	}

	virtual void Init()
	{
		char build_identifier[ 32 ];

		Q_strncpy( build_identifier, "VLV_INTERNAL                    ", sizeof( build_identifier ) );
		int iBI = CommandLine()->FindParm("-bi");
		if ( iBI > 0 )
		{
			if ( (iBI+1) < CommandLine()->ParmCount() )
			{
				char const *pBuildParam = CommandLine()->GetParm( iBI + 1 );
				Q_memset( build_identifier, 0, sizeof( build_identifier ) );
				Q_strncpy( build_identifier, pBuildParam, sizeof( build_identifier ) );
			}
			else
			{
				build_identifier[ 0 ] = '!';
			}
		}

		if ( Q_strlen( build_identifier ) >= 1 &&
			Q_strnicmp( build_identifier, "VLV_INTERNAL", Q_strlen( "VLV_INTERNAL" ) ) )
		{
			// Strip trailing spaces from identifer
			char *identifer = &build_identifier[ Q_strlen( build_identifier ) - 1 ];
			while ( identifer > build_identifier && *identifer == ' ' )
			{
				*identifer-- = 0;
			}
			
			// FIXME:  Don't hardcode CSER ip, get from Steam!!!
			if ( NET_StringToAdr( "207.173.177.12:27013", &m_cserIP ) )
			{
				m_bPhoneHome = true;
				
				Q_strncpy( m_szBuildIdentifier, build_identifier, sizeof( m_szBuildIdentifier ) );

				m_pSocket = new CBlockingUDPSocket();
			}
		}
		
	}

	virtual void Message( byte msgtype, char const *mapname )
	{
		if ( !m_bPhoneHome )
			return;

		if ( !m_pSocket )
			return;

		switch ( msgtype )
		{
		default:
			break;
		case PHONE_MSG_ENGINESTART:
			if ( !RequestSessionId( m_uSessionID ) )
			{
				ExitApp();
			}
			// Note we always return here!!!
			return;
		case PHONE_MSG_ENGINEEND:
			break;
		case PHONE_MSG_MAPSTART:
			{
				if ( m_bLevelStarted )
				{
					return;
				}

				m_bLevelStarted = true;

				// Tracker 22394:  Don't send map start/finish when building reslists...
				if ( CommandLine()->FindParm( "-makereslists" ) )
				{
					return;
				}
			}
			break;
		case PHONE_MSG_MAPEND:
			{
				if ( !m_bLevelStarted )
				{
					return;
				}

				m_bLevelStarted = false;

				// Tracker 22394:  Don't send map start/finish when building reslists...
				if ( CommandLine()->FindParm( "-makereslists" ) )
				{
					return;
				}
			}
			break;
		}

		SendSessionMessage( msgtype, mapname );
	}

private:

	void	ExitApp()
	{
		byte msgtype = 212;
		Error( g_pszExitMsg, msgtype );
	}

	//-----------------------------------------------------------------------------
	// Purpose: encrypts an 8-byte sequence
	//-----------------------------------------------------------------------------
	inline void Encrypt8ByteSequence( IceKey& cipher, const unsigned char *plainText, unsigned char *cipherText)
	{
		cipher.encrypt(plainText, cipherText);
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	void EncryptBuffer( IceKey& cipher, unsigned char *bufData, uint bufferSize)
	{
		unsigned char *cipherText = bufData;
		unsigned char *plainText = bufData;
		uint bytesEncrypted = 0;

		while (bytesEncrypted < bufferSize)
		{
			// encrypt 8 byte section
			Encrypt8ByteSequence( cipher, plainText, cipherText);
			bytesEncrypted += 8;
			cipherText += 8;
			plainText += 8;
		}
	}

	void BuildMessage( bf_write& buf, byte msgtype, char const *mapname, unsigned int uSessionID )
	{
	
		bf_write	encrypted;
		ALIGN4 byte		encrypted_data[ 2048 ] ALIGN4_POST;

		buf.WriteByte( C2M_PHONEHOME );
		buf.WriteByte( '\n' );
		buf.WriteByte( C2M_PHONEHOME_PROTOCOL_VERSION );
		buf.WriteLong( uSessionID ); // sessionid (request new id by sending 0)

		// encryption object
		IceKey cipher(1); /* medium encryption level */
		unsigned char ucEncryptionKey[8] = { 191, 1, 0, 222, 85, 39, 154, 1 };
		cipher.set( ucEncryptionKey );

		encrypted.StartWriting( encrypted_data, sizeof( encrypted_data ) );

		byte corruption_identifier = 0x01;

		encrypted.WriteByte( corruption_identifier );

		// Data version protocol
		encrypted.WriteByte( 1 );

		// Write the "build identifier"  -- unique to each person we give a build to.
		encrypted.WriteString( m_szBuildIdentifier ); 
		{
			char computername[ 64 ];
			Q_memset( computername, 0, sizeof( computername ) );
#if defined ( _WIN32 )
			DWORD length = sizeof( computername ) - 1;
			if ( !GetComputerName( computername, &length ) )
			{
				Q_strncpy( computername, "???", sizeof( computername )  );
			}
#else
			if ( gethostname( computername, sizeof(computername) ) == -1 )
			{
				Q_strncpy( computername, "Linux????", sizeof( computername ) );
			}
			computername[sizeof(computername)-1] = '\0';
#endif
			encrypted.WriteString( computername );
		}

		{
			char username[ 64 ];
			Q_memset( username, 0, sizeof( username ) );
#if defined ( _WIN32 )
			DWORD length = sizeof( username ) - 1;
			if ( !GetUserName( username, &length ) )
			{
				Q_strncpy( username, "???", sizeof( username )  );
			}
#else
			struct passwd *pass = getpwuid( getuid() );
			if ( pass )
			{
				Q_strncpy( username, pass->pw_name, sizeof( username ) );
			}
			else
			{
				Q_strncpy( username, "LinuxUser??", sizeof( username ) );
			}
			username[sizeof(username)-1] = '\0';
#endif
			encrypted.WriteString( username );
		}

		char gamedir[ 64 ];
		Q_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );
		encrypted.WriteString( gamedir );

		unsigned int uBuildNumber = build_number();

		encrypted.WriteLong( (int)uBuildNumber );

		// WRite timestamp of engine
		encrypted.WriteFloat( (float)realtime );

		encrypted.WriteByte( msgtype );
		if ( mapname != NULL )
		{
			encrypted.WriteString( mapname );
		}

		int isDebugUser = ( Sys_IsDebuggerPresent() || CommandLine()->FindParm( "-allowdebug" ) ) ? 1 : 0;

		encrypted.WriteByte( isDebugUser );

		while ( encrypted.GetNumBytesWritten() % 8 )
		{
			encrypted.WriteByte( 0 );
		}

		EncryptBuffer( cipher, (unsigned char *)encrypted.GetData(), encrypted.GetNumBytesWritten() );

		buf.WriteShort( (int)encrypted.GetNumBytesWritten() );
		buf.WriteBytes( (unsigned char *)encrypted.GetData(), encrypted.GetNumBytesWritten() );
	}

	void SendSessionMessage( byte msgtype, char const *mapname )
	{
		if ( m_uSessionID == 0 )
			return;

		bf_write	buf;
		ALIGN4 byte		data[ 2048 ] ALIGN4_POST;
		
		buf.StartWriting( data, sizeof( data ) );

		BuildMessage( buf, msgtype, mapname, m_uSessionID );

		struct sockaddr_in sa;

		m_cserIP.ToSockadr( (struct sockaddr *)&sa );

		m_pSocket->SendSocketMessage( sa, (const byte *)buf.GetData(), buf.GetNumBytesWritten() );

		// If we already have a sessionid, don't wait for the server to give us back a new one...
		if ( m_uSessionID != 0 )
		{
			return;
		}

		if ( m_pSocket->WaitForMessage( PHONE_HOME_TIMEOUT ) )
		{	
			ALIGN4 byte		readbuf[ 128 ] ALIGN4_POST;

			bf_read		replybuf( readbuf, sizeof( readbuf ) );

			struct sockaddr_in replyaddress;
			uint bytesReceived = m_pSocket->ReceiveSocketMessage( &replyaddress, (byte *)readbuf, sizeof( readbuf ) );
			if ( bytesReceived > 0 )
			{
				// Fixup actual size
				replybuf.StartReading( readbuf, bytesReceived );

				// Parse out data
				byte responseType = (byte)replybuf.ReadByte();
				if ( M2C_ACKPHONEHOME == responseType  )
				{
					bool allowPlay = replybuf.ReadByte() == 1 ? true : false;
					if ( allowPlay )
					{
						m_uSessionID = replybuf.ReadLong();
					}
				}
			}
		}
	}

	bool RequestSessionId( unsigned int& id )
	{
		id = 0u;

		bf_write	buf;
		ALIGN4 byte		data[ 2048 ] ALIGN4_POST;
		
		buf.StartWriting( data, sizeof( data ) );

		BuildMessage( buf, PHONE_MSG_ENGINESTART, NULL, id );

		struct sockaddr_in sa;

		m_cserIP.ToSockadr( (struct sockaddr *)&sa );

		for ( int retries = 0; retries < PHONE_HOME_RETRIES; ++retries )
		{
			m_pSocket->SendSocketMessage( sa, (const byte *)buf.GetData(), buf.GetNumBytesWritten() ); //lint !e534
			if ( m_pSocket->WaitForMessage( PHONE_HOME_TIMEOUT ) )
			{	
				ALIGN4 byte		readbuf[ 128 ] ALIGN4_POST;

				bf_read		replybuf( readbuf, sizeof( readbuf ) );

				struct sockaddr_in replyaddress;
				uint bytesReceived = m_pSocket->ReceiveSocketMessage( &replyaddress, (byte *)readbuf, sizeof( readbuf ) );
				if ( bytesReceived > 0 )
				{
					// Fixup actual size
					replybuf.StartReading( readbuf, bytesReceived );

					// Parse out data
					byte responseType = (byte)replybuf.ReadByte();
					if ( M2C_ACKPHONEHOME == responseType  )
					{
						bool allowPlay = replybuf.ReadByte() == 1 ? true : false;
						if ( allowPlay )
						{
							id = replybuf.ReadLong();
							return true;
						}
					}
					break;
				}

			}
		}

		return false;
	}

	// FIXME, this is BS
	bool IsExternalBuild()
	{
		if ( CommandLine()->FindParm( "-publicbuild" ) )
		{
			return true;
		}

		if ( !CommandLine()->FindParm( "-internalbuild" ) && 
			 !CommandLine()->CheckParm("-dev") )
		{
			return true;
		}

		// It's an external build...
		if ( m_bPhoneHome )
		{
			if ( !Q_stricmp( m_szBuildIdentifier, "beta-playtest" ) )
			{
				// Still internal
				return false;
			}
			return true;
		}
		return false;
	}


	bool		m_bPhoneHome;
	netadr_t	m_cserIP;
	char		m_szBuildIdentifier[ 32 ];
	bool		m_bLevelStarted;
	unsigned int m_uSessionID;

	CBlockingUDPSocket *m_pSocket;
};

CPhoneHome g_PhoneHome;

IPhoneHome *phonehome = &g_PhoneHome;
