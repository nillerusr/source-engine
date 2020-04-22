//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Responsible for drawing the scene
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "iviewrender.h"
#include "c_dod_player.h"
#include "view_shared.h"
#include "dod_headiconmanager.h"

#include "clienteffectprecachesystem.h"
#include "rendertexture.h"
#include "view_scene.h"

#include "materialsystem/imesh.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "colorcorrectionmgr.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"

#include "ScreenSpaceEffects.h"
#include "dod_view_scene.h"
#include "KeyValues.h"
#include "dod_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CDODViewRender g_ViewRender;

// Console variable for enabling camera effects
ConVar cl_enablespectatoreffects( "cl_enablespectatoreffects", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Enable/disable spectator camera effects" );
ConVar cl_enabledeatheffects( "cl_enabledeatheffects", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Enable/disable death camera effects" );
ConVar cl_enabledeathfilmgrain( "cl_enabledeathfilmgrain", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Enable/disable the death camera film grain" );

ConVar cl_deatheffect_force_on( "cl_deatheffect_always_on", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Always show the death effect" );

// Console variables to define lookup maps
static void UpdateCameraLookups( IConVar *var, char const *pOldString, float flOldValue )
{
	CDODViewRender *pView = static_cast<CDODViewRender*>(view);
	pView->InitColorCorrection();
}

static void SetSpectatorLookup( ConVar *var, char const *pOldString );
ConVar cl_spectatorlookup( "cl_spectatorlookup", "materials\\colorcorrection\\bnw_c.raw", FCVAR_CLIENTDLL | FCVAR_CHEAT, "Sets the lookup map to use for spectator cameras", UpdateCameraLookups );
ConVar cl_deathlookup( "cl_deathlookup", "materials\\colorcorrection\\bnw_c.raw", FCVAR_CLIENTDLL | FCVAR_CHEAT, "Sets the lookup map to use for death cameras",     UpdateCameraLookups );

CLIENTEFFECT_REGISTER_BEGIN( PrecacheDODViewScene )
CLIENTEFFECT_MATERIAL( "effects/stun" )
CLIENTEFFECT_REGISTER_END()

CDODViewRender::CDODViewRender()
{
	view = ( IViewRender * )this;

	m_SpectatorLookupHandle = (ClientCCHandle_t)0;
	m_DeathLookupHandle = (ClientCCHandle_t)0;
	m_bLookupActive = false;
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
};

void CDODViewRender::Init()
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

	InitColorCorrection();
}

void CDODViewRender::Shutdown()
{
	CViewRender::Shutdown();

	ShutdownColorCorrection();
}

void CDODViewRender::PerformStunEffect( const CViewSetup &view )
{
	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

	if ( pPlayer == NULL )
		return;

	if ( pPlayer->m_flStunEffectTime < gpGlobals->curtime )
		return;

	IMaterial *pMaterial = materials->FindMaterial( "effects/stun", TEXTURE_GROUP_CLIENT_EFFECTS, true );

	if ( !pMaterial )
		return;

	byte overlaycolor[4] = { 255, 255, 255, 255 };

	CMatRenderContextPtr pRenderContext( materials );
	if ( pPlayer->m_flStunAlpha < pPlayer->m_flStunMaxAlpha )
	{
		// copy current screen content into texture buffer
		UpdateScreenEffectTexture( 0, view.x, view.y, view.width, view.height );

		pPlayer->m_flStunAlpha += 45;

		pPlayer->m_flStunAlpha = MIN( pPlayer->m_flStunAlpha, pPlayer->m_flStunMaxAlpha );

		overlaycolor[3] = pPlayer->m_flStunAlpha;

		m_pStunTexture = GetFullFrameFrameBufferTexture( 1 );

		bool foundVar;

		IMaterialVar* m_BaseTextureVar = pMaterial->FindVar( "$basetexture", &foundVar, false );

		Rect_t srcRect;
		srcRect.x = view.x;
		srcRect.y = view.y;
		srcRect.width = view.width;
		srcRect.height = view.height;
		m_BaseTextureVar->SetTextureValue( m_pStunTexture );
		pRenderContext->CopyRenderTargetToTextureEx( m_pStunTexture, 0, &srcRect, NULL );
		pRenderContext->SetFrameBufferCopyTexture( m_pStunTexture );

		render->ViewDrawFade( overlaycolor, pMaterial );

		// just do one pass for dxlevel < 80.
		if (g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 80)
		{
			pRenderContext->DrawScreenSpaceQuad( pMaterial );
			render->ViewDrawFade( overlaycolor, pMaterial );
			pRenderContext->DrawScreenSpaceQuad( pMaterial );
		}
	}
	else if ( m_pStunTexture )
	{
		float flAlpha = pPlayer->m_flStunMaxAlpha * (pPlayer->m_flStunEffectTime - gpGlobals->curtime) / pPlayer->m_flStunDuration;

		flAlpha = clamp( flAlpha, 0, pPlayer->m_flStunMaxAlpha );

		overlaycolor[3] = flAlpha;

		render->ViewDrawFade( overlaycolor, pMaterial );

		// just do one pass for dxlevel < 80.
		if (g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 80)
		{
			pRenderContext->DrawScreenSpaceQuad( pMaterial );
			render->ViewDrawFade( overlaycolor, pMaterial );
			pRenderContext->DrawScreenSpaceQuad( pMaterial );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Initialise the color correction maps for spectator and death cameras
// Input  : none
//-----------------------------------------------------------------------------
void CDODViewRender::InitColorCorrection( )
{
	m_SpectatorLookupHandle = g_pColorCorrectionMgr->AddColorCorrection( "spectator", cl_spectatorlookup.GetString() );
	m_DeathLookupHandle = g_pColorCorrectionMgr->AddColorCorrection( "death", cl_deathlookup.GetString() );
}

//-----------------------------------------------------------------------------
// Purpose: Cleanup the color correction maps
// Input  : none
//-----------------------------------------------------------------------------
void CDODViewRender::ShutdownColorCorrection( )
{
	g_pColorCorrectionMgr->RemoveColorCorrection( m_SpectatorLookupHandle );
	g_pColorCorrectionMgr->RemoveColorCorrection( m_DeathLookupHandle );
}

//-----------------------------------------------------------------------------
// Purpose: Do the per-frame setup required for the spectator cam color correction
// Input  : none
//-----------------------------------------------------------------------------
void CDODViewRender::SetupColorCorrection( )
{
	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();
	if( !pPlayer )
		return;

	bool bResetColorCorrection = true;

	int nObsMode = pPlayer->GetObserverMode();

	if ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
	{
		if ( cl_enablespectatoreffects.GetBool() )
		{
			bResetColorCorrection = false;

			// Enable spectator color lookup for all other modes except OBS_MODE_NONE
			g_pColorCorrectionMgr->SetColorCorrectionWeight( m_SpectatorLookupHandle, 1.0f );
			g_pColorCorrectionMgr->SetColorCorrectionWeight( m_DeathLookupHandle,     0.0f );
			g_pColorCorrectionMgr->SetResetable( m_SpectatorLookupHandle, false );
			g_pColorCorrectionMgr->SetResetable( m_DeathLookupHandle,     true );

			if ( cl_enabledeathfilmgrain.GetBool() )
			{
				g_pScreenSpaceEffects->EnableScreenSpaceEffect( "filmgrain" );

				KeyValues *kv = new KeyValues( "params" );
				kv->SetFloat( "effect_alpha", 1.0f );
				g_pScreenSpaceEffects->SetScreenSpaceEffectParams( "filmgrain", kv );

				m_bLookupActive = true;
			}
		}
	}
	else if ( cl_enabledeatheffects.GetBool() )
	{
		float flEffectAlpha = 0.0f;

		if ( cl_deatheffect_force_on.GetBool() )
		{
			flEffectAlpha = 1.0f;
		}
		else if ( nObsMode != OBS_MODE_NONE )
		{
			flEffectAlpha = MIN( 1.0f, ( gpGlobals->curtime - pPlayer->GetDeathTime() ) / ( 2.0f ) );
		}
		else
		{
			DODRoundState roundstate = DODGameRules()->State_Get();

			if ( roundstate < STATE_RND_RUNNING )
			{
				flEffectAlpha = 1.0f;
			}
			else if ( roundstate == STATE_RND_RUNNING )
			{
				// fade out of effect at last event: spawn or unfreeze at round start
				float flFadeOutStartTime = MAX( DODGameRules()->m_flLastRoundStateChangeTime, pPlayer->m_flLastRespawnTime );

				// fade in from round start time
				flEffectAlpha = 1.0 - ( gpGlobals->curtime - flFadeOutStartTime ) / ( 2.0f );
				flEffectAlpha = MAX( 0.0f, flEffectAlpha );				
			}
		}

		if ( flEffectAlpha > 0.0f )
		{
			bResetColorCorrection = false;

			// Enable death camera color lookup

			// allow us to reset the weight
			g_pColorCorrectionMgr->SetResetable( m_DeathLookupHandle, true );
			g_pColorCorrectionMgr->SetResetable( m_SpectatorLookupHandle, true );

			// reset weight to 0
			g_pColorCorrectionMgr->ResetColorCorrectionWeights();

			g_pColorCorrectionMgr->SetColorCorrectionWeight( m_SpectatorLookupHandle, 0.0f );
			g_pColorCorrectionMgr->SetColorCorrectionWeight( m_DeathLookupHandle,    flEffectAlpha );

			if ( cl_enabledeathfilmgrain.GetBool() )
			{
				g_pScreenSpaceEffects->EnableScreenSpaceEffect( "filmgrain" );

				KeyValues *kv = new KeyValues( "params" );
				//kv->SetInt( "split_screen", 1 );

				kv->SetFloat( "effect_alpha", flEffectAlpha );

				g_pScreenSpaceEffects->SetScreenSpaceEffectParams( "filmgrain", kv );
			}

			m_bLookupActive = true;
		}
	}

	if ( bResetColorCorrection )
	{
		// Disable color lookups
		g_pColorCorrectionMgr->SetColorCorrectionWeight( m_SpectatorLookupHandle, 0.0f );
		g_pColorCorrectionMgr->SetColorCorrectionWeight( m_DeathLookupHandle,     0.0f );
		g_pColorCorrectionMgr->SetResetable( m_SpectatorLookupHandle, true );
		g_pColorCorrectionMgr->SetResetable( m_DeathLookupHandle,     true );

		if( m_bLookupActive )
			g_pScreenSpaceEffects->DisableScreenSpaceEffect( "filmgrain" );

		m_bLookupActive = false;
	}
}

void CDODViewRender::RenderView( const CViewSetup &view, int nClearFlags, int whatToDraw )
{
	// Setup the necessary parameters for color correction
	SetupColorCorrection( );

	CViewRender::RenderView( view, nClearFlags, whatToDraw );

	// Draw screen effects here
	PerformStunEffect( view );
}

//-----------------------------------------------------------------------------
// Purpose: Renders voice feedback and other sprites attached to players
// Input  : none
//-----------------------------------------------------------------------------
void CDODViewRender::RenderPlayerSprites()
{
	CViewRender::RenderPlayerSprites();

	// Draw head icons here
	HeadIconManager()->DrawHeadIcons();
}