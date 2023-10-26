//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side view model implementation. Responsible for drawing
//			the view model.
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include "c_baseviewmodel.h"
#include "model_types.h"
#include "hud.h"
#include "view_shared.h"
#include "iviewrender.h"
#include "view.h"
#include "mathlib/vmatrix.h"
#include "cl_animevent.h"
#include "eventlist.h"
#include "tools/bonelist.h"
#include <KeyValues.h>
#include "hltvcamera.h"
#include "r_efx.h"
#include "dlight.h"
#include "clientalphaproperty.h"
#include "iinput.h"
#if defined( REPLAY_ENABLED )
#include "replaycamera.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar vm_debug( "vm_debug", "0", FCVAR_CHEAT );
ConVar vm_draw_always( "vm_draw_always", "0" );

void PostToolMessage( HTOOLHANDLE hEntity, KeyValues *msg );
extern float g_flMuzzleFlashScale;

void FormatViewModelAttachment( C_BasePlayer *pPlayer, Vector &vOrigin, bool bInverse )
{
	int nSlot = 0;
	if ( pPlayer ) 
	{
		int nPlayerSlot = C_BasePlayer::GetSplitScreenSlotForPlayer( pPlayer );
		if ( nPlayerSlot == -1 )
		{
			nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
		}
		else
		{
			nSlot = nPlayerSlot;
		}
	}

	Assert( nSlot != -1 );

	// Presumably, SetUpView has been called so we know our FOV and render origin.
	const CViewSetup *pViewSetup = view->GetPlayerViewSetup( nSlot );
	
	float worldx = tan( pViewSetup->fov * M_PI/360.0 );
	float viewx = tan( pViewSetup->fovViewmodel * M_PI/360.0 );

	// aspect ratio cancels out, so only need one factor
	// the difference between the screen coordinates of the 2 systems is the ratio
	// of the coefficients of the projection matrices (tan (fov/2) is that coefficient)
	float factorX = worldx / viewx;

	float factorY = factorX;
	
	// Get the coordinates in the viewer's space.
	Vector tmp = vOrigin - pViewSetup->origin;
	Vector vTransformed( MainViewRight(nSlot).Dot( tmp ), MainViewUp(nSlot).Dot( tmp ), MainViewForward(nSlot).Dot( tmp ) );

	// Now squash X and Y.
	if ( bInverse )
	{
		if ( factorX != 0 && factorY != 0 )
		{
			vTransformed.x /= factorX;
			vTransformed.y /= factorY;
		}
		else
		{
			vTransformed.x = 0.0f;
			vTransformed.y = 0.0f;
		}
	}
	else
	{
		vTransformed.x *= factorX;
		vTransformed.y *= factorY;
	}



	// Transform back to world space.
	Vector vOut = (MainViewRight(nSlot) * vTransformed.x) + (MainViewUp(nSlot) * vTransformed.y) + (MainViewForward(nSlot) * vTransformed.z);
	vOrigin = pViewSetup->origin + vOut;
}


void C_BaseViewModel::FormatViewModelAttachment( int nAttachment, matrix3x4_t &attachmentToWorld )
{
	C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	Vector vecOrigin;
	MatrixPosition( attachmentToWorld, vecOrigin );
	::FormatViewModelAttachment( pPlayer, vecOrigin, false );
	PositionMatrix( vecOrigin, attachmentToWorld );
}

void C_BaseViewModel::UncorrectViewModelAttachment( Vector &vOrigin )
{
	C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	// Unformat the attachment.
	::FormatViewModelAttachment( pPlayer, vOrigin, true );
}


