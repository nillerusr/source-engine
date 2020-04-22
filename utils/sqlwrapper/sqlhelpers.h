//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================
#include "utlvector.h"

extern "C"
{
	#include <WinSock.H> 
	#include "mysql.h"
};
#include "utlvector.h"

typedef enum_field_types ESQLFieldType;

//-----------------------------------------------------------------------------
// Purpose: helper interface for running queries internal to this dll
//-----------------------------------------------------------------------------
class ISQLHelper
{
public:
	// run a sql query on the db
	virtual bool BInternalQuery( const char *pchQueryString, MYSQL_RES **ppMySQLRes, bool bRecurse = true ) = 0;
};


//-----------------------------------------------------------------------------
// Purpose: represents the data about a single SQL column
//-----------------------------------------------------------------------------
class CSQLColumn 
{
public:
	CSQLColumn() { m_rgchName[0] = 0; }
	CSQLColumn( const char *pchName, ESQLFieldType eSQLFieldType );
	virtual const char *PchColumnName() const { return m_rgchName; };
	virtual EColumnType GetEColumnType() const { return m_EColumnType; };
	virtual ESQLFieldType GetESQLFieldType() const { return m_ESQLFieldType; };

private:
	char m_rgchName[64];
	EColumnType m_EColumnType;
	ESQLFieldType m_ESQLFieldType;
};


//-----------------------------------------------------------------------------
// Purpose: encapsulates a table's description
//-----------------------------------------------------------------------------
class CSQLTable : public ISQLTable
{
public:
	CSQLTable( const char *pchName );
	CSQLTable( CSQLTable const &CSQLTable );  // copy constructor
	void AddColumn( const char *pchName, ESQLFieldType ESQLFieldType );
	const char *PchName() const { return m_rgchName; };
	virtual int GetCSQLColumn() const { return m_VecSQLColumn.Count(); };
	virtual const char *PchColumnName( int iSQLColumn ) const { return m_VecSQLColumn[iSQLColumn].PchColumnName(); };
	virtual EColumnType GetEColumnType( int iSQLColumn ) const { return m_VecSQLColumn[iSQLColumn].GetEColumnType(); };
	virtual ESQLFieldType GetESQLFieldType( int iSQLColumn ) const { return m_VecSQLColumn[iSQLColumn].GetESQLFieldType(); };

	void Reset() { m_VecSQLColumn.RemoveAll(); }

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, char *pchName );
#endif

private:
	char m_rgchName[64];
	CUtlVector<CSQLColumn> m_VecSQLColumn;
};


//-----------------------------------------------------------------------------
// Purpose: encapsulates a db's worth of tables
//-----------------------------------------------------------------------------
class CSQLTableSet : public ISQLTableSet
{
public:
	CSQLTableSet() { m_bInit = false; }
	bool Init( ISQLHelper *pISQLHelper  );
	bool BInit() { return m_bInit; }
	virtual int GetCSQLTable() const { return m_VecSQLTable.Count(); };
	virtual const ISQLTable *PSQLTable( int iSQLTable ) const;
	
#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, char *pchName );
#endif

private:
	static EColumnType EColumnTypeFromPchName( const char *pchName );
	CUtlVector<CSQLTable> m_VecSQLTable;
	bool m_bInit;
};


//-----------------------------------------------------------------------------
// Purpose: describes a single row in a result set
//-----------------------------------------------------------------------------
class CSQLRow : public ISQLRow
{
public:
	CSQLRow( MYSQL_ROW *pMySQLRow, const ISQLTable *pSQLTableDescription, int cSQLRowData );
	~CSQLRow();
	virtual int GetCSQLRowData() const { return m_VecSQLRowData.Count(); };
	virtual const char *PchData( int iSQLRowData ) const;
	virtual int NData( int iSQLRowData ) const;
	virtual uint64 UlData( int iSQLRowData ) const;
	virtual float FlData( int iSQLRowData ) const;
	virtual uint64 UlTime( int iSQLRowData ) const;
	virtual bool BData( int iSQLRowData ) const;
	virtual EColumnType GetEColumnType( int iSQLRowData ) const;

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, char *pchName );
#endif

private:
	struct SQLRowData_s
	{
		union
		{
			int nData;
			float flData;
			const char *pchData;
			uint64 ulTime;
			uint64 ulData;
		} data;
		EColumnType eColumnType;
	};

	CUtlVector<struct SQLRowData_s> m_VecSQLRowData;
	CUtlVector<char *> m_VecPchStoredStrings;
};


//-----------------------------------------------------------------------------
// Purpose: encapsulates a result set from a SQL query
//-----------------------------------------------------------------------------
class CResultSet : public IResultSet
{
public:
	CResultSet();
	~CResultSet();
	bool Query( const char *pchQuery, ISQLHelper *pISQLHelper );
	virtual int GetCSQLRow() const;
	virtual const ISQLRow *PSQLRowNextResult();
	void FreeResult();

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, char *pchName );
#endif

private:
	MYSQL_RES *m_MySQLRes;
	CSQLRow *m_pSQLRow;
	CSQLTable m_SQLTableDescription;
	int m_cSQLField;
	int m_cSQLRow;
};

