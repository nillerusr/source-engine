//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//
//
//			ambient_generic: a sound emitter used for one-shot and looping sounds.
//
//
//===========================================================================//

#include "cbase.h"
#include "asw_ambientgeneric.h"
#include "engine/IEngineSound.h"
#include "asw_util_shared.h"
#include "asw_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( ambient_generic, CASW_AmbientGeneric );
LINK_ENTITY_TO_CLASS( asw_ambient_generic, CASW_AmbientGeneric );

BEGIN_DATADESC( CASW_AmbientGeneric )

	DEFINE_FIELD( m_bStartedNonLoopingSound, FIELD_BOOLEAN ),
	DEFINE_KEYFIELD( m_fPreventStimMusicDuration, FIELD_FLOAT, "PreventStimMusicDuration" ),

END_DATADESC()


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CASW_AmbientGeneric::Activate( void )
{
	BaseClass::BaseClass::Activate();		// skip over the normal ambient generic's activate

	// Initialize sound source.  If no source was given, or source can't be found
	// then this is the source
	if (m_hSoundSource == NULL)
	{
		if (m_sSourceEntName != NULL_STRING)
		{
			m_hSoundSource = gEntList.FindEntityByName( NULL, m_sSourceEntName );
			if ( m_hSoundSource != NULL )
			{
				m_nSoundSourceEntIndex = m_hSoundSource->entindex();
			}
		}

		if (m_hSoundSource == NULL)
		{
			m_hSoundSource = this;
			m_nSoundSourceEntIndex = entindex();
		}
		else
		{
			if ( !FBitSet( m_spawnflags, SF_AMBIENT_SOUND_EVERYWHERE ) )
			{
				AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
			}
		}
	}

	// asw - we don't want ambient generics activating on level load, but instead after the briefing
	if ( UTIL_ASW_MissionHasBriefing( STRING( gpGlobals->mapname ) ) )
	{
		if ( !ASWGameRules() || ASWGameRules()->GetGameState() < ASW_GS_LAUNCHING )
			return;
	}

	// If active start the sound
	if ( m_fActive )
	{
		int flags = SND_SPAWNING;
		// If we are loading a saved game, we can't write into the init/signon buffer here, so just issue
		//  as a regular sound message...
		if ( gpGlobals->eLoadType == MapLoad_Transition ||
			gpGlobals->eLoadType == MapLoad_LoadGame || 
			g_pGameRules->InRoundRestart() )
		{
			flags = SND_NOFLAGS;
		}

		// Tracker 76119:  8/12/07 ywb: 
		//  Make sure pitch and volume are set up to the correct value (especially after restoring a .sav file)
		flags |= ( SND_CHANGE_PITCH | SND_CHANGE_VOL );  

		// Don't bother sending over to client if volume is zero, though
		if ( m_dpv.vol > 0 )
		{
			SendSound( (SoundFlags_t)flags );
		}

		SetNextThink( gpGlobals->curtime + 0.1f );
	}
}