//-----------------------------------------------------------------------------
// Purpose
//-----------------------------------------------------------------------------
void C_BaseViewModel::FireEvent( const Vector& origin, const QAngle& angles, int eventNum, const char *options )
{
	// We override sound requests so that we can play them locally on the owning player
	if ( ( eventNum == AE_CL_PLAYSOUND ) || ( eventNum == CL_EVENT_SOUND ) )
	{
		// Only do this if we're owned by someone
		if ( GetOwner() != NULL )
		{
			CLocalPlayerFilter filter;
			EmitSound( filter, GetOwner()->GetSoundSourceIndex(), options, &GetAbsOrigin() );
			return;
		}
	}

	C_BasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner )
		return;

	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( pOwner );

	// Otherwise pass the event to our associated weapon
	C_BaseCombatWeapon *pWeapon = pOwner->GetActiveWeapon();
	if ( pWeapon )
	{
		bool bResult = pWeapon->OnFireEvent( this, origin, angles, eventNum, options );
		if ( !bResult )
		{
			if ( eventNum == AE_CLIENT_EFFECT_ATTACH && ::input->CAM_IsThirdPerson() )
				return;

			BaseClass::FireEvent( origin, angles, eventNum, options );
		}
	}
}

bool C_BaseViewModel::Interpolate( float currentTime )
{
	CStudioHdr *pStudioHdr = GetModelPtr();
	// Make sure we reset our animation information if we've switch sequences
	UpdateAnimationParity();

	bool bret = BaseClass::Interpolate( currentTime );

	// Hack to extrapolate cycle counter for view model
	float elapsed_time = currentTime - m_flAnimTime;
	C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	// Predicted viewmodels have fixed up interval
	if ( GetPredictable() || IsClientCreated() )
	{
		Assert( pPlayer );
		float curtime = pPlayer ? pPlayer->GetFinalPredictedTime() : gpGlobals->curtime;
		elapsed_time = curtime - m_flAnimTime;
		// Adjust for interpolated partial frame
		elapsed_time += ( gpGlobals->interpolation_amount * TICK_INTERVAL );
	}

	// Prediction errors?	
	if ( elapsed_time < 0 )
	{
		elapsed_time = 0;
	}

	float dt = elapsed_time * GetSequenceCycleRate( pStudioHdr, GetSequence() );
	if ( dt >= 1.0f )
	{
		if ( !IsSequenceLooping( GetSequence() ) )
		{
			dt = 0.999f;
		}
		else
		{
			dt = fmod( dt, 1.0f );
		}
	}

	SetCycle( dt );
	return bret;
}


inline bool C_BaseViewModel::ShouldFlipViewModel()
{
	return false;
}


void C_BaseViewModel::ApplyBoneMatrixTransform( matrix3x4_t& transform )
{
	if ( ShouldFlipViewModel() )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( GetOwner() );

		matrix3x4_t viewMatrix, viewMatrixInverse;

		// We could get MATERIAL_VIEW here, but this is called sometimes before the renderer
		// has set that matrix. Luckily, this is called AFTER the CViewSetup has been initialized.
		const CViewSetup *pSetup = view->GetPlayerViewSetup();
		AngleMatrix( pSetup->angles, pSetup->origin, viewMatrixInverse );
		MatrixInvert( viewMatrixInverse, viewMatrix );

		// Transform into view space.
		matrix3x4_t temp, temp2;
		ConcatTransforms( viewMatrix, transform, temp );
		
		// Flip it along X.
		
		// (This is the slower way to do it, and it equates to negating the top row).
		//matrix3x4_t mScale;
		//SetIdentityMatrix( mScale );
		//mScale[0][0] = 1;
		//mScale[1][1] = -1;
		//mScale[2][2] = 1;
		//ConcatTransforms( mScale, temp, temp2 );
		temp[1][0] = -temp[1][0];
		temp[1][1] = -temp[1][1];
		temp[1][2] = -temp[1][2];
		temp[1][3] = -temp[1][3];

		// Transform back out of view space.
		ConcatTransforms( viewMatrixInverse, temp, transform );
	}
}

