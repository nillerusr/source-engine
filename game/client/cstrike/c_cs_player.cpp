//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "c_cs_player.h"
#include "c_user_message_register.h"
#include "view.h"
#include "iclientvehicle.h"
#include "ivieweffects.h"
#include "input.h"
#include "IEffects.h"
#include "fx.h"
#include "c_basetempentity.h"
#include "hud_macros.h"	//HOOK_COMMAND
#include "engine/ivdebugoverlay.h"
#include "smoke_fog_overlay.h"
#include "bone_setup.h"
#include "in_buttons.h"
#include "r_efx.h"
#include "dlight.h"
#include "shake.h"
#include "cl_animevent.h"
#include "c_physicsprop.h"
#include "props_shared.h"
#include "obstacle_pushaway.h"
#include "death_pose.h"

#include "effect_dispatch_data.h"	//for water ripple / splash effect
#include "c_te_effect_dispatch.h"	//ditto
#include "c_te_legacytempents.h"
#include "cs_gamerules.h"
#include "fx_cs_blood.h"
#include "c_cs_playerresource.h"
#include "c_team.h"

#include "history_resource.h"
#include "ragdoll_shared.h"
#include "collisionutils.h"

// NVNT - haptics system for spectating
#include "haptics/haptic_utils.h"

#include "steam/steam_api.h"

#include "cs_blackmarket.h"				// for vest/helmet prices

#if defined( CCSPlayer )
	#undef CCSPlayer
#endif

#include "materialsystem/imesh.h"		//for materials->FindMaterial
#include "iviewrender.h"				//for view->

#include "iviewrender_beams.h"			// flashlight beam

//=============================================================================
// HPE_BEGIN:
// [menglish] Adding and externing variables needed for the freezecam
//=============================================================================

static Vector WALL_MIN(-WALL_OFFSET,-WALL_OFFSET,-WALL_OFFSET);
static Vector WALL_MAX(WALL_OFFSET,WALL_OFFSET,WALL_OFFSET);

extern ConVar	spec_freeze_time;
extern ConVar	spec_freeze_traveltime;
extern ConVar	spec_freeze_distance_min;
extern ConVar	spec_freeze_distance_max;

//=============================================================================
// HPE_END
//=============================================================================

ConVar cl_left_hand_ik( "cl_left_hand_ik", "0", 0, "Attach player's left hand to rifle with IK." );

ConVar cl_ragdoll_physics_enable( "cl_ragdoll_physics_enable", "1", 0, "Enable/disable ragdoll physics." );

ConVar cl_minmodels( "cl_minmodels", "0", 0, "Uses one player model for each team." );
ConVar cl_min_ct( "cl_min_ct", "1", 0, "Controls which CT model is used when cl_minmodels is set.", true, 1, true, 4 );
ConVar cl_min_t( "cl_min_t", "1", 0, "Controls which Terrorist model is used when cl_minmodels is set.", true, 1, true, 4 );
const float CycleLatchTolerance = 0.15;	// amount we can diverge from the server's cycle before we're corrected

extern ConVar mp_playerid_delay;
extern ConVar mp_playerid_hold;
extern ConVar sv_allowminmodels;

class CAddonInfo
{
public:
	const char *m_pAttachmentName;
	const char *m_pWeaponClassName;	// The addon uses the w_ model from this weapon.
	const char *m_pModelName;		//If this is present, will use this model instead of looking up the weapon
	const char *m_pHolsterName;
};



// These must follow the ADDON_ ordering.
CAddonInfo g_AddonInfo[] =
{
	{ "grenade0",	"weapon_flashbang",		0, 0 },
	{ "grenade1",	"weapon_flashbang",		0, 0 },
	{ "grenade2",	"weapon_hegrenade",		0, 0 },
	{ "grenade3",	"weapon_smokegrenade",	0, 0 },
	{ "c4",			"weapon_c4",			0, 0 },
	{ "defusekit",	0,						"models/weapons/w_defuser.mdl", 0 },
	{ "primary",	0,						0, 0 },	// Primary addon model is looked up based on m_iPrimaryAddon
	{ "pistol",		0,						0, 0 },	// Pistol addon model is looked up based on m_iSecondaryAddon
	{ "eholster",	0,						"models/weapons/w_eq_eholster_elite.mdl", "models/weapons/w_eq_eholster.mdl" },
};

// -------------------------------------------------------------------------------- //
// Player animation event. Sent to the client when a player fires, jumps, reloads, etc..
// -------------------------------------------------------------------------------- //

class C_TEPlayerAnimEvent : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEPlayerAnimEvent, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	virtual void PostDataUpdate( DataUpdateType_t updateType )
	{
		// Create the effect.
		C_CSPlayer *pPlayer = dynamic_cast< C_CSPlayer* >( m_hPlayer.Get() );
		if ( pPlayer && !pPlayer->IsDormant() )
		{
			pPlayer->DoAnimationEvent( (PlayerAnimEvent_t)m_iEvent.Get(), m_nData );
		}
	}

public:
	CNetworkHandle( CBasePlayer, m_hPlayer );
	CNetworkVar( int, m_iEvent );
	CNetworkVar( int, m_nData );
};

IMPLEMENT_CLIENTCLASS_EVENT( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent, CTEPlayerAnimEvent );

BEGIN_RECV_TABLE_NOBASE( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent )
	RecvPropEHandle( RECVINFO( m_hPlayer ) ),
	RecvPropInt( RECVINFO( m_iEvent ) ),
	RecvPropInt( RECVINFO( m_nData ) )
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_CSPlayer )
#ifdef CS_SHIELD_ENABLED
	DEFINE_PRED_FIELD( m_bShieldDrawn, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
#endif
	DEFINE_PRED_FIELD_TOL( m_flStamina, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.1f ),
	DEFINE_PRED_FIELD( m_flCycle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_iShotsFired, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iDirection, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bResumeZoom, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iLastZoom, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),

END_PREDICTION_DATA()

vgui::IImage* GetDefaultAvatarImage( C_BasePlayer *pPlayer )
{
	vgui::IImage* result = NULL;

	switch ( pPlayer ? pPlayer->GetTeamNumber() : TEAM_MAXCOUNT )
	{
		case TEAM_TERRORIST: 
			result = vgui::scheme()->GetImage( CSTRIKE_DEFAULT_T_AVATAR, true );
			break;

		case TEAM_CT:		 
			result = vgui::scheme()->GetImage( CSTRIKE_DEFAULT_CT_AVATAR, true );
			break;

		default:
			result = vgui::scheme()->GetImage( CSTRIKE_DEFAULT_AVATAR, true );
			break;
	}

	return result;
}

// ----------------------------------------------------------------------------- //
// Client ragdoll entity.
// ----------------------------------------------------------------------------- //

float g_flDieTranslucentTime = 0.6;

class C_CSRagdoll : public C_BaseAnimatingOverlay
{
public:
	DECLARE_CLASS( C_CSRagdoll, C_BaseAnimatingOverlay );
	DECLARE_CLIENTCLASS();

	C_CSRagdoll();
	~C_CSRagdoll();

	virtual void OnDataChanged( DataUpdateType_t type );

	int GetPlayerEntIndex() const;
	IRagdoll* GetIRagdoll() const;
	bool GetRagdollInitBoneArrays( matrix3x4_t *pDeltaBones0, matrix3x4_t *pDeltaBones1, matrix3x4_t *pCurrentBones, float boneDt ) OVERRIDE;

	void ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName );

	virtual void ComputeFxBlend();
	virtual bool IsTransparent();
	bool IsInitialized() { return m_bInitialized; }
	// fading ragdolls don't cast shadows
	virtual ShadowType_t ShadowCastType() 
	{ 
		if ( m_flRagdollSinkStart == -1 )
			return BaseClass::ShadowCastType();
		return SHADOWS_NONE;
	}

	virtual void ValidateModelIndex( void );

private:

	C_CSRagdoll( const C_CSRagdoll & ) {}

	void Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity );

	void CreateLowViolenceRagdoll( void );
	void CreateCSRagdoll( void );

private:

	EHANDLE	m_hPlayer;
	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );
	CNetworkVar(int, m_iDeathPose );
	CNetworkVar(int, m_iDeathFrame );
	float m_flRagdollSinkStart;
	bool m_bInitialized;
	bool m_bCreatedWhilePlaybackSkipping;
};


IMPLEMENT_CLIENTCLASS_DT_NOBASE( C_CSRagdoll, DT_CSRagdoll, CCSRagdoll )
	RecvPropVector( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
	RecvPropVector( RECVINFO(m_vecRagdollOrigin) ),
	RecvPropEHandle( RECVINFO( m_hPlayer ) ),
	RecvPropInt( RECVINFO( m_nModelIndex ) ),
	RecvPropInt( RECVINFO(m_nForceBone) ),
	RecvPropVector( RECVINFO(m_vecForce) ),
	RecvPropVector( RECVINFO( m_vecRagdollVelocity ) ),
	RecvPropInt( RECVINFO(m_iDeathPose) ),
	RecvPropInt( RECVINFO(m_iDeathFrame) ),
	RecvPropInt(RECVINFO(m_iTeamNum)),
	RecvPropInt( RECVINFO(m_bClientSideAnimation)),
END_RECV_TABLE()


C_CSRagdoll::C_CSRagdoll()
{
	m_flRagdollSinkStart = -1;
	m_bInitialized = false;
	m_bCreatedWhilePlaybackSkipping = engine->IsSkippingPlayback();
}

C_CSRagdoll::~C_CSRagdoll()
{
	PhysCleanupFrictionSounds( this );
}

bool C_CSRagdoll::GetRagdollInitBoneArrays( matrix3x4_t *pDeltaBones0, matrix3x4_t *pDeltaBones1, matrix3x4_t *pCurrentBones, float boneDt )
{
	// otherwise use the death pose to set up the ragdoll
	ForceSetupBonesAtTime( pDeltaBones0, gpGlobals->curtime - boneDt );
	GetRagdollCurSequenceWithDeathPose( this, pDeltaBones1, gpGlobals->curtime, m_iDeathPose, m_iDeathFrame );
	return SetupBones( pCurrentBones, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, gpGlobals->curtime );
}

void C_CSRagdoll::Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity )
{
	if ( !pSourceEntity )
		return;

	VarMapping_t *pSrc = pSourceEntity->GetVarMapping();
	VarMapping_t *pDest = GetVarMapping();

	// Find all the VarMapEntry_t's that represent the same variable.
	for ( int i = 0; i < pDest->m_Entries.Count(); i++ )
	{
		VarMapEntry_t *pDestEntry = &pDest->m_Entries[i];
		for ( int j=0; j < pSrc->m_Entries.Count(); j++ )
		{
			VarMapEntry_t *pSrcEntry = &pSrc->m_Entries[j];
			if ( !Q_strcmp( pSrcEntry->watcher->GetDebugName(),
				pDestEntry->watcher->GetDebugName() ) )
			{
				pDestEntry->watcher->Copy( pSrcEntry->watcher );
				break;
			}
		}
	}
}

