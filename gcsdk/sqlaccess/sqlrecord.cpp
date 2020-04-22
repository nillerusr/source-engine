//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================


#include "stdafx.h"

//#include "sqlaccess.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSQLRecord::CSQLRecord( uint32 unRow, IGCSQLResultSet *pResultSet )
{
	Init( unRow, pResultSet );
}
CSQLRecord::CSQLRecord()
: m_pResultSet( NULL ), m_unRow( 0 )
{
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSQLRecord::~CSQLRecord()
{
}


//-----------------------------------------------------------------------------
// Purpose: Initializes a blank record
// Input:	iTable - table that this record will belong to
//-----------------------------------------------------------------------------
void CSQLRecord::Init( uint32 unRow, IGCSQLResultSet *pResultSet )
{
	if( unRow >= pResultSet->GetRowCount() )
	{
		m_pResultSet = NULL;
		m_unRow = 0;
	}
	else
	{
		m_pResultSet = pResultSet;
		m_unRow = unRow;
	}
}	


//-----------------------------------------------------------------------------
// Purpose: Gets data for a field in this record
// Input:	unColumn - field to get
//			pubField - pointer to get filled in with pointer to data
//			cubField - pointer to get filled in with size of data
// Output:	true if successful, false if data not present
//-----------------------------------------------------------------------------
bool CSQLRecord::BGetColumnData( uint32 unColumn, uint8 **ppubField, int *pcubField )
{
	size_t sz;
	bool bRet = BGetColumnData( unColumn, ppubField, &sz );
	*pcubField = static_cast< int >( sz );
	return bRet;
}

//-----------------------------------------------------------------------------
// Purpose: Gets data for a field in this record
// Input:	unColumn - field to get
//			pubField - pointer to get filled in with pointer to data
//			cubField - pointer to get filled in with size of data
// Output:	true if successful, false if data not present
//-----------------------------------------------------------------------------
bool CSQLRecord::BGetColumnData( uint32 unColumn, uint8 **ppubField, size_t *pcubField )
{
	Assert( ppubField );
	Assert( pcubField );
	*ppubField = NULL;
	*pcubField = 0;

	Assert( m_pResultSet );

	if ( !BValidateColumnIndex( unColumn ) )
		return false;

	*pcubField = 0;
	return m_pResultSet->GetData( m_unRow, unColumn, ppubField, (uint32*)pcubField );
}


//-----------------------------------------------------------------------------
// Purpose: Gets string data for a field in this record
// Input:	unColumn - field to get
//			ppchVal - pointer to pointer to fill in to string data
// Output:	true if successful, false if data not present or not of correct type
//-----------------------------------------------------------------------------
bool CSQLRecord::BGetStringValue( uint32 unColumn, const char **ppchVal )
{
	Assert( ppchVal );
	*ppchVal = NULL;

	uint8 *pubData = NULL;
	int cubData = 0;
	Assert( k_EGCSQLType_String == m_pResultSet->GetColumnType( unColumn ) );
	bool bRet = BGetColumnData( unColumn, &pubData, &cubData );
	if ( bRet )
		*ppchVal = (const char *) pubData;

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Gets string data for a field in this record
// Input:	unColumn - field to get
//			ppchVal - pointer to pointer to fill in to string data
// Output:	true if successful, false if data not present or not of correct type
//-----------------------------------------------------------------------------
bool CSQLRecord::BGetStringValue( uint32 unColumn, CFmtStr1024 *psVal )
{
	Assert( psVal );
	*psVal = "";

	uint8 *pubData = NULL;
	int cubData = 0;
	Assert( k_EGCSQLType_String == m_pResultSet->GetColumnType( unColumn ) );
	bool bRet = BGetColumnData( unColumn, &pubData, &cubData );
	if ( bRet )
		*psVal = (const char *) pubData;

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Gets int data for a field in this record
// Input:	unColumn - field to get
//			pnVal - pointer to fill in with data
// Output:	true if successful, false if data not present or not of correct type
//-----------------------------------------------------------------------------
bool CSQLRecord::BGetIntValue( uint32 unColumn, int *pnVal )
{
	Assert( pnVal );
	*pnVal = 0;

	uint8 *pubData = NULL;
	int cubData = 0;	
	bool bRet = BGetColumnData( unColumn, &pubData, &cubData );
	if ( bRet )
	{
		switch( m_pResultSet->GetColumnType( unColumn ) )
		{
			case k_EGCSQLType_int64:
			{
				int64 ul = *((int64 *)pubData);
				if ( ul >= LONG_MIN && ul <= LONG_MAX )
				{
					*pnVal = (int)ul;
					return true;
				}
				else
				{
					AssertMsg1(false, "GetIntValue tried to catch %lld in an int, which is too small", ul );
					return false;
				}
			}
			break;

			case k_EGCSQLType_int32:
				*pnVal = *((int32 *)pubData);
				return true;

			case k_EGCSQLType_int16:
				*pnVal = *((int16 *)pubData);
				return true;

			case k_EGCSQLType_int8:
				*pnVal = *((int8 *)pubData);
				return true;

			default:
				AssertMsg1(false, "GetIntValue tried to catch a %s, which is the wrong type", PchNameFromEGCSQLType( m_pResultSet->GetColumnType( unColumn ) ) );
				return false;
		}
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Gets float data for a field in this record
// Input:	unColumn - field to get
//			pnVal - pointer to fill in with data
// Output:	true if successful, false if data not present or not of correct type
//-----------------------------------------------------------------------------
bool CSQLRecord::BGetFloatValue( uint32 unColumn, float *pfVal )
{
	Assert( pfVal );
	*pfVal = 0.0f;

	uint8 *pubData = NULL;
	int cubData = 0;	
	bool bRet = BGetColumnData( unColumn, &pubData, &cubData );
	if ( bRet )
	{
		Assert( k_EGCSQLType_float == m_pResultSet->GetColumnType( unColumn ) );
		AssertMsg2( sizeof( float ) == cubData, "GetValue expected %llu bytes, found %d", (uint64)sizeof( float ), cubData );
		if ( sizeof( float ) != cubData )
			return false;
		*pfVal =  *( (float *) pubData );
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Gets double data for a field in this record
// Input:	unColumn - field to get
//			pnVal - pointer to fill in with data
// Output:	true if successful, false if data not present or not of correct type
//-----------------------------------------------------------------------------
bool CSQLRecord::BGetDoubleValue( uint32 unColumn, double *pdVal )
{
	Assert( pdVal );
	*pdVal = 0.0f;

	uint8 *pubData = NULL;
	int cubData = 0;	
	bool bRet = BGetColumnData( unColumn, &pubData, &cubData );
	if ( bRet )
	{
		Assert( k_EGCSQLType_double == m_pResultSet->GetColumnType( unColumn ) );
		AssertMsg2( sizeof( double ) == cubData, "GetValue expected %llu bytes, found %d", (uint64)sizeof( double ), cubData );
		if ( sizeof( double ) != cubData )
			return false;
		*pdVal =  *( (double *) pubData );
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Gets int data for a field in this record
// Input:	unColumn - field to get
//			pVal - pointer to fill in with data
// Output:	true if successful, false if data not present or not of correct type
//-----------------------------------------------------------------------------
bool CSQLRecord::BGetByteValue( uint32 unColumn, byte *pVal )
{
	Assert( pVal );
	*pVal = 0;

	uint8 *pubData = NULL;
	int cubData = 0;	
	bool bRet = BGetColumnData( unColumn, &pubData, &cubData );
	if ( bRet )
	{
		Assert( k_EGCSQLType_int8 == m_pResultSet->GetColumnType( unColumn ) );
		AssertMsg1( 1 == cubData, "GetValue expected 1 bytes, found %d", cubData );
		if ( 1 != cubData )
			return false;
		*pVal =  *( (byte *) pubData );
	}

	return bRet;
}

//-----------------------------------------------------------------------------
// Purpose: Gets int data for a field in this record
// Input:	unColumn - field to get
//			pVal - pointer to fill in with data
// Output:	true if successful, false if data not present or not of correct type
//-----------------------------------------------------------------------------
bool CSQLRecord::BGetBoolValue( uint32 unColumn, bool *pVal )
{
	int32 b;
	if ( !BGetIntValue( unColumn, &b ) )
		return false;

	// convert to boolean
	*pVal = ( b != 0 );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Gets int16 data for a field in this record
// Input:	unColumn - field to get
//			pnVal - pointer to fill in with data
// Output:	true if successful, false if data not present or not of correct type
//-----------------------------------------------------------------------------
bool CSQLRecord::BGetInt16Value( uint32 unColumn, int16 *pnVal )
{
	Assert( pnVal );
	*pnVal = 0;

	uint8 *pubData = NULL;
	int cubData = 0;	
	bool bRet = BGetColumnData( unColumn, &pubData, &cubData );
	if ( bRet )
	{
		Assert( k_EGCSQLType_int16 == m_pResultSet->GetColumnType( unColumn ) );
		AssertMsg1( 2 == cubData, "GetValue expected 2 bytes, found %d", cubData );
		if ( 2 != cubData )
			return false;
		*pnVal = *( (int16 *) pubData );
	}
	return bRet;	
}


//-----------------------------------------------------------------------------
// Purpose: Gets int64 data for a field in this record
// Input:	unColumn - field to get
//			puVal - pointer to fill in with data
// Output:	true if successful, false if data not present or not of correct type
//-----------------------------------------------------------------------------
bool CSQLRecord::BGetInt64Value( uint32 unColumn, int64 *puVal )
{
	Assert( puVal );
	*puVal = 0;

	uint8 *pubData = NULL;
	int cubData = 0;	
	bool bRet = BGetColumnData( unColumn, &pubData, &cubData );
	if ( bRet )
	{
		Assert( k_EGCSQLType_int64 == m_pResultSet->GetColumnType( unColumn ) );
		AssertMsg1( 8 == cubData, "GetValue expected 8 bytes, found %d", cubData );
		if ( 8 != cubData )
			return false;
		*puVal = *( (int64 *) pubData );
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Gets uint64 data for a field in this record
// Input:	unColumn - field to get
//			puVal - pointer to fill in with data
// Output:	true if successful, false if data not present or not of correct type
//-----------------------------------------------------------------------------
bool CSQLRecord::BGetUint64Value( uint32 unColumn, uint64 *puVal )
{
	Assert( puVal );
	*puVal = 0;

	uint8 *pubData = NULL;
	int cubData = 0;	
	bool bRet = BGetColumnData( unColumn, &pubData, &cubData );
	if ( bRet )
	{
		Assert( k_EGCSQLType_int64 == m_pResultSet->GetColumnType( unColumn ) );
		AssertMsg1( 8 == cubData, "GetValue expected 8 bytes, found %d", cubData );
		if ( 8 != cubData )
			return false;
		*puVal = *( (uint64 *) pubData );
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Gets uint32 data for a field in this record
// Input:	unColumn - field to get
//			puVal - pointer to fill in with data
// Output:	true if successful, false if data not present or not of correct type
//-----------------------------------------------------------------------------
bool CSQLRecord::BGetUint32Value( uint32 unColumn, uint32 *puVal )
{
	Assert( puVal );
	*puVal = 0;

	uint8 *pubData = NULL;
	int cubData = 0;	
	bool bRet = BGetColumnData( unColumn, &pubData, &cubData );
	if ( bRet )
	{
		Assert( k_EGCSQLType_int32 == m_pResultSet->GetColumnType( unColumn ) );
		AssertMsg1( 4 == cubData, "GetValue expected 4 bytes, found %d", cubData );
		if ( 4 != cubData )
			return false;
		*puVal = *( (uint32 *) pubData );
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Gets uint16 data for a field in this record
// Input:	unColumn - field to get
//			puVal - pointer to fill in with data
// Output:	true if successful, false if data not present or not of correct type
//-----------------------------------------------------------------------------
bool CSQLRecord::BGetUint16Value( uint32 unColumn, uint16 *puVal )
{
	Assert( puVal );
	*puVal = 0;

	uint8 *pubData = NULL;
	int cubData = 0;	
	bool bRet = BGetColumnData( unColumn, &pubData, &cubData );
	if ( bRet )
	{
		Assert( k_EGCSQLType_int16 == m_pResultSet->GetColumnType( unColumn ) );
		AssertMsg1( 2 == cubData, "GetValue expected 2 bytes, found %d", cubData );
		if ( 2 != cubData )
			return false;
		*puVal = *( (uint16 *) pubData );
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Gets uint8 data for a field in this record
// Input:	unColumn - field to get
//			puVal - pointer to fill in with data
// Output:	true if successful, false if data not present or not of correct type
//-----------------------------------------------------------------------------
bool CSQLRecord::BGetUint8Value( uint32 unColumn, uint8 *puVal )
{
	Assert( puVal );
	*puVal = 0;

	uint8 *pubData = NULL;
	int cubData = 0;	
	bool bRet = BGetColumnData( unColumn, &pubData, &cubData );
	if ( bRet )
	{
		Assert( k_EGCSQLType_int8 == m_pResultSet->GetColumnType( unColumn ) );
		AssertMsg1( 1 == cubData, "GetValue expected 1 byte, found %d", cubData );
		if ( 1 != cubData )
			return false;
		*puVal = *( (uint8 *) pubData );
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Validates column index
// Input:	unColumn - field to validate
// Output:	true if valid, false otherwise
//-----------------------------------------------------------------------------
bool CSQLRecord::BValidateColumnIndex( uint32 unColumn )
{
	if (  unColumn >= m_pResultSet->GetColumnCount() )
	{
		AssertMsg2( false, "CSQLRecord::BValidateColumnIndex: invalid column index %d.  # columns: %d", unColumn,
			m_pResultSet->GetColumnCount() );
		return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Advances the CSQLRecord to the next row 
// Output:	returns false if this call would advance the record past the last row.
//			And makes the record invalid.
//-----------------------------------------------------------------------------
bool CSQLRecord::NextRow()
{
	Assert( m_pResultSet );
	m_unRow++;
	
	if( m_unRow >= m_pResultSet->GetRowCount() )
		m_pResultSet = NULL;
	return IsValid();
}


//-----------------------------------------------------------------------------
// Purpose: Render a field to a buffer
// Input:	unColumn - field to render
//			cchBuffer - size of render buffer
//			pchBuffer - buffer to render into
//-----------------------------------------------------------------------------
void CSQLRecord::RenderField( uint32 unColumn, int cchBuffer, char *pchBuffer )
{
	Q_strncpy( pchBuffer, "", cchBuffer );

	uint8 *pubData;
	int cubData;
	if ( !BGetColumnData( unColumn, &pubData, &cubData ) )
		return;

	// Get the column info and figure out how to interpret the data
	ConvertFieldToText( m_pResultSet->GetColumnType( unColumn ), pubData, cubData, pchBuffer, cchBuffer, false );
}


//-----------------------------------------------------------------------------
// Purpose: Copies a CSQLRecord to CRecordBase
//-----------------------------------------------------------------------------
bool CSQLRecord::BWriteToRecord( CRecordBase *pRecord, const CColumnSet & csWriteFields )
{
	bool bSuccess = true;
	FOR_EACH_COLUMN_IN_SET( csWriteFields, unSQLColumn )
	{
		uint32 unRecordColumn = csWriteFields.GetColumn( unSQLColumn );

		uint8 *pubData;
		size_t cubData;
		if( !BGetColumnData( unSQLColumn, &pubData, &cubData ) )
		{
			bSuccess = false;
		}
		else
		{
			bSuccess = pRecord->BSetField( unRecordColumn, pubData, cubData ) && bSuccess;
		}

	}
	return bSuccess;
}



} // namespace GCSDK
