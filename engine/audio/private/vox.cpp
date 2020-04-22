//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Voice / Sentence streaming & parsing code
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

//===============================================================================
// VOX. Algorithms to load and play spoken text sentences from a file:
//
// In ambient sounds or entity sounds, precache the 
// name of the sentence instead of the wave name, ie: !C1A2S4
//
// During sound system init, the 'sentences.txt' is read.
// This file has the format:
//
//		C1A2S4 agrunt/vox/You will be exterminated, surrender NOW.
//      C1A2s5 hgrunt/vox/Radio check, over.
//		...
//
//		There must be at least one space between the sentence name and the sentence.
//		Sentences may be separated by one or more lines
//		There may be tabs or spaces preceding the sentence name
//		The sentence must end in a /n or /r
//		Lines beginning with // are ignored as comments
//
//		Period or comma will insert a pause in the wave unless
//		the period or comma is the last character in the string.
//
//		If first 2 chars of a word are upper case, word volume increased by 25%
// 
//		If last char of a word is a number from 0 to 9
//		then word will be pitch-shifted up by 0 to 9, where 0 is a small shift
//		and 9 is a very high pitch shift.
//
// We alloc heap space to contain this data, and track total 
// sentences read.  A pointer to each sentence is maintained in g_Sentences.
//
// When sound is played back in S_StartDynamicSound or s_startstaticsound, we detect the !name
// format and lookup the actual sentence in the sentences array
//
// To play, we parse each word in the sentence, chain the words, and play the sentence
// each word's data is loaded directy from disk and freed right after playback.
//===============================================================================

#include "audio_pch.h"
#include "vox_private.h"
#include "characterset.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "utlsymbol.h"
#include "utldict.h"
#include "../../MapReslistGenerator.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// In other C files.
// Globals
extern IFileSystem *g_pFileSystem;

// This is the initial capacity for sentences, the array will grow if necessary
#define MAX_EXPECTED_SENTENCES	900

CUtlVector<sentence_t>	g_Sentences;
// FIXME: could get this through common includes
const char *COM_Parse (const char *data);
extern	char		com_token[1024];

// Module Locals
static char		*rgpparseword[CVOXWORDMAX];	// array of pointers to parsed words
static char		voxperiod[] = "_period";				// vocal pause
static char		voxcomma[] = "_comma";				// vocal pause

#define CVOXMAPNAMESMAX 24
static char *g_rgmapnames[CVOXMAPNAMESMAX];
static int g_cmapnames = 0;

// Sentence file list management
static void			VOX_ListClear( void );
static int			VOX_ListFileIsLoaded( const char *psentenceFileName );
static void			VOX_ListMarkFileLoaded( const char *psentenceFileName );
static void			VOX_InitAllEntnames( void );

void VOX_LookupMapnames( void );

static void VOX_Reload()
{
	VOX_Shutdown();
	VOX_Init();
}
static ConCommand vox_reload( "vox_reload", VOX_Reload, "Reload sentences.txt file", FCVAR_CHEAT );

static CUtlVector<unsigned char> g_GroupLRU;
static CUtlVector<char> g_SentenceFile;

struct sentencegroup_t
{
	short count;

public:
	short lru;
	const char *GroupName() const;
	CUtlSymbol GroupNameSymbol() const;
	void SetGroupName( const char *pName );
	static CUtlSymbol GetSymbol( const char *pName );

private:
	CUtlSymbol groupname;
	static CUtlSymbolTable s_SymbolTable;
};

const char *sentencegroup_t::GroupName() const
{
	return s_SymbolTable.String( groupname ); 
}

void sentencegroup_t::SetGroupName( const char *pName )
{
	groupname = s_SymbolTable.AddString( pName ); 
}

CUtlSymbol sentencegroup_t::GroupNameSymbol() const
{
	return groupname;
}

CUtlSymbol sentencegroup_t::GetSymbol( const char *pName )
{
	return s_SymbolTable.AddString( pName ); 
}

CUtlVector<sentencegroup_t> g_SentenceGroups;
CUtlSymbolTable sentencegroup_t::s_SymbolTable( 0, 256, true );

struct WordBuf
{
	WordBuf()
	{
		word[ 0 ] = 0;
	}

	WordBuf( const WordBuf& src )
	{
		Q_strncpy( word, src.word, sizeof( word ) );
	}

	void Set( char const *w )
	{
		if ( !w )
		{
			word[ 0 ] = 0;
			return;
		}
		Q_strncpy( word, w, sizeof( word ) );
		while ( Q_strlen( word ) >= 1 && word[ Q_strlen( word ) - 1 ] == ' ' )
		{
			word[ Q_strlen( word ) - 1 ] = 0;
		}
	}

	char	word[ 256 ];
};

struct ccpair
{
	WordBuf	token;
	WordBuf	value;

	WordBuf	fullpath;
};

static void			VOX_BuildVirtualNameList( char *word, CUtlVector< WordBuf >& list );

// This module depends on these engine calls:
// DevMsg
// S_FreeChannel
// S_LoadSound
// S_FindName
// It also depends on vstdlib/RandomInt (all other random calls go through g_pSoundServices)

void VOX_Init( void ) 
{
	VOX_InitAllEntnames();

	g_SentenceFile.Purge();
	g_GroupLRU.Purge();
	g_Sentences.RemoveAll();
	g_Sentences.EnsureCapacity( MAX_EXPECTED_SENTENCES );

	VOX_ListClear();

	VOX_ReadSentenceFile( "scripts/sentences.txt" );
	VOX_LookupMapnames();
}


void VOX_Shutdown( void ) 
{
	g_Sentences.RemoveAll();
	VOX_ListClear();
	g_SentenceGroups.RemoveAll();
	g_cmapnames = 0;
}

//-----------------------------------------------------------------------------
// Purpose: This is kind of like strchr(), but we get the actual pointer to the
//			end of the string when it fails rather than NULL.  This is useful
//			for parsing buffers containing multiple strings
// Input  : *string - 
//			scan - 
// Output : char
//-----------------------------------------------------------------------------
char *ScanForwardUntil( char *string, char scan )
{
	while( string[0] )
	{
		if ( string[0] == scan )
			return string;

		string++;
	}
	return string;
}

// parse a null terminated string of text into component words, with
// pointers to each word stored in rgpparseword
// note: this code actually alters the passed in string!

char **VOX_ParseString(char *psz) 
{
	int i;
	int fdone = 0;
	char *pszscan = psz;
	char c;
	characterset_t nextWord, skip;

	memset(rgpparseword, 0, sizeof(char *) * CVOXWORDMAX);

	if (!psz)
		return NULL;

	i = 0;
	rgpparseword[i++] = psz;

	CharacterSetBuild( &nextWord, " ,.({" );
	CharacterSetBuild( &skip, "., " );
	while (!fdone && i < CVOXWORDMAX)
	{
		// scan up to next word
		c = *pszscan;
		while (c && !IN_CHARACTERSET(nextWord,c) )
			c = *(++pszscan);
			
		// if '(' then scan for matching ')'
		if ( c == '(' || c=='{' )
		{
			if ( c == '(' )
				pszscan = ScanForwardUntil( pszscan, ')' );
			else if ( c == '{' )
				pszscan = ScanForwardUntil( pszscan, '}' );

			c = *(++pszscan);
			if (!c)
				fdone = 1;
		}

		if (fdone || !c)
			fdone = 1;
		else
		{	
			// if . or , insert pause into rgpparseword,
			// unless this is the last character
			if ((c == '.' || c == ',') && *(pszscan+1) != '\n' && *(pszscan+1) != '\r'
					&& *(pszscan+1) != 0)
			{
				if (c == '.')
					rgpparseword[i++] = voxperiod;
				else
					rgpparseword[i++] = voxcomma;

				if (i >= CVOXWORDMAX)
					break;
			}

			// null terminate substring
			*pszscan++ = 0;

			// skip whitespace
			c = *pszscan;
			while (c && IN_CHARACTERSET(skip, c))
				c = *(++pszscan);

			if (!c)
				fdone = 1;
			else
				rgpparseword[i++] = pszscan;
		}
	}
	return rgpparseword;
}

// backwards scan psz for last '/'
// return substring in szpath null terminated
// if '/' not found, return 'vox/'

char *VOX_GetDirectory(char *szpath, int maxpath, char *psz)
{
	char c;
	int cb = 0;
	char *pszscan = psz + Q_strlen( psz ) - 1;

	// scan backwards until first '/' or start of string
	c = *pszscan;
	while (pszscan > psz && c != '/')
	{
		c = *(--pszscan);
		cb++;
	}

	if (c != '/')
	{
		// didn't find '/', return default directory
		Q_strncpy(szpath, "vox/", maxpath );
		return psz;
	}

	cb = Q_strlen(psz) - cb;

	cb = clamp( cb, 0, maxpath - 1 );

	// FIXME:  Is this safe?
	Q_memcpy(szpath, psz, cb);
	szpath[cb] = 0;
	return pszscan + 1;
}

// get channel volume scale if word

#ifndef SWDS
float VOX_GetChanVol(channel_t *ch)
{
	if ( !ch->pMixer )
		return 1.0;

	return ch->pMixer->GetVolumeScale();
/*
	
	if ( scale == 1.0 )
		return;

	ch->rightvol = (int) (ch->rightvol * scale);
	ch->leftvol = (int) (ch->leftvol * scale);

	if ( g_AudioDevice->Should3DMix() )
	{
		ch->rrightvol = (int) (ch->rrightvol * scale);
		ch->rleftvol = (int) (ch->rleftvol * scale);
		ch->centervol = (int) (ch->centervol * scale);
	}
	else
	{
		ch->rrightvol = 0;
		ch->rleftvol = 0;
		ch->centervol = 0;
	}
*/
}
#endif

//===============================================================================
//  Get any pitch, volume, start, end params into voxword
//  and null out trailing format characters
//  Format: 
//		someword(v100 p110 s10 e20)
//		
//		v is volume, 0% to n%
//		p is pitch shift up 0% to n%
//		s is start wave offset %
//		e is end wave offset %
//		t is timecompression %
//
//	pass fFirst == 1 if this is the first string in sentence
//  returns 1 if valid string, 0 if parameter block only.
//
//  If a ( xxx ) parameter block does not directly follow a word, 
//  then that 'default' parameter block will be used as the default value
//  for all following words.  Default parameter values are reset
//  by another 'default' parameter block.  Default parameter values
//  for a single word are overridden for that word if it has a parameter block.
// 
//===============================================================================

