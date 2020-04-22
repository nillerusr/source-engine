//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "filesystem_engine.h"
#include "filesystem.h"
#include "dt_instrumentation_server.h"
#include "dt_send.h"
#include "tier1/utlstring.h"
#include "utllinkedlist.h"
#include "dt.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define DELTA_DISTANCE_BAND			200
#define NUM_DELTA_DISTANCE_BANDS	(8000/DELTA_DISTANCE_BAND)


// Data we track per SendTable on the server.
class CDTISendTable
{
public:
	// Which SendTable we're interested in.
	CUtlString		m_NetTableName;

	// How many cycles we've spent in certain calls.
	CCycleCount		m_nCalcDeltaCycles;
	int				m_nCalcDeltaCalls;

	CCycleCount		m_nEncodeCycles;
	int				m_nEncodeCalls;

	CCycleCount		m_nShouldTransmitCycles;
	int				m_nShouldTransmitCalls;

	CCycleCount		m_nWriteDeltaPropsCycles;

	// Used to determine how much the class uses manual mode.
	int m_nChangeAutoDetects;
	int m_nNoChanges;

	// Set to false if no events were recorded for this class.
	bool HadAnyAction() const { return m_nCalcDeltaCalls || m_nEncodeCalls || m_nShouldTransmitCalls; }

	// This tracks how many times an entity was delta'd for each distance from a client.
	unsigned short	m_DistanceDeltaCounts[NUM_DELTA_DISTANCE_BANDS];
};


static CCycleCount g_TotalServerDTICycles;

static CUtlLinkedList<CDTISendTable*, unsigned short> g_DTISendTables;

bool g_bServerDTIEnabled = false;
static char const *g_pServerDTIFilename = 0;

static bool g_bFirstHookTimer = true;
static CCycleCount g_ServerDTITimer;



void ServerDTI_Init( char const *pFilename )
{
	g_pServerDTIFilename = pFilename;
	g_bServerDTIEnabled = true;
	g_TotalServerDTICycles.Init();
	g_bFirstHookTimer = true;
}


void ServerDTI_Term()
{
	if ( !g_pServerDTIFilename )
		return;
	ServerDTI_Flush();
	g_DTISendTables.PurgeAndDeleteElements();
	g_pServerDTIFilename = 0;
	g_bServerDTIEnabled = false;
}


