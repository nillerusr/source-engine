//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef VOX_H
#define VOX_H
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct sfxcache_t;
struct channel_t;

class CUtlSymbol;

extern void				VOX_Init( void );
extern void 			VOX_Shutdown( void );
extern void				VOX_ReadSentenceFile( const char *psentenceFileName );
extern int				VOX_SentenceCount( void );
extern void				VOX_LoadSound( channel_t *pchan, const char *psz );
// UNDONE: Improve the interface of this call, it returns sentence data AND the sentence index
extern char				*VOX_LookupString( const char *pSentenceName, int *psentencenum, bool *pbEmitCaption = NULL, CUtlSymbol *pCaptionSymbol = NULL, float * pflDuration = NULL );
extern void				VOX_PrecacheSentenceGroup( class IEngineSound *pSoundSystem, const char *pGroupName, const char *pPathOverride = NULL );
extern const char		*VOX_SentenceNameFromIndex( int sentencenum );
extern float			VOX_SentenceLength( int sentence_num );
extern const char		*VOX_GroupNameFromIndex( int groupIndex );
extern int				VOX_GroupIndexFromName( const char *pGroupName );
extern int				VOX_GroupPick( int isentenceg, char *szfound, int strLen );
extern int				VOX_GroupPickSequential( int isentenceg, char *szfound, int szfoundLen, int ipick, int freset );

#ifdef __cplusplus
}
#endif

#endif // VOX_H
