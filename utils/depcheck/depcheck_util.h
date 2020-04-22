//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef DEPCHECK_UTIL_H
#define DEPCHECK_UTIL_H
#ifdef _WIN32
#pragma once
#endif


void vprint( int depth, PRINTF_FORMAT_STRING const char *fmt, ... );
void CC_UngetToken( void );
char *CC_ParseToken(char *data);
char *CC_DiscardUntilMatchingCharIncludingNesting( char *input, const char *pairing );
char *CC_RawParseChar( char *input, const char *ch, char *breakchar );
char *CC_ParseUntilEndOfLine( char *input );

extern char com_token[1024];
extern bool com_ignoreinlinecomment;
extern int linesprocessed;

unsigned char *COM_LoadFile( const char *name, int *len);
void COM_FreeFile( unsigned char *buffer );
bool COM_DirectoryExists( const char *dir );

#endif // DEPCHECK_UTIL_H
