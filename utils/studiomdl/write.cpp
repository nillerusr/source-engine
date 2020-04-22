//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

//
// write.c: writes a studio .mdl file
//

#pragma warning( disable : 4244 )
#pragma warning( disable : 4237 )
#pragma warning( disable : 4305 )


#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <limits.h>

#include "cmdlib.h"
#include "scriplib.h"
#include "mathlib/mathlib.h"
#include "studio.h"
#include "studiomdl.h"
#include "collisionmodel.h"
#include "optimize.h"
#include "studiobyteswap.h"
#include "byteswap.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "mdlobjects/dmeboneflexdriver.h"
#include "perfstats.h"

#include "tier1/smartptr.h"
#include "tier2/p4helpers.h"

int totalframes = 0;
float totalseconds = 0;
extern int numcommandnodes;

// WriteFile is the only externally visible function in this file.
// pData points to the current location in an output buffer and pStart is
// the beginning of the buffer.

bool FixupToSortedLODVertexes( studiohdr_t *pStudioHdr );
bool Clamp_RootLOD(  studiohdr_t *phdr );
static void WriteAllSwappedFiles( const char *filename );

/*
============
WriteModel
============
*/

static byte *pData;
static byte *pStart;
static byte *pBlockData;
static byte *pBlockStart;

#undef ALIGN4
#undef ALIGN16
#undef ALIGN32
#define ALIGN4( a ) a = (byte *)((int)((byte *)a + 3) & ~ 3)
#define ALIGN16( a ) a = (byte *)((int)((byte *)a + 15) & ~ 15)
#define ALIGN32( a ) a = (byte *)((int)((byte *)a + 31) & ~ 31)
#define ALIGN64( a ) a = (byte *)((int)((byte *)a + 63) & ~ 63)
#define ALIGN512( a ) a = (byte *)((int)((byte *)a + 511) & ~ 511)
// make sure kalloc aligns to maximum alignment size

#define FILEBUFFER (8 * 1024 * 1024)

void WriteSeqKeyValues( mstudioseqdesc_t *pseqdesc, CUtlVector< char > *pKeyValue );

//-----------------------------------------------------------------------------
// Purpose: stringtable is a session global string table.
//-----------------------------------------------------------------------------

struct stringtable_t
{
	byte	*base;
	int		*ptr;
	const char	*string;
	int		dupindex;
	byte	*addr;
};

static int numStrings;
static stringtable_t strings[32768];

static void BeginStringTable(  )
{
	strings[0].base = NULL;
	strings[0].ptr = NULL;
	strings[0].string = "";
	strings[0].dupindex = -1;
	numStrings = 1;
}

//-----------------------------------------------------------------------------
// Purpose: add a string to the file-global string table.
//			Keep track of fixup locations
//-----------------------------------------------------------------------------
static void AddToStringTable( void *base, int *ptr, const char *string )
{
	for (int i = 0; i < numStrings; i++)
	{
		if (!string || !strcmp( string, strings[i].string ))
		{
			strings[numStrings].base = (byte *)base;
			strings[numStrings].ptr = ptr;
			strings[numStrings].string = string;
			strings[numStrings].dupindex = i;
			numStrings++;
			return;
		}
	}

	strings[numStrings].base = (byte *)base;
	strings[numStrings].ptr = ptr;
	strings[numStrings].string = string;
	strings[numStrings].dupindex = -1;
	numStrings++;
}

//-----------------------------------------------------------------------------
// Purpose: Write out stringtable
//			fixup local pointers
//-----------------------------------------------------------------------------
static byte *WriteStringTable( byte *pData )
{
	// force null at first address
	strings[0].addr = pData;
	*pData = '\0';
	pData++;

	// save all the rest
	for (int i = 1; i < numStrings; i++)
	{
		if (strings[i].dupindex == -1)
		{
			// not in table yet
			// calc offset relative to local base
			*strings[i].ptr = pData - strings[i].base;
			// keep track of address in case of duplication
			strings[i].addr = pData;
			// copy string data, add a terminating \0
			strcpy( (char *)pData, strings[i].string );
			pData += strlen( strings[i].string );
			*pData = '\0';
			pData++;
		}
		else
		{
			// already in table, calc offset of existing string relative to local base
			*strings[i].ptr = strings[strings[i].dupindex].addr - strings[i].base;
		}
	}
	ALIGN4( pData );
	return pData;
}


// compare function for qsort below
static int BoneNameCompare( const void *elem1, const void *elem2 )
{
	int index1 = *(byte *)elem1;
	int index2 = *(byte *)elem2;

	// compare bones by name
	return strcmpi( g_bonetable[index1].name, g_bonetable[index2].name );
}


static void WriteBoneInfo( studiohdr_t *phdr )
{
	int i, j, k;
	mstudiobone_t *pbone;
	mstudiobonecontroller_t *pbonecontroller;
	mstudioattachment_t *pattachment;
	mstudiobbox_t *pbbox;

	// save bone info
	pbone = (mstudiobone_t *)pData;
	phdr->numbones = g_numbones;
	phdr->boneindex = pData - pStart;

	char* pSurfacePropName = GetDefaultSurfaceProp( );
	AddToStringTable( phdr, &phdr->surfacepropindex, pSurfacePropName );
	phdr->contents = GetDefaultContents();

	for (i = 0; i < g_numbones; i++) 
	{
		AddToStringTable( &pbone[i], &pbone[i].sznameindex, g_bonetable[i].name );
		pbone[i].parent			= g_bonetable[i].parent;
		pbone[i].flags			= g_bonetable[i].flags;
		pbone[i].procindex		= 0;
		pbone[i].physicsbone	= g_bonetable[i].physicsBoneIndex;
		pbone[i].pos			= g_bonetable[i].pos;
		pbone[i].rot			= g_bonetable[i].rot;
		pbone[i].posscale		= g_bonetable[i].posscale;
		pbone[i].rotscale		= g_bonetable[i].rotscale;
		MatrixInvert( g_bonetable[i].boneToPose, pbone[i].poseToBone );
		pbone[i].qAlignment		= g_bonetable[i].qAlignment;

		AngleQuaternion( RadianEuler( g_bonetable[i].rot[0], g_bonetable[i].rot[1], g_bonetable[i].rot[2] ), pbone[i].quat );
		QuaternionAlign( pbone[i].qAlignment, pbone[i].quat, pbone[i].quat );

		pSurfacePropName = GetSurfaceProp( g_bonetable[i].name );
		AddToStringTable( &pbone[i], &pbone[i].surfacepropidx, pSurfacePropName );
		pbone[i].contents = GetContents( g_bonetable[i].name );
	}

	pData += g_numbones * sizeof( mstudiobone_t );
	ALIGN4( pData );

	// save procedural bone info
	if (g_numaxisinterpbones)
	{
		mstudioaxisinterpbone_t *pProc = (mstudioaxisinterpbone_t *)pData;
		for (i = 0; i < g_numaxisinterpbones; i++)
		{
			j = g_axisinterpbonemap[i];
			k = g_axisinterpbones[j].bone;
			pbone[k].procindex		= (byte *)&pProc[i] - (byte *)&pbone[k];
			pbone[k].proctype		= STUDIO_PROC_AXISINTERP;
			// printf("bone %d %d\n", j, pbone[k].procindex );
			pProc[i].control		= g_axisinterpbones[j].control;
			pProc[i].axis			= g_axisinterpbones[j].axis;
			for (k = 0; k < 6; k++)
			{
				VectorCopy( g_axisinterpbones[j].pos[k], pProc[i].pos[k] );
				pProc[i].quat[k] = g_axisinterpbones[j].quat[k];
			}
		}
		pData += g_numaxisinterpbones * sizeof( mstudioaxisinterpbone_t );
		ALIGN4( pData );
	}

	if (g_numquatinterpbones)
	{
		mstudioquatinterpbone_t *pProc = (mstudioquatinterpbone_t *)pData;
		pData += g_numquatinterpbones * sizeof( mstudioquatinterpbone_t );
		ALIGN4( pData );

		for (i = 0; i < g_numquatinterpbones; i++)
		{
			j = g_quatinterpbonemap[i];
			k = g_quatinterpbones[j].bone;
			pbone[k].procindex		= (byte *)&pProc[i] - (byte *)&pbone[k];
			pbone[k].proctype		= STUDIO_PROC_QUATINTERP;
			// printf("bone %d %d\n", j, pbone[k].procindex );
			pProc[i].control		= g_quatinterpbones[j].control;

			mstudioquatinterpinfo_t *pTrigger = (mstudioquatinterpinfo_t *)pData;
			pProc[i].numtriggers	= g_quatinterpbones[j].numtriggers;
			pProc[i].triggerindex	= (byte *)pTrigger - (byte *)&pProc[i];
			pData += pProc[i].numtriggers * sizeof( mstudioquatinterpinfo_t );

			for (k = 0; k < pProc[i].numtriggers; k++)
			{
				pTrigger[k].inv_tolerance	= 1.0 / g_quatinterpbones[j].tolerance[k];
				pTrigger[k].trigger		= g_quatinterpbones[j].trigger[k];
				pTrigger[k].pos			= g_quatinterpbones[j].pos[k];
				pTrigger[k].quat		= g_quatinterpbones[j].quat[k];
			}
		}
	}

	if (g_numjigglebones)
	{
		mstudiojigglebone_t *jiggleInfo = (mstudiojigglebone_t *)pData;
		
		for (i = 0; i < g_numjigglebones; i++)
		{
			j = g_jigglebonemap[i];
			k = g_jigglebones[j].bone;
			pbone[k].procindex		= (byte *)&jiggleInfo[i] - (byte *)&pbone[k];
			pbone[k].proctype		= STUDIO_PROC_JIGGLE;
			
			jiggleInfo[i] = g_jigglebones[j].data;
		}
		pData += g_numjigglebones * sizeof( mstudiojigglebone_t );
		ALIGN4( pData );
	}

	// write aim at bones
	if (g_numaimatbones)
	{
		mstudioaimatbone_t *pProc = (mstudioaimatbone_t *)pData;
		for (i = 0; i < g_numaimatbones; i++)
		{
			j = g_aimatbonemap[i];
			k = g_aimatbones[j].bone;
			pbone[k].procindex		= (byte *)&pProc[i] - (byte *)&pbone[k];
			pbone[k].proctype		= g_aimatbones[j].aimAttach == -1 ? STUDIO_PROC_AIMATBONE : STUDIO_PROC_AIMATATTACH;
			pProc[i].parent			= g_aimatbones[j].parent;
			pProc[i].aim			= g_aimatbones[j].aimAttach == -1 ? g_aimatbones[j].aimBone : g_aimatbones[j].aimAttach;
			pProc[i].aimvector		= g_aimatbones[j].aimvector;
			pProc[i].upvector		= g_aimatbones[j].upvector;
			pProc[i].basepos		= g_aimatbones[j].basepos;
		}
		pData += g_numaimatbones * sizeof( mstudioaimatbone_t );
		ALIGN4( pData );
	}

	// map g_bonecontroller to bones
	for (i = 0; i < g_numbones; i++) 
	{
		for (j = 0; j < 6; j++)	
		{
			pbone[i].bonecontroller[j] = -1;
		}
	}
	
	for (i = 0; i < g_numbonecontrollers; i++) 
	{
		j = g_bonecontroller[i].bone;
		switch( g_bonecontroller[i].type & STUDIO_TYPES )
		{
		case STUDIO_X:
			pbone[j].bonecontroller[0] = i;
			break;
		case STUDIO_Y:
			pbone[j].bonecontroller[1] = i;
			break;
		case STUDIO_Z:
			pbone[j].bonecontroller[2] = i;
			break;
		case STUDIO_XR:
			pbone[j].bonecontroller[3] = i;
			break;
		case STUDIO_YR:
			pbone[j].bonecontroller[4] = i;
			break;
		case STUDIO_ZR:
			pbone[j].bonecontroller[5] = i;
			break;
		default:
			MdlError("unknown g_bonecontroller type\n");
		}
	}

	// save g_bonecontroller info
	pbonecontroller = (mstudiobonecontroller_t *)pData;
	phdr->numbonecontrollers = g_numbonecontrollers;
	phdr->bonecontrollerindex = pData - pStart;

	for (i = 0; i < g_numbonecontrollers; i++) 
	{
		pbonecontroller[i].bone			= g_bonecontroller[i].bone;
		pbonecontroller[i].inputfield	= g_bonecontroller[i].inputfield;
		pbonecontroller[i].type			= g_bonecontroller[i].type;
		pbonecontroller[i].start		= g_bonecontroller[i].start;
		pbonecontroller[i].end			= g_bonecontroller[i].end;
	}
	pData += g_numbonecontrollers * sizeof( mstudiobonecontroller_t );
	ALIGN4( pData );

	// save attachment info
	pattachment = (mstudioattachment_t *)pData;
	phdr->numlocalattachments = g_numattachments;
	phdr->localattachmentindex = pData - pStart;

	for (i = 0; i < g_numattachments; i++) 
	{
		pattachment[i].localbone			= g_attachment[i].bone;
		AddToStringTable( &pattachment[i], &pattachment[i].sznameindex, g_attachment[i].name );
		MatrixCopy( g_attachment[i].local, pattachment[i].local );
		pattachment[i].flags = g_attachment[i].flags;
	}
	pData += g_numattachments * sizeof( mstudioattachment_t );
	ALIGN4( pData );
	
	// save hitbox sets
	phdr->numhitboxsets = g_hitboxsets.Size();

	// Remember start spot
	mstudiohitboxset_t *hitboxset = (mstudiohitboxset_t *)pData;
	phdr->hitboxsetindex = pData - pStart;

	pData += phdr->numhitboxsets * sizeof( mstudiohitboxset_t );
	ALIGN4( pData );

	for ( int s = 0; s < g_hitboxsets.Size(); s++, hitboxset++ )
	{
		s_hitboxset *set = &g_hitboxsets[ s ];

		AddToStringTable( hitboxset, &hitboxset->sznameindex, set->hitboxsetname );

		hitboxset->numhitboxes = set->numhitboxes;
		hitboxset->hitboxindex = ( pData - (byte *)hitboxset );

		// save bbox info
		pbbox = (mstudiobbox_t *)pData;
		for (i = 0; i < hitboxset->numhitboxes; i++) 
		{
			pbbox[i].bone				= set->hitbox[i].bone;
			pbbox[i].group				= set->hitbox[i].group;
			VectorCopy( set->hitbox[i].bmin, pbbox[i].bbmin );
			VectorCopy( set->hitbox[i].bmax, pbbox[i].bbmax );
			pbbox[i].szhitboxnameindex = 0;
			AddToStringTable( &(pbbox[i]), &(pbbox[i].szhitboxnameindex), set->hitbox[i].hitboxname );	
		}

		pData += hitboxset->numhitboxes * sizeof( mstudiobbox_t );
		ALIGN4( pData );
	}
	byte *pBoneTable = pData;
	phdr->bonetablebynameindex = (pData - pStart);

	// make a table in bone order and sort it with qsort
	for ( i = 0; i < phdr->numbones; i++ )
	{
		pBoneTable[i] = i;
	}
	qsort( pBoneTable, phdr->numbones, sizeof(byte), BoneNameCompare );
	pData += phdr->numbones * sizeof( byte );
	ALIGN4( pData );
}

// load a preexisting model to remember its sequence names and indices
CUtlVector< CUtlString > g_vecPreexistingSequences;
void LoadPreexistingSequenceOrder( const char *pFilename )
{
	g_vecPreexistingSequences.RemoveAll();

	if ( !FileExists( pFilename ) )
		return;

	Msg( "Loading preexisting model: %s\n", pFilename );

	studiohdr_t *pStudioHdr;
	int len = LoadFile((char*)pFilename, (void **)&pStudioHdr);

	if ( len && pStudioHdr && pStudioHdr->SequencesAvailable() )
	{
		Msg( "   Found %i preexisting sequences.\n", pStudioHdr->GetNumSeq() );

		for ( int i=0; i<pStudioHdr->GetNumSeq(); i++ )
		{
			//Msg( "   Sequence %i : \"%s\"\n", i, pStudioHdr->pSeqdesc(i).pszLabel() );
			g_vecPreexistingSequences.AddToTail( pStudioHdr->pSeqdesc(i).pszLabel() );
		}
	}
	else
	{
		MdlWarning( "Zero-size file or no sequences.\n" );
	}
}

