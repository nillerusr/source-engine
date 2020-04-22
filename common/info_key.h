//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Utility functions for parsing info strings
//
// $NoKeywords: $
//=============================================================================//

#ifndef INFO_H
#define INFO_H
#ifdef _WIN32
#pragma once
#endif

const char *Info_ValueForKey( const char *s, const char *key );
void Info_RemoveKey( char *s, const char *key );
void Info_RemovePrefixedKeys( char *start, char prefix );
bool Info_IsKeyImportant( const char *key );
char *Info_FindLargestKey( char *s, int maxsize );
void Info_SetValueForStarKey ( char *s, const char *key, const char *value, int maxsize );
void Info_SetValueForKey (char *s, const char *key, const char *value, int maxsize);



#endif // INFO_H
