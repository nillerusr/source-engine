#include "cbase.h"
#include "c_asw_render_targets.h"
#include "materialsystem\imaterialsystem.h"
#include "materialsystem/ITexture.h"

ITexture* CASWRenderTargets::InitASWMotionBlurTexture( IMaterialSystem* pMaterialSystem )
{
#ifdef _X360
	// @TODO: need to figure out where in EDRAM to put motion blur texture
	return NULL;
#else // !_X360
	return pMaterialSystem->CreateNamedRenderTargetTextureEx(
		"_rt_ASWMotionBlur",
		256, 256, RT_SIZE_FULL_FRAME_BUFFER,
		pMaterialSystem->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_NONE, 
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
		0 );
#endif // _X360
}

ITexture* CASWRenderTargets::GetASWMotionBlurTexture()
{
	// @TODO: need to figure out where in EDRAM to put motion blur texture 
	Assert( !IsX360() );
	if(!m_ASWMotionBlurTexture)
	{
		m_ASWMotionBlurTexture.InitRenderTarget(256, 256, RT_SIZE_FULL_FRAME_BUFFER, IMAGE_FORMAT_ARGB8888, MATERIAL_RT_DEPTH_NONE, false);
		Assert(!IsErrorTexture(m_ASWMotionBlurTexture));
	}
 
	return m_ASWMotionBlurTexture;
} 

//-----------------------------------------------------------------------------
// Purpose: InitClientRenderTargets, interface called by the engine at material system init in the engine
// Input  : pMaterialSystem - the interface to the material system from the engine (our singleton hasn't been set up yet)
//			pHardwareConfig - the user's hardware config, useful for conditional render targets setup
//-----------------------------------------------------------------------------
void CASWRenderTargets::InitClientRenderTargets( IMaterialSystem* pMaterialSystem, IMaterialSystemHardwareConfig* pHardwareConfig )
{	
	m_ASWMotionBlurTexture.Init( InitASWMotionBlurTexture( pMaterialSystem ) );

	// Water effects & camera from the base class (standard HL2 targets)
	BaseClass::InitClientRenderTargets( pMaterialSystem, pHardwareConfig );
}

//-----------------------------------------------------------------------------
// Purpose: Shutdown client render targets. This gets called during shutdown in the engine
// Input  :  - 
//-----------------------------------------------------------------------------
void CASWRenderTargets::ShutdownClientRenderTargets()
{
	m_ASWMotionBlurTexture.Shutdown();	

	// Clean up standard HL2 RTs (camera and water)
	BaseClass::ShutdownClientRenderTargets();
}


static CASWRenderTargets g_ASWRenderTargets;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CASWRenderTargets, IClientRenderTargets, CLIENTRENDERTARGETS_INTERFACE_VERSION, g_ASWRenderTargets );
CASWRenderTargets* g_pASWRenderTargets = &g_ASWRenderTargets;