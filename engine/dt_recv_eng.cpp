//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "dt.h"
#include "dt_recv_eng.h"
#include "dt_encode.h"
#include "dt_instrumentation.h"
#include "dt_stack.h"
#include "utllinkedlist.h"
#include "tier0/dbg.h"
#include "dt_recv_decoder.h"
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "dt_common_eng.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


class CClientSendTable;


// Testing out this pattern.. you can write simple code blocks inside of
// codeToRun. The thing that sucks is that you can't access your function's
// local variables inside of codeToRun.
//
// If it used an iterator class, it could access local function variables,
// but the iterator class might be more trouble than it's worth.
#define FOR_EACH_PROP_R( TableType, pTablePointer, tableCode, propCode )	\
	class CPropVisitor 										\
	{ 														\
		public:												\
		static void Visit_R( TableType *pTable )					\
		{													\
			tableCode;										\
															\
			for ( int i=0; i < pTable->GetNumProps(); i++ )	\
			{												\
				TableType::PropType *pProp = pTable->GetProp( i );	\
															\
				propCode;									\
															\
				if ( pProp->GetType() == DPT_DataTable )	\
					Visit_R( pProp->GetDataTable() );		\
			}												\
		}													\
	};														\
	CPropVisitor::Visit_R( pTablePointer );				

#define SENDPROP_VISIT( pTablePointer, tableCode, propCode )  FOR_EACH_PROP_R( SendTable, pTablePointer, tableCode, propCode )
#define RECVPROP_VISIT( pTablePointer, tableCode, propCode )  FOR_EACH_PROP_R( RecvTable, pTablePointer, tableCode, propCode )
#define SETUP_VISIT() class CDummyClass {} // Workaround for parser bug in VC7.1

// ------------------------------------------------------------------------------------ //
// Globals.
// ------------------------------------------------------------------------------------ //

CUtlLinkedList< RecvTable*, unsigned short > g_RecvTables;
CUtlLinkedList< CRecvDecoder *, unsigned short > g_RecvDecoders;
CUtlLinkedList< CClientSendTable*, unsigned short > g_ClientSendTables;

int g_nPropsDecoded = 0;


// ------------------------------------------------------------------------------------ //
// Static helper functions.
// ------------------------------------------------------------------------------------ //

RecvTable* FindRecvTable( const char *pName )
{
	FOR_EACH_LL( g_RecvTables, i )
	{
		if ( stricmp( g_RecvTables[i]->GetName(), pName ) == 0 )
			return g_RecvTables[i];
	}
	return 0;
}


static CClientSendTable* FindClientSendTable( const char *pName )
{
	FOR_EACH_LL( g_ClientSendTables, i )
	{
		CClientSendTable *pTable = g_ClientSendTables[i];

		if ( stricmp( pTable->GetName(), pName ) == 0 )
			return pTable;
	}

	return NULL;
}


// Find all child datatable properties for the send tables.
bool SetupClientSendTableHierarchy()
{
	FOR_EACH_LL( g_ClientSendTables, iClientTable )
	{
		CClientSendTable *pTable = g_ClientSendTables[iClientTable];
		
		// For each datatable property, find the table it references.
		for ( int iProp=0; iProp < pTable->GetNumProps(); iProp++ )
		{
			CClientSendProp *pClientProp = pTable->GetClientProp( iProp );
			SendProp *pProp = &pTable->m_SendTable.m_pProps[iProp];

			if ( pProp->m_Type == DPT_DataTable )
			{
				const char *pTableName = pClientProp->GetTableName();
				ErrorIfNot( pTableName,
					("SetupClientSendTableHierarchy: missing table name for prop '%s'.", pProp->GetName())
				);

				CClientSendTable *pChild = FindClientSendTable( pTableName );
				if ( !pChild )
				{
					DataTable_Warning( "SetupClientSendTableHierarchy: missing SendTable '%s' (referenced by '%s').\n", pTableName, pTable->GetName() );
					return false;
				}

				pProp->SetDataTable( &pChild->m_SendTable );
			}
		}
	}
	
	return true;
}


static RecvProp* FindRecvProp( RecvTable *pTable, const char *pName )
{
	for ( int i=0; i < pTable->GetNumProps(); i++ )
	{
		RecvProp *pProp = pTable->GetProp( i );

		if ( stricmp( pProp->GetName(), pName ) == 0 )
			return pProp;
	}
	
	return NULL;
}


