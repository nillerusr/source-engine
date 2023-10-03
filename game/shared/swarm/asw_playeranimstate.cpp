//====== Copyright © 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "asw_playeranimstate.h"
#include "base_playeranimstate.h"
#include "tier0/vprof.h"
#include "animation.h"
#include "basecombatweapon_shared.h"
#include "studio.h"
#include "apparent_velocity_helper.h"
#include "utldict.h"
 
#include "asw_weapon_ammo_bag_shared.h"
#include "asw_weapon_parse.h"
#ifdef CLIENT_DLL
	#include "c_asw_player.h"
	#include "bone_setup.h"
	#include "tier1/interpolatedvar.h"
	#include "c_asw_marine.h"
	#include "asw_marine_profile.h"
	#include "c_asw_weapon.h"
	#include "engine/ivdebugoverlay.h"
	#include "c_asw_game_resource.h"
	#include "c_asw_marine_resource.h"
#else
	#include "asw_player.h"
	#include "asw_marine.h"
	#include "asw_marine_profile.h"
	#define C_ASW_Marine CASW_Marine
	#define C_ASW_Weapon_Ammo_Bag CASW_Weapon_Ammo_Bag
	#define C_ASW_Marine CASW_Marine
#endif
#include "asw_melee_system.h"

#define ANIM_TOPSPEED_WALK			150
#define ANIM_TOPSPEED_RUN			300
#define ANIM_TOPSPEED_RUN_CROUCH	85
#define BASE_TOPSPEED_WALK			100
#define BASE_TOPSPEED_RUN			250
#define BASE_TOPSPEED_RUN_CROUCH	85

#define DEFAULT_IDLE_NAME "idle_upper_"
#define DEFAULT_CROUCH_IDLE_NAME "crouch_idle_upper_"
#define DEFAULT_CROUCH_WALK_NAME "crouch_walk_upper_"
#define DEFAULT_WALK_NAME "walk_upper_"
#define DEFAULT_RUN_NAME "run_upper_"

#define DEFAULT_FIRE_IDLE_NAME "idle_shoot_"
#define DEFAULT_FIRE_CROUCH_NAME "crouch_idle_shoot_"
#define DEFAULT_FIRE_CROUCH_WALK_NAME "crouch_walk_shoot_"
#define DEFAULT_FIRE_WALK_NAME "walk_shoot_"
#define DEFAULT_FIRE_RUN_NAME "run_shoot_"

#define ASW_TRANSLATE_ACTIVITIES_BY_WEAPON

#define FIRESEQUENCE_LAYER		(AIMSEQUENCE_LAYER+NUM_AIMSEQUENCE_LAYERS)
#define RELOADSEQUENCE_LAYER	(FIRESEQUENCE_LAYER + 1)
#define NUM_LAYERS_WANTED		(RELOADSEQUENCE_LAYER + 1)

ConVar asw_debuganimstate("asw_debuganimstate", "0", FCVAR_REPLICATED);
ConVar asw_walk_speed("asw_walk_speed", "175", FCVAR_REPLICATED);
ConVar asw_walk_point("asw_walk_point", "200", FCVAR_REPLICATED);
ConVar asw_run_speed("asw_run_speed", "380", FCVAR_REPLICATED, "Test of movement speed returned in animstate code");
ConVar asw_walk_fixed_blend("asw_walk_fixed_blend", "0.5", FCVAR_REPLICATED);
ConVar asw_run_fixed_blend("asw_run_fixed_blend", "0.5", FCVAR_REPLICATED);
ConVar asw_marine_speed_type("asw_marine_speed_type", "2", FCVAR_REPLICATED, "0=fixed points scaled by sequence speed, 1=fixed points, 2=just sequence speed");

ConVar asw_run_speed_scale("asw_run_speed_scale", "1", FCVAR_REPLICATED);		// was 2.1
ConVar asw_walk_speed_scale("asw_walk_speed_scale", "1", FCVAR_REPLICATED);		// was 2.4

ConVar asw_sequence_speed_cap("asw_sequence_speed_cap", "160", FCVAR_REPLICATED);
ConVar asw_walk_sequence_speed_cap("asw_walk_sequence_speed_cap", "50", FCVAR_REPLICATED);	// was 120

ConVar asw_flare_anim_speed("asw_flare_anim_speed", "1.5f", FCVAR_REPLICATED, "Playback rate of the flare throw anim");
ConVar asw_fixed_movement_playback_rate("asw_fixed_movement_playback_rate", "0", FCVAR_REPLICATED, "Movement animation playback is always unscaled");
ConVar asw_feetyawrate("asw_feetyawrate", "300",  FCVAR_REPLICATED, "How many degrees per second that we can turn our feet or upper body." );
ConVar asw_facefronttime( "asw_facefronttime", "2.0", FCVAR_REPLICATED, "How many seconds before marine faces front when standing still." );
ConVar asw_max_body_yaw( "asw_max_body_yaw", "30", FCVAR_REPLICATED, "Max angle body yaw can turn either side before feet snap." );
extern ConVar asw_melee_debug;

#define ASW_WALK_SPEED asw_walk_speed.GetFloat()
#define ASW_WALK_POINT asw_walk_point.GetFloat()
#define ASW_RUN_SPEED asw_run_speed.GetFloat()

#ifdef CLIENT_DLL
ConVar asw_prevent_ai_crouch("asw_prevent_ai_crouch", "0", FCVAR_CHEAT, "Prevents AI from crouching when they hold position");
#endif

// ------------------------------------------------------------------------------------------------ //
// CASWPlayerAnimState declaration.
// ------------------------------------------------------------------------------------------------ //

class CASWPlayerAnimState : public CBasePlayerAnimState, public IASWPlayerAnimState
{
public:
	DECLARE_CLASS( CASWPlayerAnimState, CBasePlayerAnimState );
	friend IASWPlayerAnimState* CreatePlayerAnimState( CBaseAnimatingOverlay *pEntity, IASWPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences );

	CASWPlayerAnimState();

	virtual Activity TranslateActivity(Activity actDesired);
	virtual void DoAnimationEvent( PlayerAnimEvent_t event );
	virtual bool DoAnimationEventForMiscLayer( PlayerAnimEvent_t event );
	virtual int CalcAimLayerSequence( float *flCycle, float *flAimSequenceWeight, bool bForceIdle );
	virtual void ClearAnimationState();
	virtual bool CanThePlayerMove();
	virtual float GetCurrentMaxGroundSpeed();
	virtual Activity CalcMainActivity();
	virtual bool ShouldResetMainSequence( int iCurrentSequence, int iNewSequence );
	virtual void UpdateActivityModifiers();
	virtual void AddActivityModifier( const char *szName );
	virtual int SelectWeightedSequence( Activity activity );
	virtual void DebugShowAnimState( int iStartLine );
	virtual void ComputeSequences( CStudioHdr *pStudioHdr );
	virtual void ClearAnimationLayers();
	virtual bool ShouldResetGroundSpeed( Activity oldActivity, Activity idealActivity );

	virtual void GetOuterAbsVelocity( Vector& vel ) const;
	virtual void ComputePoseParam_BodyPitch(CStudioHdr *pStudioHdr);
	virtual void ComputePoseParam_BodyYaw();
	virtual float SetOuterBodyYaw( float flValue );	
	virtual int	ConvergeAngles( float goal,float maxrate, float maxgap, float dt, float& current );