void C_CSRagdoll::ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();

	if( !pPhysicsObject )
		return;

	Vector dir = pTrace->endpos - pTrace->startpos;

	if ( iDamageType == DMG_BLAST )
	{
		dir *= 4000;  // adjust impact strenght

		// apply force at object mass center
		pPhysicsObject->ApplyForceCenter( dir );
	}
	else
	{
		Vector hitpos;

		VectorMA( pTrace->startpos, pTrace->fraction, dir, hitpos );
		VectorNormalize( dir );

		dir *= 4000;  // adjust impact strenght

		// apply force where we hit it
		pPhysicsObject->ApplyForceOffset( dir, hitpos );

		// Blood spray!
		FX_CS_BloodSpray( hitpos, dir, 10 );
	}

	m_pRagdoll->ResetRagdollSleepAfterTime();
}


void C_CSRagdoll::ValidateModelIndex( void )
{
	if ( sv_allowminmodels.GetBool() && cl_minmodels.GetBool() )
	{
		if ( GetTeamNumber() == TEAM_CT )
		{
			int index = cl_min_ct.GetInt() - 1;
			if ( index >= 0 && index < CTPlayerModels.Count() )
			{
				m_nModelIndex = modelinfo->GetModelIndex(CTPlayerModels[index]);
			}
		}
		else if ( GetTeamNumber() == TEAM_TERRORIST )
		{
			int index = cl_min_t.GetInt() - 1;
			if ( index >= 0 && index < TerroristPlayerModels.Count() )
			{
				m_nModelIndex = modelinfo->GetModelIndex(TerroristPlayerModels[index]);
			}
		}
	}

	BaseClass::ValidateModelIndex();
}


void C_CSRagdoll::CreateLowViolenceRagdoll( void )
{
	// Just play a death animation.
	// Find a death anim to play.
	int iMinDeathAnim = 9999, iMaxDeathAnim = -9999;
	for ( int iAnim=1; iAnim < 100; iAnim++ )
	{
		char str[512];
		Q_snprintf( str, sizeof( str ), "death%d", iAnim );
		if ( LookupSequence( str ) == -1 )
			break;

		iMinDeathAnim = MIN( iMinDeathAnim, iAnim );
		iMaxDeathAnim = MAX( iMaxDeathAnim, iAnim );
	}

	if ( iMinDeathAnim == 9999 )
	{
		CreateCSRagdoll();
		return;
	}

	SetNetworkOrigin( m_vecRagdollOrigin );
	SetAbsOrigin( m_vecRagdollOrigin );
	SetAbsVelocity( m_vecRagdollVelocity );

	C_CSPlayer *pPlayer = dynamic_cast< C_CSPlayer* >( m_hPlayer.Get() );
	if ( pPlayer )
	{
		if ( !pPlayer->IsDormant() )
		{
			// move my current model instance to the ragdoll's so decals are preserved.
			pPlayer->SnatchModelInstance( this );
		}

		SetAbsAngles( pPlayer->GetRenderAngles() );
		SetNetworkAngles( pPlayer->GetRenderAngles() );
	}

	int iDeathAnim = RandomInt( iMinDeathAnim, iMaxDeathAnim );
	char str[512];
	Q_snprintf( str, sizeof( str ), "death%d", iDeathAnim );
	SetSequence( LookupSequence( str ) );
	ForceClientSideAnimationOn();

	Interp_Reset( GetVarMapping() );
}


void C_CSRagdoll::CreateCSRagdoll()
{
	// First, initialize all our data. If we have the player's entity on our client,
	// then we can make ourselves start out exactly where the player is.
	C_CSPlayer *pPlayer = dynamic_cast< C_CSPlayer* >( m_hPlayer.Get() );

	// mark this to prevent model changes from overwriting the death sequence with the server sequence
	SetReceivedSequence();

	if ( pPlayer && !pPlayer->IsDormant() )
	{
		// move my current model instance to the ragdoll's so decals are preserved.
		pPlayer->SnatchModelInstance( this );

		VarMapping_t *varMap = GetVarMapping();

		// Copy all the interpolated vars from the player entity.
		// The entity uses the interpolated history to get bone velocity.
		bool bRemotePlayer = (pPlayer != C_BasePlayer::GetLocalPlayer());
		if ( bRemotePlayer )
		{
			Interp_Copy( pPlayer );

			SetAbsAngles( pPlayer->GetRenderAngles() );
			GetRotationInterpolator().Reset();

			m_flAnimTime = pPlayer->m_flAnimTime;
			SetSequence( pPlayer->GetSequence() );
			m_flPlaybackRate = pPlayer->GetPlaybackRate();
		}
		else
		{
			// This is the local player, so set them in a default
			// pose and slam their velocity, angles and origin
			SetAbsOrigin( m_vecRagdollOrigin );

			SetAbsAngles( pPlayer->GetRenderAngles() );

			SetAbsVelocity( m_vecRagdollVelocity );

			int iSeq = LookupSequence( "walk_lower" );
			if ( iSeq == -1 )
			{
				Assert( false );	// missing walk_lower?
				iSeq = 0;
			}

			SetSequence( iSeq );	// walk_lower, basic pose
			SetCycle( 0.0 );

			// go ahead and set these on the player in case the code below decides to set up bones using
			// that entity instead of this one.  The local player may not have valid animation
			pPlayer->SetSequence( iSeq );	// walk_lower, basic pose
			pPlayer->SetCycle( 0.0 );

			Interp_Reset( varMap );
		}
	}
	else
	{
		// overwrite network origin so later interpolation will
		// use this position
		SetNetworkOrigin( m_vecRagdollOrigin );

		SetAbsOrigin( m_vecRagdollOrigin );
		SetAbsVelocity( m_vecRagdollVelocity );

		Interp_Reset( GetVarMapping() );
	}

	// Turn it into a ragdoll.
	if ( cl_ragdoll_physics_enable.GetInt() )
	{
		// Make us a ragdoll..
		m_nRenderFX = kRenderFxRagdoll;

		matrix3x4_t boneDelta0[MAXSTUDIOBONES];
		matrix3x4_t boneDelta1[MAXSTUDIOBONES];
		matrix3x4_t currentBones[MAXSTUDIOBONES];
		const float boneDt = 0.05f;

		//=============================================================================
		// [pfreese], [tj]
		// There are visual problems with the attempted blending of the 
		// death pose animations in C_CSRagdoll::GetRagdollInitBoneArrays. The version
		// in C_BasePlayer::GetRagdollInitBoneArrays doesn't attempt to blend death
		// poses, so if the player is relevant, use that one regardless of whether the 
		// player is the local one or not.
		//=============================================================================
		if ( pPlayer && !pPlayer->IsDormant() )
		{
			pPlayer->GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );
		}
		else
		{
			GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );
		}

		InitAsClientRagdoll( boneDelta0, boneDelta1, currentBones, boneDt );
		m_flRagdollSinkStart = -1;
	}
	else
	{
		m_flRagdollSinkStart = gpGlobals->curtime;
		DestroyShadow();
		ClientLeafSystem()->SetRenderGroup( GetRenderHandle(), RENDER_GROUP_TRANSLUCENT_ENTITY );
	}
	m_bInitialized = true;
}


void C_CSRagdoll::ComputeFxBlend( void )
{
	if ( m_flRagdollSinkStart == -1 )
	{
		BaseClass::ComputeFxBlend();
	}
	else
	{
		float elapsed = gpGlobals->curtime - m_flRagdollSinkStart;
		float flVal = RemapVal( elapsed, 0, g_flDieTranslucentTime, 255, 0 );
		flVal = clamp( flVal, 0, 255 );
		m_nRenderFXBlend = (int)flVal;

#ifdef _DEBUG
		m_nFXComputeFrame = gpGlobals->framecount;
#endif
	}
}


bool C_CSRagdoll::IsTransparent( void )
{
	if ( m_flRagdollSinkStart == -1 )
	{
		return BaseClass::IsTransparent();
	}
	else
	{
		return true;
	}
}


void C_CSRagdoll::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		// Prevent replays from creating ragdolls on the first frame of playback after skipping through playback.
		// If a player died (leaving a ragdoll) previous to the first frame of replay playback,
		// their ragdoll wasn't yet initialized because OnDataChanged events are queued but not processed
		// until the first render. 
		if ( engine->IsPlayingDemo() && m_bCreatedWhilePlaybackSkipping )
		{
			Release();
			return;
		}

		if ( g_RagdollLVManager.IsLowViolence() )
		{
			CreateLowViolenceRagdoll();
		}
		else
		{
			CreateCSRagdoll();
		}
	}
	else
	{
		if ( !cl_ragdoll_physics_enable.GetInt() )
		{
			// Don't let it set us back to a ragdoll with data from the server.
			m_nRenderFX = kRenderFxNone;
		}
	}
}

IRagdoll* C_CSRagdoll::GetIRagdoll() const
{
	return m_pRagdoll;
}

//-----------------------------------------------------------------------------
// Purpose: Called when the player toggles nightvision
// Input  : *pData - the int value of the nightvision state
//			*pStruct - the player
//			*pOut -
//-----------------------------------------------------------------------------
void RecvProxy_NightVision( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_CSPlayer *pPlayerData = (C_CSPlayer *) pStruct;

	bool bNightVisionOn = ( pData->m_Value.m_Int > 0 );

	if ( pPlayerData->m_bNightVisionOn != bNightVisionOn )
	{
		if ( bNightVisionOn )
			 pPlayerData->m_flNightVisionAlpha = 1;
	}

	pPlayerData->m_bNightVisionOn = bNightVisionOn;
}

void RecvProxy_FlashTime( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_CSPlayer *pPlayerData = (C_CSPlayer *) pStruct;

	if( pPlayerData != C_BasePlayer::GetLocalPlayer() )
		return;

	if ( (pPlayerData->m_flFlashDuration != pData->m_Value.m_Float) && pData->m_Value.m_Float > 0 )
	{
		pPlayerData->m_flFlashAlpha = 1;
	}

	pPlayerData->m_flFlashDuration = pData->m_Value.m_Float;
	pPlayerData->m_flFlashBangTime = gpGlobals->curtime + pPlayerData->m_flFlashDuration;
}

