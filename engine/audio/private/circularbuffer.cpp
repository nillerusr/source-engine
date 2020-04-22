//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Circular Buffer
//
//=============================================================================//

#include "tier0/dbg.h"
#include "circularbuffer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CCircularBuffer::CCircularBuffer()
{
	SetSize( 0 );
}

CCircularBuffer::CCircularBuffer(int size)
{
	SetSize(size);
}

//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
//Purpose : Sets the maximum size for a circular buffer. This does not do any
//			memory allocation, it simply informs the buffer of its size.
//Author  : DSpeyrer
//------------------------------------------------------------------------------
void CCircularBuffer::SetSize(int size)
{
	Assert( this );

	m_nSize = size;
	m_nRead = 0;
	m_nWrite = 0;
	m_nCount = 0;
}


//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
//Purpose : Empties a circular buffer.
//Author  : DSpeyrer
//------------------------------------------------------------------------------
void CCircularBuffer::Flush()
{
	AssertValid();

	m_nRead = 0;
	m_nWrite = 0;
	m_nCount = 0;
}


//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
//Purpose : Returns the available space in a circular buffer.
//Author  : DSpeyrer
//------------------------------------------------------------------------------
int CCircularBuffer::GetWriteAvailable()
{
	AssertValid();

	return(m_nSize - m_nCount);
}


//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
//Purpose : Returns the size of a circular buffer.
//Author  : DSpeyrer
//------------------------------------------------------------------------------
int CCircularBuffer::GetSize()
{
	AssertValid();

	return(m_nSize);
}


//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
//Purpose : Returns the number of bytes in a circular buffer.
//Author  : DSpeyrer
//------------------------------------------------------------------------------
int CCircularBuffer::GetReadAvailable()
{
	AssertValid();

	return(m_nCount);
}


//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
//Purpose : Reads a specified number of bytes from a circular buffer without
//			consuming them. They will still be available for future calls to
//			Read or Peek.
//Input   : pchDest - destination buffer.
//			m_nCount - number of bytes to place in destination buffer.
//Output  : Returns the number of bytes placed in the destination buffer.
//Author  : DSpeyrer
//------------------------------------------------------------------------------
int CCircularBuffer::Peek(char *pchDest, int nCount)
{
	// If no data available, just return.
	if(m_nCount == 0)
	{
		return(0);
	}
	
	//
	// Requested amount should not exceed the available amount.
	//
	nCount = MIN(m_nCount, nCount);

	//
	// Copy as many of the requested bytes as possible.
	// If buffer wrap occurs split the data into two chunks.
	//
	if (m_nRead + nCount > m_nSize)
	{
		int nCount1 = m_nSize - m_nRead;
		memcpy(pchDest, &m_chData[m_nRead], nCount1);
		pchDest += nCount1;

		int nCount2 = nCount - nCount1;
		memcpy(pchDest, m_chData, nCount2);
	}
	// Otherwise copy it in one go.
	else
	{
		memcpy(pchDest, &m_chData[m_nRead], nCount);
	}
	
	AssertValid();
	return nCount;
}


//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
//Purpose : Advances the read index, consuming a specified number of bytes from
//			the circular buffer.
//Input   : m_nCount - number of bytes to consume.
//Output  : Returns the actual number of bytes consumed.
//Author  : DSpeyrer
//------------------------------------------------------------------------------
int CCircularBuffer::Advance(int nCount)
{
	// If no data available, just return.
	if (m_nCount == 0)
	{
		return(0);
	}
	
	//
	// Requested amount should not exceed the available amount.
	//
	nCount = MIN(m_nCount, nCount);

	// Advance the read pointer, checking for buffer 
	//wrap.
	//
	m_nRead = (m_nRead + nCount) % m_nSize;
	m_nCount -= nCount;

	//
	// If we have emptied the buffer, reset the read and write indices
	// to minimize buffer wrap.
	//
	if (m_nCount == 0)
	{
		m_nRead = 0;
		m_nWrite = 0;
	}

	AssertValid();
	return nCount;
}


//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
//Purpose : Reads a specified number of bytes from a circular buffer. The bytes
//			will be consumed by the read process.
//Input   : pchDest - destination buffer.
//			m_nCount - number of bytes to place in destination buffer.
//Output  : Returns the number of bytes placed in the destination buffer.
//Author  : DSpeyrer
//------------------------------------------------------------------------------
int CCircularBuffer::Read(void *pchDestIn, int nCount)
{
	int nPeeked;
	int nRead;

	char *pchDest = (char*)pchDestIn;

	nPeeked = Peek(pchDest, nCount);

	if (nPeeked != 0)
	{
		nRead = Advance(nPeeked);

		assert( nRead == nPeeked);
	}
	else
	{
		nRead = 0;
	}

	AssertValid();
	return(nRead);
}


//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
//Purpose : Writes a specified number of bytes to the buffer.
//Input   : pm_chData - buffer containing bytes to bw written.
//			m_nCount - the number of bytes to write.
//Output  : Returns the number of bytes written. If there wa insufficient space
//			to write all requested bytes, the value returned will be less than
//			the requested amount.
//Author  : DSpeyrer
//------------------------------------------------------------------------------
int CCircularBuffer::Write(void *pData, int nBytesRequested)
{
	// Write all the data.
	int nBytesToWrite = nBytesRequested;
	char *pDataToWrite = (char*)pData;
	
	while(nBytesToWrite)
	{
		int from = m_nWrite;
		int to = m_nWrite + nBytesToWrite;
		
		if(to >= m_nSize)
		{
			to = m_nSize;
		}

		memcpy(&m_chData[from], pDataToWrite, to - from);
		pDataToWrite += to - from;

		m_nWrite = to % m_nSize;
		nBytesToWrite -= to - from;
	}

	// Did it cross the read pointer? Then slide the read pointer up.
	// This way, we will discard the old data.
	if(nBytesRequested > (m_nSize - m_nCount))
	{
		m_nCount = m_nSize;
		m_nRead = m_nWrite;
	}
	else
	{
		m_nCount += nBytesRequested;
	}

	AssertValid();
	return nBytesRequested;
}

CCircularBuffer *AllocateCircularBuffer( int nSize )
{
	char *pBuff = (char *)malloc( sizeof( CCircularBuffer ) + nSize - 1 );

	CCircularBuffer *pCCircularBuffer = (CCircularBuffer *)pBuff;

	pCCircularBuffer->SetSize( nSize );
	return pCCircularBuffer;
}

void FreeCircularBuffer( CCircularBuffer *pCircularBuffer )
{
	free( (char*)pCircularBuffer );
}