int VOX_ParseWordParams(char *psz, voxword_t *pvoxword, int fFirst) 
{
	char *pszsave = psz;
	char c;
	char ct;
	char sznum[8];
	int i;
	static voxword_t voxwordDefault;
	characterset_t commandSet, delimitSet;

	// List of valid commands
	CharacterSetBuild( &commandSet, "vpset)" );

	// init to defaults if this is the first word in string.
	if (fFirst)
	{
		voxwordDefault.pitch = -1;
		voxwordDefault.volume = 100;
		voxwordDefault.start = 0;
		voxwordDefault.end = 100;
		voxwordDefault.fKeepCached = 0;
		voxwordDefault.timecompress = 0;
	}

	*pvoxword = voxwordDefault;

	// look at next to last char to see if we have a 
	// valid format:

	c = *(psz + strlen(psz) - 1);
	
	if (c != ')')
		return 1;		// no formatting, return

	// scan forward to first '('
	CharacterSetBuild( &delimitSet, "()" );
	c = *psz;
	while ( !IN_CHARACTERSET(delimitSet, c) )
		c = *(++psz);
	
	if ( c == ')' )
		return 0;		// bogus formatting
	
	// null terminate

	*psz = 0;
	ct = *(++psz);

	while (1)
	{
		// scan until we hit a character in the commandSet

		while (ct && !IN_CHARACTERSET(commandSet, ct) )
			ct = *(++psz);
		
		if (ct == ')')
			break;

		memset(sznum, 0, sizeof(sznum));
		i = 0;

		c = *(++psz);
		
		if (!V_isdigit(c))
			break;

		// read number
		while (V_isdigit(c) && i < sizeof(sznum) - 1)
		{
			sznum[i++] = c;
			c = *(++psz);
		}

		// get value of number
		i = atoi(sznum);

		switch (ct)
		{
		case 'v': pvoxword->volume = i; break;
		case 'p': pvoxword->pitch = i; break;
		case 's': pvoxword->start = i; break;
		case 'e': pvoxword->end = i; break;
		case 't': pvoxword->timecompress = i; break;
		}

		ct = c;
	}

	// if the string has zero length, this was an isolated
	// parameter block.  Set default voxword to these
	// values

	if (strlen(pszsave) == 0)
	{
		voxwordDefault = *pvoxword;
		return 0;
	}
	else 
		return 1;
}

#define CVOXSAVEDWORDSIZE	32

// saved entity name/number based on type of entity & id

#define CVOXGLOBMAX	4		// max number of rnd and seqential globals

typedef struct _vox_entname
{
	// type is defined by last character of group name.
	// for instance, V_MYNAME_S has type 'S', which is used for soldiers
	// V_MYNUM_M has type 'P' which is used for metrocops

	int			type;

	SoundSource	soundsource;				// the enity emitting the sentence
	char		*pszname;					// a custom name for the entity (this is a word name) 
	char		*psznum;					// a custom number for the entity (this is a word name)
	char		*pszglobal[CVOXGLOBMAX];	// 1 global word, shared by this type of entity, picked randomly, expires after 5min
	char		*pszglobalseq[CVOXGLOBMAX];	// 1 global word, shared by this type of entity, picked in sequence, expires after 5 min
	bool		fdied;						// true if ent died (don't clear, we need its name)
	int			iseq[CVOXGLOBMAX];			// sequence index, for global sequential lookups
	float		timestamp[CVOXGLOBMAX];		// latest update to this ent global timestamp
	float		timestampseq[CVOXGLOBMAX];	// latest update to this ent global sequential timestamp
	float		timedied;					// timestamp of death

} vox_entname;

#define CENTNAMESMAX	64

vox_entname g_entnames[CENTNAMESMAX];

int g_entnamelastsaved = 0;

// init all

void VOX_InitAllEntnames( void )
{
	g_entnamelastsaved = 0;	
	Q_memset(g_entnames, 0, sizeof(g_entnames));
	Q_memset(g_rgmapnames, 0, sizeof(g_rgmapnames));
	g_cmapnames = 0;
}

// get new index

int VOX_GetNextEntnameIndex( void )
{
	g_entnamelastsaved++;

	if (g_entnamelastsaved >= CENTNAMESMAX)
	{
		g_entnamelastsaved = 0;
	}

	return g_entnamelastsaved;
}

// get index of this ent, or get a new index. if fallocnew is true, 
// get a new slot if none found. 
// NOTE: this routine always sets fdied to false - fdied is later
// set to true by the caller if in IDIED routine. This 
// ensures that if an ent is reused, it won't be marked as fdied.

int VOX_LookupEntIndex( int type, SoundSource soundsource, bool fallocnew) 
{
	int i;

	for (i = 0; i < CENTNAMESMAX; i++)
	{
		if ((g_entnames[i].type == type) && (g_entnames[i].soundsource == soundsource))
		{
			g_entnames[i].fdied = false;
			return i;
		}
	}
	
	if ( !fallocnew )
		return -1;

	// new index slot - init

	int inew = VOX_GetNextEntnameIndex();

	g_entnames[inew].type = type;
	g_entnames[inew].soundsource = soundsource;
	g_entnames[inew].timedied = 0;
	g_entnames[inew].fdied = 0;
	g_entnames[inew].pszname = NULL;
	g_entnames[inew].psznum = NULL;

	for (i = 0; i < CVOXGLOBMAX; i++)
	{
		g_entnames[inew].pszglobal[i] = NULL;
		g_entnames[inew].timestamp[i] = 0;
		g_entnames[inew].iseq[i] = 0;
		g_entnames[inew].timestampseq[i] = 0;
		g_entnames[inew].pszglobalseq[i] = NULL;
	}

	return inew;
}

// lookup random first word from this named group,
// return static, null terminated string

char * VOX_LookupRndVirtual( char *pGroupName )
{
	// get group index

	int isentenceg = VOX_GroupIndexFromName( pGroupName );
	
	if ( isentenceg < 0)
		return NULL;

	char szsentencename[32];
	
	// get pointer to sentence name within group, using lru

	int isentence = VOX_GroupPick( isentenceg, szsentencename, sizeof(szsentencename)-1 );
	
	if (isentence < 0)
		return NULL;
	
	// get pointer to sentence data

	char *psz = VOX_LookupString( szsentencename[0] == '!' ? szsentencename+1 : szsentencename, NULL);

	// strip trailing whitespace

	if (!psz)
		return NULL;

	char *pend = Q_strstr(psz, " ");
	if (pend)
		*pend = 0;

	// return pointer to first (and only) word

	return psz;
}

// given groupname, get pointer to first word of n'th sentence in group

char *VOX_LookupSentenceByIndex( char *pGroupname, int ipick, int *pipicknext )
{
	// get group index

	int isentenceg = VOX_GroupIndexFromName( pGroupname );
	
	if ( isentenceg < 0)
		return NULL;

	char szsentencename[32];
	
	// get pointer to sentence name within group, using lru

	int isentence = VOX_GroupPickSequential( isentenceg, szsentencename, sizeof(szsentencename)-1, ipick, true );
	
	if (isentence < 0)
		return NULL;
	
	// get pointer to sentence data

	char *psz = VOX_LookupString( szsentencename[0] == '!' ? szsentencename+1 : szsentencename, NULL);

	// strip trailing whitespace
	
	char *pend = Q_strstr(psz, " ");
	if (pend)
		*pend = 0;

	if (pipicknext)
		*pipicknext = isentence;

	// return pointer to first (and only) word
	return psz;
}

// lookup first word from this named group, group entry 'ipick',
// return static, null terminated string

char * VOX_LookupNumber( char *pGroupName, int ipick )
{
	// construct group name from V_NUMBERS + TYPE

	char sznumbers[16];
	int glen = Q_strlen(pGroupName);
	int slen = Q_strlen("V_NUMBERS");
	
	V_strcpy_safe(sznumbers, "V_NUMBERS");

	// insert type character
	sznumbers[slen] = pGroupName[glen-1];
	sznumbers[slen+1] = 0;

	return VOX_LookupSentenceByIndex( sznumbers, ipick, NULL );
}

// lookup ent & type, return static, null terminated string
// if no saved string, create one.
// UNDONE: init ent/type/string array, wrap when saving

char * VOX_LookupMyVirtual( int iname, char *pGroupName, char chtype, SoundSource soundsource)
{
	char *psz = NULL;
	char **ppsz = NULL;

	// get existing ent index, or index to new slot

	int ient = VOX_LookupEntIndex( (int)chtype, soundsource, true );

	if (iname == 1)
	{
		// lookup saved name

		psz = g_entnames[ient].pszname;
		ppsz = &(g_entnames[ient].pszname);
	}
	else
	{
		// lookup saved number

		psz = g_entnames[ient].psznum;
		ppsz = &(g_entnames[ient].psznum);
	}

	// if none found for this ent - pick one and save it

	if (psz == NULL)
	{
		// get new string
		psz = VOX_LookupRndVirtual( pGroupName );

		// save pointer to new string in g_entnames
		*ppsz = psz;
	}
	
	return psz;	
}

// get range or heading from ent to player,
// store range in from 1 to 3 words as ppszNew...ppszNew2
// store count of words in pcnew
// if fsimple is true, return numeric sequence based on ten digit max

