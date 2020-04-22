//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================
//#include "misc.h"
//#include "stdafx.h"

#include <windows.h>

////// MySQL API includes
#include <WinSock.H> 
#include "mysql.h"
#include "errmsg.h"


#include "platform.h"
#include "isqlwrapper.h"
#include "sqlhelpers.h"
#include "interface.h"
#include "utllinkedlist.h"
#include "utlvector.h"

#include "tier0/memdbgon.h"


///////

//-----------------------------------------------------------------------------
// Purpose: Main dll entry point
// Input:	hModule -		our module handle
//			dwReason -		reason we were called
//			lpReserved -	bad idea that some Windows developer had some day that
//							we're stuck with
//-----------------------------------------------------------------------------
#ifdef _WIN32
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  dwReason, 
                       LPVOID lpReserved
					 )
{
	switch ( dwReason )
	{
		case DLL_PROCESS_ATTACH:
			break;
	}

    return TRUE;
}
#elif _LINUX
void __attribute__ ((constructor)) app_init(void);
void app_init(void)
{
}
#endif


class CSQLWrapper : public ISQLWrapper, public ISQLHelper
{
public:
	CSQLWrapper();
	~CSQLWrapper ();

	// ISQLWrapper
	virtual void Init( const char *pchDB, const char *pchHost, const char *pchUsername, const char *pchPassword );
	virtual bool BInsert( const char *pchQueryString );
	virtual const ISQLTableSet *PSQLTableSetDescription();
	virtual IResultSet *PResultSetQuery( const char *pchQueryString );
	virtual void FreeResult();

	// ISQLHelper
	virtual bool BInternalQuery( const char *pchQueryString, MYSQL_RES **ppMySQLRes, bool bRecurse /* = true */ );

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, char *pchName );		// Validate our internal structures
#endif

private:
	bool BConnect();
	void Disconnect();
	bool _Query( const char *pchQueryString, MYSQL_RES **result );

	char *m_pchDB;
	char *m_pchHost;
	char *m_pchUsername;
	char *m_pchPassword;
	bool m_bConnected;

	MYSQL m_MySQL;
	CSQLTableSet m_SQLTableSet;
	CResultSet  m_ResultSet;
	bool m_bInQuery;
};


class CSQLWrapperFactory : public ISQLWrapperFactory
{
public:
	CSQLWrapperFactory() {};
	~CSQLWrapperFactory() {};

	virtual ISQLWrapper *Create( const char *pchDB, const char *pchHost, const char *pchUsername, const char *pchPassword );
	virtual void Free( ISQLWrapper *pWrapper );

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, char *pchName );		// Validate our internal structures
#endif

private:
	CUtlFixedLinkedList<CSQLWrapper> m_ListSQLWrapper; // use a fixed in memory data struct so we can return pointers to the interfaces
};

CSQLWrapperFactory g_SQLWrapperFactory;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CSQLWrapperFactory, ISQLWrapperFactory, INTERFACEVERSION_ISQLWRAPPER, g_SQLWrapperFactory );

#if 0
//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//-----------------------------------------------------------------------------
class CDLLValidate : public IValidate
{
public:
	virtual void Validate( CValidator & validator )
	{
#ifdef DBGFLAG_VALIDATE
		g_SQLWrapperFactory.Validate( validator, "g_SQLWrapperFactory" );
#endif
	}
};
CDLLValidate g_DLLValidate;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CDLLValidate, IValidate, INTERFACEVERSION_IVALIDATE, g_DLLValidate );
#endif

//-----------------------------------------------------------------------------
// Purpose: Create a SQLWrapper interface to use
// Input:	pchDB -		database name to connect to
//			pchHost -		host to connect to
//			pchUsername -		username to connect as
//			pchPassword -		password to use
// Output:	a pointer to a sql wrapper interface
//-----------------------------------------------------------------------------
ISQLWrapper *CSQLWrapperFactory::Create( const char *pchDB, const char *pchHost, const char *pchUsername, const char *pchPassword )
{
	int iSQLWrapper = m_ListSQLWrapper.AddToTail();
	CSQLWrapper &sqlWrapper = m_ListSQLWrapper[iSQLWrapper];
	sqlWrapper.Init( pchDB, pchHost, pchUsername, pchPassword );
	return &sqlWrapper;
}


