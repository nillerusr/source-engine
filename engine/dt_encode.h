//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DATATABLE_ENCODE_H
#define DATATABLE_ENCODE_H
#ifdef _WIN32
#pragma once
#endif


#include "dt_send.h"
#include "dt_recv.h"


class DecodeInfo : public CRecvProxyData
{
public:
		
	// Copy everything except val.
	void			CopyVars( const DecodeInfo *pOther );

public:
	
	//
	// NOTE: it's valid to pass in m_pRecvProp and m_pData and m_pSrtuct as null, in which 
	// case the buffer is advanced but the property is not stored anywhere. 
	//
	// This is used by SendTable_CompareDeltas.
	//
	void			*m_pStruct;			// Points at the base structure
	void			*m_pData;			// Points at where the variable should be encoded. 

	const SendProp 	*m_pProp;		// Provides the client's info on how to decode and its proxy.
	bf_read			*m_pIn;			// The buffer to get the encoded data from.

	char			m_TempStr[DT_MAX_STRING_BUFFERSIZE];	// m_Value.m_pString is set to point to this.
};


typedef struct
{
	// Encode a value.
	// pStruct : points at the base structure
	// pVar    : holds data in the correct type (ie: PropVirtualsInt will have DVariant::m_Int set).
	// pProp   : describes the property to be encoded.
	// pOut    : the buffer to encode into.
	// objectID: for debug output.
	void			(*Encode)( const unsigned char *pStruct, DVariant *pVar, const SendProp *pProp, bf_write *pOut, int objectID );

	// Decode a value.
	// See the DecodeInfo class for a description of the parameters.
	void			(*Decode)( DecodeInfo *pInfo );

	// Compare the deltas in the two buffers. The property in both buffers must be fully decoded
	int				(*CompareDeltas)( const SendProp *pProp, bf_read *p1, bf_read *p2 );

	// Used for the local single-player connection to copy the data straight from the server ent into the client ent.
	void			(*FastCopy)( 
		const SendProp *pSendProp, 
		const RecvProp *pRecvProp, 
		const unsigned char *pSendData, 
		unsigned char *pRecvData, 
		int objectID );

	// Return a string with the name of the type ("DPT_Float", "DPT_Int", etc).
	const char*		(*GetTypeNameString)();

	// Returns true if the property's value is zero.
	// NOTE: this does NOT strictly mean that it would encode to zeros. If it were a float with
	// min and max values, a value of zero could encode to some other integer value.
	bool			(*IsZero)( const unsigned char *pStruct, DVariant *pVar, const SendProp *pProp );

	// This writes a zero value in (ie: a value that would make IsZero return true).
	void			(*DecodeZero)( DecodeInfo *pInfo );
	
	// This reades this property from stream p and returns true, if it's a zero value
	bool			(*IsEncodedZero) ( const SendProp *pProp, bf_read *p );
	void			(*SkipProp) ( const SendProp *pProp, bf_read *p );
} PropTypeFns;


extern PropTypeFns g_PropTypeFns[DPT_NUMSendPropTypes];


// This is used for comparing packed buffers. Just extracts the raw bits for the 
// data and returns the number of bits used to encode the data.
int	DecodeBits( DecodeInfo *pInfo, unsigned char *pOut );


#endif // DATATABLE_ENCODE_H