void VOX_LookupRangeHeadingOrGrid( int irhg, char *pGroupName, channel_t *pChannel, SoundSource soundsource, char **ppszNew, char **ppszNew1, char **ppszNew2, int *pcnew, bool fsimple )
{
	Vector SL;				// sound -> listener vector
	char *phundreds = NULL;
	char *ptens = NULL;
	char *pones = NULL;
	int cnew = 0;
	float dist;
	int dmeters = 0;
	int hundreds, tens, ones;

	VectorSubtract(listener_origin, pChannel->origin, SL);

	if (irhg == 0)
	{
		// get range
		dist = VectorLength(SL);

		dmeters = (int)((dist * 2.54 / 100.0));	// convert inches to meters
	
		dmeters = clamp(dmeters, 0, 900);
	}
	else if (irhg == 1)
	{
		// get heading
		QAngle source_angles;

		source_angles.Init(0.0, 0.0, 0.0);

		VectorAngles( SL, source_angles );

		dmeters = source_angles[YAW];
	} else if (irhg == 2)
	{
		// get gridx
		dmeters = (int)(((16384 + listener_origin.x) * 2.54 / 100.0) / 10) % 20;
	}
	else if (irhg == 3)
	{
		// get gridy
		dmeters = (int)(((16384 + listener_origin.y) * 2.54 / 100.0) / 10) % 20;
	}

	dmeters = clamp(dmeters, 0, 999);

	// get hundreds, tens, ones

	hundreds = dmeters / 100;
	tens = (dmeters - hundreds * 100) / 10;
	ones = (dmeters - hundreds * 100 - tens * 10);
	

	if (fsimple)
	{
		// just return simple ten digit lookups for ones, tens, hundreds

		pones = VOX_LookupNumber( pGroupName, ones);
		cnew++;
		
		if (tens || hundreds)
		{
			ptens = VOX_LookupNumber( pGroupName, tens);
			cnew++;
		}

		if (hundreds)
		{
			phundreds = VOX_LookupNumber( pGroupName, hundreds );
			cnew++;
		}

		goto LookupNumExit;
	}

	// get pointer to string from groupname and number
	
	// 100,200,300,400,500,600,700,800,900
	if (hundreds && !tens && !ones)
	{
		if (hundreds <= 3)
		{
			phundreds = VOX_LookupNumber( pGroupName, 27 + hundreds);
			cnew++;
		}
		else
		{
			phundreds = VOX_LookupNumber( pGroupName, hundreds );
			ptens = VOX_LookupNumber( pGroupName, 0);
			pones = VOX_LookupNumber( pGroupName, 0);
			cnew++;
			cnew++;
		
		}
		goto LookupNumExit;
	}


	if ( hundreds ) 
	{
		// 101..999
		if (hundreds <= 3 && !tens && ones)
			phundreds = VOX_LookupNumber( pGroupName, 27 + hundreds);
		else
			phundreds = VOX_LookupNumber( pGroupName, hundreds );

		cnew++;

		// 101..109 to 901..909
		if (!tens && ones)
		{
			pones = VOX_LookupNumber( pGroupName, ones);
			cnew++;
			if (hundreds > 3)
			{
				ptens = VOX_LookupNumber( pGroupName, 0);
				cnew++;
			}
			goto LookupNumExit;
		}
	}

	// 1..19
	if (tens <= 1 && (tens || ones))
	{
		pones = VOX_LookupNumber( pGroupName, ones + tens * 10 );
		cnew++;
		tens = 0;
		goto LookupNumExit;
	}

	// 20..99
	if (tens > 1)
	{
		if (ones)
		{
			pones = VOX_LookupNumber( pGroupName, ones );
			cnew++;
		}
		
		ptens = VOX_LookupNumber( pGroupName, 18 + tens);
		cnew++;
	}


LookupNumExit:
	// return values

	*pcnew = cnew;

	// return
	switch (cnew)
	{
	default:
		*ppszNew = NULL;
		return;
	case 1: // 1..19,20,30,40,50,60,70,80,90,100,200,300
		*ppszNew	= pones ? pones : (ptens ? ptens : (phundreds ? phundreds : NULL));
		return;
	case 2: 
		if (ptens && pones)
		{
			*ppszNew	= ptens;
			*ppszNew1	= pones;
		}
		else if (phundreds && pones)
		{
			*ppszNew	= phundreds;
			*ppszNew1	= pones;
		}
		else if (phundreds && ptens)
		{
			*ppszNew	= phundreds;
			*ppszNew1	= ptens;
		}
		return;
	case 3:
		*ppszNew	= phundreds;
		*ppszNew1	= ptens;
		*ppszNew2	= pones;
		return;
	}
}

// find most recent ent of this type marked as dead

int VOX_LookupLastDeadIndex( int type )
{
	float timemax = -1;
	int ifound = -1;
	int i;

	for (i = 0; i < CENTNAMESMAX; i++)
	{
		if (g_entnames[i].type == type && g_entnames[i].fdied)
		{
			if (g_entnames[i].timedied >= timemax)
			{
				timemax = g_entnames[i].timedied;
				ifound = i;
			}
		}
	}

	return ifound;
}

ConVar snd_vox_globaltimeout("snd_vox_globaltimeout", "300"); // n second timeout to reset global vox words 
ConVar snd_vox_seqtimeout("snd_vox_seqtimetout", "300");		// n second timeout to reset global sequential vox words
ConVar snd_vox_sectimeout("snd_vox_sectimetout", "300");		// n second timeout to reset global sector id
ConVar snd_vox_captiontrace( "snd_vox_captiontrace", "0", 0, "Shows sentence name for sentences which are set not to show captions." );

// return index to ent which knows the current sector.
// if no ent found, alloc a new one and establish shector.
// sectors expire after approx 5 minutes.

#define VOXSECTORMAX			20

static float g_vox_lastsectorupdate = 0;
static int g_vox_isector = -1;

char *VOX_LookupSectorVirtual( char *pGroupname )
{
	float curtime = g_pSoundServices->GetClientTime();

	if (g_vox_isector == -1)
	{
		g_vox_isector = RandomInt(0, VOXSECTORMAX-1);
	}

// update sector every 5 min

	if (curtime - g_vox_lastsectorupdate > snd_vox_sectimeout.GetInt())
	{
		g_vox_isector++;
		if (g_vox_isector > VOXSECTORMAX)
			g_vox_isector = 1;
		g_vox_lastsectorupdate = curtime;
	}

	return VOX_LookupNumber( pGroupname, g_vox_isector );
}



char *VOX_LookupGlobalVirtual( int type, SoundSource soundsource, char *pGroupName, int iglobal )
{
	int i;
	float curtime = g_pSoundServices->GetClientTime();

	// look for ent of this type with un-expired global
	
	for (i = 0; i < CENTNAMESMAX; i++)
	{
		if (g_entnames[i].type == type)
		{
			if (curtime - g_entnames[i].timestamp[iglobal] <= snd_vox_globaltimeout.GetInt())
			{
				// if this ent has an un-expired global, return it, otherwise break

				if (g_entnames[i].pszglobal[iglobal])
					return g_entnames[i].pszglobal[iglobal];
				else
					break;
			}
		}
	}
	
	// if not found, construct a new global for this ent

	// pick random word from groupname

	char *psz = VOX_LookupRndVirtual( pGroupName );

	// get existing ent index, or index to new slot

	int ient = VOX_LookupEntIndex( type, soundsource, true ); 

	g_entnames[ient].timestamp[iglobal] = curtime;
	g_entnames[ient].pszglobal[iglobal] =  psz;

	return psz;
}

// lookup global values in group in sequence - get next value
// in sequence. sequence counter expires every 2.5 minutes.

char *VOX_LookupGlobalSeqVirtual( int type, SoundSource soundsource, char *pGroupName, int iglobal )
{

	int i;
	int ient;
	float curtime = g_pSoundServices->GetClientTime();

	// look for ent of this type with un-expired global
	
	for (i = 0; i < CENTNAMESMAX; i++)
	{
		if (g_entnames[i].type == type)
		{
			if (curtime - g_entnames[i].timestampseq[iglobal] <= (snd_vox_seqtimeout.GetInt()/2))
			{
				// if first ent found has an un-expired global sequence set, 
				// get next value in sequence, otherwise break

				ient = i;
				goto Pick_next;
			}
			else
			{
				// global has expired - reset sequence

				ient = i;
				g_entnames[ient].iseq[iglobal] = 0;
				goto Pick_next;
			}
		}
	}
	
	// if not found, construct a new sequential global for this ent

	ient = VOX_LookupEntIndex( type, soundsource, true ); 

	// pick next word from groupname
Pick_next:
	int ipick = g_entnames[ient].iseq[iglobal];
	int ipicknext = 0;

	char *psz = VOX_LookupSentenceByIndex( pGroupName, ipick, &ipicknext );
	g_entnames[ient].iseq[iglobal] = ipicknext;

	// get existing ent index, or index to new slot

	g_entnames[ient].timestampseq[iglobal] = curtime;
	g_entnames[ient].pszglobalseq[iglobal] =  psz;

	return psz;
}

// insert new words into rgpparseword at 'ireplace' slot

void VOX_InsertWords( int ireplace, int cnew, char *pszNew, char *pszNew1, char *pszNew2 )
{
	if ( cnew )
	{
		// make space in rgpparseword for 'cnew - 1' new words
		int ccopy = cnew - 1; // number of new slots we need
		int j;

		if (ccopy)
		{
			for (j = CVOXWORDMAX-1; j > ireplace + ccopy; j--)
				rgpparseword[j] = rgpparseword[j - ccopy ];
		}

		// replace rgpparseword entry(s) with the substitued name(s)

		rgpparseword[ireplace] = pszNew;

		if ( cnew == 2 || cnew == 3)
			rgpparseword[ireplace+1] = pszNew1;

		if ( cnew == 3 )
			rgpparseword[ireplace+2] = pszNew2;
	}
}

// remove 'silent' word from rgpparseword

void VOX_DeleteWord( int iword )
{
	if (iword < 0 || iword >= CVOXWORDMAX)
		return;

	rgpparseword[iword] = 0;

	// slide all words > iword up into vacated slot

	for (int j = iword; j < CVOXWORDMAX-1; j++)
		rgpparseword[j] = rgpparseword[j+1];
}


// get global list of map names from sentences.txt
// map names are stored in order in V_MAPNAMES group

void VOX_LookupMapnames( void )
{
	// get group V_MAPNAMES

	int i;
	char *psz;
	int inext = 0;

	for (i = 0; i < CVOXMAPNAMESMAX; i++)
	{
		// step sequentially through group - return ptr to 1st word in each group (map name)

		psz = VOX_LookupSentenceByIndex( "V_MAPNAME", i, &inext );

		if (!psz)
			return;

		g_rgmapnames[i] = psz;
		g_cmapnames++;
	}
}

// get index of current map name
// return 0 as default index if not found