	void InitASW( CBaseAnimatingOverlay *pPlayer, IASWPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences );
	virtual bool IsAnimatingReload();
	virtual bool IsDoingEmoteGesture();
	virtual bool IsAnimatingJump();
	bool IsCarryingSuitcase();

	virtual	float GetFeetYawRate( void ) { return asw_feetyawrate.GetFloat(); }
	virtual float CalcMovementPlaybackRate( bool *bIsMoving );

	virtual int GetMiscSequence() { return m_iMiscSequence; }
	virtual float GetMiscCycle() { return m_flMiscCycle; }

	CASW_Marine *GetMarine() { return m_pMarine; }

	virtual void SetMiscPlaybackRate( float flRate ) { m_fMiscPlaybackRate = flRate; };

	// for events that happened in the past, re-pose the model appropriately
	virtual void MiscCycleRewind( float flCycle );
	virtual void MiscCycleUnrewind();

protected:

	int CalcFireLayerSequence(PlayerAnimEvent_t event);
	void ComputeFireSequence();

	void ComputeReloadSequence();
	int CalcReloadLayerSequence();
	int CalcReloadSucceedLayerSequence();
	int CalcReloadFailLayerSequence();

	void ComputeWeaponSwitchSequence();
	int CalcWeaponSwitchLayerSequence();

	void ComputeMiscSequence();

	const char* GetWeaponSuffix();
	bool HandleJumping();

	void UpdateLayerSequenceGeneric( int iLayer, bool &bEnabled, float &flCurCycle,
									int &iSequence, bool bWaitAtEnd,
									float fBlendIn=0.15f, float fBlendOut=0.15f, bool bMoveBlend = false, 
									float fPlaybackRate=1.0f, bool bUpdateCycle = true );

	CUtlVector<CUtlSymbol> m_ActivityModifiers;

private:

	CASW_Marine *m_pMarine;

	// Current state variables.
	bool m_bJumping;			// Set on a jump event.
	float m_flJumpStartTime;
	bool m_bFirstJumpFrame;

	// Aim sequence plays reload while this is on.
	bool m_bReloading;
	float m_flReloadCycle;
	int m_iReloadSequence;
	float m_flReloadBlendOut, m_flReloadBlendIn;
	float m_fReloadPlaybackRate;

	bool m_bWeaponSwitching;
	float m_flWeaponSwitchCycle;
	int m_iWeaponSwitchSequence;

	bool m_bPlayingMisc;
	float m_flMiscCycle, m_flMiscBlendOut, m_flMiscBlendIn;
	int m_iMiscSequence;
	bool m_bMiscOnlyWhenStill;
	bool m_bMiscNoOverride;
	float m_fMiscPlaybackRate;
	bool m_bMiscCycleRewound;
	float m_flMiscRewindCycle;
	// This is set to true if ANY animation is being played in the fire layer.
	bool m_bFiring;						// If this is on, then it'll continue the fire animation in the fire layer
										// until it completes.
	int m_iFireSequence;				// (For any sequences in the fire layer, including grenade throw).
	float m_flFireCycle;
	bool m_bPlayingEmoteGesture;

	IASWPlayerAnimStateHelpers *m_pHelpers;

	Vector2D			m_vLastPrevMovePose;
	Vector2D			m_vLastNextMovePose;
	float			m_flLastD;
};


IASWPlayerAnimState* CreatePlayerAnimState( CBaseAnimatingOverlay *pEntity, IASWPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences )
{
	CASWPlayerAnimState *pRet = new CASWPlayerAnimState;
	pRet->InitASW( pEntity, pHelpers, legAnimType, bUseAimSequences );
	return pRet;
}



// ------------------------------------------------------------------------------------------------ //
// CASWPlayerAnimState implementation.
// ------------------------------------------------------------------------------------------------ //

CASWPlayerAnimState::CASWPlayerAnimState()
{
	m_pOuter = NULL;
	m_pMarine = NULL;
	m_bReloading = false;
	m_bPlayingEmoteGesture = false;
	m_bWeaponSwitching = false;
	m_flMiscBlendOut = 0.1f;
	m_flMiscBlendIn = 0.1f;
	m_fReloadPlaybackRate = 1.0f;
	m_flReloadBlendOut = 0.1f;
	m_flReloadBlendIn = 0.1f;
	m_fMiscPlaybackRate = 1.0f;
	m_flMiscCycle = 0.0f;
	m_bMiscCycleRewound = false;
}


void CASWPlayerAnimState::InitASW( CBaseAnimatingOverlay *pEntity, IASWPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences )
{
	CModAnimConfig config;
	config.m_flMaxBodyYawDegrees = asw_max_body_yaw.GetFloat();
	config.m_LegAnimType = legAnimType;
	config.m_bUseAimSequences = bUseAimSequences;

	m_pHelpers = pHelpers;
	m_pMarine = CASW_Marine::AsMarine( pEntity );
	
	BaseClass::Init( pEntity, config );
}


void CASWPlayerAnimState::ClearAnimationState()
{
	m_bJumping = false;
	m_bFiring = false;
	m_bReloading = false;
	m_bPlayingEmoteGesture = false;
	m_bWeaponSwitching = false;
	m_bPlayingMisc = false;
	BaseClass::ClearAnimationState();
}


void CASWPlayerAnimState::DoAnimationEvent( PlayerAnimEvent_t event )
{
	if ( event == PLAYERANIMEVENT_FIRE_GUN_PRIMARY || 
		 event == PLAYERANIMEVENT_FIRE_GUN_SECONDARY )
	{
		// Regardless of what we're doing in the fire layer, restart it.
		m_flFireCycle = 0;
		m_iFireSequence = CalcFireLayerSequence( event );
		m_bFiring = m_iFireSequence != -1;
	}
	else if ( event == PLAYERANIMEVENT_JUMP )
	{
		// Play the jump animation.
		if (!m_bJumping)
		{
			m_bJumping = true;
			m_bFirstJumpFrame = true;
			m_flJumpStartTime = gpGlobals->curtime;
		}
	}
	else if ( event == PLAYERANIMEVENT_RELOAD )
	{
		m_iReloadSequence = CalcReloadLayerSequence();
		if ( m_iReloadSequence != -1 )
		{
			// clear other events that might be playing in our layer
			m_bWeaponSwitching = false;
			m_bPlayingEmoteGesture = false;
			bool bShortReloadAnim = false;

#ifdef CLIENT_DLL
			C_ASW_Marine* pMarine = dynamic_cast<C_ASW_Marine*>(GetOuter());
#else
			CASW_Marine* pMarine = dynamic_cast<CASW_Marine*>(GetOuter());
#endif			
			float fReloadTime = 2.2f;
			CASW_Weapon* pActiveWeapon = pMarine->GetActiveASWWeapon();
			if (pActiveWeapon && pActiveWeapon->GetWeaponInfo())
			{
				fReloadTime = pActiveWeapon->GetWeaponInfo()->flReloadTime;

				if (pActiveWeapon->ASW_SelectWeaponActivity(ACT_RELOAD) == ACT_RELOAD_PISTOL)
					bShortReloadAnim = true;
			}
			if (pMarine)
				fReloadTime *= MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_RELOADING);
						
			// calc playback rate needed to play the whole anim in this time
			// normal weapon reload anim is 48fps and 105 frames = 2.1875 seconds
			// normal weapon reload time in script is 2.2 seconds
			// pistol weapon reload anim is 38fps and 38 frames = 1 second
			// pistol weapon reload time in script is 1.0 seconds
			if (bShortReloadAnim)
				m_fReloadPlaybackRate = 1.0f / fReloadTime; //38.0f / (fReloadTime * 38.0f);
			else
				m_fReloadPlaybackRate = 105.6f / (fReloadTime * 48.0f);

			m_bReloading = true;			
			m_flReloadCycle = 0;
		}
	}
	else
	{
		DoAnimationEventForMiscLayer( event );
	}
	//else if (m_bMiscNoOverride && m_bPlayingMisc)
	//{
		// can't play the requested anim since we're in the middle of one that can't be overridden
	//}
}

