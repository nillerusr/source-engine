//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DATATABLE_H
#define DATATABLE_H
#ifdef _WIN32
#pragma once
#endif


#include "dt_common.h"
#include "dt_recv_eng.h"
#include "dt_send_eng.h"
#include "utlvector.h"
#include "dt_encode.h"
#include "utlmap.h"
#include "tier1/bitbuf.h"


class SendTable;
class RecvTable;
class CDTISendTable;



#define MAX_EXCLUDE_PROPS		512


// Bit counts used to encode the information about a property.
#define PROPINFOBITS_NUMPROPS			10
#define PROPINFOBITS_TYPE				5
#define PROPINFOBITS_FLAGS				SPROP_NUMFLAGBITS_NETWORKED
#define PROPINFOBITS_STRINGBUFFERLEN	10
#define PROPINFOBITS_NUMBITS			7
#define PROPINFOBITS_RIGHTSHIFT			6
#define PROPINFOBITS_NUMELEMENTS		10	// For arrays.



class ExcludeProp
{
public:
	char const	*m_pTableName;
	char const	*m_pPropName;
};


// ------------------------------------------------------------------------------------ //
// CDeltaBitsReader.
// ------------------------------------------------------------------------------------ //


class CDeltaBitsReader
{
public:
	CDeltaBitsReader( bf_read *pBuf );
	~CDeltaBitsReader();

	// Write the next property index. Returns the number of bits used.
	unsigned int ReadNextPropIndex();
	unsigned int ReadNextPropIndex_Continued();
	void	SkipPropData( const SendProp *pProp );
	int		ComparePropData( CDeltaBitsReader* pOut, const SendProp *pProp );
	void	CopyPropData( bf_write* pOut, const SendProp *pProp );

	// If you know you're done but you're not at the end (you haven't called until
	// ReadNextPropIndex returns -1), call this so it won't assert in its destructor.
	void		ForceFinished();

private:
	bf_read		*m_pBuf;
	int			m_iLastProp;
};


FORCEINLINE CDeltaBitsReader::CDeltaBitsReader( bf_read *pBuf )
{
	m_pBuf = pBuf;
	m_iLastProp = -1;
}

FORCEINLINE CDeltaBitsReader::~CDeltaBitsReader()
{
	// Make sure they read to the end unless they specifically said they don't care.
	Assert( !m_pBuf );
}

FORCEINLINE void CDeltaBitsReader::ForceFinished()
{
#ifdef DBGFLAG_ASSERT
	m_pBuf = NULL;
#endif
}

FORCEINLINE unsigned int CDeltaBitsReader::ReadNextPropIndex()
{
	Assert( m_pBuf );
	// Expanded and optimized version of
	// if ( m_pBuf->ReadOneBit() )
	// { 
	//    m_iLastProp += 1 + m_pBuf->ReadUBitVar();
	//    return m_iLastProp;
	// }
	// return ~0u;
	if ( m_pBuf->GetNumBitsLeft() >= 7 )
	{
		uint bits = m_pBuf->ReadUBitLong( 7 );
		if ( bits & 1 )
		{
			uint delta = bits >> 3;
			if ( bits & 6 )
			{
				delta = m_pBuf->ReadUBitVarInternal( (bits & 6) >> 1 );
			}
			m_iLastProp = m_iLastProp + 1 + delta;
			Assert( m_iLastProp < MAX_DATATABLE_PROPS );
			return m_iLastProp;
		}
		m_pBuf->m_iCurBit -= 6; // Unread six bits we shouldn't have looked at
	}
	else
	{
		// Not enough bits for a property index.
		if ( m_pBuf->ReadOneBit() )
		{
			// Expected a zero bit! Force an overflow!
			m_pBuf->Seek(-1);
		}
	}
	ForceFinished();
	return ~0u;
}

FORCEINLINE void CDeltaBitsReader::SkipPropData( const SendProp *pProp )
{
	g_PropTypeFns[ pProp->GetType() ].SkipProp( pProp, m_pBuf );
}

