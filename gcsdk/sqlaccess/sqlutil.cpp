//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================


#include "stdafx.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{
const char *GetInsertArgString()
{
	static char s_str[1024];
	static bool s_bInit = false;

	if ( !s_bInit )
	{
		for ( int i = 0; i < 1023; i++ )
		{
			s_str[i] = i % 2 == 0 ? '?' : ',';
		}

		s_str[1023] = NULL;
		s_bInit = true;
	}

	return s_str;
}

uint32 GetInsertArgStringChars( uint32 nNumParams )
{
	AssertMsg( nNumParams <= GetInsertArgStringMaxParams(), "Error: Requested more characters than are provided by the GetInsertArgString" );
	if( nNumParams == 0 )
		return 0;

	return nNumParams * 2 - 1;
}

uint32 GetInsertArgStringMaxParams()
{
	return 512;
}

//-----------------------------------------------------------------------------
// Purpose: Converts array of field data to text for SQL IN clause
// Input:	columnInfo - schema of column being converted
//			pubData - pointer to array of data to convert
//			cubData - size of array of data
//			rgchResult - pointer to output buffer
//			cubResultLen - size of output buffer
//			bForPreparedStatement - Should we prepare the text for a prepared statement or directly place the values?
//-----------------------------------------------------------------------------
void ConvertFieldArrayToInText( const CColumnInfo &columnInfo, byte *pubData, int cubData, char *rgchResult, int cubResultLen, bool bForPreparedStatement )
{
	int32 cubLength = columnInfo.GetFixedSize();
	Assert( cubData % cubLength == 0 );
	int32 nArrayCount = cubData / cubLength;

	int32 len = 0;
	rgchResult[len++] = '(';
	for( int i = 0; i < nArrayCount; ++i )
	{
		if ( bForPreparedStatement )
		{
			if ( i < nArrayCount - 1 )
			{
				rgchResult[len++] = '?';
				rgchResult[len++] = ',';	
			}
			else
			{
				rgchResult[len++] = '?';
				rgchResult[len++] = ')';	
			}
		}
		else
		{
			switch ( columnInfo.GetType() )
			{
			case k_EGCSQLType_int8:
				if ( i < nArrayCount - 1 )
					len += Q_snprintf( rgchResult + len, cubResultLen - len, "%d,", *( (byte *) pubData ) );
				else
					len += Q_snprintf( rgchResult + len, cubResultLen - len, "%d)", *( (byte *) pubData ) );
				break;
			case k_EGCSQLType_int16:
				if ( i < nArrayCount - 1 )
					len += Q_snprintf( rgchResult + len, cubResultLen - len, "%d,", *( (short *) pubData ) );
				else
					len += Q_snprintf( rgchResult + len, cubResultLen - len, "%d)", *( (short *) pubData ) );
				break;
			case k_EGCSQLType_int32:
				if ( i < nArrayCount - 1 )
					len += Q_snprintf( rgchResult + len, cubResultLen - len, "%d,", *( (int *) pubData ) );
				else
					len += Q_snprintf( rgchResult + len, cubResultLen - len, "%d)", *( (int *) pubData ) );
				break;
			case k_EGCSQLType_int64:
				if ( i < nArrayCount - 1 )
					len += Q_snprintf( rgchResult + len, cubResultLen - len, "%lld,", *( (int64 *) pubData ) );
				else
					len += Q_snprintf( rgchResult + len, cubResultLen - len, "%lld)", *( (int64 *) pubData ) );
				break;
			default:
				AssertMsg( false, "Unsupported data type for non prepares statement with IN clause\n" );
				rgchResult[0] = 0;
				return;
			}
		}

		if( len >= cubResultLen - 1 )
		{
			AssertMsg( false, "Generation of IN clause foverflowed\n" );
			rgchResult[0] = 0;
			return;
		}
		pubData += cubLength;
	}

	rgchResult[len] = 0;
	return;
}


