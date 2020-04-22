//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef UTIL_H
#define UTIL_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Utility function for the server browser
//-----------------------------------------------------------------------------
class CUtil
{
public:
	const char *InfoGetValue(const char *s, const char *key);
	const char *GetString(const char *stringName);

};

extern CUtil *util;



#endif // UTIL_H
