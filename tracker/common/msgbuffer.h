//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#if !defined( MSGBUFFER_H )
#define MSGBUFFER_H
#ifdef _WIN32
#pragma once
#endif

#include "netadr.h"

//-----------------------------------------------------------------------------
// Purpose: Generic byte level message buffer with read/write support
//-----------------------------------------------------------------------------
class CMsgBuffer  
{
public:
	enum 
	{
		NET_MAXMESSAGE = 8192
	};

	// Buffers must be named
					CMsgBuffer( const char *buffername = "unnamed", void (*ef)( PRINTF_FORMAT_STRING const char *fmt, ... ) = 0 );
	virtual			~CMsgBuffer( void );

	// Reset the buffer for writing
	void			Clear( void );
	// Get current # of bytes 
	int				GetCurSize( void );
	// Get max # of bytes
	int				GetMaxSize( void );
	// Get pointer to raw data
	void			*GetData( void );
	// Set/unset the allow overflow flag
	void			SetOverflow( bool allowed );
	// Start reading from buffer
	void			BeginReading( void );
	// Get current read byte
	int				GetReadCount( void );

	// Push read count ( to peek at data )
	void			Push( void );
	void			Pop( void );

	// Writing functions
	void			WriteByte(int c);
	void			WriteShort(int c);
	void			WriteLong(int c);
	void			WriteFloat(float f);
	void			WriteString(const char *s);
	void			WriteBuf( int iSize, void *buf );

	// Reading functions
	int				ReadByte( void );
	int				ReadShort( void );
	int				ReadLong( void );
	float			ReadFloat( void );
	char			*ReadString( void );
	int				ReadBuf( int iSize, void *pbuf );

	// setting and storing time received
	void			SetTime(float time);
	float			GetTime();

	// net address received from
	void			SetNetAddress(netadr_t &adr);
	netadr_t		&GetNetAddress();

private:
	// Ensures sufficient space to append an object of length
	void			*GetSpace( int length );
	// Copy buffer in at current writing point
	void			Write( const void *data, int length );

private:
	// Name of buffer in case of debugging/errors
	const char		*m_pszBufferName;
	// Optional error callback
	void			( *m_pfnErrorFunc )( PRINTF_FORMAT_STRING const char *fmt, ... );

	// Current read pointer
	int				m_nReadCount;
	// Push/pop read state
	int				m_nPushedCount;
	bool			m_bPushed;
	// Did we hit the end of the read buffer?
	bool			m_bBadRead;
	// Max size of buffer
	int				m_nMaxSize;
	// Current bytes written
	int				m_nCurSize;
	// if false, call m_pfnErrorFunc
	bool			m_bAllowOverflow;
	// set to true when buffer hits end
	bool			m_bOverFlowed;
	// Internal storage
	unsigned char	m_rgData[ NET_MAXMESSAGE ];
	// time received
	float			m_fRecvTime;
	// address received from
	netadr_t		m_NetAddr;
};

#endif // !MSGBUFFER_H