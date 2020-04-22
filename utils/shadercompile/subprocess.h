//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef SUBPROCESS_H
#define SUBPROCESS_H
#ifdef _WIN32
#pragma once
#endif

class SubProcessKernelObjects
{
	friend class SubProcessKernelObjects_Memory;

public:
	SubProcessKernelObjects( void );
	~SubProcessKernelObjects( void );

private:
	SubProcessKernelObjects( SubProcessKernelObjects const & );
	SubProcessKernelObjects & operator =( SubProcessKernelObjects const & );

protected:
	BOOL Create( char const *szBaseName );
	BOOL Open( char const *szBaseName );

public:
	BOOL IsValid( void ) const;
	void Close( void );

protected:
	HANDLE m_hMemorySection;
	HANDLE m_hMutex;
	HANDLE m_hEvent[2];
	DWORD m_dwCookie;
};

class SubProcessKernelObjects_Create : public SubProcessKernelObjects
{
public:
	SubProcessKernelObjects_Create( char const *szBaseName ) { Create( szBaseName ), m_dwCookie = 1; }
};

class SubProcessKernelObjects_Open : public SubProcessKernelObjects
{
public:
	SubProcessKernelObjects_Open( char const *szBaseName ) { Open( szBaseName ), m_dwCookie = 0; }
};

class SubProcessKernelObjects_Memory
{
public:
	SubProcessKernelObjects_Memory( SubProcessKernelObjects *p ) : m_pObjs( p ), m_pLockData( NULL ), m_pMemory( NULL ) {  }
	~SubProcessKernelObjects_Memory() { Unlock(); }

public:
	void * Lock( void );
	BOOL Unlock( void );

public:
	BOOL IsValid( void ) const { return m_pLockData != NULL; }
	void * GetMemory( void ) const { return m_pMemory; }

protected:
	void *m_pMemory;

private:
	SubProcessKernelObjects *m_pObjs;
	void *m_pLockData;
};


//
// Response implementation
//
class CSubProcessResponse : public CmdSink::IResponse
{
public:
	explicit CSubProcessResponse( void const *pvMemory );
	~CSubProcessResponse( void ) { }

public:
	virtual bool Succeeded( void ) { return ( 1 == m_dwResult ); }
	virtual size_t GetResultBufferLen( void ) { return ( Succeeded() ? m_dwResultBufferLength : 0 ); }
	virtual const void * GetResultBuffer( void ) { return ( Succeeded() ? m_pvResultBuffer : NULL ); }
	virtual const char * GetListing( void ) { return (const char *) ( ( m_szListing && * m_szListing ) ? m_szListing : NULL ); }

protected:
	void const *m_pvMemory;
	DWORD m_dwResult;
	DWORD m_dwResultBufferLength;
	void const *m_pvResultBuffer;
	char const *m_szListing;
};


int ShaderCompile_Subprocess_Main( char const *szSubProcessData );


#endif // #ifndef SUBPROCESS_H