bool CASWPlayerAnimState::DoAnimationEventForMiscLayer( PlayerAnimEvent_t event )
{
#ifdef CLIENT_DLL
	C_ASW_Marine* pMarine = dynamic_cast<C_ASW_Marine*>(GetOuter());
#else
	CASW_Marine* pMarine = dynamic_cast<CASW_Marine*>(GetOuter());
#endif		
	bool bHasMeleeAttack = ( pMarine && pMarine->GetCurrentMeleeAttack() );

	if ( event >= PLAYERANIMEVENT_MELEE && event <= PLAYERANIMEVENT_MELEE_LAST )
	{
		CASW_Melee_Attack *pAttack = ASWMeleeSystem()->GetMeleeAttackByID( event - PLAYERANIMEVENT_MELEE + 1 );

		m_flMiscBlendOut = 0.1f;
		m_flMiscBlendIn = 0.0f;

		if ( pAttack )
		{
			m_iMiscSequence = m_pOuter->LookupSequence( pAttack->m_szSequenceName );
			m_flMiscBlendIn = pAttack->m_flBlendIn;
			m_flMiscBlendOut = pAttack->m_flBlendOut;
		}
		else
		{
			m_iMiscSequence = m_pOuter->LookupSequence( "punch" );
		}

		m_bPlayingMisc = true;
		m_bReloading = false;
		m_bPlayingEmoteGesture = false;
		m_flMiscCycle = 0;
		m_bMiscOnlyWhenStill = false;
		m_bMiscNoOverride = true;
		m_fMiscPlaybackRate = 1.0f;

		return true;
	}

	// don't interrupt melee animations with other misc layer animations
	if ( bHasMeleeAttack )
	{
		return true;
	}

	if ( event == PLAYERANIMEVENT_WEAPON_SWITCH )
	{
		// weapon change actually interrupts flare throws, so we allow cancelling this special anim
		if (m_bPlayingMisc && m_iMiscSequence == m_pOuter->LookupSequence( "grenade_roll_trim" ))
			m_bMiscNoOverride = false;
		if (!m_bMiscNoOverride || !m_bPlayingMisc)	// weapon switch anim is low priority, so don't play it if we're in the middle of a more interesting animation
		{
			if (!(m_bPlayingMisc && m_iMiscSequence == m_pOuter->LookupSequence( "pickup" )))	// don't let weapon switch override pickup anim
			{
				m_iWeaponSwitchSequence = CalcWeaponSwitchLayerSequence();
				if ( m_iWeaponSwitchSequence != -1 )
				{
					// clear other events that might be playing in our layer
					m_bPlayingMisc = false;
					m_bReloading = false;
					m_bPlayingEmoteGesture = false;

					m_bWeaponSwitching = true;
					m_flWeaponSwitchCycle = 0;
					m_flMiscBlendOut = 0.1f;
					m_flMiscBlendIn = 0.1f;
					m_bMiscNoOverride = false;
				}
			}
		}
	}
	else if ( event == PLAYERANIMEVENT_FLINCH )
	{
		m_iMiscSequence = SelectWeightedSequence( ACT_SMALL_FLINCH );
		if ( m_iMiscSequence != -1 && !m_bReloading && !m_bPlayingMisc	)	// we don't flinch if we're in the middle of reloading/switching
		{
			m_bPlayingMisc = true;
			m_flMiscCycle = 0;
			m_flMiscBlendOut = 0.1f;
			m_flMiscBlendIn = 0.1f;
			m_bMiscOnlyWhenStill = false;
			m_bMiscNoOverride = false;
			m_fMiscPlaybackRate = 2.5f;
		}
		return true;
	}
	else if ( event == PLAYERANIMEVENT_GO )
	{
		m_iMiscSequence = m_pOuter->LookupSequence( "signal_advance" );
		if ( m_iMiscSequence != -1 && !m_bReloading && !m_bPlayingMisc	)	// we don't play anim if we're in the middle of reloading/switching
		{
			m_bPlayingMisc = true;
			m_flMiscCycle = 0;
			m_flMiscBlendOut = 0.275f;
			m_flMiscBlendIn = 0.1f;
			m_bMiscOnlyWhenStill = false;
			m_bMiscNoOverride = false;
			m_fMiscPlaybackRate = 1.0f;
			m_bPlayingEmoteGesture = true;
		}
		return true;
	}
	else if ( event == PLAYERANIMEVENT_HALT )
	{
		m_iMiscSequence = m_pOuter->LookupSequence( "signal_halt" );
		if ( m_iMiscSequence != -1 && !m_bReloading && !m_bPlayingMisc	)	// we don't play anim if we're in the middle of reloading/switching
		{
			m_bPlayingMisc = true;
			m_flMiscCycle = 0;
			m_flMiscBlendOut = 0.15f;
			m_flMiscBlendIn = 0.1f;
			m_bMiscOnlyWhenStill = false;
			m_bMiscNoOverride = false;
			m_fMiscPlaybackRate = 1.0f;
			m_bPlayingEmoteGesture = true;
		}
		return true;
	}
	else if ( event == PLAYERANIMEVENT_PICKUP )
	{
		m_iMiscSequence = m_pOuter->LookupSequence( "pickup" );
		if ( m_iMiscSequence != -1 && !m_bReloading && !m_bPlayingMisc	)
		{
			m_bPlayingMisc = true;
			m_flMiscCycle = 0;
			m_flMiscBlendOut = 0.15f;
			m_flMiscBlendIn = 0.1f;
			m_bMiscOnlyWhenStill = false;
			m_bMiscNoOverride = false;
			m_fMiscPlaybackRate = 1.0f;
			m_bPlayingEmoteGesture = true;
		}
		return true;
	}
	else if ( event == PLAYERANIMEVENT_HEAL )
	{
		m_iMiscSequence = m_pOuter->LookupSequence( "heal" );
		if ( m_iMiscSequence != -1 && !m_bReloading && !m_bPlayingMisc	)
		{
			m_bPlayingMisc = true;
			m_flMiscCycle = 0;
			m_flMiscBlendOut = 0.15f;
			m_flMiscBlendIn = 0.1f;
			m_bMiscOnlyWhenStill = false;
			m_bMiscNoOverride = false;
			m_fMiscPlaybackRate = 1.0f;
			m_bPlayingEmoteGesture = true;
		}
		return true;
	}
	else if ( event == PLAYERANIMEVENT_KICK || event == PLAYERANIMEVENT_PUNCH )
	{
		if ( event == PLAYERANIMEVENT_KICK )
		{
			m_iMiscSequence = m_pOuter->LookupSequence( "melee_kick" );
		}
		else
		{
			m_iMiscSequence = m_pOuter->LookupSequence( "melee_punch" );
		}
		if ( m_iMiscSequence != -1	)
		{
			m_bPlayingMisc = true;
			m_bReloading = false;
			m_flMiscCycle = 0;
			m_flMiscBlendOut = 0.15f;
			m_flMiscBlendIn = 0.1f;
			m_bMiscOnlyWhenStill = false;
			m_bMiscNoOverride = true;
			m_bPlayingEmoteGesture = false;
			m_fMiscPlaybackRate = 1.0f;
#ifdef CLIENT_DLL
			C_ASW_Marine* pMarine = dynamic_cast<C_ASW_Marine*>(GetOuter());
#else
			CASW_Marine* pMarine = dynamic_cast<CASW_Marine*>(GetOuter());
#endif		
			if (pMarine)
				m_fMiscPlaybackRate *= MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_MELEE, ASW_MARINE_SUBSKILL_MELEE_SPEED);
		}
		return true;
	}
	else if ( event == PLAYERANIMEVENT_BAYONET )
	{
		m_iMiscSequence = m_pOuter->LookupSequence( "bayonet_stab" );
		if ( m_iMiscSequence != -1 )
		{
			m_bPlayingMisc = true;
			m_bReloading = false;
			m_flMiscCycle = 0;
			m_flMiscBlendOut = 0.15f;
			m_flMiscBlendIn = 0.1f;
			m_bMiscOnlyWhenStill = false;
			m_bMiscNoOverride = true;
			m_fMiscPlaybackRate = 1.0f;
			m_bPlayingEmoteGesture = false;
		}
		return true;
	}
	else if ( event == PLAYERANIMEVENT_GETUP )
	{
		m_iMiscSequence = m_pOuter->LookupSequence( "StandB_1" );
		if ( m_iMiscSequence != -1 )
		{
			m_bPlayingMisc = true;
			m_flMiscCycle = 0;
			m_flMiscBlendOut = 0.15f;
			m_flMiscBlendIn = 0;
			m_bMiscOnlyWhenStill = false;
			m_bMiscNoOverride = true;
			m_fMiscPlaybackRate = 1.0f;
			m_bPlayingEmoteGesture = false;
		}
		return true;
	}
	else if ( event == PLAYERANIMEVENT_THROW_GRENADE )
	{
		m_iMiscSequence = m_pOuter->LookupSequence( "grenade_roll_trim" );
		if ( m_iMiscSequence != -1 )
		{
			m_bPlayingMisc = true;
			m_flMiscCycle = 0;
			m_flMiscBlendOut = 0.15f;
			m_flMiscBlendIn = 0.15f;
			m_bMiscOnlyWhenStill = false;
			m_bMiscNoOverride = true;
			m_bPlayingEmoteGesture = true;
			m_fMiscPlaybackRate = asw_flare_anim_speed.GetFloat();
		}
		return true;
	}
	return false;
}

