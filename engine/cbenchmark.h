//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#ifndef CBENCHMARK_H
#define CBENCHMARK_H

#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CCommand;


#define MAX_BUFFER_SIZE 2048

//-----------------------------------------------------------------------------
// Purpose: Holds benchmark data & state
//-----------------------------------------------------------------------------
class CBenchmarkResults
{
public:
	CBenchmarkResults();

	bool IsBenchmarkRunning();
	void StartBenchmark( const CCommand &args );
	void StopBenchmark();
	void SetResultsFilename( const char *pFilename );
	void Upload();

private:
	bool m_bIsTestRunning;
	char m_szFilename[256];

	int m_flStartTime;
	int m_iStartFrame;
};

inline CBenchmarkResults *GetBenchResultsMgr()
{
	extern CBenchmarkResults g_BenchmarkResults;
	return &g_BenchmarkResults;
}


#endif // CBENCHMARK_H