void ServerDTI_Flush()
{
	if ( !g_pServerDTIFilename )
		return;

	CCycleCount curTime;
	curTime.Sample();
	
	CCycleCount runningTime;
	CCycleCount::Sub( curTime, g_ServerDTITimer, runningTime );

	// Write out a file that can be used by Excel.
	FileHandle_t fp = g_pFileSystem->Open( g_pServerDTIFilename, "wt", "LOGDIR" );
	
	if( fp != FILESYSTEM_INVALID_HANDLE )
	{
		// Write the header.
		g_pFileSystem->FPrintf( fp, 
			"DTName"
			
			"\tCalcDelta calls"
			"\tCalcDelta ms"
			
			"\tEncode calls"
			"\tEncode ms"
			
			"\tShouldTransmit calls"
			"\tShouldTransmit ms"

			"\tWriteDeltaProps ms"

			"\t%% manual mode"

			"\tTotal"
			"\tPercent"
			"\n"
			);

		// Calculate totals.
		CCycleCount totalCalcDelta, totalEncode, totalShouldTransmit, totalDeltaProps;
		totalCalcDelta.Init();
		totalEncode.Init();
		totalShouldTransmit.Init();
		
		FOR_EACH_LL( g_DTISendTables, i )
		{
			CDTISendTable *pTable = g_DTISendTables[i];
			
			CCycleCount::Add( pTable->m_nCalcDeltaCycles, totalCalcDelta, totalCalcDelta );
			CCycleCount::Add( pTable->m_nEncodeCycles, totalEncode, totalEncode );
			CCycleCount::Add( pTable->m_nShouldTransmitCycles, totalShouldTransmit, totalShouldTransmit );
			CCycleCount::Add( pTable->m_nWriteDeltaPropsCycles, totalDeltaProps, totalDeltaProps );
		}
	

		FOR_EACH_LL( g_DTISendTables, j )
		{
			CDTISendTable *pTable = g_DTISendTables[j];

			if ( !pTable->HadAnyAction() )
				continue;

			CCycleCount total;
			CCycleCount::Add( pTable->m_nEncodeCycles, pTable->m_nCalcDeltaCycles, total );
			CCycleCount::Add( pTable->m_nShouldTransmitCycles, total, total );

			g_pFileSystem->FPrintf( fp, 
				"%s"

				"\t%d"
				"\t%.3f"

				"\t%d"
				"\t%.3f"

				"\t%d"
				"\t%.3f"

				"\t%.3f"

				"\t%.2f"

				"\t%.3f"
				"\t%.3f"
				"\n",
				pTable->m_NetTableName.String(),
				
				pTable->m_nCalcDeltaCalls,
				pTable->m_nCalcDeltaCycles.GetMillisecondsF(),
				
				pTable->m_nEncodeCalls,
				pTable->m_nEncodeCycles.GetMillisecondsF(),
				
				pTable->m_nShouldTransmitCalls,
				pTable->m_nShouldTransmitCycles.GetMillisecondsF(),

				pTable->m_nWriteDeltaPropsCycles.GetMillisecondsF(),
				
				(float)pTable->m_nNoChanges * 100.0f / (pTable->m_nNoChanges + pTable->m_nChangeAutoDetects),

				total.GetMillisecondsF(),
				total.GetMillisecondsF() * 100 / runningTime.GetMillisecondsF()
				);
		}

		g_pFileSystem->FPrintf( fp, "\n\n" );

		g_pFileSystem->FPrintf( fp,
			"Total profile ms:"
			"\t%.3f\n",
			runningTime.GetMillisecondsF()
			);
		
		g_pFileSystem->FPrintf( fp, 
			"Total CalcDelta ms:"
			"\t%.3f"
			"\tPercent:"
			"\t%.3f\n",
			totalCalcDelta.GetMillisecondsF(),
			totalCalcDelta.GetMillisecondsF() * 100.0 / runningTime.GetMillisecondsF() 
			);

		g_pFileSystem->FPrintf( fp, 
			"Total Encode ms:"
			"\t%.3f"
			"\tPercent:"
			"\t%.3f\n",
			totalEncode.GetMillisecondsF(),
			totalEncode.GetMillisecondsF() * 100.0 / runningTime.GetMillisecondsF() 
			);

		g_pFileSystem->FPrintf( fp,
			"Total ShouldTransmit ms:"
			"\t%.3f"
			"\tPercent:"
			"\t%.3f\n",
			totalShouldTransmit.GetMillisecondsF(),
			totalShouldTransmit.GetMillisecondsF() * 100.0 / runningTime.GetMillisecondsF()
			);
		
		g_pFileSystem->FPrintf( fp,
			"Total WriteDeltaProps ms:"
			"\t%.3f"
			"\tPercent:"
			"\t%.3f\n",
			totalDeltaProps.GetMillisecondsF(),
			totalDeltaProps.GetMillisecondsF() * 100.0 / runningTime.GetMillisecondsF()
			);
		
		g_pFileSystem->Close( fp );

		Msg( "DTI: Wrote delta distances into %s.\n", g_pServerDTIFilename );
	}

	
	// Write the delta distances.
	const char *pDeltaDistancesFilename = "dti_delta_distances.txt";
	fp = g_pFileSystem->Open( pDeltaDistancesFilename, "wt", "LOGDIR" );
	if( fp != FILESYSTEM_INVALID_HANDLE )
	{
		// Write the column labels.
		g_pFileSystem->FPrintf( fp, "ClassName" );
		for ( int i=0; i < NUM_DELTA_DISTANCE_BANDS; i++ )
		{
			g_pFileSystem->FPrintf( fp, "\t<%d", (i+1) * DELTA_DISTANCE_BAND );
		}
		g_pFileSystem->FPrintf( fp, "\n" );
		
		// Now write the data.
		FOR_EACH_LL( g_DTISendTables, j )
		{
			CDTISendTable *pTable = g_DTISendTables[j];

			if ( !pTable->HadAnyAction() )
				continue;

			g_pFileSystem->FPrintf( fp, "%s", pTable->m_NetTableName.String() );
			for ( int i=0; i < NUM_DELTA_DISTANCE_BANDS; i++ )
			{
				g_pFileSystem->FPrintf( fp, "\t%d", pTable->m_DistanceDeltaCounts[i] );
			}
			g_pFileSystem->FPrintf( fp, "\n" );
		}		

		g_pFileSystem->Close( fp );
		
		Msg( "DTI: Wrote instrumentation data into %s.\n", pDeltaDistancesFilename );
	}
}


