//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DT_RECV_DECODER_H
#define DT_RECV_DECODER_H
#ifdef _WIN32
#pragma once
#endif


#include "dt.h"


class CDTIRecvTable;
class RecvProp;


// ------------------------------------------------------------------------------------ //
// CClientSendTable and CClientSendProp. This is the data that we receive a SendTable into.
//
// For the most part, it's just a SendTable, but we have slots for extra 
// data we need to store.
// ------------------------------------------------------------------------------------ //

class CClientSendProp
{
public:

				CClientSendProp();
				~CClientSendProp();

	const char*	GetTableName()				{ return m_pTableName; }
	void		SetTableName( char *pName )	{ m_pTableName = pName; }
		

private:

	char	*m_pTableName;	// For DPT_DataTable properties.. this tells the table name.
};


//
// CClientSendTables are the client's version of the SendTables that are sent down
// from the server. It stores these, then builds CRecvDecoders that allow it to
// decode packets of data.
//
class CClientSendTable
{
public:
								CClientSendTable();
								~CClientSendTable();
	
	int							GetNumProps() const		{ return m_SendTable.m_nProps; }
	CClientSendProp*			GetClientProp( int i )	{ return &m_Props[i]; }
	
	const char*					GetName()				{ return m_SendTable.GetName(); }
	SendTable*					GetSendTable()			{ return &m_SendTable; }


public:

	SendTable					m_SendTable;
	CUtlVector<CClientSendProp>	m_Props;	// Extra data for the properties.
};


// ------------------------------------------------------------------------------------ //
// CRecvDecoder.
// ------------------------------------------------------------------------------------ //

class CRecvDecoder
{
public:
				
					CRecvDecoder();

	const char*		GetName() const;
	SendTable*		GetSendTable() const;
	RecvTable*		GetRecvTable() const;

	int				GetNumProps() const;
	const RecvProp*	GetProp( int i ) const;
	const SendProp*	GetSendProp( int i ) const;

	int				GetNumDatatableProps() const;
	const RecvProp*	GetDatatableProp( int i ) const;


public:
	
	RecvTable			*m_pTable;
	CClientSendTable	*m_pClientSendTable;

	// This is from the data that we've received from the server.
	CSendTablePrecalc	m_Precalc;

	// This mirrors m_Precalc.m_Props. 
	CUtlVector<const RecvProp*>	m_Props;
	CUtlVector<const RecvProp*>	m_DatatableProps;

	CDTIRecvTable *m_pDTITable;
};


// ------------------------------------------------------------------------------------ //
// Inlines.
// ------------------------------------------------------------------------------------ //

inline const char* CRecvDecoder::GetName() const
{
	return m_pTable->GetName(); 
}

inline SendTable* CRecvDecoder::GetSendTable() const
{
	return m_Precalc.GetSendTable(); 
}

inline RecvTable* CRecvDecoder::GetRecvTable() const
{
	return m_pTable; 
}

inline int CRecvDecoder::GetNumProps() const
{
	return m_Props.Count();
}

inline const RecvProp* CRecvDecoder::GetProp( int i ) const
{
	// When called from RecvTable_Decode it is expected that this will
	// return NULL if 'i' is out of range, but for two years there has been
	// nothing to ensure that this is true. This has caused significant
	// crashes in RecvTable_Decode. Putting the check here rather
	// than in RecvTable_Decode helps check for other functions that might
	// have this same assumption. If the cost is too great then this check
	// can be moved to RecvTable_Decode. Initial testing suggests that this
	// function is called ~1,200 times per second, so the cost of the branch is
	// not significant.
	// Do the check using unsigned math to check for < 0 simultaneously.
	if ( (unsigned)i < (unsigned)GetNumProps() )
		return m_Props[i];
	return NULL;
}

inline const SendProp* CRecvDecoder::GetSendProp( int i ) const
{
	return m_Precalc.GetProp( i );
}

inline int CRecvDecoder::GetNumDatatableProps() const
{
	return m_DatatableProps.Count();
}

inline const RecvProp* CRecvDecoder::GetDatatableProp( int i ) const
{
	return m_DatatableProps[i]; 
}


#endif // DT_RECV_DECODER_H