void RecvProxy_HasDefuser( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_CSPlayer *pPlayerData = (C_CSPlayer *)pStruct;

	if (pPlayerData == NULL)
	{
		return;
	}

	bool drawIcon = false;

	if (pData->m_Value.m_Int == 0)
	{
		pPlayerData->RemoveDefuser();
	}
	else
	{
		if (pPlayerData->HasDefuser() == false)
		{
			drawIcon = true;
		}
		pPlayerData->GiveDefuser();
	}

	if (pPlayerData->IsLocalPlayer() && drawIcon)
	{
		// add to pickup history
		CHudHistoryResource *pHudHR = GET_HUDELEMENT( CHudHistoryResource );

		if ( pHudHR )
		{
			pHudHR->AddToHistory(HISTSLOT_ITEM, "defuser_pickup");
		}
	}
}

void C_CSPlayer::RecvProxy_CycleLatch( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	// This receive proxy looks to see if the server's value is close enough to what we think it should
	// be.  We've been running the same code; this is an error correction for changes we didn't simulate
	// while they were out of PVS.
	C_CSPlayer *pPlayer = (C_CSPlayer *)pStruct;
	if( pPlayer->IsLocalPlayer() )
		return; // Don't need to fixup ourselves.

	float incomingCycle = (float)(pData->m_Value.m_Int) / 16; // Came in as 4 bit fixed point
	float currentCycle = pPlayer->GetCycle();
	bool closeEnough = fabs(currentCycle - incomingCycle) < CycleLatchTolerance;
	if( fabs(currentCycle - incomingCycle) > (1 - CycleLatchTolerance) )
	{
		closeEnough = true;// Handle wrapping around 1->0
	}

	if( !closeEnough )
	{
		// Server disagrees too greatly.  Correct our value.
		if ( pPlayer && pPlayer->GetTeam() )
		{
			DevMsg( 2, "%s %s(%d): Cycle latch wants to correct %.2f in to %.2f.\n",
				pPlayer->GetTeam()->Get_Name(), pPlayer->GetPlayerName(), pPlayer->entindex(), currentCycle, incomingCycle );
		}
		pPlayer->SetServerIntendedCycle( incomingCycle );
	}
}

void __MsgFunc_ReloadEffect( bf_read &msg )
{
	int iPlayer = msg.ReadShort();
	C_CSPlayer *pPlayer = dynamic_cast< C_CSPlayer* >( C_BaseEntity::Instance( iPlayer ) );
	if ( pPlayer )
		pPlayer->PlayReloadEffect();

}
USER_MESSAGE_REGISTER( ReloadEffect );

BEGIN_RECV_TABLE_NOBASE( C_CSPlayer, DT_CSLocalPlayerExclusive )
	RecvPropFloat( RECVINFO(m_flStamina) ),
	RecvPropInt( RECVINFO( m_iDirection ) ),
	RecvPropInt( RECVINFO( m_iShotsFired ) ),
	RecvPropFloat( RECVINFO( m_flVelocityModifier ) ),

	RecvPropVector( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),

    //=============================================================================
    // HPE_BEGIN:
    // [tj]Set up the receive table for per-client domination data
    //=============================================================================

    RecvPropArray3( RECVINFO_ARRAY( m_bPlayerDominated ), RecvPropBool( RECVINFO( m_bPlayerDominated[0] ) ) ),
    RecvPropArray3( RECVINFO_ARRAY( m_bPlayerDominatingMe ), RecvPropBool( RECVINFO( m_bPlayerDominatingMe[0] ) ) )

    //=============================================================================
    // HPE_END
    //=============================================================================

END_RECV_TABLE()


BEGIN_RECV_TABLE_NOBASE( C_CSPlayer, DT_CSNonLocalPlayerExclusive )
	RecvPropVector( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
END_RECV_TABLE()


IMPLEMENT_CLIENTCLASS_DT( C_CSPlayer, DT_CSPlayer, CCSPlayer )
	RecvPropDataTable( "cslocaldata", 0, 0, &REFERENCE_RECV_TABLE(DT_CSLocalPlayerExclusive) ),
	RecvPropDataTable( "csnonlocaldata", 0, 0, &REFERENCE_RECV_TABLE(DT_CSNonLocalPlayerExclusive) ),
	RecvPropInt( RECVINFO( m_iAddonBits ) ),
	RecvPropInt( RECVINFO( m_iPrimaryAddon ) ),
	RecvPropInt( RECVINFO( m_iSecondaryAddon ) ),
	RecvPropInt( RECVINFO( m_iThrowGrenadeCounter ) ),
	RecvPropInt( RECVINFO( m_iPlayerState ) ),
	RecvPropInt( RECVINFO( m_iAccount ) ),
	RecvPropInt( RECVINFO( m_bInBombZone ) ),
	RecvPropInt( RECVINFO( m_bInBuyZone ) ),
	RecvPropInt( RECVINFO( m_iClass ) ),
	RecvPropInt( RECVINFO( m_ArmorValue ) ),
	RecvPropFloat( RECVINFO( m_angEyeAngles[0] ) ),
	RecvPropFloat( RECVINFO( m_angEyeAngles[1] ) ),
	RecvPropFloat( RECVINFO( m_flStamina ) ),
	RecvPropInt( RECVINFO( m_bHasDefuser ), 0, RecvProxy_HasDefuser ),
	RecvPropInt( RECVINFO( m_bNightVisionOn), 0, RecvProxy_NightVision ),
	RecvPropBool( RECVINFO( m_bHasNightVision ) ),


    //=============================================================================
    // HPE_BEGIN:
    // [dwenger] Added for fun-fact support
    //=============================================================================

    //RecvPropBool( RECVINFO( m_bPickedUpDefuser ) ),
    //RecvPropBool( RECVINFO( m_bDefusedWithPickedUpKit ) ),

    //=============================================================================
    // HPE_END
    //=============================================================================

    RecvPropBool( RECVINFO( m_bInHostageRescueZone ) ),
	RecvPropInt( RECVINFO( m_ArmorValue ) ),
	RecvPropBool( RECVINFO( m_bIsDefusing ) ),
	RecvPropBool( RECVINFO( m_bResumeZoom ) ),
	RecvPropInt( RECVINFO( m_iLastZoom ) ),

#ifdef CS_SHIELD_ENABLED
	RecvPropBool( RECVINFO( m_bHasShield ) ),
	RecvPropBool( RECVINFO( m_bShieldDrawn ) ),
#endif
	RecvPropInt( RECVINFO( m_bHasHelmet ) ),
	RecvPropVector( RECVINFO( m_vecRagdollVelocity ) ),
	RecvPropFloat( RECVINFO( m_flFlashDuration ), 0, RecvProxy_FlashTime ),
	RecvPropFloat( RECVINFO( m_flFlashMaxAlpha)),
	RecvPropInt( RECVINFO( m_iProgressBarDuration ) ),
	RecvPropFloat( RECVINFO( m_flProgressBarStartTime ) ),
	RecvPropEHandle( RECVINFO( m_hRagdoll ) ),
	RecvPropInt( RECVINFO( m_cycleLatch ), 0, &C_CSPlayer::RecvProxy_CycleLatch ),

END_RECV_TABLE()



C_CSPlayer::C_CSPlayer() :
	m_iv_angEyeAngles( "C_CSPlayer::m_iv_angEyeAngles" )
{
	m_PlayerAnimState = CreatePlayerAnimState( this, this, LEGANIM_9WAY, true );

	m_angEyeAngles.Init();

	AddVar( &m_angEyeAngles, &m_iv_angEyeAngles, LATCH_SIMULATION_VAR );

	m_iLastAddonBits = m_iAddonBits = 0;
	m_iLastPrimaryAddon = m_iLastSecondaryAddon = WEAPON_NONE;
	m_iProgressBarDuration = 0;
	m_flProgressBarStartTime = 0.0f;
	m_ArmorValue = 0;
	m_bHasHelmet = false;
	m_iIDEntIndex = 0;
	m_delayTargetIDTimer.Reset();
	m_iOldIDEntIndex = 0;
	m_holdTargetIDTimer.Reset();
	m_iDirection = 0;

	m_Activity = ACT_IDLE;

	m_pFlashlightBeam = NULL;
	m_fNextThinkPushAway = 0.0f;

	m_serverIntendedCycle = -1.0f;

	view->SetScreenOverlayMaterial( NULL );

    m_bPlayingFreezeCamSound = false;
}


C_CSPlayer::~C_CSPlayer()
{
	RemoveAddonModels();

	ReleaseFlashlight();

	m_PlayerAnimState->Release();
}


bool C_CSPlayer::HasDefuser() const
{
	return m_bHasDefuser;
}

void C_CSPlayer::GiveDefuser()
{
	m_bHasDefuser = true;
}

void C_CSPlayer::RemoveDefuser()
{
	m_bHasDefuser = false;
}

bool C_CSPlayer::HasNightVision() const
{
	return m_bHasNightVision;
}

bool C_CSPlayer::IsVIP() const
{
	C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource*)GameResources();

	if ( !pCSPR )
		return false;

	return pCSPR->IsVIP( entindex() );
}

C_CSPlayer* C_CSPlayer::GetLocalCSPlayer()
{
	return (C_CSPlayer*)C_BasePlayer::GetLocalPlayer();
}


CSPlayerState C_CSPlayer::State_Get() const
{
	return m_iPlayerState;
}


float C_CSPlayer::GetMinFOV() const
{
	// Min FOV for AWP.
	return 10;
}


int C_CSPlayer::GetAccount() const
{
	return m_iAccount;
}


int C_CSPlayer::PlayerClass() const
{
	return m_iClass;
}

bool C_CSPlayer::IsInBuyZone()
{
	return m_bInBuyZone;
}

bool C_CSPlayer::CanShowTeamMenu() const
{
	return true;
}


int C_CSPlayer::ArmorValue() const
{
	return m_ArmorValue;
}

bool C_CSPlayer::HasHelmet() const
{
	return m_bHasHelmet;
}

int C_CSPlayer::GetCurrentAssaultSuitPrice()
{
	// WARNING: This price logic also exists in CCSPlayer::AttemptToBuyAssaultSuit
	// and must be kept in sync if changes are made.

	int fullArmor = ArmorValue() >= 100 ? 1 : 0;
	if ( fullArmor && !HasHelmet() )
	{
		return HELMET_PRICE;
	}
	else if ( !fullArmor && HasHelmet() )
	{
		return KEVLAR_PRICE;
	}
	else
	{
		// NOTE: This applies to the case where you already have both
		// as well as the case where you have neither.  In the case
		// where you have both, the item should still have a price
		// and become disabled when you have little or no money left.
		return ASSAULTSUIT_PRICE;
	}
}