FORCEINLINE void CDeltaBitsReader::CopyPropData( bf_write* pOut, const SendProp *pProp )
{
	int start = m_pBuf->GetNumBitsRead();
	g_PropTypeFns[ pProp->GetType() ].SkipProp( pProp, m_pBuf );
	int len = m_pBuf->GetNumBitsRead() - start;
	m_pBuf->Seek( start );
	pOut->WriteBitsFromBuffer( m_pBuf, len );
}

FORCEINLINE int CDeltaBitsReader::ComparePropData( CDeltaBitsReader *pInReader, const SendProp *pProp )
{
	bf_read *pIn = pInReader->m_pBuf;
	return g_PropTypeFns[pProp->m_Type].CompareDeltas( pProp, m_pBuf, pIn );
}


// ------------------------------------------------------------------------------------ //
// CDeltaBitsWriter.
// ------------------------------------------------------------------------------------ //

class CDeltaBitsWriter
{
public:
				CDeltaBitsWriter( bf_write *pBuf );
				~CDeltaBitsWriter();

	// Write the next property index. Returns the number of bits used.
	void		WritePropIndex( int iProp );

	// Access the buffer it's outputting to.
	bf_write*	GetBitBuf();

private:
	bf_write	*m_pBuf;
	int			m_iLastProp;
};

inline CDeltaBitsWriter::CDeltaBitsWriter( bf_write *pBuf )
{
	m_pBuf = pBuf;
	m_iLastProp = -1;
}

inline bf_write* CDeltaBitsWriter::GetBitBuf()
{
	return m_pBuf;
}

FORCEINLINE void CDeltaBitsWriter::WritePropIndex( int iProp )
{
	Assert( iProp >= 0 && iProp < MAX_DATATABLE_PROPS );
	unsigned int diff = iProp - m_iLastProp;
	m_iLastProp = iProp;
	Assert( diff > 0 && diff <= MAX_DATATABLE_PROPS );
	// Expanded inline for maximum efficiency.
	//m_pBuf->WriteOneBit( 1 );
	//m_pBuf->WriteUBitVar( diff - 1 );
	COMPILE_TIME_ASSERT( MAX_DATATABLE_PROPS <= 0x1000u );
	int n = ((diff < 0x11u) ? -1 : 0) + ((diff < 0x101u) ? -1 : 0);
	m_pBuf->WriteUBitLong( diff*8 - 8 + 4 + n*2 + 1, 8 + n*4 + 4 + 2 + 1 );
}

inline CDeltaBitsWriter::~CDeltaBitsWriter()
{
	m_pBuf->WriteOneBit( 0 );
}


// ----------------------------------------------------------------------------- //
// 
// CSendNode
//
// Each datatable gets a tree of CSendNodes. There is one CSendNode
// for each datatable property that was in the original SendTable.
//
// ----------------------------------------------------------------------------- //

class CSendNode
{
public:

					CSendNode();
					~CSendNode();

	int				GetNumChildren() const;
	CSendNode*		GetChild( int i ) const;
	
	
	// Returns true if the specified prop is in this node or any of its children.
	bool			IsPropInRecursiveProps( int i ) const;

	// Each datatable property (without SPROP_PROXY_ALWAYS_YES set) gets a unique index here.
	// The engine stores arrays of CSendProxyRecipients with the results of the proxies and indexes the results
	// with this index.
	//
	// Returns DATATABLE_PROXY_INDEX_NOPROXY if the property has SPROP_PROXY_ALWAYS_YES set.
	unsigned short	GetDataTableProxyIndex() const;
	void			SetDataTableProxyIndex( unsigned short val );

	// Similar to m_DataTableProxyIndex, but doesn't use DATATABLE_PROXY_INDEX_INVALID,
	// so this can be used to index CDataTableStack::m_pProxies. 
	unsigned short	GetRecursiveProxyIndex() const;
	void			SetRecursiveProxyIndex( unsigned short val );


public:

	// Child datatables.
	CUtlVector<CSendNode*>	m_Children;

