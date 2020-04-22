//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef CARDSTATS_H
#define CARDSTATS_H

#include "tier1/UtlBuffer.h"
#include "tier1/utlvector.h"
#include "tier1/strtools.h"

int LoadFileIntoBuffer( CUtlBuffer &buf, char *pszFilename );
void WriteOutputToFile( CUtlBuffer &buf, char *pszFilename, char *pszSearchString );
void ParseHeader( CUtlBuffer &buf );
void ParseFile( CUtlBuffer &inbuf, CUtlBuffer &outbuf, char *pszSearchString, int size, int binwidth, int nofilter );
void ParseFile2( CUtlBuffer &inbuf, CUtlBuffer &outbuf, char *pszSearchString, int size, int binwidth );
void ParseFile3( CUtlBuffer &inbuf, CUtlBuffer &outbuf, char *pszSearchString, int size, int binwidth);
void TimeSeriesCPU( CUtlBuffer &inbuf, CUtlBuffer &outbuf, char *pszSearchString, int size, int binwidth);
void TimeSeriesVCard( CUtlBuffer &inbuf, CUtlBuffer &outbuf, char *pszSearchString, int size, int binwidth);
void HistogramVidCards( CUtlBuffer &inbuf, CUtlBuffer &outbuf, char *pszSearchString, int size, int binwidth);
void HistogramNetSpeed( CUtlBuffer &inbuf, CUtlBuffer &outbuf, char *pszSearchString, int size, int binwidth);
void HistogramRam( CUtlBuffer &inbuf, CUtlBuffer &outbuf, char *pszSearchString, int size, int binwidth);
void HistogramCPU( CUtlBuffer &inbuf, CUtlBuffer &outbuf, char *pszSearchString, int size, int binwidth);
void InsertResult( int nCpu, CUtlVector<int> &nCpuList, CUtlVector<int> &nQuantity );

#endif