// See if the RecvProp is fit to receive the SendProp's data.
bool CompareRecvPropToSendProp( const RecvProp *pRecvProp, const SendProp *pSendProp )
{
	while ( 1 )
	{
		ErrorIfNot( pRecvProp && pSendProp,
			("CompareRecvPropToSendProp: missing a property.")
		);

		if ( pRecvProp->GetType() != pSendProp->GetType() || pRecvProp->IsInsideArray() != pSendProp->IsInsideArray() )
		{
			return false;
		}

		if ( pRecvProp->GetType() == DPT_Array )
		{
			if ( pRecvProp->GetNumElements() != pSendProp->GetNumElements() )
				return false;
			
			pRecvProp = pRecvProp->GetArrayProp();
			pSendProp = pSendProp->GetArrayProp();
		}
		else
		{
			return true;
		}
	}
}

struct MatchingProp_t
{
	SendProp	*m_pProp;
	RecvProp	*m_pMatchingRecvProp;

	static bool LessFunc( const MatchingProp_t& lhs, const MatchingProp_t& rhs )
	{
		return lhs.m_pProp < rhs.m_pProp;
	}
};

static bool MatchRecvPropsToSendProps_R( CUtlRBTree< MatchingProp_t, unsigned short >& lookup, char const *sendTableName, SendTable *pSendTable, RecvTable *pRecvTable, bool bAllowMismatches, bool *pAnyMismatches )
{
	for ( int i=0; i < pSendTable->m_nProps; i++ )
	{
		SendProp *pSendProp = &pSendTable->m_pProps[i];

		if ( pSendProp->IsExcludeProp() || pSendProp->IsInsideArray() )
			continue;

		// Find a RecvProp by the same name and type.
		RecvProp *pRecvProp = 0;
		if ( pRecvTable )
			pRecvProp = FindRecvProp( pRecvTable, pSendProp->GetName() );

		if ( pRecvProp )
		{
			if ( !CompareRecvPropToSendProp( pRecvProp, pSendProp ) )
			{
				Warning( "RecvProp type doesn't match server type for %s/%s\n", pSendTable->GetName(), pSendProp->GetName() );
				return false;
			}

			MatchingProp_t info;
			info.m_pProp = pSendProp;
			info.m_pMatchingRecvProp = pRecvProp;

			lookup.Insert( info );
		}
		else
		{
			if ( pAnyMismatches )
			{
				*pAnyMismatches = true;
			}

			Warning( "Missing RecvProp for %s - %s/%s\n", sendTableName, pSendTable->GetName(), pSendProp->GetName() );
			if ( !bAllowMismatches )
			{
				return false;
			}
		}

		// Recurse.
		if ( pSendProp->GetType() == DPT_DataTable )
		{
			if ( !MatchRecvPropsToSendProps_R( lookup, sendTableName, pSendProp->GetDataTable(), FindRecvTable( pSendProp->GetDataTable()->m_pNetTableName ), bAllowMismatches, pAnyMismatches ) )
				return false;
		}
	}

	return true;
}


// ------------------------------------------------------------------------------------ //
// Interface functions.
// ------------------------------------------------------------------------------------ //
bool RecvTable_Init( RecvTable **pTables, int nTables )
{
	SETUP_VISIT();

	for ( int i=0; i < nTables; i++ )
	{
		RECVPROP_VISIT( pTables[i], 
			{
				if ( pTable->IsInMainList() )
					return;
				
				// Shouldn't have a decoder yet.
				ErrorIfNot( !pTable->m_pDecoder,
					("RecvTable_Init: table '%s' has a decoder already.", pTable->GetName()));
				
				pTable->SetInMainList( true );
				g_RecvTables.AddToTail( pTable );
			},

			{}
		);
	}

	return true;
}


void RecvTable_Term( bool clearall /*= true*/ )
{
	DTI_Term();

	SETUP_VISIT();

	FOR_EACH_LL( g_RecvTables, i )
	{
		RECVPROP_VISIT( g_RecvTables[i],
			{
				if ( !pTable->IsInMainList() )
					return;

				pTable->SetInMainList( false );
				pTable->m_pDecoder = 0;
			},

			{}
		);
	}

	if ( clearall )
	{
		g_RecvTables.Purge();
	}
	g_RecvDecoders.PurgeAndDeleteElements();
	g_ClientSendTables.PurgeAndDeleteElements();
}