	// The datatable property that leads us to this CSendNode.
	// This indexes the CSendTablePrecalc or CRecvDecoder's m_DatatableProps list.
	// The root CSendNode sets this to -1.
	short					m_iDatatableProp;

	// The SendTable that this node represents.
	// ALL CSendNodes have this.
	const SendTable	*m_pTable;

	//
	// Properties in this table.
	//

	// m_iFirstRecursiveProp to m_nRecursiveProps defines the list of propertise
	// of this node and all its children.
	unsigned short	m_iFirstRecursiveProp;
	unsigned short	m_nRecursiveProps;


	// See GetDataTableProxyIndex().
	unsigned short	m_DataTableProxyIndex;
	
	// See GetRecursiveProxyIndex().
	unsigned short	m_RecursiveProxyIndex;
};


inline int CSendNode::GetNumChildren() const
{
	return m_Children.Count(); 
}

inline CSendNode* CSendNode::GetChild( int i ) const
{
	return m_Children[i];
}


inline bool CSendNode::IsPropInRecursiveProps( int i ) const
{
	int index = i - (int)m_iFirstRecursiveProp;
	return index >= 0 && index < m_nRecursiveProps;
}

inline unsigned short CSendNode::GetDataTableProxyIndex() const
{
	Assert( m_DataTableProxyIndex != DATATABLE_PROXY_INDEX_INVALID );	// Make sure it's been set before.
	return m_DataTableProxyIndex;
}

inline void CSendNode::SetDataTableProxyIndex( unsigned short val )
{
	m_DataTableProxyIndex = val;
}

inline unsigned short CSendNode::GetRecursiveProxyIndex() const
{
	return m_RecursiveProxyIndex;
}

inline void CSendNode::SetRecursiveProxyIndex( unsigned short val )
{
	m_RecursiveProxyIndex = val;
}



class CFastLocalTransferPropInfo
{
public:
	unsigned short	m_iRecvOffset;
	unsigned short	m_iSendOffset;
	unsigned short	m_iProp;
};


class CFastLocalTransferInfo
{
public:
	CUtlVector<CFastLocalTransferPropInfo> m_FastInt32;
	CUtlVector<CFastLocalTransferPropInfo> m_FastInt16;
	CUtlVector<CFastLocalTransferPropInfo> m_FastInt8;
	CUtlVector<CFastLocalTransferPropInfo> m_FastVector;
	CUtlVector<CFastLocalTransferPropInfo> m_OtherProps;	// Props that must be copied slowly (proxies and all).
};


// ----------------------------------------------------------------------------- //
// CSendTablePrecalc
// ----------------------------------------------------------------------------- //
class CSendTablePrecalc
{
public:
						CSendTablePrecalc();
	virtual				~CSendTablePrecalc();

	// This function builds the flat property array given a SendTable.
	bool				SetupFlatPropertyArray();

	int					GetNumProps() const;
	const SendProp*		GetProp( int i ) const;

	int					GetNumDatatableProps() const;
	const SendProp*		GetDatatableProp( int i ) const;

	SendTable*			GetSendTable() const;
	CSendNode*			GetRootNode();

	int			GetNumDataTableProxies() const;
	void		SetNumDataTableProxies( int count );


public:

	class CProxyPathEntry
	{
	public:
		unsigned short m_iDatatableProp;	// Lookup into CSendTablePrecalc or CRecvDecoder::m_DatatableProps.
		unsigned short m_iProxy;
	};
	class CProxyPath
	{
	public:
		unsigned short m_iFirstEntry;	// Index into m_ProxyPathEntries.
		unsigned short m_nEntries;
	};
	
	CUtlVector<CProxyPathEntry> m_ProxyPathEntries;	// For each proxy index, this is all the DT proxies that generate it.
	CUtlVector<CProxyPath> m_ProxyPaths;			// CProxyPathEntries lookup into this.
	
	// These are what CSendNodes reference.
	// These are actual data properties (ints, floats, etc).
	CUtlVector<const SendProp*>	m_Props;

	// Each datatable in a SendTable's tree gets a proxy index, and its properties reference that.
	CUtlVector<unsigned char> m_PropProxyIndices;
	