int VOX_GetMapNameIndex( const char *pszmapname )
{
	for (int i = 0; i < g_cmapnames; i++)
	{
		if ( Q_strstr( pszmapname, g_rgmapnames[i] ) )
			return i;
	}
	return 0;
}

// look for virtual 'V_' values in rgpparseword.
	// V_MYNAME - replace with saved name value (based on type + entity)
	//			- if no saved name, create one and save
	// V_MYNUM  - replace with saved number value (based on type + entity)
	//			- if no saved num, create on and save
	// V_RNDNUM	- grab a random number string from V_RNDNUM_<type>
	// V_RNDNAME - grab a random name string from V_RNDNAME_<type>

	// replace any 'V_' values with actual string names in rgpparseword

extern ConVar	host_map;
inline bool IsVirtualName( const char *pName )
{
	return (pName[0] == 'V' && pName[1] == '_');
}

void VOX_ReplaceVirtualNames( channel_t *pchan )
{
	// for each word in the sentence, check for V_, if found
	// replace virtual word with saved word or rnd word

	int i = 0;
	char *pszNew = NULL;
	char *pszNew1 = NULL;
	char *pszNew2 = NULL;
	int iname = -1;
	int cnew = 0;
	bool fbymap;
	char *pszmaptoken;
	SoundSource	soundsource = pchan ? pchan->soundsource : 0;

	const char *pszmap = host_map.GetString();

	// get global list of map names from sentences.txt

	while (rgpparseword[i])
	{
		
		if ( IsVirtualName( rgpparseword[i] ) )
		{
			iname = -1;
			cnew = 0;
			pszNew = NULL;
			pszNew1 = NULL;
			pszNew2 = NULL;
			char szparseword[256];
			
			int slen = Q_strlen(rgpparseword[i]);
			char chtype = rgpparseword[i][slen-1];

			// copy word to temp location so we can perform in-place substitutions

			V_strcpy_safe(szparseword, rgpparseword[i]);

			// fbymap is true if lookup is performed via mapname instead of via ordinal

			pszmaptoken = ( Q_strstr(szparseword, "_MAP__") );

			fbymap = (pszmaptoken == NULL ? false : true);

			if (fbymap)
			{
				int imap = VOX_GetMapNameIndex( pszmap );
				imap = clamp (imap, 0, 99);
				
				// replace last 2 characters in _MAP__ substring
				// with imap - this effectively makes all 
				// '_map_' lookups relative to the mapname
				if ( imap >= 10 )
				{
					pszmaptoken[4] = (imap/10) + '0';
					pszmaptoken[5] = (imap%10) + '0';
				}
				else
				{
					pszmaptoken[4] = '0';
					pszmaptoken[5] = imap + '0';
				}
			}
			
			if ( Q_strstr(szparseword, "V_MYNAME") )
			{
				iname = 1;
			}
			else if ( Q_strstr(szparseword, "V_MYNUM") )
			{
				iname = 0;
			}
			
			if ( iname >= 0 )
			{
	
				// lookup ent & type, return static, null terminated string
				// if no saved string, create one

				pszNew = VOX_LookupMyVirtual( iname, szparseword, chtype, soundsource);
				cnew = 1;
			}
			else
			{
				if ( Q_strstr(szparseword, "V_RND") )
				{
					// lookup random first word from this named group,
					// return static, null terminated string

					pszNew = VOX_LookupRndVirtual( szparseword );
					cnew = 1;
				}
				else if ( Q_strstr(szparseword, "V_DIST") )
				{
					// get range from ent to player, return pointers to new words
					VOX_LookupRangeHeadingOrGrid( 0, szparseword, pchan, soundsource, &pszNew, &pszNew1, &pszNew2, &cnew, true );
				}
				else if ( Q_strstr(szparseword, "V_DIR") )
				{
					// get heading from ent to player, return pointers to new words
					VOX_LookupRangeHeadingOrGrid( 1, szparseword, pchan, soundsource, &pszNew, &pszNew1, &pszNew2, &cnew, false);
				}
				else if ( Q_strstr(szparseword, "V_IDIED") )
				{
					// SILENT MARKER - this ent died - mark as dead and timestamp

					int ient =  VOX_LookupEntIndex( chtype, soundsource, false);
					if (ient < 0)
					{
						// if not found, allocate new ent, give him a name & number, mark as dead
						char szgroup1[32];
						char szgroup2[32];
						V_strcpy_safe(szgroup1, "V_MYNAME");
						szgroup1[8] = chtype;
						szgroup1[9] = 0;

						V_strcpy_safe(szgroup2, "V_MYNUM");
						szgroup2[7] = chtype;
						szgroup2[8] = 0;

						ient =  VOX_LookupEntIndex( chtype, soundsource, true);
						g_entnames[ient].pszname = VOX_LookupRndVirtual( szgroup1 );
						g_entnames[ient].psznum  = VOX_LookupRndVirtual( szgroup2 );
					}

					g_entnames[ient].fdied = true;
					g_entnames[ient].timedied = g_pSoundServices->GetClientTime();

					// clear this 'silent' word from rgpparseword

					VOX_DeleteWord(i);

				}
				else if ( Q_strstr(szparseword, "V_WHODIED") )
				{
					// get last dead unit of this type

					int ient = VOX_LookupLastDeadIndex( chtype );

					// get name and number

					if (ient >= 0)
					{
						cnew = 1;
						pszNew = g_entnames[ient].pszname;
						pszNew1 = g_entnames[ient].psznum;
						if (pszNew1)
							cnew++;
					}
					else
					{
						// no dead units, just clear V_WHODIED

						VOX_DeleteWord(i);
					}

				}
				else if ( Q_strstr(szparseword, "V_SECTOR") )
				{
					// sectors are fictional - they simply
					// increase sequentially and expire every 5 minutes

					pszNew = VOX_LookupSectorVirtual( szparseword );
					if (pszNew)
						cnew = 1;
				}
				else if ( Q_strstr(szparseword, "V_GRIDX") )
				{
					// player x position in 10 meter increments
					VOX_LookupRangeHeadingOrGrid( 2, szparseword, pchan, soundsource, &pszNew, &pszNew1, &pszNew2, &cnew, true );
				}
				else if ( Q_strstr(szparseword, "V_GRIDY") )
				{
					// player y position in 10 meter increments
					VOX_LookupRangeHeadingOrGrid( 3, szparseword, pchan, soundsource, &pszNew, &pszNew1, &pszNew2, &cnew, true );
				
				}
				else if ( Q_strstr(szparseword, "V_G0_") )
				{
					// 4 rnd globals per type, globals expire after 5 minutes
					// used for target designation, master sector code name etc.

					pszNew = VOX_LookupGlobalVirtual( chtype, soundsource, szparseword, 0 );
					if (pszNew)
						cnew = 1;
				}
				else if ( Q_strstr(szparseword, "V_G1_") )
				{
					// 4 rnd globals per type, globals expire after 5 minutes
					// used for target designation, master sector code name etc.

					pszNew = VOX_LookupGlobalVirtual( chtype, soundsource, szparseword, 1 );
					if (pszNew)
						cnew = 1;
				}
				else if ( Q_strstr(szparseword, "V_G2_") )
				{
					// 4 rnd globals per type, globals expire after 5 minutes
					// used for target designation, master sector code name etc.

					pszNew = VOX_LookupGlobalVirtual( chtype, soundsource, szparseword, 2 );
					if (pszNew)
						cnew = 1;
				}
				else if ( Q_strstr(szparseword, "V_G3_") )
				{
					// 4 rnd globals per type, globals expire after 5 minutes
					// used for target designation, master sector code name etc.

					pszNew = VOX_LookupGlobalVirtual( chtype, soundsource, szparseword, 3 );
					if (pszNew)
						cnew = 1;
				}
				else if ( Q_strstr(szparseword, "V_SEQG0_") )
				{
					// 4 sequential globals per type, selected sequentially in list
					// used for total target hit count etc.

					pszNew = VOX_LookupGlobalSeqVirtual( chtype, soundsource, szparseword, 0 );
					if (pszNew)
						cnew = 1;
				}
				else if ( Q_strstr(szparseword, "V_SEQG1_") )
				{
					// 4 sequential globals per type, selected sequentially in list
					// used for total target hit count etc.

					pszNew = VOX_LookupGlobalSeqVirtual( chtype, soundsource, szparseword, 1 );
					if (pszNew)
						cnew = 1;
				}
				else if ( Q_strstr(szparseword, "V_SEQG2_") )
				{
					// 4 sequential globals per type, selected sequentially in list
					// used for total target hit count etc.

					pszNew = VOX_LookupGlobalSeqVirtual( chtype, soundsource, szparseword, 2 );
					if (pszNew)
						cnew = 1;
				}
				else if ( Q_strstr(szparseword, "V_SEQG3_") )
				{
					// 4 sequential globals per type, selected sequentially in list
					// used for total target hit count etc.

					pszNew = VOX_LookupGlobalSeqVirtual( chtype, soundsource, szparseword, 3 );
					if (pszNew)
						cnew = 1;
				}

			}

			// insert up to 3 new words into rgpparseword at 'i' location

			VOX_InsertWords( i, cnew, pszNew, pszNew1, pszNew2 );
		}
		i++;
	}
}

