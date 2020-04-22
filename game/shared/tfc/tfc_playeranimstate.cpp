//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "tfc_playeranimstate.h"
#include "base_playeranimstate.h"
#include "tier0/vprof.h"
#include "animation.h"
#include "studio.h"
#include "apparent_velocity_helper.h"
#include "utldict.h"


#ifdef CLIENT_DLL
	#include "c_tfc_player.h"
#else
	#include "tfc_player.h"
#endif


// When moving this fast, he plays run anim.
#define ARBITRARY_RUN_SPEED		175.0f

#define MAX_STANDING_RUN_SPEED	320
#define MAX_CROUCHED_RUN_SPEED	110


// ------------------------------------------------------------------------------------------------ //
// CPlayerAnimState declaration.
// ------------------------------------------------------------------------------------------------ //

class CTFCPlayerAnimState : public ITFCPlayerAnimState, public CBasePlayerAnimState
{
public:
	
	DECLARE_CLASS( CTFCPlayerAnimState, CBasePlayerAnimState );

	CTFCPlayerAnimState();
	void InitTFC( CTFCPlayer *pPlayer );

	// This is called by both the client and the server in the same way to trigger events for
	// players firing, jumping, throwing grenades, etc.
	virtual void DoAnimationEvent( PlayerAnimEvent_t event, int nData );
	virtual int CalcAimLayerSequence( float *flCycle, float *flAimSequenceWeight );
	virtual float SetOuterBodyYaw( float flValue );
	virtual Activity CalcMainActivity();
	virtual float GetCurrentMaxGroundSpeed();
	virtual void ClearAnimationState();
	virtual bool ShouldUpdateAnimState();


private:
	
	const char* GetWeaponSuffix();
	bool HandleJumping();
	bool HandleDeath( Activity *deathActivity );


private:
	
	CTFCPlayer *m_pOuterTFC;
	
	bool m_bJumping;
	bool m_bFirstJumpFrame;
	float m_flJumpStartTime;

	bool m_bFiring;
	float m_flFireStartTime;

	bool m_bDying;
	Activity m_DeathActivity;
};


ITFCPlayerAnimState* CreatePlayerAnimState( CTFCPlayer *pPlayer )
{
	CTFCPlayerAnimState *pRet = new CTFCPlayerAnimState;
	pRet->InitTFC( pPlayer );
	return pRet;
}


// ----------------------------------------------------------------------------- //
// CTFCPlayerAnimState implementation.
// ----------------------------------------------------------------------------- //

CTFCPlayerAnimState::CTFCPlayerAnimState()
{
	m_pOuterTFC = NULL;
	m_bJumping = false;
	m_bFirstJumpFrame = false;
	m_bFiring = false;
}


void CTFCPlayerAnimState::InitTFC( CTFCPlayer *pPlayer )
{
	m_pOuterTFC = pPlayer;
	
	CModAnimConfig config;
	config.m_flMaxBodyYawDegrees = 30;
	config.m_LegAnimType = LEGANIM_GOLDSRC;
	config.m_bUseAimSequences = true;

	BaseClass::Init( pPlayer, config );
}


const char* CTFCPlayerAnimState::GetWeaponSuffix()
{
	CBaseCombatWeapon *pWeapon = m_pOuterTFC->GetActiveWeapon();
	if ( pWeapon )
		return pWeapon->GetWpnData().szAnimationPrefix;
	else
		return "shotgun";
}


int CTFCPlayerAnimState::CalcAimLayerSequence( float *flCycle, float *flAimSequenceWeight )
{
	const char *pWeaponSuffix = GetWeaponSuffix();
	if ( !pWeaponSuffix )
		return 0;

	// Are we aiming or firing?
	const char *pAimOrShoot = "aim";
	if ( m_bFiring )
		pAimOrShoot = "shoot";

	// Are we standing or crouching?
	int iSequence = 0;
	const char *pPrefix = "ref";
	if ( m_bDying )
	{
		// While dying, only play the main sequence.. don't layer this one on top.
		*flAimSequenceWeight = 0;	
	}
	else
	{
		switch ( GetCurrentMainSequenceActivity() )
		{
			case ACT_CROUCHIDLE:
			case ACT_RUN_CROUCH:
				pPrefix = "crouch";
				break;
		}
	}

	iSequence = CalcSequenceIndex( "%s_%s_%s", pPrefix, pAimOrShoot, pWeaponSuffix );
	
	// Check if we're done firing.
	if ( m_bFiring )
	{
		float dur = m_pOuterTFC->SequenceDuration( iSequence );
		*flCycle = (gpGlobals->curtime - m_flFireStartTime) / dur;
		if ( *flCycle >= 1 )
		{
			*flCycle = 1;
			m_bFiring = false;
		}
	}	

	return iSequence;
}


