//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CLIENTNETMESSAGE_H
#define CLIENTNETMESSAGE_H
#ifdef _WIN32
#pragma once
#pragma warning(disable : 4100)	// unreferenced formal parameter
#pragma warning(disable : 4514) // unreferenced inline function has been removed
#endif

#include "inetmessage.h"
#include "protocol.h"
#include "checksum_crc.h"
#include "tier0/dbg.h"

class bf_read;
class bf_write;

#define DECLARE_CLIENTNETMESSAGE( msgtype )					\
	public:													\
		int				ReadFromBuffer( bf_read * buffer );	\
		int				WriteToBuffer( bf_write * buffer );	\
		void			Clear();							\
		const char		*ToString();						\
		static int		GetType() { return msgtype; };		\
		static const char *GetName() { return #msgtype; };

class CNetMessage : public INetMessage
{
public:
	CNetMessage() {  m_bReliable = 0; m_bOwnData = false; };
	virtual ~CNetMessage() {};
	
	void	SetReliable( bool state = true) {m_bReliable = state;};
	bool	IsReliable() { return m_bReliable; };
	bool	IsConnectionless() { return false;};
		
	virtual const char *ToString() { return "Unknown CNetMessage"; };

public:

	bool	m_bReliable;	// true if message should be send reliable
	bool	m_bOwnData;		// true if message object uses dynamic allocated memory
};

class CLC_SendFile : public CNetMessage
{
		DECLARE_CLIENTNETMESSAGE( clc_sendfile );

		CLC_SendFile(CRC32_t fileCRC)
		{
			m_bReliable = true;
			m_bOwnData = false;
			m_FileCRC = fileCRC;
		}

public:
	CRC32_t		m_FileCRC;		// CRC of file to send
};

class CLC_Move : public CNetMessage
{
		DECLARE_CLIENTNETMESSAGE( clc_move );

		CLC_Move( int numBackup, int numNew, int length, unsigned char * data)
		{
			m_bReliable = true;
			m_bOwnData = false;
			m_nNumBackupCommands = numBackup;
			m_nNumNewCommands = numNew;
			m_nLength = length; // in bits
			m_Data = data;
		}

public:
	int				m_nNumBackupCommands;
	int				m_nNumNewCommands;
	int				m_nLength;
	unsigned char	*m_Data;
};



class CLC_StringCmd : public CNetMessage
{
		DECLARE_CLIENTNETMESSAGE( clc_stringcmd );

		CLC_StringCmd(const char *command)
		{
			Assert( command );
			m_szClientCommand = (char*)command;
			m_bReliable = true;
			m_bOwnData = false;
		};

public:

	char	*m_szClientCommand;

};

class CLC_Delta : public CNetMessage
{
		DECLARE_CLIENTNETMESSAGE( clc_delta );

		CLC_Delta( int deltaSequeenceNr );

public:
	int	m_nSequenceNumber;
};

class CLC_VoiceData : public CNetMessage
{
		DECLARE_CLIENTNETMESSAGE( clc_voicedata );

		CLC_VoiceData( unsigned char *data, int length )
		{
			Assert( data );
			m_Data = data; 
			m_nLength = length;
			m_bReliable = false;
			m_bOwnData = false;
		};

public:

	int				m_nLength;
	unsigned char	*m_Data;
};




#endif // CLIENTNETMESSAGE_H