const QAngle& C_CSPlayer::GetRenderAngles()
{
	if ( IsRagdoll() )
	{
		return vec3_angle;
	}
	else
	{
		return m_PlayerAnimState->GetRenderAngles();
	}
}


float g_flFattenAmt = 4;
void C_CSPlayer::GetShadowRenderBounds( Vector &mins, Vector &maxs, ShadowType_t shadowType )
{
	if ( shadowType == SHADOWS_SIMPLE )
	{
		// Don't let the render bounds change when we're using blobby shadows, or else the shadow
		// will pop and stretch.
		mins = CollisionProp()->OBBMins();
		maxs = CollisionProp()->OBBMaxs();
	}
	else
	{
		GetRenderBounds( mins, maxs );

		// We do this because the normal bbox calculations don't take pose params into account, and
		// the rotation of the guy's upper torso can place his gun a ways out of his bbox, and
		// the shadow will get cut off as he rotates.
		//
		// Thus, we give it some padding here.
		mins -= Vector( g_flFattenAmt, g_flFattenAmt, 0 );
		maxs += Vector( g_flFattenAmt, g_flFattenAmt, 0 );
	}
}


void C_CSPlayer::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	// TODO POSTSHIP - this hack/fix goes hand-in-hand with a fix in CalcSequenceBoundingBoxes in utils/studiomdl/simplify.cpp.
	// When we enable the fix in CalcSequenceBoundingBoxes, we can get rid of this.
	//
	// What we're doing right here is making sure it only uses the bbox for our lower-body sequences since,
	// with the current animations and the bug in CalcSequenceBoundingBoxes, are WAY bigger than they need to be.
	C_BaseAnimating::GetRenderBounds( theMins, theMaxs );

	// If we're ducking, we should reduce the render height by the difference in standing and ducking heights.
	// This prevents shadows from drawing above ducking players etc.
	if ( GetFlags() & FL_DUCKING )
	{
		theMaxs.z -= 18.5f;
	}
}


bool C_CSPlayer::GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const
{
	if ( shadowType == SHADOWS_SIMPLE )
	{
		// Blobby shadows should sit directly underneath us.
		pDirection->Init( 0, 0, -1 );
		return true;
	}
	else
	{
		return BaseClass::GetShadowCastDirection( pDirection, shadowType );
	}
}


void C_CSPlayer::VPhysicsUpdate( IPhysicsObject *pPhysics )
{
	BaseClass::VPhysicsUpdate( pPhysics );
}


int C_CSPlayer::GetIDTarget() const
{
	if ( !m_delayTargetIDTimer.IsElapsed() )
		return 0;

	if ( m_iIDEntIndex )
	{
		return m_iIDEntIndex;
	}

	if ( m_iOldIDEntIndex && !m_holdTargetIDTimer.IsElapsed() )
	{
		return m_iOldIDEntIndex;
	}

	return 0;
}


void InitializeAddonModelFromWeapon( CWeaponCSBase *weapon, C_BreakableProp *addon )
{
	if ( !weapon )
	{
		return;
	}

	const CCSWeaponInfo& weaponInfo = weapon->GetCSWpnData();
	if ( weaponInfo.m_szAddonModel[0] == 0 )
	{
		addon->InitializeAsClientEntity( weaponInfo.szWorldModel, RENDER_GROUP_OPAQUE_ENTITY );
	}
	else
	{
		addon->InitializeAsClientEntity( weaponInfo.m_szAddonModel, RENDER_GROUP_OPAQUE_ENTITY );
	}
}

void C_CSPlayer::CreateAddonModel( int i )
{
	COMPILE_TIME_ASSERT( (sizeof( g_AddonInfo ) / sizeof( g_AddonInfo[0] )) == NUM_ADDON_BITS );

	// Create the model entity.
	CAddonInfo *pAddonInfo = &g_AddonInfo[i];

	int iAttachment = LookupAttachment( pAddonInfo->m_pAttachmentName );
	if ( iAttachment <= 0 )
		return;

	C_BreakableProp *pEnt = new C_BreakableProp;

	int addonType = (1<<i);
	if ( addonType == ADDON_PISTOL || addonType == ADDON_PRIMARY )
	{
		CCSWeaponInfo *weaponInfo = GetWeaponInfo( (CSWeaponID)((addonType == ADDON_PRIMARY) ? m_iPrimaryAddon.Get() : m_iSecondaryAddon.Get()) );
		if ( !weaponInfo )
		{
			Warning( "C_CSPlayer::CreateAddonModel: Unable to get weapon info.\n" );
			pEnt->Release();
			return;
		}
		if ( weaponInfo->m_szAddonModel[0] == 0 )
		{
			pEnt->InitializeAsClientEntity( weaponInfo->szWorldModel, RENDER_GROUP_OPAQUE_ENTITY );
		}
		else
		{
			pEnt->InitializeAsClientEntity( weaponInfo->m_szAddonModel, RENDER_GROUP_OPAQUE_ENTITY );
		}
	}
	else if( pAddonInfo->m_pModelName )
	{
		if ( addonType == ADDON_PISTOL2 && !(m_iAddonBits & ADDON_PISTOL ) )
		{
			pEnt->InitializeAsClientEntity( pAddonInfo->m_pHolsterName, RENDER_GROUP_OPAQUE_ENTITY );
		}
		else
		{
			pEnt->InitializeAsClientEntity( pAddonInfo->m_pModelName, RENDER_GROUP_OPAQUE_ENTITY );
		}
	}
	else
	{
		WEAPON_FILE_INFO_HANDLE	hWpnInfo = LookupWeaponInfoSlot( pAddonInfo->m_pWeaponClassName );
		if ( hWpnInfo == GetInvalidWeaponInfoHandle() )
		{
			Assert( false );
			return;
		}

		CCSWeaponInfo *pWeaponInfo = dynamic_cast< CCSWeaponInfo* >( GetFileWeaponInfoFromHandle( hWpnInfo ) );
		if ( pWeaponInfo )
		{
			if ( pWeaponInfo->m_szAddonModel[0] == 0 )
				pEnt->InitializeAsClientEntity( pWeaponInfo->szWorldModel, RENDER_GROUP_OPAQUE_ENTITY );
			else
				pEnt->InitializeAsClientEntity( pWeaponInfo->m_szAddonModel, RENDER_GROUP_OPAQUE_ENTITY );
		}
		else
		{
			pEnt->Release();
			Warning( "C_CSPlayer::CreateAddonModel: Unable to get weapon info for %s.\n", pAddonInfo->m_pWeaponClassName );
			return;
		}
	}

	if ( Q_strcmp( pAddonInfo->m_pAttachmentName, "c4" ) )
	{
		// fade out all attached models except C4
		pEnt->SetFadeMinMax( 400, 500 );
	}

	// Create the addon.
	CAddonModel *pAddon = &m_AddonModels[m_AddonModels.AddToTail()];

	pAddon->m_hEnt = pEnt;
	pAddon->m_iAddon = i;
	pAddon->m_iAttachmentPoint = iAttachment;
	pEnt->SetParent( this, pAddon->m_iAttachmentPoint );
	pEnt->SetLocalOrigin( Vector( 0, 0, 0 ) );
	pEnt->SetLocalAngles( QAngle( 0, 0, 0 ) );
	if ( IsLocalPlayer() )
	{
		pEnt->SetSolid( SOLID_NONE );
		pEnt->RemoveEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );
	}
}


void C_CSPlayer::UpdateAddonModels()
{
	int iCurAddonBits = m_iAddonBits;

	// Don't put addon models on the local player unless in third person.
	if ( IsLocalPlayer() && !C_BasePlayer::ShouldDrawLocalPlayer() )
		iCurAddonBits = 0;

	// If the local player is observing this entity in first-person mode, get rid of its addons.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer && pPlayer->GetObserverMode() == OBS_MODE_IN_EYE && pPlayer->GetObserverTarget() == this )
		iCurAddonBits = 0;

	// Any changes to the attachments we should have?
	if ( m_iLastAddonBits == iCurAddonBits &&
		m_iLastPrimaryAddon == m_iPrimaryAddon &&
		m_iLastSecondaryAddon == m_iSecondaryAddon )
	{
		return;
	}

	bool rebuildPistol2Addon = false;
	if ( m_iSecondaryAddon == WEAPON_ELITE && ((m_iLastAddonBits ^ iCurAddonBits) & ADDON_PISTOL) != 0 )
	{
		rebuildPistol2Addon = true;
	}
	m_iLastAddonBits = iCurAddonBits;
	m_iLastPrimaryAddon = m_iPrimaryAddon;
	m_iLastSecondaryAddon = m_iSecondaryAddon;

	// Get rid of any old models.
	int i,iNext;
	for ( i=m_AddonModels.Head(); i != m_AddonModels.InvalidIndex(); i = iNext )
	{
		iNext = m_AddonModels.Next( i );
		CAddonModel *pModel = &m_AddonModels[i];

		int addonBit = 1<<pModel->m_iAddon;
		if ( !( iCurAddonBits & addonBit ) || (rebuildPistol2Addon && addonBit == ADDON_PISTOL2) )
		{
			if ( pModel->m_hEnt.Get() )
				pModel->m_hEnt->Release();

			m_AddonModels.Remove( i );
		}
	}

	// Figure out which models we have now.
	int curModelBits = 0;
	FOR_EACH_LL( m_AddonModels, j )
	{
		curModelBits |= (1<<m_AddonModels[j].m_iAddon);
	}

	// Add any new models.
	for ( i=0; i < NUM_ADDON_BITS; i++ )
	{
		if ( (iCurAddonBits & (1<<i)) && !( curModelBits & (1<<i) ) )
		{
			// Ok, we're supposed to have this one.
			CreateAddonModel( i );
		}
	}
}


void C_CSPlayer::RemoveAddonModels()
{
	m_iAddonBits = 0;
	UpdateAddonModels();
}


void C_CSPlayer::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	// Remove all addon models if we go out of the PVS.
	if ( state == SHOULDTRANSMIT_END )
	{
		RemoveAddonModels();

		if( m_pFlashlightBeam != NULL )
		{
			ReleaseFlashlight();
		}
	}

	BaseClass::NotifyShouldTransmit( state );
}


void C_CSPlayer::UpdateSoundEvents()
{
	int iNext;
	for ( int i=m_SoundEvents.Head(); i != m_SoundEvents.InvalidIndex(); i = iNext )
	{
		iNext = m_SoundEvents.Next( i );

		CCSSoundEvent *pEvent = &m_SoundEvents[i];
		if ( gpGlobals->curtime >= pEvent->m_flEventTime )
		{
			CLocalPlayerFilter filter;
			EmitSound( filter, GetSoundSourceIndex(), STRING( pEvent->m_SoundName ) );

			m_SoundEvents.Remove( i );
		}
	}
}