void RecvTable_FreeSendTable( SendTable *pTable )
{
	for ( int iProp=0; iProp < pTable->m_nProps; iProp++ )
	{
		SendProp *pProp = &pTable->m_pProps[iProp];

		delete [] pProp->m_pVarName;

		if ( pProp->m_pExcludeDTName )
			delete [] pProp->m_pExcludeDTName;
	}

	if ( pTable->m_pProps )
		delete [] pTable->m_pProps;

	delete pTable;
}


SendTable *RecvTable_ReadInfos( bf_read *pBuf, int nDemoProtocol )
{
	SendTable *pTable = new SendTable;

	pTable->m_pNetTableName = pBuf->ReadAndAllocateString();

	// Read the property list.
	pTable->m_nProps = pBuf->ReadUBitLong( PROPINFOBITS_NUMPROPS );
	pTable->m_pProps = pTable->m_nProps ? new SendProp[ pTable->m_nProps ] : NULL;

	for ( int iProp=0; iProp < pTable->m_nProps; iProp++ )
	{
		SendProp *pProp = &pTable->m_pProps[iProp];

		pProp->m_Type = (SendPropType)pBuf->ReadUBitLong( PROPINFOBITS_TYPE );
		pProp->m_pVarName = pBuf->ReadAndAllocateString();

		int nFlagsBits = PROPINFOBITS_FLAGS;
        
		// HACK to playback old demos. SPROP_NUMFLAGBITS was 11, now 13
		// old nDemoProtocol was 2 
		if ( nDemoProtocol == 2 )
		{
			nFlagsBits = 11;
		}

		pProp->SetFlags( pBuf->ReadUBitLong( nFlagsBits ) );

		if ( pProp->m_Type == DPT_DataTable )
		{
			pProp->m_pExcludeDTName = pBuf->ReadAndAllocateString();
		}
		else
		{
			if ( pProp->IsExcludeProp() )
			{
				pProp->m_pExcludeDTName = pBuf->ReadAndAllocateString();
			}
			else if ( pProp->GetType() == DPT_Array )
			{
				pProp->SetNumElements( pBuf->ReadUBitLong( PROPINFOBITS_NUMELEMENTS ) );
			}
			else
			{
				pProp->m_fLowValue = pBuf->ReadBitFloat();
				pProp->m_fHighValue = pBuf->ReadBitFloat();
				pProp->m_nBits = pBuf->ReadUBitLong( PROPINFOBITS_NUMBITS );
			}
		}
	}

	return pTable;
}

bool RecvTable_RecvClassInfos( bf_read *pBuf, bool bNeedsDecoder, int nDemoProtocol )
{
	SendTable *pSendTable = RecvTable_ReadInfos( pBuf, nDemoProtocol );

	if ( !pSendTable )
		return false;

	bool ret = DataTable_SetupReceiveTableFromSendTable( pSendTable, bNeedsDecoder );
		
	RecvTable_FreeSendTable( pSendTable );

	return ret;
}

static void CopySendPropsToRecvProps( 
	CUtlRBTree< MatchingProp_t, unsigned short >& lookup, 
	const CUtlVector<const SendProp*> &sendProps, 
	CUtlVector<const RecvProp*> &recvProps 
	)
{
	recvProps.SetSize( sendProps.Count() );
	for ( int iSendProp=0; iSendProp < sendProps.Count(); iSendProp++ )
	{
		const SendProp *pSendProp = sendProps[iSendProp];
		MatchingProp_t search;
		search.m_pProp = (SendProp *)pSendProp;
		int idx = lookup.Find( search );
		if ( idx == lookup.InvalidIndex() )
		{
			recvProps[iSendProp] = 0;
		}
		else
		{
			recvProps[iSendProp] = lookup[ idx ].m_pMatchingRecvProp;
		}
	}
}

