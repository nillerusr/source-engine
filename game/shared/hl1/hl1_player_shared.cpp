//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "tier0/vprof.h"

#ifdef CLIENT_DLL
#include "hl1_player_shared.h"
#include "hl1/c_hl1mp_player.h"
//#define CRecipientFilter C_RecipientFilter
#else
#include "hl1mp_player.h"
#endif

// When moving this fast, he plays run anim.
#define ARBITRARY_RUN_SPEED		175.0f

#define MAX_STANDING_RUN_SPEED	320
#define MAX_CROUCHED_RUN_SPEED	110


// ------------------------------------------------------------------------------------------------ //
// CPlayerAnimState declaration.
// ------------------------------------------------------------------------------------------------ //

class CPlayerAnimState : public IHL1MPPlayerAnimState, public CBasePlayerAnimState
{
public:
	
	DECLARE_CLASS( CPlayerAnimState, CBasePlayerAnimState );

	CPlayerAnimState();
	void Init( CHL1MP_Player *pPlayer );

	// This is called by both the client and the server in the same way to trigger events for
	// players firing, jumping, throwing grenades, etc.
	virtual void DoAnimationEvent( PlayerAnimEvent_t event, int nData );
	virtual int CalcAimLayerSequence( float *flCycle, float *flAimSequenceWeight, bool bForceIdle );
	virtual float SetOuterBodyYaw( float flValue );
	virtual Activity CalcMainActivity();
	virtual float GetCurrentMaxGroundSpeed();
	virtual void ClearAnimationState();
	virtual bool ShouldUpdateAnimState();
	virtual int SelectWeightedSequence( Activity activity ) ;

	float CalcMovementPlaybackRate( bool *bIsMoving );

	virtual void		ComputePoseParam_BodyPitch( CStudioHdr *pStudioHdr );


private:
	
	const char* GetWeaponSuffix();
	bool HandleJumping();
	bool HandleDeath( Activity *deathActivity );


private:
	
	CHL1MP_Player *m_pOuter;
	
	bool m_bJumping;
	bool m_bFirstJumpFrame;
	float m_flJumpStartTime;

	bool m_bFiring;
	float m_flFireStartTime;

	bool m_bDying;
	Activity m_DeathActivity;
};


IHL1MPPlayerAnimState* CreatePlayerAnimState( CHL1MP_Player *pPlayer )
{
	CPlayerAnimState *pRet = new CPlayerAnimState;
	pRet->Init( pPlayer );
	return pRet;
}


// ----------------------------------------------------------------------------- //
// CPlayerAnimState implementation.
// ----------------------------------------------------------------------------- //

CPlayerAnimState::CPlayerAnimState()
{
	m_pOuter = NULL;
	m_bJumping = false;
	m_bFirstJumpFrame = false;
	m_bFiring = false;
}


void CPlayerAnimState::Init( CHL1MP_Player *pPlayer )
{
	m_pOuter = pPlayer;
	
	CModAnimConfig config;
	config.m_flMaxBodyYawDegrees = 90;
	config.m_LegAnimType = LEGANIM_GOLDSRC;
	config.m_bUseAimSequences = true;

	BaseClass::Init( pPlayer, config );
}


const char* CPlayerAnimState::GetWeaponSuffix()
{
	CBaseCombatWeapon *pWeapon = m_pOuter->GetActiveWeapon();
	if ( pWeapon )
		return pWeapon->GetWpnData().szAnimationPrefix;
	else
		return "shotgun";
}


int CPlayerAnimState::CalcAimLayerSequence( float *flCycle, float *flAimSequenceWeight, bool bForceIdle )
{
	const char *pWeaponSuffix = GetWeaponSuffix();
	if ( !pWeaponSuffix )
		return 0;

	if ( strcmp( pWeaponSuffix, "glock" ) == 0 )
		pWeaponSuffix = "onehanded";

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
		float dur = m_pOuter->SequenceDuration( iSequence );
		*flCycle = (gpGlobals->curtime - m_flFireStartTime) / dur;
		if ( *flCycle >= 1 )
		{
			*flCycle = 1;
			m_bFiring = false;
		}
	}	

	return iSequence;
}


void CPlayerAnimState::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
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
}


float CPlayerAnimState::SetOuterBodyYaw( float flValue )
{
//	m_pOuter->SetBoneController( 0, flValue );

	float fAcc = flValue / 4;

	for ( int i = 0; i < 4; i++ )
	{
		m_pOuter->SetBoneController( i, fAcc );
	}

	return flValue;
}


bool CPlayerAnimState::HandleJumping()
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
			if ( m_pOuter->GetFlags() & FL_ONGROUND )
			{
				m_bJumping = false;
				RestartMainSequence();	// Reset the animation.
			}
		}
	}

	// Are we still jumping? If so, keep playing the jump animation.
	return m_bJumping;
}

int CPlayerAnimState::SelectWeightedSequence( Activity activity ) 
{
	return m_pOuter->m_iRealSequence;
}

bool CPlayerAnimState::HandleDeath( Activity *deathActivity )
{
	if ( m_bDying )
	{
		if ( m_pOuter->IsAlive() )
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


Activity CPlayerAnimState::CalcMainActivity()
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

		if ( m_pOuter->GetFlags() & FL_DUCKING )
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

		return idealActivity	;
	}
}


float CPlayerAnimState::GetCurrentMaxGroundSpeed()
{
	Activity act = GetCurrentMainSequenceActivity();
	if ( act == ACT_CROUCHIDLE || act == ACT_RUN_CROUCH )
		return MAX_CROUCHED_RUN_SPEED;
	else
		return MAX_STANDING_RUN_SPEED;
}


void CPlayerAnimState::ClearAnimationState()
{
	m_bJumping = false;
	m_bFiring = false;
	m_bDying = false;

	BaseClass::ClearAnimationState();
}


bool CPlayerAnimState::ShouldUpdateAnimState()
{
	return true;
}

float CPlayerAnimState::CalcMovementPlaybackRate( bool *bIsMoving )
{
	// Determine ideal playback rate
	Vector vel;
	GetOuterAbsVelocity( vel );

	float flReturnValue = BaseClass::CalcMovementPlaybackRate( bIsMoving );

	Activity eActivity = GetOuter()->GetSequenceActivity( GetOuter()->GetSequence() ) ;

	if ( eActivity == ACT_RUN || eActivity == ACT_WALK || eActivity == ACT_CROUCH )
	{
		VectorNormalize( vel );

		Vector vForward;
		AngleVectors( GetOuter()->EyeAngles(), &vForward );

		float flDot = DotProduct( vel, vForward );

		if ( flDot < 0 )
		{
			flReturnValue *= -1;
		}
	}

	return flReturnValue;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerAnimState::ComputePoseParam_BodyPitch( CStudioHdr *pStudioHdr )
{
	VPROF( "CBasePlayerAnimState::ComputePoseParam_BodyPitch" );

	// Get pitch from v_angle
	float flPitch = -m_flEyePitch;
	if ( flPitch > 180.0f )
	{
		flPitch -= 360.0f;
	}
	flPitch = clamp( flPitch, -50, 45 );

	// See if we have a blender for pitch
	int pitch = GetOuter()->LookupPoseParameter( pStudioHdr, "XR" );
	if ( pitch < 0 )
		return;

	GetOuter()->SetPoseParameter( pStudioHdr, pitch, flPitch );
	g_flLastBodyPitch = flPitch;
}

