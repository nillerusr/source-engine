//====== Copyright (c), Valve Corporation, All rights reserved. =======
//
// Purpose: Implements singleton datacache with support for yielding updates
//
//=============================================================================

#ifndef GCPARALLELJOBFARM_H
#define GCPARALLELJOBFARM_H
#ifdef _WIN32
#pragma once
#endif


namespace GCSDK
{

class IYieldingParallelFarmJobHandler
{
	//
	// Derived class instances implementing farm job workload processing must
	// be allocated on the heap as the calling job will yield for processing.
	//
protected:
	//
	// BYieldingRunWorkload is called on different newly created farm jobs
	// Param passed iJobSequenceCounter starts at 0 (zero) for the first framed job
	// and is incremented by one for each subsequently farmed job.
	// When the parallel processing should end job must set pbWorkloadCompleted to true
	// and return true as success.
	// Any farmed job returning false will abort further farmed processing and
	// will result in BYieldingExecuteParallel returning false to the caller job.
	//
	virtual bool BYieldingRunWorkload( int iJobSequenceCounter, bool *pbWorkloadCompleted ) = 0;

public:
	bool BYieldingExecuteParallel( int numJobsParallel, char const *pchJobName = NULL, uint nTimeoutSec = 0 );
};


} // namespace GCSDK

#endif // GCPARALLELJOBFARM_H
