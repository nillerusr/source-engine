//====== Copyright (c), Valve Corporation, All rights reserved. =======
//
// Purpose: Implements parallel job farming process
//
//=============================================================================

#include "stdafx.h"
#include "rtime.h"
#include "gcparalleljobfarm.h"


namespace GCSDK
{

bool IYieldingParallelFarmJobHandler::BYieldingExecuteParallel( int numJobsParallel, char const *pchJobName, uint nTimeoutSec )
{
	AssertRunningJob();

	if ( !pchJobName )
		pchJobName = GJobCur().GetName();

	struct CParallelFarmHeapData_t
	{
		explicit CParallelFarmHeapData_t( IYieldingParallelFarmJobHandler *pHandler, int numJobsFarmLimit )
		{
			m_pHandler = pHandler;
			m_jobIdParent = GJobCur().GetJobID();
			m_numJobsFarmed = 0;
			m_numJobsFarmLimit = MAX( 1, numJobsFarmLimit );
			m_iJobSequenceCounter = 0;
			m_bErrorEncountered = false;
			m_bWorkloadCompleted = false;
		}

		IYieldingParallelFarmJobHandler *m_pHandler;
		JobID_t m_jobIdParent;
		int m_numJobsFarmLimit;
		int m_numJobsFarmed;
		int m_iJobSequenceCounter;
		bool m_bErrorEncountered;
		bool m_bWorkloadCompleted;
	};
	CParallelFarmHeapData_t *pHeapData = new CParallelFarmHeapData_t( this, numJobsParallel );

	class CYieldingParallelFarmJob : public CGCJob
	{
	public:
		CYieldingParallelFarmJob( CGCBase *pGC, CParallelFarmHeapData_t *pJobData, char const *pchJobName, uint nTimeoutSec ) : CGCJob( pGC, pchJobName )
			, m_pJobData( pJobData ), m_iJobSequenceCounter( pJobData->m_iJobSequenceCounter ), m_nTimeoutSec( nTimeoutSec )
		{
		}
		virtual bool BYieldingRunJob( void *pvStartParam )
		{
			if ( m_nTimeoutSec )
				SetJobTimeout( m_nTimeoutSec );

			bool bWorkloadCompleted = false;
			bool bResult = m_pJobData->m_pHandler
				? m_pJobData->m_pHandler->BYieldingRunWorkload( m_iJobSequenceCounter, &bWorkloadCompleted )
				: false;

			if ( !bResult )
				m_pJobData->m_bErrorEncountered = true;
			else if ( bWorkloadCompleted )
				m_pJobData->m_bWorkloadCompleted = true;

			-- m_pJobData->m_numJobsFarmed;

			if ( !m_pJobData->m_bErrorEncountered && !m_pJobData->m_bWorkloadCompleted )
			{
				CYieldingParallelFarmJob *pFarmedJob = new CYieldingParallelFarmJob( m_pGC, m_pJobData, GetName(), m_nTimeoutSec );
				++ m_pJobData->m_numJobsFarmed;
				++ m_pJobData->m_iJobSequenceCounter;
				pFarmedJob->StartJobDelayed( NULL );
			}

			if ( !m_pJobData->m_numJobsFarmed )
			{	// No more farmed jobs to wait for
				m_pGC->GetJobMgr().BRouteWorkItemCompletedDelayed( m_pJobData->m_jobIdParent, false );
			}

			return bResult;
		}

	protected:
		CParallelFarmHeapData_t *m_pJobData;
		int m_iJobSequenceCounter;
		uint m_nTimeoutSec;
	};

	for ( ; ; ++ pHeapData->m_iJobSequenceCounter )
	{
		if ( pHeapData->m_numJobsFarmed < pHeapData->m_numJobsFarmLimit )
		{
			CYieldingParallelFarmJob *pFarmedJob = new CYieldingParallelFarmJob( GGCBase(), pHeapData, pchJobName, nTimeoutSec );
			++ pHeapData->m_numJobsFarmed;
			pFarmedJob->StartJobDelayed( NULL );
		}
		else
		{
			if ( !GJobCur().BYieldingWaitForWorkItem( pchJobName ) )
			{
				EmitError( SPEW_GC, "YieldingExecuteParallel: failed to sync with %u farmed work items.\n", pHeapData->m_numJobsFarmed );
				pHeapData->m_bErrorEncountered = true;
				pHeapData->m_pHandler = NULL; // handler itself may become invalid when the function returns
				return false; // leak pHeapData because work items might still be running and this can avoid a crash (this condition is abnormal)
			}

			break;
		}
	}

	bool bResult = pHeapData->m_bWorkloadCompleted && !pHeapData->m_bErrorEncountered;
	delete pHeapData;
	return bResult;
}


}
