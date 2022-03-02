//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Responsible for drawing the scene
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "view.h"
#include "iviewrender.h"
#include "view_shared.h"
#include "ivieweffects.h"
#include "iinput.h"
#include "model_types.h"
#include "clientsideeffects.h"
#include "particlemgr.h"
#include "viewrender.h"
#include "iclientmode.h"
#include "voice_status.h"
#include "radio_status.h"
#include "glow_overlay.h"
#include "materialsystem/imesh.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "detailobjectsystem.h"
#include "tier0/vprof.h"
#include "engine/IEngineTrace.h"
#include "engine/ivmodelinfo.h"
#include "view_scene.h"
#include "particles_ez.h"
#include "engine/IStaticPropMgr.h"
#include "engine/ivdebugoverlay.h"
#include "cs_view_scene.h"
#include "c_cs_player.h"
#include "cs_gamerules.h"
#include "shake.h"
#include "clienteffectprecachesystem.h"
#include <vgui/ISurface.h>

CLIENTEFFECT_REGISTER_BEGIN( PrecacheCSViewScene )
CLIENTEFFECT_MATERIAL( "effects/flashbang" )
CLIENTEFFECT_MATERIAL( "effects/flashbang_white" )
CLIENTEFFECT_MATERIAL( "effects/nightvision" )
CLIENTEFFECT_REGISTER_END()

static CCSViewRender g_ViewRender;

CCSViewRender::CCSViewRender()
{
	view = ( IViewRender * )&g_ViewRender;
	m_pFlashTexture = NULL;
}

struct ConVarFlags
{
	const char *name;
	int flags;
};

ConVarFlags s_flaggedConVars[] =
{
	{ "r_screenfademinsize", FCVAR_CHEAT },
	{ "r_screenfademaxsize", FCVAR_CHEAT },
	{ "developer", FCVAR_CHEAT },
};

void CCSViewRender::Init( void )
{
	for ( int i=0; i<ARRAYSIZE( s_flaggedConVars ); ++i )
	{
		ConVar *flaggedConVar = cvar->FindVar( s_flaggedConVars[i].name );
		if ( flaggedConVar )
		{
			flaggedConVar->AddFlags( s_flaggedConVars[i].flags );
		}
	}

	CViewRender::Init();
}

//-----------------------------------------------------------------------------
// Purpose: Returns the min/max fade distances
//-----------------------------------------------------------------------------
void CCSViewRender::GetScreenFadeDistances( float *min, float *max )
{
	if ( min )
	{
		*min = 0.0f;
	}

	if ( max )
	{
		*max = 0.0f;
	}
}


void CCSViewRender::PerformNightVisionEffect( const CViewSetup &view )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( !pPlayer )
		return;

	if (pPlayer->GetObserverMode() == OBS_MODE_IN_EYE)
	{
		CBaseEntity *target = pPlayer->GetObserverTarget();
		if (target && target->IsPlayer())
		{
			pPlayer = (C_CSPlayer *)target;
		}
	}

	if ( pPlayer && pPlayer->m_flNightVisionAlpha > 0 )
	{
		IMaterial *pMaterial = materials->FindMaterial( "effects/nightvision", TEXTURE_GROUP_CLIENT_EFFECTS, true );

		if ( pMaterial )
		{
			int iMaxValue = 255;
			byte overlaycolor[4] = { 0, 255, 0, 255 };
			
			if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 80 )
			{
				UpdateScreenEffectTexture( 0, view.x, view.y, view.width, view.height );
			}
			else
			{
				// In DX7, use the values CS:goldsrc uses.
				iMaxValue = 225;
				overlaycolor[0] = overlaycolor[2] = 50 / 2;
				overlaycolor[1] = 225 / 2;
			}

			if ( pPlayer->m_bNightVisionOn )
			{
				pPlayer->m_flNightVisionAlpha += 15;

				pPlayer->m_flNightVisionAlpha = MIN( pPlayer->m_flNightVisionAlpha, iMaxValue );
			}
			else 
			{
				pPlayer->m_flNightVisionAlpha -= 40;

				pPlayer->m_flNightVisionAlpha = MAX( pPlayer->m_flNightVisionAlpha, 0 );
				
			}

			overlaycolor[3] = pPlayer->m_flNightVisionAlpha;
	
			render->ViewDrawFade( overlaycolor, pMaterial );

			// Only one pass in DX7.
			if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 80 )
			{
				CMatRenderContextPtr pRenderContext( materials );
				pRenderContext->DrawScreenSpaceQuad( pMaterial );
				render->ViewDrawFade( overlaycolor, pMaterial );
				pRenderContext->DrawScreenSpaceQuad( pMaterial );
			}
		}
	}
}