void CASW_AmbientGeneric::SendSound( SoundFlags_t flags)
{
	BaseClass::SendSound( flags );
	if ( flags != SND_STOP )
	{
		if (m_fPreventStimMusicDuration > 0 && gpGlobals->curtime + m_fPreventStimMusicDuration > ASWGameRules()->m_fPreventStimMusicTime.Get())
		{
			Msg("Ambient generic setting stim music time +%f\n", m_fPreventStimMusicDuration);
			ASWGameRules()->m_fPreventStimMusicTime = gpGlobals->curtime + m_fPreventStimMusicDuration;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Turns an ambient sound on or off.  If the ambient is a looping sound,
//			mark sound as active (m_fActive) if it's playing, innactive if not.
//			If the sound is not a looping sound, never mark it as active.
// Input  : pActivator - 
//			pCaller - 
//			useType - 
//			value - 
//-----------------------------------------------------------------------------
void CASW_AmbientGeneric::ToggleSound()
{
	// m_fActive is true only if a looping sound is playing.

	if ( m_fActive )
	{// turn sound off

		if (m_dpv.cspinup)
		{
			// Don't actually shut off. Each toggle causes
			// incremental spinup to max pitch

			if (m_dpv.cspincount <= m_dpv.cspinup)
			{	
				int pitchinc;

				// start a new spinup
				m_dpv.cspincount++;

				pitchinc = (255 - m_dpv.pitchstart) / m_dpv.cspinup;

				m_dpv.spinup = m_dpv.spinupsav;
				m_dpv.spindown = 0;

				m_dpv.pitchrun = m_dpv.pitchstart + pitchinc * m_dpv.cspincount;
				if (m_dpv.pitchrun > 255) m_dpv.pitchrun = 255;

				SetNextThink( gpGlobals->curtime + 0.1f );
			}

		}
		else
		{
			m_fActive = false;

			// HACKHACK - this makes the code in Precache() work properly after a save/restore
			m_spawnflags |= SF_AMBIENT_SOUND_START_SILENT;

			if (m_dpv.spindownsav || m_dpv.fadeoutsav)
			{
				// spin it down (or fade it) before shutoff if spindown is set
				m_dpv.spindown = m_dpv.spindownsav;
				m_dpv.spinup = 0;

				m_dpv.fadeout = m_dpv.fadeoutsav;
				m_dpv.fadein = 0;
				SetNextThink( gpGlobals->curtime + 0.1f );
			}
			else
			{
				SendSound( SND_STOP ); // stop sound
			}
		}
	}
	else 
	{// turn sound on

		// only toggle if this is a looping sound.  If not looping, each
		// trigger will cause the sound to play.  If the sound is still
		// playing from a previous trigger press, it will be shut off
		// and then restarted.

		if (m_fLooping)
			m_fActive = true;
		else
		{
			// shut sound off now - may be interrupting a long non-looping sound
			SendSound( SND_STOP ); // stop sound
			m_bStartedNonLoopingSound = true;
		}

		// init all ramp params for startup

		InitModulationParms();

		SendSound( SND_NOFLAGS ); // send sound

		SetNextThink( gpGlobals->curtime + 0.1f );

	} 
}


// KeyValue - load keyvalue pairs into member data of the
// ambient generic. NOTE: called BEFORE spawn!
bool CASW_AmbientGeneric::KeyValue( const char *szKeyName, const char *szValue )
{	
	if (FStrEq(szKeyName, "fadeout"))
	{
		// asw - only fade out if we're active (this is used to stop all ambient generics at the debrief
		if (m_bStartedNonLoopingSound)		// force stop non looping sounds (can't fade them out, as fade causes them to play if they're not actually playing and we don't know if they're playing or not)
		{
			SendSound(SND_STOP);
		}
		else if (m_fActive)
		{
			m_dpv.fadein = 0;
			m_dpv.fadeinsav = 0;
			m_dpv.fadeout = atoi(szValue);

			if (m_dpv.fadeout > 100) m_dpv.fadeout = 100;
			if (m_dpv.fadeout < 0) m_dpv.fadeout = 0;

			if (m_dpv.fadeout > 0)
				m_dpv.fadeout = (101 - m_dpv.fadeout) * 64;
			m_dpv.fadeoutsav = m_dpv.fadeout;
		}
	}
	else if (FStrEq(szKeyName, "aswactivate"))		// asw - a way to activate ambient generics after the briefing
	{
		// If active start the sound
		if ( m_fActive )
		{
			SoundFlags_t flags = SND_NOFLAGS; //SND_SPAWNING;
			// If we are loading a saved game, we can't write into the init/signon buffer here, so just issue
			//  as a regular sound message...
			//if ( gpGlobals->eLoadType == MapLoad_Transition ||
			//gpGlobals->eLoadType == MapLoad_LoadGame || 
			//g_pGameRules->InRoundRestart() )
			//{
			//flags = SND_NOFLAGS;
			//}

			SendSound( flags );

			SetNextThink( gpGlobals->curtime + 0.1f );
		}
	}
	else
		return BaseClass::KeyValue( szKeyName, szValue );

	return true;
}
