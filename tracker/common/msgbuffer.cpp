//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include <string.h>
#include <assert.h>
#include "msgbuffer.h"
// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#pragma warning(disable: 4244) // warning C4244: '=' : conversion from 'int' to 'unsigned char', possible loss of data

//-----------------------------------------------------------------------------
// Purpose: Allocate message buffer
// Input  : *buffername - 
//			*ef - 
//-----------------------------------------------------------------------------
CMsgBuffer::CMsgBuffer( const char *buffername, void (*ef)( const char *fmt, ... ) /*= NULL*/ )
{
	m_pszBufferName		= buffername;
	m_pfnErrorFunc		= ef;
	m_bAllowOverflow	= false;	// if false, Error
	m_bOverFlowed		= false;	// set to true if the buffer size failed
	m_nMaxSize			= NET_MAXMESSAGE;
	m_nPushedCount		= 0;
	m_bPushed			= false;
	m_nReadCount		= 0;
	m_bBadRead			= false;

	Clear();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMsgBuffer::~CMsgBuffer( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Temporarily remember the read position so we can reset it
//-----------------------------------------------------------------------------
void CMsgBuffer::Push( void )
{
	// ??? Allow multiple pushes without matching pops ???
	assert( !m_bPushed );

	m_nPushedCount	= m_nReadCount;
	m_bPushed		= true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMsgBuffer::Pop( void )
{
	assert( m_bPushed );

	m_nReadCount	= m_nPushedCount;
	m_bPushed		= false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : allowed - 
//-----------------------------------------------------------------------------
void CMsgBuffer::SetOverflow( bool allowed )
{
	m_bAllowOverflow = allowed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CMsgBuffer::GetMaxSize( void )
{
	return m_nMaxSize;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void *
//-----------------------------------------------------------------------------
void * CMsgBuffer::GetData( void )
{
	return m_rgData;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CMsgBuffer::GetCurSize( void )
{
	return m_nCurSize;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CMsgBuffer::GetReadCount( void )
{
	return m_nReadCount;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CMsgBuffer::SetTime(float time)
{
	m_fRecvTime = time;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
float CMsgBuffer::GetTime()
{
	return m_fRecvTime;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CMsgBuffer::SetNetAddress(netadr_t &adr)
{
	m_NetAddr = adr;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
netadr_t &CMsgBuffer::GetNetAddress()
{
	return m_NetAddr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMsgBuffer::WriteByte( int c )
{
	unsigned char    *buf;
	buf = (unsigned char *)GetSpace( 1 );
	buf[0] = c;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : c - 
//-----------------------------------------------------------------------------
void CMsgBuffer::WriteShort ( int c )
{
	unsigned char    *buf;
	buf = (unsigned char *)GetSpace( 2 );
	buf[0] = c&0xff;
	buf[1] = c>>8;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : c - 
//-----------------------------------------------------------------------------
void CMsgBuffer::WriteLong (int c)
{
	unsigned char    *buf;
	
	buf = (unsigned char *)GetSpace( 4 );
	buf[0] = c&0xff;
	buf[1] = (c>>8)&0xff;
	buf[2] = (c>>16)&0xff;
	buf[3] = c>>24;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : f - 
//-----------------------------------------------------------------------------
void CMsgBuffer::WriteFloat (float f)
{
	union
	{
		float   f;
		int     l;
	} dat;
	
	dat.f = f;
	Write( &dat.l, 4 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *s - 
//-----------------------------------------------------------------------------
void CMsgBuffer::WriteString (const char *s)
{
	if ( !s )
	{
		Write ("", 1);
	}
	else
	{
		Write ( s, strlen( s ) + 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iSize - 
//			*buf - 
//-----------------------------------------------------------------------------
void CMsgBuffer::WriteBuf( int iSize, void *buf )
{
	if ( !buf )
	{
		return;
	}

	Write( buf, iSize );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMsgBuffer::BeginReading (void)
{
	m_nReadCount		= 0;
	m_bBadRead		= false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int CMsgBuffer::ReadByte
//-----------------------------------------------------------------------------
int CMsgBuffer::ReadByte (void)
{
	int     c;
	
	if ( m_nReadCount + 1 > m_nCurSize )
	{
		m_bBadRead = true;
		return -1;
	}
		
	c = ( unsigned char )m_rgData[ m_nReadCount ];
	m_nReadCount++;
	return c;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int CMsgBuffer::ReadShort
//-----------------------------------------------------------------------------
int CMsgBuffer::ReadShort (void)
{
	int     c;
	
	if ( m_nReadCount + 2 > m_nCurSize )
	{
		m_bBadRead = true;
		return -1;
	}
		
	c = (short)(m_rgData[m_nReadCount] + (m_rgData[m_nReadCount+1]<<8));
	m_nReadCount += 2;
	
	return c;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int CMsgBuffer::ReadLong
//-----------------------------------------------------------------------------
int CMsgBuffer::ReadLong (void)
{
	int     c;
	
	if (m_nReadCount+4 > m_nCurSize)
	{
		m_bBadRead = true;
		return -1;
	}
		
	c = m_rgData[m_nReadCount]
	+ (m_rgData[m_nReadCount+1]<<8)
	+ (m_rgData[m_nReadCount+2]<<16)
	+ (m_rgData[m_nReadCount+3]<<24);
	
	m_nReadCount += 4;
	
	return c;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float CMsgBuffer::ReadFloat
//-----------------------------------------------------------------------------
float CMsgBuffer::ReadFloat (void)
{
	union
	{
		unsigned char    b[4];
		float   f;
	} dat;
	
	dat.b[0] =      m_rgData[m_nReadCount];
	dat.b[1] =      m_rgData[m_nReadCount+1];
	dat.b[2] =      m_rgData[m_nReadCount+2];
	dat.b[3] =      m_rgData[m_nReadCount+3];
	m_nReadCount += 4;
	return dat.f;   
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iSize - 
//			*pbuf - 
// Output : int
//-----------------------------------------------------------------------------
int CMsgBuffer::ReadBuf( int iSize, void *pbuf )
{
	if (m_nReadCount + iSize > m_nCurSize)
	{
		m_bBadRead = true;
		return -1;
	}
		
	memcpy( pbuf, &m_rgData[m_nReadCount], iSize );
	m_nReadCount += iSize;
	
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char *
//-----------------------------------------------------------------------------
char *CMsgBuffer::ReadString (void)
{
	static char     string[ NET_MAXMESSAGE ];
	int             l,c;
	
	l = 0;
	do
	{
		c = (char)ReadByte();
		if ( c == -1 || c == 0 )
			break;
		string[l] = c;
		l++;
	} while ( l < sizeof(string)-1 );
	
	string[ l ] = 0;
	
	return string;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMsgBuffer::Clear( void )
{
	m_nCurSize		= 0;
	m_bOverFlowed	= false;
	m_nReadCount	= 0;
	m_bBadRead		= false;
	memset( m_rgData, 0, sizeof( m_rgData ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : length - 
//-----------------------------------------------------------------------------
void *CMsgBuffer::GetSpace( int length )
{
	void    *d;
	
	if (m_nCurSize + length > m_nMaxSize)
	{
		if ( !m_bAllowOverflow )
		{
			if ( m_pfnErrorFunc )
			{
				( *m_pfnErrorFunc )( "CMsgBuffer(%s), no room for %i bytes, %i / %i already in use\n",
					m_pszBufferName, length, m_nCurSize, m_nMaxSize );
			}
			return NULL;
		}
		
		if (length > m_nMaxSize)
		{
			if ( m_pfnErrorFunc )
			{
				( *m_pfnErrorFunc )( "CMsgBuffer(%s), no room for %i bytes, %i is max\n",
					m_pszBufferName, length, m_nMaxSize );
			}
			return NULL;
		}
			
		m_bOverFlowed = true;
		Clear(); 
	}

	d = m_rgData + m_nCurSize;
	m_nCurSize += length;
	return d;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *m_rgData - 
//			length - 
//-----------------------------------------------------------------------------
void CMsgBuffer::Write(const void *m_rgData, int length)
{
	memcpy( GetSpace(length), m_rgData, length );         
}