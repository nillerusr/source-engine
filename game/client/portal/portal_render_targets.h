//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal mod render targets are specified by and accessable through this singleton
//
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#ifndef PORTALRENDERTARGETS_H_
#define PORTALRENDERTARGETS_H_
#ifdef _WIN32
#pragma once
#endif

#include "baseclientrendertargets.h" // Base class, with interfaces called by engine and inherited members to init common render targets

#ifndef PORTAL 
#pragma message ( "This file should only be built with portal builds" )
#endif

// externs
class IMaterialSystem;
class IMaterialSystemHardwareConfig;

class CPortalRenderTargets : public CBaseClientRenderTargets
{
	// no networked vars
	DECLARE_CLASS_GAMEROOT( CPortalRenderTargets, CBaseClientRenderTargets );
public:
	virtual void InitClientRenderTargets( IMaterialSystem* pMaterialSystem, IMaterialSystemHardwareConfig* pHardwareConfig );
	virtual void ShutdownClientRenderTargets();

	ITexture* GetPortal1Texture( void );
	ITexture* GetPortal2Texture( void );
	ITexture* GetDepthDoublerTexture( void );

	//recursive views require different water textures
	ITexture* GetWaterReflectionTextureForStencilDepth( int iStencilDepth );
	ITexture* GetWaterRefractionTextureForStencilDepth( int iStencilDepth );

private:
	CTextureReference m_Portal1Texture;
	CTextureReference m_Portal2Texture;
	CTextureReference m_DepthDoublerTexture;

	CTextureReference m_WaterRefractionTextures[2];
	CTextureReference m_WaterReflectionTextures[2];

	ITexture* InitPortal1Texture ( IMaterialSystem* pMaterialSystem );
	ITexture* InitPortal2Texture ( IMaterialSystem* pMaterialSystem );
	ITexture* InitDepthDoublerTexture ( IMaterialSystem* pMaterialSystem );

	void InitPortalWaterTextures ( IMaterialSystem* pMaterialSystem );

};

extern CPortalRenderTargets* portalrendertargets;


#endif //PORTALRENDERTARGETS_H_