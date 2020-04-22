//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Sound code shared between server and client 
//
//=============================================================================//


#include <math.h>

#include "convar.h"
#include "sound.h"

ConVar snd_refdist( "snd_refdist", "36", FCVAR_CHEAT);
ConVar snd_refdb( "snd_refdb", "60", FCVAR_CHEAT );
ConVar snd_foliage_db_loss( "snd_foliage_db_loss", "4", FCVAR_CHEAT ); 
ConVar snd_gain( "snd_gain", "1", FCVAR_CHEAT );
ConVar snd_gain_max( "snd_gain_max", "1", FCVAR_CHEAT );
ConVar snd_gain_min( "snd_gain_min", "0.01", FCVAR_CHEAT );

// calculate gain based on atmospheric attenuation.
// as gain excedes threshold, round off (compress) towards 1.0 using spline
#define SND_GAIN_COMP_EXP_MAX	2.5f	// Increasing SND_GAIN_COMP_EXP_MAX fits compression curve more closely
										// to original gain curve as it approaches 1.0.  
#define SND_GAIN_COMP_EXP_MIN	0.8f	

#define SND_GAIN_COMP_THRESH	0.5f		// gain value above which gain curve is rounded to approach 1.0

#define SND_DB_MAX				140.0f	// max db of any sound source
#define SND_DB_MED				90.0f	// db at which compression curve changes

#define SNDLVL_TO_DIST_MULT( sndlvl ) ( sndlvl ? ((pow( 10.0f, snd_refdb.GetFloat() / 20 ) / pow( 10.0f, (float)sndlvl / 20 )) / snd_refdist.GetFloat()) : 0 )
#define DIST_MULT_TO_SNDLVL( dist_mult ) (soundlevel_t)(int)( dist_mult ? ( 20 * log10( pow( 10.0f, snd_refdb.GetFloat() / 20 ) / (dist_mult * snd_refdist.GetFloat()) ) ) : 0 )



float SND_GetGainFromMult( float gain, float dist_mult, vec_t dist )
{
	// test additional attenuation
	// at 30c, 14.7psi, 60% humidity, 1000Hz == 0.22dB / 100ft.
	// dense foliage is roughly 2dB / 100ft

	float additional_dB_loss = snd_foliage_db_loss.GetFloat() * (dist / 1200);
	float additional_dist_mult = pow( 10.0f, additional_dB_loss / 20);

	float relative_dist = dist * dist_mult * additional_dist_mult;

	// hard code clamp gain to 10x normal (assumes volume and external clipping)

	if (relative_dist > 0.1)
	{
		gain *= (1/relative_dist);
	}
	else
		gain *= 10.0;

	// if gain passess threshold, compress gain curve such that gain smoothly approaches 1.0

	if ( gain > SND_GAIN_COMP_THRESH )
	{
		float snd_gain_comp_power = SND_GAIN_COMP_EXP_MAX;
		soundlevel_t sndlvl = DIST_MULT_TO_SNDLVL( dist_mult );
		float Y;
		
		// decrease compression curve fit for higher sndlvl values

		if ( sndlvl > SND_DB_MED )
		{
			// snd_gain_power varies from max to min as sndlvl varies from 90 to 140

			snd_gain_comp_power = RemapVal ((float)sndlvl, SND_DB_MED, SND_DB_MAX, SND_GAIN_COMP_EXP_MAX, SND_GAIN_COMP_EXP_MIN);
		}

		// calculate crossover point

		Y = -1.0 / ( pow(SND_GAIN_COMP_THRESH, snd_gain_comp_power) * (SND_GAIN_COMP_THRESH - 1) );
		
		// calculate compressed gain

		gain = 1.0 - 1.0 / (Y * pow( gain, snd_gain_comp_power ) );

		gain = gain * snd_gain_max.GetFloat();
	}

	if ( gain < snd_gain_min.GetFloat() )
	{
		// sounds less than snd_gain_min fall off to 0 in distance it took them to fall to snd_gain_min

		gain = snd_gain_min.GetFloat() * (2.0 - relative_dist * snd_gain_min.GetFloat());
		
		if (gain <= 0.0)
			gain = 0.001;	// don't propagate 0 gain
	}

	return gain;
}

float S_GetGainFromSoundLevel( soundlevel_t soundlevel, vec_t dist )
{
	float gain = snd_gain.GetFloat();

	float dist_mult = SNDLVL_TO_DIST_MULT( soundlevel );
	if ( dist_mult )
	{
		gain = SND_GetGainFromMult( gain, dist_mult, dist );
	}

	return gain;
}