int CASWPlayerAnimState::CalcWeaponSwitchLayerSequence()
{
	// single weapon switch anim
	int iWeaponSwitchSequence = m_pOuter->LookupSequence( "WeaponSwitch_smg1" );
	return iWeaponSwitchSequence;
}

int CASWPlayerAnimState::CalcReloadLayerSequence()
{	
#ifdef ASW_TRANSLATE_ACTIVITIES_BY_WEAPON

	Activity idealActivity = ACT_RELOAD;
#ifdef CLIENT_DLL
	C_ASW_Marine* pMarine = dynamic_cast<C_ASW_Marine*>(GetOuter());
#else
	CASW_Marine* pMarine = dynamic_cast<CASW_Marine*>(GetOuter());
#endif
	bool bHasWeapon = (pMarine && pMarine->GetActiveASWWeapon());
	// allow our weapon to decide on the reload sequence
	if (bHasWeapon)
	{
		idealActivity = (Activity) pMarine->GetActiveASWWeapon()->ASW_SelectWeaponActivity(idealActivity);
	}
	return SelectWeightedSequence(idealActivity);

#else
	return GetOuter()->SelectWeightedSequenceFromModifiers( ACT_RELOAD, m_ActivityModifiers.Base(), m_ActivityModifiers.Count() );
#endif
}

int CASWPlayerAnimState::CalcReloadSucceedLayerSequence()
{	
	return GetOuter()->SelectWeightedSequenceFromModifiers( ACT_RELOAD_SUCCEED, m_ActivityModifiers.Base(), m_ActivityModifiers.Count() );
}

int CASWPlayerAnimState::CalcReloadFailLayerSequence()
{	
	return GetOuter()->SelectWeightedSequenceFromModifiers( ACT_RELOAD_FAIL, m_ActivityModifiers.Base(), m_ActivityModifiers.Count() );
}

void CASWPlayerAnimState::MiscCycleRewind( float flCycle )
{
	CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(GetOuter());
	CASW_Weapon *pWeapon = pMarine ? pMarine->GetActiveASWWeapon() : NULL;

	Assert( pMarine );

	bool bMiscEnabled = true;
	float flMiscCycleRewind = flCycle;
	UpdateLayerSequenceGeneric( RELOADSEQUENCE_LAYER, bMiscEnabled, flMiscCycleRewind, m_iMiscSequence, false, m_flMiscBlendIn, m_flMiscBlendOut, m_bMiscOnlyWhenStill, m_fMiscPlaybackRate, false );
	m_bMiscCycleRewound = true;
	pMarine->InvalidateBoneCache();

	if ( pWeapon )
	{
		pWeapon->InvalidateBoneCache();
	}
#ifdef GAME_DLL
	if ( asw_melee_debug.GetBool() )
	{
		Msg ( "%d: Rewind to %f!\n", gpGlobals->framecount, flCycle );
		GetOuter()->DrawServerHitboxes( 10.0f );
	}
#endif
}

void CASWPlayerAnimState::MiscCycleUnrewind()
{
	// Just let the next UpdateLayerSequenceGeneric() generate the proper pose
	m_bMiscCycleRewound = false;
}

