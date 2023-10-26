//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Entities relating to in-level sound effects.
//
//			env_speaker: used for public address announcements over loudspeakers.
//				This tries not to drown out talking NPCs.
//
//			env_soundscape: controls what sound script an area uses.
//
//=============================================================================//

#include "cbase.h"
#include "player.h"
#include "mathlib/mathlib.h"
#include "ai_speech.h"
#include "stringregistry.h"
#include "gamerules.h"
#include "game.h"
#include <ctype.h>
#include "entitylist.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "ndebugoverlay.h"
#include "soundscape.h"
#include "igamesystem.h"
#include "KeyValues.h"
#include "filesystem.h"
#include "ambientgeneric.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// =================== ROOM SOUND FX ==========================================




// ==================== SENTENCE GROUPS, UTILITY FUNCTIONS  ======================================

int fSentencesInit = false;

// ===================== SENTENCE GROUPS, MAIN ROUTINES ========================

// given sentence group index, play random sentence for given entity.
// returns sentenceIndex - which sentence was picked 
// Ipick is only needed if you plan on stopping the sound before playback is done (see SENTENCEG_Stop). 
// sentenceIndex can be used to find the name/length of the sentence

int SENTENCEG_PlayRndI(edict_t *entity, int isentenceg, 
					  float volume, soundlevel_t soundlevel, int flags, int pitch)
{
	char name[64];
	int ipick;

	if (!fSentencesInit)
		return -1;

	name[0] = 0;

	ipick = engine->SentenceGroupPick( isentenceg, name, sizeof( name ) );
	if (ipick > 0 && name)
	{
		int sentenceIndex = SENTENCEG_Lookup( name );
		CPASAttenuationFilter filter( GetContainingEntity( entity ), soundlevel );
		CBaseEntity::EmitSentenceByIndex( filter, ENTINDEX(entity), CHAN_VOICE, sentenceIndex, volume, soundlevel, flags, pitch );
		return sentenceIndex;
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Picks a sentence, but doesn't play it
//-----------------------------------------------------------------------------
int SENTENCEG_PickRndSz(const char *szgroupname)
{
	char name[64];
	int ipick;
	int isentenceg;

	if (!fSentencesInit)
		return -1;

	name[0] = 0;

	isentenceg = engine->SentenceGroupIndexFromName(szgroupname);
	if (isentenceg < 0)
	{
		Warning( "No such sentence group %s\n", szgroupname );
		return -1;
	}

	ipick = engine->SentenceGroupPick(isentenceg, name, sizeof( name ));
	if (ipick >= 0 && name[0])
	{
		return SENTENCEG_Lookup( name );
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Plays a sentence by sentence index
//-----------------------------------------------------------------------------
void SENTENCEG_PlaySentenceIndex( edict_t *entity, int iSentenceIndex, float volume, soundlevel_t soundlevel, int flags, int pitch )
{
	if ( iSentenceIndex >= 0 )
	{
		CPASAttenuationFilter filter( GetContainingEntity( entity ), soundlevel );
		CBaseEntity::EmitSentenceByIndex( filter, ENTINDEX(entity), CHAN_VOICE, iSentenceIndex, volume, soundlevel, flags, pitch );
	}
}


int SENTENCEG_PlayRndSz(edict_t *entity, const char *szgroupname, 
					  float volume, soundlevel_t soundlevel, int flags, int pitch)
{
	char name[64];
	int ipick;
	int isentenceg;

	if (!fSentencesInit)
		return -1;

	name[0] = 0;

	isentenceg = engine->SentenceGroupIndexFromName(szgroupname);
	if (isentenceg < 0)
	{
		Warning( "No such sentence group %s\n", szgroupname );
		return -1;
	}

	ipick = engine->SentenceGroupPick(isentenceg, name, sizeof( name ));
	if (ipick >= 0 && name[0])
	{
		int sentenceIndex = SENTENCEG_Lookup( name );
		CPASAttenuationFilter filter( GetContainingEntity( entity ), soundlevel );
		CBaseEntity::EmitSentenceByIndex( filter, ENTINDEX(entity), CHAN_VOICE, sentenceIndex, volume, soundlevel, flags, pitch );
		return sentenceIndex;
	}

	return -1;
}

// play sentences in sequential order from sentence group.  Reset after last sentence.

int SENTENCEG_PlaySequentialSz(edict_t *entity, const char *szgroupname, 
					  float volume, soundlevel_t soundlevel, int flags, int pitch, int ipick, int freset)
{
	char name[64];
	int ipicknext;
	int isentenceg;

	if (!fSentencesInit)
		return -1;

	name[0] = 0;

	isentenceg = engine->SentenceGroupIndexFromName(szgroupname);
	if (isentenceg < 0)
		return -1;

	ipicknext = engine->SentenceGroupPickSequential(isentenceg, name, sizeof( name ), ipick, freset);
	if (ipicknext >= 0 && name[0])
	{
		int sentenceIndex = SENTENCEG_Lookup( name );
		CPASAttenuationFilter filter( GetContainingEntity( entity ), soundlevel );
		CBaseEntity::EmitSentenceByIndex( filter, ENTINDEX(entity), CHAN_VOICE, sentenceIndex, volume, soundlevel, flags, pitch );
		return sentenceIndex;
	}
	
	return -1;
}


#if 0
// for this entity, for the given sentence within the sentence group, stop
// the sentence.

void SENTENCEG_Stop(edict_t *entity, int isentenceg, int ipick)
{
	char buffer[64];
	char sznum[8];
	
	if (!fSentencesInit)
		return;

	if (isentenceg < 0 || ipick < 0)
		return;

	Q_snprintf(buffer,sizeof(buffer),"!%s%d", engine->SentenceGroupNameFromIndex( isentenceg ), ipick );

	UTIL_StopSound(entity, CHAN_VOICE, buffer);
}
#endif

// open sentences.txt, scan for groups, build rgsentenceg
// Should be called from world spawn, only works on the
// first call and is ignored subsequently.
void SENTENCEG_Init()
{
	if (fSentencesInit)
		return;

	engine->PrecacheSentenceFile( "scripts/sentences.txt" );
	fSentencesInit = true;
}

// convert sentence (sample) name to !sentencenum, return !sentencenum

int SENTENCEG_Lookup(const char *sample)
{
	return engine->SentenceIndexFromName( sample + 1 );
}


int SENTENCEG_GetIndex(const char *szrootname)
{
	return engine->SentenceGroupIndexFromName( szrootname );
}

void UTIL_RestartAmbientSounds( void )
{
	CAmbientGeneric *pAmbient = NULL;
	while ( ( pAmbient = (CAmbientGeneric*) gEntList.FindEntityByClassname( pAmbient, "ambient_generic" ) ) != NULL )
	{
		if (pAmbient->m_fActive )
		{
			if ( strstr( STRING( pAmbient->m_iszSound ), "mp3" ) )
			{
				pAmbient->SendSound( SND_CHANGE_VOL ); // fake a change, so we don't create 2 sounds
			}
			pAmbient->SendSound( SND_CHANGE_VOL ); // fake a change, so we don't create 2 sounds
		}
	}
#ifdef INFESTED_DLL
	pAmbient = NULL;
	while ( ( pAmbient = (CAmbientGeneric*) gEntList.FindEntityByClassname( pAmbient, "asw_ambient_generic" ) ) != NULL )
	{
		if (pAmbient->m_fActive )
		{
			if ( strstr( STRING( pAmbient->m_iszSound ), "mp3" ) )
			{
				pAmbient->SendSound( SND_CHANGE_VOL ); // fake a change, so we don't create 2 sounds
			}
			pAmbient->SendSound( SND_CHANGE_VOL ); // fake a change, so we don't create 2 sounds
		}
	}
#endif
}


// play a specific sentence over the HEV suit speaker - just pass player entity, and !sentencename

void UTIL_EmitSoundSuit(edict_t *entity, const char *sample)
{
	float fvol;
	int pitch = PITCH_NORM;

	fvol = suitvolume.GetFloat();
	if (random->RandomInt(0,1))
		pitch = random->RandomInt(0,6) + 98;

	// If friendlies are talking, reduce the volume of the suit
	if ( !g_AIFriendliesTalkSemaphore.IsAvailable( GetContainingEntity( entity ) ) )
	{
		fvol *= 0.3;
	}

	if (fvol > 0.05)
	{
		CPASAttenuationFilter filter( GetContainingEntity( entity ) );
		filter.MakeReliable();

		EmitSound_t ep;
		ep.m_nChannel = CHAN_STATIC;
		ep.m_pSoundName = sample;
		ep.m_flVolume = fvol;
		ep.m_SoundLevel = SNDLVL_NORM;
		ep.m_nPitch = pitch;

		CBaseEntity::EmitSound( filter, ENTINDEX(entity), ep );
	}
}

// play a sentence, randomly selected from the passed in group id, over the HEV suit speaker

int UTIL_EmitGroupIDSuit(edict_t *entity, int isentenceg)
{
	float fvol;
	int pitch = PITCH_NORM;
	int sentenceIndex = -1;

	fvol = suitvolume.GetFloat();
	if (random->RandomInt(0,1))
		pitch = random->RandomInt(0,6) + 98;

	// If friendlies are talking, reduce the volume of the suit
	if ( !g_AIFriendliesTalkSemaphore.IsAvailable( GetContainingEntity( entity ) ) )
	{
		fvol *= 0.3;
	}

	if (fvol > 0.05)
		sentenceIndex = SENTENCEG_PlayRndI(entity, isentenceg, fvol, SNDLVL_NORM, 0, pitch);

	return sentenceIndex;
}

// play a sentence, randomly selected from the passed in groupname

int UTIL_EmitGroupnameSuit(edict_t *entity, const char *groupname)
{
	float fvol;
	int pitch = PITCH_NORM;
	int sentenceIndex = -1;

	fvol = suitvolume.GetFloat();
	if (random->RandomInt(0,1))
		pitch = random->RandomInt(0,6) + 98;

	// If friendlies are talking, reduce the volume of the suit
	if ( !g_AIFriendliesTalkSemaphore.IsAvailable( GetContainingEntity( entity ) ) )
	{
		fvol *= 0.3;
	}

	if (fvol > 0.05)
		sentenceIndex = SENTENCEG_PlayRndSz(entity, groupname, fvol, SNDLVL_NORM, 0, pitch);

	return sentenceIndex;
}

// ===================== MATERIAL TYPE DETECTION, MAIN ROUTINES ========================
// 
// Used to detect the texture the player is standing on, map the
// texture name to a material type.  Play footstep sound based
// on material type.

char TEXTURETYPE_Find( trace_t *ptr )
{
	const surfacedata_t *psurfaceData = physprops->GetSurfaceData( ptr->surface.surfaceProps );

	return psurfaceData->game.material;
}