//-----------------------------------------------------------------------------
void C_CSPlayer::UpdateMinModels( void )
{
	int modelIndex = m_nModelIndex;

	// cl_minmodels convar dependent on sv_allowminmodels convar

	if ( !IsVIP() && sv_allowminmodels.GetBool() && cl_minmodels.GetBool() && !IsLocalPlayer() )
	{
		if ( GetTeamNumber() == TEAM_CT )
		{
			int index = cl_min_ct.GetInt() - 1;
			if ( index >= 0 && index < CTPlayerModels.Count() )
			{
				modelIndex = modelinfo->GetModelIndex( CTPlayerModels[index] );
			}
		}
		else if ( GetTeamNumber() == TEAM_TERRORIST )
		{
			int index = cl_min_t.GetInt() - 1;
			if ( index >= 0 && index < TerroristPlayerModels.Count() )
			{
				modelIndex = modelinfo->GetModelIndex( TerroristPlayerModels[index] );
			}
		}
	}

	SetModelByIndex( modelIndex );
}

// NVNT gate for spectating.
static bool inSpectating_Haptics = false;
//-----------------------------------------------------------------------------
void C_CSPlayer::ClientThink()
{
	BaseClass::ClientThink();

	UpdateSoundEvents();

	UpdateAddonModels();

	UpdateIDTarget();

	if ( gpGlobals->curtime >= m_fNextThinkPushAway )
	{
		PerformObstaclePushaway( this );
		m_fNextThinkPushAway =  gpGlobals->curtime + PUSHAWAY_THINK_INTERVAL;
	}

	// NVNT - check for spectating forces
	if ( IsLocalPlayer() )
	{
		if ( GetTeamNumber() == TEAM_SPECTATOR || !this->IsAlive() || GetLocalOrInEyeCSPlayer() != this )
		{
			if (!inSpectating_Haptics)
			{
				if ( haptics )
					haptics->SetNavigationClass("spectate");

				inSpectating_Haptics = true;
			}
		}
		else
		{
			if (inSpectating_Haptics)
			{
				if ( haptics )
					haptics->SetNavigationClass("on_foot");

				inSpectating_Haptics = false;
			}
		}

		if ( m_iObserverMode == OBS_MODE_FREEZECAM )
		{
			//=============================================================================
			// HPE_BEGIN:
			// [Forrest] Added sv_disablefreezecam check
			//=============================================================================
			static ConVarRef sv_disablefreezecam( "sv_disablefreezecam" );
			if ( !m_bPlayingFreezeCamSound && !cl_disablefreezecam.GetBool() && !sv_disablefreezecam.GetBool() )
				//=============================================================================
				// HPE_END
				//=============================================================================
			{
				// Play sound
				m_bPlayingFreezeCamSound = true;

				CLocalPlayerFilter filter;
				EmitSound_t ep;
				ep.m_nChannel = CHAN_VOICE;
				ep.m_pSoundName =  "UI/freeze_cam.wav";
				ep.m_flVolume = VOL_NORM;
				ep.m_SoundLevel = SNDLVL_NORM;
				ep.m_bEmitCloseCaption = false;

				EmitSound( filter, GetSoundSourceIndex(), ep );
			}
		}
		else
		{
			m_bPlayingFreezeCamSound = false;
		}
	}
}


void C_CSPlayer::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );

		if ( IsLocalPlayer() )
		{
			if ( CSGameRules() && CSGameRules()->IsBlackMarket() )
			{
				CSGameRules()->m_pPrices = NULL;
				CSGameRules()->m_StringTableBlackMarket = NULL;
				CSGameRules()->GetBlackMarketPriceList();

				CSGameRules()->SetBlackMarketPrices( false );
			}
		}
	}

	UpdateVisibility();
}


void C_CSPlayer::ValidateModelIndex( void )
{
	UpdateMinModels();
}


