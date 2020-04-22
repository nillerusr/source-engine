//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HARDWAREVERTEXCACHE_H
#define HARDWAREVERTEXCACHE_H
#ifdef _WIN32
#pragma once
#endif

// emulate a hardware post T&L vertex fifo

class CHardwareVertexCache
{
public:
	CHardwareVertexCache();
	void Init( int size );
	void Insert( int index );
	bool IsPresent( int index );
	void Flush( void );
	void Print( void );
private:
	int m_Size;
	int *m_Fifo;
	int m_HeadIndex;
	int m_NumEntries;
};

#endif // HARDWAREVERTEXCACHE_H
