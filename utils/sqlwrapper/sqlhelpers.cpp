//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

//#include "misc.h"
#include "isqlwrapper.h"
#include "time.h"
#include "sqlhelpers.h"

#include "tier0/memdbgon.h"

#pragma warning( disable : 4706 ) // Warning C4706: assignment within conditional expression

//-----------------------------------------------------------------------------
// Purpose: converts between a MySQL field type and our column types
//-----------------------------------------------------------------------------
EColumnType GetEColumnTypeFromESQLFieldType( ESQLFieldType eSQLFieldType )
{
	switch( eSQLFieldType )
	{
	case FIELD_TYPE_DECIMAL:
	case FIELD_TYPE_TINY:
	case FIELD_TYPE_SHORT:
	case FIELD_TYPE_LONG:
		return SQL_INT;
		break;
	case FIELD_TYPE_FLOAT:
	case FIELD_TYPE_DOUBLE:
		return SQL_FLOAT;
		break;
	case FIELD_TYPE_LONGLONG:
		return SQL_UINT64;
		break;
	case FIELD_TYPE_NULL: 
	case FIELD_TYPE_INT24:
		return SQL_NONE;
		break;
	case FIELD_TYPE_DATE:
	case FIELD_TYPE_TIMESTAMP:
	case FIELD_TYPE_TIME:
	case FIELD_TYPE_DATETIME:
	case FIELD_TYPE_YEAR:
	case FIELD_TYPE_NEWDATE:
		return SQL_TIME;
		break;
	case FIELD_TYPE_ENUM:
	case FIELD_TYPE_SET:
	case FIELD_TYPE_TINY_BLOB:
	case FIELD_TYPE_MEDIUM_BLOB:
	case FIELD_TYPE_LONG_BLOB:
	case FIELD_TYPE_BLOB:
		return SQL_NONE;
		break;
	case FIELD_TYPE_VAR_STRING:
	case FIELD_TYPE_STRING:
		return SQL_STRING;
		break;
	}
	return SQL_NONE;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CSQLColumn::CSQLColumn( const char *pchName, ESQLFieldType eSQLFieldType )
{
	Assert( pchName );
	Q_strncpy( m_rgchName, pchName, sizeof( m_rgchName ) );
	m_ESQLFieldType = eSQLFieldType;
	m_EColumnType = GetEColumnTypeFromESQLFieldType( m_ESQLFieldType );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSQLTable::CSQLTable( const char *pchName )
{
	Assert( pchName );
	Q_strncpy( m_rgchName, pchName, sizeof( m_rgchName ) );
}


//-----------------------------------------------------------------------------
// Purpose: copy constructor (used by CUtlVector)
//-----------------------------------------------------------------------------
CSQLTable::CSQLTable( CSQLTable const &sqlTable )
{
	Q_strncpy( m_rgchName, sqlTable.m_rgchName, sizeof( m_rgchName ) );
	for ( int iSQLColumn = 0; iSQLColumn < sqlTable.m_VecSQLColumn.Count(); iSQLColumn++ )
	{
		m_VecSQLColumn.AddToTail( CSQLColumn( sqlTable.m_VecSQLColumn[iSQLColumn].PchColumnName(),
			sqlTable.m_VecSQLColumn[iSQLColumn].GetESQLFieldType() ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: add a new column to this data description
//-----------------------------------------------------------------------------
void CSQLTable::AddColumn( const char *pchName, ESQLFieldType eSQLFieldType )
{
	m_VecSQLColumn.AddToTail( CSQLColumn( pchName, eSQLFieldType ) );
}


//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CSQLTable::Validate( CValidator &validator, char *pchName )
{
	validator.Push( "CSQLTable", this, pchName );

	m_VecSQLColumn.Validate( validator, "m_VecSQLColumn" );

	validator.Pop();
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Create the table descriptions from a data base
//-----------------------------------------------------------------------------
bool CSQLTableSet::Init( ISQLHelper *pSQLHelper )
{
	m_VecSQLTable.RemoveAll();

	MYSQL_RES *pMySQLRes = NULL;
	bool bRet = pSQLHelper->BInternalQuery( "show tables", &pMySQLRes );
	if ( !bRet || !pMySQLRes )
	{
		return false;
	}

	MYSQL_ROW row;
	while ( ( row = mysql_fetch_row( pMySQLRes ) ) )
	{
		CSQLTable sqlTable( row[0] );
		m_VecSQLTable.AddToTail( sqlTable );
	}
	mysql_free_result( pMySQLRes );

	for ( int i = 0; i < m_VecSQLTable.Count(); i++ )
	{
		CSQLTable &sqlTable = m_VecSQLTable[i];

		char szQuery[512];
		Q_snprintf( szQuery, sizeof(szQuery), "select * from %s limit 1", sqlTable.PchName() );
		MYSQL_RES *pMySQLRes = NULL;
		bool bRet = pSQLHelper->BInternalQuery( szQuery, &pMySQLRes );
		if ( !bRet || !pMySQLRes )
		{
			m_VecSQLTable.RemoveAll();
			return false;
		}

		uint nFields = mysql_num_fields( pMySQLRes );
		Assert( nFields > 0 );
		MYSQL_FIELD *pFields = mysql_fetch_fields( pMySQLRes );
		for( uint i = 0; i < nFields; i++)
		{
			sqlTable.AddColumn( pFields[i].name, pFields[i].type );
		}

		mysql_free_result( pMySQLRes );
	}

	m_bInit = true;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: returns a pointer to each table in the db
//-----------------------------------------------------------------------------
const ISQLTable *CSQLTableSet::PSQLTable( int iSQLTable ) const 
{
	return &m_VecSQLTable[iSQLTable];
}


//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CSQLTableSet::Validate( CValidator &validator, char *pchName )
{
	validator.Push( "CSQLTableSet", this, pchName );

	m_VecSQLTable.Validate( validator, "m_VecSQLTable" );
	for ( int i = 0; i < m_VecSQLTable.Count(); i++ )
	{
		m_VecSQLTable[i].Validate( validator, "m_VecSQLTable[i]" );
	}

	validator.Pop();
}
#endif


//-----------------------------------------------------------------------------
// Purpose: constructor 
//-----------------------------------------------------------------------------
CSQLRow::CSQLRow( MYSQL_ROW *pMySQLRow, const ISQLTable *pSQLTableDescription, int cRowData )
{
	Assert( pMySQLRow && pSQLTableDescription );
	for ( int iRowData = 0; iRowData < cRowData; iRowData++ )
	{
		struct SQLRowData_s sqlRowData;
		sqlRowData.eColumnType = pSQLTableDescription->GetEColumnType( iRowData );
		const CSQLTable *tableDesc = static_cast<const CSQLTable *>(pSQLTableDescription);
		switch ( tableDesc->GetESQLFieldType( iRowData ) )
		{
		case FIELD_TYPE_TINY:
		case FIELD_TYPE_DECIMAL:
		case FIELD_TYPE_SHORT:
		case FIELD_TYPE_LONG:
			sqlRowData.data.nData = atoi( ( *pMySQLRow ) [iRowData] );
			break;

		case FIELD_TYPE_FLOAT:
		case FIELD_TYPE_DOUBLE:
			sqlRowData.data.flData = atof( ( *pMySQLRow ) [iRowData] );
			break;

		case FIELD_TYPE_NULL: 
		case FIELD_TYPE_INT24:
			memset( &sqlRowData.data, 0x0, sizeof( sqlRowData.data ) );
			break;

		case FIELD_TYPE_LONGLONG:
			sqlRowData.data.nData = atoi( ( *pMySQLRow ) [iRowData] );
			break;

		case FIELD_TYPE_TIMESTAMP:
		case FIELD_TYPE_DATETIME:
			{
			struct tm tm;
			char num[5];
			char szValue[64];
			Q_memset( &tm, 0x0, sizeof( tm ) );
			Q_strncpy( szValue, ( *pMySQLRow )[iRowData], sizeof( szValue ) );
			const char *str= szValue;
			num[0] =*str++; num[1] =*str++; num[2] =*str++; num[3] =*str++; num[4] = 0;
			tm.tm_year = strtol( num, 0, 10 ) - 1900;
			if (*str == '-') str++;
			num[0] = *str++; num[1] = *str++; num[2] = 0;
			tm.tm_mon = strtol( num, 0, 10 ) - 1;
			if (*str == '-') str++;
			num[0] = *str++; num[1] = *str++; num[2] = 0;
			tm.tm_mday = strtol( num, 0, 10 );
			num[0] = *str++; num[1] = *str++; num[2] = 0;
			tm.tm_hour = strtol( num, 0, 10 );
			if (*str == ':') str++;
			num[0] = *str++; num[1] = *str++; num[2] = 0;
			tm.tm_min = strtol( num, 0, 10 );
			if (*str == ':') str++;
			num[0] = *str++; num[1] = *str++; num[2] = 0;
			tm.tm_sec = strtol( num, 0, 10 );
		
			sqlRowData.data.ulTime = _mktime64( &tm );
			}
			break;

		case FIELD_TYPE_TIME:
		case FIELD_TYPE_DATE:
		case FIELD_TYPE_YEAR:
		case FIELD_TYPE_NEWDATE:
			sqlRowData.data.ulTime = time( NULL ); // FIXME
			Assert( !"Not implemented" );
			break;

		case FIELD_TYPE_ENUM:
		case FIELD_TYPE_SET:
		case FIELD_TYPE_TINY_BLOB:
		case FIELD_TYPE_MEDIUM_BLOB:
		case FIELD_TYPE_LONG_BLOB:
		case FIELD_TYPE_BLOB:
			memset( &sqlRowData.data, 0x0, sizeof( sqlRowData.data ) );
			Assert( !"Not implemented" );
			break;

		case FIELD_TYPE_VAR_STRING:
		case FIELD_TYPE_STRING:
			{
			int cchString = Q_strlen( ( char * )( *pMySQLRow )[iRowData] );
			char *pchNewString = new char[ cchString + 1 ];
			m_VecPchStoredStrings.AddToTail( pchNewString );
			Q_strncpy( pchNewString, ( char * )( *pMySQLRow )[iRowData], cchString + 1 );
			pchNewString[ cchString ] = 0;
			sqlRowData.data.pchData = pchNewString;
			}
			break;
		}
		m_VecSQLRowData.AddToTail( sqlRowData );
	}
}

//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CSQLRow::~CSQLRow()
{
	for ( int ipch = 0; ipch < m_VecPchStoredStrings.Count(); ipch++ )
	{
		delete m_VecPchStoredStrings[ipch];
	}
}


//-----------------------------------------------------------------------------
// Purpose: pointer to the char data
//-----------------------------------------------------------------------------
const char *CSQLRow::PchData( int iRowData ) const
{
	Assert( m_VecSQLRowData[iRowData].eColumnType == SQL_STRING );
	return m_VecSQLRowData[iRowData].data.pchData;
}


//-----------------------------------------------------------------------------
// Purpose: returns the int value of this column
//-----------------------------------------------------------------------------
int CSQLRow::NData( int iRowData ) const
{
	Assert( m_VecSQLRowData[iRowData].eColumnType == SQL_INT);
	return m_VecSQLRowData[iRowData].data.nData;
}


//-----------------------------------------------------------------------------
// Purpose: returns the uint64 value of this column
//-----------------------------------------------------------------------------
uint64 CSQLRow::UlData( int iRowData ) const
{
	Assert( m_VecSQLRowData[iRowData].eColumnType == SQL_UINT64);
	return m_VecSQLRowData[iRowData].data.ulData;
}


//-----------------------------------------------------------------------------
// Purpose: returns the float value of this column
//-----------------------------------------------------------------------------
float CSQLRow::FlData( int iRowData ) const
{
	Assert( m_VecSQLRowData[iRowData].eColumnType == SQL_FLOAT );
	return m_VecSQLRowData[iRowData].data.flData;
}


//-----------------------------------------------------------------------------
// Purpose: returns the time value of this column (time_t equivalent, seconds since midnight (00:00:00), January 1, 1970, coordinated universal time (UTC)
//			ignoring daylight savings
//-----------------------------------------------------------------------------
uint64 CSQLRow::UlTime( int iRowData ) const
{
	Assert( m_VecSQLRowData[iRowData].eColumnType == SQL_TIME );
	return m_VecSQLRowData[iRowData].data.ulTime;
}


//-----------------------------------------------------------------------------
// Purpose: returns the boolean value of this column
//-----------------------------------------------------------------------------
bool CSQLRow::BData( int iRowData ) const
{
	Assert( m_VecSQLRowData[iRowData].eColumnType == SQL_INT );
	return (m_VecSQLRowData[iRowData].data.nData == 1);
}


//-----------------------------------------------------------------------------
// Purpose: returns the data type contained in this column
//-----------------------------------------------------------------------------
EColumnType CSQLRow::GetEColumnType( int iRowData ) const
{
	return m_VecSQLRowData[iRowData].eColumnType;
}


//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CSQLRow::Validate( CValidator &validator, char *pchName )
{
	validator.Push( "CSQLRow", this, pchName );

	m_VecSQLRowData.Validate( validator, "m_VecSQLRowData" );

	m_VecPchStoredStrings.Validate( validator, "m_VecPchStoredStrings" );
	for ( int i = 0; i < m_VecPchStoredStrings.Count(); i++ )
	{
		validator.ClaimMemory( m_VecPchStoredStrings[i] );
	}

	validator.Pop();
}
#endif


//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CResultSet::CResultSet()
:m_SQLTableDescription( "Table" )
{
	m_pSQLRow = NULL;
	m_MySQLRes = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: run a query on the database
//-----------------------------------------------------------------------------
bool CResultSet::Query( const char *pchQuery, ISQLHelper *pSQLHelper )
{
	m_SQLTableDescription.Reset();

	m_pSQLRow = NULL;

	bool bRet = pSQLHelper->BInternalQuery( pchQuery, &m_MySQLRes );
	if ( !bRet || !m_MySQLRes ) 
	{
		m_cSQLField = -1;
		m_cSQLRow = -1;
		return false;
	}

	m_cSQLField = mysql_num_fields( m_MySQLRes );
	m_cSQLRow = mysql_num_rows( m_MySQLRes );

	MYSQL_FIELD  *pMySQLField = mysql_fetch_fields( m_MySQLRes );
	for ( int iMySQLField = 0; iMySQLField < m_cSQLField; iMySQLField++ )
	{
		m_SQLTableDescription.AddColumn( pMySQLField[iMySQLField].name, pMySQLField[iMySQLField].type );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CResultSet::~CResultSet()
{
	FreeResult();
}


//-----------------------------------------------------------------------------
// Purpose: free any outstanding query
//-----------------------------------------------------------------------------
void CResultSet::FreeResult()
{
	if ( m_MySQLRes )
	{
		mysql_free_result( m_MySQLRes );
		m_MySQLRes = NULL;
	}

	if ( m_pSQLRow )
	{
		delete m_pSQLRow;
		m_pSQLRow = NULL;
	}

	m_SQLTableDescription.Reset();
}


//-----------------------------------------------------------------------------
// Purpose: number of rows in the result set
//-----------------------------------------------------------------------------
int CResultSet::GetCSQLRow() const
{
	return m_cSQLRow;
}


//-----------------------------------------------------------------------------
// Purpose: get the new row from the result set
//-----------------------------------------------------------------------------
const ISQLRow *CResultSet::PSQLRowNextResult()
{
	if ( m_pSQLRow )
	{
		delete m_pSQLRow;
		m_pSQLRow = NULL;
	}

	MYSQL_ROW mySQLRow;
	if ( mySQLRow = mysql_fetch_row( m_MySQLRes ) )
	{
		m_pSQLRow = new CSQLRow( &mySQLRow, &m_SQLTableDescription, m_cSQLField );
	}
	else
	{
		FreeResult(); // end of result set
	}

	return m_pSQLRow;
}


//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CResultSet::Validate( CValidator &validator, char *pchName )
{
	validator.Push( "CResultSet", this, pchName );

	validator.ClaimMemory( m_MySQLRes );
	if ( m_pSQLRow )
	{
		validator.ClaimMemory( m_pSQLRow );
		m_pSQLRow->Validate( validator, "m_pSQLRow" );
	}

	m_SQLTableDescription.Validate( validator, "m_SQLTableDescription" );

	validator.Pop();
}
#endif