void VOX_Precache( IEngineSound *pSoundSystem, int sentenceIndex, const char *pPathOverride = NULL )
{
	voxword_t	rgvoxword[CVOXWORDMAX];
	char		buffer[512];
	char		szpath[MAX_PATH];
	char		pathbuffer[MAX_PATH];
	char		*pWords[CVOXWORDMAX];	// array of pointers to parsed words

	if ( !IsVirtualName(g_Sentences[sentenceIndex].pName))
	{
		g_Sentences[sentenceIndex].isPrecached = true;
	}

	memset(rgvoxword, 0, sizeof (voxword_t) * CVOXWORDMAX);
	char *psz = (char *)(g_Sentences[sentenceIndex].pName + Q_strlen(g_Sentences[sentenceIndex].pName) + 1);
	// get directory from string, advance psz
	psz = VOX_GetDirectory(szpath, sizeof( szpath ), psz );
	Q_strncpy(buffer, psz, sizeof( buffer ) );
	psz = buffer;
	if ( pPathOverride )
	{
		Q_strncpy(szpath, pPathOverride, sizeof(szpath));
	}

	// parse sentence (also inserts null terminators between words)

	VOX_ParseString(psz);
	int i = 0, count = 0;
	// copy the parsed words out of the globals
	for ( i = 0; rgpparseword[i]; i++ )
	{
		pWords[i] = rgpparseword[i];
		count++;
	}
	int cword = 0;
	for ( i = 0; i < count; i++ )
	{
		if ( IsVirtualName(pWords[i]) )
		{
			CUtlVector< WordBuf > list;

			VOX_BuildVirtualNameList( pWords[i], list );

			int c = list.Count();
			for ( int j = 0 ; j < c; ++j )
			{
				Q_snprintf( pathbuffer, sizeof( pathbuffer ), "%s%s.wav", szpath, list[j].word );
				pSoundSystem->PrecacheSound( pathbuffer, false );
			}
		}
		else
		{
			// Get any pitch, volume, start, end params into voxword
			if (VOX_ParseWordParams(pWords[i], &rgvoxword[cword], i == 0))
			{
				// this is a valid word (as opposed to a parameter block)
				Q_snprintf( pathbuffer, sizeof( pathbuffer ), "%s%s.wav", szpath, pWords[i] );
				// find name, if already in cache, mark voxword
				// so we don't discard when word is done playing
				pSoundSystem->PrecacheSound( pathbuffer, false );
				cword++;
			}
		}
	}
}

void VOX_PrecacheSentenceGroup( IEngineSound *pSoundSystem, const char *pGroupName, const char *pPathOverride )
{
	int i;

	int len = Q_strlen( pGroupName );
	for ( i = 0; i < g_Sentences.Count(); i++ )
	{
		if ( !g_Sentences[i].isPrecached && !Q_strncasecmp( g_Sentences[i].pName, pGroupName, len ) )
		{
			VOX_Precache( pSoundSystem, i, pPathOverride );
		}
	}
}


// link all sounds in sentence, start playing first word.
// return number of words loaded
void VOX_LoadSound( channel_t *pchan, const char *pszin )
{
#ifndef SWDS
	char		buffer[512];
	int			i, cword;
	char		pathbuffer[MAX_PATH];
	char		szpath[MAX_PATH];
	voxword_t	rgvoxword[CVOXWORDMAX];
	char		*psz;
	bool		emitcaption = false;
	CUtlSymbol	captionSymbol = UTL_INVAL_SYMBOL;
	float		duration = 0.0f;

	if (!pszin)
		return;

	memset(rgvoxword, 0, sizeof (voxword_t) * CVOXWORDMAX);
	memset(buffer, 0, sizeof(buffer));

	// lookup actual string in g_Sentences, 
	// set pointer to string data

	psz = VOX_LookupString(pszin, NULL, &emitcaption, &captionSymbol, &duration );

	if (!psz)
	{
		DevMsg ("VOX_LoadSound: no sentence named %s\n",pszin);
		return;
	}

	// get directory from string, advance psz
	psz = VOX_GetDirectory(szpath, sizeof( szpath ), psz );

	if ( Q_strlen(psz) > sizeof(buffer) - 1 )
	{
		DevMsg ("VOX_LoadSound: sentence is too long %s\n",psz);
		return;
	}

	// copy into buffer
	Q_strncpy(buffer, psz, sizeof( buffer ) );
	psz = buffer;

	// parse sentence (also inserts null terminators between words)
	
	VOX_ParseString(psz);

	// replace any 'V_' values with actual string names in rgpparseword

	VOX_ReplaceVirtualNames( pchan );

	// for each word in the sentence, construct the filename,
	// lookup the sfx and save each pointer in a temp array	

	i = 0;
	cword = 0;

	char captionstream[ 1024 ];

	char groupname[ 512 ];
	Q_strncpy( groupname, pszin, sizeof( groupname ) );

	int len = Q_strlen( groupname );

	while ( len > 0 && V_isdigit( groupname[ len - 1 ] ) )
	{
		groupname[ len - 1 ] = 0;
		--len;
	}
	
	Q_snprintf( captionstream, sizeof( captionstream ), "%s ", groupname );

	while (rgpparseword[i])
	{
		// Get any pitch, volume, start, end params into voxword

		if (VOX_ParseWordParams(rgpparseword[i], &rgvoxword[cword], i == 0))
		{
			// this is a valid word (as opposed to a parameter block)
			Q_snprintf( pathbuffer, sizeof( pathbuffer ), "%s%s.wav", szpath, rgpparseword[i] );

			// find name, if already in cache, mark voxword
			// so we don't discard when word is done playing
			rgvoxword[cword].sfx = S_FindName(pathbuffer, 
					&(rgvoxword[cword].fKeepCached));
			// JAY: HACKHACK: Keep all sentences cached for now
			rgvoxword[cword].fKeepCached = 1;

			char captiontoken[ 128 ];
			Q_snprintf( captiontoken, sizeof( captiontoken ), "S(%s%s) ", szpath, rgpparseword[i] );

			Q_strncat( captionstream, captiontoken, sizeof( captionstream ), COPY_ALL_CHARACTERS );

			cword++;
		}
		i++;
	}

	pchan->pMixer = NULL;

	if (cword)
	{
		// some 'virtual' sentences can end up with 0 words
		// if no words, then pchan->pMixer is null; chan will be released right away.

		pchan->pMixer = CreateSentenceMixer( rgvoxword );
		if ( !pchan->pMixer )
			return;

		pchan->flags.isSentence = true;
		pchan->sfx = rgvoxword[0].sfx;
		Assert(pchan->sfx);

		if ( g_pSoundServices )
		{
			if ( emitcaption )
			{
				if ( captionSymbol != UTL_INVAL_SYMBOL )
				{
					g_pSoundServices->EmitCloseCaption( captionSymbol.String(), duration );

					if ( snd_vox_captiontrace.GetBool() )
					{
						Msg( "Vox: caption '%s'\n", captionSymbol.String() );
					}
				}
				else
				{
					g_pSoundServices->EmitSentenceCloseCaption( captionstream );

					if ( snd_vox_captiontrace.GetBool() )
					{
						Msg( "Vox: captionstream '%s'\n", captionstream );
					}
				}
			}
			else
			{
				if ( snd_vox_captiontrace.GetBool() )
				{
					Msg( "Vox:  No caption for '%s'\n", pszin ? pszin : "NULL" );
				}
			}
		}
	}

#endif
}

static bool CCPairLessFunc( const ccpair& lhs, const ccpair& rhs )
{
	return Q_stricmp( lhs.token.word, rhs.token.word ) < 0;
}

void VOX_AddNumbers( char *pGroupName, CUtlVector< WordBuf >& list )
{
	// construct group name from V_NUMBERS + TYPE
	for ( int i = 0; i <= 30; ++i )
	{
		char sznumbers[16];
		int glen = Q_strlen(pGroupName);
		int slen = Q_strlen("V_NUMBERS");
		
		V_strcpy_safe(sznumbers, "V_NUMBERS");

		// insert type character
		sznumbers[slen] = pGroupName[glen-1];
		sznumbers[slen+1] = 0;

		WordBuf w;
		// w.Set( VOX_LookupString( VOX_LookupSentenceByIndex( sznumbers, i, NULL ), NULL ) );
		w.Set( VOX_LookupSentenceByIndex( sznumbers, i, NULL ) );
		list.AddToTail( w );
	}
}

void VOX_AddRndVirtual( char *pGroupName, CUtlVector< WordBuf >& list )
{
	// get group index

	int isentenceg = VOX_GroupIndexFromName( pGroupName );
	
	if ( isentenceg < 0)
		return;

	char szsentencename[32];

	char const *szgroupname = g_SentenceGroups[ isentenceg ].GroupName();
	
	// get pointer to sentence name within group, using lru
	for ( int snum = 0; snum < g_SentenceGroups[ isentenceg ].count; ++snum )
	{
		Q_snprintf( szsentencename, sizeof( szsentencename ), "%s%d", szgroupname, snum );

		char *psz = VOX_LookupString( szsentencename[0] == '!' ? szsentencename+1 : szsentencename, NULL);

		if ( psz )
		{
			WordBuf w;
			w.Set( psz );
			list.AddToTail( w );
		}
	}
}

void VOX_AddMyVirtualWords( int iname, char *pGroupName, char chtype, CUtlVector< WordBuf >& list )
{
	VOX_AddRndVirtual( pGroupName, list );
}

