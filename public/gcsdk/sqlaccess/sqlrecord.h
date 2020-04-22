//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef GCSQLRECORD_H
#define GCSQLRECORD_H

namespace GCSDK
{

class CSQLRecord
{
public:
	CSQLRecord( uint32 unRow, IGCSQLResultSet *pResultSet );
	CSQLRecord(  );
	~CSQLRecord();

	void Init( uint32 unRow, IGCSQLResultSet *pResultSet );

	bool BWriteToRecord( CRecordBase *pRecord, const CColumnSet & csWriteFields );
	bool BGetColumnData( uint32 unColumn, uint8 **ppubField, int *pcubField );
	bool BGetColumnData( uint32 unColumn, uint8 **ppubField, size_t *pcubField );
	bool BGetStringValue( uint32 unColumn, const char **ppchVal );
	bool BGetStringValue( uint32 unColumn, CFmtStr1024 *psVal );
	bool BGetIntValue( uint32 unColumn, int *pnVal );
	bool BGetInt16Value( uint32 unColumn, int16 *pnVal );
	bool BGetInt64Value( uint32 unColumn, int64 *puVal );
	bool BGetUint64Value( uint32 unColumn, uint64 *puVal );
	bool BGetByteValue( uint32 unColumn, byte *pVal );
	bool BGetBoolValue( uint32 unColumn, bool *pVal );
	bool BGetUint32Value( uint32 unColumn, uint32 *puVal );
	bool BGetUint16Value( uint32 unColumn, uint16 *puVal );
	bool BGetUint8Value( uint32 unColumn, uint8 *puVal );
	bool BGetFloatValue( uint32 unColumn, float *pfVal );
	bool BGetDoubleValue( uint32 unColumn, double *pdVal );

	void RenderField( uint32 unColumn, int cchBuffer, char *pchBuffer );
	
	bool NextRow();
	bool IsValid() const { return m_pResultSet != NULL; }

private:


	bool BValidateColumnIndex( uint32 unColumn );
	IGCSQLResultSet *m_pResultSet;
	uint32 m_unRow;
};

} // namespace GCSDK
#endif // GCSQLRECORD_H