void C_CSPlayer::PostDataUpdate( DataUpdateType_t updateType )
{
	// C_BaseEntity assumes we're networking the entity's angles, so pretend that it
	// networked the same value we already have.
	SetNetworkAngles( GetLocalAngles() );

	BaseClass::PostDataUpdate( updateType );
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_CSPlayer::Interpolate( float currentTime )
{
	if ( !BaseClass::Interpolate( currentTime ) )
		return false;

	if ( CSGameRules()->IsFreezePeriod() )
	{
		// don't interpolate players position during freeze period
		SetAbsOrigin( GetNetworkOrigin() );
	}

	return true;
}

int	C_CSPlayer::GetMaxHealth() const
{
	return 100;
}

//-----------------------------------------------------------------------------
// Purpose: Return the local player, or the player being spectated in-eye
//-----------------------------------------------------------------------------
C_CSPlayer* GetLocalOrInEyeCSPlayer( void )
{
	C_CSPlayer *player = C_CSPlayer::GetLocalCSPlayer();

	if( player && player->GetObserverMode() == OBS_MODE_IN_EYE )
	{
		C_BaseEntity *target = player->GetObserverTarget();

		if( target && target->IsPlayer() )
		{
			return ToCSPlayer( target );
		}
	}
	return player;
}

#define MAX_FLASHBANG_OPACITY 75.0f

//-----------------------------------------------------------------------------
// Purpose: Update this client's targetid entity
//-----------------------------------------------------------------------------
void C_CSPlayer::UpdateIDTarget()
{
	if ( !IsLocalPlayer() )
		return;

	// Clear old target and find a new one
	m_iIDEntIndex = 0;

	// don't show IDs if mp_playerid == 2
	if ( mp_playerid.GetInt() == 2 )
		return;

	// don't show IDs if mp_fadetoblack is on
	if ( mp_fadetoblack.GetBool() && !IsAlive() )
		return;

	// don't show IDs in chase spec mode
	if ( GetObserverMode() == OBS_MODE_CHASE ||
		 GetObserverMode() == OBS_MODE_DEATHCAM )
		 return;

	//Check how much of a screen fade we have.
	//if it's more than 75 then we can't see what's going on so we don't display the id.
	byte color[4];
	bool blend;
	vieweffects->GetFadeParams( &color[0], &color[1], &color[2], &color[3], &blend );

	if ( color[3] > MAX_FLASHBANG_OPACITY && ( IsAlive() || GetObserverMode() == OBS_MODE_IN_EYE ) )
		 return;

	trace_t tr;
	Vector vecStart, vecEnd;
	VectorMA( MainViewOrigin(), 2500, MainViewForward(), vecEnd );
	VectorMA( MainViewOrigin(), 10,   MainViewForward(), vecStart );
	UTIL_TraceLine( vecStart, vecEnd, MASK_VISIBLE_AND_NPCS, GetLocalOrInEyeCSPlayer(), COLLISION_GROUP_NONE, &tr );
	if ( !tr.startsolid && !tr.DidHitNonWorldEntity() )
	{
		CTraceFilterSimple filter( GetLocalOrInEyeCSPlayer(), COLLISION_GROUP_NONE );

		// Check for player hitboxes extending outside their collision bounds
		const float rayExtension = 40.0f;
		UTIL_ClipTraceToPlayers(vecStart, vecEnd + MainViewForward() * rayExtension, MASK_SOLID|CONTENTS_HITBOX, &filter, &tr );
	}

	if ( !tr.startsolid && tr.DidHitNonWorldEntity() )
	{
		C_BaseEntity *pEntity = tr.m_pEnt;

		if ( pEntity && (pEntity != this) )
		{
			if ( mp_playerid.GetInt() == 1 ) // only show team names
			{
				if ( pEntity->GetTeamNumber() != GetTeamNumber() )
				{
					return;
				}
			}

			//Adrian: If there's a smoke cloud in my way, don't display the name
			//We check this AFTER we found a player, just so we don't go thru this for nothing.
			for ( int i = 0; i < m_SmokeGrenades.Count(); i++ )
			{
				C_BaseParticleEntity *pSmokeGrenade = (C_BaseParticleEntity*)m_SmokeGrenades.Element( i );

				if ( pSmokeGrenade )
				{
					float flHit1, flHit2;

					float flRadius = ( SMOKEGRENADE_PARTICLERADIUS * NUM_PARTICLES_PER_DIMENSION + 1 ) * 0.5f;

					Vector vPos = pSmokeGrenade->GetAbsOrigin();

					/*debugoverlay->AddBoxOverlay( pSmokeGrenade->GetAbsOrigin(), Vector( flRadius, flRadius, flRadius ),
					 Vector( -flRadius, -flRadius, -flRadius ), QAngle( 0, 0, 0 ), 255, 0, 0, 255, 0.2 );*/

					if ( IntersectInfiniteRayWithSphere( MainViewOrigin(), MainViewForward(), vPos, flRadius, &flHit1, &flHit2 ) )
					{
						 return;
					}
				}
			}

			if ( !GetIDTarget() && ( !m_iOldIDEntIndex || m_holdTargetIDTimer.IsElapsed() ) )
			{
				// track when we first mouse over the target
				m_delayTargetIDTimer.Start( mp_playerid_delay.GetFloat() );
			}
			m_iIDEntIndex = pEntity->entindex();

			m_iOldIDEntIndex = m_iIDEntIndex;
			m_holdTargetIDTimer.Start( mp_playerid_hold.GetFloat() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handling
//-----------------------------------------------------------------------------
bool C_CSPlayer::CreateMove( float flInputSampleTime, CUserCmd *pCmd )
{
	// Bleh... we will wind up needing to access bones for attachments in here.
	C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, true );

	return BaseClass::CreateMove( flInputSampleTime, pCmd );
}

//-----------------------------------------------------------------------------
// Purpose: Flash this entity on the radar
//-----------------------------------------------------------------------------
bool C_CSPlayer::IsInHostageRescueZone()
{
	return 	m_bInHostageRescueZone;
}

CWeaponCSBase* C_CSPlayer::GetActiveCSWeapon() const
{
	return dynamic_cast< CWeaponCSBase* >( GetActiveWeapon() );
}

CWeaponCSBase* C_CSPlayer::GetCSWeapon( CSWeaponID id ) const
{
	for (int i=0;i<MAX_WEAPONS;i++)
	{
		CBaseCombatWeapon *weapon = GetWeapon( i );
		if ( weapon )
		{
			CWeaponCSBase *csWeapon = dynamic_cast< CWeaponCSBase * >( weapon );
			if ( csWeapon )
			{
				if ( id == csWeapon->GetWeaponID() )
				{
					return csWeapon;
				}
			}
		}
	}

	return NULL;
}

//REMOVEME
/*
void C_CSPlayer::SetFireAnimation( PLAYER_ANIM playerAnim )
{
	Activity idealActivity = ACT_WALK;

	// Figure out stuff about the current state.
	float speed = GetAbsVelocity().Length2D();
	bool isMoving = ( speed != 0.0f ) ? true : false;
	bool isDucked = ( GetFlags() & FL_DUCKING ) ? true : false;
	bool isStillJumping = false; //!( GetFlags() & FL_ONGROUND );
	bool isRunning = false;

	if ( speed > ARBITRARY_RUN_SPEED )
	{
		isRunning = true;
	}

	// Now figure out what to do based on the current state and the new state.
	switch ( playerAnim )
	{
	default:
	case PLAYER_RELOAD:
	case PLAYER_ATTACK1:
	case PLAYER_IDLE:
	case PLAYER_WALK:
		// Are we still jumping?
		// If so, keep playing the jump animation.
		if ( !isStillJumping )
		{
			idealActivity = ACT_WALK;

			if ( isDucked )
			{
				idealActivity = !isMoving ? ACT_CROUCHIDLE : ACT_RUN_CROUCH;
			}
			else
			{
				if ( isRunning )
				{
					idealActivity = ACT_RUN;
				}
				else
				{
					idealActivity = isMoving ? ACT_WALK : ACT_IDLE;
				}
			}

			// Allow body yaw to override for standing and turning in place
			idealActivity = m_PlayerAnimState.BodyYawTranslateActivity( idealActivity );
		}
		break;

	case PLAYER_JUMP:
		idealActivity = ACT_HOP;
		break;

	case PLAYER_DIE:
		// Uses Ragdoll now???
		idealActivity = ACT_DIESIMPLE;
		break;

	// FIXME:  Use overlays for reload, start/leave aiming, attacking
	case PLAYER_START_AIMING:
	case PLAYER_LEAVE_AIMING:
		idealActivity = ACT_WALK;
		break;
	}

	CWeaponCSBase *pWeapon = GetActiveCSWeapon();

	if ( pWeapon )
	{
		Activity aWeaponActivity = idealActivity;

		if ( playerAnim == PLAYER_ATTACK1 )
		{
			switch ( idealActivity )
			{
				case ACT_WALK:
				default:
					aWeaponActivity = ACT_PLAYER_WALK_FIRE;
					break;
				case ACT_RUN:
					aWeaponActivity = ACT_PLAYER_RUN_FIRE;
					break;
				case ACT_IDLE:
					aWeaponActivity = ACT_PLAYER_IDLE_FIRE;
					break;
				case ACT_CROUCHIDLE:
					aWeaponActivity = ACT_PLAYER_CROUCH_FIRE;
					break;
				case ACT_RUN_CROUCH:
					aWeaponActivity = ACT_PLAYER_CROUCH_WALK_FIRE;
					break;
			}
		}

		m_PlayerAnimState.SetWeaponLayerSequence( pWeapon->GetCSWpnData().m_szAnimExtension, aWeaponActivity );
	}
}
*/

ShadowType_t C_CSPlayer::ShadowCastType( void )
{
	if ( !IsVisible() )
		 return SHADOWS_NONE;

	return SHADOWS_RENDER_TO_TEXTURE_DYNAMIC;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not we can switch to the given weapon.
// Input  : pWeapon -
//-----------------------------------------------------------------------------
bool C_CSPlayer::Weapon_CanSwitchTo( CBaseCombatWeapon *pWeapon )
{
	if ( !pWeapon->CanDeploy() )
		return false;

	if ( GetActiveWeapon() )
	{
		if ( !GetActiveWeapon()->CanHolster() )
			return false;
	}

	return true;
}


void C_CSPlayer::UpdateClientSideAnimation()
{
	// We do this in a different order than the base class.
	// We need our cycle to be valid for when we call the playeranimstate update code,
	// or else it'll synchronize the upper body anims with the wrong cycle.
	if ( GetSequence() != -1 )
	{
		// move frame forward
		FrameAdvance( 0.0f ); // 0 means to use the time we last advanced instead of a constant
	}

	// Update the animation data. It does the local check here so this works when using
	// a third-person camera (and we don't have valid player angles).
	if ( this == C_CSPlayer::GetLocalCSPlayer() )
		m_PlayerAnimState->Update( EyeAngles()[YAW], m_angEyeAngles[PITCH] );
	else
		m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );

	if ( GetSequence() != -1 )
	{
		// latch old values
		OnLatchInterpolatedVariables( LATCH_ANIMATION_VAR );
	}
}


float g_flMuzzleFlashScale=1;

void C_CSPlayer::ProcessMuzzleFlashEvent()
{
	CBasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

	// Reenable when the weapons have muzzle flash attachments in the right spot.
	if ( this == pLocalPlayer )
		return; // don't show own world muzzle flashs in for localplayer

	if ( pLocalPlayer && pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
	{
		// also don't show in 1st person spec mode
		if ( pLocalPlayer->GetObserverTarget() == this )
			return;
	}

	CWeaponCSBase *pWeapon = GetActiveCSWeapon();

	if ( !pWeapon )
		return;

	bool hasMuzzleFlash = (pWeapon->GetMuzzleFlashStyle() != CS_MUZZLEFLASH_NONE);

	Vector vector;
	QAngle angles;

	int iAttachment = LookupAttachment( "muzzle_flash" );

	if ( iAttachment >= 0 )
	{
		bool bFoundAttachment = GetAttachment( iAttachment, vector, angles );
		// If we have an attachment, then stick a light on it.
		if ( bFoundAttachment )
		{
			if ( hasMuzzleFlash )
			{
				dlight_t *el = effects->CL_AllocDlight( LIGHT_INDEX_MUZZLEFLASH + index );
				el->origin = vector;
				el->radius = 70;
				el->decay = el->radius / 0.05f;
				el->die = gpGlobals->curtime + 0.05f;
				el->color.r = 255;
				el->color.g = 192;
				el->color.b = 64;
				el->color.exponent = 5;
			}

			int shellType = GetShellForAmmoType( pWeapon->GetCSWpnData().szAmmo1 );

			QAngle playerAngle = EyeAngles();
			Vector vForward, vRight, vUp;

			AngleVectors( playerAngle, &vForward, &vRight, &vUp );

			QAngle angVelocity;
			Vector vVel = vRight * 100 + vUp * 20;
			VectorAngles( vVel, angVelocity );

			if ( pWeapon->GetMaxClip1() > 0 )
			{
				tempents->CSEjectBrass( vector, angVelocity, 120, shellType, this  );
			}
		}
	}

	if ( hasMuzzleFlash )
	{
		iAttachment = pWeapon->GetMuzzleAttachment();

		if ( iAttachment > 0 )
		{
			float flScale = pWeapon->GetCSWpnData().m_flMuzzleScale;
			flScale *= 0.75;
			FX_MuzzleEffectAttached( flScale, pWeapon->GetRefEHandle(), iAttachment, NULL, false );

		}
	}
}

const QAngle& C_CSPlayer::EyeAngles()
{
	if ( IsLocalPlayer() && !g_nKillCamMode )
	{
		return BaseClass::EyeAngles();
	}
	else
	{
		return m_angEyeAngles;
	}
}

bool C_CSPlayer::ShouldDraw( void )
{
	// If we're dead, our ragdoll will be drawn for us instead.
	if ( !IsAlive() )
		return false;

	if( GetTeamNumber() == TEAM_SPECTATOR )
		return false;

	if( IsLocalPlayer() )
	{
		if ( IsRagdoll() )
			return true;
	}

	return BaseClass::ShouldDraw();
}


bool FindWeaponAttachmentBone( C_BaseCombatWeapon *pWeapon, int &iWeaponBone )
{
	if ( !pWeapon )
		return false;

	CStudioHdr *pHdr = pWeapon->GetModelPtr();
	if ( !pHdr )
		return false;

	for ( iWeaponBone=0; iWeaponBone < pHdr->numbones(); iWeaponBone++ )
	{
		if ( stricmp( pHdr->pBone( iWeaponBone )->pszName(), "L_Hand_Attach" ) == 0 )
			break;
	}

	return iWeaponBone != pHdr->numbones();
}


bool FindMyAttachmentBone( C_BaseAnimating *pModel, int &iBone, CStudioHdr *pHdr )
{
	if ( !pHdr )
		return false;

	for ( iBone=0; iBone < pHdr->numbones(); iBone++ )
	{
		if ( stricmp( pHdr->pBone( iBone )->pszName(), "Valvebiped.Bip01_L_Hand" ) == 0 )
			break;
	}

	return iBone != pHdr->numbones();
}


inline bool IsBoneChildOf( CStudioHdr *pHdr, int iBone, int iParent )
{
	if ( iBone == iParent )
		return false;

	while ( iBone != -1 )
	{
		if ( iBone == iParent )
			return true;

		iBone = pHdr->pBone( iBone )->parent;
	}
	return false;
}

void ApplyDifferenceTransformToChildren(
	C_BaseAnimating *pModel,
	const matrix3x4_t &mSource,
	const matrix3x4_t &mDest,
	int iParentBone )
{
	CStudioHdr *pHdr = pModel->GetModelPtr();
	if ( !pHdr )
		return;

	// Build a matrix to go from mOriginalHand to mHand.
	// ( mDest * Inverse( mSource ) ) * mSource = mDest
	matrix3x4_t mSourceInverse, mToDest;
	MatrixInvert( mSource, mSourceInverse );
	ConcatTransforms( mDest, mSourceInverse, mToDest );

	// Now multiply iMyBone and all its children by mToWeaponBone.
	for ( int i=0; i < pHdr->numbones(); i++ )
	{
		if ( IsBoneChildOf( pHdr, i, iParentBone ) )
		{
			matrix3x4_t &mCur = pModel->GetBoneForWrite( i );
			matrix3x4_t mNew;
			ConcatTransforms( mToDest, mCur, mNew );
			mCur = mNew;
		}
	}
}


void GetCorrectionMatrices(
	const matrix3x4_t &mShoulder,
	const matrix3x4_t &mElbow,
	const matrix3x4_t &mHand,
	matrix3x4_t &mShoulderCorrection,
	matrix3x4_t &mElbowCorrection
	)
{
	extern void Studio_AlignIKMatrix( matrix3x4_t &mMat, const Vector &vAlignTo );

	// Get the positions of each node so we can get the direction vectors.
	Vector vShoulder, vElbow, vHand;
	MatrixPosition( mShoulder, vShoulder );
	MatrixPosition( mElbow, vElbow );
	MatrixPosition( mHand, vHand );

	// Get rid of the translation.
	matrix3x4_t mOriginalShoulder = mShoulder;
	matrix3x4_t mOriginalElbow = mElbow;
	MatrixSetColumn( Vector( 0, 0, 0 ), 3, mOriginalShoulder );
	MatrixSetColumn( Vector( 0, 0, 0 ), 3, mOriginalElbow );

	// Let the IK code align them like it would if we did IK on the joint.
	matrix3x4_t mAlignedShoulder = mOriginalShoulder;
	matrix3x4_t mAlignedElbow = mOriginalElbow;
	Studio_AlignIKMatrix( mAlignedShoulder, vElbow-vShoulder );
	Studio_AlignIKMatrix( mAlignedElbow, vHand-vElbow );

	// Figure out the transformation from the aligned bones to the original ones.
	matrix3x4_t mInvAlignedShoulder, mInvAlignedElbow;
	MatrixInvert( mAlignedShoulder, mInvAlignedShoulder );
	MatrixInvert( mAlignedElbow, mInvAlignedElbow );

	ConcatTransforms( mInvAlignedShoulder, mOriginalShoulder, mShoulderCorrection );
	ConcatTransforms( mInvAlignedElbow, mOriginalElbow, mElbowCorrection );
}


void C_CSPlayer::BuildTransformations( CStudioHdr *pHdr, Vector *pos, Quaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed )
{
	// First, setup our model's transformations like normal.
	BaseClass::BuildTransformations( pHdr, pos, q, cameraTransform, boneMask, boneComputed );

	if ( IsLocalPlayer() && !C_BasePlayer::ShouldDrawLocalPlayer() )
		return;

	if ( !cl_left_hand_ik.GetInt() )
		return;

	// If our current weapon has a bone named L_Hand_Attach, then we attach the player's
	// left hand (Valvebiped.Bip01_L_Hand) to it.
	C_BaseCombatWeapon *pWeapon = GetActiveWeapon();

	if ( !pWeapon )
		return;

	// Have the weapon setup its bones.
	pWeapon->SetupBones( NULL, 0, BONE_USED_BY_ANYTHING, gpGlobals->curtime );

	int iWeaponBone = 0;
	if ( FindWeaponAttachmentBone( pWeapon, iWeaponBone ) )
	{
		int iMyBone = 0;
		if ( FindMyAttachmentBone( this, iMyBone, pHdr ) )
		{
			int iHand = iMyBone;
			int iElbow = pHdr->pBone( iHand )->parent;
			int iShoulder = pHdr->pBone( iElbow )->parent;
			matrix3x4_t *pBones = &GetBoneForWrite( 0 );

			// Store off the original hand position.
			matrix3x4_t mSource = pBones[iHand];


			// Figure out the rotation offset from the current shoulder and elbow bone rotations
			// and what the IK code's alignment code is going to produce, because we'll have to
			// re-apply that offset after the IK runs.
			matrix3x4_t mShoulderCorrection, mElbowCorrection;
			GetCorrectionMatrices( pBones[iShoulder], pBones[iElbow], pBones[iHand], mShoulderCorrection, mElbowCorrection );


			// Do the IK solution.
			Vector vHandTarget;
			MatrixPosition( pWeapon->GetBone( iWeaponBone ), vHandTarget );
			Studio_SolveIK( iShoulder, iElbow, iHand, vHandTarget, pBones );


			// Now reapply the rotation correction.
			matrix3x4_t mTempShoulder = pBones[iShoulder];
			matrix3x4_t mTempElbow = pBones[iElbow];
			ConcatTransforms( mTempShoulder, mShoulderCorrection, pBones[iShoulder] );
			ConcatTransforms( mTempElbow, mElbowCorrection, pBones[iElbow] );


			// Now apply the transformation on the hand to the fingers.
			matrix3x4_t &mDest = GetBoneForWrite( iHand );
			ApplyDifferenceTransformToChildren( this, mSource, mDest, iHand );
		}
	}
}


C_BaseAnimating * C_CSPlayer::BecomeRagdollOnClient()
{
	return NULL;
}


IRagdoll* C_CSPlayer::GetRepresentativeRagdoll() const
{
	if ( m_hRagdoll.Get() )
	{
		C_CSRagdoll *pRagdoll = (C_CSRagdoll*)m_hRagdoll.Get();

		return pRagdoll->GetIRagdoll();
	}
	else
	{
		return NULL;
	}
}


void C_CSPlayer::PlayReloadEffect()
{
	// Only play the effect for other players.
	if ( this == C_CSPlayer::GetLocalCSPlayer() )
	{
		Assert( false ); // We shouldn't have been sent this message.
		return;
	}

	// Get the view model for our current gun.
	CWeaponCSBase *pWeapon = GetActiveCSWeapon();
	if ( !pWeapon )
		return;

	// The weapon needs two models, world and view, but can only cache one. Synthesize the other.
	const CCSWeaponInfo &info = pWeapon->GetCSWpnData();
	const model_t *pModel = modelinfo->GetModel( modelinfo->GetModelIndex( info.szViewModel ) );
	if ( !pModel )
		return;
	CStudioHdr studioHdr( modelinfo->GetStudiomodel( pModel ), mdlcache );
	if ( !studioHdr.IsValid() )
		return;

	// Find the reload animation.
	for ( int iSeq=0; iSeq < studioHdr.GetNumSeq(); iSeq++ )
	{
		mstudioseqdesc_t *pSeq = &studioHdr.pSeqdesc( iSeq );

		if ( pSeq->activity == ACT_VM_RELOAD )
		{
			float poseParameters[MAXSTUDIOPOSEPARAM];
			memset( poseParameters, 0, sizeof( poseParameters ) );
			float cyclesPerSecond = Studio_CPS( &studioHdr, *pSeq, iSeq, poseParameters );

			// Now read out all the sound events with their timing
			for ( int iEvent=0; iEvent < pSeq->numevents; iEvent++ )
			{
				mstudioevent_t *pEvent = pSeq->pEvent( iEvent );

				if ( pEvent->event == CL_EVENT_SOUND )
				{
					CCSSoundEvent event;
					event.m_SoundName = pEvent->options;
					event.m_flEventTime = gpGlobals->curtime + pEvent->cycle / cyclesPerSecond;
					m_SoundEvents.AddToTail( event );
				}
			}

			break;
		}
	}
}

void C_CSPlayer::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	if ( event == PLAYERANIMEVENT_THROW_GRENADE )
	{
		// Let the server handle this event. It will update m_iThrowGrenadeCounter and the client will
		// pick up the event in CCSPlayerAnimState.
	}
	else
	{
		m_PlayerAnimState->DoAnimationEvent( event, nData );
	}
}

void C_CSPlayer::FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options )
{
	if( event == 7001 )
	{
		bool bInWater = ( enginetrace->GetPointContents(origin) & CONTENTS_WATER );

		//Msg( "run event ( %d )\n", bInWater ? 1 : 0 );

		if( bInWater )
		{
			//run splash
			CEffectData data;

			//trace up from foot position to the water surface
			trace_t tr;
			Vector vecTrace(0,0,1024);
			UTIL_TraceLine( origin, origin + vecTrace, MASK_WATER, NULL, COLLISION_GROUP_NONE, &tr );
			if ( tr.fractionleftsolid )
			{
				data.m_vOrigin = origin + (vecTrace * tr.fractionleftsolid);
			}
			else
			{
				data.m_vOrigin = origin;
			}

			data.m_vNormal = Vector( 0,0,1 );
			data.m_flScale = random->RandomFloat( 4.0f, 5.0f );
			DispatchEffect( "watersplash", data );
		}
	}
	else if( event == 7002 )
	{
		bool bInWater = ( enginetrace->GetPointContents(origin) & CONTENTS_WATER );

		//Msg( "walk event ( %d )\n", bInWater ? 1 : 0 );

		if( bInWater )
		{
			//walk ripple
			CEffectData data;

			//trace up from foot position to the water surface
			trace_t tr;
			Vector vecTrace(0,0,1024);
			UTIL_TraceLine( origin, origin + vecTrace, MASK_WATER, NULL, COLLISION_GROUP_NONE, &tr );
			if ( tr.fractionleftsolid )
			{
				data.m_vOrigin = origin + (vecTrace * tr.fractionleftsolid);
			}
			else
			{
				data.m_vOrigin = origin;
			}

			data.m_vNormal = Vector( 0,0,1 );
			data.m_flScale = random->RandomFloat( 4.0f, 7.0f );
			DispatchEffect( "waterripple", data );
		}
	}
	else
		BaseClass::FireEvent( origin, angles, event, options );
}


void C_CSPlayer::SetActivity( Activity eActivity )
{
	m_Activity = eActivity;
}


Activity C_CSPlayer::GetActivity() const
{
	return m_Activity;
}


const Vector& C_CSPlayer::GetRenderOrigin( void )
{
	if ( m_hRagdoll.Get() )
	{
		C_CSRagdoll *pRagdoll = (C_CSRagdoll*)m_hRagdoll.Get();
		if ( pRagdoll->IsInitialized() )
			return pRagdoll->GetRenderOrigin();
	}

	return BaseClass::GetRenderOrigin();
}


void C_CSPlayer::Simulate( void )
{
	if( this != C_BasePlayer::GetLocalPlayer() )
	{
		if ( IsEffectActive( EF_DIMLIGHT ) )
		{
			QAngle eyeAngles = EyeAngles();
			Vector vForward;
			AngleVectors( eyeAngles, &vForward );

			int iAttachment = LookupAttachment( "muzzle_flash" );

			if ( iAttachment < 0 )
				return;

			Vector vecOrigin;
			QAngle dummy;
			GetAttachment( iAttachment, vecOrigin, dummy );

			trace_t tr;
			UTIL_TraceLine( vecOrigin, vecOrigin + (vForward * 200), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

			if( !m_pFlashlightBeam )
			{
				BeamInfo_t beamInfo;
				beamInfo.m_nType = TE_BEAMPOINTS;
				beamInfo.m_vecStart = tr.startpos;
				beamInfo.m_vecEnd = tr.endpos;
				beamInfo.m_pszModelName = "sprites/glow01.vmt";
				beamInfo.m_pszHaloName = "sprites/glow01.vmt";
				beamInfo.m_flHaloScale = 3.0;
				beamInfo.m_flWidth = 8.0f;
				beamInfo.m_flEndWidth = 35.0f;
				beamInfo.m_flFadeLength = 300.0f;
				beamInfo.m_flAmplitude = 0;
				beamInfo.m_flBrightness = 60.0;
				beamInfo.m_flSpeed = 0.0f;
				beamInfo.m_nStartFrame = 0.0;
				beamInfo.m_flFrameRate = 0.0;
				beamInfo.m_flRed = 255.0;
				beamInfo.m_flGreen = 255.0;
				beamInfo.m_flBlue = 255.0;
				beamInfo.m_nSegments = 8;
				beamInfo.m_bRenderable = true;
				beamInfo.m_flLife = 0.5;
				beamInfo.m_nFlags = FBEAM_FOREVER | FBEAM_ONLYNOISEONCE | FBEAM_NOTILE | FBEAM_HALOBEAM;

				m_pFlashlightBeam = beams->CreateBeamPoints( beamInfo );
			}

			if( m_pFlashlightBeam )
			{
				BeamInfo_t beamInfo;
				beamInfo.m_vecStart = tr.startpos;
				beamInfo.m_vecEnd = tr.endpos;
				beamInfo.m_flRed = 255.0;
				beamInfo.m_flGreen = 255.0;
				beamInfo.m_flBlue = 255.0;

				beams->UpdateBeamInfo( m_pFlashlightBeam, beamInfo );

				dlight_t *el = effects->CL_AllocDlight( 0 );
				el->origin = tr.endpos;
				el->radius = 50;
				el->color.r = 200;
				el->color.g = 200;
				el->color.b = 200;
				el->die = gpGlobals->curtime + 0.1;
			}
		}
		else if ( m_pFlashlightBeam )
		{
			ReleaseFlashlight();
		}
	}

	BaseClass::Simulate();
}

void C_CSPlayer::ReleaseFlashlight( void )
{
	if( m_pFlashlightBeam )
	{
		m_pFlashlightBeam->flags = 0;
		m_pFlashlightBeam->die = gpGlobals->curtime - 1;

		m_pFlashlightBeam = NULL;
	}
}

bool C_CSPlayer::HasC4( void )
{
	if( this == C_CSPlayer::GetLocalPlayer() )
	{
		return Weapon_OwnsThisType( "weapon_c4" );
	}
	else
	{
		C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource*)GameResources();

		return pCSPR->HasC4( entindex() );
	}
}

void C_CSPlayer::ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName )
{
	static ConVar *violence_hblood = cvar->FindVar( "violence_hblood" );
	if ( violence_hblood && !violence_hblood->GetBool() )
		return;

	BaseClass::ImpactTrace( pTrace, iDamageType, pCustomImpactName );
}


//-----------------------------------------------------------------------------
void C_CSPlayer::CalcObserverView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov )
{
	/**
	 * TODO: Fix this!
	// CS:S standing eyeheight is above the collision volume, so we need to pull it
	// down when we go into close quarters.
	float maxEyeHeightAboveBounds = VEC_VIEW_SCALED( this ).z - VEC_HULL_MAX_SCALED( this ).z;
	if ( GetObserverMode() == OBS_MODE_IN_EYE &&
		maxEyeHeightAboveBounds > 0.0f &&
		GetObserverTarget() &&
		GetObserverTarget()->IsPlayer() )
	{
		const float eyeClearance = 12.0f; // eye pos must be this far below the ceiling

		C_CSPlayer *target = ToCSPlayer( GetObserverTarget() );

		Vector offset = eyeOrigin - GetAbsOrigin();

		Vector vHullMin = VEC_HULL_MIN_SCALED( this );
		vHullMin.z = 0.0f;
		Vector vHullMax = VEC_HULL_MAX_SCALED( this );

		Vector start = GetAbsOrigin();
		start.z += vHullMax.z;
		Vector end = start;
		end.z += eyeClearance + VEC_VIEW_SCALED( this ).z - vHullMax_SCALED( this ).z;

		vHullMax.z = 0.0f;

		Vector fudge( 1, 1, 0 );
		vHullMin += fudge;
		vHullMax -= fudge;

		trace_t trace;
		Ray_t ray;
		ray.Init( start, end, vHullMin, vHullMax );
		UTIL_TraceRay( ray, MASK_PLAYERSOLID, target, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

		if ( trace.fraction < 1.0f )
		{
			float est = start.z + trace.fraction * (end.z - start.z) - GetAbsOrigin().z - eyeClearance;
			if ( ( target->GetFlags() & FL_DUCKING ) == 0 && !target->GetFallVelocity() && !target->IsDucked() )
			{
				offset.z = est;
			}
			else
			{
				offset.z = MIN( est, offset.z );
			}
			eyeOrigin.z = GetAbsOrigin().z + offset.z;
		}
	}
	*/

	BaseClass::CalcObserverView( eyeOrigin, eyeAngles, fov );
}

//=============================================================================
// HPE_BEGIN:
//=============================================================================
// [tj] checks if this player has another given player on their Steam friends list.
bool C_CSPlayer::HasPlayerAsFriend(C_CSPlayer* player)
{
    if (!steamapicontext || !steamapicontext->SteamFriends() || !steamapicontext->SteamUtils() || !player)
    {
        return false;
    }

    player_info_t pi;
    if ( !engine->GetPlayerInfo( player->entindex(), &pi ) )
    {
        return false;
    }

    if ( !pi.friendsID )
    {
        return false;
    }

    // check and see if they're on the local player's friends list
    CSteamID steamID( pi.friendsID, 1, steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual );
    return steamapicontext->SteamFriends()->HasFriend( steamID, k_EFriendFlagImmediate);
}

// [menglish] Returns whether this player is dominating or is being dominated by the specified player
bool C_CSPlayer::IsPlayerDominated( int iPlayerIndex )
{
	return m_bPlayerDominated.Get( iPlayerIndex );
}

bool C_CSPlayer::IsPlayerDominatingMe( int iPlayerIndex )
{
	return m_bPlayerDominatingMe.Get( iPlayerIndex );
}


// helper interpolation functions
namespace Interpolators
{
	inline float Linear( float t ) { return t; }

	inline float SmoothStep( float t )
	{
		t = 3 * t * t - 2.0f * t * t * t;
		return t;
	}

	inline float SmoothStep2( float t )
	{
		return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
	}

	inline float SmoothStepStart( float t )
	{
		t = 0.5f * t;
		t = 3 * t * t - 2.0f * t * t * t;
		t = t* 2.0f;
		return t;
	}

	inline float SmoothStepEnd( float t )
	{
		t = 0.5f * t + 0.5f;
		t = 3 * t * t - 2.0f * t * t * t;
		t = (t - 0.5f) * 2.0f;
		return t;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Calculate the view for the player while he's in freeze frame observer mode
//-----------------------------------------------------------------------------
void C_CSPlayer::CalcFreezeCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov )
{
	C_BaseEntity *pTarget = GetObserverTarget();

	//=============================================================================
	// HPE_BEGIN:
	// [Forrest] Added sv_disablefreezecam check
	//=============================================================================
	static ConVarRef sv_disablefreezecam( "sv_disablefreezecam" );
	if ( !pTarget || cl_disablefreezecam.GetBool() || sv_disablefreezecam.GetBool() )
	//=============================================================================
	// HPE_END
	//=============================================================================
	{
		return CalcDeathCamView( eyeOrigin, eyeAngles, fov );
	}

	// pick a zoom camera target
	Vector vLookAt = pTarget->GetObserverCamOrigin();	// Returns ragdoll origin if they're ragdolled
	vLookAt += GetChaseCamViewOffset( pTarget );

	// look over ragdoll, not through
	if ( !pTarget->IsAlive() )
		vLookAt.z += pTarget->GetBaseAnimating() ? VEC_DEAD_VIEWHEIGHT_SCALED( pTarget->GetBaseAnimating() ).z : VEC_DEAD_VIEWHEIGHT.z;

	// Figure out a view position in front of the target
	Vector vEyeOnPlane = eyeOrigin;
	vEyeOnPlane.z = vLookAt.z;
	Vector vToTarget = vLookAt - vEyeOnPlane;
	VectorNormalize( vToTarget );

	// goal position of camera is pulled away from target by m_flFreezeFrameDistance
	Vector vTargetPos = vLookAt - (vToTarget * m_flFreezeFrameDistance);

	// Now trace out from the target, so that we're put in front of any walls
	trace_t trace;
	C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
	UTIL_TraceHull( vLookAt, vTargetPos, WALL_MIN, WALL_MAX, MASK_SOLID, pTarget, COLLISION_GROUP_NONE, &trace );
	C_BaseEntity::PopEnableAbsRecomputations();
	if ( trace.fraction < 1.0 )
	{
		// The camera's going to be really close to the target. So we don't end up
		// looking at someone's chest, aim close freezecams at the target's eyes.
		vTargetPos = trace.endpos;

		// To stop all close in views looking up at character's chins, move the view up.
		vTargetPos.z += fabs(vLookAt.z - vTargetPos.z) * 0.85;
		C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
		UTIL_TraceHull( vLookAt, vTargetPos, WALL_MIN, WALL_MAX, MASK_SOLID, pTarget, COLLISION_GROUP_NONE, &trace );
		C_BaseEntity::PopEnableAbsRecomputations();
		vTargetPos = trace.endpos;
	}

	// Look directly at the target
	vToTarget = vLookAt - vTargetPos;
	VectorNormalize( vToTarget );
	VectorAngles( vToTarget, eyeAngles );

	float fCurTime = gpGlobals->curtime - m_flFreezeFrameStartTime;
	float fInterpolant = clamp( fCurTime / spec_freeze_traveltime.GetFloat(), 0.0f, 1.0f );
	fInterpolant = Interpolators::SmoothStepEnd( fInterpolant );

	// move the eye toward our killer
	VectorLerp( m_vecFreezeFrameStart, vTargetPos, fInterpolant, eyeOrigin );

	if ( fCurTime >= spec_freeze_traveltime.GetFloat() && !m_bSentFreezeFrame )
	{
		IGameEvent *pEvent = gameeventmanager->CreateEvent( "freezecam_started" );
		if ( pEvent )
		{
			gameeventmanager->FireEventClientSide( pEvent );
		}

		m_bSentFreezeFrame = true;
		view->FreezeFrame( spec_freeze_time.GetFloat() );
	}
}

float C_CSPlayer::GetDeathCamInterpolationTime()
{
	static ConVarRef sv_disablefreezecam( "sv_disablefreezecam" );
	if ( cl_disablefreezecam.GetBool() || sv_disablefreezecam.GetBool() || !GetObserverTarget() )
		return spec_freeze_time.GetFloat();
	else
		return CS_DEATH_ANIMATION_TIME;

}


//=============================================================================
// HPE_END
//=============================================================================