void VOX_BuildVirtualNameList( char *word, CUtlVector< WordBuf >& list )
{
	// for each word in the sentence, check for V_, if found
	// replace virtual word with saved word or rnd word

	int iname = -1;
	bool fbymap;
	char *pszmaptoken;


	char szparseword[256];
	
	int slen = Q_strlen(word);
	char chtype = word[slen-1];

	// copy word to temp location so we can perform in-place substitutions

	Q_strncpy( szparseword, word, sizeof( szparseword ) );

	// fbymap is true if lookup is performed via mapname instead of via ordinal

	pszmaptoken = ( Q_strstr(szparseword, "_MAP__") );

	fbymap = (pszmaptoken == NULL ? false : true);

	if (fbymap)
	{
		for ( int imap = 0; imap < g_cmapnames; ++imap )
		{
			// replace last 2 characters in _MAP__ substring
			// with imap - this effectively makes all 
			// '_map_' lookups relative to the mapname
			pszmaptoken[4] = '0';
			if (imap < 10)
				Q_snprintf( &(pszmaptoken[5]), 1, "%1d", imap );	
			else
				Q_snprintf( &(pszmaptoken[4]), 2, "%d", imap );

			// Recurse...
			VOX_BuildVirtualNameList( szparseword, list );
		}
		return;
	}
	
	if ( Q_strstr(szparseword, "V_MYNAME") )
	{
		iname = 1;
	}
	else if ( Q_strstr(szparseword, "V_MYNUM") )
	{
		iname = 0;
	}
	
	if ( iname >= 0 )
	{

		// lookup ent & type, return static, null terminated string
		// if no saved string, create one

		VOX_AddMyVirtualWords( iname, szparseword, chtype, list );
	}
	else
	{
		if ( Q_strstr(szparseword, "V_RND") )
		{
			// lookup random first word from this named group,
			// return static, null terminated string
			VOX_AddRndVirtual( szparseword, list );
		}
		else if ( Q_strstr(szparseword, "V_DIST") )
		{
			VOX_AddNumbers( szparseword, list );
		}
		else if ( Q_strstr(szparseword, "V_DIR") )
		{
			VOX_AddNumbers( szparseword, list );
		}
		else if ( Q_strstr(szparseword, "V_IDIED") )
		{
			// SILENT MARKER - this ent died - mark as dead and timestamp

			// if not found, allocate new ent, give him a name & number, mark as dead
			char szgroup1[32];
			char szgroup2[32];
			V_strcpy_safe(szgroup1, "V_MYNAME");
			szgroup1[8] = chtype;
			szgroup1[9] = 0;

			V_strcpy_safe(szgroup2, "V_MYNUM");
			szgroup2[7] = chtype;
			szgroup2[8] = 0;

			VOX_BuildVirtualNameList( szgroup1, list );
			VOX_BuildVirtualNameList( szgroup2, list );
			return;

		}
		else if ( Q_strstr(szparseword, "V_WHODIED") )
		{
			// get last dead unit of this type
			/*

			int ient = VOX_LookupLastDeadIndex( chtype );

			// get name and number

			if (ient >= 0)
			{
				cnew = 1;
				pszNew = g_entnames[ient].pszname;
				pszNew1 = g_entnames[ient].psznum;
				if (pszNew1)
					cnew++;
			}
			else
			{
				// no dead units, just clear V_WHODIED

				VOX_DeleteWord(i);
			}
			*/

		}
		else if ( Q_strstr(szparseword, "V_SECTOR") )
		{
			VOX_AddNumbers( szparseword, list );
		}
		else if ( Q_strstr(szparseword, "V_GRIDX") )
		{
			VOX_AddNumbers( szparseword, list );
		}
		else if ( Q_strstr(szparseword, "V_GRIDY") )
		{
			VOX_AddNumbers( szparseword, list );
		}
		else if ( Q_strstr(szparseword, "V_G0_") )
		{
			VOX_AddRndVirtual( szparseword, list );
		}
		else if ( Q_strstr(szparseword, "V_G1_") )
		{
			VOX_AddRndVirtual( szparseword, list );
		}
		else if ( Q_strstr(szparseword, "V_G2_") )
		{
			VOX_AddRndVirtual( szparseword, list );
		}
		else if ( Q_strstr(szparseword, "V_G3_") )
		{
			VOX_AddRndVirtual( szparseword, list );
		}
		else if ( Q_strstr(szparseword, "V_SEQG0_") )
		{
			VOX_AddRndVirtual( szparseword, list );
		}
		else if ( Q_strstr(szparseword, "V_SEQG1_") )
		{
			VOX_AddRndVirtual( szparseword, list );
		}
		else if ( Q_strstr(szparseword, "V_SEQG2_") )
		{
			VOX_AddRndVirtual( szparseword, list );
		}
		else if ( Q_strstr(szparseword, "V_SEQG3_") )
		{
			VOX_AddRndVirtual( szparseword, list );
		}

	}

	if ( Q_strnicmp( szparseword, "V_", 2 ) )
	{
		WordBuf w;
		w.Set( szparseword );
		list.AddToTail( w );
	}
}