//-----------------------------------------------------------------------------
// Purpose: check if weapon viewmodel should be drawn
//-----------------------------------------------------------------------------
bool C_BaseViewModel::ShouldDraw()
{
	if ( g_bEngineIsHLTV )
	{
		return ( HLTVCamera()->GetMode() == OBS_MODE_IN_EYE &&
				 HLTVCamera()->GetPrimaryTarget() == GetOwner()	);
	}
#if defined( REPLAY_ENABLED )
	else if ( engine->IsReplay() )
	{
		return ( ReplayCamera()->GetMode() == OBS_MODE_IN_EYE &&
				 ReplayCamera()->GetPrimaryTarget() == GetOwner() );
	}
#endif
	else
	{
		Assert( !IsEffectActive( EF_NODRAW ) );
		Assert(	GetRenderMode() != kRenderNone );

		if ( vm_draw_always.GetBool() )
			return true;
		if ( GetOwner() != C_BasePlayer::GetLocalPlayer() )
			return false;

		return BaseClass::ShouldDraw();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Render the weapon. Draw the Viewmodel if the weapon's being carried
//			by this player, otherwise draw the worldmodel.
//-----------------------------------------------------------------------------
int C_BaseViewModel::DrawModel( int flags, const RenderableInstance_t &instance )
{
	if ( !m_bReadyToDraw )
		return 0;

	if ( flags & STUDIO_RENDER )
	{
		// Determine blending amount and tell engine
		float blend = (float)( instance.m_nAlpha / 255.0f );

		// Totally gone
		if ( blend <= 0.0f )
			return 0;

		// Tell engine
		render->SetBlend( blend );

		float color[3];
		GetColorModulation( color );
		render->SetColorModulation(	color );
	}
	
	CMatRenderContextPtr pRenderContext( materials );
	
	if ( ShouldFlipViewModel() )
		pRenderContext->CullMode( MATERIAL_CULLMODE_CW );

	int ret = 0;
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	C_BaseCombatWeapon *pWeapon = GetOwningWeapon();

	// If the local player's overriding the viewmodel rendering, let him do it
	if ( pPlayer && pPlayer->IsOverridingViewmodel() )
	{
		ret = pPlayer->DrawOverriddenViewmodel( this, flags, instance );
	}
	else if ( pWeapon && pWeapon->IsOverridingViewmodel() )
	{
		ret = pWeapon->DrawOverriddenViewmodel( this, flags, instance );
	}
	else
	{
		ret = BaseClass::DrawModel( flags, instance );
	}

	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );

	// Now that we've rendered, reset the animation restart flag
	if ( flags & STUDIO_RENDER )
	{
		if ( m_nOldAnimationParity != m_nAnimationParity )
		{
			m_nOldAnimationParity = m_nAnimationParity;
		}

		// Tell the weapon itself that we've rendered, in case it wants to do something
		if ( pWeapon )
		{
			pWeapon->ViewModelDrawn( this );
		}

		if ( vm_debug.GetBool() )
		{
			MDLCACHE_CRITICAL_SECTION();

			int line = 16;
			CStudioHdr *hdr = GetModelPtr();
			engine->Con_NPrintf( line++, "%s: %s(%d), cycle: %.2f cyclerate: %.2f playbackrate: %.2f\n", 
				(hdr)?hdr->pszName():"(null)",
				GetSequenceName( GetSequence() ),
				GetSequence(),
				GetCycle(), 
				GetSequenceCycleRate( hdr, GetSequence() ),
				GetPlaybackRate()
				);
			if ( hdr )
			{
				for( int i=0; i < hdr->GetNumPoseParameters(); ++i )
				{
					const mstudioposeparamdesc_t &Pose = hdr->pPoseParameter( i );
					engine->Con_NPrintf( line++, "pose_param %s: %f",
						Pose.pszName(), GetPoseParameter( i ) );
				}
			}

			// Determine blending amount and tell engine
			float blend = (float)( instance.m_nAlpha / 255.0f );
			float color[3];
			GetColorModulation( color );
			engine->Con_NPrintf( line++, "blend=%f, color=%f,%f,%f", blend, color[0], color[1], color[2] );
			engine->Con_NPrintf( line++, "GetRenderMode()=%d", GetRenderMode() );
			engine->Con_NPrintf( line++, "m_nRenderFX=0x%8.8X", GetRenderFX() );

			color24 c = GetRenderColor();
			unsigned char a = GetRenderAlpha();
			engine->Con_NPrintf( line++, "rendercolor=%d,%d,%d,%d", c.r, c.g, c.b, a );

			engine->Con_NPrintf( line++, "origin=%f, %f, %f", GetRenderOrigin().x, GetRenderOrigin().y, GetRenderOrigin().z );
			engine->Con_NPrintf( line++, "angles=%f, %f, %f", GetRenderAngles()[0], GetRenderAngles()[1], GetRenderAngles()[2] );

			if ( IsEffectActive( EF_NODRAW ) )
			{
				engine->Con_NPrintf( line++, "EF_NODRAW" );
			}
		}
	}

	return ret;
}

//-----------------------------------------------------------------------------
// Purpose: Called by the player when the player's overriding the viewmodel drawing. Avoids infinite recursion.
//-----------------------------------------------------------------------------
int C_BaseViewModel::DrawOverriddenViewmodel( int flags, const RenderableInstance_t &instance )
{
	return BaseClass::DrawModel( flags, instance );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
uint8 C_BaseViewModel::OverrideAlphaModulation( uint8 nAlpha )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( GetOwner() );

	// See if the local player wants to override the viewmodel's rendering
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer && pPlayer->IsOverridingViewmodel() )
		return pPlayer->AlphaProp()->ComputeRenderAlpha();
	
	C_BaseCombatWeapon *pWeapon = GetOwningWeapon();
	if ( pWeapon && pWeapon->IsOverridingViewmodel() )
		return pWeapon->AlphaProp()->ComputeRenderAlpha();

	return nAlpha;

}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
RenderableTranslucencyType_t C_BaseViewModel::ComputeTranslucencyType( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( GetOwner() );

	// See if the local player wants to override the viewmodel's rendering
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer && pPlayer->IsOverridingViewmodel() )
		return pPlayer->ComputeTranslucencyType();

	C_BaseCombatWeapon *pWeapon = GetOwningWeapon();
	if ( pWeapon && pWeapon->IsOverridingViewmodel() )
		return pWeapon->ComputeTranslucencyType();

	return BaseClass::ComputeTranslucencyType();

}