//Adrian - Super Nifty Flashbang Effect(tm)
// this does the burn in for the flashbang effect.
void CCSViewRender::PerformFlashbangEffect( const CViewSetup &view )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( pPlayer == NULL )
		 return;

	if ( pPlayer->m_flFlashBangTime < gpGlobals->curtime )
		return;
	
	IMaterial *pMaterial = materials->FindMaterial( "effects/flashbang", TEXTURE_GROUP_CLIENT_EFFECTS, true );

	if ( !pMaterial )
		return;

	byte overlaycolor[4] = { 255, 255, 255, 255 };
	
	CMatRenderContextPtr pRenderContext( materials );
	
	if ( pPlayer->m_flFlashAlpha < pPlayer->m_flFlashMaxAlpha )
	{
		pPlayer->m_flFlashAlpha += 45;
		
		pPlayer->m_flFlashAlpha = MIN( pPlayer->m_flFlashAlpha, pPlayer->m_flFlashMaxAlpha );

		overlaycolor[0] = overlaycolor[1] = overlaycolor[2] = pPlayer->m_flFlashAlpha;

		m_pFlashTexture = GetFullFrameFrameBufferTexture( 1 );

		bool foundVar;

		IMaterialVar* m_BaseTextureVar = pMaterial->FindVar( "$basetexture", &foundVar, false );
	
		Rect_t srcRect;
		srcRect.x = view.x;
		srcRect.y = view.y;
		srcRect.width = view.width;
		srcRect.height = view.height;
		m_BaseTextureVar->SetTextureValue( m_pFlashTexture );
		pRenderContext->CopyRenderTargetToTextureEx( m_pFlashTexture, 0, &srcRect, NULL );
		pRenderContext->SetFrameBufferCopyTexture( m_pFlashTexture );

		render->ViewDrawFade( overlaycolor, pMaterial );

		// just do one pass for dxlevel < 80.
		if (g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 80)
		{
			pRenderContext->DrawScreenSpaceRectangle( pMaterial, view.x, view.y, view.width, view.height,
				0, 0, m_pFlashTexture->GetActualWidth()-1, m_pFlashTexture->GetActualHeight()-1, 
				m_pFlashTexture->GetActualWidth(), m_pFlashTexture->GetActualHeight() );
			render->ViewDrawFade( overlaycolor, pMaterial );
			pRenderContext->DrawScreenSpaceRectangle( pMaterial, view.x, view.y, view.width, view.height,
				0, 0, m_pFlashTexture->GetActualWidth()-1, m_pFlashTexture->GetActualHeight()-1, 
				m_pFlashTexture->GetActualWidth(), m_pFlashTexture->GetActualHeight() );
		}
	}
	else if ( m_pFlashTexture )
	{
		float flAlpha = pPlayer->m_flFlashMaxAlpha * (pPlayer->m_flFlashBangTime - gpGlobals->curtime) / pPlayer->m_flFlashDuration;

		flAlpha = clamp( flAlpha, 0, pPlayer->m_flFlashMaxAlpha );
		
		overlaycolor[0] = overlaycolor[1] = overlaycolor[2] = flAlpha;

		render->ViewDrawFade( overlaycolor, pMaterial );

		// just do one pass for dxlevel < 80.
		if (g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 80)
		{
			pRenderContext->DrawScreenSpaceRectangle( pMaterial, view.x, view.y, view.width, view.height,
				0, 0, m_pFlashTexture->GetActualWidth()-1, m_pFlashTexture->GetActualHeight()-1, 
				m_pFlashTexture->GetActualWidth(), m_pFlashTexture->GetActualHeight() );
			render->ViewDrawFade( overlaycolor, pMaterial );
			pRenderContext->DrawScreenSpaceRectangle( pMaterial, view.x, view.y, view.width, view.height,
				0, 0, m_pFlashTexture->GetActualWidth()-1, m_pFlashTexture->GetActualHeight()-1, 
				m_pFlashTexture->GetActualWidth(), m_pFlashTexture->GetActualHeight() );
		}
	}

	// this does the pure white overlay part of the flashbang effect.
	pMaterial = materials->FindMaterial( "effects/flashbang_white", TEXTURE_GROUP_CLIENT_EFFECTS, true );

	if ( !pMaterial )
		return;

	float flAlpha = 255;

	if ( pPlayer->m_flFlashAlpha < pPlayer->m_flFlashMaxAlpha )
	{
		 flAlpha = pPlayer->m_flFlashAlpha;
	}
	else
	{
		float flFlashTimeLeft = pPlayer->m_flFlashBangTime - gpGlobals->curtime;
		float flAlphaPercentage = 1.0;
		const float certainBlindnessTimeThresh = 3.0; // yes this is a magic number, necessary to match CS/CZ flashbang effectiveness cause the rendering system is completely different.

		if (flFlashTimeLeft > certainBlindnessTimeThresh)
		{
			// if we still have enough time of blindness left, make sure the player can't see anything yet.
			flAlphaPercentage = 1.0;
		}
		else
		{
			// blindness effects shorter than 'certainBlindnessTimeThresh' will start off at less than 255 alpha.
			flAlphaPercentage = flFlashTimeLeft / certainBlindnessTimeThresh;

			if (g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 80)
			{
				// reduce alpha level quicker with dx 8 support and higher to compensate
				// for having the burn-in effect.
				flAlphaPercentage *= flAlphaPercentage;
			}
		}

		flAlpha = flAlphaPercentage *= pPlayer->m_flFlashMaxAlpha; // scale a [0..1) value to a [0..MaxAlpha] value for the alpha.

		// make sure the alpha is in the range of [0..MaxAlpha]
		flAlpha = MAX ( flAlpha, 0 );
		flAlpha = MIN ( flAlpha, pPlayer->m_flFlashMaxAlpha);
	}

	overlaycolor[0] = overlaycolor[1] = overlaycolor[2] = flAlpha;
	render->ViewDrawFade( overlaycolor, pMaterial );
}


//-----------------------------------------------------------------------------
// Purpose: Renders extra 2D effects in derived classes while the 2D view is on the stack
//-----------------------------------------------------------------------------
void CCSViewRender::Render2DEffectsPreHUD( const CViewSetup &view )
{
	PerformNightVisionEffect( view );	// this needs to come before the HUD is drawn, or it will wash the HUD out
}


//-----------------------------------------------------------------------------
// Purpose: Renders extra 2D effects in derived classes while the 2D view is on the stack
//-----------------------------------------------------------------------------
void CCSViewRender::Render2DEffectsPostHUD( const CViewSetup &view )
{
	PerformFlashbangEffect( view );
}


//-----------------------------------------------------------------------------
// Purpose: Renders voice feedback and other sprites attached to players
// Input  : none
//-----------------------------------------------------------------------------
void CCSViewRender::RenderPlayerSprites()
{
	GetClientVoiceMgr()->SetHeadLabelOffset( 40 );

	CViewRender::RenderPlayerSprites();
	RadioManager()->DrawHeadLabels();
}