bool RecvTable_CreateDecoders( const CStandardSendProxies *pSendProxies, bool bAllowMismatches, bool *pAnyMismatches )
{
	DTI_Init();

	SETUP_VISIT();

	if ( pAnyMismatches )
	{
		*pAnyMismatches = false;
	}

	// First, now that we've supposedly received all the SendTables that we need,
	// set their datatable child pointers.
	if ( !SetupClientSendTableHierarchy() )
		return false;

	FOR_EACH_LL( g_RecvDecoders, i )
	{
		CRecvDecoder *pDecoder = g_RecvDecoders[i];


		// It should already have been linked to its ClientSendTable.
		Assert( pDecoder->m_pClientSendTable );
		if ( !pDecoder->m_pClientSendTable )
			return false;


		// For each decoder, precalculate the SendTable's flat property list.
		if ( !pDecoder->m_Precalc.SetupFlatPropertyArray() )
			return false;

		CUtlRBTree< MatchingProp_t, unsigned short >	PropLookup( 0, 0, MatchingProp_t::LessFunc );

		// Now match RecvProp with SendProps.
		if ( !MatchRecvPropsToSendProps_R( PropLookup, pDecoder->GetSendTable()->m_pNetTableName, pDecoder->GetSendTable(), FindRecvTable( pDecoder->GetSendTable()->m_pNetTableName ), bAllowMismatches, pAnyMismatches ) )
			return false;
	
		// Now fill out the matching RecvProp array.
		CSendTablePrecalc *pPrecalc = &pDecoder->m_Precalc;
		CopySendPropsToRecvProps( PropLookup, pPrecalc->m_Props, pDecoder->m_Props );
		CopySendPropsToRecvProps( PropLookup, pPrecalc->m_DatatableProps, pDecoder->m_DatatableProps );
	
		DTI_HookRecvDecoder( pDecoder );
	}

	return true;
}


bool RecvTable_Decode( 
	RecvTable *pTable, 
	void *pStruct, 
	bf_read *pIn, 
	int objectID,
	bool updateDTI
	)
{
	CRecvDecoder *pDecoder = pTable->m_pDecoder;
	ErrorIfNot( pDecoder,
		("RecvTable_Decode: table '%s' missing a decoder.", pTable->GetName())
	);

	// While there are properties, decode them.. walk the stack as you go.
	CClientDatatableStack theStack( pDecoder, (unsigned char*)pStruct, objectID );
	
	theStack.Init();
	int iStartBit = 0, nIndexBits = 0, iLastBit = pIn->GetNumBitsRead();
	unsigned int iProp;
	CDeltaBitsReader deltaBitsReader( pIn );
	while ( (iProp = deltaBitsReader.ReadNextPropIndex()) < MAX_DATATABLE_PROPS )
	{
		theStack.SeekToProp( iProp );
		
		const RecvProp *pProp = pDecoder->GetProp( iProp );

		// Instrumentation (store the # bits for the prop index).
		if ( g_bDTIEnabled )
		{
			iStartBit = pIn->GetNumBitsRead();
			nIndexBits = iStartBit - iLastBit;
		}

		DecodeInfo decodeInfo;
		decodeInfo.m_pStruct = theStack.GetCurStructBase();
		
		if ( pProp )
		{
			decodeInfo.m_pData = theStack.GetCurStructBase() + pProp->GetOffset();
		}
		else
		{
			// They're allowed to be missing props here if they're playing back a demo.
			// This allows us to change the datatables and still preserve old demos.
			decodeInfo.m_pData = NULL;
		}

		decodeInfo.m_pRecvProp = theStack.IsCurProxyValid() ? pProp : NULL; // Just skip the data if the proxies are screwed.
		decodeInfo.m_pProp = pDecoder->GetSendProp( iProp );
		decodeInfo.m_pIn = pIn;
		decodeInfo.m_ObjectID = objectID;

		g_PropTypeFns[ decodeInfo.m_pProp->GetType() ].Decode( &decodeInfo );
		++g_nPropsDecoded;

		// Instrumentation (store # bits for the encoded property).
		if ( updateDTI && g_bDTIEnabled )
		{
			iLastBit = pIn->GetNumBitsRead();
			DTI_HookDeltaBits( pDecoder, iProp, iLastBit - iStartBit, nIndexBits );
		}
	}
	
	return !pIn->IsOverflowed();	
}


