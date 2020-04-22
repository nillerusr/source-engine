//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef IGCSQLRESULTSETLIST_H
#define IGCSQLRESULTSETLIST_H
#ifdef _WIN32
#pragma once
#endif

#include "igcsqlquery.h"

enum EGCSQLError
{
	k_EGCSQLErrorNone = 0,
	k_EGCSQLErrorUnknown,
	k_EGCSQLErrorBacklog,
	k_EGCSQLErrorBadQueryParameters,
	k_EGCSQLErrorConnectionError,
	k_EGCSQLErrorDataTruncated,
	k_EGCSQLErrorDeadlockLoser,
	k_EGCSQLErrorDuplicateKey,
	k_EGCSQLErrorGenericError,
	k_EGCSQLErrorNoResultSet,
	k_EGCSQLErrorSyntaxError,
	k_EGCSQLErrorTableOrViewNotFound,
	k_EGCSQLErrorTimeout,
	k_EGCSQLErrorConstraintViolation,
	k_EGCSQLErrorNumericValueOutOfRange,
	k_EGCSQLErrorRollbackFailed,
	k_EGCSQLErrorColumnNotFound,
};


class IGCSQLResultSet
{
public:
	virtual uint32 GetColumnCount() = 0;
	virtual EGCSQLType GetColumnType( uint32 nColumn ) = 0;
	virtual const char *GetColumnName( uint32 nColumn ) = 0;

	virtual uint32 GetRowCount() = 0;
	virtual bool GetData( uint32 unRow, uint32 unColumn, uint8 **ppData, uint32 *punSize ) = 0;
};


class IGCSQLResultSetList
{
public:
	virtual EGCSQLError GetError() = 0;
	virtual uint32 GetResultSetCount() = 0;
	virtual IGCSQLResultSet *GetResultSet( uint32 nResultSetIndex ) = 0;
	virtual uint32 GetRowsAffected( uint32 unWhichStatement ) = 0;
	virtual void Destroy() = 0;
	virtual const char *GetErrorText() = 0;
};


#endif // IGCSQLRESULTSETLIST_H