//-----------------------------------------------------------------------------
// Purpose: For generating reslists, adds the wavefile to the dictionary
// Input  : *fn - 
//-----------------------------------------------------------------------------
void VOX_Touch( char const *fn, CUtlDict< int, int >& list )
{
	if ( list.Find( fn ) == list.InvalidIndex() )
	{
		list.Insert( fn );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Iterates the touch list and touches all referenced .wav files.
// Input  : int - 
//			list - 
//-----------------------------------------------------------------------------
void VOX_TouchSounds( CUtlDict< int, int >& list, CUtlRBTree< ccpair, int >& ccpairs, bool spewsentences )
{
	int i;
	for ( i = list.First(); i != list.InvalidIndex(); i = list.Next( i ) )
	{
		char const *fn = list.GetElementName( i );
		
		// Msg( "touch %s\n", fn );
		char expanded[ 512 ];
		Q_snprintf( expanded, sizeof( expanded ), "sound/%s", fn );

		FileHandle_t fh = g_pFileSystem->Open( expanded, "rb" );
		if ( FILESYSTEM_INVALID_HANDLE != fh )
		{
			g_pFileSystem->Close( fh );
		}
	}

	if ( spewsentences )
	{
		for ( i = ccpairs.FirstInorder() ; i != ccpairs.InvalidIndex(); i = ccpairs.NextInorder( i ) )
		{
			ccpair& pair = ccpairs[ i ];

			Msg( "\"%s\"\t\"%s\"\n",
				pair.token.word,
				pair.value.word );
		}

		FileHandle_t fh = g_pFileSystem->Open( "sentences.m3u", "wt", "GAME" );
		if ( FILESYSTEM_INVALID_HANDLE != fh )
		{
			for ( i = ccpairs.FirstInorder() ; i != ccpairs.InvalidIndex(); i = ccpairs.NextInorder( i ) )
			{
				ccpair& pair = ccpairs[ i ];

				char outline[ 512 ];
				Q_snprintf( outline, sizeof( outline ), "%s\n", pair.fullpath.word );

				g_pFileSystem->Write( outline, Q_strlen(outline), fh );
			}

			g_pFileSystem->Close( fh );
		}
	}
}

// link all sounds in sentence, start playing first word.
// return number of words loaded
void VOX_TouchSound( const char *pszin, CUtlDict< int, int >& filelist, CUtlRBTree< ccpair, int >& ccpairs, bool spewsentences )
{
#ifndef SWDS
	char		buffer[512];
	int			i, cword;
	char		pathbuffer[MAX_PATH];
	char		szpath[MAX_PATH];
	voxword_t	rgvoxword[CVOXWORDMAX];
	char		*psz;

	if (!pszin)
		return;

	memset(rgvoxword, 0, sizeof (voxword_t) * CVOXWORDMAX);
	memset(buffer, 0, sizeof(buffer));

	// lookup actual string in g_Sentences, 
	// set pointer to string data

	psz = VOX_LookupString(pszin, NULL);

	if (!psz)
	{
		DevMsg ("VOX_TouchSound: no sentence named %s\n",pszin);
		return;
	}

	// get directory from string, advance psz
	psz = VOX_GetDirectory(szpath, sizeof( szpath ), psz );

	if ( Q_strlen(psz) > sizeof(buffer) - 1 )
	{
		DevMsg ("VOX_TouchSound: sentence is too long %s\n",psz);
		return;
	}

	// copy into buffer
	Q_strncpy(buffer, psz, sizeof( buffer ) );
	psz = buffer;

	// parse sentence (also inserts null terminators between words)
	
	VOX_ParseString(psz);

	// for each word in the sentence, construct the filename,
	// lookup the sfx and save each pointer in a temp array	

	i = 0;
	cword = 0;

	CUtlVector< WordBuf > rep;

	while (rgpparseword[i])
	{
		// Get any pitch, volume, start, end params into voxword

		if ( VOX_ParseWordParams(rgpparseword[i], &rgvoxword[cword], i == 0 ) )
		{
			// Iterate all virtuals here...
			if ( !Q_strnicmp( rgpparseword[i], "V_", 2 ) )
			{
				CUtlVector< WordBuf > list;

				VOX_BuildVirtualNameList( rgpparseword[i], list );
				
				int c = list.Count();
				for ( int j = 0 ; j < c; ++j )
				{
					char name[ 256 ];
					Q_snprintf( name, sizeof( name ), "%s", list[ j ].word );

					if ( !Q_strnicmp( name, "V_", 2 ) )
					{
						Warning( "VOX_TouchSound didn't resolve virtual token %s!\n", name );
					}

					Q_snprintf( pathbuffer, sizeof( pathbuffer ), "%s%s.wav", szpath, name );
					VOX_Touch( pathbuffer, filelist );

					WordBuf w;
					if ( j == 0 )
					{
						w.Set( name );
						rep.AddToTail( w );
					}
					ccpair pair;
					Q_snprintf( pair.token.word, sizeof( pair.token.word ), "S(%s%s)", szpath, name );
					pair.value.Set( name );

					Q_snprintf( pathbuffer, sizeof( pathbuffer ), "%s/sound/%s%s.wav", g_pSoundServices->GetGameDir(), szpath, name );
					Q_FixSlashes( pathbuffer, '\\' );
					pair.fullpath.Set( pathbuffer );

					if ( ccpairs.Find( pair ) == ccpairs.InvalidIndex() )
					{
						ccpairs.Insert( pair );
					}
				}
			}
			else
			{
				// this is a valid word (as opposed to a parameter block)
				Q_snprintf( pathbuffer, sizeof( pathbuffer ), "%s%s.wav", szpath, rgpparseword[i] );
				VOX_Touch( pathbuffer, filelist );

				WordBuf w;
				w.Set( rgpparseword[ i ] );
				rep.AddToTail( w );

				ccpair pair;
				Q_snprintf( pair.token.word, sizeof( pair.token.word ), "S(%s%s)", szpath, rgpparseword[i] );
				pair.value.Set( rgpparseword[i] );

				Q_snprintf( pathbuffer, sizeof( pathbuffer ), "%s/sound/%s%s.wav", g_pSoundServices->GetGameDir(), szpath, rgpparseword[ i ] );
				Q_FixSlashes( pathbuffer, CORRECT_PATH_SEPARATOR );
				pair.fullpath.Set( pathbuffer );

				if ( ccpairs.Find( pair ) == ccpairs.InvalidIndex() )
				{
					ccpairs.Insert( pair );
				}
			}
		}
		i++;
	}

	if ( spewsentences )
	{
		char outbuf[ 1024 ];
		// Build representative text
		outbuf[ 0 ] = 0;
		for ( i = 0; i < rep.Count(); ++i )
		{
			/*
			if ( !Q_stricmp( rep[ i ].word, "_comma" ) )
			{
				if ( i != 0 && Q_strlen( outbuf ) >= 1 )
				{
					outbuf[ Q_strlen( outbuf ) - 1 ] =0;
				}

				// Don't end sentence with comma..
				if ( i != rep.Count() - 1 )
				{
					Q_strncat( outbuf, ", ", sizeof( outbuf ), COPY_ALL_CHARACTERS );
				}
				continue;
			}
			*/

			Q_strncat( outbuf, rep[ i ].word, sizeof( outbuf ), COPY_ALL_CHARACTERS );
			if ( i != rep.Count() - 1 )
			{
				Q_strncat( outbuf, " ", sizeof( outbuf ), COPY_ALL_CHARACTERS );
			}
		}

		Msg( "     %s\n", outbuf );
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Take a NULL terminated sentence, and parse any commands contained in
//			{}.  The string is rewritten in place with those commands removed.
//
// Input  : *pSentenceData - sentence data to be modified in place
//			sentenceIndex - global sentence table index for any data that is 
//							parsed out
//-----------------------------------------------------------------------------
void VOX_ParseLineCommands( char *pSentenceData, int sentenceIndex )
{
	char tempBuffer[512];
	char *pNext, *pStart;
	int  length, tempBufferPos = 0;

	if ( !pSentenceData )
		return;

	pStart = pSentenceData;

	while ( *pSentenceData )
	{
		pNext = ScanForwardUntil( pSentenceData, '{' );

		// Find length of "good" portion of the string (not a {} command)
		length = pNext - pSentenceData;
		if ( tempBufferPos + length > sizeof(tempBuffer) )
		{
			DevMsg("Error! sentence too long!\n" );
			return;
		}

		// Copy good string to temp buffer
		memcpy( tempBuffer + tempBufferPos, pSentenceData, length );
		
		// Move the copy position
		tempBufferPos += length;

		pSentenceData = pNext;

		// Skip ahead of the opening brace
		if ( *pSentenceData )
		{
			pSentenceData++;
		}

		while ( 1 )
		{
			// Skip whitespace
			while ( *pSentenceData && *pSentenceData <= 32 )
			{
				pSentenceData++;
			}

			// Simple comparison of string commands:
			switch( tolower( *pSentenceData ) )
			{
			case 'l':
				// All commands starting with the letter 'l' here
				if ( !Q_strnicmp( pSentenceData, "len", 3 ) )
				{
					g_Sentences[sentenceIndex].length = atof( pSentenceData + 3 ) ;

					// "len " len + space
					pSentenceData += 4;

					// Skip until next } or whitespace character
					while ( *pSentenceData && ( *pSentenceData != '}' && !( *pSentenceData <= 32 ) ) )
						pSentenceData++;
				}
				break;
			case 'c':
				// This sentence should emit a close caption
				if ( !Q_strnicmp( pSentenceData, "closecaption", 12 ) )
				{
					g_Sentences[sentenceIndex].closecaption = true;

					pSentenceData += 12;

					pSentenceData = (char *)COM_Parse( pSentenceData );

					// Skip until next } or whitespace character
					while ( *pSentenceData && ( *pSentenceData != '}' && !( *pSentenceData <= 32 ) ) )
						pSentenceData++;

					if ( Q_strlen( com_token ) > 0 )
					{
						g_Sentences[sentenceIndex].caption = com_token;
					}
					else
					{
						g_Sentences[sentenceIndex].caption = UTL_INVAL_SYMBOL;
					}
				}
				break;
			case 0:
			default:
				{
					// Skip until next } or whitespace character
					while ( *pSentenceData && ( *pSentenceData != '}' && !( *pSentenceData <= 32 ) ) )
						pSentenceData++;
				}
				break;
			}

			// Done?
			if ( !*pSentenceData || *pSentenceData == '}' )
			{
				break;
			}
		}

		// pSentenceData = ScanForwardUntil( pSentenceData, '}' );
		
		// Skip the closing brace
		if ( *pSentenceData )
			pSentenceData++;

		// Skip trailing whitespace
		while ( *pSentenceData && *pSentenceData <= 32 )
			pSentenceData++;
	}

	if ( tempBufferPos < sizeof(tempBuffer) )
	{
		// terminate cleaned up copy
		tempBuffer[ tempBufferPos ] = 0;
		
		// Copy it over the original data
		Q_strcpy( pStart, tempBuffer );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add a new group or increment count of the existing one
// Input  : *pSentenceName - text of the sentence name
//-----------------------------------------------------------------------------
int VOX_GroupAdd( const char *pSentenceName )
{
	int len = strlen( pSentenceName ) - 1;

	// group members end in a number
	if ( len <= 0 || !V_isdigit(pSentenceName[len]) )
		return -1;

	// truncate away the index
	while ( len > 0 && V_isdigit(pSentenceName[len]) )
	{
		len--;
	}

	// make a copy of the actual group name
	char *groupName = (char *)stackalloc( len + 2 );
	Q_strncpy( groupName, pSentenceName, len+2 );

	// check for it in the list
	int i;
	sentencegroup_t *pGroup;

	CUtlSymbol symGroupName = sentencegroup_t::GetSymbol( groupName );
	int groupCount = g_SentenceGroups.Size();
	for ( i = 0; i < groupCount; i++ )
	{
		int groupIndex = (i + groupCount-1) % groupCount;

		// Start at the last group a loop around
		pGroup = &g_SentenceGroups[groupIndex];
		if ( symGroupName == pGroup->GroupNameSymbol() )
		{
			// Matches previous group, bump count
			pGroup->count++;
			return i;
		}
	}

	// new group
	int addIndex = g_SentenceGroups.AddToTail();
	sentencegroup_t *group = &g_SentenceGroups[addIndex];
	group->SetGroupName( groupName );
	group->count = 1;
	return addIndex;
}

#if DEAD
//-----------------------------------------------------------------------------
// Purpose: clear the sentence groups
//-----------------------------------------------------------------------------
void VOX_GroupClear( void )
{
	g_SentenceGroups.RemoveAll();
}
#endif


void VOX_LRUInit( sentencegroup_t *pGroup )
{
	int i, n1, n2, temp;

	if ( pGroup->count )
	{
		unsigned char *pLRU = &g_GroupLRU[pGroup->lru];
		for (i = 0; i < pGroup->count; i++)
			pLRU[i] = (unsigned char) i;

		// randomize array by swapping random elements
		for (i = 0; i < (pGroup->count * 4); i++)
		{
			// FIXME: This should probably call through g_pSoundServices
			// or some other such call?
			n1 = RandomInt(0,pGroup->count-1);
			n2 = RandomInt(0,pGroup->count-1);
			temp = pLRU[n1];
			pLRU[n1] = pLRU[n2];
			pLRU[n2] = temp;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Init the LRU for each sentence group
//-----------------------------------------------------------------------------
void VOX_GroupInitAllLRUs( void )
{
	int i;

	int totalCount = 0;
	for ( i = 0; i < g_SentenceGroups.Size(); i++ )
	{
		g_SentenceGroups[i].lru = totalCount;
		totalCount += g_SentenceGroups[i].count;
	}
	g_GroupLRU.Purge();
	g_GroupLRU.EnsureCount( totalCount );
	for ( i = 0; i < g_SentenceGroups.Size(); i++ )
	{
		VOX_LRUInit( &g_SentenceGroups[i] );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Only during reslist generation
//-----------------------------------------------------------------------------
void VOX_AddSentenceWavesToResList( void )
{
	if ( !CommandLine()->FindParm( "-makereslists" ) &&
		 !CommandLine()->FindParm( "-spewsentences" ) )
	{
		return;
	}

	bool spewsentences = CommandLine()->FindParm( "-spewsentences" ) != 0 ? true : false;

	CUtlDict< int, int > list;
	CUtlRBTree< ccpair, int > ccpairs( 0, 0, CCPairLessFunc );

	int i;
	int sentencecount = g_Sentences.Count();

	for ( i = 0; i < sentencecount; i++ )
	{
		// Walk through all nonvirtual sentences and touch the referenced sounds...
		sentence_t *pSentence = &g_Sentences[i];

		if ( !Q_strnicmp( pSentence->pName, "V_", 2 ) )
		{
			continue;
		}

		if ( spewsentences )
		{
			const char *psz = VOX_LookupString(pSentence->pName, NULL);
			if ( psz )
			{
				Msg( "%s : %s\n", pSentence->pName, psz );
			}
		}

		VOX_TouchSound( pSentence->pName, list, ccpairs, spewsentences );

	}

	VOX_TouchSounds( list, ccpairs, spewsentences );

	list.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Given a group name, return that group's index
// Input  : *pGroupName - name of the group
// Output : int - index in group table, returns -1 if no matching group is found
//-----------------------------------------------------------------------------
int VOX_GroupIndexFromName( const char *pGroupName )
{
	int i;

	if ( pGroupName )
	{
		// search rgsentenceg for match on szgroupname
		CUtlSymbol symGroupName = sentencegroup_t::GetSymbol( pGroupName );
		for ( i = 0; i < g_SentenceGroups.Size(); i++ )
		{
			if ( symGroupName == g_SentenceGroups[i].GroupNameSymbol() )
				return i;
		}
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: return the group's name
// Input  : groupIndex - index of the group
// Output : const char * - name pointer
//-----------------------------------------------------------------------------
const char *VOX_GroupNameFromIndex( int groupIndex )
{
	if ( groupIndex >= 0 && groupIndex < g_SentenceGroups.Size() )
		return g_SentenceGroups[groupIndex].GroupName();

	return NULL;
}

// ignore lru. pick next sentence from sentence group. Go in order until we hit the last sentence, 
// then repeat list if freset is true.  If freset is false, then repeat last sentence.
// ipick is passed in as the requested sentence ordinal.
// ipick 'next' is returned.  
// return of -1 indicates an error.

int VOX_GroupPickSequential( int isentenceg, char *szfound, int szfoundLen, int ipick, int freset )
{
	const char *szgroupname;
	unsigned char count;
	
	if (isentenceg < 0 || isentenceg > g_SentenceGroups.Size())
		return -1;

	szgroupname = g_SentenceGroups[isentenceg].GroupName();
	count = g_SentenceGroups[isentenceg].count;
	
	if (count == 0)
		return -1;

	if (ipick >= count)
		ipick = count-1;

	Q_snprintf( szfound, szfoundLen, "!%s%d", szgroupname, ipick );
	
	if (ipick >= count)
	{
		if (freset)
			// reset at end of list
			return 0;
		else
			return count;
	}

	return ipick + 1;
}



// pick a random sentence from rootname0 to rootnameX.
// picks from the rgsentenceg[isentenceg] least
// recently used, modifies lru array. returns the sentencename.
// note, lru must be seeded with 0-n randomized sentence numbers, with the
// rest of the lru filled with -1. The first integer in the lru is
// actually the size of the list.  Returns ipick, the ordinal
// of the picked sentence within the group.

int VOX_GroupPick( int isentenceg, char *szfound, int strLen )
{
	const char *szgroupname;
	unsigned char *plru;
	unsigned char i;
	unsigned char count;
	unsigned char ipick=0;
	int ffound = FALSE;
	
	if (isentenceg < 0 || isentenceg > g_SentenceGroups.Size())
		return -1;

	szgroupname = g_SentenceGroups[isentenceg].GroupName();
	count = g_SentenceGroups[isentenceg].count;
	plru = &g_GroupLRU[g_SentenceGroups[isentenceg].lru];

	while (!ffound)
	{
		for (i = 0; i < count; i++)
			if (plru[i] != 0xFF)
			{
				ipick = plru[i];
				plru[i] = 0xFF;
				ffound = TRUE;
				break;
			}

		if (!ffound)
		{
			VOX_LRUInit( &g_SentenceGroups[isentenceg] );
		}
		else
		{
			Q_snprintf( szfound, strLen, "!%s%d", szgroupname, ipick );
			return ipick;
		}
	}
	return -1;
}


struct filelist_t
{
	const char	*pFileName;
	filelist_t	*pNext;
};

static filelist_t *g_pSentenceFileList = NULL;

//-----------------------------------------------------------------------------
// Purpose: clear / reinitialize the vox list
//-----------------------------------------------------------------------------
void VOX_ListClear( void )
{
	filelist_t *pList, *pNext;
	
	pList = g_pSentenceFileList;
	
	while ( pList )
	{
		pNext = pList->pNext;
		free( pList );

		pList = pNext;
	}

	g_pSentenceFileList = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if this file is in the list
// Input  : *psentenceFileName - 
// Output : int, true if the file is in the list, false if not
//-----------------------------------------------------------------------------
int VOX_ListFileIsLoaded( const char *psentenceFileName )
{
	filelist_t *pList = g_pSentenceFileList;
	while ( pList )
	{
		if ( !strcmp( psentenceFileName, pList->pFileName ) )
			return true;

		pList = pList->pNext;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Add this file name to the sentence list
// Input  : *psentenceFileName - 
//-----------------------------------------------------------------------------
void VOX_ListMarkFileLoaded( const char *psentenceFileName )
{
	filelist_t	*pEntry;
	char		*pName;

	pEntry = (filelist_t *)malloc( sizeof(filelist_t) + strlen( psentenceFileName ) + 1);

	if ( pEntry )
	{
		pName = (char *)(pEntry+1);
		Q_strcpy( pName, psentenceFileName );

		pEntry->pFileName = pName;
		pEntry->pNext = g_pSentenceFileList;

		g_pSentenceFileList = pEntry;
	}
}

// This creates a compact copy of the sentence file in memory with only the necessary data
void VOX_CompactSentenceFile()
{
	int totalMem = 0;
	int i;
	for ( i = 0; i < g_Sentences.Count(); i++ )
	{
		int len = Q_strlen( g_Sentences[i].pName ) + 1;
		const char *pData = g_Sentences[i].pName + len;
		int dataLen = Q_strlen( pData ) + 1;
		totalMem += len + dataLen;
	}
	g_SentenceFile.EnsureCount( totalMem );
	totalMem = 0;
	for ( i = 0; i < g_Sentences.Count(); i++ )
	{
		int len = Q_strlen( g_Sentences[i].pName ) + 1;
		const char *pData = g_Sentences[i].pName + len;
		int dataLen = Q_strlen( pData ) + 1;
		char *pDest = &g_SentenceFile[totalMem];
		memcpy( pDest, g_Sentences[i].pName, len + dataLen );
		g_Sentences[i].pName = pDest;
		totalMem += len + dataLen;
	}
}

// Load sentence file into memory, insert null terminators to
// delimit sentence name/sentence pairs.  Keep pointer to each
// sentence name so we can search later.

void VOX_ReadSentenceFile( const char *psentenceFileName )
{
	char *pch;
	byte *pFileData;
	int fileSize;
	char c;
	char *pchlast, *pSentenceData;
	characterset_t whitespace;

	// Have we already loaded this file?
	if ( VOX_ListFileIsLoaded( psentenceFileName ) )
	{
		// must touch any sentence wavs again to ensure the map's init path gets the results
		if ( MapReslistGenerator().IsLoggingToMap() )
		{
			VOX_AddSentenceWavesToResList();
		}
		return;
	}

	// load file

	FileHandle_t file;
	file = g_pFileSystem->Open( psentenceFileName, "rb" );
	if ( FILESYSTEM_INVALID_HANDLE == file )
	{
		DevMsg ("Couldn't load %s\n", psentenceFileName);
		return;
	}

	fileSize = g_pFileSystem->Size( file );
	if ( fileSize <= 0 )
	{
		DevMsg ("VOX_ReadSentenceFile: %s has invalid size %i\n", psentenceFileName, fileSize );
		g_pFileSystem->Close( file );
		return;
	}

	pFileData = (byte *)g_pFileSystem->AllocOptimalReadBuffer( file, fileSize + 1 );
	if ( !pFileData )
	{
		DevMsg ("VOX_ReadSentenceFile: %s couldn't allocate %i bytes for data\n", psentenceFileName, fileSize );
		g_pFileSystem->Close( file );
		return;
	}

	// Read the data and close the file
	g_pFileSystem->ReadEx( pFileData, g_pFileSystem->GetOptimalReadSize( file, fileSize ), fileSize, file );
	g_pFileSystem->Close( file );

	// Make sure we end with a null terminator
	pFileData[ fileSize ] = 0;

	pch = (char *)pFileData;
	pchlast = pch + fileSize;
	CharacterSetBuild( &whitespace, "\n\r\t " );
	const char *pName = 0;
	while (pch < pchlast)
	{
		// Only process this pass on sentences
		pSentenceData = NULL;

		// skip newline, cr, tab, space

		c = *pch;
		while (pch < pchlast && IN_CHARACTERSET( whitespace, c ))
			c = *(++pch);

		// YWB:  Fix possible crashes reading past end of file if the last line has only whitespace on it...
		if ( !*pch )
			break;

		// skip entire line if first char is /
		if (*pch != '/')
		{
			int addIndex = g_Sentences.AddToTail();
			sentence_t *pSentence = &g_Sentences[addIndex];
			pName = pch;
			pSentence->pName = pch;
			pSentence->length = 0;
			pSentence->closecaption = false;
			pSentence->isPrecached = false;
			pSentence->caption = UTL_INVAL_SYMBOL;

			// scan forward to first space, insert null terminator
			// after sentence name

			c = *pch;
			while (pch < pchlast && c != ' ')
				c = *(++pch);

			if (pch < pchlast)
				*pch++ = 0;

			// A sentence may have some line commands, make an extra pass
			pSentenceData = pch;
		}
		// scan forward to end of sentence or eof
		while (pch < pchlast && pch[0] != '\n' && pch[0] != '\r')
			pch++;
	
		// insert null terminator
		if (pch < pchlast)
			*pch++ = 0;

		// If we have some sentence data, parse out any line commands
		if ( pSentenceData && pSentenceData < pchlast )
		{
			// Add a new group or increment count of the existing one
			VOX_GroupAdd( pName );
			int index = g_Sentences.Size()-1;
			// The current sentence has an index of count-1
			VOX_ParseLineCommands( pSentenceData, index );

		}
	}
	// now compact the file data in memory
	VOX_CompactSentenceFile();
	g_pFileSystem->FreeOptimalReadBuffer( pFileData );

	VOX_GroupInitAllLRUs();
	
	// This only does stuff during reslist generation...
	VOX_AddSentenceWavesToResList();

	VOX_ListMarkFileLoaded( psentenceFileName );
}


//-----------------------------------------------------------------------------
// Purpose: Get the current number of sentences in the database
// Output : int
//-----------------------------------------------------------------------------
int VOX_SentenceCount( void )
{
	return g_Sentences.Size();
}


float VOX_SentenceLength( int sentence_num )
{
	if ( sentence_num < 0 || sentence_num > g_Sentences.Size()-1 )
		return 0.0f;
	
	return g_Sentences[ sentence_num ].length;
}

// scan g_Sentences, looking for pszin sentence name
// return pointer to sentence data if found, null if not
// CONSIDER: if we have a large number of sentences, should
// CONSIDER: sort strings in g_Sentences and do binary search.
char *VOX_LookupString(const char *pSentenceName, int *psentencenum, bool *pbEmitCaption /*=NULL*/, CUtlSymbol *pCaptionSymbol /*=NULL*/, float *pflDuration /*= NULL*/ )
{
	if ( pbEmitCaption )
	{
		*pbEmitCaption = false;
	}

	if ( pCaptionSymbol )
	{
		*pCaptionSymbol = UTL_INVAL_SYMBOL;
	}

	if ( pflDuration )
	{
		*pflDuration = 0.0f;
	}

	int i;
	int c = g_Sentences.Size();
	for (i = 0; i < c; i++)
	{
		char const *name = g_Sentences[i].pName;

		if (!stricmp(pSentenceName, name))
		{
			if (psentencenum)
			{
				*psentencenum = i;
			}

			if ( pbEmitCaption )
			{
				*pbEmitCaption = g_Sentences[ i ].closecaption;
			}

			if ( pCaptionSymbol )
			{
				*pCaptionSymbol = g_Sentences[ i ].caption;
			}
		
			if ( pflDuration )
			{
				*pflDuration = g_Sentences[ i ].length;
			}

			return (char *)(name + Q_strlen(name) + 1);
		}
	}
	return NULL;
}


// Abstraction for sentence name array
const char *VOX_SentenceNameFromIndex( int sentencenum )
{
	if ( sentencenum < g_Sentences.Size() )
		return g_Sentences[sentencenum].pName;
	return NULL;
}



