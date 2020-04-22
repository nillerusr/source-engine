//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Command sink interface implementation.
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef CMDSINK_H
#define CMDSINK_H
#ifdef _WIN32
#pragma once
#endif

#include <stdio.h>
#include <tier1/utlbuffer.h>


namespace CmdSink
{

/*

struct IResponse

Interface to give back command execution results.

*/
struct IResponse
{
	virtual ~IResponse( void ) {}
	virtual void Release( void ) { delete this; }

	// Returns whether the command succeeded
	virtual bool Succeeded( void ) = 0;

	// If the command succeeded returns the result buffer length, otherwise zero
	virtual size_t GetResultBufferLen( void ) = 0;
	// If the command succeeded returns the result buffer base pointer, otherwise NULL
	virtual const void * GetResultBuffer( void ) = 0;

	// Returns a zero-terminated string of messages reported during command execution, or NULL if nothing was reported
	virtual const char * GetListing( void ) = 0;
};




/*

Response implementation when the result should appear in
one file and the listing should appear in another file.

*/
class CResponseFiles : public IResponse
{
public:
	explicit CResponseFiles( char const *szFileResult, char const *szFileListing );
	~CResponseFiles( void );

public:
	// Returns whether the command succeeded
	virtual bool Succeeded( void );

	// If the command succeeded returns the result buffer length, otherwise zero
	virtual size_t GetResultBufferLen( void );
	// If the command succeeded returns the result buffer base pointer, otherwise NULL
	virtual const void * GetResultBuffer( void );

	// Returns a zero-terminated string of messages reported during command execution
	virtual const char * GetListing( void );

protected:
	void OpenResultFile( void );		//!< Opens the result file if not open yet
	void ReadResultFile( void );		//!< Reads the result buffer if not read yet
	void ReadListingFile( void );		//!< Reads the listing buffer if not read yet

protected:
	char m_szFileResult[MAX_PATH];		//!< Name of the result file
	char m_szFileListing[MAX_PATH];		//!< Name of the listing file

	FILE *m_fResult;					//!< Result file (NULL if not open)
	FILE *m_fListing;					//!< Listing file (NULL if not open)

	CUtlBuffer m_bufResult;				//!< Buffer holding the result data
	size_t m_lenResult;					//!< Result data length (0 if result not read yet)
	const void *m_dataResult;			//!< Data buffer pointer (NULL if result not read yet)

	CUtlBuffer m_bufListing;			//!< Buffer holding the listing
	const char *m_dataListing;			//!< Listing buffer pointer (NULL if listing not read yet)
};

/*

Response implementation when the result is a generic error.

*/
class CResponseError : public IResponse
{
public:
	explicit CResponseError( void ) {}
	~CResponseError( void ) {}

public:
	virtual bool Succeeded( void ) { return false; }

	virtual size_t GetResultBufferLen( void ) { return 0; }
	virtual const void * GetResultBuffer( void ) { return NULL; }

	virtual const char * GetListing( void ) { return NULL; }
};


}; // namespace CmdSink


#endif // #ifndef CMDSINK_H