	// CSendNode::m_iDatatableProp indexes this.
	// These are the datatable properties (SendPropDataTable).
	CUtlVector<const SendProp*>	m_DatatableProps;

	// This is the property hierarchy, with the nodes indexing m_Props.
	CSendNode				m_Root;

	// From whence we came.
	SendTable				*m_pSendTable;

	// For instrumentation.
	CDTISendTable			*m_pDTITable;

	// This is precalculated in single player to allow faster direct copying of the entity data
	// from the server entity to the client entity.
	CFastLocalTransferInfo	m_FastLocalTransfer;

	// This tells how many data table properties there are without SPROP_PROXY_ALWAYS_YES.
	// Arrays allocated with this size can be indexed by CSendNode::GetDataTableProxyIndex().
	int						m_nDataTableProxies;
	
	// Map prop offsets to indices for properties that can use it.
	CUtlMap<unsigned short, unsigned short> m_PropOffsetToIndexMap;
};


inline int CSendTablePrecalc::GetNumProps() const
{
	return m_Props.Count(); 
}

inline const SendProp* CSendTablePrecalc::GetProp( int i ) const
{
	return m_Props[i]; 
}

inline int CSendTablePrecalc::GetNumDatatableProps() const
{
	return m_DatatableProps.Count();
}

inline const SendProp* CSendTablePrecalc::GetDatatableProp( int i ) const
{
	return m_DatatableProps[i];
}

inline SendTable* CSendTablePrecalc::GetSendTable() const
{
	return m_pSendTable; 
}

inline CSendNode* CSendTablePrecalc::GetRootNode()
{
	return &m_Root; 
}

inline int CSendTablePrecalc::GetNumDataTableProxies() const
{
	return m_nDataTableProxies;
}


inline void CSendTablePrecalc::SetNumDataTableProxies( int count )
{
	m_nDataTableProxies = count;
}					   


// ------------------------------------------------------------------------ //
// Helpers.
// ------------------------------------------------------------------------ //

// Used internally by various datatable modules.
void DataTable_Warning( PRINTF_FORMAT_STRING const char *pInMessage, ... ) FMTFUNCTION( 1, 2 );
bool ShouldWatchThisProp( const SendTable *pTable, int objectID, const char *pPropName );

// Same as AreBitArraysEqual but does a trivial test to make sure the 
// two arrays are equally sized.
bool CompareBitArrays(
	void const *pPacked1,
	void const *pPacked2,
	int nBits1,
	int nBits2
	);

// to skip of a Property we just IsEncodedZero to read over it
// this is faster then doing a full Decode()
inline void SkipPropData( bf_read *pIn, const SendProp *pProp )
{
	g_PropTypeFns[ pProp->GetType() ].SkipProp( pProp, pIn );
}

// This is to be called on SendTables and RecvTables to setup array properties
// to point at their property templates and to set the SPROP_INSIDEARRAY flag
// on the properties inside arrays.
// We make the proptype an explicit template parameter because
// gcc templating cannot deduce typedefs from classes in templates properly

template< class TableType, class PropType >
void SetupArrayProps_R( TableType *pTable )
{
	// If this table has already been initialized in here, then jump out.
	if ( pTable->IsInitialized() )
		return;

	pTable->SetInitialized( true );

	for ( int i=0; i < pTable->GetNumProps(); i++ )
	{
		PropType *pProp = pTable->GetProp( i );

		if ( pProp->GetType() == DPT_Array )
		{
			ErrorIfNot( i >= 1,
				("SetupArrayProps_R: array prop '%s' is at index zero.", pProp->GetName())
			);

			// Get the property defining the elements in the array.
			PropType *pArrayProp = pTable->GetProp( i-1 );
			pArrayProp->SetInsideArray();
			pProp->SetArrayProp( pArrayProp );
		}
		else if ( pProp->GetType() == DPT_DataTable )
		{
			// Recurse into children datatables.
			SetupArrayProps_R<TableType,PropType>( pProp->GetDataTable() );
		}
	}
}


#endif // DATATABLE_H