void CTFCPlayerAnimState::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	if ( event == PLAYERANIMEVENT_JUMP )
	{
		// Main animation goes to ACT_HOP.
		m_bJumping = true;
		m_bFirstJumpFrame = true;
		m_flJumpStartTime = gpGlobals->curtime;
	}
	else if ( event == PLAYERANIMEVENT_FIRE_GUN )
	{
		// The middle part of the aim layer sequence becomes "shoot" until that animation is complete.
		m_bFiring = true;
		m_flFireStartTime = gpGlobals->curtime;
	}
	else if ( event == PLAYERANIMEVENT_DIE )
	{
		m_bFiring = m_bJumping = false;
		m_bDying = true;
		
		Activity acts[] = 
		{
			ACT_DIESIMPLE,
			ACT_DIEBACKWARD,
			ACT_DIEFORWARD,
			ACT_DIE_HEADSHOT,
			ACT_DIE_GUTSHOT
		};

		m_DeathActivity = acts[ RandomInt( 0, ARRAYSIZE( acts ) - 1 ) ];
		RestartMainSequence();	// Play a death animation.
	}
}


float CTFCPlayerAnimState::SetOuterBodyYaw( float flValue )
{
	m_pOuterTFC->SetBoneController( 0, flValue );
	return flValue;
}


bool CTFCPlayerAnimState::HandleJumping()
{
	if ( m_bJumping )
	{
		if ( m_bFirstJumpFrame )
		{
			m_bFirstJumpFrame = false;
			RestartMainSequence();	// Reset the animation.
		}

		// Don't check if he's on the ground for a sec.. sometimes the client still has the
		// on-ground flag set right when the message comes in.
		if ( gpGlobals->curtime - m_flJumpStartTime > 0.2f )
		{
			if ( m_pOuterTFC->GetFlags() & FL_ONGROUND )
			{
				m_bJumping = false;
				RestartMainSequence();	// Reset the animation.
			}
		}
	}

	// Are we still jumping? If so, keep playing the jump animation.
	return m_bJumping;
}


bool CTFCPlayerAnimState::HandleDeath( Activity *deathActivity )
{
	if ( m_bDying )
	{
		if ( m_pOuterTFC->IsAlive() )
		{
			m_bDying = false;
		}
		else
		{
			*deathActivity = m_DeathActivity;
		}
	}
	return m_bDying;
}


Activity CTFCPlayerAnimState::CalcMainActivity()
{
	Activity deathActivity = ACT_IDLE;
	if ( HandleDeath( &deathActivity ) )
	{
		return deathActivity;
	}
	else if ( HandleJumping() )
	{
		return ACT_HOP;
	}
	else
	{
		Activity idealActivity = ACT_IDLE;
		float flOuterSpeed = GetOuterXYSpeed();

		if ( m_pOuterTFC->GetFlags() & FL_DUCKING )
		{
			if ( flOuterSpeed > 0.1f )
				idealActivity = ACT_RUN_CROUCH;
			else
				idealActivity = ACT_CROUCHIDLE;
		}
		else
		{
			if ( flOuterSpeed > 0.1f )
			{
				if ( flOuterSpeed > ARBITRARY_RUN_SPEED )
					idealActivity = ACT_RUN;
				else
					idealActivity = ACT_WALK;
			}
			else
			{
				idealActivity = ACT_IDLE;
			}
		}

		return idealActivity;
	}
}


float CTFCPlayerAnimState::GetCurrentMaxGroundSpeed()
{
	Activity act = GetCurrentMainSequenceActivity();
	if ( act == ACT_CROUCHIDLE || act == ACT_RUN_CROUCH )
		return MAX_CROUCHED_RUN_SPEED;
	else
		return MAX_STANDING_RUN_SPEED;
}


void CTFCPlayerAnimState::ClearAnimationState()
{
	m_bJumping = false;
	m_bFiring = false;
	m_bDying = false;

	BaseClass::ClearAnimationState();
}


bool CTFCPlayerAnimState::ShouldUpdateAnimState()
{
	return true;
}