//-----------------------------------------------------------------------------
// Purpose: Free a previously allocated sql interface
// Input:	pWrapper - interface that was alloced		
//-----------------------------------------------------------------------------
void CSQLWrapperFactory::Free( ISQLWrapper *pSQLWrapper )
{
	FOR_EACH_LL( m_ListSQLWrapper, iSQLWrapper )
	{
		if ( &m_ListSQLWrapper[iSQLWrapper] == ((CSQLWrapper *)pSQLWrapper) )
		{
			m_ListSQLWrapper.Remove(iSQLWrapper);
			break;
		}
	}

	Assert( iSQLWrapper != m_ListSQLWrapper.InvalidIndex() );
}


//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CSQLWrapperFactory::Validate( CValidator &validator, char *pchName )
{
	validator.Push( "CSQLWrapperFactory", this, pchName );

	m_ListSQLWrapper.Validate( validator, "m_ListSQLWrapper" );

	FOR_EACH_LL( m_ListSQLWrapper, iListSQLWrapper )
	{
		m_ListSQLWrapper[ iListSQLWrapper ].Validate( validator, "m_ListSQLWrapper[ iListSQLWrapper ]" );
	}

	validator.Pop();
}
#endif

#define SAFE_DELETE( pointer )	if ( pointer != NULL ) { delete pointer; }

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CSQLWrapper::CSQLWrapper()
{
	m_pchDB = NULL;
	m_pchHost  = NULL;
	m_pchUsername = NULL;
	m_pchPassword = NULL;
	m_bConnected = false;
	m_bInQuery = false;

	mysql_init( &m_MySQL );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CSQLWrapper::~CSQLWrapper()
{
	FreeResult();
	mysql_close( &m_MySQL );
	m_bConnected = false;

	SAFE_DELETE(m_pchDB);
	SAFE_DELETE(m_pchHost);
	SAFE_DELETE(m_pchUsername);
	SAFE_DELETE(m_pchPassword);
}

// helper macro to alloc and copy a string to a member var
// BUGBUG Alfred: Make this a Q_function
#define COPY_STRING( dst, src ) \
	dst = new char[Q_strlen(src) + 1]; \
	Assert( dst ); \
	Q_strncpy( dst, src, Q_strlen(src) + 1); \
	dst[ Q_strlen(src) ] = 0;


//-----------------------------------------------------------------------------
// Purpose: Initializer.  Sets up connection params but DOESN'T actually do the connection
// Input:	pchDB -		database name to connect to
//			pchHost -		host to connect to
//			pchUsername -		username to connect as
//			pchPassword -		password to use
//-----------------------------------------------------------------------------
void CSQLWrapper::Init( const char *pchDB, const char *pchHost, const char *pchUsername, const char *pchPassword )
{
	COPY_STRING( m_pchDB, pchDB );
	COPY_STRING( m_pchHost, pchHost );
	COPY_STRING( m_pchUsername, pchUsername );
	COPY_STRING( m_pchPassword, pchPassword );
}


//-----------------------------------------------------------------------------
// Purpose: Connects to a MySQL server.
// Output:	true if connect works, false otherwise
//-----------------------------------------------------------------------------
bool CSQLWrapper::BConnect()
{
	m_bInQuery = false;

	MYSQL *pMYSQL = mysql_real_connect( &m_MySQL, m_pchHost, m_pchUsername, m_pchPassword, m_pchDB, 0, NULL, 0);
	if ( !pMYSQL || pMYSQL != &m_MySQL ) // on success we get our SQL pointer back
	{
		DevMsg( "Failed to connect to DB server (%s)\n", mysql_error(&m_MySQL) );
		return false;
	}
	m_bConnected = true;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Disconnects from a mysql if you are already connected
//-----------------------------------------------------------------------------
void CSQLWrapper::Disconnect()
{
	mysql_close( &m_MySQL );
	mysql_init( &m_MySQL );
	m_bConnected = false;
}


//-----------------------------------------------------------------------------
// Purpose: run a query on the db with retries
// Input:	pchQueryString -		query string to run
//			ppMySQLRes	-			mysql result set to set
//			bRecurse -				if true allow this function to call itself again (to rety the query)
// Output:	true if query succeeds, false otherwise
//-----------------------------------------------------------------------------
bool CSQLWrapper::BInternalQuery( const char *pchQueryString, MYSQL_RES **ppMySQLRes, bool bRecurse )
{
	*ppMySQLRes = NULL;
	if ( !m_pchDB || !m_pchHost || !m_pchUsername || !m_pchPassword || !ppMySQLRes )
	{
		return false;
	}

	if ( !m_bConnected )
	{
		BConnect(); // need to reconnect
	}

	bool bRet = _Query( pchQueryString, ppMySQLRes );
	if ( !bRet && !m_bConnected && bRecurse ) // hmmm, got hung up when running the query
	{
		bRet = BInternalQuery( pchQueryString, ppMySQLRes, false ); // run the query again now we reconnected	
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Actually runs a query on the server
// Input:	pchQueryString -		query string to run
//			result	-				mysql result set to set
// Output:	true if query succeeds, false otherwise, result it set on success
//-----------------------------------------------------------------------------
bool CSQLWrapper::_Query( const char *pchQueryString, MYSQL_RES **ppMySQLRes )
{
	*ppMySQLRes = NULL;
	int iResult = mysql_real_query( &m_MySQL, pchQueryString, Q_strlen(pchQueryString) );
	if ( iResult != 0 )
	{
		int iErrNo = mysql_errno(&m_MySQL);
		if ( iErrNo == CR_COMMANDS_OUT_OF_SYNC ) // I hate this return code, you just need to hang up and try again
		{
			Disconnect();
			return false;
		}
		else if ( iErrNo == CR_SERVER_LOST || iErrNo == CR_SERVER_GONE_ERROR )
		{
			m_bConnected = false;
			return false;
		}
		else if ( iErrNo != 0 )
		{
			DevMsg( "CSQLWrapper::_Query Generic SQL query error: %s\n", mysql_error( &m_MySQL ) );
			return false;
		}
	}

	if ( mysql_field_count( &m_MySQL ) > 0 ) // if there are results clear them from the connection
	{
		*ppMySQLRes = mysql_store_result( &m_MySQL );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Runs a insert style query on the db (i.e no return set), opens the connection if it isn't currently
// Input:	pchQueryString -		query string to run
// Output:	true if query succeeds, false otherwise
//-----------------------------------------------------------------------------
bool CSQLWrapper::BInsert( const char *pchQueryString )
{
	if ( m_bInQuery )
	{
		Assert( !m_bInQuery );
		return false;
	}

	m_bInQuery = true;
	MYSQL_RES *pMySQLRes = NULL;
	bool bRet = BInternalQuery( pchQueryString, &pMySQLRes, true ); 
	if ( bRet && pMySQLRes ) 
	{
		mysql_free_result( pMySQLRes );
	}
	m_bInQuery = false;
	return bRet;
}
	

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to the table descriptions for this DB
// Output:	table description pointer
//-----------------------------------------------------------------------------
const ISQLTableSet *CSQLWrapper::PSQLTableSetDescription()
{
	if ( m_bInQuery )
	{
		Assert( !m_bInQuery );
		return NULL;
	}

	m_bInQuery = true;
	if ( !m_SQLTableSet.Init( this ) ) 
	{
		return NULL;
	}

	m_bInQuery = false;
	return &m_SQLTableSet;
}


//-----------------------------------------------------------------------------
// Purpose: Runs a select style query on the db (i.e a return set), opens the connection if it isn't currently
// Input:	pchQueryString -		query string to run
// Output:	returns a pointer to the result set (NULL on failure)
//-----------------------------------------------------------------------------
IResultSet *CSQLWrapper::PResultSetQuery( const char *pchQueryString )
{
	if ( m_bInQuery )
	{
		Assert( !m_bInQuery );
		return NULL;
	}

	bool bRet = m_ResultSet.Query( pchQueryString, this );
	if ( !bRet )
	{
		return NULL;
	}
	m_bInQuery = true;
	return &m_ResultSet;
}


//-----------------------------------------------------------------------------
// Purpose: Free's any currently running result set
//-----------------------------------------------------------------------------
void CSQLWrapper::FreeResult()
{
	m_bInQuery = false;
	m_ResultSet.FreeResult();
}


//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CSQLWrapper::Validate( CValidator &validator, char *pchName )
{
	validator.Push( "CSQLWrapper", this, pchName );

	validator.ClaimMemory( m_pchDB );
	validator.ClaimMemory( m_pchHost );
	validator.ClaimMemory( m_pchUsername );
	validator.ClaimMemory( m_pchPassword );

	m_SQLTableSet.Validate( validator, "m_SQLTableSet" );
	m_ResultSet.Validate( validator, "m_ResultSet" );

	validator.Pop();
}
#endif
