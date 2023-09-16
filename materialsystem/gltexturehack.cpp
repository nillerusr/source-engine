#include "pch_materialsystem.h"
#include "togl/rendermechanism.h"

#define MATSYS_INTERNAL

#include "cmaterialsystem.h"
//-----------------------------------------------------------------------------
// The Base implementation of the shader rendering interface
//-----------------------------------------------------------------------------
class CShaderAPIBase : public IShaderAPI
{
public:
	// constructor, destructor
	CShaderAPIBase();
	virtual ~CShaderAPIBase();

	// Called when the device is initializing or shutting down
	virtual bool OnDeviceInit() = 0;
	virtual void OnDeviceShutdown() = 0;

	// Pix events
	virtual void BeginPIXEvent( unsigned long color, const char *szName ) = 0;
	virtual void EndPIXEvent() = 0;
	virtual void AdvancePIXFrame() = 0;

	// Release, reacquire objects
	virtual void ReleaseShaderObjects() = 0;
	virtual void RestoreShaderObjects() = 0;

	// Resets the render state to its well defined initial value
	virtual void ResetRenderState( bool bFullReset = true ) = 0;

	// Returns a d3d texture associated with a texture handle
	virtual IDirect3DBaseTexture9* GetD3DTexture( ShaderAPITextureHandle_t hTexture ) = 0;

	// Queues a non-full reset of render state next BeginFrame.
	virtual void QueueResetRenderState() = 0;

	// Methods of IShaderDynamicAPI
public:
	virtual void GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo );

protected:
};

uint32_t CMaterialSystem::GetShaderAPIGLTexture( ITexture *pTexture, int nFrame, int nTextureChannel )
{
	ShaderAPITextureHandle_t handle = ShaderSystem()->GetShaderAPITextureBindHandle( pTexture, nFrame, nTextureChannel );
	IDirect3DTexture9* pTex = ((CShaderAPIBase*)g_pShaderAPI)->GetD3DTexture(handle);
	IDirect3DSurface9* surf = pTex->m_surfZero;
	CGLMTex *tex = surf->m_tex;
	return tex->GetTexName();
}
