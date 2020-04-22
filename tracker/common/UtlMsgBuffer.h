//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Generic named data buffer, declaration and implementation
//
//=============================================================================

#ifndef UTLMSGBUFFER_H
#define UTLMSGBUFFER_H
#ifdef _WIN32
#pragma once
#endif

#include "UtlMemory.h"

#pragma warning(disable: 4244) // warning C4244: '=' : conversion from 'int' to 'short', possible loss of data

//-----------------------------------------------------------------------------
// Purpose: Generic named data buffer
//-----------------------------------------------------------------------------
class CUtlMsgBuffer
{
public:
	CUtlMsgBuffer(unsigned short msgID, int initialSize);
	CUtlMsgBuffer(unsigned short msgID, void const *data, int dataSize);
	~CUtlMsgBuffer();

	unsigned short GetMsgID() const				{ return m_iMsgID; }
	void SetMsgID(unsigned short msgID)			{ m_iMsgID = msgID; }

	// read functions
	bool ReadInt(const char *name, int &data);
	bool ReadUInt(const char *name, unsigned int &data);
	bool ReadString(const char *name, char *data, int dataBufferSize);
	bool ReadBuffer(const char *name, CUtlMsgBuffer &buffer);
	// returns number of bytes read, 0 on failure
	int ReadBlob(const char *name, void *data, int dataBufferSize);	

	// reads out the next variable available in the buffer
	// fills out parameters with var details and data
	// returns false if no more vars available
	bool ReadNextVar(char name[32], bool &stringData, void *data, int &dataSize);

	// write functions
	void WriteInt(const char *name, int data);
	void WriteUInt(const char *name, unsigned int data);
	void WriteString(const char *name, const char *data);
	void WriteBlob(const char *name, const void *data, int dataSize);
	void WriteBuffer(const char *name, const CUtlMsgBuffer *buffer);

	// returns a pointer to the data buffer, and its size, of the specified variable
	void *FindVar(const char *name, int &dataSizeOut);

	// pads the buffer to the specified boundary (in bytes)
	void PadBuffer(int boundary);

    // makes sure the message has this much space allocated
    void EnsureCapacity(int dataSize);

	// returns the number of bytes used by the message
	int DataSize() const;

	// returns a pointer to the base data
	void *Base();

	// returns a const pointer to the base data
	const void *Base() const;

    // advances the write pointer - used when you write directly into the buffer
    void SetWritePos(int size);

    CUtlMsgBuffer& Copy(const CUtlMsgBuffer &rhs);

	// copy constructor
	CUtlMsgBuffer(const CUtlMsgBuffer &rhs)
	{
		m_iMsgID = rhs.m_iMsgID;
		m_iWritePos = rhs.m_iWritePos;
		m_iReadPos = rhs.m_iReadPos;
		m_iNextVarPos = rhs.m_iNextVarPos;

		m_Memory.EnsureCapacity(rhs.m_Memory.NumAllocated());
		if ( rhs.m_Memory.NumAllocated() > 0 )
		{
			memcpy(Base(), rhs.Base(), rhs.m_Memory.NumAllocated());
		}
	}

private:

	bool Read(void *buffer, int readAmount);
	bool ReadUntilNull(void *buffer, int bufferSize);
	void Write(void const *data, int size);

	CUtlMemory<char> m_Memory;

	unsigned short m_iMsgID;
	short m_iWritePos;		// position in buffer we are currently writing to
	short m_iReadPos;			// current reading position
	short m_iNextVarPos;		// a guess at which variable is most likely to be read next
};

//-----------------------------------------------------------------------------
// Purpose: returns the number of bytes used by the message
//-----------------------------------------------------------------------------
inline int CUtlMsgBuffer::DataSize() const
{
	// return the highest read/write mark
	if (m_iWritePos > m_iReadPos)
		return m_iWritePos;

	return m_iReadPos;
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to the base data
//-----------------------------------------------------------------------------
inline void *CUtlMsgBuffer::Base()
{
	return &m_Memory[0];
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to the base data
//-----------------------------------------------------------------------------
inline const void *CUtlMsgBuffer::Base() const
{
	return &m_Memory[0];
}

//-----------------------------------------------------------------------------
// Purpose: ensures capacity
//-----------------------------------------------------------------------------
inline void CUtlMsgBuffer::EnsureCapacity(int dataSize)
{
    m_Memory.EnsureCapacity(dataSize);
}

//-----------------------------------------------------------------------------
// Purpose: pads the buffer to the specified boundary (in bytes)
//-----------------------------------------------------------------------------
inline void CUtlMsgBuffer::PadBuffer(int boundary)
{
	// pad the buffer to be the right size for encryption
	int pad = (boundary - (DataSize() % boundary));	
	Write("\0\0\0\0\0\0\0\0\0\0\0\0", pad);
}

//-----------------------------------------------------------------------------
// Purpose: Reads in a named 4-byte int, returns true on sucess, false on failure
//-----------------------------------------------------------------------------
inline bool CUtlMsgBuffer::ReadInt(const char *name, int &data)
{
	return (ReadBlob(name, &data, 4) == 4);
}

//-----------------------------------------------------------------------------
// Purpose: Reads in a named 4-byte unsigned int, returns true on sucess, false on failure
//-----------------------------------------------------------------------------
inline bool CUtlMsgBuffer::ReadUInt(const char *name, unsigned int &data)
{
	return (ReadBlob(name, &data, 4) == 4);
}

//-----------------------------------------------------------------------------
// Purpose: Reads in a named variable length character string
//			returns true on sucess, false on failure
//-----------------------------------------------------------------------------
inline bool CUtlMsgBuffer::ReadString(const char *name, char *data, int dataBufferSize)
{
	return (ReadBlob(name, data, dataBufferSize) > 0);
}

//-----------------------------------------------------------------------------
// Purpose: Writes out an integer to the message
//-----------------------------------------------------------------------------
inline void CUtlMsgBuffer::WriteInt(const char *name, int data)
{
	WriteBlob(name, &data, 4);
}

//-----------------------------------------------------------------------------
// Purpose: Writes out an unsigned integer to the message
//-----------------------------------------------------------------------------
inline void CUtlMsgBuffer::WriteUInt(const char *name, unsigned int data)
{
	WriteBlob(name, &data, 4);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline void CUtlMsgBuffer::SetWritePos(int size)
{
    m_iWritePos = size;
}


#endif // UTLMSGBUFFER_H