void CASWPlayerAnimState::UpdateLayerSequenceGeneric( int iLayer, bool &bEnabled,
					float &flCurCycle, int &iSequence, bool bWaitAtEnd,
					float fBlendIn, float fBlendOut, bool bMoveBlend, float fPlaybackRate, bool bUpdateCycle /* = true */ )
{
	if ( !bEnabled )
		return;

	CStudioHdr *hdr = GetOuter()->GetModelPtr();
	if ( !hdr )
		return;

	if ( iSequence < 0 || iSequence >= hdr->GetNumSeq() )
		return;

	// Increment the fire sequence's cycle.
	if ( bUpdateCycle )
	{
		flCurCycle += m_pOuter->GetSequenceCycleRate( hdr, iSequence ) * gpGlobals->frametime * fPlaybackRate;
	}

	// temp: if the sequence is looping, don't override it - we need better handling of looping anims, 
	//  especially in misc layer from melee (right now the same melee attack is looped manually in asw_melee_system.cpp)
	bool bLooping = m_pOuter->IsSequenceLooping( hdr, iSequence );

	if ( flCurCycle > 1 && !bLooping )
	{
		if ( iLayer == RELOADSEQUENCE_LAYER )
		{
			m_bPlayingEmoteGesture = false;
			m_bReloading = false;
		}
		if ( bWaitAtEnd )
		{
			flCurCycle = 1;
		}
		else
		{
			// Not firing anymore.
			bEnabled = false;
			iSequence = 0;
			return;
		}
	}

	// if this animation should blend out as we move, then check for dropping it completely since we're moving too fast
	float speed = 0;
	if (bMoveBlend)
	{
		Vector vel;
		GetOuterAbsVelocity( vel );

		float speed = vel.Length2D();

		if (speed > 50)
		{
			bEnabled = false;
			iSequence = 0;
			return;
		}
	}

	// Now dump the state into its animation layer.
	CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iLayer );

	pLayer->SetCycle( flCurCycle );
	pLayer->SetSequence( iSequence );

	pLayer->SetPlaybackRate( fPlaybackRate );
	pLayer->SetWeight( 1.0f );
	if (iLayer == RELOADSEQUENCE_LAYER)
	{
		// blend this layer in and out for smooth reloading
		if (flCurCycle < fBlendIn && fBlendIn>0)
		{
			pLayer->SetWeight( clamp<float>(flCurCycle / fBlendIn,
				0.001f, 1.0f) );
		}
		else if (flCurCycle >= (1.0f - fBlendOut) && fBlendOut>0)
		{
			pLayer->SetWeight( clamp<float>((1.0f - flCurCycle) / fBlendOut,
				0.001f, 1.0f) );
		}
		else
		{
			pLayer->SetWeight( 1.0f );
		}
	}
	else
	{
		pLayer->SetWeight( 1.0f );			
	}
	if (bMoveBlend)		
	{
		// blend the animation out as we move faster
		if (speed <= 50)			
			pLayer->SetWeight( pLayer->GetWeight() * (50.0f - speed) / 50.0f );
	}

#ifndef CLIENT_DLL
	pLayer->m_fFlags |= ANIM_LAYER_ACTIVE;
#endif
	pLayer->SetOrder( iLayer );
}

void CASWPlayerAnimState::ComputeFireSequence()
{
#ifdef CLIENT_DLL
	UpdateLayerSequenceGeneric( FIRESEQUENCE_LAYER, m_bFiring, m_flFireCycle, m_iFireSequence, false );
#else
	// Server doesn't bother with different fire sequences.
#endif
}

void CASWPlayerAnimState::ComputeReloadSequence()
{
#ifdef CLIENT_DLL
	UpdateLayerSequenceGeneric( RELOADSEQUENCE_LAYER, m_bReloading, m_flReloadCycle, m_iReloadSequence, false, m_flReloadBlendIn, m_flReloadBlendOut, false, m_fReloadPlaybackRate );
#else
	// Server doesn't bother with different fire sequences.
#endif
}

void CASWPlayerAnimState::ComputeWeaponSwitchSequence()
{
#ifdef CLIENT_DLL
	UpdateLayerSequenceGeneric( RELOADSEQUENCE_LAYER, m_bWeaponSwitching, m_flWeaponSwitchCycle, m_iWeaponSwitchSequence, false, 0, 0.5f );
#else
	// Server doesn't bother with different fire sequences.
#endif
}

// does misc gestures if we're not firing
void CASWPlayerAnimState::ComputeMiscSequence()
{
	// Run melee anims on the server so we can get bone positions for damage traces, etc
	CASW_Marine* pMarine = dynamic_cast<CASW_Marine*>(GetOuter());
	
// 	bool bFiring = pMarine && pMarine->GetActiveASWWeapon() && pMarine->GetActiveASWWeapon()->IsFiring() && !pMarine->GetActiveASWWeapon()->ASWIsMeleeWeapon();
// 	if (m_bPlayingMisc && bFiring)
// 	{
// 		m_bPlayingMisc = false;
// 	}

	CASW_Melee_Attack *pAttack = pMarine ? pMarine->GetCurrentMeleeAttack() : NULL;
	bool bHoldAtEnd = pAttack ? pAttack->m_bHoldAtEnd : false;

	// If this is hit it means MiscCycleRewind() was called, and then ComputeMiscSequence() was called before MiscCycleUnrewind()
	//  Add a call to MiscCycleUnrewind() so that the cycle update in UpdateLayerSequenceGeneric() is correct 
	Assert( !m_bMiscCycleRewound );

	UpdateLayerSequenceGeneric( RELOADSEQUENCE_LAYER, m_bPlayingMisc, m_flMiscCycle, m_iMiscSequence, bHoldAtEnd, m_flMiscBlendIn, m_flMiscBlendOut, m_bMiscOnlyWhenStill, m_fMiscPlaybackRate );	
}

bool CASWPlayerAnimState::ShouldResetGroundSpeed( Activity oldActivity, Activity idealActivity )
{
	// If we went from idle to walk, reset the interpolation history.
	return ( (oldActivity == ACT_CROUCHIDLE || oldActivity == ACT_IDLE || oldActivity == ACT_IDLE_RELAXED
		|| oldActivity == ACT_MP_CROUCHWALK_ITEM1 || oldActivity == ACT_MP_STAND_ITEM1) && 
		(idealActivity == ACT_WALK || idealActivity == ACT_RUN_CROUCH || idealActivity == ACT_WALK_AIM_RIFLE
		|| idealActivity == ACT_WALK_AIM_PISTOL || idealActivity == ACT_RUN || idealActivity == ACT_RUN_AIM_RIFLE
		|| idealActivity == ACT_RUN_AIM_PISTOL || idealActivity == ACT_RUN_RELAXED || idealActivity == ACT_WALK_RELAXED
		|| idealActivity == ACT_MP_RUN_ITEM1 || idealActivity == ACT_MP_WALK_ITEM1) );
}


int CASWPlayerAnimState::CalcAimLayerSequence( float *flCycle, float *flAimSequenceWeight, bool bForceIdle )
{
	return m_pOuter->LookupSequence( "run_aiming_all" );
}


const char* CASWPlayerAnimState::GetWeaponSuffix()
{
	return 0;
}


int CASWPlayerAnimState::CalcFireLayerSequence(PlayerAnimEvent_t event)
{
	return SelectWeightedSequence( TranslateActivity(ACT_RANGE_ATTACK1) );
}


bool CASWPlayerAnimState::CanThePlayerMove()
{
	return m_pHelpers->ASWAnim_CanMove();
}

float CASWPlayerAnimState::CalcMovementPlaybackRate( bool *bIsMoving )
{
	if ( asw_fixed_movement_playback_rate.GetBool() )
	{
		return 1.0f;
	}
	return BaseClass::CalcMovementPlaybackRate( bIsMoving );
}

