//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
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

#define SPEAKER_START_SILENT			1	// wait for trigger 'on' to start announcements

// ===================================================================================
//
// Speaker class. Used for announcements per level, for door lock/unlock spoken voice. 
//

class CSpeaker : public CPointEntity
{
	DECLARE_CLASS( CSpeaker, CPointEntity );
public:
	bool KeyValue( const char *szKeyName, const char *szValue );
	void Spawn( void );
	void Precache( void );
	void ToggleUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void SpeakerThink( void );
	
	virtual int	ObjectCaps( void ) { return (CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }
	
	int	m_preset;			// preset number
	string_t m_iszMessage;

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( speaker, CSpeaker );

BEGIN_DATADESC( CSpeaker )
	DEFINE_FIELD( m_preset, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_iszMessage, FIELD_STRING, "message" ),
	DEFINE_THINKFUNC( SpeakerThink ),
	DEFINE_USEFUNC( ToggleUse ),
END_DATADESC()

//
// ambient_generic - general-purpose user-defined static sound
//
void CSpeaker::Spawn( void )
{
	char* szSoundFile = (char*) STRING( m_iszMessage );

	if ( !m_preset && ( m_iszMessage == NULL_STRING || strlen( szSoundFile ) < 1 ) )
	{
		Msg( "SPEAKER with no Level/Sentence! at: %f, %f, %f\n", GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
		SetNextThink( gpGlobals->curtime + 0.1 );
		SetThink( &CSpeaker::SUB_Remove );
		return;
	}
    SetSolid( SOLID_NONE );
    SetMoveType( MOVETYPE_NONE );

	
	SetThink(&CSpeaker::SpeakerThink);
	SetNextThink( TICK_NEVER_THINK );

	// allow on/off switching via 'use' function.
	SetUse ( &CSpeaker::ToggleUse );

	Precache( );
}

#define ANNOUNCE_MINUTES_MIN	0.25	 
#define ANNOUNCE_MINUTES_MAX	2.25

void CSpeaker::Precache( void )
{
	if ( !FBitSet ( GetSpawnFlags(), SPEAKER_START_SILENT ) )
		// set first announcement time for random n second
		SetNextThink( gpGlobals->curtime + random->RandomFloat( 5.0, 15.0 ) );
}
void CSpeaker::SpeakerThink( void )
{
	char* szSoundFile = NULL;
	float flvolume = m_iHealth * 0.1;
	int flags = 0;
	int pitch = 100;


	// Wait for the talking characters to finish first.
	if ( !g_AIFriendliesTalkSemaphore.IsAvailable( this ) || !g_AIFoesTalkSemaphore.IsAvailable( this ) )
	{
		float releaseTime = MAX( g_AIFriendliesTalkSemaphore.GetReleaseTime(), g_AIFoesTalkSemaphore.GetReleaseTime() );
		SetNextThink( gpGlobals->curtime + releaseTime + random->RandomFloat( 5, 10 ) );
		return;
	}
	
	if (m_preset)
	{
		// go lookup preset text, assign szSoundFile
		switch (m_preset)
		{
		case 1: szSoundFile =  "C1A0_"; break;
		case 2: szSoundFile =  "C1A1_"; break;
		case 3: szSoundFile =  "C1A2_"; break;
		case 4: szSoundFile =  "C1A3_"; break;
		case 5: szSoundFile =  "C1A4_"; break; 
		case 6: szSoundFile =  "C2A1_"; break;
		case 7: szSoundFile =  "C2A2_"; break;
		case 8: szSoundFile =  "C2A3_"; break;
		case 9: szSoundFile =  "C2A4_"; break;
		case 10: szSoundFile = "C2A5_"; break;
		case 11: szSoundFile = "C3A1_"; break;
		case 12: szSoundFile = "C3A2_"; break;
		}
	} else
		szSoundFile = (char*) STRING( m_iszMessage );
	
	if (szSoundFile[0] == '!')
	{
		// play single sentence, one shot
		UTIL_EmitAmbientSound ( GetSoundSourceIndex(), GetAbsOrigin(), szSoundFile, 
			flvolume, SNDLVL_120dB, flags, pitch);

		// shut off and reset
		SetNextThink( TICK_NEVER_THINK );
	}
	else
	{
		// make random announcement from sentence group

		if ( SENTENCEG_PlayRndSz( edict(), szSoundFile, flvolume, SNDLVL_120dB, flags, pitch) < 0 )
			 Msg(  "Level Design Error!\nSPEAKER has bad sentence group name: %s\n",szSoundFile); 

		// set next announcement time for random 5 to 10 minute delay
		SetNextThink ( gpGlobals->curtime + 
						random->RandomFloat( ANNOUNCE_MINUTES_MIN * 60.0, ANNOUNCE_MINUTES_MAX * 60.0 ) );

		// time delay until it's ok to speak: used so that two NPCs don't talk at once
		g_AIFriendliesTalkSemaphore.Acquire( 5, this );		
		g_AIFoesTalkSemaphore.Acquire( 5, this );		
	}

	return;
}


//
// ToggleUse - if an announcement is pending, cancel it.  If no announcement is pending, start one.
//
void CSpeaker::ToggleUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int fActive = (GetNextThink() > 0.0);

	// fActive is TRUE only if an announcement is pending
	
	if ( useType != USE_TOGGLE )
	{
		// ignore if we're just turning something on that's already on, or
		// turning something off that's already off.
		if ( (fActive && useType == USE_ON) || (!fActive && useType == USE_OFF) )
			return;
	}

	if ( useType == USE_ON )
	{
		// turn on announcements
		SetNextThink( gpGlobals->curtime + 0.1 );
		return;
	}

	if ( useType == USE_OFF )
	{
		// turn off announcements
		SetNextThink( TICK_NEVER_THINK );
		return;
	
	}

	// Toggle announcements

	
	if ( fActive )
	{
		// turn off announcements
		SetNextThink( TICK_NEVER_THINK );
	}
	else 
	{
		// turn on announcements
		SetNextThink( gpGlobals->curtime + 0.1 );
	} 
}

// KeyValue - load keyvalue pairs into member data
// NOTE: called BEFORE spawn!

bool CSpeaker::KeyValue( const char *szKeyName, const char *szValue )
{
	// preset
	if (FStrEq(szKeyName, "preset"))
	{
		m_preset = atoi(szValue);
		return true;
	}
	else
		return BaseClass::KeyValue( szKeyName, szValue );
}
