//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// mysql_wrapper.cpp : Defines the entry point for the DLL application.
//
#include "stdafx.h"

extern "C"
{
	#include "mysql.h"
};

#include "imysqlwrapper.h"
#include <stdio.h>


static char* CopyString( const char *pStr )
{
	if ( !pStr )
	{
		pStr = "";
	}

	char *pRet = new char[ strlen( pStr ) + 1 ];
	strcpy( pRet, pStr );
	return pRet;
}


BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    return TRUE;
}


class CCopiedRow
{
public:
	~CCopiedRow()
	{
		m_Columns.PurgeAndDeleteElements();
	}

	CUtlVector<char*> m_Columns;
};


class CMySQLCopiedRowSet : public IMySQLRowSet
{
public:
	~CMySQLCopiedRowSet()
	{
		m_Rows.PurgeAndDeleteElements();
		m_ColumnNames.PurgeAndDeleteElements();
	}

	virtual void			Release()
	{
		delete this;
	}

	virtual int				NumFields()
	{
		return m_ColumnNames.Count();
	}

	virtual const char*		GetFieldName( int iColumn )
	{
		return m_ColumnNames[iColumn];
	}

	virtual bool			NextRow()
	{
		++m_iCurRow;
		return m_iCurRow < m_Rows.Count();
	}

	virtual bool			SeekToFirstRow()
	{
		m_iCurRow = 0;
		return m_iCurRow < m_Rows.Count();
	}

	virtual CColumnValue	GetColumnValue( int iColumn )
	{
		return CColumnValue( this, iColumn );
	}

	virtual CColumnValue	GetColumnValue( const char *pColumnName )
	{
		return CColumnValue( this, GetColumnIndex( pColumnName ) );
	}

	virtual const char*		GetColumnValue_String( int iColumn )
	{
		if ( iColumn < 0 || iColumn >= m_ColumnNames.Count() )
			return "<invalid column specified>";
		else if ( m_iCurRow < 0 || m_iCurRow >= m_Rows.Count() )
			return "<invalid row specified>";
		else
			return m_Rows[m_iCurRow]->m_Columns[iColumn];
	}

	virtual long			GetColumnValue_Int( int iColumn )
	{
		return atoi( GetColumnValue_String( iColumn ) );
	}

	virtual int				GetColumnIndex( const char *pColumnName )
	{
		for ( int i=0; i < m_ColumnNames.Count(); i++ )
		{
			if ( stricmp( m_ColumnNames[i], pColumnName ) == 0 )
				return i;
		}
		return -1;
	}


public:
	int m_iCurRow;
	CUtlVector<CCopiedRow*> m_Rows;
	CUtlVector<char*> m_ColumnNames;
};


// -------------------------------------------------------------------------------------------------------- //
// CMySQL class.
// -------------------------------------------------------------------------------------------------------- //

class CMySQL : public IMySQL
{
public:
							CMySQL();
	virtual					~CMySQL();

	virtual bool			InitMySQL( const char *pDBName, const char *pHostName, const char *pUserName, const char *pPassword );
	virtual void			Release();
	virtual int				Execute( const char *pString );
	virtual IMySQLRowSet*	DuplicateRowSet();
	virtual unsigned long	InsertID();
	virtual int				NumFields();
	virtual const char*		GetFieldName( int iColumn );
	virtual bool			NextRow();
	virtual bool			SeekToFirstRow();
	virtual CColumnValue	GetColumnValue( int iColumn );
	virtual CColumnValue	GetColumnValue( const char *pColumnName );
	virtual const char*		GetColumnValue_String( int iColumn );
	virtual long			GetColumnValue_Int( int iColumn );
	virtual int				GetColumnIndex( const char *pColumnName );

	// Cancels the storage of the rows from the latest query.
	void					CancelIteration();

	virtual const char *	GetLastError( void );


public:

	MYSQL		*m_pSQL;
	MYSQL_RES	*m_pResult;
	MYSQL_ROW	m_Row;
	CUtlVector<MYSQL_FIELD>	m_Fields;

	char m_szLastError[128];
};


EXPOSE_INTERFACE( CMySQL, IMySQL, MYSQL_WRAPPER_VERSION_NAME );


// -------------------------------------------------------------------------------------------------------- //
// CMySQL implementation.
// -------------------------------------------------------------------------------------------------------- //

CMySQL::CMySQL()
{
	m_pSQL = NULL;
	m_pResult = NULL;
	m_Row = NULL;
}