// NOTE: CalcMovementPlaybackRate compares the current speed of the marine entity to this value
//   and sets the playbackrate accordingly.  i.e. if marine is moving at the same speed as this,
//   the playback rate will be 1.  If marine is actually moving at half this speed, then playback
//   rate will be 0.5.
float CASWPlayerAnimState::GetCurrentMaxGroundSpeed()
{
	// find the speed of currently playing directional sequences when full blended in
	CStudioHdr *pStudioHdr = GetOuter()->GetModelPtr();

	if ( pStudioHdr == NULL )
		return 1.0f;

	int iMoveX = GetOuter()->LookupPoseParameter( pStudioHdr, "move_x" );
	int iMoveY = GetOuter()->LookupPoseParameter( pStudioHdr, "move_y" );
	if ( iMoveX < 0 || iMoveY < 0 )
			return 1.0f;

	//float prevX = m_vLastMovePose.x; //GetOuter()->GetPoseParameter( iMoveX );
	//float prevY = m_vLastMovePose.y; //GetOuter()->GetPoseParameter( iMoveY );
	float prevX = GetOuter()->GetPoseParameter( iMoveX );
	float prevY = GetOuter()->GetPoseParameter( iMoveY );

	float d = sqrt( prevX * prevX + prevY * prevY );
	float newX, newY;
	if ( d == 0.0 )
	{ 
		newX = 1.0;
		newY = 0.0;
	}
	else
	{
		newX = prevX / d;
		newY = prevY / d;
	}	

	m_vLastPrevMovePose.x = prevX;
	m_vLastPrevMovePose.y = prevY;
	m_vLastNextMovePose.x = newX;
	m_vLastNextMovePose.y = newY;
	m_flLastD = d;

	GetOuter()->SetPoseParameter( pStudioHdr, iMoveX, newX );
	GetOuter()->SetPoseParameter( pStudioHdr, iMoveY, newY );

	float flSequenceSpeed = GetOuter()->GetSequenceGroundSpeed( GetOuter()->GetSequence() );

	GetOuter()->SetPoseParameter( pStudioHdr, iMoveX, prevX );		// restore pose params
	GetOuter()->SetPoseParameter( pStudioHdr, iMoveY, prevY );

	//if ( flSequenceSpeed > 0 )
	{
#ifdef CLIENT_DLL
		//Msg( "GetCurrentMaxGroundSpeed: d = %f new_x = %f prevX = %f flSequenceSpeed = %f seq = %d iMoveX = %d iMoveY = %d\n",
			//d, newX, prevX, flSequenceSpeed, GetOuter()->GetSequence(), iMoveX, iMoveY );
#endif
	}

	if (asw_marine_speed_type.GetInt() == 2)	// just using the sequence speed
	{
		float flOuterSpeed = GetOuterXYSpeed();
		if (flOuterSpeed < ASW_WALK_POINT)
		{
			if (flSequenceSpeed > asw_walk_sequence_speed_cap.GetFloat())
				flSequenceSpeed = asw_walk_sequence_speed_cap.GetFloat();
			return flSequenceSpeed * asw_walk_speed_scale.GetFloat();
		}
		if (flSequenceSpeed > asw_sequence_speed_cap.GetFloat())
			flSequenceSpeed = asw_sequence_speed_cap.GetFloat();
		return flSequenceSpeed * asw_run_speed_scale.GetFloat();
	}

	if (asw_marine_speed_type.GetInt() == 1)	// just using fixed points
	{
		float flOuterSpeed = GetOuterXYSpeed();
		if (flOuterSpeed < ASW_WALK_POINT)
		{
			return ASW_WALK_SPEED;
		}
		return ASW_RUN_SPEED ;
	}

	// blend between fixed points and the sequence speed
	float flOuterSpeed = GetOuterXYSpeed();
	if (flOuterSpeed < ASW_WALK_POINT)
	{		
		return (ASW_WALK_SPEED * asw_walk_fixed_blend.GetFloat())
			+ (flSequenceSpeed * asw_walk_speed_scale.GetFloat() * (1.0f - asw_walk_fixed_blend.GetFloat()) );
	}
	return (ASW_RUN_SPEED * asw_run_fixed_blend.GetFloat())
			+ (flSequenceSpeed * asw_run_speed_scale.GetFloat() * (1.0f - asw_run_fixed_blend.GetFloat()) );
}


bool CASWPlayerAnimState::HandleJumping()
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
		if (m_flJumpStartTime > gpGlobals->curtime)
			m_flJumpStartTime = gpGlobals->curtime;
		if ( gpGlobals->curtime - m_flJumpStartTime > 0.2f)
		{
			if ( m_pOuter->GetFlags() & FL_ONGROUND || GetOuter()->GetGroundEntity() != NULL)
			{
				m_bJumping = false;
				RestartMainSequence();	// Reset the animation.				
			}
		}
	}

	// Are we still jumping? If so, keep playing the jump animation.
	return m_bJumping;
}

bool CASWPlayerAnimState::IsCarryingSuitcase()
{
	C_ASW_Marine* pMarine = dynamic_cast<C_ASW_Marine*>(GetOuter());
	if (pMarine)
	{
		C_ASW_Weapon_Ammo_Bag *pAmmoBag = dynamic_cast<C_ASW_Weapon_Ammo_Bag*>(pMarine->GetActiveASWWeapon());
		if (pAmmoBag)
		{
			return true;
		}
	}
	return false;
}

extern CUtlSymbolTable g_ActivityModifiersTable;

void CASWPlayerAnimState::AddActivityModifier( const char *szName )
{
	CUtlSymbol sym = g_ActivityModifiersTable.Find( szName );
	if ( !sym.IsValid() )
	{
		sym = g_ActivityModifiersTable.AddString( szName );
	}
	m_ActivityModifiers.AddToTail( sym );
}

void CASWPlayerAnimState::UpdateActivityModifiers()
{
	m_ActivityModifiers.Purge();
	CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>( GetOuter() );
	if ( !pMarine )
		return;	

	// TODO: If the activity modifier isn't found in the activity modifiers table, then add it to the table?  Otherwise debug output of activity modifiers will be missing entries that don't exist in the mdl
	CASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();
	if ( pWeapon )
	{
		if ( pWeapon->IsFiring() )
		{
			AddActivityModifier( "Firing" );
		}

		AddActivityModifier( pWeapon->GetWeaponInfo()->szAnimationPrefix );
	}	
}
bool CASWPlayerAnimState::ShouldResetMainSequence( int iCurrentSequence, int iNewSequence )
{
	return iCurrentSequence != iNewSequence;
}