//-----------------------------------------------------------------------------
// Purpose: Converts field data to text equivalent for SQL statement
// Input:	eFieldType - The type of the field to convert to text
//			pubRecord - pointer to record data to convert
//			cubRecord - size of record data
//			rgchField - pointer to output buffer
//			cchField - size of output buffer
//-----------------------------------------------------------------------------
void ConvertFieldToText( EGCSQLType eFieldType, uint8 *pubRecord, int cubRecord, char *rgchField, int cchField, bool bQuoteString )
{
	char rgchTmp[k_cMedBuff];

	switch ( eFieldType )
	{
	case k_EGCSQLType_int8:
		Q_snprintf( rgchField, cchField, "%d", *( (byte *) pubRecord ) );
		break;
	case k_EGCSQLType_int16:
		Q_snprintf( rgchField, cchField, "%d", *( (short *) pubRecord ) );
		break;
	case k_EGCSQLType_int32:
		Q_snprintf( rgchField, cchField, "%d", *( (int *) pubRecord ) );
		break;
	case k_EGCSQLType_int64:
		Q_snprintf( rgchField, cchField, "%lld", *( (int64 *) pubRecord ) );
		break;
	case k_EGCSQLType_float:
		Q_snprintf( rgchField, cchField, "%f", *((float*) pubRecord) );
			break;
	case k_EGCSQLType_double:
		Q_snprintf( rgchField, cchField, "%f", *((double*) pubRecord) );
		break;
	case k_EGCSQLType_String:
		if ( pubRecord && *pubRecord )
		{
			Assert( cubRecord + 1 < Q_ARRAYSIZE( rgchTmp ) );

			Q_memcpy( rgchTmp, (char *) pubRecord, cubRecord );
			rgchTmp[cubRecord] = 0;

			if ( bQuoteString )
			{
				EscapeStringValue( rgchTmp, Q_ARRAYSIZE( rgchTmp ) );
				Q_snprintf( rgchField, cchField, "'%s'", rgchTmp );
			}
			else
			{
				Q_strncpy( rgchField, rgchTmp, cchField );
			}
		}
		else
		{
			if ( bQuoteString )
			{
				Q_strncpy( rgchField, "''", cchField );
			}
			else
			{
				Q_strncpy( rgchField, "", cchField );
			}
		}
		break;
	case k_EGCSQLType_Blob:
	case k_EGCSQLType_Image:
		Q_strncpy( rgchField, "0x", cchField );
		Q_binarytohex( pubRecord, cubRecord, rgchField + 2, cchField - 2 );
		break;
	default:
		Assert( false );
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns the text SQL type for a given field
// Input:	field - field to determine type for
//			pchBuf - pointer to output buffer
//			cchBuf - size of output buffer
// Output:	returns pchBuf for convenience of one-line usage
//-----------------------------------------------------------------------------
char *SQLTypeFromField( const CColumnInfo &colInfo, char *pchBuf, int cchBuf )
{
	EGCSQLType eType = colInfo.GetType();
	*pchBuf = 0;
	switch ( eType )
	{
	case k_EGCSQLType_int8:
		Q_strncpy( pchBuf, "TINYINT", cchBuf );
		break;
	case k_EGCSQLType_int16:
		Q_strncpy( pchBuf, "SMALLINT", cchBuf );
		break;
	case k_EGCSQLType_int32:
		Q_strncpy( pchBuf, "INT", cchBuf );
		break;
	case k_EGCSQLType_int64:
		Q_strncpy( pchBuf, "BIGINT", cchBuf );
		break;
	case k_EGCSQLType_float:
		Q_strncpy( pchBuf, "REAL", cchBuf );
		break;
	case k_EGCSQLType_double:
		Q_strncpy( pchBuf, "FLOAT", cchBuf );
		break;
	case k_EGCSQLType_String:
		Q_snprintf( pchBuf, cchBuf, "VARCHAR(%d)", colInfo.GetMaxSize() );
		break;
	case k_EGCSQLType_Blob:
		Q_snprintf( pchBuf, cchBuf, "VARBINARY(%d)", colInfo.GetMaxSize() );
		break;
	case k_EGCSQLType_Image:
		Q_strncpy( pchBuf, "IMAGE", cchBuf );
		break;
	default:
		Assert( false );
		break;
	}

	return pchBuf;
}



//-----------------------------------------------------------------------------
// Purpose: Escapes any single quotes to a string value to double single quotes
// Input:	rgchField - text to escape
//			cchField - size of text buffer
// Notes:	The text will be escaped and expanded in place in the buffer.
//			In the worst case, the text may expand by 2x.  (If the field is all
//			single quotes.)  So, you must pass in a buffer which is at least
//			twice as long as the text length so we can guarantee to be able to
//			escape the string.
//-----------------------------------------------------------------------------
void EscapeStringValue( char *rgchField, int cchField )
{
	// TODO - what else do we need to escape?  %() ...
	char *pubCur = rgchField;
	int nLen = 0;
	int cSingleQuotes = 0;

	// This function gets called on every text field we write but most text fields
	// don't need to be escaped, so try to be as fast as possible in the normal case.

	// first, walk through the string and count the string length and number of single quotes
	while ( *pubCur )
	{
		if ( '\'' == *pubCur )
			cSingleQuotes++;
		nLen ++;
		pubCur++;
	}

	// if no single quotes, nothing to do
	if ( !cSingleQuotes )
		return;

	// caller must pass in a buffer that's long enough for expansion
	Assert( nLen + cSingleQuotes + 1 <= cchField );
	if ( !( nLen + cSingleQuotes + 1 <= cchField ) )
		return;

	// We know exactly how many characters the string will expand by (the # of single quotes).  Walk backward
	// and copy the characters into the right places.  This touches each character only once.
	pubCur = rgchField + nLen + cSingleQuotes;
	*pubCur = 0;
	pubCur--;
	while ( pubCur > rgchField && cSingleQuotes > 0 )
	{
		// read pointer is offset from write pointer by # of remaining single quotes
		char *pubRead = pubCur - cSingleQuotes;
		Assert( pubRead >= rgchField );
		// copy each character
		*pubCur = *pubRead;
		if ( '\'' == *pubRead )
		{
			// if the character is a single quote, back up one more and insert another single quote to escape it
			pubCur --;
			*pubCur = '\'';
			// decrement # of single quotes remaining
			cSingleQuotes --;
			Assert( cSingleQuotes >= 0 );
		}
		pubCur--;
	}
}
//-----------------------------------------------------------------------------
// Purpose: Adds constraint information to a SQL command to add or remove constraint
// Input:	pchTableName - name of table
//			pchColumnName - name of column
//			nColFlagConstraint - flag with which constraint to 
//			bForAdd - whether constraint is being added or removed
//			pchCmd - buffer to append SQL command to
//			cchCmd - size of buffer
//-----------------------------------------------------------------------------
void AppendConstraint( const char *pchTableName, const char *pchColumnName, int nColFlagConstraint, bool bForAdd, 
					  bool bClustered, CFmtStrMax & sCmd, int nFillFactor )
{
	Assert( pchTableName && pchTableName[0] );
	Assert( pchColumnName && pchColumnName[0] );

	switch ( nColFlagConstraint )
	{
	case k_nColFlagPrimaryKey:
		sCmd.AppendFormat( " CONSTRAINT %s_%s_PrimaryKey", pchTableName, pchColumnName);
		if ( bForAdd )
		{
			sCmd += " PRIMARY KEY ";
			if ( bClustered )
			{
				sCmd.AppendFormat( " CLUSTERED WITH (FILLFACTOR = %d) ", nFillFactor );
			}
			else
			{
				sCmd += "NONCLUSTERED";
			}
		}
		break;
	case k_nColFlagUnique:
		/* do nothing - the uniqueness will be handled by creation of an index */
		break;
	case k_nColFlagAutoIncrement:
		sCmd += " IDENTITY";
		break;
	default:
		AssertMsg( false, "CSQLThread::AppendContraint: invalid constraint type" );
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds constraint information to a SQL command to add or remove constraint
// Input:	pRecordInfo - record info describing table
//			pColumnInfo - record info describing column
//			bForAdd - whether constraint is being added or removed
//			pchCmd - buffer to append SQL command to
//			cchCmd - size of buffer
//-----------------------------------------------------------------------------
void AppendConstraints( const CRecordInfo *pRecordInfo, const CColumnInfo *pColumnInfo, bool bForAdd, CFmtStrMax & sCmd )
{
	Assert( pRecordInfo != NULL );
	Assert( pColumnInfo != NULL );

	if ( pColumnInfo->BIsPrimaryKey() )
	{
		// any column in a PK can't be NULL.
		if ( bForAdd )
		{
			sCmd += " NOT NULL";
		}

		// only add primary key constraint here if it is a single-column PK
		if ( pRecordInfo->GetPrimaryKeyType() == k_EPrimaryKeyTypeSingle )
		{
			// get the fields on the primary key
			const CUtlVector< FieldSet_t > &refFields = pRecordInfo->GetIndexFields( );
			int nFillFactor = refFields.Element( pRecordInfo->GetPKIndex() ).GetFillFactor();
			AppendConstraint( pRecordInfo->GetName(), pColumnInfo->GetName(), k_nColFlagPrimaryKey, bForAdd, pColumnInfo->BIsClustered(), sCmd, nFillFactor );
		}
	}
	else if ( pColumnInfo->BIsUnique() )
	{
		AppendConstraint( pRecordInfo->GetName(), pColumnInfo->GetName(), k_nColFlagUnique, bForAdd, pColumnInfo->BIsClustered(), sCmd, 0 );
	}
	
	if ( pColumnInfo->BIsAutoIncrement() )
	{
		AppendConstraint( pRecordInfo->GetName(), pColumnInfo->GetName(), k_nColFlagAutoIncrement, bForAdd, pColumnInfo->BIsClustered(), sCmd, 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Generates the "CONSTRAINT ..." text for the table primary key
//-----------------------------------------------------------------------------
void BuildTablePKConstraintText( TSQLCmdStr *psStatement, CRecordInfo *pRecordInfo )
{
	const FieldSet_t& vecFields = pRecordInfo->GetPKFields( );

	psStatement->sprintf( "CONSTRAINT %s_PrimaryKey PRIMARY KEY %s ( ",
		pRecordInfo->GetName(), 
		vecFields.IsClustered() ? "CLUSTERED" : "NONCLUSTERED" );

	for ( int nField = 0; nField < vecFields.GetCount(); nField++ )
	{
		// what field is the next column in our index?
		int nThisField = vecFields.GetField( nField );
		const CColumnInfo& columnInfo = pRecordInfo->GetColumnInfo(nThisField);

		if (nField != 0)
		{
			*psStatement += ", ";
		}
		*psStatement += columnInfo.GetName();
	}

	// close our list
	*psStatement += ") ";

	if ( vecFields.GetFillFactor() != 0 )
	{
		// non-default fill factor, so specify it
		psStatement->AppendFormat( " WITH FILLFACTOR = %d ",
			vecFields.GetFillFactor() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Adds constraint information to a SQL command to add or remove table-level constraints
// Input:	pRecordInfo - record info describing table
//			pchCmd - buffer to append SQL command to
//			cchCmd - size of buffer
//-----------------------------------------------------------------------------

void AppendTableConstraints( CRecordInfo *pRecordInfo, CFmtStrMax & sCmd )
{
	// the only supported table constraint is for PKs or FKs
	if ( pRecordInfo->GetPrimaryKeyType() == k_EPrimaryKeyTypeMulti )
	{
		TSQLCmdStr tmp;
		BuildTablePKConstraintText( &tmp, pRecordInfo );
		sCmd += ", ";
		sCmd += tmp;
	}

	// Look for FKs required on this table
	// the only supported table constraint is for PKs or FKs
	int cFKs = pRecordInfo->GetFKCount();
	for( int i=0; i < cFKs; ++i )
	{
		FKData_t &fkData = pRecordInfo->GetFKData( i );

		CFmtStr sColumns, sParentColumns;
		FOR_EACH_VEC( fkData.m_VecColumnRelations, nCol )
		{
			FKColumnRelation_t &colRelation = fkData.m_VecColumnRelations[nCol];
			if ( nCol > 0)
			{
				sColumns += ",";
				sParentColumns += ",";
			}
			sColumns += colRelation.m_rgchCol;
			sParentColumns += colRelation.m_rgchParentCol;
		}

		TSQLCmdStr sTmp;
		sTmp.sprintf( ", CONSTRAINT %s FOREIGN KEY (%s) REFERENCES %s(%s) ON DELETE %s ON UPDATE %s",
			fkData.m_rgchName, sColumns.Access(), fkData.m_rgchParentTableName, sParentColumns.Access(),
			PchNameFromEForeignKeyAction( fkData.m_eOnDeleteAction ), PchNameFromEForeignKeyAction( fkData.m_eOnUpdateAction ) );

		// add to the command
		sCmd += sTmp;
	}
}




//-----------------------------------------------------------------------------
// Purpose: Builds a SQL INSERT statement
// Input:	psStatement - The string to put the statement into
//			pRecordInfo - record info describing table inserting into
//-----------------------------------------------------------------------------
void BuildInsertStatementText( TSQLCmdStr *psStatement, const CRecordInfo *pRecordInfo )
{
	psStatement->sprintf("INSERT INTO %s.%s (", GSchemaFull().GetDefaultSchemaNameForCatalog( pRecordInfo->GetESchemaCatalog() ), pRecordInfo->GetName() );

	// build a string of the field names
	int cColumns = pRecordInfo->GetNumColumns();
	int nInsertable = 0;
	bool bAddedBefore = false;
	for ( int iColumn = 0; iColumn < cColumns; iColumn++ )
	{
		const CColumnInfo &columnInfo = pRecordInfo->GetColumnInfo( iColumn );
		if ( !columnInfo.BIsInsertable() )
			continue;

		nInsertable++;

		if ( bAddedBefore )
			psStatement->Append( ',' );
		bAddedBefore = true;
		psStatement->Append( columnInfo.GetName() );
	}
	
	psStatement->AppendFormat( ") VALUES (%.*s)", GetInsertArgStringChars( nInsertable ), GetInsertArgString() );
}


//-----------------------------------------------------------------------------
// Purpose: Builds a SQL INSERT statement
//			IMPORTANT NOTE - This Insert statement will use the Microsoft SQL Server 
//			specific clause 'OUTPUT Inserted.ColumnName' 
//			The result of that will be that the SQL statement will return to us 
//			the columns that could not be specified by the Insert.
//			At the time of writing, that is primarily AutoIncrement columns,
//			however in theory we should be able to recover any computed column 
//			from SQL server, with the caveats specified at : 
//			http://msdn.microsoft.com/en-us/library/ms177564.aspx 
//
// Input:	psStatement - The output statement string
//			pRecordInfo - record info describing table inserting into
//-----------------------------------------------------------------------------

void BuildInsertAndReadStatementText( TSQLCmdStr *psStatement, CUtlVector<int> *pvecOutputFields, const CRecordInfo *pRecordInfo )
{
	psStatement->sprintf("INSERT INTO %s.%s (", GSchemaFull().GetDefaultSchemaNameForCatalog( pRecordInfo->GetESchemaCatalog() ), pRecordInfo->GetName() );

	// build a string of the field names
	int nInsertable = 0;
	int cColumns = pRecordInfo->GetNumColumns();
	bool bAddedBefore = false;
	for ( int iColumn = 0; iColumn < cColumns; iColumn++ )
	{
		const CColumnInfo &columnInfo = pRecordInfo->GetColumnInfo( iColumn );
		if ( !columnInfo.BIsInsertable() )
			continue;

		nInsertable++;

		if ( bAddedBefore )
			psStatement->Append( ',' );
		bAddedBefore = true;
		psStatement->Append( columnInfo.GetName() );
	}

	bAddedBefore = false ;	
	int nOutputColumn = 0;
	for( int iColumn = 0; iColumn < cColumns; iColumn++ )
	{
		const CColumnInfo &columnInfo = pRecordInfo->GetColumnInfo( iColumn ) ;
	
		//
		//	If we can't Insert it - we want SQL Server to tell us what value was stored
		//	in the column !!
		//
		if( !columnInfo.BIsInsertable() )
		{
			if( bAddedBefore )
				psStatement->Append( ", INSERTED." );
			else
				psStatement->Append( ") OUTPUT INSERTED." );
			bAddedBefore = true ;
			psStatement->Append( columnInfo.GetName() );
			pvecOutputFields->AddToTail( iColumn );
			nOutputColumn++;
		}
	}

	// add field values to SQL statement
	psStatement->AppendFormat( " VALUES (%.*s)", GetInsertArgStringChars( nInsertable ), GetInsertArgString() );
}


//-----------------------------------------------------------------------------
// Purpose: Builds a SQL MERGE statement update or insert using in-flight values table
// Input:	psStatement - The string to put the statement into
//			pRecordInfo - record info describing table inserting into
//-----------------------------------------------------------------------------
void BuildMergeStatementTextOnPKWhenMatchedUpdateWhenNotMatchedInsert( TSQLCmdStr *psStatement, const CRecordInfo *pRecordInfo )
{
	psStatement->sprintf( "MERGE INTO %s.%s WITH( HOLDLOCK, ROWLOCK ) T USING ( VALUES (%.*s) ) AS S(",
		GSchemaFull().GetDefaultSchemaNameForCatalog( pRecordInfo->GetESchemaCatalog() ), pRecordInfo->GetName(),
		GetInsertArgStringChars( pRecordInfo->GetNumColumns() ), GetInsertArgString() );

	{
		int cColumns = pRecordInfo->GetNumColumns();
		for ( int iColumn = 0; iColumn < cColumns; iColumn++ )
		{
			const CColumnInfo &columnInfo = pRecordInfo->GetColumnInfo( iColumn );
			if ( iColumn )
				psStatement->Append( ',' );
			psStatement->Append( columnInfo.GetName() );
		}
	}

	psStatement->Append( ") ON " );

	// build a string of the PK columns
	const FieldSet_t &fsPK = pRecordInfo->GetIndexFields()[pRecordInfo->GetPKIndex()];
	{
		int cColumns = fsPK.GetCount();
		for ( int iColumn = 0; iColumn < cColumns; iColumn++ )
		{
			const CColumnInfo &columnInfo = pRecordInfo->GetColumnInfo( fsPK.GetField( iColumn ) );
			if ( iColumn )
				psStatement->Append( " AND " );
			psStatement->Append( "T." );
			psStatement->Append( columnInfo.GetName() );
			psStatement->Append( "=S." );
			psStatement->Append( columnInfo.GetName() );
		}
	}

	psStatement->Append( " WHEN MATCHED THEN UPDATE SET " );

	// build the update string
	{
		int cColumns = pRecordInfo->GetNumColumns();
		bool bAddedBefore = false;
		for ( int iColumn = 0; iColumn < cColumns; iColumn++ )
		{
			bool bThisColumnIsPartOfPK = false;
			for ( int ipkCheck = 0; ipkCheck < fsPK.GetCount(); ++ipkCheck )
			{
				if ( iColumn == fsPK.GetField( ipkCheck ) )
				{
					bThisColumnIsPartOfPK = true;
					break;
				}
			}
			if ( bThisColumnIsPartOfPK )
				continue;

			const CColumnInfo &columnInfo = pRecordInfo->GetColumnInfo( iColumn );
			if ( bAddedBefore )
				psStatement->Append( ',' );
			bAddedBefore = true;
			psStatement->Append( columnInfo.GetName() );
			psStatement->Append( "=S." );
			psStatement->Append( columnInfo.GetName() );
		}
	}

	psStatement->Append( " WHEN NOT MATCHED BY TARGET THEN INSERT (" );

	// build a string of the field names
	{
		int cColumns = pRecordInfo->GetNumColumns();
		bool bAddedBefore = false;
		for ( int iColumn = 0; iColumn < cColumns; iColumn++ )
		{
			const CColumnInfo &columnInfo = pRecordInfo->GetColumnInfo( iColumn );
			if ( !columnInfo.BIsInsertable() )
				continue;

			if ( bAddedBefore )
				psStatement->Append( ',' );
			bAddedBefore = true;
			psStatement->Append( columnInfo.GetName() );
		}
	}

	psStatement->Append( ") VALUES (" );
	{
		int cColumns = pRecordInfo->GetNumColumns();
		bool bAddedBefore = false;
		for ( int iColumn = 0; iColumn < cColumns; iColumn++ )
		{
			const CColumnInfo &columnInfo = pRecordInfo->GetColumnInfo( iColumn );
			if ( !columnInfo.BIsInsertable() )
				continue;

			if ( bAddedBefore )
				psStatement->Append( ',' );
			bAddedBefore = true;
			psStatement->Append( "S." );
			psStatement->Append( columnInfo.GetName() );
		}
	}
	psStatement->Append( ");" );
}


//-----------------------------------------------------------------------------
// Purpose: Builds a SQL MERGE statement using CTE_MergeParams as supplied table holding rows
// Input:	psStatement - The string to put the statement into
//			pRecordInfo - record info describing table inserting into
//-----------------------------------------------------------------------------
void BuildMergeStatementTextOnPKWhenNotMatchedInsert( TSQLCmdStr *psStatement, const CRecordInfo *pRecordInfo )
{
	psStatement->sprintf( "MERGE INTO %s.%s WITH( HOLDLOCK, ROWLOCK ) T USING ( VALUES (%.*s) ) AS S(",
		GSchemaFull().GetDefaultSchemaNameForCatalog( pRecordInfo->GetESchemaCatalog() ), pRecordInfo->GetName(),
		GetInsertArgStringChars( pRecordInfo->GetNumColumns() ), GetInsertArgString() );

	{
		int cColumns = pRecordInfo->GetNumColumns();
		for ( int iColumn = 0; iColumn < cColumns; iColumn++ )
		{
			const CColumnInfo &columnInfo = pRecordInfo->GetColumnInfo( iColumn );
			if ( iColumn )
				psStatement->Append( ',' );
			psStatement->Append( columnInfo.GetName() );
		}
	}

	psStatement->Append( ") ON " );

	// build a string of the PK columns
	const FieldSet_t &fsPK = pRecordInfo->GetIndexFields()[pRecordInfo->GetPKIndex()];
	{
		int cColumns = fsPK.GetCount();
		for ( int iColumn = 0; iColumn < cColumns; iColumn++ )
		{
			const CColumnInfo &columnInfo = pRecordInfo->GetColumnInfo( fsPK.GetField( iColumn ) );
			if ( iColumn )
				psStatement->Append( " AND " );
			psStatement->Append( "T." );
			psStatement->Append( columnInfo.GetName() );
			psStatement->Append( "=S." );
			psStatement->Append( columnInfo.GetName() );
		}
	}

	psStatement->Append( " WHEN NOT MATCHED BY TARGET THEN INSERT (" );

	// build a string of the field names
	{
		int cColumns = pRecordInfo->GetNumColumns();
		bool bAddedBefore = false;
		for ( int iColumn = 0; iColumn < cColumns; iColumn++ )
		{
			const CColumnInfo &columnInfo = pRecordInfo->GetColumnInfo( iColumn );
			if ( !columnInfo.BIsInsertable() )
				continue;

			if ( bAddedBefore )
				psStatement->Append( ',' );
			bAddedBefore = true;
			psStatement->Append( columnInfo.GetName() );
		}
	}

	psStatement->Append( ") VALUES (" );
	{
		int cColumns = pRecordInfo->GetNumColumns();
		bool bAddedBefore = false;
		for ( int iColumn = 0; iColumn < cColumns; iColumn++ )
		{
			const CColumnInfo &columnInfo = pRecordInfo->GetColumnInfo( iColumn );
			if ( !columnInfo.BIsInsertable() )
				continue;

			if ( bAddedBefore )
				psStatement->Append( ',' );
			bAddedBefore = true;
			psStatement->Append( "S." );
			psStatement->Append( columnInfo.GetName() );
		}
	}
	psStatement->Append( ");" );
}

void BuildSelectStatementText( TSQLCmdStr *psStatement, const CColumnSet & selectSet, const char *pchTopClause )
{
	*psStatement = "SELECT ";

	if( pchTopClause )
	{
		psStatement->Append( pchTopClause );
		psStatement->Append( ' ' );
	}

	// build a string of the field names
	bool bAddedBefore = false;
	FOR_EACH_COLUMN_IN_SET( selectSet, nColumnIndex )
	{
		const CColumnInfo &columnInfo = selectSet.GetColumnInfo( nColumnIndex );
		if ( bAddedBefore )
			psStatement->Append( ',' );
		bAddedBefore = true;
		psStatement->Append( columnInfo.GetName() );
	}

	psStatement->Append( " FROM ");
	psStatement->Append( GSchemaFull().GetDefaultSchemaNameForCatalog( selectSet.GetRecordInfo()->GetESchemaCatalog() ) );
	psStatement->Append( '.' );
	psStatement->Append( selectSet.GetRecordInfo()->GetName() );
}


//-----------------------------------------------------------------------------
// Purpose: Builds a SQL UPDATE statement
// Input:	pRecordInfo - record info describing table inserting into
//			bForPreparedStatement - if true, inserts values as '?' for later
//				binding.  If false, values are inserted in text.
//			pchStatement - pointer to buffer to build statement in
//			cchStatement - size of buffer
//			pSQLRecord - pointer to record with data to update
//			iColumnMatch - column to use for WHERE condition
//			pvMatch - data value to use for WHERE condition
//			cubMatch - size of pvMatch data
//			rgiColumnUpdate - array of column #'s to update
//			ciColumnUpdate - count of column #'s to update
//-----------------------------------------------------------------------------
void BuildUpdateStatementText( TSQLCmdStr *psStatement, const CColumnSet & updateColumns )
{
	// build the UPDATE statement
	psStatement->sprintf( "UPDATE %s.%s SET ", GSchemaFull().GetDefaultSchemaNameForCatalog( updateColumns.GetRecordInfo()->GetESchemaCatalog() ), updateColumns.GetRecordInfo()->GetName() );

	// add each field we're updating to the UPDATE statement
	FOR_EACH_COLUMN_IN_SET( updateColumns, nColumnIndex )
	{
		const CColumnInfo &columnInfo = updateColumns.GetColumnInfo( nColumnIndex );

		if( nColumnIndex > 0 )
			psStatement->Append( ',' );
		psStatement->Append( columnInfo.GetName() );
		psStatement->Append( "=?" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Builds a SQL UPDATE statement
//-----------------------------------------------------------------------------
void BuildDeleteStatementText( TSQLCmdStr *psStatement, const CRecordInfo *pRecordInfo )
{
	psStatement->sprintf( "DELETE FROM %s.%s", GSchemaFull().GetDefaultSchemaNameForCatalog( pRecordInfo->GetESchemaCatalog() ), pRecordInfo->GetName() );
}


//-----------------------------------------------------------------------------
// Purpose: Builds a where clause for the provided fields
//-----------------------------------------------------------------------------
void AppendWhereClauseText( TSQLCmdStr *psClause, const CColumnSet & columnSet )
{
	// add each field we're updating to the UPDATE statement
	FOR_EACH_COLUMN_IN_SET( columnSet, nColumnIndex )
	{
		const CColumnInfo &columnInfo = columnSet.GetColumnInfo( nColumnIndex );

		if( nColumnIndex > 0 )
			psClause->Append( " AND ");
		psClause->Append( columnInfo.GetName() );
		psClause->Append( "=?" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Builds an OUTPUT [fields] INTO [table] for the provided fields/data
//-----------------------------------------------------------------------------
void BuildOutputClauseText( TSQLCmdStr *psClause, const CColumnSet & columnSet )
{
	*psClause = " OUTPUT ";

	FOR_EACH_COLUMN_IN_SET( columnSet, nColumnIndex )
	{
		const CColumnInfo &columnInfo = columnSet.GetColumnInfo( nColumnIndex );

		if( nColumnIndex > 0 )
			psClause->Append( ", ");
		
		psClause->Append( " ? AS " );
		psClause->Append( columnInfo.GetName() );
	}

	psClause->Append( " INTO " );
	psClause->Append( columnSet.GetRecordInfo()->GetName() );
}

////-----------------------------------------------------------------------------
//// Purpose: our own special "upsert" into a column with a uniqueness constraint
////-----------------------------------------------------------------------------
//EResult UpdateOrInsertUnique( CSQLAccess &sqlAccess, int iTable, int iField, CRecordBase *pRecordBase, int iIndexID )
//{
//	// attempt an update - if it fails due to duplicate primary key, they can't use this
//	// url (it's taken) - if it succeeds but affects 0 rows, they didn't have a vanity url
//	// and we need to do an insert (which could again fail due to primary key constraints)
//	int cRecordsUpdated = 0;	
//	bool bRet = sqlAccess.BYieldingUpdateFieldFromRecordWithIndex( iTable, &cRecordsUpdated, iField, pRecordBase, iIndexID );
//	if ( !bRet )
//	{ 
//		// ODBC is the suck - give me Spring JDBC templates, please.
//		if ( sqlAccess.GetLastError()->IsDuplicateInsertAttempt() )  
//		{
//			return k_EResultDuplicateName;
//		}
//		return k_EResultFail;
//	}
//	else if ( 0 == cRecordsUpdated )
//	{
//		// the user didn't have an entry, so insert one.
//		bRet = sqlAccess.BYieldingInsertRecord( iTable, pRecordBase );
//		if ( !bRet )
//		{ 
//			// ODBC is the suck - give me Spring JDBC templates, please.
//			if ( sqlAccess.GetLastError()->IsDuplicateInsertAttempt() )  
//			{
//				return k_EResultDuplicateName;
//			}
//			return k_EResultFail;
//		}
//	}
//	return k_EResultOK;
//}
//

} // namespace GCSDK