CMySQL::~CMySQL()
{
	CancelIteration();

	if ( m_pSQL )
	{
		mysql_close( m_pSQL );
		m_pSQL = NULL;
	}
}


bool CMySQL::InitMySQL( const char *pDBName, const char *pHostName, const char *pUserName, const char *pPassword )
{
	MYSQL *pSQL = mysql_init( NULL );
	if ( !pSQL )
		return NULL;

	if ( !mysql_real_connect( pSQL, pHostName, pUserName, pPassword, pDBName, 0, NULL, 0 ) )
	{
		Q_strncpy( m_szLastError, mysql_error( pSQL ), sizeof(m_szLastError) );

		mysql_close( pSQL );
		return false;
	}

	m_pSQL = pSQL;
	return true;
}


void CMySQL::Release()
{
	delete this;
}


int CMySQL::Execute( const char *pString )
{
	CancelIteration();

	int result = mysql_query( m_pSQL, pString );
	if ( result == 0 )
	{
		// Is this a query with a result set?
		m_pResult = mysql_store_result( m_pSQL );
		if ( m_pResult )
		{
			// Store the field information.
			int count = mysql_field_count( m_pSQL );
			MYSQL_FIELD *pFields = mysql_fetch_fields( m_pResult );
			m_Fields.CopyArray( pFields, count );
			return 0;
		}
		else
		{
			// No result set. Was a set expected?
			if ( mysql_field_count( m_pSQL ) != 0 )
				return 1;	// error! The query expected data but didn't get it.
		}
	}
	else
	{
		const char *pError = mysql_error( m_pSQL );
		pError = pError;
	}

	return result;
}


IMySQLRowSet* CMySQL::DuplicateRowSet()
{
	CMySQLCopiedRowSet *pSet = new CMySQLCopiedRowSet;

	pSet->m_iCurRow = -1;

	pSet->m_ColumnNames.SetSize( m_Fields.Count() );
	for ( int i=0; i < m_Fields.Count(); i++ )
		pSet->m_ColumnNames[i] = CopyString( m_Fields[i].name );

	while ( NextRow() )
	{
		CCopiedRow *pRow = new CCopiedRow;
		pSet->m_Rows.AddToTail( pRow );
		pRow->m_Columns.SetSize( m_Fields.Count() );

		for ( int i=0; i < m_Fields.Count(); i++ )
		{
			pRow->m_Columns[i] = CopyString( m_Row[i] );
		}
	}

	return pSet;
}


unsigned long CMySQL::InsertID()
{
	return mysql_insert_id( m_pSQL );
}


int CMySQL::NumFields()
{
	return m_Fields.Count();
}


const char* CMySQL::GetFieldName( int iColumn )
{
	return m_Fields[iColumn].name;
}


bool CMySQL::NextRow()
{
	if ( !m_pResult )
		return false;

	m_Row = mysql_fetch_row( m_pResult );
	if ( m_Row == 0 )
	{
		return false;
	}
	else
	{
		return true;
	}
}


bool CMySQL::SeekToFirstRow()
{
	if ( !m_pResult )
		return false;

	mysql_data_seek( m_pResult, 0 );
	return true;
}


CColumnValue CMySQL::GetColumnValue( int iColumn )
{
	return CColumnValue( this, iColumn );
}


CColumnValue CMySQL::GetColumnValue( const char *pColumnName )
{
	return CColumnValue( this, GetColumnIndex( pColumnName ) );
}


const char* CMySQL::GetColumnValue_String( int iColumn )
{
	if ( m_Row && iColumn >= 0 && iColumn < m_Fields.Count() && m_Row[iColumn] )
		return m_Row[iColumn];
	else
		return "";
}


long CMySQL::GetColumnValue_Int( int iColumn )
{
	return atoi( GetColumnValue_String( iColumn ) );
}


int CMySQL::GetColumnIndex( const char *pColumnName )
{
	for ( int i=0; i < m_Fields.Count(); i++ )
	{
		if ( stricmp( pColumnName, m_Fields[i].name ) == 0 )
		{
			return i;
		}
	}
	
	return -1;
}


void CMySQL::CancelIteration()
{
	m_Fields.Purge();
	
	if ( m_pResult )
	{
		mysql_free_result( m_pResult );
		m_pResult = NULL;
	}

	m_Row = NULL;
}

const char *CMySQL::GetLastError( void )
{
	// Default to the last error if m_pSQL was not successfully initialized
	const char *pszLastError = m_szLastError;

	if ( m_pSQL )
	{
		pszLastError = mysql_error( m_pSQL );
	}

	return pszLastError;
}