CDTISendTable* ServerDTI_HookTable( SendTable *pTable )
{
	if ( !g_bServerDTIEnabled )
		return NULL;

	CDTISendTable *pRet = new CDTISendTable;
	memset( pRet, 0, sizeof( *pRet ) );

	pRet->m_NetTableName.Set( pTable->m_pNetTableName );

	g_DTISendTables.AddToTail( pRet );
	return pRet;
}


void ServerDTI_AddEntityEncodeEvent( SendTable *pSendTable, float distToPlayer )
{
	CSendTablePrecalc *pPrecalc = pSendTable->m_pPrecalc;
	if ( !pPrecalc || !pPrecalc->m_pDTITable )
		return;

	CDTISendTable *pTable = pPrecalc->m_pDTITable;		
	if ( !pTable )
		return;

	int iDist = (int)( distToPlayer / DELTA_DISTANCE_BAND );
	iDist = clamp( iDist, 0, NUM_DELTA_DISTANCE_BANDS - 1 );
	pTable->m_DistanceDeltaCounts[iDist]++;
}

void _ServerDTI_HookTimer( const SendTable *pSendTable, ServerDTITimerType timerType, CCycleCount const &count )
{
	CSendTablePrecalc *pPrecalc = pSendTable->m_pPrecalc;
	if ( !pPrecalc || !pPrecalc->m_pDTITable )
		return;

	CDTISendTable *pTable = pPrecalc->m_pDTITable;		

	if ( g_bFirstHookTimer )
	{
		g_ServerDTITimer.Sample();
		g_bFirstHookTimer = false;
	}

	// Add to the total cycles.
	CCycleCount::Add( count, g_TotalServerDTICycles, g_TotalServerDTICycles );

	if ( timerType == SERVERDTI_CALCDELTA )
	{
		CCycleCount::Add( count, pTable->m_nCalcDeltaCycles, pTable->m_nCalcDeltaCycles );
		++pTable->m_nCalcDeltaCalls;
	}
	else if ( timerType == SERVERDTI_ENCODE )
	{
		CCycleCount::Add( count, pTable->m_nEncodeCycles, pTable->m_nEncodeCycles );
		++pTable->m_nEncodeCalls;
	}
	else if ( timerType == SERVERDTI_SHOULDTRANSMIT )
	{
		CCycleCount::Add( count, pTable->m_nShouldTransmitCycles, pTable->m_nShouldTransmitCycles );
		++pTable->m_nShouldTransmitCalls;
	}
	else if ( timerType == SERVERDTI_WRITE_DELTA_PROPS )
	{
		CCycleCount::Add( count, pTable->m_nWriteDeltaPropsCycles, pTable->m_nWriteDeltaPropsCycles );
	}
}

void _ServerDTI_RegisterNetworkStateChange( SendTable *pSendTable, bool bStateChanged )
{
	CSendTablePrecalc *pPrecalc = pSendTable->m_pPrecalc;
	if ( !pPrecalc || !pPrecalc->m_pDTITable )
		return;

	CDTISendTable *pTable = pPrecalc->m_pDTITable;		

	if ( bStateChanged )
		++pTable->m_nChangeAutoDetects;
	else
		++pTable->m_nNoChanges;
}