//-----------------------------------------------------------------------------
// Purpose: If the animation parity of the weapon has changed, we reset cycle to avoid popping
//-----------------------------------------------------------------------------
void C_BaseViewModel::UpdateAnimationParity( void )
{
	C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	
	// If we're predicting, then we don't use animation parity because we change the animations on the clientside
	// while predicting. When not predicting, only the server changes the animations, so a parity mismatch
	// tells us if we need to reset the animation.
	if ( m_nOldAnimationParity != m_nAnimationParity && !GetPredictable() )
	{
		float curtime = (pPlayer && IsIntermediateDataAllocated()) ? pPlayer->GetFinalPredictedTime() : gpGlobals->curtime;
		// FIXME: this is bad
		// Simulate a networked m_flAnimTime and m_flCycle
		// FIXME:  Do we need the magic 0.1?
		SetCycle( 0.0f ); // GetSequenceCycleRate( GetSequence() ) * 0.1;
		m_flAnimTime = curtime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update global map state based on data received
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void C_BaseViewModel::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		AlphaProp()->EnableAlphaModulationOverride( true );
	}

	SetPredictionEligible( true );
	BaseClass::OnDataChanged(updateType);
}
void C_BaseViewModel::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate(updateType);
	OnLatchInterpolatedVariables( LATCH_ANIMATION_VAR );
}

//-----------------------------------------------------------------------------
// Purpose: Return the player who will predict this entity
//-----------------------------------------------------------------------------
CBasePlayer *C_BaseViewModel::GetPredictionOwner()
{
	return ToBasePlayer( GetOwner() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseViewModel::GetBoneControllers(float controllers[MAXSTUDIOBONECTRLS])
{
	BaseClass::GetBoneControllers( controllers );

	// Tell the weapon itself that we've rendered, in case it wants to do something
	C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	C_BaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
	if ( pWeapon )
	{
		pWeapon->GetViewmodelBoneControllers( this, controllers );
	}
}

