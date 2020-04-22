//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: TF2 Accuracy system
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "player.h"
#include "tf_player.h"
#include "basecombatweapon.h"
#include "vstdlib/random.h"




// THIS ISN'T USED ANYMORE. NO REASON TO MAKE OUR HITSCAN WPNS THIS COMPLEX




// Accuracy is measured as the weapons spread in inches at 1024 units (~85 feet)
// Accuracy is sent to the client, where it's used to generate the size of the accuracy representation.
// Accuracy is a "floating" value, in that it's always moving towards a target accuracy, and takes some time to change

// Accuracy Multipliers
// < 1 increases accuracy, > 1 decreases
#define ACCMULT_DUCKING				0.75			// Player is ducking
#define ACCMULT_RUNNING				1.25			// Player is moving >50% of his max speed

// Ricochet Multiplier
// This works differently to other acc multipliers.
#define ACCMULT_RICOCHET			1.0				// Player is being suppressed by bullet fire
#define ACC_RICOCHET_TIME			1.0				// Amount of time accuracy is affected by a ricochet near the player
#define ACC_RICOCHET_MULTIPLE		0.25			// The effect of ricochets on accuracy is multiplied by this by the number of ricochets nearby
#define ACC_RICOCHET_CAP			10				// Maximum number of bullets to register for suppression fire

#define ACCURACY_CHANGE_SPEED		5

//-----------------------------------------------------------------------------
// Purpose: Calculates the players "accuracy" level
//-----------------------------------------------------------------------------
void CBaseTFPlayer::CalculateAccuracy( void )
{
	static flLastTime = 0;

	// Get the time since the last calculation
	float flTimeSlice = (gpGlobals->curtime - flLastTime);
	m_flTargetAccuracy = 0;

	if ( !GetPlayerClass()  )
		return;

	// Get the base accuracy from the current weapon
	if ( m_hActiveWeapon )
	{
		m_flTargetAccuracy = m_hActiveWeapon->GetAccuracy();

		// Accuracy is increased if the player's crouching
		if ( GetFlags() & FL_DUCKING )
			m_flTargetAccuracy *= m_hActiveWeapon->GetDuckingMultiplier();

		// Accuracy is decreased if the player's moving
		if ( m_vecVelocity.Length2D() > ( GetPlayerClass()->GetMaxSpeed() * 0.5 ) )
			m_flTargetAccuracy *= m_hActiveWeapon->GetRunningMultiplier();
	}

	// Accuracy is decreased if the player's arms are injured

	// Accuracy is increased if there's an Officer nearby

	// Accuracy is decreased if this player's being supressed (bullets/explosions impacting nearby)
	float flFarTime = (m_flLastRicochetNearby + ACC_RICOCHET_TIME);
	if ( gpGlobals->curtime <= flFarTime )
		m_flTargetAccuracy *= 1 + (m_flNumberOfRicochets * ACC_RICOCHET_MULTIPLE) * (ACCMULT_RICOCHET * ((flFarTime - gpGlobals->curtime) / ACC_RICOCHET_TIME));

	// Accuracy is decreased if the player's just been hit by a bullet/explosion

	// Now float towards the target accuracy
	if ( m_bSnapAccuracy )
	{
		m_bSnapAccuracy = false;
		m_flAccuracy = m_flTargetAccuracy;
	}
	else
	{
		if ( m_flAccuracy < m_flTargetAccuracy )
		{
			m_flAccuracy += (flTimeSlice * ACCURACY_CHANGE_SPEED);
			if ( m_flAccuracy > m_flTargetAccuracy )
				m_flAccuracy = m_flTargetAccuracy ;
		}
		else if ( m_flAccuracy > m_flTargetAccuracy )
		{
			m_flAccuracy -= (flTimeSlice * ACCURACY_CHANGE_SPEED);
			if ( m_flAccuracy < m_flTargetAccuracy )
				m_flAccuracy = m_flTargetAccuracy ;
		}
	}

	// Clip to prevent silly accuracies
	if ( m_flAccuracy > 1024 )
		m_flAccuracy = 1024;

	flLastTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Snap the players accuracy immediately
//-----------------------------------------------------------------------------
void CBaseTFPlayer::SnapAccuracy( void )
{
	m_bSnapAccuracy = true;
}

//-----------------------------------------------------------------------------
// Purpose: Return the player's current accuracy
//-----------------------------------------------------------------------------
float CBaseTFPlayer::GetAccuracy( void )
{
	return m_flAccuracy;
}

//-----------------------------------------------------------------------------
// Purpose: Bullets / Explosions are hitting near the player. Reduce his/her accuracy.
//-----------------------------------------------------------------------------
void CBaseTFPlayer::Supress( void )
{
	if ( gpGlobals->curtime <= (m_flLastRicochetNearby + ACC_RICOCHET_TIME) )
	{
		m_flNumberOfRicochets = MIN( ACC_RICOCHET_CAP, m_flNumberOfRicochets + 1 );
	}
	else
	{
		m_flNumberOfRicochets = 1;
	}

	m_flLastRicochetNearby = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector CBaseTFPlayer::GenerateFireVector( Vector *viewVector )
{
	// Calculate the weapon spread from the player's accuracy
	float flAcc = (GetAccuracy() * 0.5) / ACCURACY_DISTANCE;
	float flAccuracyAngle = RAD2DEG( atan( flAcc ) );
	// If the user passed in a viewVector, use it, otherwise use player's v_angle
	Vector angShootAngles = viewVector ? *viewVector : pl->v_angle;
	if ( flAccuracyAngle )
	{
		float x, y, z;
		do {
			x = random->RandomFloat(-0.5,0.5) + random->RandomFloat(-0.5,0.5);
			y = random->RandomFloat(-0.5,0.5) + random->RandomFloat(-0.5,0.5);
			z = x*x+y*y;
		} while (z > 1);

		angShootAngles.x = UTIL_AngleMod( angShootAngles.x + (x * flAccuracyAngle) );
		angShootAngles.y = UTIL_AngleMod( angShootAngles.y + (y * flAccuracyAngle) );
	}
	
	Vector forward;
	AngleVectors( angShootAngles, &forward );
	return forward;
}