Activity CASWPlayerAnimState::CalcMainActivity()
{
	UpdateActivityModifiers();

	CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(GetOuter());
	if ( !pMarine )
		return ACT_IDLE;

	float flOuterSpeed = GetOuterXYSpeed();

	if ( HandleJumping() )
	{
		return ACT_JUMP;
	}
	else
	{
		Activity idealActivity = ACT_IDLE;

		// asw - commented out crouching for now
		//if ( m_pOuter->GetFlags() & FL_DUCKING )
		//{
			//if ( flOuterSpeed > 0.1f )
				//idealActivity = ACT_RUN_CROUCH;
			//else
				//idealActivity = ACT_CROUCHIDLE;
		//}
		//else
		{
			if ( flOuterSpeed > 0.1f )
			{
				if ( flOuterSpeed <= ASW_WALK_POINT 
					//&& ( pMarine->m_bWalking.Get() ) //|| IsAnimatingReload() || pMarine->IsFiring() )
					)
				{
					idealActivity = ACT_WALK;	
				}
				else
				{
					idealActivity = ACT_RUN;
				}
			}
			else
			{
				// TODO: Move CROUCHIDLE to activity modifiers, if we want to keep this AI behavior
				if (pMarine && pMarine->GetASWOrders() == ASW_ORDER_HOLD_POSITION && !pMarine->IsInhabited())
				{
#ifdef CLIENT_DLL
					if (asw_prevent_ai_crouch.GetBool() && ASWGameResource())
					{
						C_ASW_Game_Resource *pGameResource = ASWGameResource();
						int iMarineResourceIndex = -1;
						for (int m=0;m<pGameResource->GetMaxMarineResources();m++)
						{
							if (pGameResource->GetMarineResource(m) && pGameResource->GetMarineResource(m)->GetMarineEntity() == GetOuter())
							{
								iMarineResourceIndex = m;
								break;
							}
						}

						// find which marine info is ours
						if (iMarineResourceIndex!=-1 && ((1<<iMarineResourceIndex) & asw_prevent_ai_crouch.GetInt()))
						{
							// leave anim as standing idle
						}
						else
						{
							idealActivity = ACT_CROUCHIDLE;
						}
					}
					else
					{
						idealActivity = ACT_CROUCHIDLE;
					}
#else
					idealActivity = ACT_CROUCHIDLE;
#endif
				}
				else if ( pMarine->m_bWalking.Get() || ( !pMarine->IsInhabited() && pMarine->m_bAICrouch.Get() ) )
				{
					idealActivity = ACT_CROUCHIDLE;
				}
			}
		}
		return idealActivity;
	}
}


void CASWPlayerAnimState::DebugShowAnimState( int iStartLine )
{
	engine->Con_NPrintf( iStartLine++, "fire  : %s, cycle: %.2f eyepitch:%f\n", m_bFiring ? GetSequenceName( m_pOuter->GetModelPtr(), m_iFireSequence ) : "[not firing]", m_flFireCycle, m_flEyePitch);
	Vector estimatedv(0,0,0), absv(0,0,0);	
#ifdef CLIENT_DLL
	GetOuter()->EstimateAbsVelocity(estimatedv);
#endif
	absv = GetOuter()->GetAbsVelocity();
	engine->Con_NPrintf( iStartLine++, "estv = %f absv = %f\n",
		VectorLength(estimatedv),
		VectorLength(absv));
	engine->Con_NPrintf( iStartLine++, "Clientonground = %d (%s) Flags: %d \n",
		( GetOuter()->GetFlags() & FL_ONGROUND ), GetOuter()->GetGroundEntity() ? GetOuter()->GetGroundEntity()->GetClassname() : "NULL", GetOuter()->GetFlags() );
	bool bMoving;
	//engine->Con_NPrintf( iStartLine++, "interpgroundspeed=%f calcmovementplaybackrate=%f\n", GetInterpolatedGroundSpeed(), CalcMovementPlaybackRate(&bMoving));
	engine->Con_NPrintf( iStartLine++, "calcmovementplaybackrate=%f\n", CalcMovementPlaybackRate(&bMoving));
	engine->Con_NPrintf( iStartLine++, "curmaxgroundspeed=%f\n", GetCurrentMaxGroundSpeed());	
	engine->Con_NPrintf( iStartLine++, "d=%f newX=%f newY=%f\n", m_flLastD, m_vLastNextMovePose.x, m_vLastNextMovePose.y);	
	engine->Con_NPrintf( iStartLine++, "prevX=%f prevY=%f\n", m_vLastPrevMovePose.x, m_vLastPrevMovePose.y);		

	engine->Con_NPrintf( iStartLine++, "PlayingMisc=%d\n", m_bPlayingMisc );		
	engine->Con_NPrintf( iStartLine++, "miscsequence=%d\n", m_iMiscSequence );		
	engine->Con_NPrintf( iStartLine++, "reloading=%d MiscOnlyWhenStill=%d MiscNoOverride=%d\n", m_bReloading, m_bMiscOnlyWhenStill, m_bMiscNoOverride );		
	engine->Con_NPrintf( iStartLine++, "MiscCycle=%f rate=%f\n", m_flMiscCycle, m_fMiscPlaybackRate );

	// show activity modifiers
	for ( int i = 0; i < m_ActivityModifiers.Count(); i++ )
	{
		engine->Con_NPrintf( iStartLine++, "ActivityModifier[%d] = %s\n", i, g_ActivityModifiersTable.String( m_ActivityModifiers[i] ) );
	}

	BaseClass::DebugShowAnimState( iStartLine );	

	// Draw a green triangle on the ground for the feet yaw.
	float flBaseSize = 10;
	float flHeight = 80;
	Vector vBasePos = GetOuter()->GetAbsOrigin() + Vector( 0, 0, 5 );
	QAngle angles( 0, 0, 0 );
	Vector vForward, vRight, vUp;
	angles[YAW] = m_flCurrentFeetYaw;
	AngleVectors( angles, &vForward, &vRight, &vUp );
	debugoverlay->AddTriangleOverlay( vBasePos+vRight*flBaseSize/2, vBasePos-vRight*flBaseSize/2, vBasePos+vForward*flHeight, 0, 255, 0, 255, false, 0.01 );
}


void CASWPlayerAnimState::ComputeSequences( CStudioHdr *pStudioHdr )
{
	BaseClass::ComputeSequences(pStudioHdr);

	ComputeFireSequence();
	ComputeMiscSequence();
	ComputeReloadSequence();
	ComputeWeaponSwitchSequence();	
}


void CASWPlayerAnimState::ClearAnimationLayers()
{
	if ( !m_pOuter )
		return;

	m_pOuter->SetNumAnimOverlays( NUM_LAYERS_WANTED );
	for ( int i=0; i < m_pOuter->GetNumAnimOverlays(); i++ )
	{
		m_pOuter->GetAnimOverlay( i )->SetOrder( CBaseAnimatingOverlay::MAX_OVERLAYS );
#ifndef CLIENT_DLL
		m_pOuter->GetAnimOverlay( i )->m_fFlags = 0;
#endif
	}
}

void CASWPlayerAnimState::GetOuterAbsVelocity( Vector& vel ) const
{
#if defined( CLIENT_DLL )
	C_ASW_Marine* pMarine = dynamic_cast<C_ASW_Marine*>(GetOuter());
	// for inhabited, predicted marines, just grab their actual velocity rather than
	//  relying on (the seemingly broken?) estimateabsvelocity
	if ( pMarine != NULL && pMarine->IsInhabited() && C_BasePlayer::IsLocalPlayer( pMarine->GetCommander() ) )
		vel = pMarine->GetAbsVelocity();
	else
		GetOuter()->EstimateAbsVelocity( vel );
#else
	vel = GetOuter()->GetAbsVelocity();
#endif
}

