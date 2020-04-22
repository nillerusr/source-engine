//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef IGCSQLQUERY_H
#define IGCSQLQUERY_H
#ifdef _WIN32
#pragma once
#endif


// the type of the parameter
enum EGCSQLType
{
	k_EGCSQLTypeInvalid = -1,
	
	// Variable length types
	k_EGCSQLType_Blob,
	k_EGCSQLType_String,

	// fixed length types
	k_EGCSQLType_int8,			// also uint8
	k_EGCSQLType_int16,			// also uint16
	k_EGCSQLType_int32,			// also uint32
	k_EGCSQLType_int64,			// also uint64
	k_EGCSQLType_float,
	k_EGCSQLType_double,
	k_EGCSQLType_Binary,	// raw binary data of fixed size (i.e. a C struct).
	k_EGCSQLType_Image,
	k_EGCSQLType_bool,
};

class IGCSQLResultSetList;

class IGCSQLQuery
{
protected:
	// call Destroy() instead of deleting this object directly
	virtual ~IGCSQLQuery() {}

public:
	// returns the number of statements in the transaction 
	// represented by this query object
	virtual uint32 GetStatementCount() = 0;

	// returns a string that represents where in the GC this
	// query came from.  Usually this is FILE_AND_LINE.
	virtual const char *PchName() = 0;

	// get the null-terminated query string itself
	virtual const char *PchCommand( uint32 unStatement ) = 0;

	// gets the parameter data
	virtual uint32 CnParams( uint32 unStatement ) = 0;
	virtual EGCSQLType EParamType( uint32 unStatement, uint32 uIndex ) = 0;
	virtual byte *PubParam( uint32 unStatement, uint32 uIndex ) = 0;
	virtual uint32 CubParam( uint32 unStatement, uint32 uIndex ) = 0;
	
	// reports the result
	virtual void SetResults( IGCSQLResultSetList *pResults ) = 0;
};


#endif // IGCSQLQUERY_H
