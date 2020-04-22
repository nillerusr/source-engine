//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal mod render targets are specified by and accessable through this singleton
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "portal_render_targets.h"
#include "materialsystem/imaterialsystem.h"
#include "rendertexture.h"

//-----------------------------------------------------------------------------
// Purpose: Called in CClientRenderTargets::InitClientRenderTargets, used to set
//			the CTextureReference member
// Input  : pMaterialSystem - material system passed in from the engine
// Output : ITexture* - the created texture
//-----------------------------------------------------------------------------
ITexture* CPortalRenderTargets::InitPortal1Texture( IMaterialSystem* pMaterialSystem )
{
	if ( IsX360() )
	{
		// shouldn't be using
		Assert( 0 );
		return NULL;
	}

	return pMaterialSystem->CreateNamedRenderTargetTextureEx2(
		"_rt_Portal1",
		1, 1, RT_SIZE_FULL_FRAME_BUFFER,
		pMaterialSystem->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_SHARED, 
		0,
		CREATERENDERTARGETFLAGS_HDR );
}

ITexture* CPortalRenderTargets::GetPortal1Texture()
{
	return m_Portal1Texture;
}


//-----------------------------------------------------------------------------
// Purpose: Called in CClientRenderTargets::InitClientRenderTargets, used to set
//			the CTextureReference member
// Input  : pMaterialSystem - material system passed in from the engine
// Output : ITexture* - the created texture
//-----------------------------------------------------------------------------
ITexture* CPortalRenderTargets::InitPortal2Texture( IMaterialSystem* pMaterialSystem )
{
	if ( IsX360() )
	{
		// shouldn't be using
		Assert( 0 );
		return NULL;
	}

	return pMaterialSystem->CreateNamedRenderTargetTextureEx2(
		"_rt_Portal2",
		1, 1, RT_SIZE_FULL_FRAME_BUFFER,
		pMaterialSystem->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_SHARED, 
		0,
		CREATERENDERTARGETFLAGS_HDR );
}

ITexture* CPortalRenderTargets::GetPortal2Texture()
{
	return m_Portal2Texture;
} 



//-----------------------------------------------------------------------------
// Purpose: Called in CClientRenderTargets::InitClientRenderTargets, used to set
//			the CTextureReference member
// Input  : pMaterialSystem - material system passed in from the engine
// Output : ITexture* - the created texture
//-----------------------------------------------------------------------------
ITexture* CPortalRenderTargets::InitDepthDoublerTexture( IMaterialSystem* pMaterialSystem )
{
	return pMaterialSystem->CreateNamedRenderTargetTextureEx2(
		"_rt_DepthDoubler",
		512, 512, RT_SIZE_DEFAULT,
		pMaterialSystem->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_SHARED, 
		0,
		CREATERENDERTARGETFLAGS_HDR );
}

ITexture* CPortalRenderTargets::GetDepthDoublerTexture()
{
	return m_DepthDoublerTexture;
}


void CPortalRenderTargets::InitPortalWaterTextures( IMaterialSystem* pMaterialSystem )
{
	if ( IsX360() )
	{
		return;
	}

	//Reflections
	m_WaterReflectionTextures[0].Init( 
		pMaterialSystem->CreateNamedRenderTargetTextureEx2(
			"_rt_PortalWaterReflection_Depth1",
			512, 512, RT_SIZE_PICMIP,
			pMaterialSystem->GetBackBufferFormat(), 
			MATERIAL_RT_DEPTH_SEPARATE, 
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
			CREATERENDERTARGETFLAGS_HDR ) );

	m_WaterReflectionTextures[1].Init( 
		pMaterialSystem->CreateNamedRenderTargetTextureEx2(
			"_rt_PortalWaterReflection_Depth2",
			256, 256, RT_SIZE_PICMIP,
			pMaterialSystem->GetBackBufferFormat(), 
			MATERIAL_RT_DEPTH_SEPARATE,
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
			CREATERENDERTARGETFLAGS_HDR ) );


	//Refractions
	m_WaterRefractionTextures[0].Init( 
		pMaterialSystem->CreateNamedRenderTargetTextureEx2(
			"_rt_PortalWaterRefraction_Depth1",
			512, 512, RT_SIZE_PICMIP,
			// This is different than reflection because it has to have alpha for fog factor.
			IMAGE_FORMAT_RGBA8888, 
			MATERIAL_RT_DEPTH_SEPARATE,
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
			CREATERENDERTARGETFLAGS_HDR ) );

	m_WaterRefractionTextures[1].Init( 
		pMaterialSystem->CreateNamedRenderTargetTextureEx2(
			"_rt_PortalWaterRefraction_Depth2",
			256, 256, RT_SIZE_PICMIP,
			// This is different than reflection because it has to have alpha for fog factor.
			IMAGE_FORMAT_RGBA8888, 
			MATERIAL_RT_DEPTH_SEPARATE,
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
			CREATERENDERTARGETFLAGS_HDR ) );
}

