//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#if (defined(_WIN32) && (!defined(_X360) ) )
#include <windows.h>
#endif
#include "tier0/platform.h"
#include "tier0/icommandline.h"
#include "dt_instrumentation.h"
#include "utlvector.h"
#include "utllinkedlist.h"
#include "tier0/fasttimer.h"
#include "utllinkedlist.h"
#include "tier0/dbg.h"
#include "tier1/utlstring.h"
#include "dt_recv_decoder.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "cdll_int.h"
#include "client.h"
#include "common.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool g_bDTIEnabled = false;
const char *g_pDTIFilename;


class CDTIProp
{
public:
	CDTIProp()
	{
		m_nDecodes = m_nDataBits = m_nIndexBits = 0;
	}

	CUtlString m_Name;
	int m_nDecodes;
	int m_nDataBits;
	int m_nIndexBits;
};


class CDTIRecvTable
{
public:
	CDTIRecvTable()
	{
		m_bSawAction = false;
	}

	CUtlString m_Name;
	CUtlVector<CDTIProp> m_Props;
	bool m_bSawAction;
};


CUtlLinkedList<CDTIRecvTable*, int> g_DTIRecvTables;


void DTI_Init()
{
#if ( defined( IS_WINDOWS_PC ) && (! defined( SWDS ) ) )
	extern IVEngineClient *engineClient;
	if ( CommandLine()->FindParm( "-dti" ) && !g_bDTIEnabled )
	{
		g_bDTIEnabled = true;

		SYSTEMTIME systemTime;
		GetLocalTime( &systemTime );

		char dtiFileName[MAX_PATH];
		char dtiLevelName[MAX_PATH];
		V_FileBase( engineClient->GetLevelName(), dtiLevelName, ARRAYSIZE( dtiLevelName ) );
		V_snprintf( dtiFileName, ARRAYSIZE( dtiFileName ), "dti_client_%s_%02d%02d%02d-%02d%02d%02d.csv", 
					dtiLevelName,
					systemTime.wYear % 100, systemTime.wMonth, systemTime.wDay,
					systemTime.wHour, systemTime.wMinute, systemTime.wSecond );
		g_pDTIFilename = COM_StringCopy( dtiFileName );
	}
#endif
}


void DTI_Term()
{
	if ( g_bDTIEnabled )
	{
		DTI_Flush();
		g_DTIRecvTables.PurgeAndDeleteElements();
		delete g_pDTIFilename;
		g_pDTIFilename = NULL;
		g_bDTIEnabled = false;
	}
}


void DTI_Flush()
{
	if ( !g_bDTIEnabled )
		return;

	FileHandle_t fp = g_pFileSystem->Open( g_pDTIFilename, "wt" );
	if( fp != FILESYSTEM_INVALID_HANDLE )
	{
		// Write the header.
		g_pFileSystem->FPrintf( fp,
			"Class"
			",Prop"
			",Decode Count"
			",Total Bits"
			",Avg Bits"
			",Total Index Bits"
			",Avg Index Bits"
			",=SUM(D:D)"
			"\n" );
	
		int row = 2;

		FOR_EACH_LL( g_DTIRecvTables, iTable )
		{
			CDTIRecvTable *pTable = g_DTIRecvTables[iTable];
			
			if ( !pTable->m_bSawAction )
				continue;
			
			for ( int iProp=0; iProp < pTable->m_Props.Count(); iProp++ )
			{
				CDTIProp *pProp = &pTable->m_Props[iProp];

				if ( pProp->m_nDecodes == 0 )
					continue;
			
				g_pFileSystem->FPrintf( fp,
					// Class/Prop names
					"%s"
					",%s"
					
					// Decode count
					",%d"

					// Total/Avg bits
					",%d"
					",%.3f"

					// Total/Avg index bits
					",%d"
					",%.3f"
					",=D%d/H$1"

					"\n",
					
					// Class/Prop names
					pTable->m_Name.String(),
					pProp->m_Name.String(),

					// Decode count
					pProp->m_nDecodes,

					// Total/Avg bits
					pProp->m_nDataBits,
					(float)pProp->m_nDataBits / pProp->m_nDecodes,

					// Total/Avg index bits
					pProp->m_nIndexBits,
					(float)pProp->m_nIndexBits / pProp->m_nDecodes,
					row++
					);
			}
		}

		g_pFileSystem->Close( fp );

		Msg( "DTI: wrote client stats into %s.\n", g_pDTIFilename );
	}
}


void DTI_HookRecvDecoder( CRecvDecoder *pDecoder )
{
	if ( !g_bDTIEnabled )
		return;

	bool dtiEnabled = CommandLine()->FindParm("-dti" ) > 0;

	CDTIRecvTable *pTable = new CDTIRecvTable;
	pTable->m_Name.Set( pDecoder->GetName() );
	
	pTable->m_Props.SetSize( pDecoder->GetNumProps() );
	for ( int i=0; i < pTable->m_Props.Count(); i++ )
	{
		const SendProp *pSendProp = pDecoder->GetSendProp( i );
		if ( !dtiEnabled )
		{
			pTable->m_Props[i].m_Name.Set( pSendProp->GetName() );
		}
		else
		{
			char *parentArrayPropName = const_cast< char * >(const_cast< SendProp * >(pSendProp)->GetParentArrayPropName());
			if ( parentArrayPropName )
			{
				char temp[256];
				V_snprintf( temp, sizeof( temp ), "%s:%s", parentArrayPropName, pSendProp->GetName() );
				pTable->m_Props[i].m_Name.Set( temp );
			}
			else
			{
				pTable->m_Props[i].m_Name.Set( pSendProp->GetName() );
			}
		}
	}
	
	g_DTIRecvTables.AddToTail( pTable );

	pDecoder->m_pDTITable = pTable;
}


void _DTI_HookDeltaBits( CRecvDecoder *pDecoder, int iProp, int nDataBits, int nIndexBits )
{
	CDTIRecvTable *pTable = pDecoder->m_pDTITable;
	if ( !pTable )
		return;

	CDTIProp *pProp = &pTable->m_Props[iProp];
	pProp->m_nDecodes++;
	pProp->m_nDataBits += nDataBits;
	pProp->m_nIndexBits += nIndexBits;

	pTable->m_bSawAction = true;
}