void RecvTable_DecodeZeros( RecvTable *pTable, void *pStruct, int objectID )
{
	CRecvDecoder *pDecoder = pTable->m_pDecoder;
	ErrorIfNot( pDecoder,
		("RecvTable_DecodeZeros: table '%s' missing a decoder.", pTable->GetName())
	);

	// While there are properties, decode them.. walk the stack as you go.
	CClientDatatableStack theStack( pDecoder, (unsigned char*)pStruct, objectID );

	theStack.Init();

	for ( int iProp=0; iProp < pDecoder->GetNumProps(); iProp++ )
	{	
		theStack.SeekToProp( iProp );
	
		// They're allowed to be missing props here if they're playing back a demo.
		// This allows us to change the datatables and still preserve old demos.
		const RecvProp *pProp = pDecoder->GetProp( iProp );
		if ( !pProp )
			continue;

		DecodeInfo decodeInfo;
		decodeInfo.m_pStruct = theStack.GetCurStructBase();
		decodeInfo.m_pData = theStack.GetCurStructBase() + pProp->GetOffset();
		decodeInfo.m_pRecvProp = theStack.IsCurProxyValid() ? pProp : NULL; // Just skip the data if the proxies are screwed.
		decodeInfo.m_pProp = pDecoder->GetSendProp( iProp );
		decodeInfo.m_pIn = NULL;
		decodeInfo.m_ObjectID = objectID;

		g_PropTypeFns[pProp->GetType()].DecodeZero( &decodeInfo );
	}
}



int RecvTable_MergeDeltas(
	RecvTable *pTable,

	bf_read *pOldState,		// this can be null
	bf_read *pNewState,

	bf_write *pOut,

	int objectID,
	int *pChangedProps,
	bool updateDTI
	)
{
	ErrorIfNot( pTable && pNewState && pOut,
		("RecvTable_MergeDeltas: invalid parameters passed.")
	);

	CRecvDecoder *pDecoder = pTable->m_pDecoder;
	ErrorIfNot( pDecoder, ("RecvTable_MergeDeltas: table '%s' is missing its decoder.", pTable->GetName()) );

	int nChanged = 0;
	
	// Setup to read the delta bits from each buffer.
	CDeltaBitsReader oldStateReader( pOldState );
	CDeltaBitsReader newStateReader( pNewState );

	// Setup to write delta bits into the output.
	CDeltaBitsWriter deltaBitsWriter( pOut );

	unsigned int iOldProp = ~0u;
	if ( pOldState )
		iOldProp = oldStateReader.ReadNextPropIndex();

	int iStartBit = 0, nIndexBits = 0, iLastBit = pNewState->GetNumBitsRead();

	unsigned int iNewProp = newStateReader.ReadNextPropIndex();
	
	while ( 1 )
	{
		// Write any properties in the previous state that aren't in the new state.
		while ( iOldProp < iNewProp )
		{
			deltaBitsWriter.WritePropIndex( iOldProp );
			oldStateReader.CopyPropData( deltaBitsWriter.GetBitBuf(), pDecoder->GetSendProp( iOldProp ) );
			iOldProp = oldStateReader.ReadNextPropIndex();
		}

		// Check if we're at the end here so the while() statement above can seek the old buffer
		// to its end too.
		if ( iNewProp >= MAX_DATATABLE_PROPS )
			break;
		
		// If the old state has this property too, then just skip over its data.
		if ( iOldProp == iNewProp )
		{
			oldStateReader.SkipPropData( pDecoder->GetSendProp( iOldProp ) );
			iOldProp = oldStateReader.ReadNextPropIndex();
		}

		// Instrumentation (store the # bits for the prop index).
		if ( updateDTI && g_bDTIEnabled )
		{
			iStartBit = pNewState->GetNumBitsRead();
			nIndexBits = iStartBit - iLastBit;
		}

		// Now write the new state's value.
		deltaBitsWriter.WritePropIndex( iNewProp );
		newStateReader.CopyPropData( deltaBitsWriter.GetBitBuf(), pDecoder->GetSendProp( iNewProp ) );
	
		if ( pChangedProps )
		{
			pChangedProps[nChanged] = iNewProp;
		}

		nChanged++;

		// Instrumentation (store # bits for the encoded property).
		if ( updateDTI && g_bDTIEnabled )
		{
			iLastBit = pNewState->GetNumBitsRead();
			DTI_HookDeltaBits( pDecoder, iNewProp, iLastBit - iStartBit, nIndexBits );
		}

		iNewProp = newStateReader.ReadNextPropIndex();
	}

	Assert( nChanged <= MAX_DATATABLE_PROPS );

	ErrorIfNot( 
		!(pOldState && pOldState->IsOverflowed()) && !pNewState->IsOverflowed() && !pOut->IsOverflowed(),
		("RecvTable_MergeDeltas: overflowed in RecvTable '%s'.", pTable->GetName())
		);

	return nChanged;
}


void RecvTable_CopyEncoding( RecvTable *pTable, bf_read *pIn, bf_write *pOut, int objectID )
{
	RecvTable_MergeDeltas( pTable, NULL, pIn, pOut, objectID );
}