ITexture* CPortalRenderTargets::GetWaterReflectionTextureForStencilDepth( int iStencilDepth )
{
	if ( IsX360() )
	{
		return NULL;
	}

	if ( iStencilDepth > 2 )
		return NULL;

	if ( iStencilDepth == 0 )
		return m_WaterReflectionTexture; //from CBaseClientRenderTargets

	return m_WaterReflectionTextures[ iStencilDepth - 1 ];
}

ITexture* CPortalRenderTargets::GetWaterRefractionTextureForStencilDepth( int iStencilDepth )
{
	if ( IsX360() )
	{
		return NULL;
	}

	if ( iStencilDepth > 2 )
		return NULL;

	if ( iStencilDepth == 0 )
		return m_WaterRefractionTexture; //from CBaseClientRenderTargets

	return m_WaterRefractionTextures[ iStencilDepth - 1 ];
}


//-----------------------------------------------------------------------------
// Purpose: InitClientRenderTargets, interface called by the engine at material system init in the engine
// Input  : pMaterialSystem - the interface to the material system from the engine (our singleton hasn't been set up yet)
//			pHardwareConfig - the user's hardware config, useful for conditional render targets setup
//-----------------------------------------------------------------------------
void CPortalRenderTargets::InitClientRenderTargets( IMaterialSystem* pMaterialSystem, IMaterialSystemHardwareConfig* pHardwareConfig )
{
	// If they don't support stencils, allocate render targets for drawing portals.
	// TODO: When stencils are default, do the below check before bothering to allocate the RTs
	//		and make sure that switching from Stencil<->RT mode reinits the material system.
//	if ( materials->StencilBufferBits() == 0 )
	if ( IsPC() || !IsX360() )
	{
		m_Portal1Texture.Init( InitPortal1Texture( pMaterialSystem ) );
		m_Portal2Texture.Init( InitPortal2Texture( pMaterialSystem ) );
	}

	m_DepthDoublerTexture.Init( InitDepthDoublerTexture( pMaterialSystem ) );

	if ( IsPC() || !IsX360() )
	{
		InitPortalWaterTextures( pMaterialSystem );
	}

	// Water effects & camera from the base class (standard HL2 targets)
	BaseClass::InitClientRenderTargets( pMaterialSystem, pHardwareConfig, 512, 256 );
}

//-----------------------------------------------------------------------------
// Purpose: Shutdown client render targets. This gets called during shutdown in the engine
// Input  :  - 
//-----------------------------------------------------------------------------
void CPortalRenderTargets::ShutdownClientRenderTargets()
{
	m_Portal1Texture.Shutdown();
	m_Portal2Texture.Shutdown();
	m_DepthDoublerTexture.Shutdown();

	for ( int i = 0; i < 2; ++i )
	{
		m_WaterReflectionTextures[i].Shutdown();
		m_WaterRefractionTextures[i].Shutdown();
	}

	// Clean up standard HL2 RTs (camera and water)
	BaseClass::ShutdownClientRenderTargets();
}


static CPortalRenderTargets g_PortalRenderTargets;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CPortalRenderTargets, IClientRenderTargets, CLIENTRENDERTARGETS_INTERFACE_VERSION, g_PortalRenderTargets );
CPortalRenderTargets* portalrendertargets = &g_PortalRenderTargets;