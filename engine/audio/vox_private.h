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

#ifndef VOX_PRIVATE_H
#define VOX_PRIVATE_H
#pragma once

#ifndef VOX_H
#include "vox.h"
#endif

#ifndef UTLVECTOR_H
#include "utlvector.h"
#endif

#include "utlsymbol.h"

struct channel_t;
class CSfxTable;
class CAudioMixer;

struct voxword_t
{
	int		volume;						// increase percent, ie: 125 = 125% increase
	int		pitch;						// pitch shift up percent
	int		start;						// offset start of wave percent
	int		end;						// offset end of wave percent
	int		cbtrim;						// end of wave after being trimmed to 'end'
	int		fKeepCached;				// 1 if this word was already in cache before sentence referenced it
	int		samplefrac;					// if pitch shifting, this is position into wav * 256
	int		timecompress;				// % of wave to skip during playback (causes no pitch shift)
	CSfxTable *sfx;					// name and cache pointer
};

#define CVOXWORDMAX		32
#define CVOXZEROSCANMAX	255			// scan up to this many samples for next zero crossing

struct sentence_t
{
	sentence_t() :
		pName( 0 ),
		length( 0.0f ),
		closecaption( false )
	{
	}

	char		*pName;
	float		length;
	bool		closecaption : 7;
	bool		isPrecached : 1;
	CUtlSymbol	caption;
};

extern CUtlVector<sentence_t>	g_Sentences;


extern int				VOX_FPaintPitchChannelFrom8( channel_t *ch, sfxcache_t *sc, int count, int pitch, int timecompress );
extern void				VOX_TrimStartEndTimes( channel_t *ch, sfxcache_t *sc );
extern int				VOX_ParseWordParams( char *psz, voxword_t *pvoxword, int fFirst );
extern void				VOX_SetChanVol( channel_t *ch );
extern char				**VOX_ParseString( char *psz );
extern CAudioMixer		*CreateSentenceMixer( voxword_t *pwords );

#endif // VOX_PRIVATE_H
