//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef BASE_GAMESTATS_PARSE_H
#define BASE_GAMESTATS_PARSE_H
#ifdef _WIN32
#pragma once
#endif

enum CustomDataReturnCode
{
	CUSTOMDATA_NONE = 0,
	CUSTOMDATA_FAILED,
	CUSTOMDATA_SUCCESS
};

class IMySQL;

struct ParseContext_t
{
	char const *file;
	char const *gamename;
	bool		describeonly;
	IMySQL		*mysql;
	int			skipcount;
	bool		bCustomDirectoryNotMade;
};

void ProcessPerfData( IMySQL *pSQL, time_t fileTime, char const *pGameName, char const *pPerfString );

#endif // BASE_GAMESTATS_PARSE_H