static void WriteSequenceInfo( studiohdr_t *phdr )
{
	int i, j, k;

	mstudioseqdesc_t	*pseqdesc;
	mstudioseqdesc_t	*pbaseseqdesc;
	mstudioevent_t		*pevent;
	byte				*ptransition;

	// write models to disk with this flag set false. This will force
	// the sequences to be indexed by activity whenever the g_model is loaded
	// from disk.
	phdr->activitylistversion = 0;
	phdr->eventsindexed = 0;

	// save g_sequence info
	pseqdesc = (mstudioseqdesc_t *)pData;
	pbaseseqdesc = pseqdesc;
	phdr->numlocalseq = g_sequence.Count();
	phdr->localseqindex = (pData - pStart);
	pData += g_sequence.Count() * sizeof( mstudioseqdesc_t );

	bool bErrors = false;


	// build a table to remap new sequence indices to match the preexisting model
	bool bUseSeqOrderRemapping = false;
	int nSeqOrderRemappingTable[MAXSTUDIOSEQUENCES];
	for (i=0; i<MAXSTUDIOSEQUENCES; i++)
		nSeqOrderRemappingTable[i] = -1;

	bool bAllowSequenceRemoval = false;

	if ( g_vecPreexistingSequences.Count() )
	{

		if ( g_sequence.Count() < g_vecPreexistingSequences.Count() && !bAllowSequenceRemoval )
		{
			Msg( "\n" );
			MdlWarning( "This model has fewer sequences than its predecessor.\nPlease confirm sequence deletion: [y/n] " );
			int nInput = 0;
			do { nInput = getchar(); } while ( nInput != 121 /* y */ && nInput != 110 /* n */ );

			if ( nInput == 110 )
			{
				MdlError( "Model contains fewer sequences than its predecessor!\n" );
			}
			else if ( nInput == 121 )
			{
				bAllowSequenceRemoval = true;
			}
		}

		{
			Msg( "Building sequence index remapping table...\n" );
			
			CUtlVector<int> vecNewIndices;
			vecNewIndices.RemoveAll();

			// map current sequences to their old indices
			for (i = 0; i < g_sequence.Count(); i++ )
			{
				int nIdx = g_vecPreexistingSequences.Find( g_sequence[i].name );
				if ( nIdx >= 0 )
				{
					nSeqOrderRemappingTable[nIdx] = i;
				}
				else
				{
					if ( i < g_vecPreexistingSequences.Count() )
					{
						Msg( "  Found new sequence \"%s\" using index of old sequence \"%s\".\n", g_sequence[i].name, g_vecPreexistingSequences[i].String() );
					}
					else
					{
						Msg( "  Found new sequence \"%s\".\n", g_sequence[i].name );
					}
					
					vecNewIndices.AddToTail(i);
				}
			}

			// slot new sequences into unused indices
			while ( vecNewIndices.Count() )
			{
				for (i = 0; i < MAXSTUDIOSEQUENCES; i++ )
				{
					if ( nSeqOrderRemappingTable[i] == -1 )
					{
						nSeqOrderRemappingTable[i] = vecNewIndices[0];
						vecNewIndices.Remove(0);
						break;
					}
				}
			}

			// verify no indices are undefined
			for (i = 0; i < g_sequence.Count(); i++ )
			{
				if ( nSeqOrderRemappingTable[i] == -1 )
				{
					if ( bAllowSequenceRemoval )
					{
						do
						{
							for ( int nB=i; nB<g_vecPreexistingSequences.Count(); nB++ )
							{
								nSeqOrderRemappingTable[nB] = nSeqOrderRemappingTable[nB+1];
							}
						}
						while (nSeqOrderRemappingTable[i] == -1);
					}
					else
					{
						MdlError( "Failed to reorder sequence indices.\n" );
					}

				}
				else if ( nSeqOrderRemappingTable[i] != i )
				{
					bUseSeqOrderRemapping = true;
				}
			}

			if ( bUseSeqOrderRemapping )
			{
				Msg( "Sequence indices need re-ordering.\n" );
			}
			else
			{
				Msg( "No re-ordering required.\n" );
			}
		}
	}	

	// build an inverted remapping table so autolayer sequence indices can find their sources later
	int nSeqOrderRemappingTableInv[MAXSTUDIOSEQUENCES];
	if ( bUseSeqOrderRemapping )
	{
		for (i=0; i<MAXSTUDIOSEQUENCES; i++)
			nSeqOrderRemappingTableInv[nSeqOrderRemappingTable[i]] = i;
	}
	
	int m;
	for (m = 0; m < g_sequence.Count(); m++, pseqdesc++) 
	{

		if ( bUseSeqOrderRemapping )
		{
			i = nSeqOrderRemappingTable[m];
			if ( i != m )
			{
				Msg( "   Remapping sequence %i to index %i (%s) to retain existing order.\n", i, m, g_sequence[i].name );
			}
		}
		else
		{
			i = m;
		}

		byte *pSequenceStart = (byte *)pseqdesc;

		AddToStringTable( pseqdesc, &pseqdesc->szlabelindex, g_sequence[i].name );
		AddToStringTable( pseqdesc, &pseqdesc->szactivitynameindex, g_sequence[i].activityname );

		pseqdesc->baseptr		= pStart - (byte *)pseqdesc;

		pseqdesc->flags			= g_sequence[i].flags;

		pseqdesc->numblends		= g_sequence[i].numblends;
		pseqdesc->groupsize[0]	= g_sequence[i].groupsize[0];
		pseqdesc->groupsize[1]	= g_sequence[i].groupsize[1];

		pseqdesc->paramindex[0]	= g_sequence[i].paramindex[0];
		pseqdesc->paramstart[0] = g_sequence[i].paramstart[0];
		pseqdesc->paramend[0]	= g_sequence[i].paramend[0];
		pseqdesc->paramindex[1]	= g_sequence[i].paramindex[1];
		pseqdesc->paramstart[1] = g_sequence[i].paramstart[1];
		pseqdesc->paramend[1]	= g_sequence[i].paramend[1];

		if (g_sequence[i].groupsize[0] > 1 || g_sequence[i].groupsize[1] > 1)
		{
			// save posekey values
			float *pposekey			= (float *)pData;
			pseqdesc->posekeyindex	= (pData - pSequenceStart);
			pData += (pseqdesc->groupsize[0] + pseqdesc->groupsize[1]) * sizeof( float );
			for (j = 0; j < pseqdesc->groupsize[0]; j++)
			{
				*(pposekey++) = g_sequence[i].param0[j];
				// printf("%.2f ", g_sequence[i].param0[j] );
			}
			for (j = 0; j < pseqdesc->groupsize[1]; j++)
			{
				*(pposekey++) = g_sequence[i].param1[j];
				// printf("%.2f ", g_sequence[i].param1[j] );
			}
			// printf("\n" );
		}

		// pseqdesc->motiontype	= g_sequence[i].motiontype;
		// pseqdesc->motionbone	= 0; // g_sequence[i].motionbone;
		// VectorCopy( g_sequence[i].linearmovement, pseqdesc->linearmovement );

		pseqdesc->activity		= g_sequence[i].activity;
		pseqdesc->actweight		= g_sequence[i].actweight;

		pseqdesc->bbmin			= g_sequence[i].bmin;
		pseqdesc->bbmax			= g_sequence[i].bmax;

		pseqdesc->fadeintime	= g_sequence[i].fadeintime;
		pseqdesc->fadeouttime	= g_sequence[i].fadeouttime;

		pseqdesc->localentrynode	= g_sequence[i].entrynode; 
		pseqdesc->localexitnode		= g_sequence[i].exitnode;
		//pseqdesc->entryphase	= g_sequence[i].entryphase;
		//pseqdesc->exitphase		= g_sequence[i].exitphase;
		pseqdesc->nodeflags		= g_sequence[i].nodeflags;

		// save events
		pevent					= (mstudioevent_t *)pData;
		pseqdesc->numevents		= g_sequence[i].numevents;
		pseqdesc->eventindex	= (pData - pSequenceStart);
		pData += pseqdesc->numevents * sizeof( mstudioevent_t );
		for (j = 0; j < g_sequence[i].numevents; j++)
		{
			k = g_sequence[i].panim[0][0]->numframes - 1;

			if (g_sequence[i].event[j].frame <= k)
				pevent[j].cycle		= g_sequence[i].event[j].frame / ((float)k);
			else if (k == 0 && g_sequence[i].event[j].frame == 0)
				pevent[j].cycle		= 0;
			else
			{
				MdlWarning("Event %d (frame %d) out of range in %s\n", g_sequence[i].event[j].event, g_sequence[i].event[j].frame, g_sequence[i].name );
				bErrors = true;
			}

			//Adrian - Remove me once we phase out the old event system.
			if ( V_isdigit( g_sequence[i].event[j].eventname[0] ) )
			{
				 pevent[j].event = atoi( g_sequence[i].event[j].eventname );
				 pevent[j].type = 0;
				 pevent[j].szeventindex = 0;
			}
			else
			{
				 AddToStringTable( &pevent[j], &pevent[j].szeventindex, g_sequence[i].event[j].eventname );
				 pevent[j].type = NEW_EVENT_STYLE;
			}
				 						
			
			// printf("%4d : %d %f\n", pevent[j].event, g_sequence[i].event[j].frame, pevent[j].cycle );
			// AddToStringTable( &pevent[j], &pevent[j].szoptionindex, g_sequence[i].event[j].options );
			strcpy( pevent[j].options, g_sequence[i].event[j].options );
		}
		ALIGN4( pData );

		// save ikrules
		pseqdesc->numikrules	= g_sequence[i].numikrules;

		// save autolayers
		mstudioautolayer_t *pautolayer			= (mstudioautolayer_t *)pData;
		pseqdesc->numautolayers	= g_sequence[i].numautolayers;
		pseqdesc->autolayerindex = (pData - pSequenceStart);
		pData += pseqdesc->numautolayers * sizeof( mstudioautolayer_t );
		for (j = 0; j < g_sequence[i].numautolayers; j++)
		{
			pautolayer[j].iSequence = g_sequence[i].autolayer[j].sequence;
			pautolayer[j].iPose		= g_sequence[i].autolayer[j].pose;
			pautolayer[j].flags		= g_sequence[i].autolayer[j].flags;

			// autolayer indices are stored by index, so remap them now using the invertex lookup table
			if ( bUseSeqOrderRemapping )
			{
				int nRemapAutoLayer = nSeqOrderRemappingTableInv[ pautolayer[j].iSequence ];
				if ( nRemapAutoLayer != pautolayer[j].iSequence )
				{
					Msg( "       Autolayer remapping index %i to %i.\n", pautolayer[j].iSequence, nRemapAutoLayer );
					pautolayer[j].iSequence = nRemapAutoLayer;
				}
			}

			if (!(pautolayer[j].flags & STUDIO_AL_POSE))
			{
				pautolayer[j].start		= g_sequence[i].autolayer[j].start / (g_sequence[i].panim[0][0]->numframes - 1);
				pautolayer[j].peak		= g_sequence[i].autolayer[j].peak / (g_sequence[i].panim[0][0]->numframes - 1);
				pautolayer[j].tail		= g_sequence[i].autolayer[j].tail / (g_sequence[i].panim[0][0]->numframes - 1);
				pautolayer[j].end		= g_sequence[i].autolayer[j].end / (g_sequence[i].panim[0][0]->numframes - 1);
			}
			else
			{
				pautolayer[j].start		= g_sequence[i].autolayer[j].start;
				pautolayer[j].peak		= g_sequence[i].autolayer[j].peak;
				pautolayer[j].tail		= g_sequence[i].autolayer[j].tail;
				pautolayer[j].end		= g_sequence[i].autolayer[j].end;
			}
		}


		// save boneweights
		float *pweight = 0;
		j = 0;
		// look up previous sequence weights and try to find a match
		for (k = 0; k < m; k++)
		{
			j = 0;
			// only check newer boneweights than the last one
			if (pseqdesc[k-m].pBoneweight( 0 ) > pweight)
			{
				pweight = pseqdesc[k-m].pBoneweight( 0 );
				for (j = 0; j < g_numbones; j++)
				{
					// we're not walking the linear sequence list if we're remapping, so we need to remap this check
					int nRemap = k;
					if ( bUseSeqOrderRemapping )
						nRemap = nSeqOrderRemappingTable[k];

					if (g_sequence[i].weight[j] != g_sequence[nRemap].weight[j])
						break;
				}
				if (j == g_numbones)
					break;
			}
		}

		// check to see if all the bones matched
		if (j < g_numbones)
		{
			// allocate new block
			//printf("new %08x\n", pData );
			pweight						= (float *)pData;
			pseqdesc->weightlistindex = (pData - pSequenceStart);
			pData += g_numbones * sizeof( float );
			for (j = 0; j < g_numbones; j++)
			{
				pweight[j] = g_sequence[i].weight[j];
			}
		}
		else
		{
			// use previous boneweight
			//printf("prev %08x\n", pweight );
			pseqdesc->weightlistindex = ((byte *)pweight - pSequenceStart);
		}


		// save iklocks
		mstudioiklock_t *piklock	= (mstudioiklock_t *)pData;
		pseqdesc->numiklocks		= g_sequence[i].numiklocks;
		pseqdesc->iklockindex		= (pData - pSequenceStart);
		pData += pseqdesc->numiklocks * sizeof( mstudioiklock_t );
		ALIGN4( pData );

		for (j = 0; j < pseqdesc->numiklocks; j++)
		{
			piklock->chain			= g_sequence[i].iklock[j].chain;
			piklock->flPosWeight	= g_sequence[i].iklock[j].flPosWeight;
			piklock->flLocalQWeight	= g_sequence[i].iklock[j].flLocalQWeight;
			piklock++;
		}

		// Write animation blend parameters
		short *blends = ( short * )pData;
		pseqdesc->animindexindex = ( pData - pSequenceStart );
		pData += ( g_sequence[i].groupsize[0] * g_sequence[i].groupsize[1] ) * sizeof( short );
		ALIGN4( pData );

		for ( j = 0; j < g_sequence[i].groupsize[0] ; j++ )
		{
			for ( k = 0; k < g_sequence[i].groupsize[1]; k++ )
			{
				// height value * width of row + width value
				int offset = k * g_sequence[i].groupsize[0] + j;

				if ( g_sequence[i].panim[j][k] )
				{
					int animindex = g_sequence[i].panim[j][k]->index;

					Assert( animindex >= 0 && animindex < SHRT_MAX );

					blends[ offset ] = (short)animindex;
				}
				else
				{
					blends[ offset ] = 0;
				}
			}
		}

		// Write cycle overrides
		pseqdesc->cycleposeindex = g_sequence[i].cycleposeindex;

		WriteSeqKeyValues( pseqdesc, &g_sequence[i].KeyValue );
	}

	if (bErrors)
	{
		MdlError( "Exiting due to Errors\n");
	}

	// save transition graph
	int *pxnodename = (int *)pData;
	phdr->localnodenameindex = (pData - pStart);
	pData += g_numxnodes * sizeof( *pxnodename );
	ALIGN4( pData );
	for (i = 0; i < g_numxnodes; i++)
	{
		AddToStringTable( phdr, pxnodename, g_xnodename[i+1] );
		// printf("%d : %s\n", i, g_xnodename[i+1] );
		pxnodename++;
	}

	ptransition	= (byte *)pData;
	phdr->numlocalnodes = g_numxnodes;
	phdr->localnodeindex = pData - pStart;
	pData += g_numxnodes * g_numxnodes * sizeof( byte );
	ALIGN4( pData );
	for (i = 0; i < g_numxnodes; i++)
	{
//		printf("%2d (%12s) : ", i + 1, g_xnodename[i+1] );
		for (j = 0; j < g_numxnodes; j++)
		{
			*ptransition++ = g_xnode[i][j];
//			printf(" %2d", g_xnode[i][j] );
		}
//		printf("\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Stub implementation
// Input  : *group - 
//-----------------------------------------------------------------------------

const studiohdr_t *studiohdr_t::FindModel( void **cache, char const *modelname ) const
{
	return NULL;
}

virtualmodel_t *studiohdr_t::GetVirtualModel( void ) const
{
	return NULL;
}

const studiohdr_t *virtualgroup_t::GetStudioHdr( void ) const
{
	return (studiohdr_t *)cache;
}

byte *studiohdr_t::GetAnimBlock( int i ) const
{
	return NULL;
}

int	studiohdr_t::GetAutoplayList( unsigned short **pOut ) const
{
	return 0;
}


int rawanimbytes = 0;
int animboneframes = 0;

int numAxis[4] = { 0, 0, 0, 0 };
int numPos[4] = { 0, 0, 0, 0 };
int useRaw = 0;

void WriteAnimationData( s_animation_t *srcanim, mstudioanimdesc_t *destanimdesc, byte *&pLocalData, byte *&pExtData )
{
	int j, k, n;

	byte *pData = NULL;

	for (int w = 0; w < srcanim->numsections; w++)
	{
		bool bUseExtData = false;
		pData = pLocalData;

		if (pExtData != NULL && !srcanim->disableAnimblocks && !(w == 0 && srcanim->isFirstSectionLocal))
		{
			pData = pExtData;
			bUseExtData = true;
		}

		mstudioanim_t	*destanim = (mstudioanim_t *)pData;
		byte	*pStartSection = pData;
		pData += sizeof( *destanim );

		destanim->bone = 255;

		mstudioanim_t	*prevanim = NULL;

		// save animation value info
		for (j = 0; j < g_numbones; j++)
		{
			// destanim->weight = srcanim->weight[j];
			// printf( "%s %.1f\n", g_bonetable[j].name, destanim->weight );
			destanim->flags = 0;
			s_compressed_t *psrcdata = &srcanim->anim[w][j];

			numPos[ (psrcdata->num[0] != 0) + (psrcdata->num[1] != 0) + (psrcdata->num[2] != 0) ]++;
			numAxis[ (psrcdata->num[3] != 0) + (psrcdata->num[4] != 0) + (psrcdata->num[5] != 0) ]++;

			if (psrcdata->num[0] + psrcdata->num[1] + psrcdata->num[2] + psrcdata->num[3] + psrcdata->num[4] + psrcdata->num[5] == 0)
			{
				// no animation, skip
				continue;
			}

			destanim->bone = j;

			// copy flags over if delta animation
			if (srcanim->flags & STUDIO_DELTA)
			{
				destanim->flags |= STUDIO_ANIM_DELTA;
			}

			if ((srcanim->numframes == 1) || (psrcdata->num[0] <= 2 && psrcdata->num[1] <= 2 && psrcdata->num[2] <= 2 && psrcdata->num[3] <= 2 && psrcdata->num[4] <= 2 && psrcdata->num[5] <= 2))
			{
				// printf("%d : %d %d %d : %d %d %d\n", j, psrcdata->num[0], psrcdata->num[1], psrcdata->num[2], psrcdata->num[3], psrcdata->num[4], psrcdata->num[5] );
				// single frame, if animation detected just store as raw
				int iFrame = min( w * srcanim->sectionframes, srcanim->numframes - 1 );
				if (psrcdata->num[3] != 0 || psrcdata->num[4] != 0 || psrcdata->num[5] != 0)
				{
					Quaternion q;
					AngleQuaternion( srcanim->sanim[iFrame][j].rot, q );
					*((Quaternion64 *)pData) = q;
					pData += sizeof( Quaternion64 );
					rawanimbytes += sizeof( Quaternion64 );
					destanim->flags |= STUDIO_ANIM_RAWROT2;
				}

				if (psrcdata->num[0] != 0 || psrcdata->num[1] != 0 || psrcdata->num[2] != 0)
				{
					*((Vector48 *)pData) = srcanim->sanim[iFrame][j].pos;
					pData += sizeof( Vector48 );
					rawanimbytes += sizeof( Vector48 );
					destanim->flags |= STUDIO_ANIM_RAWPOS;
				}
			}
			else
			{
				// look to see if storing raw quat's would have taken less space
				if (psrcdata->num[3] >= srcanim->numframes && psrcdata->num[4] >= srcanim->numframes && psrcdata->num[5] >= srcanim->numframes)
				{
					useRaw++;
				}

				mstudioanim_valueptr_t *posvptr	= NULL;
				mstudioanim_valueptr_t *rotvptr	= NULL;

				// allocate room for rotation ptrs
				rotvptr	= (mstudioanim_valueptr_t *)pData;
				pData += sizeof( *rotvptr );

				// skip all position info if there's no animation
				if (psrcdata->num[0] != 0 || psrcdata->num[1] != 0 || psrcdata->num[2] != 0)
				{
					posvptr	= (mstudioanim_valueptr_t *)pData;
					pData += sizeof( *posvptr );
				}

				mstudioanimvalue_t	*destanimvalue = (mstudioanimvalue_t *)pData;

				if (rotvptr)
				{
					// store rotation animations
					for (k = 3; k < 6; k++)
					{
						if (psrcdata->num[k] == 0)
						{
							rotvptr->offset[k-3] = 0;
						}
						else
						{
							rotvptr->offset[k-3] = ((byte *)destanimvalue - (byte *)rotvptr);
							for (n = 0; n < psrcdata->num[k]; n++)
							{
								destanimvalue->value = psrcdata->data[k][n].value;
								destanimvalue++;
							}
						}
					}
					destanim->flags |= STUDIO_ANIM_ANIMROT;
				}

				if (posvptr)
				{
					// store position animations
					for (k = 0; k < 3; k++)
					{
						if (psrcdata->num[k] == 0)
						{
							posvptr->offset[k] = 0;
						}
						else
						{
							posvptr->offset[k] = ((byte *)destanimvalue - (byte *)posvptr);
							for (n = 0; n < psrcdata->num[k]; n++)
							{
								destanimvalue->value = psrcdata->data[k][n].value;
								destanimvalue++;
							}
						}
					}
					destanim->flags |= STUDIO_ANIM_ANIMPOS;
				}
				rawanimbytes += ((byte *)destanimvalue - pData);
				pData = (byte *)destanimvalue;
			}

			prevanim					= destanim;
			destanim->nextoffset		= pData - (byte *)destanim;
			destanim					= (mstudioanim_t *)pData;
			pData						+= sizeof( *destanim );
		}

		if (prevanim)
		{
			prevanim->nextoffset		= 0;
		}

		ALIGN4( pData );

		// write into anim blocks if needed
		if (destanimdesc->sectionindex)
		{
			if (bUseExtData)
			{
				if (g_numanimblocks && pData - g_animblock[g_numanimblocks-1].start > g_animblocksize)
				{
					// advance to next animblock
					g_animblock[g_numanimblocks-1].end = pStartSection;
					g_animblock[g_numanimblocks].start = pStartSection;
					g_numanimblocks++;
				}

				destanimdesc->pSection(w)->animblock = g_numanimblocks - 1;
				destanimdesc->pSection(w)->animindex = pStartSection - g_animblock[g_numanimblocks-1].start;
			}
			else
			{
				destanimdesc->pSection(w)->animblock = 0;
				destanimdesc->pSection(w)->animindex = pStartSection - (byte *)destanimdesc;
			}
			// printf("%s (%d) : %d:%d\n", srcanim->name, w, destanimdesc->pSection(w)->animblock, destanimdesc->pSection(w)->animindex );
		}

		if (!bUseExtData)
		{
			pLocalData = pData;
		}
		else
		{
			pExtData = pData;
		}
	}
}


byte *WriteIkErrors( s_animation_t *srcanim, byte *pData )
{
	int j, k;

	// write IK error keys
	mstudioikrule_t *pikruledata = (mstudioikrule_t *)pData;
	pData += srcanim->numikrules * sizeof( *pikruledata );
	ALIGN4( pData );

	for (j = 0; j < srcanim->numikrules; j++)
	{
		mstudioikrule_t *pikrule = pikruledata + j;

		pikrule->index	= srcanim->ikrule[j].index;

		pikrule->chain	= srcanim->ikrule[j].chain;
		pikrule->bone	= srcanim->ikrule[j].bone;
		pikrule->type	= srcanim->ikrule[j].type;
		pikrule->slot	= srcanim->ikrule[j].slot;
		pikrule->pos	= srcanim->ikrule[j].pos;
		pikrule->q		= srcanim->ikrule[j].q;
		pikrule->height	= srcanim->ikrule[j].height;
		pikrule->floor	= srcanim->ikrule[j].floor;
		pikrule->radius = srcanim->ikrule[j].radius;

		if (srcanim->numframes > 1.0)
		{
			pikrule->start	= srcanim->ikrule[j].start / (srcanim->numframes - 1.0f);
			pikrule->peak	= srcanim->ikrule[j].peak / (srcanim->numframes - 1.0f);
			pikrule->tail	= srcanim->ikrule[j].tail / (srcanim->numframes - 1.0f);
			pikrule->end	= srcanim->ikrule[j].end / (srcanim->numframes - 1.0f);
			pikrule->contact= srcanim->ikrule[j].contact / (srcanim->numframes - 1.0f);
		}
		else
		{
			pikrule->start	= 0.0f;
			pikrule->peak	= 0.0f;
			pikrule->tail	= 1.0f;
			pikrule->end	= 1.0f;
			pikrule->contact= 0.0f;
		}

		/*
		printf("%d %d %d %d : %.2f %.2f %.2f %.2f\n", 
			srcanim->ikrule[j].start, srcanim->ikrule[j].peak, srcanim->ikrule[j].tail, srcanim->ikrule[j].end, 
			pikrule->start, pikrule->peak, pikrule->tail, pikrule->end );
		*/

		pikrule->iStart = srcanim->ikrule[j].start;

#if 0
		// uncompressed
		pikrule->ikerrorindex = (pData - (byte*)pikrule);
		mstudioikerror_t *perror = (mstudioikerror_t *)pData;
		pData += srcanim->ikrule[j].numerror * sizeof( *perror );

		for (k = 0; k < srcanim->ikrule[j].numerror; k++)
		{
			perror[k].pos = srcanim->ikrule[j].pError[k].pos;
			perror[k].q = srcanim->ikrule[j].pError[k].q;
		}
#endif
#if 1
		// skip writting the header if there's no IK data
		for (k = 0; k < 6; k++)
		{
			if (srcanim->ikrule[j].errorData.numanim[k]) break;
		}

		if (k == 6)
			continue;

		// compressed
		pikrule->compressedikerrorindex = (pData - (byte*)pikrule);
		mstudiocompressedikerror_t *pCompressed = (mstudiocompressedikerror_t *)pData;
		pData += sizeof( *pCompressed );

		for (k = 0; k < 6; k++)
		{
			pCompressed->scale[k] = srcanim->ikrule[j].errorData.scale[k];
			pCompressed->offset[k] = (pData - (byte*)pCompressed);
			int size = srcanim->ikrule[j].errorData.numanim[k] * sizeof( mstudioanimvalue_t );
			memmove( pData, srcanim->ikrule[j].errorData.anim[k], size );
			pData += size;
		}

		if (strlen( srcanim->ikrule[j].attachment ) > 0)
		{
			// don't use string table, we're probably not in the same file.
			int size = strlen( srcanim->ikrule[j].attachment ) + 1;
			strcpy( (char *)pData, srcanim->ikrule[j].attachment );
			pikrule->szattachmentindex = pData - (byte *)pikrule;
			pData += size;
		}

		ALIGN4( pData );

#endif
		// AddToStringTable( pikrule, &pikrule->szattachmentindex, srcanim->ikrule[j].attachment );
	}

	return pData;
}






byte *WriteLocalHierarchy( s_animation_t *srcanim, byte *pData )
{
	int j, k;

	// write hierarchy  keys
	mstudiolocalhierarchy_t *pHierarchyData = (mstudiolocalhierarchy_t *)pData;
	pData += srcanim->numlocalhierarchy * sizeof( *pHierarchyData );
	ALIGN4( pData );

	for (j = 0; j < srcanim->numlocalhierarchy; j++)
	{
		mstudiolocalhierarchy_t *pHierarchy = pHierarchyData + j;

		pHierarchy->iBone	= srcanim->localhierarchy[j].bone;
		pHierarchy->iNewParent	= srcanim->localhierarchy[j].newparent;

		if (srcanim->numframes > 1.0)
		{
			pHierarchy->start	= srcanim->localhierarchy[j].start / (srcanim->numframes - 1.0f);
			pHierarchy->peak	= srcanim->localhierarchy[j].peak / (srcanim->numframes - 1.0f);
			pHierarchy->tail	= srcanim->localhierarchy[j].tail / (srcanim->numframes - 1.0f);
			pHierarchy->end		= srcanim->localhierarchy[j].end / (srcanim->numframes - 1.0f);
		}
		else
		{
			pHierarchy->start	= 0.0f;
			pHierarchy->peak	= 0.0f;
			pHierarchy->tail	= 1.0f;
			pHierarchy->end		= 1.0f;
		}

		pHierarchy->iStart = srcanim->localhierarchy[j].start;

#if 0
		// uncompressed
		pHierarchy->ikerrorindex = (pData - (byte*)pHierarchy);
		mstudioikerror_t *perror = (mstudioikerror_t *)pData;
		pData += srcanim->ikrule[j].numerror * sizeof( *perror );

		for (k = 0; k < srcanim->ikrule[j].numerror; k++)
		{
			perror[k].pos = srcanim->ikrule[j].pError[k].pos;
			perror[k].q = srcanim->ikrule[j].pError[k].q;
		}
#endif
#if 1
		// skip writting the header if there's no IK data
		for (k = 0; k < 6; k++)
		{
			if (srcanim->localhierarchy[j].localData.numanim[k]) break;
		}

		if (k == 6)
			continue;

		// compressed
		pHierarchy->localanimindex = (pData - (byte*)pHierarchy);
		mstudiocompressedikerror_t *pCompressed = (mstudiocompressedikerror_t *)pData;
		pData += sizeof( *pCompressed );

		for (k = 0; k < 6; k++)
		{
			pCompressed->scale[k] = srcanim->localhierarchy[j].localData.scale[k];
			pCompressed->offset[k] = (pData - (byte*)pCompressed);
			int size = srcanim->localhierarchy[j].localData.numanim[k] * sizeof( mstudioanimvalue_t );
			memmove( pData, srcanim->localhierarchy[j].localData.anim[k], size );
			pData += size;
		}

		ALIGN4( pData );

#endif
		// AddToStringTable( pHierarchy, &pHierarchy->szattachmentindex, srcanim->ikrule[j].attachment );
	}

	return pData;
}


static byte *WriteAnimations( byte *pData, byte *pStart, studiohdr_t *phdr )
{
	int i, j;

	mstudioanimdesc_t	*panimdesc;

	// save animations
	panimdesc = (mstudioanimdesc_t *)pData;
	if( phdr )
	{
		phdr->numlocalanim = g_numani;
		phdr->localanimindex = (pData - pStart);
	}
	pData += g_numani * sizeof( *panimdesc );
	ALIGN4( pData );
	//      ------------ ------- ------- : ------- (-------)
	if( g_verbose )
	{
		printf("   animation       x       y       ips    angle\n");
	}

	for (i = 0; i < g_numani; i++) 
	{
		s_animation_t *srcanim = g_panimation[ i ];
		mstudioanimdesc_t *destanim = &panimdesc[i];
		Assert( srcanim );

		AddToStringTable( destanim, &destanim->sznameindex, srcanim->name );

		destanim->baseptr	= pStart - (byte *)destanim;
		destanim->fps		= srcanim->fps;
		destanim->flags		= srcanim->flags;

		destanim->sectionframes = srcanim->sectionframes;

		totalframes				+= srcanim->numframes;
		totalseconds			+= srcanim->numframes / srcanim->fps;

		destanim->numframes	= srcanim->numframes;

		// destanim->motiontype = srcanim->motiontype;	
		// destanim->motionbone = srcanim->motionbone;
		// VectorCopy( srcanim->linearpos, destanim->linearpos );

		j = srcanim->numpiecewisekeys - 1;
		if (srcanim->piecewisemove[j].pos[0] != 0 || srcanim->piecewisemove[j].pos[1] != 0) 
		{
			float t = (srcanim->numframes - 1) / srcanim->fps;

			float r = 1 / t;
			
			float a = atan2( srcanim->piecewisemove[j].pos[1], srcanim->piecewisemove[j].pos[0] ) * (180 / M_PI);
			float d = sqrt( DotProduct( srcanim->piecewisemove[j].pos, srcanim->piecewisemove[j].pos ) );
			if( g_verbose )
			{
				printf("%12s %7.2f %7.2f : %7.2f (%7.2f) %.1f\n", srcanim->name, srcanim->piecewisemove[j].pos[0], srcanim->piecewisemove[j].pos[1], d * r, a, t );
			}
		}

		if (srcanim->numsections > 1)
		{
			destanim->sectionindex = pData - (byte *)destanim;
			pData += srcanim->numsections * sizeof( mstudioanimsections_t );
		}

		// VectorCopy( srcanim->linearrot, destanim->linearrot );
		// destanim->automoveposindex = srcanim->automoveposindex;
		// destanim->automoveangleindex = srcanim->automoveangleindex;

		// align all animation data to cache line boundaries
		ALIGN16( pData );
		ALIGN16( pBlockData );

		if (pBlockStart)
		{
			// allocate the first block if needed
			if (g_numanimblocks == 0)
			{
				g_numanimblocks = 1;
				g_animblock[g_numanimblocks].start = pBlockData;
				g_numanimblocks++;
			}
		}

		if (!pBlockStart || (g_bonesaveframe.Count() == 0 && srcanim->numframes == 1))
		{
			// hack
			srcanim->disableAnimblocks = true;
		}
		else if (g_bNoAnimblockStall)
		{
			srcanim->isFirstSectionLocal = true;
		}
	
		// block zero is relative to me
		g_animblock[0].start = (byte *)(destanim);

		byte *pAnimData = NULL;
		byte *pIkData = NULL;
		byte *pLocalHierarchy = NULL;
		byte *pBlockEnd = pBlockData;

		if (srcanim->disableAnimblocks || srcanim->isFirstSectionLocal)
		{
			destanim->animblock	= 0;
			pAnimData = pData;
			WriteAnimationData( srcanim, destanim, pData, pBlockEnd );
			pIkData = pData;
			pLocalHierarchy = WriteIkErrors( srcanim, pIkData );
			pData = WriteLocalHierarchy( srcanim, pLocalHierarchy );
		}
		else
		{
			pAnimData = pBlockEnd;
			WriteAnimationData( srcanim, destanim, pData, pBlockEnd );
			if ( destanim->sectionindex )
			{
				// if sections were written, don't move the data already written to the last block
				pBlockData = pBlockEnd;
			}
			destanim->animblock = g_numanimblocks-1;
			pIkData = pBlockEnd;
			pLocalHierarchy = WriteIkErrors( srcanim, pIkData );
			pBlockEnd = WriteLocalHierarchy( srcanim, pLocalHierarchy );
		}

		// printf("%d %x %x %x   %s : %d\n", g_numanimblocks - 1, g_animblock[g_numanimblocks-1].start, pBlockData, pBlockEnd, srcanim->name, srcanim->numsections );

		if (pBlockData != pBlockEnd && pBlockEnd - g_animblock[g_numanimblocks-1].start > g_animblocksize)
		{
			g_animblock[g_numanimblocks-1].end = pBlockData;
			g_animblock[g_numanimblocks].start = pBlockData;
			g_numanimblocks++;
			destanim->animblock	= g_numanimblocks-1;
		}

		destanim->animindex = pAnimData - g_animblock[destanim->animblock].start;

		if ( srcanim->numikrules )
		{
			destanim->numikrules = srcanim->numikrules;
			if (destanim->animblock == 0)
			{
				destanim->ikruleindex = pIkData - g_animblock[destanim->animblock].start;
			}
			else
			{
				destanim->animblockikruleindex = pIkData - g_animblock[destanim->animblock].start;
			}
		}
		if ( srcanim->numlocalhierarchy )
		{
			destanim->numlocalhierarchy = srcanim->numlocalhierarchy;
			destanim->localhierarchyindex = pLocalHierarchy - g_animblock[destanim->animblock].start;
		}

		if (g_numanimblocks)
		{
			g_animblock[g_numanimblocks-1].end = pBlockEnd;
			pBlockData = pBlockEnd;
		}

		// printf("%s : %d:%d\n", srcanim->name, destanim->animblock, destanim->animindex );

		// printf("raw bone data %d : %s\n", (byte *)destanimvalue - pData, srcanim->name);
	}

	if( !g_quiet )
	{
		/*
		for (i = 0; i < g_numanimblocks; i++)
		{
			printf("%2d (%3d:%3d): %d\n", i, g_animblock[i].iStartAnim, g_animblock[i].iEndAnim, g_animblock[i].end - g_animblock[i].start );
		}
		*/
	}

	if( !g_quiet )
	{
		/*
		printf("raw anim data %d : %d\n", rawanimbytes, animboneframes );
		printf("pos  %d %d %d %d\n", numPos[0], numPos[1], numPos[2], numPos[3] );
		printf("axis %d %d %d %d : %d\n", numAxis[0], numAxis[1], numAxis[2], numAxis[3], useRaw );
		*/
	}

	// write movement keys
	for (i = 0; i < g_numani; i++) 
	{
		s_animation_t *anim = g_panimation[ i ];

		// panimdesc[i].entrancevelocity = anim->entrancevelocity;
		panimdesc[i].nummovements = anim->numpiecewisekeys;
		if (panimdesc[i].nummovements)
		{
			panimdesc[i].movementindex = pData - (byte*)&panimdesc[i];

			mstudiomovement_t	*pmove = (mstudiomovement_t *)pData;
			pData += panimdesc[i].nummovements * sizeof( *pmove );
			ALIGN4( pData );

			for (j = 0; j < panimdesc[i].nummovements; j++)
			{
				pmove[j].endframe		= anim->piecewisemove[j].endframe;
				pmove[j].motionflags	= anim->piecewisemove[j].flags;
				pmove[j].v0				= anim->piecewisemove[j].v0;
				pmove[j].v1				= anim->piecewisemove[j].v1;
				pmove[j].angle			= RAD2DEG( anim->piecewisemove[j].rot[2] );
				VectorCopy( anim->piecewisemove[j].vector, pmove[j].vector );
				VectorCopy( anim->piecewisemove[j].pos, pmove[j].position );
			}
		}
	}

	// only write zero frames if the animation data is demand loaded
	if (!pBlockStart)
		return pData;


	// calculate what bones should be have zero frame saved out
	if (g_bonesaveframe.Count() == 0)
	{
		for (j = 0; j < g_numbones; j++)
		{
			if ((g_bonetable[j].parent == -1) || (g_bonetable[j].posrange.Length() > 2.0))
			{
				g_bonetable[j].flags |= BONE_HAS_SAVEFRAME_POS;
			}
			g_bonetable[j].flags |= BONE_HAS_SAVEFRAME_ROT;

			if ((!g_quiet) && (g_bonetable[j].flags & (BONE_HAS_SAVEFRAME_POS | BONE_HAS_SAVEFRAME_ROT)))
			{
				printf("$BoneSaveFrame \"%s\"", g_bonetable[j].name );
				if (g_bonetable[j].flags & BONE_HAS_SAVEFRAME_POS)
					printf(" position" );
				if (g_bonetable[j].flags & BONE_HAS_SAVEFRAME_ROT)
					printf(" rotation" );
				printf("\n");
			}
		}
	}
	else
	{
		for (i = 0; i < g_bonesaveframe.Count(); i++)
		{
			j = findGlobalBone( g_bonesaveframe[i].name );

			if (j != -1)
			{
				if (g_bonesaveframe[i].bSavePos)
				{
					g_bonetable[j].flags |= BONE_HAS_SAVEFRAME_POS;
				}
				if (g_bonesaveframe[i].bSaveRot)
				{
					g_bonetable[j].flags |= BONE_HAS_SAVEFRAME_ROT;
				}
			}
		}
	}

	for (j = 0; j < g_numbones; j++)
	{
		phdr->pBone(j)->flags |= g_bonetable[j].flags;
	}

	ALIGN4( pData );

	// write zero frames
	for (i = 0; i < g_numani; i++) 
	{
		s_animation_t *anim = g_panimation[ i ];

		if (panimdesc[i].animblock != 0)
		{
			panimdesc[i].zeroframeindex = pData - (byte *)&panimdesc[i];

			int k = min( panimdesc[i].numframes - 1, 9 );
			if (panimdesc[i].flags & STUDIO_LOOPING)
			{
				k = min( (panimdesc[i].numframes - 1) / 2, k );
			}
			panimdesc[i].zeroframespan = k;
			if (k > 2)
			{
				panimdesc[i].zeroframecount = min( (panimdesc[i].numframes - 1) / panimdesc[i].zeroframespan, 3 ); // save frames 0..24 frames
			}
			if (panimdesc[i].zeroframecount < 1)
				panimdesc[i].zeroframecount = 1;

			for (j = 0; j < g_numbones; j++)
			{
				if (g_bonetable[j].flags & BONE_HAS_SAVEFRAME_POS)
				{
					for (int n = 0; n < panimdesc[i].zeroframecount; n++)
					{
						*(Vector48 *)pData = anim->sanim[panimdesc[i].zeroframespan*n][j].pos;
						pData += sizeof( Vector48 );
					}
				}
				if (g_bonetable[j].flags & BONE_HAS_SAVEFRAME_ROT)
				{
					for (int n = 0; n < panimdesc[i].zeroframecount; n++)
					{
						Quaternion q;
						AngleQuaternion( anim->sanim[panimdesc[i].zeroframespan*n][j].rot, q );
						*((Quaternion64 *)pData) = q;
						pData += sizeof( Quaternion64 );
					}
				}
			}
		}
	}

	ALIGN4( pData );

	return pData;
}



static void WriteTextures( studiohdr_t *phdr )
{
	int i, j;
	short	*pref;

	// save texture info
	mstudiotexture_t *ptexture = (mstudiotexture_t *)pData;
	phdr->numtextures = g_nummaterials;
	phdr->textureindex = pData - pStart;
	pData += g_nummaterials * sizeof( mstudiotexture_t );
	for (i = 0; i < g_nummaterials; i++) 
	{
		j = g_material[i];
		AddToStringTable( &ptexture[i], &ptexture[i].sznameindex, g_texture[j].name );
	}
	ALIGN4( pData );

	int *cdtextureoffset = (int *)pData;
	phdr->numcdtextures = numcdtextures;
	phdr->cdtextureindex = pData - pStart;
	pData += numcdtextures * sizeof( int );
	for (i = 0; i < numcdtextures; i++) 
	{
		AddToStringTable( phdr, &cdtextureoffset[i], cdtextures[i] );
	}
	ALIGN4( pData );

	// save texture directory info
	phdr->skinindex = (pData - pStart);
	phdr->numskinref = g_numskinref;
	phdr->numskinfamilies = g_numskinfamilies;
	pref = (short *)pData;

	for (i = 0; i < phdr->numskinfamilies; i++) 
	{
		for (j = 0; j < phdr->numskinref; j++) 
		{
			*pref = g_skinref[i][j];
			pref++;
		}
	}
	pData = (byte *)pref;
	ALIGN4( pData );
}


//-----------------------------------------------------------------------------
// Write source bone transforms
//-----------------------------------------------------------------------------
static void WriteBoneTransforms( studiohdr2_t *phdr, mstudiobone_t *pBone )
{
	matrix3x4_t identity;
	SetIdentityMatrix( identity );

	int nTransformCount = 0;
	for (int i = 0; i < g_numbones; i++)
	{
		if ( g_bonetable[i].flags & BONE_ALWAYS_PROCEDURAL )
			continue;
		int nParent = g_bonetable[i].parent;

		// Transformation is necessary if either you or your parent was realigned
		if ( MatricesAreEqual( identity, g_bonetable[i].srcRealign ) &&
			 ( ( nParent < 0 ) || MatricesAreEqual( identity, g_bonetable[nParent].srcRealign ) ) )
			continue;

		++nTransformCount;
	}
	
	// save bone transform info
	mstudiosrcbonetransform_t *pSrcBoneTransform = (mstudiosrcbonetransform_t *)pData;
	phdr->numsrcbonetransform = nTransformCount;
	phdr->srcbonetransformindex = pData - pStart;
	pData += nTransformCount * sizeof( mstudiosrcbonetransform_t );
	int bt = 0;
	for ( int i = 0; i < g_numbones; i++ )
	{
		if ( g_bonetable[i].flags & BONE_ALWAYS_PROCEDURAL )
			continue;
		int nParent = g_bonetable[i].parent;
		if ( MatricesAreEqual( identity, g_bonetable[i].srcRealign ) &&
			( ( nParent < 0 ) || MatricesAreEqual( identity, g_bonetable[nParent].srcRealign ) ) )
			continue;
							   
		// What's going on here?
		// So, when we realign a bone, we want to do it in a way so that the child bones
		// have the same bone->world transform. If we take T as the src realignment transform
		// for the parent, P is the parent to world, and C is the child to parent, we expect 
		// the child->world is constant after realignment:
		//		CtoW = P * C = ( P * T ) * ( T^-1 * C )
		// therefore Cnew = ( T^-1 * C )
		if ( nParent >= 0 )
		{
			MatrixInvert( g_bonetable[nParent].srcRealign, pSrcBoneTransform[bt].pretransform );
		}
		else
		{
			SetIdentityMatrix( pSrcBoneTransform[bt].pretransform );
		}
		MatrixCopy( g_bonetable[i].srcRealign, pSrcBoneTransform[bt].posttransform );
		AddToStringTable( &pSrcBoneTransform[bt], &pSrcBoneTransform[bt].sznameindex, g_bonetable[i].name );
		++bt;
	}
	ALIGN4( pData );

	if (g_numbones > 1)
	{
		// write second bone table
		phdr->linearboneindex = pData - (byte *)phdr;
		mstudiolinearbone_t *pLinearBone =  (mstudiolinearbone_t *)pData;
		pData += sizeof( *pLinearBone );

		pLinearBone->numbones = g_numbones;

#define WRITE_BONE_BLOCK( type, srcfield, dest, destindex ) \
		type *##dest = (type *)pData; \
		pLinearBone->##destindex = pData - (byte *)pLinearBone; \
		pData += g_numbones * sizeof( *##dest ); \
		ALIGN4( pData ); \
		for ( int i = 0; i < g_numbones; i++) \
			dest##[i] = pBone[i].##srcfield;

		WRITE_BONE_BLOCK( int, flags, pFlags, flagsindex );
		WRITE_BONE_BLOCK( int, parent, pParent, parentindex );
		WRITE_BONE_BLOCK( Vector, pos, pPos, posindex );
		WRITE_BONE_BLOCK( Quaternion, quat, pQuat, quatindex );
		WRITE_BONE_BLOCK( RadianEuler, rot, pRot, rotindex );
		WRITE_BONE_BLOCK( matrix3x4_t, poseToBone, pPoseToBone, posetoboneindex );
		WRITE_BONE_BLOCK( Vector, posscale, pPoseScale, posscaleindex );
		WRITE_BONE_BLOCK( Vector, rotscale, pRotScale, rotscaleindex );
		WRITE_BONE_BLOCK( Quaternion, qAlignment, pQAlignment, qalignmentindex );
	}
}


//-----------------------------------------------------------------------------
// Write the bone flex drivers
//-----------------------------------------------------------------------------
static void WriteBoneFlexDrivers( studiohdr2_t *pStudioHdr2 )
{
	ALIGN4( pData );

	pStudioHdr2->m_nBoneFlexDriverCount = 0;
	pStudioHdr2->m_nBoneFlexDriverIndex = 0;

	CDmeBoneFlexDriverList *pDmeBoneFlexDriverList = GetElement< CDmeBoneFlexDriverList >( g_hDmeBoneFlexDriverList );
	if ( !pDmeBoneFlexDriverList )
		return;

	const int nBoneFlexDriverCount = pDmeBoneFlexDriverList->m_eBoneFlexDriverList.Count();
	if ( nBoneFlexDriverCount <= 0 )
		return;

	mstudioboneflexdriver_t *pBoneFlexDriver = (mstudioboneflexdriver_t *)pData;
	pStudioHdr2->m_nBoneFlexDriverCount = nBoneFlexDriverCount;
	pStudioHdr2->m_nBoneFlexDriverIndex = pData - (byte *)pStudioHdr2;
	pData += nBoneFlexDriverCount * sizeof( mstudioboneflexdriver_t );
	ALIGN4( pData );

	for ( int i = 0; i < nBoneFlexDriverCount; ++i )
	{
		CDmeBoneFlexDriver *pDmeBoneFlexDriver = pDmeBoneFlexDriverList->m_eBoneFlexDriverList[i];
		Assert( pDmeBoneFlexDriver );
		Assert( pDmeBoneFlexDriver->m_eControlList.Count() > 0 );
		Assert( pDmeBoneFlexDriver->GetValue< int >( "__boneIndex", -1 ) >= 0 );

		pBoneFlexDriver->m_nBoneIndex = pDmeBoneFlexDriver->GetValue< int >( "__boneIndex", 0 );
		pBoneFlexDriver->m_nControlCount = pDmeBoneFlexDriver->m_eControlList.Count();
		pBoneFlexDriver->m_nControlIndex = pData - (byte *)pBoneFlexDriver;

		mstudioboneflexdrivercontrol_t *pControl = reinterpret_cast< mstudioboneflexdrivercontrol_t * >( pData );

		for ( int j = 0; j < pBoneFlexDriver->m_nControlCount; ++j )
		{
			CDmeBoneFlexDriverControl *pDmeControl = pDmeBoneFlexDriver->m_eControlList[j];
			Assert( pDmeControl );
			Assert( pDmeControl->GetValue< int >( "__flexControlIndex", -1 ) >= 0 );
			Assert( pDmeControl->m_nBoneComponent >= STUDIO_BONE_FLEX_TX );
			Assert( pDmeControl->m_nBoneComponent <= STUDIO_BONE_FLEX_TZ );

			pControl[j].m_nFlexControllerIndex = pDmeControl->GetValue< int >( "__flexControlIndex", 0 );
			pControl[j].m_nBoneComponent = pDmeControl->m_nBoneComponent;
			pControl[j].m_flMin = pDmeControl->m_flMin;
			pControl[j].m_flMax = pDmeControl->m_flMax;
		}

		pData += pBoneFlexDriver->m_nControlCount * sizeof( mstudioboneflexdrivercontrol_t );
		ALIGN4( pData );

		++pBoneFlexDriver;
	}
}


//-----------------------------------------------------------------------------
// Write the processed vertices
//-----------------------------------------------------------------------------
static void WriteVertices( studiohdr_t *phdr )
{
	char			fileName[MAX_PATH];
	byte			*pStart;
	byte			*pData;
	int				i;
	int				j;
	int				k;
	int				cur;

	if (!g_nummodelsbeforeLOD)
		return;

	V_strcpy_safe( fileName, gamedir );
//	if( *g_pPlatformName )
//	{
//		strcat( fileName, "platform_" );
//		strcat( fileName, g_pPlatformName );
//		strcat( fileName, "/" );	
//	}
	V_strcat_safe( fileName, "models/" );	
	V_strcat_safe( fileName, outname );
	Q_StripExtension( fileName, fileName, sizeof( fileName ) );
	V_strcat_safe( fileName, ".vvd" );

	if ( !g_quiet )
	{
		printf ("---------------------\n");
		printf ("writing %s:\n", fileName);
	}

	pStart = (byte *)kalloc( 1, FILEBUFFER );
	pData  = pStart;

	vertexFileHeader_t *fileHeader = (vertexFileHeader_t *)pData;
	pData += sizeof(vertexFileHeader_t);

	fileHeader->id        = MODEL_VERTEX_FILE_ID;
	fileHeader->version   = MODEL_VERTEX_FILE_VERSION;
	fileHeader->checksum  = phdr->checksum;

	// data has no fixes and requires no fixes
	fileHeader->numFixups       = 0;
	fileHeader->fixupTableStart = 0;

	// unfinalized during first pass, fixed during second pass
	// data can be considered as single lod at lod 0
	fileHeader->numLODs = 1;
	fileHeader->numLODVertexes[0] = 0;

	// store vertexes grouped by mesh order
	ALIGN16( pData );
	fileHeader->vertexDataStart  = pData-pStart;
	for (i = 0; i < g_nummodelsbeforeLOD; i++) 
	{
		s_loddata_t *pLodData = g_model[i]->m_pLodData;

		// skip blank empty model
		if (!pLodData)
			continue;

		// save vertices
		ALIGN16( pData );
		cur = (int)pData;
		mstudiovertex_t *pVert = (mstudiovertex_t *)pData;
		pData += pLodData->numvertices * sizeof( mstudiovertex_t );
		for (j = 0; j < pLodData->numvertices; j++)
		{
//			printf( "saving bone weight %d for model %d at 0x%p\n",
//				j, i, &pbone[j] );

			const s_vertexinfo_t &lodVertex = pLodData->vertex[j];
			VectorCopy( lodVertex.position, pVert[j].m_vecPosition );
			VectorCopy( lodVertex.normal, pVert[j].m_vecNormal );
			Vector2DCopy( lodVertex.texcoord, pVert[j].m_vecTexCoord );

			mstudioboneweight_t *pBoneWeight = &pVert[j].m_BoneWeights;
			memset( pBoneWeight, 0, sizeof( mstudioboneweight_t ) );
			pBoneWeight->numbones = lodVertex.boneweight.numbones;
			for (k = 0; k < pBoneWeight->numbones; k++)
			{
				pBoneWeight->bone[k]   = lodVertex.boneweight.bone[k];
				pBoneWeight->weight[k] = lodVertex.boneweight.weight[k];
			}
		}

		fileHeader->numLODVertexes[0] += pLodData->numvertices;

		if (!g_quiet)
		{
			printf( "vertices   %7d bytes (%d vertices)\n", (int)(pData - cur), pLodData->numvertices );
		}
	}

	// store tangents grouped by mesh order
	ALIGN4( pData );
	fileHeader->tangentDataStart = pData-pStart;
	for (i = 0; i < g_nummodelsbeforeLOD; i++) 
	{
		s_loddata_t *pLodData = g_model[i]->m_pLodData;

		// skip blank empty model
		if (!pLodData)
			continue;

		// save tangent space S
		ALIGN4( pData );
		cur = (int)pData;
		Vector4D *ptangents = (Vector4D *)pData;
		pData += pLodData->numvertices * sizeof( Vector4D );
		for (j = 0; j < pLodData->numvertices; j++)
		{
			Vector4DCopy( pLodData->vertex[j].tangentS, ptangents[j] );
#ifdef _DEBUG
			float w = ptangents[j].w;
			Assert( w == 1.0f || w == -1.0f );
#endif
		}

		if (!g_quiet)
		{
			printf( "tangents   %7d bytes (%d vertices)\n", (int)(pData - cur), pLodData->numvertices );
		}
	}

	if (!g_quiet)
	{
		printf( "total      %7d bytes\n", pData - pStart );
	}

	// fileHeader->length = pData - pStart;
	{
		CP4AutoEditAddFile autop4( fileName, "binary" );
		SaveFile( fileName, pStart, pData - pStart );
	}
}


//-----------------------------------------------------------------------------
// Computes the maximum absolute value of any component of all vertex animation
// pos (x,y,z) normal (x,y,z) or wrinkle
//
// Returns the fixed point scale and also sets appropriate values & flags in
// passed studiohdr_t
//-----------------------------------------------------------------------------
float ComputeVertAnimFixedPointScale( studiohdr_t *pStudioHdr )
{
	float flVertAnimRange = 0.0f;

	for ( int j = 0; j < g_numflexkeys; ++j )
	{
		if ( g_flexkey[j].numvanims <= 0 )
			continue;

		const bool bWrinkleVAnim = ( g_flexkey[j].vanimtype == STUDIO_VERT_ANIM_WRINKLE );

		s_vertanim_t *pVertAnim = g_flexkey[j].vanim;

		for ( int k = 0; k < g_flexkey[j].numvanims; ++k )
		{
			if ( fabs( pVertAnim->pos.x ) > flVertAnimRange )
			{
				flVertAnimRange = fabs( pVertAnim->pos.x );
			}

			if ( fabs( pVertAnim->pos.y ) > flVertAnimRange )
			{
				flVertAnimRange = fabs( pVertAnim->pos.y );
			}

			if ( fabs( pVertAnim->pos.z ) > flVertAnimRange )
			{
				flVertAnimRange = fabs( pVertAnim->pos.z );
			}

			if ( fabs( pVertAnim->normal.x ) > flVertAnimRange )
			{
				flVertAnimRange = fabs( pVertAnim->normal.x );
			}

			if ( fabs( pVertAnim->normal.y ) > flVertAnimRange )
			{
				flVertAnimRange = fabs( pVertAnim->normal.y );
			}

			if ( fabs( pVertAnim->normal.z ) > flVertAnimRange )
			{
				flVertAnimRange = fabs( pVertAnim->normal.z );
			}

			if ( bWrinkleVAnim )
			{
				if ( fabs( pVertAnim->wrinkle ) > flVertAnimRange )
				{
					flVertAnimRange = fabs( pVertAnim->wrinkle );
				}
			}

			pVertAnim++;
		}
	}

	// Legacy value
	float flVertAnimFixedPointScale = 1.0 / 4096.0f;

	if ( flVertAnimRange > 0.0f )
	{
		if ( flVertAnimRange > 32767 )
		{
			MdlWarning( "Flex value too large: %.2f, Max: 32767\n", flVertAnimRange );

			flVertAnimFixedPointScale = 1.0f;
		}
		else
		{
			const float flTmpScale = flVertAnimRange / 32767.0f;
			if ( flTmpScale > flVertAnimFixedPointScale )
			{
				flVertAnimFixedPointScale = flTmpScale;
			}
		}
	}

	if ( flVertAnimFixedPointScale != 1.0f / 4096.0f )
	{
		pStudioHdr->flags |= STUDIOHDR_FLAGS_VERT_ANIM_FIXED_POINT_SCALE;
		pStudioHdr->flVertAnimFixedPointScale = flVertAnimFixedPointScale;
	}

	return flVertAnimFixedPointScale;
}


static void WriteModel( studiohdr_t *phdr )
{
	int i, j, k, m;
	mstudiobodyparts_t	*pbodypart;
	mstudiomodel_t		*pmodel;
	s_source_t			*psource;
	mstudiovertanim_t	*pvertanim;
	s_vertanim_t		*pvanim;

	int cur	= (int)pData;

	// vertex data is written to external file, offsets kept internal
	// track expected external base to store proper offsets
	byte *externalVertexIndex   = 0;
	byte *externalTangentsIndex = 0;

	// write bodypart info
	pbodypart = (mstudiobodyparts_t *)pData;
	phdr->numbodyparts = g_numbodyparts;
	phdr->bodypartindex = pData - pStart;
	pData += g_numbodyparts * sizeof( mstudiobodyparts_t );

	pmodel = (mstudiomodel_t *)pData;
	pData += g_nummodelsbeforeLOD * sizeof( mstudiomodel_t );

	for (i = 0, j = 0; i < g_numbodyparts; i++)
	{
		AddToStringTable( &pbodypart[i], &pbodypart[i].sznameindex, g_bodypart[i].name );
		pbodypart[i].nummodels		= g_bodypart[i].nummodels;
		pbodypart[i].base			= g_bodypart[i].base;
		pbodypart[i].modelindex		= ((byte *)&pmodel[j]) - (byte *)&pbodypart[i];
		j += g_bodypart[i].nummodels;
	}
	ALIGN4( pData );

	// write global flex names
	mstudioflexdesc_t *pflexdesc = (mstudioflexdesc_t *)pData;
	phdr->numflexdesc			= g_numflexdesc;
	phdr->flexdescindex			= pData - pStart;
	pData += g_numflexdesc * sizeof( mstudioflexdesc_t );
	ALIGN4( pData );

	for (j = 0; j < g_numflexdesc; j++)
	{
		// printf("%d %s\n", j, g_flexdesc[j].FACS );
		AddToStringTable( pflexdesc, &pflexdesc->szFACSindex, g_flexdesc[j].FACS );
		pflexdesc++;
	}

	// write global flex controllers
	mstudioflexcontroller_t *pflexcontroller = (mstudioflexcontroller_t *)pData;
	phdr->numflexcontrollers	= g_numflexcontrollers;
	phdr->flexcontrollerindex	= pData - pStart;
	pData += g_numflexcontrollers * sizeof( mstudioflexcontroller_t );
	ALIGN4( pData );

	for (j = 0; j < g_numflexcontrollers; j++)
	{
		AddToStringTable( pflexcontroller, &pflexcontroller->sznameindex, g_flexcontroller[j].name );
		AddToStringTable( pflexcontroller, &pflexcontroller->sztypeindex, g_flexcontroller[j].type );
		pflexcontroller->min = g_flexcontroller[j].min;
		pflexcontroller->max = g_flexcontroller[j].max;
		pflexcontroller->localToGlobal = -1;
		pflexcontroller++;
	}

	// write flex rules
	mstudioflexrule_t *pflexrule = (mstudioflexrule_t *)pData;
	phdr->numflexrules			= g_numflexrules;
	phdr->flexruleindex			= pData - pStart;
	pData += g_numflexrules * sizeof( mstudioflexrule_t );
	ALIGN4( pData );

	for (j = 0; j < g_numflexrules; j++)
	{
		pflexrule->flex		= g_flexrule[j].flex;
		pflexrule->numops	= g_flexrule[j].numops;
		pflexrule->opindex	= (pData - (byte *)pflexrule);

		mstudioflexop_t *pflexop = (mstudioflexop_t *)pData;

		for (i = 0; i < pflexrule->numops; i++)
		{
			pflexop[i].op = g_flexrule[j].op[i].op;
			pflexop[i].d.index = g_flexrule[j].op[i].d.index;
		}

		pData += sizeof( mstudioflexop_t ) * pflexrule->numops;
		ALIGN4( pData );

		pflexrule++;
	}

	// write global flex controller information

	mstudioflexcontrollerui_t *pFlexControllerUI = (mstudioflexcontrollerui_t *)pData;
	phdr->numflexcontrollerui	= 0;
	phdr->flexcontrolleruiindex	= pData - pStart;

	// Loop through all defined controllers and create a UI structure for them
	// All actual controllers will be defined as a member of some ui structure
	// and all actual controllers can only be a member of one ui structure
	bool *pControllerHandled = ( bool * )_alloca( g_numflexcontrollers * sizeof( bool ) );
	memset( pControllerHandled, 0, g_numflexcontrollers * sizeof( bool ) );

	for ( j = 0; j < g_numflexcontrollers; ++j )
	{
		// Don't handle controls twice
		if ( pControllerHandled[ j ] )
			continue;

		const s_flexcontroller_t &flexcontroller = g_flexcontroller[ j ];

		bool found = false;

		// See if this controller is in the remap table
		for ( k = 0; k < g_FlexControllerRemap.Count(); ++k )
		{
			s_flexcontrollerremap_t &remap = g_FlexControllerRemap[ k ];
			if ( j == remap.m_Index || j == remap.m_LeftIndex || j == remap.m_RightIndex || j == remap.m_MultiIndex )
			{
				AddToStringTable( pFlexControllerUI, &pFlexControllerUI->sznameindex, remap.m_Name );

				pFlexControllerUI->stereo = remap.m_bIsStereo;
				if ( pFlexControllerUI->stereo )
				{
					Assert( !pControllerHandled[ remap.m_LeftIndex ] );
					pFlexControllerUI->szindex0 = (
						phdr->flexcontrollerindex - int( pData - pStart ) +
						remap.m_LeftIndex * sizeof( mstudioflexcontroller_t ) );
					pControllerHandled[ remap.m_LeftIndex ] = true;

					Assert( !pControllerHandled[ remap.m_RightIndex ] );
					pFlexControllerUI->szindex1 = ( 
						phdr->flexcontrollerindex - int( pData - pStart ) +
						remap.m_RightIndex * sizeof( mstudioflexcontroller_t ) );
					pControllerHandled[ remap.m_RightIndex ] = true;
				}
				else
				{
					Assert( !pControllerHandled[ remap.m_Index ] );
					pFlexControllerUI->szindex0 = (
						phdr->flexcontrollerindex - int( pData - pStart ) +
						remap.m_Index * sizeof( mstudioflexcontroller_t ) );
					pControllerHandled[ remap.m_Index ] = true;
					pFlexControllerUI->szindex1 = ( 0 );
				}

				pFlexControllerUI->remaptype = remap.m_RemapType;
				if ( pFlexControllerUI->remaptype == FLEXCONTROLLER_REMAP_NWAY || pFlexControllerUI->remaptype == FLEXCONTROLLER_REMAP_EYELID )
				{
					Assert( remap.m_MultiIndex != -1 );
					Assert( !pControllerHandled[ remap.m_MultiIndex ] );
					pFlexControllerUI->szindex2 = (
						phdr->flexcontrollerindex - int( pData - pStart ) +
						remap.m_MultiIndex * sizeof( mstudioflexcontroller_t ) );
					pControllerHandled[ remap.m_MultiIndex ] = true;
				}
				else
				{
					pFlexControllerUI->szindex2 = 0;
				}

				found = true;
				break;
			}
		}

		if ( !found )
		{
			pFlexControllerUI->remaptype = FLEXCONTROLLER_REMAP_PASSTHRU;
			pFlexControllerUI->szindex2 = 0;	// Unused in this case

			if ( j < g_numflexcontrollers - 1 &&
				StringAfterPrefixCaseSensitive( flexcontroller.name, "right_" ) &&
				StringAfterPrefixCaseSensitive( g_flexcontroller[ j + 1 ].name, "left_" ) &&
				!Q_strcmp( StringAfterPrefixCaseSensitive( flexcontroller.name, "right_" ), StringAfterPrefixCaseSensitive( g_flexcontroller[ j + 1 ].name, "left_" ) ) )
			{
				AddToStringTable( pFlexControllerUI, &pFlexControllerUI->sznameindex, flexcontroller.name + 6 );

				pFlexControllerUI->stereo = true;

				Assert( !pControllerHandled[ j + 1 ] );
				pFlexControllerUI->szindex0 = (
						phdr->flexcontrollerindex - int( pData - pStart ) +
						( j + 1 ) * sizeof( mstudioflexcontroller_t ) );
				pControllerHandled[ j + 1 ] = true;

				Assert( !pControllerHandled[ j ] );
				pFlexControllerUI->szindex1 = (
						phdr->flexcontrollerindex - int( pData - pStart ) +
						j * sizeof( mstudioflexcontroller_t ) );
				pControllerHandled[ j ] = true;
			}
			else if ( j > 0 &&
				StringAfterPrefixCaseSensitive( flexcontroller.name, "left_" ) &&
				StringAfterPrefixCaseSensitive( g_flexcontroller[ j - 1 ].name, "right_" ) &&
				!Q_strcmp( StringAfterPrefixCaseSensitive( flexcontroller.name, "left_" ), StringAfterPrefixCaseSensitive( g_flexcontroller[ j - 1 ].name, "right_" ) ) )
			{
				AddToStringTable( pFlexControllerUI, &pFlexControllerUI->sznameindex, flexcontroller.name + 5 );

				pFlexControllerUI->stereo = true;

				Assert( !pControllerHandled[ j ] );
				pFlexControllerUI->szindex0 = (
						phdr->flexcontrollerindex - int( pData - pStart ) +
						j * sizeof( mstudioflexcontroller_t ) );
				pControllerHandled[ j ] = true;

				Assert( !pControllerHandled[ j - 1 ] );
				pFlexControllerUI->szindex1 = (
						phdr->flexcontrollerindex - int( pData - pStart ) +
						( j - 1 ) * sizeof( mstudioflexcontroller_t ) );
				pControllerHandled[ j - 1 ] = true;
			}
			else
			{
				AddToStringTable( pFlexControllerUI, &pFlexControllerUI->sznameindex, flexcontroller.name );
				pFlexControllerUI->stereo = false;
				pFlexControllerUI->szindex0 = (
						phdr->flexcontrollerindex - int( pData - pStart ) +
						j * sizeof( mstudioflexcontroller_t ) );
				pFlexControllerUI->szindex1 = 0;	// Unused in this case
				pControllerHandled[ j ] = true;
			}
		}

		phdr->numflexcontrollerui++;
		pData += sizeof( mstudioflexcontrollerui_t );
		++pFlexControllerUI;
	}
	ALIGN4( pData );

#ifdef _DEBUG
	for ( j = 0; j < g_numflexcontrollers; ++j )
	{
		Assert( pControllerHandled[ j ] );
	}
#endif // _DEBUG

	// write ik chains
	mstudioikchain_t *pikchain = (mstudioikchain_t *)pData;
	phdr->numikchains			= g_numikchains;
	phdr->ikchainindex			= pData - pStart;
	pData += g_numikchains * sizeof( mstudioikchain_t );
	ALIGN4( pData );

	for (j = 0; j < g_numikchains; j++)
	{
		AddToStringTable( pikchain, &pikchain->sznameindex, g_ikchain[j].name );
		pikchain->numlinks		= g_ikchain[j].numlinks;

		mstudioiklink_t *piklink = (mstudioiklink_t *)pData;
		pikchain->linkindex		= (pData - (byte *)pikchain);
		pData += pikchain->numlinks * sizeof( mstudioiklink_t );

		for (i = 0; i < pikchain->numlinks; i++)
		{
			piklink[i].bone = g_ikchain[j].link[i].bone;
			piklink[i].kneeDir = g_ikchain[j].link[i].kneeDir;
		}

		pikchain++;
	}

	// save autoplay locks
	mstudioiklock_t *piklock = (mstudioiklock_t *)pData;
	phdr->numlocalikautoplaylocks	= g_numikautoplaylocks;
	phdr->localikautoplaylockindex	= pData - pStart;
	pData += g_numikautoplaylocks * sizeof( mstudioiklock_t );
	ALIGN4( pData );

	for (j = 0; j < g_numikautoplaylocks; j++)
	{
		piklock->chain			= g_ikautoplaylock[j].chain;
		piklock->flPosWeight	= g_ikautoplaylock[j].flPosWeight;
		piklock->flLocalQWeight	= g_ikautoplaylock[j].flLocalQWeight;
		piklock++;
	}

	// save mouth info
	mstudiomouth_t *pmouth = (mstudiomouth_t *)pData;
	phdr->nummouths = g_nummouths;
	phdr->mouthindex = pData - pStart;
	pData += g_nummouths * sizeof( mstudiomouth_t );
	ALIGN4( pData );

	for (i = 0; i < g_nummouths; i++) {
		pmouth[i].bone			= g_mouth[i].bone;
		VectorCopy( g_mouth[i].forward, pmouth[i].forward );
		pmouth[i].flexdesc = g_mouth[i].flexdesc;
	}

	// save pose parameters
	mstudioposeparamdesc_t *ppose = (mstudioposeparamdesc_t *)pData;
	phdr->numlocalposeparameters = g_numposeparameters;
	phdr->localposeparamindex = pData - pStart;
	pData += g_numposeparameters * sizeof( mstudioposeparamdesc_t );
	ALIGN4( pData );

	for (i = 0; i < g_numposeparameters; i++)
	{
		AddToStringTable( &ppose[i], &ppose[i].sznameindex, g_pose[i].name );
		ppose[i].start	= g_pose[i].min;
		ppose[i].end	= g_pose[i].max;
		ppose[i].flags	= g_pose[i].flags;
		ppose[i].loop	= g_pose[i].loop;
	}

	if( !g_quiet )
	{
		printf("ik/pose    %7d bytes\n", (int)(pData - cur) );
	}
	cur = (int)pData;

	const float flVertAnimFixedPointScale = ComputeVertAnimFixedPointScale( phdr );

	// write model
	for (i = 0; i < g_nummodelsbeforeLOD; i++) 
	{
		int n = 0;

		byte *pModelStart = (byte *)(&pmodel[i]);
		
		strcpy( pmodel[i].name, g_model[i]->filename );
		// AddToStringTable( &pmodel[i], &pmodel[i].sznameindex, g_model[i]->filename );

		// pmodel[i].mrmbias = g_model[i]->mrmbias;
		// pmodel[i].minresolution = g_model[i]->minresolution;
		// pmodel[i].maxresolution = g_model[i]->maxresolution;

		// save bbox info
		
		psource = g_model[i]->source;
		s_loddata_t *pLodData = g_model[i]->m_pLodData;

		// save mesh info
		if (pLodData)
		{
			pmodel[i].numvertices = pLodData->numvertices;
		}
		else
		{
			// empty model
			pmodel[i].numvertices = 0;
		}

		if ( pmodel[i].numvertices >= MAXSTUDIOVERTS )
		{
			// We have to check this here so that we don't screw up decal
			// vert caching in the runtime.
			MdlError( "Too many verts in model. (%d verts, MAXSTUDIOVERTS==%d)\n", 
				pmodel[i].numvertices, ( int )MAXSTUDIOVERTS );
		}

		mstudiomesh_t *pmesh = (mstudiomesh_t *)pData;
		pmodel[i].meshindex = (pData - pModelStart);
		pData += psource->nummeshes * sizeof( mstudiomesh_t );
		ALIGN4( pData );

		pmodel[i].nummeshes = psource->nummeshes;
		for (m = 0; m < pmodel[i].nummeshes; m++)
		{
			n = psource->meshindex[m];

			pmesh[m].material     = n;
			pmesh[m].modelindex   = (byte *)&pmodel[i] - (byte *)&pmesh[m];
			pmesh[m].numvertices  = pLodData->mesh[n].numvertices;
			pmesh[m].vertexoffset = pLodData->mesh[n].vertexoffset;
		}

		// set expected base offsets to external data
		ALIGN16( externalVertexIndex );
		pmodel[i].vertexindex = (int)externalVertexIndex; 
		externalVertexIndex += pmodel[i].numvertices * sizeof(mstudiovertex_t);

		// set expected base offsets to external data
		ALIGN4( externalTangentsIndex );
		pmodel[i].tangentsindex = (int)externalTangentsIndex;
		externalTangentsIndex += pmodel[i].numvertices * sizeof( Vector4D );

		cur = (int)pData;

		// save eyeballs
		mstudioeyeball_t *peyeball;
		peyeball					= (mstudioeyeball_t *)pData;
		pmodel[i].numeyeballs		= g_model[i]->numeyeballs;
		pmodel[i].eyeballindex		= pData - pModelStart;
		pData += g_model[i]->numeyeballs * sizeof( mstudioeyeball_t );
			
		ALIGN4( pData );
		for (j = 0; j < g_model[i]->numeyeballs; j++)
		{
			k = g_model[i]->eyeball[j].mesh;
			pmesh[k].materialtype		= 1;	// FIXME: tag custom material
			pmesh[k].materialparam		= j;	// FIXME: tag custom material

			peyeball[j].bone			= g_model[i]->eyeball[j].bone;
			VectorCopy( g_model[i]->eyeball[j].org, peyeball[j].org );
			peyeball[j].zoffset			= g_model[i]->eyeball[j].zoffset;
			peyeball[j].radius			= g_model[i]->eyeball[j].radius;
			VectorCopy( g_model[i]->eyeball[j].up, peyeball[j].up );
			VectorCopy( g_model[i]->eyeball[j].forward, peyeball[j].forward );
			peyeball[j].iris_scale		= g_model[i]->eyeball[j].iris_scale;

			for (k = 0; k < 3; k++)
			{
				peyeball[j].upperflexdesc[k]	= g_model[i]->eyeball[j].upperflexdesc[k];
				peyeball[j].lowerflexdesc[k]	= g_model[i]->eyeball[j].lowerflexdesc[k];
				peyeball[j].uppertarget[k]		= g_model[i]->eyeball[j].uppertarget[k];
				peyeball[j].lowertarget[k]		= g_model[i]->eyeball[j].lowertarget[k];
			}

			peyeball[j].upperlidflexdesc	= g_model[i]->eyeball[j].upperlidflexdesc;
			peyeball[j].lowerlidflexdesc	= g_model[i]->eyeball[j].lowerlidflexdesc;
		}	

		if ( !g_quiet )
		{
			printf("eyeballs   %7d bytes (%d eyeballs)\n", (int)(pData - cur), g_model[i]->numeyeballs );
		}

		// move flexes into individual meshes
		cur = (int)pData;
		for (m = 0; m < pmodel[i].nummeshes; m++)
		{
			int numflexkeys[MAXSTUDIOFLEXKEYS];
			pmesh[m].numflexes = 0;

			// initialize array
			for (j = 0; j < g_numflexkeys; j++)
			{
				numflexkeys[j] = 0;
			}

			// count flex instances per mesh
			for (j = 0; j < g_numflexkeys; j++)
			{
				if (g_flexkey[j].imodel == i)
				{
					for (k = 0; k < g_flexkey[j].numvanims; k++)
					{
						n = g_flexkey[j].vanim[k].vertex - pmesh[m].vertexoffset;
						if (n >= 0 && n < pmesh[m].numvertices)
						{
							if (numflexkeys[j]++ == 0)
							{
								pmesh[m].numflexes++;
							}
						}
					}
				}
			}

			if (pmesh[m].numflexes)
			{
				pmesh[m].flexindex	= ( pData - (byte *)&pmesh[m] );
				mstudioflex_t *pflex = (mstudioflex_t *)pData;
				pData += pmesh[m].numflexes * sizeof( mstudioflex_t );
				ALIGN4( pData );

				for (j = 0; j < g_numflexkeys; j++)
				{
					if (!numflexkeys[j])
						continue;

					pflex->flexdesc		= g_flexkey[j].flexdesc;
					pflex->target0		= g_flexkey[j].target0;
					pflex->target1		= g_flexkey[j].target1;
					pflex->target2		= g_flexkey[j].target2;
					pflex->target3		= g_flexkey[j].target3;
					pflex->numverts		= numflexkeys[j];
					pflex->vertindex	= (pData - (byte *)pflex);
					pflex->flexpair		= g_flexkey[j].flexpair;
					pflex->vertanimtype	= g_flexkey[j].vanimtype;

					// printf("%d %d %s : %f %f %f %f\n", j, g_flexkey[j].flexdesc, g_flexdesc[g_flexkey[j].flexdesc].FACS, g_flexkey[j].target0, g_flexkey[j].target1, g_flexkey[j].target2, g_flexkey[j].target3 );
					// if (j < 9) printf("%d %d %s : %d (%d) %f\n", j, g_flexkey[j].flexdesc, g_flexdesc[g_flexkey[j].flexdesc].FACS, g_flexkey[j].numvanims, pflex->numverts, g_flexkey[j].target );

					// printf("%d %d : %d %f\n", j, g_flexkey[j].flexnum, g_flexkey[j].numvanims, g_flexkey[j].target );

					pvanim = g_flexkey[j].vanim;

					bool bWrinkleVAnim = ( pflex->vertanimtype == STUDIO_VERT_ANIM_WRINKLE );
					int nVAnimDeltaSize = bWrinkleVAnim ? sizeof(mstudiovertanim_wrinkle_t) : sizeof(mstudiovertanim_t);

					pvertanim = (mstudiovertanim_t *)pData;
					pData += pflex->numverts * nVAnimDeltaSize;
					ALIGN4( pData );
				
					for ( k = 0; k < g_flexkey[j].numvanims; k++ )
					{
						n = g_flexkey[j].vanim[k].vertex - pmesh[m].vertexoffset;
						if ( n >= 0 && n < pmesh[m].numvertices )
						{
							pvertanim->index = n;
							pvertanim->speed = 255.0F*pvanim->speed;
							pvertanim->side  = 255.0F*pvanim->side;

							pvertanim->SetDeltaFloat( pvanim->pos );
							pvertanim->SetNDeltaFloat( pvanim->normal );
							
							if ( bWrinkleVAnim )
							{
								( (mstudiovertanim_wrinkle_t*)pvertanim )->SetWrinkleFixed( pvanim->wrinkle, flVertAnimFixedPointScale );
							}

							pvertanim = (mstudiovertanim_t*)( (byte*)pvertanim + nVAnimDeltaSize );

							/*
							if ((tmp - pvanim->pos).Length() > 0.1)
							{	
								pvertanim->delta.x = pvanim->pos.x;
								printf("%f %f %f  : %f %f %f\n", 
									pvanim->pos[0], pvanim->pos[1], pvanim->pos[2],
									tmp.x, tmp.y, tmp.z );
							}
							*/
							// if (j < 9) printf("%d %.2f %.2f %.2f\n", n, pvanim->pos[0], pvanim->pos[1], pvanim->pos[2] );
						}
						// printf("%d %.2f %.2f %.2f\n", pvanim->vertex, pvanim->pos[0], pvanim->pos[1], pvanim->pos[2] );
						pvanim++;
					}
					pflex++;
				}
			}
		}

		if( !g_quiet )
		{
			printf("flexes     %7d bytes (%d flexes)\n", (int)(pData - cur), g_numflexkeys );
		}
		cur = (int)pData;
	}


	ALIGN4( pData );

	mstudiomodelgroup_t *pincludemodel = (mstudiomodelgroup_t *)pData;
	phdr->numincludemodels = g_numincludemodels;
	phdr->includemodelindex = pData - pStart;
	pData += g_numincludemodels * sizeof( mstudiomodelgroup_t );

	for (i = 0; i < g_numincludemodels; i++)
	{
		AddToStringTable( pincludemodel, &pincludemodel->sznameindex, g_includemodel[i].name );
		pincludemodel++;
	}

	// save animblock group info
	mstudioanimblock_t *panimblock = (mstudioanimblock_t *)pData;
	phdr->numanimblocks = g_numanimblocks;
	phdr->animblockindex = pData - pStart;
	pData += phdr->numanimblocks * sizeof( mstudioanimblock_t );
	ALIGN4( pData );

	for (i = 1; i < g_numanimblocks; i++) 
	{
		panimblock[i].datastart = g_animblock[i].start - pBlockStart;
		panimblock[i].dataend = g_animblock[i].end - pBlockStart;
		// printf("block %d : %x %x (%d)\n", i, panimblock[i].datastart, panimblock[i].dataend, panimblock[i].dataend - panimblock[i].datastart );
	}
	AddToStringTable( phdr, &phdr->szanimblocknameindex, g_animblockname );
}

static void AssignMeshIDs( studiohdr_t *pStudioHdr )
{
	int					i;
	int					j;
	int					m;
	int					numMeshes;
	mstudiobodyparts_t	*pStudioBodyPart;
	mstudiomodel_t		*pStudioModel;
	mstudiomesh_t		*pStudioMesh;

	numMeshes = 0;
	for (i=0; i<pStudioHdr->numbodyparts; i++)
	{
		pStudioBodyPart = pStudioHdr->pBodypart(i);
		for (j=0; j<pStudioBodyPart->nummodels; j++)
		{
			pStudioModel = pStudioBodyPart->pModel(j);
			for (m=0; m<pStudioModel->nummeshes; m++)
			{				
				// get each mesh
				pStudioMesh = pStudioModel->pMesh(m);
				pStudioMesh->meshid = numMeshes + m;
			}
			numMeshes += pStudioModel->nummeshes;
		}
	}
}

	
void LoadMaterials( studiohdr_t *phdr )
{
	int					i, j;

	// get index of each material
	if( phdr->textureindex != 0 )
	{
		for( i = 0; i < phdr->numtextures; i++ )
		{
			char szPath[256];
			IMaterial *pMaterial = NULL;
			// search through all specified directories until a valid material is found
			for( j = 0; j < phdr->numcdtextures && IsErrorMaterial( pMaterial ); j++ )
			{
				strcpy( szPath, phdr->pCdtexture( j ) );
				strcat( szPath, phdr->pTexture( i )->pszName( ) );

				pMaterial = g_pMaterialSystem->FindMaterial( szPath, TEXTURE_GROUP_OTHER, false );
			}
			if( IsErrorMaterial( pMaterial ) && !g_quiet )
			{
				// hack - if it isn't found, go through the motions of looking for it again
				// so that the materialsystem will give an error.
				for( j = 0; j < phdr->numcdtextures; j++ )
				{
					strcpy( szPath, phdr->pCdtexture( j ) );
					strcat( szPath, phdr->pTexture( i )->pszName( ) );
					g_pMaterialSystem->FindMaterial( szPath, TEXTURE_GROUP_OTHER, true );
				}
			}

			phdr->pTexture( i )->material = pMaterial;

			// FIXME: hack, needs proper client side material system interface
			bool found = false;
			IMaterialVar *clientShaderVar = pMaterial->FindVar( "$clientShader", &found, false );
			if( found )
			{
				if (stricmp( clientShaderVar->GetStringValue(), "MouthShader") == 0)
				{
					phdr->pTexture( i )->flags = 1;
				}
				phdr->pTexture( i )->used = 1;
			}
		}
	}
}


void WriteKeyValues( studiohdr_t *phdr, CUtlVector< char > *pKeyValue )
{
	phdr->keyvalueindex = (pData - pStart);
	phdr->keyvaluesize = KeyValueTextSize( pKeyValue );
	if (phdr->keyvaluesize)
	{
		memcpy(	pData, KeyValueText( pKeyValue ), phdr->keyvaluesize );

		// Add space for a null terminator
		pData[phdr->keyvaluesize] = 0;
		++phdr->keyvaluesize;

		pData += phdr->keyvaluesize * sizeof( char );
	}
	ALIGN4( pData );
}


void WriteSeqKeyValues( mstudioseqdesc_t *pseqdesc, CUtlVector< char > *pKeyValue )
{
	pseqdesc->keyvalueindex = (pData - (byte *)pseqdesc);
	pseqdesc->keyvaluesize = KeyValueTextSize( pKeyValue );
	if (pseqdesc->keyvaluesize)
	{
		memcpy(	pData, KeyValueText( pKeyValue ), pseqdesc->keyvaluesize );

		// Add space for a null terminator
		pData[pseqdesc->keyvaluesize] = 0;
		++pseqdesc->keyvaluesize;

		pData += pseqdesc->keyvaluesize * sizeof( char );
	}
	ALIGN4( pData );
}


void EnsureFileDirectoryExists( const char *pFilename )
{
	char dirName[MAX_PATH];
	Q_strncpy( dirName, pFilename, sizeof( dirName ) );
	Q_FixSlashes( dirName );
	char *pLastSlash = strrchr( dirName, CORRECT_PATH_SEPARATOR );
	if ( pLastSlash )
	{
		*pLastSlash = 0;

		if ( _access( dirName, 0 ) != 0 )
		{
			char cmdLine[512];
			Q_snprintf( cmdLine, sizeof( cmdLine ), "md \"%s\"", dirName );
			system( cmdLine );
		}
	}
}


void WriteModelFiles(void)
{
	FileHandle_t modelouthandle = 0;
	FileHandle_t blockouthandle = 0;
	CPlainAutoPtr< CP4File > spFileBlockOut, spFileModelOut;
	int			total = 0;
	int			i;
	char		filename[MAX_PATH];
	studiohdr_t *phdr;
	studiohdr_t *pblockhdr = 0;

	pStart = (byte *)kalloc( 1, FILEBUFFER );

	pBlockData = NULL;
	pBlockStart = NULL;

	Q_StripExtension( outname, outname, sizeof( outname ) );
		
	if (g_animblocksize != 0)
	{
		// write the non-default g_sequence group data to separate files
		sprintf( g_animblockname, "models/%s.ani", outname );

		V_strcpy_safe( filename, gamedir );
		V_strcat_safe( filename, g_animblockname );	

		EnsureFileDirectoryExists( filename );

		if (!g_bVerifyOnly)
		{
			spFileBlockOut.Attach( g_p4factory->AccessFile( filename ) );
			spFileBlockOut->Edit();

			// Create the directory hierarchy for the ANI
			char parentdir[MAX_PATH];	 
			V_strcpy_safe( parentdir, filename );
			V_StripFilename( parentdir );
			g_pFullFileSystem->CreateDirHierarchy( parentdir );

			blockouthandle = SafeOpenWrite( filename );
		}

		pBlockStart = (byte *)kalloc( 1, FILEBUFFER );
		pBlockData = pBlockStart;

		pblockhdr = (studiohdr_t *)pBlockData;
		pblockhdr->id = IDSTUDIOANIMGROUPHEADER;
		pblockhdr->version = STUDIO_VERSION;

		pBlockData += sizeof( *pblockhdr ); 
	}

//
// write the g_model output file
//
	phdr = (studiohdr_t *)pStart;

	phdr->id = IDSTUDIOHEADER;
	phdr->version = STUDIO_VERSION;

	V_strcat_safe (outname, ".mdl");

	// strcpy( outname, ExpandPath( outname ) );

	V_strcpy_safe( filename, gamedir );
//	if( *g_pPlatformName )
//	{
//		strcat( filename, "platform_" );
//		strcat( filename, g_pPlatformName );
//		strcat( filename, "/" );
//	}
	V_strcat_safe( filename, "models/" );	
	V_strcat_safe( filename, outname );	

	
	// Create the directory.
	EnsureFileDirectoryExists( filename );


	if( !g_quiet )
	{
		printf ("---------------------\n");
		printf ("writing %s:\n", filename);
	}

	LoadPreexistingSequenceOrder( filename );

	if (!g_bVerifyOnly)
	{
		spFileModelOut.Attach( g_p4factory->AccessFile( filename ) );
		spFileModelOut->Edit();

		// Create the directory hierarchy for the MDL
		char parentdir[MAX_PATH];	 
		V_strcpy_safe( parentdir, filename );
		V_StripFilename( parentdir );
		g_pFullFileSystem->CreateDirHierarchy( parentdir );

		modelouthandle = SafeOpenWrite (filename);
	}

	phdr->eyeposition = eyeposition;
	phdr->illumposition = illumposition;

	if ( !g_wrotebbox && g_sequence.Count() > 0)
	{
		VectorCopy( g_sequence[0].bmin, bbox[0] );
		VectorCopy( g_sequence[0].bmax, bbox[1] );
		CollisionModel_ExpandBBox( bbox[0], bbox[1] );
		VectorCopy( bbox[0], g_sequence[0].bmin );
		VectorCopy( bbox[1], g_sequence[0].bmax );
	}
	if ( !g_wrotecbox )
	{
		// no default clipping box, just use per-sequence box
		VectorCopy( vec3_origin, cbox[0] );
		VectorCopy( vec3_origin, cbox[1] );
	}

	phdr->hull_min = bbox[0]; 
	phdr->hull_max = bbox[1]; 
	phdr->view_bbmin = cbox[0]; 
	phdr->view_bbmax = cbox[1]; 

	phdr->flags = gflags;
	phdr->mass = GetCollisionModelMass();	
	phdr->constdirectionallightdot = g_constdirectionalightdot;

	if ( g_numAllowedRootLODs > 0 )
	{
		phdr->numAllowedRootLODs = g_numAllowedRootLODs;
	}

	pData = (byte *)phdr + sizeof( studiohdr_t );

	// FIXME: Remove when we up the model version
	phdr->studiohdr2index = ( pData - pStart );
	studiohdr2_t* phdr2 = (studiohdr2_t*)pData;
	memset( phdr2, 0, sizeof(studiohdr2_t) );
	pData = (byte*)phdr2 + sizeof(studiohdr2_t);

	phdr2->illumpositionattachmentindex = g_illumpositionattachment;
	phdr2->flMaxEyeDeflection = g_flMaxEyeDeflection;

	BeginStringTable( );

	// Copy the full path for compatibility with older programs
	//V_strcpy_safe( phdr->name, V_UnqualifiedFileName( outname ) );
	V_strcpy_safe( phdr->name, outname );
	AddToStringTable( phdr2, &phdr2->sznameindex, outname );

	WriteBoneInfo( phdr );
	if( !g_quiet )
	{
		printf("bones      %7d bytes (%d)\n", pData - pStart - total, g_numbones );
	}
	total = pData - pStart;

	pData = WriteAnimations( pData, pStart, phdr );
	if( !g_quiet )
	{
		printf("animations %7d bytes (%d anims) (%d frames) [%d:%02d]\n", pData - pStart - total, g_numani, totalframes, (int)totalseconds / 60, (int)totalseconds % 60 );
	}
	total  = pData - pStart;

	WriteSequenceInfo( phdr );
	if( !g_quiet )
	{
		printf("sequences  %7d bytes (%d seq) \n", pData - pStart - total, g_sequence.Count() );
	}
	total  = pData - pStart;

	WriteModel( phdr );
	/*
	if( !g_quiet )
	{
		printf("models     %7d bytes\n", pData - pStart - total );
	}
	*/
	total  = pData - pStart;

	WriteTextures( phdr );
	if( !g_quiet )
	{
 		printf("textures   %7d bytes\n", pData - pStart - total );
	}
	total  = pData - pStart;

	WriteKeyValues( phdr, &g_KeyValueText );
	if( !g_quiet )
	{
		printf("keyvalues  %7d bytes\n", pData - pStart - total );
	}
	total  = pData - pStart;

	WriteBoneTransforms( phdr2, phdr->pBone( 0 ) );
	if( !g_quiet )
	{
		printf("bone transforms  %7d bytes\n", pData - pStart - total );
	}
	total  = pData - pStart;
	if ( total > FILEBUFFER )
	{
		MdlError( "file exceeds %d bytes (%d)", FILEBUFFER, total );
	}

	WriteBoneFlexDrivers( phdr2 );
	if ( !g_quiet )
	{
		printf("bone flex driver %7d bytes\n", pData - pStart - total );
	}
	total  = pData - pStart;
	if ( total > FILEBUFFER )
	{
		MdlError( "file exceeds %d bytes (%d)", FILEBUFFER, total );
	}

	pData = WriteStringTable( pData );

	total  = pData - pStart;
	if ( total > FILEBUFFER )
	{
		MdlError( "file exceeds %d bytes (%d)", FILEBUFFER, total );
	}

	phdr->checksum = 0;
	for (i = 0; i < total; i += 4)
	{
		// TODO: does this need something more than a simple shift left and add checksum?
		phdr->checksum = (phdr->checksum << 1) + ((phdr->checksum & 0x8000000) ? 1 : 0) + *((long *)(pStart + i));
	}

	if (g_bVerifyOnly)
		return;

	CollisionModel_Write( phdr->checksum );

	if( !g_quiet )
	{
		printf("collision  %7d bytes\n", pData - pStart - total );
	}

	AssignMeshIDs( phdr );

	phdr->length = pData - pStart;
	if( !g_quiet )
	{
		printf("total      %7d\n", phdr->length );
	}
	if ( phdr->length > FILEBUFFER )
	{
		MdlError( "file exceeds %d bytes (%d)", FILEBUFFER, total );
	}

	// Load materials for this model via the material system so that the
	// optimizer can ask questions about the materials.
	LoadMaterials( phdr );

	SafeWrite( modelouthandle, pStart, phdr->length );

	g_pFileSystem->Close(modelouthandle);
	if ( spFileModelOut.IsValid() ) spFileModelOut->Add();

	if (pBlockStart)
	{
		pblockhdr->length = pBlockData - pBlockStart;

		if ( g_bX360 )
		{
			// Before writing this .ani, write the byteswapped version
			void *pOutBase = kalloc(1, pblockhdr->length + BYTESWAP_ALIGNMENT_PADDING);
			int finalSize = StudioByteSwap::ByteswapANI( phdr, pOutBase, pBlockStart, pblockhdr->length );
			if ( finalSize == 0 )
			{
				MdlError("Aborted ANI byteswap on '%s':\n", g_animblockname);
			}

			char outname[ MAX_PATH ];
			Q_StripExtension( g_animblockname, outname, sizeof( outname ) );
			Q_strcat( outname, ".360.ani", sizeof( outname ) );
			
			{
				CP4AutoEditAddFile autop4( outname );
				SaveFile( outname, pOutBase, finalSize );
			}
		}

		SafeWrite( blockouthandle, pBlockStart, pblockhdr->length );
		g_pFileSystem->Close( blockouthandle );
		if ( spFileBlockOut.IsValid() ) spFileBlockOut->Add();


		if ( !g_quiet )
		{
			printf ("---------------------\n");
			printf("writing %s:\n", g_animblockname);
			printf("blocks	   %7d\n", g_numanimblocks );
			printf("total      %7d\n", pblockhdr->length );
		}
	}

	if (phdr->numbodyparts != 0)
	{
		// vertices have become an external peer data store
		// write now prior to impending vertex access from any further code
		// vertex accessors hide shifting vertex data
		WriteVertices( phdr );

	#ifdef _DEBUG
		int bodyPartID;
		for( bodyPartID = 0; bodyPartID < phdr->numbodyparts; bodyPartID++ )
		{
			mstudiobodyparts_t *pBodyPart = phdr->pBodypart( bodyPartID );
			int modelID;
			for( modelID = 0; modelID < pBodyPart->nummodels; modelID++ )
			{
				mstudiomodel_t *pModel = pBodyPart->pModel( modelID );
				const mstudio_modelvertexdata_t *vertData = pModel->GetVertexData();
				Assert( vertData ); // This can only return NULL on X360 for now
				int vertID;
				for( vertID = 0; vertID < pModel->numvertices; vertID++ )
				{
					Vector4D *pTangentS = vertData->TangentS( vertID );
					Assert( pTangentS->w == -1.0f || pTangentS->w == 1.0f );
				}
			}
		}
	#endif

		if ( !g_StudioMdlCheckUVCmd.CheckUVs( g_source, g_numsources ) )
		{
			MdlError( "UV checks failed\n" );
		}

		OptimizedModel::WriteOptimizedFiles( phdr, g_bodypart );

		// now have external finalized vtx (windings) and vvd (vertexes)
		// re-open files, sort vertexes, perform fixups, and rewrite
		// purposely isolated as a post process for stability
		if (!FixupToSortedLODVertexes( phdr ))
		{
			MdlError("Aborted vertex sort fixup on '%s':\n", filename);
		}

		if (!Clamp_RootLOD( phdr ))
		{
			MdlError("Aborted root lod shift '%s':\n", filename);
		}
	}

	if ( g_bX360 )
	{
		// now all files have been finalized and fixed up.
		// re-open the files once more and swap all little-endian 
		// data to big-endian format to produce Xbox360 files.
		WriteAllSwappedFiles( filename );
	}

	// NOTE!  If you don't want to go through the effort of loading studiorender for perf reasons,
	// make sure spewFlags ends up being zero.
	unsigned int spewFlags = SPEWPERFSTATS_SHOWSTUDIORENDERWARNINGS;

	if ( g_bPerf )
	{
		spewFlags |= SPEWPERFSTATS_SHOWPERF;
	}
	if( spewFlags )
	{
		SpewPerfStats( phdr, filename, spewFlags );
	}
}

const vertexFileHeader_t * mstudiomodel_t::CacheVertexData( void * pModelData )
{
	static vertexFileHeader_t	*pVertexHdr;
	char						filename[MAX_PATH];

	Assert( pModelData == NULL );

	if (pVertexHdr)
	{
		// studiomdl is a single model process, can simply persist data in static
		goto hasData;
	}

	// load and persist the vertex file
	V_strcpy_safe( filename, gamedir );
//	if( *g_pPlatformName )
//	{
//		strcat( filename, "platform_" );
//		strcat( filename, g_pPlatformName );
//		strcat( filename, "/" );	
//	}
	V_strcat_safe( filename, "models/" );	
	V_strcat_safe( filename, outname );
	Q_StripExtension( filename, filename, sizeof( filename ) );
	V_strcat_safe( filename, ".vvd" );

	LoadFile(filename, (void**)&pVertexHdr);

	// check id
	if (pVertexHdr->id != MODEL_VERTEX_FILE_ID)
	{
		MdlError("Error Vertex File: '%s' (id %d should be %d)\n", filename, pVertexHdr->id, MODEL_VERTEX_FILE_ID);
	}

	// check version
	if (pVertexHdr->version != MODEL_VERTEX_FILE_VERSION)
	{
		MdlError("Error Vertex File: '%s' (version %d should be %d)\n", filename, pVertexHdr->version, MODEL_VERTEX_FILE_VERSION);
	}

hasData:
	return pVertexHdr;
}

typedef struct
{
	int meshVertID;
	int	finalMeshVertID;
	int	vertexOffset;
	int	lodFlags;
} usedVertex_t;

typedef struct
{
	int	offsets[MAX_NUM_LODS];
	int	numVertexes[MAX_NUM_LODS];
} lodMeshInfo_t;

typedef struct
{
	usedVertex_t	*pVertexList;
	unsigned short	*pVertexMap;
	int				numVertexes;
	lodMeshInfo_t	lodMeshInfo;
} vertexPool_t;

#define ALIGN(b,s)		(((unsigned int)(b)+(s)-1)&~((s)-1))

//-----------------------------------------------------------------------------
// FindVertexOffsets
// 
// Iterate sorted vertex list to determine mesh starts and counts.
//-----------------------------------------------------------------------------
void FindVertexOffsets(int vertexOffset, int offsets[MAX_NUM_LODS], int counts[MAX_NUM_LODS], int numLods, const usedVertex_t *pVertexList, int numVertexes)
{
	int lodFlags;
	int	i;
	int	j;
	int	k;

	// vertexOffset uniquely identifies a single mesh's vertexes in lod vertex sorted list
	// lod vertex list is sorted from lod N..lod 0
	for (i=numLods-1; i>=0; i--)
	{
		offsets[i] = 0;
		counts[i]  = 0;

		lodFlags = (1<<(i+1))-1;
		for (j=0; j<numVertexes; j++)
		{
			// determine start of mesh at desired lod
			if (pVertexList[j].lodFlags > lodFlags)
				continue;
			if (pVertexList[j].vertexOffset != vertexOffset)
				continue;

			for (k=j; k<numVertexes; k++)
			{
				// determine end of mesh at desired lod
				if (pVertexList[k].vertexOffset != vertexOffset)
					break;
				if (!(pVertexList[k].lodFlags & (1<<i)))
					break;
			}

			offsets[i] = j;
			counts[i]  = k-j;
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// _CompareUsedVertexes
// 
// qsort callback
//-----------------------------------------------------------------------------
static int _CompareUsedVertexes(const void *a, const void *b)
{
	usedVertex_t	*pVertexA;
	usedVertex_t	*pVertexB;
	int				sort;
	int				lodA;
	int				lodB;

	pVertexA = (usedVertex_t*)a;
	pVertexB = (usedVertex_t*)b;

	// determine highest (lowest detail) lod
	// forces grouping into discrete MAX_NUM_LODS sections
	lodA = Q_log2(pVertexA->lodFlags);
	lodB = Q_log2(pVertexB->lodFlags);

	// descending sort (LodN..Lod0)
	sort = lodB-lodA;
	if (sort)
		return sort;

	// within same lod, sub sort (ascending) by mesh
	sort = pVertexA->vertexOffset - pVertexB->vertexOffset;
	if (sort)
		return sort;
	
	// within same mesh, sub sort (ascending) by vertex
	sort = pVertexA->meshVertID - pVertexB->meshVertID;
	return sort;
}

//-----------------------------------------------------------------------------
// BuildSortedVertexList
// 
// Generates the sorted vertex list. Routine is purposely serial to
// ensure vertex integrity.
//-----------------------------------------------------------------------------
bool BuildSortedVertexList(const studiohdr_t *pStudioHdr, const void *pVtxBuff, vertexPool_t **ppVertexPools, int *pNumVertexPools, usedVertex_t **ppVertexList, int *pNumVertexes)
{
	OptimizedModel::FileHeader_t		*pVtxHdr;
	OptimizedModel::BodyPartHeader_t	*pBodyPartHdr;
	OptimizedModel::ModelHeader_t		*pModelHdr;
	OptimizedModel::ModelLODHeader_t	*pModelLODHdr;
	OptimizedModel::MeshHeader_t		*pMeshHdr;
	OptimizedModel::StripGroupHeader_t	*pStripGroupHdr;
	OptimizedModel::Vertex_t			*pStripVertex;
	mstudiobodyparts_t					*pStudioBodyPart;
	mstudiomodel_t						*pStudioModel;
	mstudiomesh_t						*pStudioMesh;
	usedVertex_t						*usedVertexes;
	vertexPool_t						*pVertexPools;
	vertexPool_t						*pPool;
	usedVertex_t						*pVertexList;
	int									*pVertexes;
	unsigned short						*pVertexMap;
	int									index;
	int									currLod;
	int									vertexOffset;
	int									i,j,k,m,n,p;
	int									poolStart;
	int									numVertexPools;
	int									numVertexes;
	int									numMeshVertexes;
	int									offsets[MAX_NUM_LODS];
	int									counts[MAX_NUM_LODS];
	int									finalMeshVertID;
	int									baseMeshVertID;

	*ppVertexPools   = NULL;
	*pNumVertexPools = 0;
	*ppVertexList    = NULL;
	*pNumVertexes    = 0;

	pVtxHdr = (OptimizedModel::FileHeader_t*)pVtxBuff; 

	// determine number of vertex pools
	if (pStudioHdr->numbodyparts != pVtxHdr->numBodyParts)
		return false;
	numVertexPools = 0;
	for (i=0; i<pVtxHdr->numBodyParts; i++)
	{
		pBodyPartHdr    = pVtxHdr->pBodyPart(i);
		pStudioBodyPart = pStudioHdr->pBodypart(i);
		if (pStudioBodyPart->nummodels != pBodyPartHdr->numModels)
			return false;

		// the model's subordinate lods only reference from a single top level pool 
		// no new verts are created for sub lods
		// each model's subordinate mesh dictates its own vertex pool
		for (j=0; j<pBodyPartHdr->numModels; j++)
		{
			pStudioModel    = pStudioBodyPart->pModel(j);
			numVertexPools += pStudioModel->nummeshes;
		}
	}

	// allocate pools
	pVertexPools = (vertexPool_t*)malloc(numVertexPools*sizeof(vertexPool_t));
	memset(pVertexPools, 0, numVertexPools*sizeof(vertexPool_t));

	// iterate lods, mark referenced indexes
	numVertexPools = 0;
	for (i=0; i<pVtxHdr->numBodyParts; i++)
	{
		pBodyPartHdr    = pVtxHdr->pBodyPart(i);
		pStudioBodyPart = pStudioHdr->pBodypart(i);

		for (j=0; j<pBodyPartHdr->numModels; j++)
		{
			pModelHdr    = pBodyPartHdr->pModel(j);
			pStudioModel = pStudioBodyPart->pModel(j);

			// allocate each mesh's vertex list
			poolStart = numVertexPools;
			for (k=0; k<pStudioModel->nummeshes; k++)
			{
				// track the expected relative offset into a flattened vertex list
				vertexOffset = 0;
				for (m=0; m<poolStart+k; m++)
					vertexOffset += pVertexPools[m].numVertexes;

				pStudioMesh = pStudioModel->pMesh(k);
				numMeshVertexes = pStudioMesh->numvertices;
				if (numMeshVertexes)
				{
					usedVertexes = (usedVertex_t*)malloc(numMeshVertexes*sizeof(usedVertex_t));
					pVertexMap   = (unsigned short*)malloc(numMeshVertexes*sizeof(unsigned short));

					for (n=0; n<numMeshVertexes; n++)
					{
						// setup mapping
						// due to the hierarchial layout, the vertID's map per mesh's pool
						// a linear layout of the vertexes requires a unique signature to achieve a remap
						// the offset and index form a unique signature
						usedVertexes[n].meshVertID      = n;
						usedVertexes[n].finalMeshVertID = -1;
						usedVertexes[n].vertexOffset    = vertexOffset;
						usedVertexes[n].lodFlags        = 0;
						pVertexMap[n]                   = n;
					}
				
					pVertexPools[numVertexPools].pVertexList = usedVertexes;
					pVertexPools[numVertexPools].pVertexMap  = pVertexMap;
				}
				pVertexPools[numVertexPools].numVertexes = numMeshVertexes;
				numVertexPools++;
			}

			// iterate all lods
			for (currLod=0; currLod<pVtxHdr->numLODs; currLod++)
			{
				pModelLODHdr = pModelHdr->pLOD(currLod);

				if (pModelLODHdr->numMeshes != pStudioModel->nummeshes)
					return false;

				for (k=0; k<pModelLODHdr->numMeshes; k++)
				{
					pMeshHdr      = pModelLODHdr->pMesh(k);
					pStudioMesh   = pStudioModel->pMesh(k);
					for (m=0; m<pMeshHdr->numStripGroups; m++)
					{
						pStripGroupHdr = pMeshHdr->pStripGroup(m);
						
						// sanity check the indexes have 100% coverage of the vertexes
						pVertexes = (int*)malloc(pStripGroupHdr->numVerts*sizeof(int));
						memset(pVertexes, 0xFF, pStripGroupHdr->numVerts*sizeof(int));

						for (n=0; n<pStripGroupHdr->numIndices; n++)
						{
							index = *pStripGroupHdr->pIndex(n);
							if (index < 0 || index >= pStripGroupHdr->numVerts)
								return false;
							pVertexes[index] = index;
						}

						// sanity check for coverage
						for (n=0; n<pStripGroupHdr->numVerts; n++)
						{
							if (pVertexes[n] != n)
								return false;
						}

						free(pVertexes);

						// iterate vertexes
						pPool = &pVertexPools[poolStart + k];
						for (n=0; n<pStripGroupHdr->numVerts; n++)
						{
							pStripVertex = pStripGroupHdr->pVertex(n);
							if (pStripVertex->origMeshVertID < 0 || pStripVertex->origMeshVertID >= pPool->numVertexes)
								return false;

							// arrange binary flags for numerical sorting
							// the lowest detail lod's verts at the top, the root lod's verts at the bottom
							pPool->pVertexList[pStripVertex->origMeshVertID].lodFlags |= 1<<currLod;
						}
					}
				}
			}
		}
	}

	// flatten the vertex pool hierarchy into a linear sequence
	numVertexes = 0;
	for (i=0; i<numVertexPools; i++)
		numVertexes += pVertexPools[i].numVertexes;
	pVertexList = (usedVertex_t*)malloc(numVertexes*sizeof(usedVertex_t));
	numVertexes  = 0;
	for (i=0; i<numVertexPools; i++)
	{
		pPool = &pVertexPools[i];
		for (j=0; j<pPool->numVertexes; j++)
		{
			if (!pPool->pVertexList[j].lodFlags)
			{
				// found an orphaned vertex that is unreferenced at any lod strip winding
				// don't know how these occur or who references them
				// cannot cull the orphaned vertexes, otherwise vertex counts are wrong
				// every vertex must be remapped
				// force the vertex to belong to the lowest lod 
				// lod flags must be nonzero for proper sorted runs
				pPool->pVertexList[j].lodFlags = 1<<(pVtxHdr->numLODs-1);
			}
		}

		memcpy(&pVertexList[numVertexes], pPool->pVertexList, pPool->numVertexes*sizeof(usedVertex_t));
		numVertexes += pPool->numVertexes;
	}

	// sort the vertexes based on lod flags
	// the sort dictates the linear sequencing of the .vvd data file
	// the vtx file indexes get remapped to the new sort order
	qsort(pVertexList, numVertexes, sizeof(usedVertex_t), _CompareUsedVertexes);
	
	// build a mapping table from mesh relative indexes to the flat lod sorted array
	vertexOffset = 0;
	for (i=0; i<numVertexPools; i++)
	{
		pPool = &pVertexPools[i];
		for (j=0; j<pPool->numVertexes; j++)
		{
			// scan flattened sorted vertexes
			for (k=0; k<numVertexes; k++)
			{
				if (pVertexList[k].vertexOffset == vertexOffset && pVertexList[k].meshVertID == j)
					break;
			}
			pPool->pVertexMap[j] = k;
		}
		vertexOffset += pPool->numVertexes;
	}

	// build offsets and counts that identifies mesh's distribution across lods
	// calc final fixed vertex location if vertexes were gathered to mesh order from lod sorted list
	finalMeshVertID = 0;
	poolStart = 0; 
	for (i=0; i<pStudioHdr->numbodyparts; i++)
	{
		pStudioBodyPart = pStudioHdr->pBodypart(i);
		for (j=0; j<pStudioBodyPart->nummodels; j++)
		{
			pStudioModel = pStudioBodyPart->pModel(j);
			for (m=0; m<pStudioModel->nummeshes; m++)
			{
				// track the expected offset into linear vertexes
				vertexOffset = 0;
				for (n=0; n<poolStart+m; n++)
					vertexOffset += pVertexPools[n].numVertexes;

				// vertexOffset works as unique key to identify vertexes for a specific mesh
				// a mesh's verts are distributed, but guaranteed sequential in the lod sorted vertex list
				// determine base index and offset and run length for target mesh for all lod levels
				FindVertexOffsets(vertexOffset, offsets, counts, pVtxHdr->numLODs, pVertexList, numVertexes);

				for (n=0; n<pVtxHdr->numLODs; n++)
				{
					if (!counts[n])
						offsets[n] = 0;

					pVertexPools[poolStart+m].lodMeshInfo.offsets[n]     = offsets[n];
					pVertexPools[poolStart+m].lodMeshInfo.numVertexes[n] = counts[n];
				}

				// iterate using calced offsets to walk each mesh
				// set its expected final vertex id, which is its "gathered" index relative to mesh
				baseMeshVertID = finalMeshVertID;
				for (n=pVtxHdr->numLODs-1; n>=0; n--)
				{
					// iterate each vert in the mesh
					// vertex id is relative to
					for (p=0; p<counts[n]; p++)
					{
						pVertexList[offsets[n] + p].finalMeshVertID = finalMeshVertID - baseMeshVertID;
						finalMeshVertID++;
					}
				}
			}
			poolStart += pStudioModel->nummeshes;
		}
	}

	// safety check
	// every referenced vertex should have been remapped correctly
	// some models do have orphaned vertexes, ignore these
	for (i=0; i<numVertexes; i++)
	{
		if (pVertexList[i].lodFlags && pVertexList[i].finalMeshVertID == -1)
		{
			// should never happen, data occured in unknown manner
			// don't build corrupted data
			return false;
		}
	}

	// provide generated tables
	*ppVertexPools   = pVertexPools;
	*pNumVertexPools = numVertexPools;
	*ppVertexList    = pVertexList;
	*pNumVertexes    = numVertexes;

	// success
	return true;
}

//-----------------------------------------------------------------------------
// FixupVVDFile
// 
// VVD files get vertexes remapped to a flat lod sorted order.
//-----------------------------------------------------------------------------
bool FixupVVDFile(const char *fileName,  const studiohdr_t *pStudioHdr, const void *pVtxBuff, const vertexPool_t *pVertexPools, int numVertexPools, const usedVertex_t *pVertexList, int numVertexes)
{	
	OptimizedModel::FileHeader_t	*pVtxHdr;
	vertexFileHeader_t				*pFileHdr_old;
	vertexFileHeader_t				*pFileHdr_new;
	mstudiobodyparts_t				*pStudioBodyPart;
	mstudiomodel_t					*pStudioModel;
	mstudiomesh_t					*pStudioMesh;
	mstudiovertex_t					*pVertex_old;
	mstudiovertex_t					*pVertex_new;
	Vector4D						*pTangent_new;
	Vector4D						*pTangent_old;
	mstudiovertex_t					**pFlatVertexes;
	Vector4D						**pFlatTangents;
	vertexFileFixup_t				*pFixupTable;
	const lodMeshInfo_t				*pLodMeshInfo;
	byte							*pStart_new;
	byte							*pData_new;
	byte							*pStart_base;
	byte							*pVertexBase_old;
	byte							*pTangentBase_old;
	void							*pVvdBuff;
	int								i;
	int								j;
	int								k;
	int								n;
	int								p;
	int								numFixups;
	int								numFlat;
	int								oldIndex;
	int								mask;
	int								maxCount;
	int								numMeshes;
	int								numOutFixups;

	pVtxHdr = (OptimizedModel::FileHeader_t*)pVtxBuff; 

	LoadFile((char*)fileName, &pVvdBuff);

	pFileHdr_old = (vertexFileHeader_t*)pVvdBuff;
	if (pFileHdr_old->numLODs != 1)
	{
		// file has wrong expected state
		return false;
	}

	// meshes need relocation fixup from lod order back to mesh order
	numFixups = 0;
	numMeshes = 0;
	for (i=0; i<pStudioHdr->numbodyparts; i++)
	{
		pStudioBodyPart = pStudioHdr->pBodypart(i);
		for (j=0; j<pStudioBodyPart->nummodels; j++)
		{
			pStudioModel = pStudioBodyPart->pModel(j);
			for (k=0; k<pStudioModel->nummeshes; k++)
			{
				pStudioMesh = pStudioModel->pMesh(k);
				if (!pStudioMesh->numvertices)
				{
					// no vertexes for this mesh, skip it
					continue;
				}
				for (n=pVtxHdr->numLODs-1; n>=0; n--)
				{
					pLodMeshInfo = &pVertexPools[numMeshes+k].lodMeshInfo;
					if (!pLodMeshInfo->numVertexes[n])
					{
						// no vertexes for this portion of the mesh at this lod, skip it
						continue;
					}
					numFixups++;
				}
			}
			numMeshes += k;
		}
	}
	if (numMeshes == 1 || numFixups == 1 || pVtxHdr->numLODs == 1)
	{
		// no fixup required for a single mesh
		// no fixup required for single lod
		// no fixup required when mesh data is contiguous as expected
		numFixups = 0;
	}

	pStart_base = (byte*)malloc(FILEBUFFER);
	memset(pStart_base, 0, FILEBUFFER);
	pStart_new  = (byte*)ALIGN(pStart_base,16);
	pData_new   = pStart_new;

	// setup headers
	pFileHdr_new = (vertexFileHeader_t*)pData_new;
	pData_new += sizeof(vertexFileHeader_t);

	// clone and fixup new header
	*pFileHdr_new = *pFileHdr_old;
	pFileHdr_new->numLODs   = pVtxHdr->numLODs;
	pFileHdr_new->numFixups = numFixups;

	// skip new fixup table
	pData_new   = (byte*)ALIGN(pData_new, 4);
	pFixupTable = (vertexFileFixup_t*)pData_new;
	pFileHdr_new->fixupTableStart = pData_new - pStart_new;
	pData_new += numFixups*sizeof(vertexFileFixup_t);

	// skip new vertex data
	pData_new    = (byte*)ALIGN(pData_new, 16);
	pVertex_new  = (mstudiovertex_t*)pData_new;
	pFileHdr_new->vertexDataStart = pData_new - pStart_new;
	pData_new += numVertexes*sizeof(mstudiovertex_t);

	// skip new tangent data
	pData_new    = (byte*)ALIGN(pData_new, 16);
	pTangent_new = (Vector4D*)pData_new;
	pFileHdr_new->tangentDataStart = pData_new - pStart_new;
	pData_new += numVertexes*sizeof(Vector4D);

	pVertexBase_old  = (byte*)pFileHdr_old + pFileHdr_old->vertexDataStart;
	pTangentBase_old = (byte*)pFileHdr_old + pFileHdr_old->tangentDataStart;

	// determine number of aggregate verts towards root lod
	// loader can truncate read according to desired root lod
	maxCount = -1;
	for (n=pVtxHdr->numLODs-1; n>=0; n--)
	{
		mask = 1<<n;
		for (p=0; p<numVertexes; p++)
		{
			if (mask & pVertexList[p].lodFlags)
			{
				if (maxCount < p)
					maxCount = p;
			}
		}
		pFileHdr_new->numLODVertexes[n] = maxCount+1;
	}
	for (n=pVtxHdr->numLODs; n<MAX_NUM_LODS; n++)
	{
		// ripple the last valid lod entry all the way down
		pFileHdr_new->numLODVertexes[n] = pFileHdr_new->numLODVertexes[pVtxHdr->numLODs-1];
	}
	
	// build mesh relocation fixup table
	if (numFixups)
	{
		numMeshes    = 0;
		numOutFixups = 0;
		for (i=0; i<pStudioHdr->numbodyparts; i++)
		{
			pStudioBodyPart = pStudioHdr->pBodypart(i);
			for (j=0; j<pStudioBodyPart->nummodels; j++)
			{
				pStudioModel = pStudioBodyPart->pModel(j);
				for (k=0; k<pStudioModel->nummeshes; k++)
				{
					pStudioMesh = pStudioModel->pMesh(k);
					if (!pStudioMesh->numvertices)
					{
						// not vertexes for this mesh, skip it
						continue;
					}
					for (n=pVtxHdr->numLODs-1; n>=0; n--)
					{
						pLodMeshInfo = &pVertexPools[numMeshes+k].lodMeshInfo;
						if (!pLodMeshInfo->numVertexes[n])
						{
							// no vertexes for this portion of the mesh at this lod, skip it
							continue;
						}
						pFixupTable[numOutFixups].lod            = n;
						pFixupTable[numOutFixups].numVertexes    = pLodMeshInfo->numVertexes[n];
						pFixupTable[numOutFixups].sourceVertexID = pLodMeshInfo->offsets[n];
						numOutFixups++;
					}
				}
				numMeshes += pStudioModel->nummeshes;
			}
		}

		if (numOutFixups != numFixups)
		{
			// logic sync error, final calc should match precalc, otherwise memory corruption
			return false;
		}
	}

	// generate offsets to vertexes
	numFlat = 0;
	pFlatVertexes = (mstudiovertex_t**)malloc(numVertexes*sizeof(mstudiovertex_t*));
	pFlatTangents = (Vector4D**)malloc(numVertexes*sizeof(Vector4D*));
	for (i=0; i<pStudioHdr->numbodyparts; i++)
	{
		pStudioBodyPart = pStudioHdr->pBodypart(i);
		for (j=0; j<pStudioBodyPart->nummodels; j++)
		{
			pStudioModel = pStudioBodyPart->pModel(j);
			pVertex_old  = (mstudiovertex_t*)&pVertexBase_old[pStudioModel->vertexindex];
			pTangent_old = (Vector4D*)&pTangentBase_old[pStudioModel->tangentsindex];
			for (k=0; k<pStudioModel->nummeshes; k++)
			{
				// get each mesh's vertexes
				pStudioMesh = pStudioModel->pMesh(k);
				for (n=0; n<pStudioMesh->numvertices; n++)
				{
					// old vertex pools are per model, seperated per mesh by a start offset
					// vertexes are then isolated subpools per mesh
					// build the flat linear array of lookup pointers
					pFlatVertexes[numFlat] = &pVertex_old[pStudioMesh->vertexoffset + n];
					pFlatTangents[numFlat] = &pTangent_old[pStudioMesh->vertexoffset + n];
					numFlat++;
				}
			}
		}
	}

	// write in lod sorted order
	for (i=0; i<numVertexes; i++)
	{
		// iterate sorted order, remap old vert location to new vert location
		oldIndex = pVertexList[i].vertexOffset + pVertexList[i].meshVertID;
		
		memcpy(&pVertex_new[i], pFlatVertexes[oldIndex], sizeof(mstudiovertex_t));
		memcpy(&pTangent_new[i], pFlatTangents[oldIndex], sizeof(Vector4D));
	}

	// pFileHdr_new->length =  pData_new-pStart_new;
	{
		CP4AutoEditAddFile autop4( fileName, "binary" );
		SaveFile((char*)fileName, pStart_new, pData_new-pStart_new);
	}

	free(pStart_base);
	free(pFlatVertexes);
	free(pFlatTangents);

	// success
	return true;
}

//-----------------------------------------------------------------------------
// FixupVTXFile
// 
// VTX files get their windings remapped.
//-----------------------------------------------------------------------------
bool FixupVTXFile(const char *fileName, const studiohdr_t *pStudioHdr, const vertexPool_t *pVertexPools, int numVertexPools, const usedVertex_t *pVertexList, int numVertexes)
{
	OptimizedModel::FileHeader_t		*pVtxHdr;
	OptimizedModel::BodyPartHeader_t	*pBodyPartHdr;
	OptimizedModel::ModelHeader_t		*pModelHdr;
	OptimizedModel::ModelLODHeader_t	*pModelLODHdr;
	OptimizedModel::MeshHeader_t		*pMeshHdr;
	OptimizedModel::StripGroupHeader_t	*pStripGroupHdr;
	OptimizedModel::Vertex_t			*pStripVertex;
	int									currLod;
	int									vertexOffset;
	mstudiobodyparts_t					*pStudioBodyPart;
	mstudiomodel_t						*pStudioModel;
	int									i,j,k,m,n;
	int									poolStart;
	int									VtxLen;
	int									newMeshVertID;
	void								*pVtxBuff;

	VtxLen  = LoadFile((char*)fileName, &pVtxBuff);
	pVtxHdr = (OptimizedModel::FileHeader_t*)pVtxBuff; 

	// iterate all lod's windings
	poolStart = 0;
	for (i=0; i<pVtxHdr->numBodyParts; i++)
	{
		pBodyPartHdr    = pVtxHdr->pBodyPart(i);
		pStudioBodyPart = pStudioHdr->pBodypart(i);

		for (j=0; j<pBodyPartHdr->numModels; j++)
		{
			pModelHdr    = pBodyPartHdr->pModel(j);
			pStudioModel = pStudioBodyPart->pModel(j);

			// iterate all lods
			for (currLod=0; currLod<pVtxHdr->numLODs; currLod++)
			{
				pModelLODHdr = pModelHdr->pLOD(currLod);

				if (pModelLODHdr->numMeshes != pStudioModel->nummeshes)
					return false;

				for (k=0; k<pModelLODHdr->numMeshes; k++)
				{
					// track the expected relative offset into the flat vertexes
					vertexOffset = 0;
					for (m=0; m<poolStart+k; m++)
						vertexOffset += pVertexPools[m].numVertexes;

					pMeshHdr = pModelLODHdr->pMesh(k);
					for (m=0; m<pMeshHdr->numStripGroups; m++)
					{
						pStripGroupHdr = pMeshHdr->pStripGroup(m);
						
						for (n=0; n<pStripGroupHdr->numVerts; n++)
						{
							pStripVertex = pStripGroupHdr->pVertex(n);

							// remap old mesh relative vertex index to absolute flat sorted list
							newMeshVertID = pVertexPools[poolStart+k].pVertexMap[pStripVertex->origMeshVertID];

							// map to expected final fixed vertex locations
							// final fixed vertex location is performed by runtime loading code
							newMeshVertID = pVertexList[newMeshVertID].finalMeshVertID;

							// fixup to expected 
							pStripVertex->origMeshVertID = newMeshVertID;
						}
					}
				}
			}
			poolStart += pStudioModel->nummeshes;
		}
	}

	// pVtxHdr->length = VtxLen;
	{
		CP4AutoEditAddFile autop4( fileName, "binary" );
		SaveFile((char*)fileName, pVtxBuff, VtxLen);
	}

	free(pVtxBuff);

	return true;
}

//-----------------------------------------------------------------------------
// FixupMDLFile
// 
// MDL files get flexes/vertex/tangent data offsets fixed
//-----------------------------------------------------------------------------
bool FixupMDLFile(const char *fileName, studiohdr_t *pStudioHdr, const void *pVtxBuff, const vertexPool_t *pVertexPools, int numVertexPools, const usedVertex_t *pVertexList, int numVertexes)
{
	OptimizedModel::FileHeader_t	*pVtxHdr;
	const lodMeshInfo_t				*pLodMeshInfo;
	mstudiobodyparts_t				*pStudioBodyPart;
	mstudiomodel_t					*pStudioModel;
	mstudiomesh_t					*pStudioMesh;
	mstudioflex_t					*pStudioFlex;
	mstudiovertanim_t				*pStudioVertAnim;
	int								newMeshVertID;
	int								i;
	int								j;
	int								m;
	int								n;
	int								p;
	int								numLODs;
	int								numMeshes;
	int								total;

	pVtxHdr = (OptimizedModel::FileHeader_t*)pVtxBuff;

	numLODs = pVtxHdr->numLODs; 

	numMeshes = 0;
	for (i=0; i<pStudioHdr->numbodyparts; i++)
	{
		pStudioBodyPart = pStudioHdr->pBodypart(i);

		for (j=0; j<pStudioBodyPart->nummodels; j++)
		{
			pStudioModel = pStudioBodyPart->pModel(j);

			for (m=0; m<pStudioModel->nummeshes; m++)
			{				
				// get each mesh
				pStudioMesh  = pStudioModel->pMesh(m);
				pLodMeshInfo = &pVertexPools[numMeshes+m].lodMeshInfo;

				for (n=0; n<numLODs; n++)
				{
					// the root lod, contains all the lower detail lods verts
					// tally the verts that are at each lod
					total = 0;
					for (p=n; p<numLODs; p++)
						total += pLodMeshInfo->numVertexes[p];

					// embed the fixup for loader
					pStudioMesh->vertexdata.numLODVertexes[n] = total;
				}
				for (p=n; p<MAX_NUM_LODS; p++)
				{
					// duplicate last valid lod to end of list
					pStudioMesh->vertexdata.numLODVertexes[p] = pStudioMesh->vertexdata.numLODVertexes[numLODs-1];
				}

				// fix the flexes
				for (n=0; n<pStudioMesh->numflexes; n++)
				{
					pStudioFlex = pStudioMesh->pFlex(n);

					byte *pvanim = pStudioFlex->pBaseVertanim();
					int nVAnimSizeBytes = pStudioFlex->VertAnimSizeBytes();

					for (p=0; p<pStudioFlex->numverts; p++, pvanim += nVAnimSizeBytes )
					{
						pStudioVertAnim = (mstudiovertanim_t*)( pvanim );

						if (pStudioVertAnim->index < 0 || pStudioVertAnim->index >= pStudioMesh->numvertices)
							return false;

						// remap old mesh relative vertex index to absolute flat sorted list
						newMeshVertID = pVertexPools[numMeshes+m].pVertexMap[pStudioVertAnim->index];

						// map to expected final fixed vertex locations
						// final fixed vertex location is performed by runtime loading code
						newMeshVertID = pVertexList[newMeshVertID].finalMeshVertID;

						// fixup to expected 
						pStudioVertAnim->index = newMeshVertID;
					}
				}
			}
			numMeshes += pStudioModel->nummeshes;
		}
	}

	{
		CP4AutoEditAddFile autop4( fileName, "binary" );
		SaveFile((char*)fileName, (void*)pStudioHdr, pStudioHdr->length);
	}

	// success
	return true;
}

//-----------------------------------------------------------------------------
// FixupToSortedLODVertexes
// 
// VVD files get vertexes fixed to a flat sorted order, ascending in lower detail lod usage
// VTX files get their windings remapped to the sort.
//-----------------------------------------------------------------------------
bool FixupToSortedLODVertexes(studiohdr_t *pStudioHdr)
{
	char							filename[MAX_PATH];
	char							tmpFileName[MAX_PATH];
	void							*pVtxBuff;
	usedVertex_t					*pVertexList;
	vertexPool_t					*pVertexPools;
	int								numVertexes;
	int								numVertexPools;
	int								VtxLen;
	int								i;
	const char						*vtxPrefixes[] = {".dx80.vtx", ".dx90.vtx", ".sw.vtx"};

	V_strcpy_safe( filename, gamedir );
//	if( *g_pPlatformName )
//	{
//		strcat( filename, "platform_" );
//		strcat( filename, g_pPlatformName );
//		strcat( filename, "/" );	
//	}
	V_strcat_safe( filename, "models/" );	
	V_strcat_safe( filename, outname );
	Q_StripExtension( filename, filename, sizeof( filename ) );

	// determine lod usage per vertex
	// all vtx files enumerate model's lod verts, but differ in their mesh makeup
	// use xxx.dx80.vtx to establish which vertexes are used by each lod
	V_strcpy_safe( tmpFileName, filename );
	V_strcat_safe( tmpFileName, ".dx80.vtx" );
	VtxLen = LoadFile( tmpFileName, &pVtxBuff );

	// build the sorted vertex tables
	if (!BuildSortedVertexList(pStudioHdr, pVtxBuff, &pVertexPools, &numVertexPools, &pVertexList, &numVertexes))
	{
		// data sync error
		return false;
	}

	// fixup ???.vvd
	V_strcpy_safe( tmpFileName, filename );
	V_strcat_safe( tmpFileName, ".vvd" );
	if (!FixupVVDFile(tmpFileName, pStudioHdr, pVtxBuff, pVertexPools, numVertexPools, pVertexList, numVertexes))
	{
		// data error
		return false;
	}

	for (i=0; i<ARRAYSIZE(vtxPrefixes); i++)
	{
		// fixup ???.vtx
		V_strcpy_safe( tmpFileName, filename );
		V_strcat_safe( tmpFileName, vtxPrefixes[i] );
		if (!FixupVTXFile(tmpFileName, pStudioHdr, pVertexPools, numVertexPools, pVertexList, numVertexes))
		{
			// data error
			return false;
		}
	}

	// fixup ???.mdl
	V_strcpy_safe( tmpFileName, filename );
	V_strcat_safe( tmpFileName, ".mdl" );
	if (!FixupMDLFile(tmpFileName, pStudioHdr, pVtxBuff, pVertexPools, numVertexPools, pVertexList, numVertexes))
	{
		// data error
		return false;
	}

	// free the tables
	for (i=0; i<numVertexPools; i++)
	{
		if (pVertexPools[i].pVertexList)
			free(pVertexPools[i].pVertexList);
		if (pVertexPools[i].pVertexMap)
			free(pVertexPools[i].pVertexMap);
	}
	if (numVertexPools)
		free(pVertexPools);
	free(pVtxBuff);

	// success
	return true;
}


byte IsByte( int val )
{
	if (val < 0 || val > 0xFF)
	{
		MdlError("byte conversion out of range %d\n", val );
	}
	return val;
}

char IsChar( int val )
{
	if (val < -0x80 || val > 0x7F)
	{
		MdlError("char conversion out of range %d\n", val );
	}
	return val;
}

int IsInt24( int val )
{
	if (val < -0x800000 || val > 0x7FFFFF)
	{
		MdlError("int24 conversion out of range %d\n", val );
	}
	return val;
}


short IsShort( int val )
{
	if (val < -0x8000 || val > 0x7FFF)
	{
		MdlError("short conversion out of range %d\n", val );
	}
	return val;
}

unsigned short IsUShort( int val )
{
	if (val < 0 || val > 0xFFFF)
	{
		MdlError("ushort conversion out of range %d\n", val );
	}
	return val;
}


bool Clamp_MDL_LODS( const char *fileName, int rootLOD )
{
	studiohdr_t *pStudioHdr;
	int			len;

	len  = LoadFile((char*)fileName, (void **)&pStudioHdr);

	Studio_SetRootLOD( pStudioHdr, rootLOD );

#if 0
	// shift down bone LOD masks
	int iBone;
	for ( iBone = 0; iBone < pStudioHdr->numbones; iBone++)
	{
		mstudiobone_t *pBone = pStudioHdr->pBone( iBone );

		int nLodID;
		for ( nLodID = 0; nLodID < rootLOD; nLodID++)
		{
			int iLodMask = BONE_USED_BY_VERTEX_LOD0 << nLodID;

			if (pBone->flags & (BONE_USED_BY_VERTEX_LOD0 << rootLOD))
			{
				pBone->flags = pBone->flags | iLodMask;
			}
			else
			{
				pBone->flags = pBone->flags & (~iLodMask);
			}
		}
	}
#endif

	{
		CP4AutoEditAddFile autop4( fileName, "binary" );
		SaveFile( (char *)fileName, pStudioHdr, len );
	}

	return true;
}




bool Clamp_VVD_LODS( const char *fileName, int rootLOD )
{
	vertexFileHeader_t *pTempVvdHdr;
	int			len;

	len  = LoadFile((char*)fileName, (void **)&pTempVvdHdr);

	int newLength = Studio_VertexDataSize( pTempVvdHdr, rootLOD, true );

	// printf("was %d now %d\n", len, newLength );

	vertexFileHeader_t *pNewVvdHdr = (vertexFileHeader_t *)calloc( newLength, 1 );

	Studio_LoadVertexes( pTempVvdHdr, pNewVvdHdr, rootLOD, true );

	if (!g_quiet)
	{
		printf ("---------------------\n");
		printf ("writing %s:\n", fileName);
		printf( "vertices   (%d vertices)\n", pNewVvdHdr->numLODVertexes[ 0 ] );
	}

	// pNewVvdHdr->length = newLength;

	{
		CP4AutoEditAddFile autop4( fileName, "binary" );
		SaveFile( (char *)fileName, pNewVvdHdr, newLength );
	}

	return true;
}


bool Clamp_VTX_LODS( const char *fileName, int rootLOD, studiohdr_t *pStudioHdr )
{
	int i, j, k, m, n;
	int nLodID;
	int size;

	OptimizedModel::FileHeader_t *pVtxHdr;
	int			len;

	len  = LoadFile((char*)fileName, (void **)&pVtxHdr);

	OptimizedModel::FileHeader_t *pNewVtxHdr = (OptimizedModel::FileHeader_t *)calloc( FILEBUFFER, 1 );

	byte *pData = (byte *)pNewVtxHdr;
	pData += sizeof( OptimizedModel::FileHeader_t );
	ALIGN4( pData );

	// header
	pNewVtxHdr->version = pVtxHdr->version;
	pNewVtxHdr->vertCacheSize = pVtxHdr->vertCacheSize;
	pNewVtxHdr->maxBonesPerStrip = pVtxHdr->maxBonesPerStrip;
	pNewVtxHdr->maxBonesPerTri = pVtxHdr->maxBonesPerTri;
	pNewVtxHdr->maxBonesPerVert = pVtxHdr->maxBonesPerVert;
	pNewVtxHdr->checkSum = pVtxHdr->checkSum;
	pNewVtxHdr->numLODs = pVtxHdr->numLODs;

	// material replacement list
	pNewVtxHdr->materialReplacementListOffset = (pData - (byte *)pNewVtxHdr);
	pData += pVtxHdr->numLODs * sizeof( OptimizedModel::MaterialReplacementListHeader_t );
	// ALIGN4( pData );

	BeginStringTable( );

	// allocate replacement list arrays
	for ( nLodID = rootLOD; nLodID < pVtxHdr->numLODs; nLodID++ )
	{
		OptimizedModel::MaterialReplacementListHeader_t *pReplacementList = pVtxHdr->pMaterialReplacementList( nLodID );
		OptimizedModel::MaterialReplacementListHeader_t *pNewReplacementList = pNewVtxHdr->pMaterialReplacementList( nLodID );

		pNewReplacementList->numReplacements = pReplacementList->numReplacements;
		pNewReplacementList->replacementOffset = (pData - (byte *)pNewReplacementList);
		pData += pNewReplacementList->numReplacements * sizeof( OptimizedModel::MaterialReplacementHeader_t );
		// ALIGN4( pData );

		for (i = 0; i < pReplacementList->numReplacements; i++)
		{
			OptimizedModel::MaterialReplacementHeader_t *pReplacement = pReplacementList->pMaterialReplacement( i );
			OptimizedModel::MaterialReplacementHeader_t *pNewReplacement = pNewReplacementList->pMaterialReplacement( i );

			pNewReplacement->materialID = pReplacement->materialID;
			AddToStringTable( pNewReplacement, &pNewReplacement->replacementMaterialNameOffset, pReplacement->pMaterialReplacementName() );
		}
	}
	pData = WriteStringTable( pData );

	// link previous LODs to higher LODs
	for ( nLodID = 0; nLodID < rootLOD; nLodID++ )
	{
		OptimizedModel::MaterialReplacementListHeader_t *pRootReplacementList = pNewVtxHdr->pMaterialReplacementList( rootLOD );
		OptimizedModel::MaterialReplacementListHeader_t *pNewReplacementList = pNewVtxHdr->pMaterialReplacementList( nLodID );

		int delta = (byte *)pRootReplacementList - (byte *)pNewReplacementList;

		pNewReplacementList->numReplacements = pRootReplacementList->numReplacements;
		pNewReplacementList->replacementOffset = pRootReplacementList->replacementOffset + delta;
	}

	// body parts
	pNewVtxHdr->numBodyParts = pStudioHdr->numbodyparts;
	pNewVtxHdr->bodyPartOffset = (pData - (byte *)pNewVtxHdr);
	pData += pNewVtxHdr->numBodyParts * sizeof( OptimizedModel::BodyPartHeader_t );
	// ALIGN4( pData );

	// Iterate over every body part...
	for ( i = 0; i < pStudioHdr->numbodyparts; i++ )
	{
		mstudiobodyparts_t* pBodyPart = pStudioHdr->pBodypart(i);
		OptimizedModel::BodyPartHeader_t* pVtxBodyPart = pVtxHdr->pBodyPart(i);
		OptimizedModel::BodyPartHeader_t* pNewVtxBodyPart = pNewVtxHdr->pBodyPart(i);

		pNewVtxBodyPart->numModels = pBodyPart->nummodels;
		pNewVtxBodyPart->modelOffset = (pData - (byte *)pNewVtxBodyPart);
		pData += pNewVtxBodyPart->numModels * sizeof( OptimizedModel::ModelHeader_t );
		// ALIGN4( pData );

		// Iterate over every submodel...
		for (j = 0; j < pBodyPart->nummodels; ++j)
		{
			mstudiomodel_t* pModel = pBodyPart->pModel(j);
			OptimizedModel::ModelHeader_t* pVtxModel = pVtxBodyPart->pModel(j);
			OptimizedModel::ModelHeader_t* pNewVtxModel = pNewVtxBodyPart->pModel(j);

			pNewVtxModel->numLODs = pVtxModel->numLODs;
			pNewVtxModel->lodOffset = (pData - (byte *)pNewVtxModel);
			pData += pNewVtxModel->numLODs * sizeof( OptimizedModel::ModelLODHeader_t );
			ALIGN4( pData );

			for ( nLodID = rootLOD; nLodID < pVtxModel->numLODs; nLodID++ )
			{
				OptimizedModel::ModelLODHeader_t *pVtxLOD = pVtxModel->pLOD( nLodID );
				OptimizedModel::ModelLODHeader_t *pNewVtxLOD = pNewVtxModel->pLOD( nLodID );

				pNewVtxLOD->numMeshes = pVtxLOD->numMeshes;
				pNewVtxLOD->switchPoint = pVtxLOD->switchPoint;
				pNewVtxLOD->meshOffset = (pData - (byte *)pNewVtxLOD);
				pData += pNewVtxLOD->numMeshes * sizeof( OptimizedModel::MeshHeader_t );
				ALIGN4( pData );

				// Iterate over all the meshes....
				for (k = 0; k < pModel->nummeshes; ++k)
				{
					Assert( pModel->nummeshes == pVtxLOD->numMeshes );
//					mstudiomesh_t* pMesh = pModel->pMesh(k);
					OptimizedModel::MeshHeader_t* pVtxMesh = pVtxLOD->pMesh(k);
					OptimizedModel::MeshHeader_t* pNewVtxMesh = pNewVtxLOD->pMesh(k);

					pNewVtxMesh->numStripGroups = pVtxMesh->numStripGroups;
					pNewVtxMesh->flags = pVtxMesh->flags;
					pNewVtxMesh->stripGroupHeaderOffset = (pData - (byte *)pNewVtxMesh);
					pData += pNewVtxMesh->numStripGroups * sizeof( OptimizedModel::StripGroupHeader_t );

					// printf("part %d : model %d : lod %d : mesh %d : strips %d : offset %d\n", i, j, nLodID, k, pVtxMesh->numStripGroups, pVtxMesh->stripGroupHeaderOffset );

					for (m = 0; m < pVtxMesh->numStripGroups; m++)
					{
						OptimizedModel::StripGroupHeader_t *pStripGroup = pVtxMesh->pStripGroup( m );
						OptimizedModel::StripGroupHeader_t *pNewStripGroup = pNewVtxMesh->pStripGroup( m );

						// int delta = ((byte *)pStripGroup - (byte *)pVtxHdr) - ((byte *)pNewStripGroup - (byte *)pNewVtxHdr);

						pNewStripGroup->numVerts = pStripGroup->numVerts;
						pNewStripGroup->vertOffset = (pData - (byte *)pNewStripGroup);
						size = pNewStripGroup->numVerts * sizeof( OptimizedModel::Vertex_t );
						memcpy( pData, pStripGroup->pVertex(0), size );
						pData += size;

						pNewStripGroup->numIndices = pStripGroup->numIndices;
						pNewStripGroup->indexOffset = (pData - (byte *)pNewStripGroup);
						size = pNewStripGroup->numIndices * sizeof( unsigned short );
						memcpy( pData, pStripGroup->pIndex(0), size );
						pData += size;

						pNewStripGroup->numStrips = pStripGroup->numStrips;
						pNewStripGroup->stripOffset = (pData - (byte *)pNewStripGroup);
						size = pNewStripGroup->numStrips * sizeof( OptimizedModel::StripHeader_t );
						pData += size;

						pNewStripGroup->flags = pStripGroup->flags;

						/*
						printf("\tnumVerts %d %d :\n", pStripGroup->numVerts, pStripGroup->vertOffset );
						printf("\tnumIndices %d %d :\n", pStripGroup->numIndices, pStripGroup->indexOffset );
						printf("\tnumStrips %d %d :\n", pStripGroup->numStrips, pStripGroup->stripOffset );
						*/

						for (n = 0; n < pStripGroup->numStrips; n++)
						{
							OptimizedModel::StripHeader_t *pStrip = pStripGroup->pStrip( n );
							OptimizedModel::StripHeader_t *pNewStrip = pNewStripGroup->pStrip( n );

							pNewStrip->numIndices = pStrip->numIndices;
							pNewStrip->indexOffset = pStrip->indexOffset;

							pNewStrip->numVerts = pStrip->numVerts;
							pNewStrip->vertOffset = pStrip->vertOffset;

							pNewStrip->numBones = pStrip->numBones;
							pNewStrip->flags = pStrip->flags;

							pNewStrip->numBoneStateChanges = pStrip->numBoneStateChanges;
							pNewStrip->boneStateChangeOffset = (pData - (byte *)pNewStrip);
							size = pNewStrip->numBoneStateChanges * sizeof( OptimizedModel::BoneStateChangeHeader_t );
							memcpy( pData, pStrip->pBoneStateChange(0), size );
							pData += size;

							/*
							printf("\t\tnumIndices %d %d :\n", pNewStrip->numIndices, pNewStrip->indexOffset );
							printf("\t\tnumVerts %d %d :\n", pNewStrip->numVerts, pNewStrip->vertOffset );
							printf("\t\tnumBoneStateChanges %d %d :\n", pNewStrip->numBoneStateChanges, pNewStrip->boneStateChangeOffset );
							*/
							// printf("(%d)\n", delta );
						}
						// printf("(%d)\n", delta );
					}
				}
			}
		}
	}

	// Iterate over every body part...
	for ( i = 0; i < pStudioHdr->numbodyparts; i++ )
	{
		mstudiobodyparts_t* pBodyPart = pStudioHdr->pBodypart(i);

		// Iterate over every submodel...
		for (j = 0; j < pBodyPart->nummodels; ++j)
		{
			// link previous LODs to higher LODs
			for ( nLodID = 0; nLodID < rootLOD; nLodID++ )
			{
				OptimizedModel::ModelLODHeader_t *pVtxLOD = pVtxHdr->pBodyPart(i)->pModel(j)->pLOD(nLodID);
				OptimizedModel::ModelLODHeader_t *pRootVtxLOD = pNewVtxHdr->pBodyPart(i)->pModel(j)->pLOD(rootLOD);
				OptimizedModel::ModelLODHeader_t *pNewVtxLOD = pNewVtxHdr->pBodyPart(i)->pModel(j)->pLOD(nLodID);

				pNewVtxLOD->numMeshes = pRootVtxLOD->numMeshes;
				pNewVtxLOD->switchPoint = pVtxLOD->switchPoint;

				int delta = (byte *)pRootVtxLOD - (byte *)pNewVtxLOD;
				pNewVtxLOD->meshOffset = pRootVtxLOD->meshOffset + delta;
			}
		}
	}

	int newLen = pData - (byte *)pNewVtxHdr;
	// printf("len %d : %d\n", len, newLen );

	// pNewVtxHdr->length = newLen;

	if (!g_quiet)
	{
		printf ("writing %s:\n", fileName);
		printf( "everything (%d bytes)\n", newLen );
	}
	
	{
		CP4AutoEditAddFile autop4( fileName, "binary" );
		SaveFile( (char *)fileName, pNewVtxHdr, newLen );
	}

	free( pNewVtxHdr );

	return true;
}




bool Clamp_RootLOD( studiohdr_t *phdr )
{
	char	filename[MAX_PATH];
	char	tmpFileName[MAX_PATH];
	int		i;
	const char						*vtxPrefixes[] = {".dx80.vtx", ".dx90.vtx", ".sw.vtx"};

	int rootLOD = g_minLod;

	if (rootLOD > g_ScriptLODs.Size() - 1)
	{
		rootLOD = g_ScriptLODs.Size() -1;
	}

	if (rootLOD == 0)
	{
		return true;
	}

	V_strcpy_safe( filename, gamedir );
	V_strcat_safe( filename, "models/" );	
	V_strcat_safe( filename, outname );
	Q_StripExtension( filename, filename, sizeof( filename ) );

	// shift the files so that g_minLod is the root LOD
	V_strcpy_safe( tmpFileName, filename );
	V_strcat_safe( tmpFileName, ".mdl" );
	Clamp_MDL_LODS( tmpFileName, rootLOD );

	V_strcpy_safe( tmpFileName, filename );
	V_strcat_safe( tmpFileName, ".vvd" );
	Clamp_VVD_LODS( tmpFileName, rootLOD );

	for (i=0; i<ARRAYSIZE(vtxPrefixes); i++)
	{
		// fixup ???.vtx
		V_strcpy_safe( tmpFileName, filename );
		V_strcat_safe( tmpFileName, vtxPrefixes[i] );
		Clamp_VTX_LODS( tmpFileName, rootLOD, phdr );
	}

	return true;
}


//----------------------------------------------------------------------
// For a particular .qc, converts all studiomdl generated files to big-endian format.
//----------------------------------------------------------------------
void WriteSwappedFile( char *srcname, char *outname, int(*pfnSwapFunc)(void*, const void*, int)  )
{
	if ( FileExists( srcname ) )
	{
		if( !g_quiet )
		{
			printf( "---------------------\n" );
			printf( "Generating Xbox360 file format for \"%s\":\n", srcname );
		}

		void *pFileBase = NULL;
		int fileSize = LoadFile( srcname, &pFileBase );
		int paddedSize = fileSize + BYTESWAP_ALIGNMENT_PADDING;

		void *pOutBase = malloc( paddedSize );

		int bytes = pfnSwapFunc( pOutBase, pFileBase, fileSize );

		if ( bytes != 0 )
		{
			CP4AutoEditAddFile autop4( outname, "binary" );
			SaveFile( outname, pOutBase, bytes );
		}

		free(pOutBase);
		free(pFileBase);

		if ( bytes == 0 )
		{
			MdlError( "Aborted byteswap on '%s':\n", srcname );
		}
	}
}

//----------------------------------------------------------------------
// For a particular .qc, converts all studiomdl generated files to big-endian format.
//----------------------------------------------------------------------
void WriteAllSwappedFiles( const char *filename )
{
	char srcname[ MAX_PATH ];
	char outname[ MAX_PATH ];

	extern IPhysicsCollision *physcollision;
	if ( physcollision )
	{
		StudioByteSwap::SetCollisionInterface( physcollision );
	}

	// Convert PHY
	Q_StripExtension( filename, srcname, sizeof( srcname ) );
	Q_strncpy( outname, srcname, sizeof( outname ) );

	Q_strcat( srcname, ".phy", sizeof( srcname ) );
	Q_strcat( outname, ".360.phy", sizeof( outname ) );

	WriteSwappedFile( srcname, outname, StudioByteSwap::ByteswapPHY );

	// Convert VVD
	Q_StripExtension( filename, srcname, sizeof( srcname ) );
	Q_strncpy( outname, srcname, sizeof( outname ) );

	Q_strcat( srcname, ".vvd", sizeof( srcname ) );
	Q_strcat( outname, ".360.vvd", sizeof( outname ) );

	WriteSwappedFile( srcname, outname, StudioByteSwap::ByteswapVVD );

	// Convert VTX
	Q_StripExtension( filename, srcname, sizeof( srcname ) );
	Q_StripExtension( srcname, srcname, sizeof( srcname ) );
	Q_strncpy( outname, srcname, sizeof( outname ) );

	Q_strcat( srcname, ".dx90.vtx", sizeof( srcname ) );
	Q_strcat( outname, ".360.vtx", sizeof( outname ) );

	WriteSwappedFile( srcname, outname, StudioByteSwap::ByteswapVTX );

	// Convert MDL
	Q_StripExtension( filename, srcname, sizeof( srcname ) );
	Q_strncpy( outname, srcname, sizeof( outname ) );

	Q_strcat( srcname, ".mdl", sizeof( srcname ) );
	Q_strcat( outname, ".360.mdl", sizeof( outname ) );

	WriteSwappedFile( srcname, outname, StudioByteSwap::ByteswapMDL );
}