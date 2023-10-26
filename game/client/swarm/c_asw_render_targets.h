#ifndef _INCLUDED_ASW_RENDER_TARGETS_H
#define _INCLUDED_ASW_RENDER_TARGETS_H
#ifdef _WIN32
#pragma once
#endif

#include "baseclientrendertargets.h" // Base class, with interfaces called by engine and inherited members to init common render targets

#ifndef INFESTED_DLL 
#pragma message ( "This file should only be built with AS:Infested builds" )
#endif

// externs
class IMaterialSystem;
class IMaterialSystemHardwareConfig;

class CASWRenderTargets : public CBaseClientRenderTargets
{
	// no networked vars
	DECLARE_CLASS_GAMEROOT( CASWRenderTargets, CBaseClientRenderTargets );
public:
	virtual void InitClientRenderTargets( IMaterialSystem* pMaterialSystem, IMaterialSystemHardwareConfig* pHardwareConfig );
	virtual void ShutdownClientRenderTargets();

	ITexture* InitASWMotionBlurTexture( IMaterialSystem* pMaterialSystem );
	ITexture* GetASWMotionBlurTexture( void );

private:
	CTextureReference m_ASWMotionBlurTexture;	
};

extern CASWRenderTargets* g_pASWRenderTargets;


#endif //_INCLUDED_ASW_RENDER_TARGETS_H