void CASWPlayerAnimState::ComputePoseParam_BodyYaw()
{
	VPROF( "CBasePlayerAnimState::ComputePoseParam_BodyYaw" );

	// Find out which way he's running (m_flEyeYaw is the way he's looking).
	Vector vel;
	GetOuterAbsVelocity( vel );
	bool bIsMoving = vel.Length2D() > MOVING_MINIMUM_SPEED;
	float flFeetYawRate = GetFeetYawRate();
	float flMaxGap = m_AnimConfig.m_flMaxBodyYawDegrees;

	if ( m_pMarine && m_pMarine->GetCurrentMeleeAttack() )		// don't allow upper body turning when melee attacking
	{
		flMaxGap = 0.0f;
	}

	// If we just initialized this guy (maybe he just came into the PVS), then immediately
	// set his feet in the right direction, otherwise they'll spin around from 0 to the 
	// right direction every time someone switches spectator targets.
	if ( !m_bCurrentFeetYawInitialized )
	{
		m_bCurrentFeetYawInitialized = true;
		m_flGoalFeetYaw = m_flCurrentFeetYaw = m_flEyeYaw;
		m_flLastTurnTime = 0.0f;
	}
	else if ( bIsMoving )
	{
		// player is moving, feet yaw = aiming yaw
		if ( m_AnimConfig.m_LegAnimType == LEGANIM_9WAY || m_AnimConfig.m_LegAnimType == LEGANIM_8WAY )
		{
			// His feet point in the direction his eyes are, but they can run in any direction.
			m_flGoalFeetYaw = m_flEyeYaw;
		}
		else
		{
			m_flGoalFeetYaw = RAD2DEG( atan2( vel.y, vel.x ) );

			// If he's running backwards, flip his feet backwards.
			Vector vEyeYaw( cos( DEG2RAD( m_flEyeYaw ) ), sin( DEG2RAD( m_flEyeYaw ) ), 0 );
			Vector vFeetYaw( cos( DEG2RAD( m_flGoalFeetYaw ) ), sin( DEG2RAD( m_flGoalFeetYaw ) ), 0 );
			if ( vEyeYaw.Dot( vFeetYaw ) < -0.01 )
			{
				m_flGoalFeetYaw += 180;
			}
		}

	}
	else if ( (gpGlobals->curtime - m_flLastTurnTime) > asw_facefronttime.GetFloat() )
	{
		// player didn't move & turn for quite some time
		m_flGoalFeetYaw = m_flEyeYaw;
	}
	else
	{
		// If he's rotated his view further than the model can turn, make him face forward.
		float flDiff = AngleNormalize(  m_flGoalFeetYaw - m_flEyeYaw );

		if ( fabs(flDiff) > flMaxGap )
		{
			m_flGoalFeetYaw = m_flEyeYaw;
			//flMaxGap = 180.0f / gpGlobals->frametime;
		}
	}

	m_flGoalFeetYaw = AngleNormalize( m_flGoalFeetYaw );

	if ( m_flCurrentFeetYaw != m_flGoalFeetYaw )
	{
		ConvergeAngles( m_flGoalFeetYaw, flFeetYawRate, flMaxGap,
			gpGlobals->frametime, m_flCurrentFeetYaw );

		m_flLastTurnTime = gpGlobals->curtime;
	}

	float flCurrentTorsoYaw = AngleNormalize( m_flEyeYaw - m_flCurrentFeetYaw );

	// Rotate entire body into position
	m_angRender[YAW] = m_flCurrentFeetYaw;
	m_angRender[PITCH] = m_angRender[ROLL] = 0;

	SetOuterBodyYaw( flCurrentTorsoYaw );
	g_flLastBodyYaw = flCurrentTorsoYaw;
}

float CASWPlayerAnimState::SetOuterBodyYaw( float flValue )
{
	C_ASW_Marine* pMarine = dynamic_cast<C_ASW_Marine*>(GetOuter());	
	int body_yaw = GetOuter()->LookupPoseParameter( "aim_yaw" );
	if ( body_yaw < 0 )
	{
		return 0;
	}
	if (pMarine && !pMarine->IsInhabited())
	{
		// let the ai_basenpc do this
	}

	SetOuterPoseParameter( body_yaw, flValue );
	return flValue;
}

void CASWPlayerAnimState::ComputePoseParam_BodyPitch(CStudioHdr *pStudioHdr)
{
	// Get pitch from v_angle
	float flPitch = m_flEyePitch;
	if ( flPitch > 180.0f )
	{
		flPitch -= 360.0f;
	}
	flPitch = clamp( flPitch, -90, 90 );

	// See if we have a blender for pitch
	int pitch = GetOuter()->LookupPoseParameter( "aim_pitch" );
	if ( pitch < 0 )
	{
		Msg("Couldn't find pitch pose parameter to set to %f\n", flPitch);
		return;
	}

	SetOuterPoseParameter( pitch, flPitch );
//	m_flLastBodyPitch = flPitch;
}

bool CASWPlayerAnimState::IsAnimatingReload()
{
	return m_bReloading;
}

bool CASWPlayerAnimState::IsAnimatingJump()
{
	return m_bJumping;
}

bool CASWPlayerAnimState::IsDoingEmoteGesture()
{
	return m_bPlayingEmoteGesture;
}

Activity CASWPlayerAnimState::TranslateActivity(Activity actDesired)
{
	Activity idealActivity = actDesired;

#ifdef ASW_TRANSLATE_ACTIVITIES_BY_WEAPON

#ifdef CLIENT_DLL
	C_ASW_Marine* pMarine = dynamic_cast<C_ASW_Marine*>(GetOuter());
#else
	CASW_Marine* pMarine = dynamic_cast<CASW_Marine*>(GetOuter());
#endif
	if ( pMarine && pMarine->GetActiveASWWeapon() )
	{
		// allow the weapon to decide which type of animation to play
		idealActivity = (Activity) pMarine->GetActiveASWWeapon()->ASW_SelectWeaponActivity( idealActivity );
	}

#endif

	return idealActivity;
}

// Below this many degrees, slow down turning rate linearly
#define FADE_TURN_DEGREES	45.0f

int CASWPlayerAnimState::ConvergeAngles( float goal,float maxrate, float maxgap, float dt, float& current )
{
	int direction = TURN_NONE;

	float anglediff = goal - current;
	anglediff = AngleNormalize( anglediff );

	float anglediffabs = fabs( anglediff );

	float scale = 1.0f;
	if ( anglediffabs <= FADE_TURN_DEGREES )
	{
		scale = anglediffabs / FADE_TURN_DEGREES;
		// Always do at least a bit of the turn ( 1% )
		scale = clamp( scale, 0.01f, 1.0f );
	}

	float maxmove = maxrate * dt * scale;

	if ( anglediffabs > maxgap )
	{
		float flMoveScale = clamp( ( anglediffabs - maxgap ) / 5, 1.0f, 10.0f );
		float flTooFar = MIN( anglediffabs - maxgap, maxmove * flMoveScale );
		if ( anglediff > 0 )
		{
			current += flTooFar;
		}
		else
		{
			current -= flTooFar;
		}
		current = AngleNormalize( current );
		anglediff = AngleNormalize( goal - current );
		anglediffabs = fabs( anglediff );
	}

	if ( anglediffabs < maxmove )
	{
		// we are close enough, just set the final value
		current = goal;
	}
	else
	{
		// adjust value up or down
		if ( anglediff > 0 )
		{
			current += maxmove;
			direction = TURN_LEFT;
		}
		else
		{
			current -= maxmove;
			direction = TURN_RIGHT;
		}
	}

	current = AngleNormalize( current );

	return direction;
}

int CASWPlayerAnimState::SelectWeightedSequence( Activity activity )
{
	return GetOuter()->SelectWeightedSequenceFromModifiers( activity, m_ActivityModifiers.Base(), m_ActivityModifiers.Count() );
}
