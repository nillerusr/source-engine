//========= Copyright Valve Corporation, All rights reserved. ============//
/* 
TODO: add option to null out drawprim calls.
 
Also maybe hook the PIX_ENABLE stuff to Telemetry
 
		In imaterialsystem.h:
		#define PIX_ENABLE 1                   // set this to 1 and build engine/studiorender to enable pix events in the engine

		And in shaderapidx8.h:
    	#define PIX_ENABLE 1                   // set this to 1 and build engine/studiorender to enable pix events in the engine
 
Might be interesting to make it so dx9hook.h paid attention to the PIX_ENABLE names
	and allowed you to filter drawprim calls based on those?
*/

#ifndef _DX9HOOK_H_
#define _DX9HOOK_H_

#if D3D_BATCH_PERF_ANALYSIS
	#include "../../thirdparty/miniz/miniz.c"
	#include "../../thirdparty/miniz/simple_bitmap.h"

	extern ConVar d3d_batch_vis, d3d_batch_vis_abs_scale, d3d_batch_vis_y_scale, d3d_present_vis_abs_scale;
	extern uint64 g_nTotalD3DCalls, g_nTotalD3DCycles;

	class CD3DCallTimer
	{
	public:
		inline CD3DCallTimer() { g_nTotalD3DCalls++; g_nTotalD3DCycles -= tmFastTime(); }
		inline ~CD3DCallTimer() {  g_nTotalD3DCycles += tmFastTime(); }
	};
	
	#define D3D_BATCH_PERF(...) __VA_ARGS__
#else
	#define D3D_BATCH_PERF(...)
#endif

#if D3D_BATCH_PERF_ANALYSIS
	#define XXX \
		tmZone( TELEMETRY_LEVEL3, TMZF_NONE, "D3D9: %s", __FUNCTION__ ); \
		CD3DCallTimer scopedCallTimer;
#else
	#define XXX																													\
		if( ThreadInMainThread() )  																							\
		{   																													\
			tmMessage( TELEMETRY_LEVEL0, TMMF_ICON_NOTE | TMMF_SEVERITY_WARNING, "(source/d3d)%s", __FUNCTION__ );				\
			tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "%s", __FUNCTION__ );												\
		}
#endif

// Hooks for routines which return values.
#define _DOCALL0( _member )				( m_Data.pHWObj->_member )()
#define _DOCALL( _member, ... )			( m_Data.pHWObj->_member )( __VA_ARGS__ )
// And hooks for routines which return squatola.
#define _DOCALL0_NORET( _member )		( m_Data.pHWObj->_member )()
#define _DOCALL_NORET( _member, ... )	( m_Data.pHWObj->_member )( __VA_ARGS__)

#define DEF_HOOKCLASSES( X )												\
    template<> class CDx9HookBase< struct I ## X >							\
    {                                                   					\
    public:                                             					\
        typedef I ## X 					_D3DINTERFACE;                		\
        typedef class C ## X ## Hook 	_HOOKCLASS;                			\
																			\
	public:  																\
		CDx9HookBase() { memset( &m_Data, 0, sizeof( m_Data ) ); }  		\
		virtual ~CDx9HookBase() {}  										\
																			\
	public: 																\
		struct DATA 														\
		{   																\
			class CDirect3DDevice9Hook *pDevice;   							\
			_D3DINTERFACE	*pHWObj;    									\
		};  																\
																			\
		DATA m_Data;														\
    }

template < class T > class CDx9HookBase;
DEF_HOOKCLASSES( Direct3DVertexDeclaration9 );
DEF_HOOKCLASSES( Direct3DPixelShader9 );
DEF_HOOKCLASSES( Direct3DVertexShader9 );
DEF_HOOKCLASSES( Direct3DVertexBuffer9 );
DEF_HOOKCLASSES( Direct3DIndexBuffer9 );
DEF_HOOKCLASSES( Direct3DQuery9 );
DEF_HOOKCLASSES( Direct3DStateBlock9 );
DEF_HOOKCLASSES( Direct3DSurface9 );
DEF_HOOKCLASSES( Direct3DBaseTexture9 );
DEF_HOOKCLASSES( Direct3DTexture9 );
DEF_HOOKCLASSES( Direct3DCubeTexture9 );
DEF_HOOKCLASSES( Direct3DVolume9 );
DEF_HOOKCLASSES( Direct3DVolumeTexture9 );
DEF_HOOKCLASSES( Direct3DSwapChain9 );
DEF_HOOKCLASSES( Direct3D9 );
DEF_HOOKCLASSES( Direct3DDevice9 );

template < class _D3DINTERFACE > HRESULT AllocOverride( HRESULT *hr, class CDirect3DDevice9Hook *pDevice, _D3DINTERFACE **ppHWObj )
{
    if( SUCCEEDED(*hr) && ppHWObj && *ppHWObj )
    {
        CDx9HookBase< _D3DINTERFACE >::_HOOKCLASS *pClass = new CDx9HookBase< _D3DINTERFACE >::_HOOKCLASS;

        if(!pClass)
        {
            ( *ppHWObj )->Release();
            *hr = E_OUTOFMEMORY;
            return NULL;
        }
        pClass->m_Data.pDevice = pDevice;
		pClass->m_Data.pHWObj = *ppHWObj;
		*ppHWObj = pClass;
        return *hr;
    }

    return *hr;
}

template <class _D3DINTERFACE> _D3DINTERFACE *GetHWPtr( _D3DINTERFACE *pD3DInterface )
{
	if( pD3DInterface )
	{
        CDx9HookBase< _D3DINTERFACE >::_HOOKCLASS *pClass = ( CDx9HookBase< _D3DINTERFACE >::_HOOKCLASS * )pD3DInterface;
		return pClass->m_Data.pHWObj;
	}
	return NULL;
}

template <class _D3DINTERFACE> CDirect3DDevice9Hook *GetHookDevice( _D3DINTERFACE *pD3DInterface )
{
	if( pD3DInterface )
	{
        CDx9HookBase< _D3DINTERFACE >::_HOOKCLASS *pClass = ( CDx9HookBase< _D3DINTERFACE >::_HOOKCLASS * )pD3DInterface;
		return pClass->m_Data.pDevice;
	}
	return NULL;
}

//$ TODO: if(riid == IID_IDirect3DDevice9Ex, IID_IDirect3DDevice9, etc.
#define IMPL_QUERYINTERFACE()                                                   \
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj)                 \
	{																			\
		__debugbreak();															\
		XXX; return _DOCALL(QueryInterface, riid, ppvObj);						\
	}

#define IMPL_IUNKOWN()                                                          \
    IMPL_QUERYINTERFACE();                                                      \
    STDMETHOD_(ULONG,AddRef)(THIS)                                              \
        { XXX; return _DOCALL0(AddRef); }										\
    STDMETHOD_(ULONG,Release)(THIS)                                             \
	{																			\
		XXX;																	\
		ULONG retval = _DOCALL0(Release);   									\
		if(retval == 0) 														\
			delete this;														\
		return retval;  														\
	}

#define IMPL_GETDEVICE()                                                        \
    STDMETHOD(GetDevice)(THIS_ IDirect3DDevice9** ppDevice)                     \
	{																			\
		XXX;																	\
		HRESULT hr = _DOCALL( GetDevice, ppDevice );							\
		return AllocOverride( &hr, m_Data.pDevice, ppDevice );					\
	}

#define IMPL_SETPRIVATEDATA()                                                                       \
    STDMETHOD(SetPrivateData)(THIS_ REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags) \
        { XXX; return _DOCALL(SetPrivateData, refguid, pData, SizeOfData, Flags); }

#define IMPL_GETPRIVATEDATA()                                                           \
    STDMETHOD(GetPrivateData)(THIS_ REFGUID refguid,void* pData,DWORD* pSizeOfData)     \
        { XXX; return _DOCALL(GetPrivateData, refguid, pData, pSizeOfData); }

#define IMPL_FREEPRIVATEDATA()                                                  \
    STDMETHOD(FreePrivateData)(THIS_ REFGUID refguid)                           \
        { XXX; return _DOCALL(FreePrivateData, refguid); }

#define IMPL_IDIRECT3DRESOURCE9()                                               \
    IMPL_GETDEVICE();                                                           \
    IMPL_SETPRIVATEDATA();                                                      \
    IMPL_GETPRIVATEDATA();                                                      \
    IMPL_FREEPRIVATEDATA();                                                     \
    STDMETHOD_(DWORD, SetPriority)(THIS_ DWORD PriorityNew)                     \
		{ XXX; return _DOCALL(SetPriority, PriorityNew); }                      \
    STDMETHOD_(DWORD, GetPriority)(THIS)                                        \
        { XXX; return _DOCALL0(GetPriority); }                                  \
    STDMETHOD_(void, PreLoad)(THIS)                                             \
        { XXX; _DOCALL0_NORET(PreLoad); }                                     	\
    STDMETHOD_(D3DRESOURCETYPE, GetType)(THIS)                                  \
        { XXX; return _DOCALL0(GetType); }

class CDirect3DSwapChain9Hook : public CDx9HookBase<IDirect3DSwapChain9>, public IDirect3DSwapChain9
{
public:
    /*** IUnknown methods ***/
    IMPL_IUNKOWN();

    /*** IDirect3DSwapChain9 methods ***/
    STDMETHOD(Present)(THIS_ CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion,DWORD dwFlags)
		{ XXX; return _DOCALL(Present, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags); }

    STDMETHOD(GetFrontBufferData)(THIS_ IDirect3DSurface9* pDestSurface)
	{
		XXX; return _DOCALL(GetFrontBufferData, GetHWPtr(pDestSurface));
	}
    STDMETHOD(GetBackBuffer)(THIS_ UINT iBackBuffer,D3DBACKBUFFER_TYPE Type,IDirect3DSurface9** ppBackBuffer)
	{
		XXX;
		HRESULT hr = _DOCALL(GetBackBuffer, iBackBuffer, Type, ppBackBuffer);
		return AllocOverride( &hr, m_Data.pDevice, ppBackBuffer );
	}

    STDMETHOD(GetRasterStatus)(THIS_ D3DRASTER_STATUS* pRasterStatus)
        { XXX; return _DOCALL(GetRasterStatus, pRasterStatus); }
    STDMETHOD(GetDisplayMode)(THIS_ D3DDISPLAYMODE* pMode)
        { XXX; return _DOCALL(GetDisplayMode, pMode); }

    IMPL_GETDEVICE();

    STDMETHOD(GetPresentParameters)(THIS_ D3DPRESENT_PARAMETERS* pPresentationParameters)
        { XXX; return _DOCALL(GetPresentParameters, pPresentationParameters); }
};

class CDirect3DVertexDeclaration9Hook : public CDx9HookBase<IDirect3DVertexDeclaration9>, public IDirect3DVertexDeclaration9
{
public:
    /*** IUnknown methods ***/
    IMPL_IUNKOWN();

    // IDirect3DVertexDeclaration9 methods
    IMPL_GETDEVICE();

    STDMETHOD(GetDeclaration)(THIS_ D3DVERTEXELEMENT9 *pVertElem, UINT* pNumElements)
        { XXX; return _DOCALL(GetDeclaration, pVertElem, pNumElements); }
};

class CDirect3DPixelShader9Hook : public CDx9HookBase<IDirect3DPixelShader9>, public IDirect3DPixelShader9
{
public:
    /*** IUnknown methods ***/
    IMPL_IUNKOWN();

    // IDirect3DPixelShader9 methods
    IMPL_GETDEVICE();

    STDMETHOD(GetFunction)(THIS_ void *pData,UINT* pSizeOfData)
        { XXX; return _DOCALL(GetFunction, pData, pSizeOfData); }
};

class CDirect3DVertexShader9Hook : public CDx9HookBase<IDirect3DVertexShader9>, public IDirect3DVertexShader9
{
public:
    /*** IUnknown methods ***/
    IMPL_IUNKOWN();

    // IDirect3DVertexDeclaration9 methods
    IMPL_GETDEVICE();

    STDMETHOD(GetFunction)(THIS_ void *pData,UINT* pSizeOfData)
        { XXX; return _DOCALL(GetFunction, pData, pSizeOfData); }
};

class CDirect3DVertexBuffer9Hook : public CDx9HookBase<IDirect3DVertexBuffer9>, public IDirect3DVertexBuffer9
{
public:
    /*** IUnknown methods ***/
    IMPL_IUNKOWN();

    // IDirect3DResource9 methods
    IMPL_IDIRECT3DRESOURCE9();

    // IDirect3DVertexBuffer9
    STDMETHOD(Lock)(THIS_ UINT OffsetToLock,UINT SizeToLock,void** ppbData,DWORD Flags)
		{ XXX; return _DOCALL(Lock, OffsetToLock, SizeToLock, ppbData, Flags); }
    STDMETHOD(Unlock)(THIS)
		{ XXX; return _DOCALL0(Unlock); }

    STDMETHOD(GetDesc)(THIS_ D3DVERTEXBUFFER_DESC *pDesc)
        { XXX; return _DOCALL(GetDesc, pDesc); }
};

class CDirect3DIndexBuffer9Hook : public CDx9HookBase<IDirect3DIndexBuffer9>, public IDirect3DIndexBuffer9
{
public:
    /*** IUnknown methods ***/
    IMPL_IUNKOWN();

    // IDirect3DResource9 methods
    IMPL_IDIRECT3DRESOURCE9();

    // IDirect3DIndexBuffer9
    STDMETHOD(Lock)(THIS_ UINT OffsetToLock,UINT SizeToLock,void** ppbData,DWORD Flags)
		{ XXX; return _DOCALL(Lock, OffsetToLock, SizeToLock, ppbData, Flags); }
    STDMETHOD(Unlock)(THIS)
		{ XXX; return _DOCALL0(Unlock); }

    STDMETHOD(GetDesc)(THIS_ D3DINDEXBUFFER_DESC *pDesc)
        { XXX; return _DOCALL(GetDesc, pDesc); }
};

class CDirect3DQuery9Hook : public CDx9HookBase<IDirect3DQuery9>, public IDirect3DQuery9
{
public:
    /*** IUnknown methods ***/
    IMPL_IUNKOWN();

    /*** IDirect3DQuery9 methods ***/
    IMPL_GETDEVICE();

    // IDirect3DQuery9
    STDMETHOD_(D3DQUERYTYPE, GetType)(THIS)
        { XXX; return _DOCALL0(GetType); }
    STDMETHOD_(DWORD, GetDataSize)(THIS)
        { XXX; return _DOCALL0(GetDataSize); }
    STDMETHOD(Issue)(THIS_ DWORD dwIssueFlags)
        { XXX; return _DOCALL(Issue, dwIssueFlags); }
    STDMETHOD(GetData)(THIS_ void* pData,DWORD dwSize,DWORD dwGetDataFlags)
        { XXX; return _DOCALL(GetData, pData, dwSize, dwGetDataFlags); }
};

class CDirect3DStateBlock9Hook : public CDx9HookBase<IDirect3DStateBlock9>, public IDirect3DStateBlock9
{
public:
    CDirect3DStateBlock9Hook() {}
    virtual ~CDirect3DStateBlock9Hook() {}

    /*** IUnknown methods ***/
    IMPL_IUNKOWN();

    // IDirect3DStateBlock9 methods
    IMPL_GETDEVICE();

    STDMETHOD(Capture)(THIS)
		{ XXX; return _DOCALL0(Capture); }
    STDMETHOD(Apply)(THIS)
		{ XXX; return _DOCALL0(Apply); }
};

class CDirect3DSurface9Hook : public CDx9HookBase<IDirect3DSurface9>, public IDirect3DSurface9
{
public:
    /*** IUnknown methods ***/
    IMPL_IUNKOWN();

    // IDirect3DResource9 methods
    IMPL_IDIRECT3DRESOURCE9();

    STDMETHOD(GetContainer)(THIS_ REFIID riid,void** ppContainer)
	{
		//$ TODO: do the call, check riid, and wrap the returned ppContainer
		__debugbreak();
		XXX; return _DOCALL(GetContainer, riid, ppContainer);
	}
    STDMETHOD(GetDesc)(THIS_ D3DSURFACE_DESC *pDesc)
        { XXX; return _DOCALL(GetDesc, pDesc); }

    STDMETHOD(LockRect)(THIS_ D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags)
		{ XXX; return _DOCALL(LockRect, pLockedRect, pRect, Flags); }
    STDMETHOD(UnlockRect)(THIS)
		{ XXX; return _DOCALL0(UnlockRect); }

    STDMETHOD(GetDC)(THIS_ HDC *phdc)
        { XXX; return _DOCALL(GetDC, phdc); }
    STDMETHOD(ReleaseDC)(THIS_ HDC hdc)
        { XXX; return _DOCALL(ReleaseDC, hdc); }
};

class CDirect3DBaseTexture9Hook : public CDx9HookBase<IDirect3DBaseTexture9>, public IDirect3DBaseTexture9
{
public:
    /*** IUnknown methods ***/
    IMPL_IUNKOWN();

    /*** IDirect3DBaseTexture9 methods ***/
    IMPL_IDIRECT3DRESOURCE9();

	STDMETHOD_(DWORD, SetLOD)(THIS_ DWORD LODNew)
		{ XXX; return _DOCALL(SetLOD, LODNew); }
	STDMETHOD_(DWORD, GetLOD)(THIS)
		{ XXX; return _DOCALL0(GetLOD); }
	STDMETHOD_(DWORD, GetLevelCount)(THIS)
		{ XXX; return _DOCALL0(GetLevelCount); }
	STDMETHOD(SetAutoGenFilterType)(THIS_ D3DTEXTUREFILTERTYPE FilterType)
		{ XXX; return _DOCALL(SetAutoGenFilterType, FilterType); }
	STDMETHOD_(D3DTEXTUREFILTERTYPE, GetAutoGenFilterType)(THIS)
		{ XXX; return _DOCALL0(GetAutoGenFilterType); }
	STDMETHOD_(void, GenerateMipSubLevels)(THIS)
		{ XXX; return _DOCALL0(GenerateMipSubLevels); }
};

class CDirect3DTexture9Hook : public CDx9HookBase<IDirect3DTexture9>, public IDirect3DTexture9
{
public:
    /*** IUnknown methods ***/
    IMPL_IUNKOWN();

    /*** IDirect3DBaseTexture9 methods ***/
    IMPL_IDIRECT3DRESOURCE9();

    STDMETHOD_(DWORD, SetLOD)(THIS_ DWORD LODNew)
		{ XXX; return _DOCALL(SetLOD, LODNew); }
    STDMETHOD_(DWORD, GetLOD)(THIS)
        { XXX; return _DOCALL0(GetLOD); }
    STDMETHOD_(DWORD, GetLevelCount)(THIS)
        { XXX; return _DOCALL0(GetLevelCount); }
    STDMETHOD(SetAutoGenFilterType)(THIS_ D3DTEXTUREFILTERTYPE FilterType)
		{ XXX; return _DOCALL(SetAutoGenFilterType, FilterType); }
    STDMETHOD_(D3DTEXTUREFILTERTYPE, GetAutoGenFilterType)(THIS)
        { XXX; return _DOCALL0(GetAutoGenFilterType); }

    STDMETHOD_(void, GenerateMipSubLevels)(THIS)
		{ XXX; return _DOCALL0(GenerateMipSubLevels); }

    STDMETHOD(GetLevelDesc)(THIS_ UINT Level,D3DSURFACE_DESC *pDesc)
        { XXX; return _DOCALL(GetLevelDesc, Level, pDesc); }

    STDMETHOD(GetSurfaceLevel)(THIS_ UINT Level,IDirect3DSurface9** ppSurfaceLevel)
	{
		XXX;
		HRESULT hr = _DOCALL(GetSurfaceLevel, Level, ppSurfaceLevel);
		return AllocOverride( &hr, m_Data.pDevice, ppSurfaceLevel );
	}
    STDMETHOD(LockRect)(THIS_ UINT Level,D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags)
		{ XXX; return _DOCALL(LockRect, Level, pLockedRect, pRect, Flags); }
    STDMETHOD(UnlockRect)(THIS_ UINT Level)
		{ XXX; return _DOCALL(UnlockRect, Level); }

    STDMETHOD(AddDirtyRect)(THIS_ CONST RECT* pDirtyRect)
        { XXX; return _DOCALL(AddDirtyRect, pDirtyRect); }
};

class CDirect3DCubeTexture9Hook : public CDx9HookBase<IDirect3DCubeTexture9>, public IDirect3DCubeTexture9
{
public:
    /*** IUnknown methods ***/
    IMPL_IUNKOWN();

    /*** IDirect3DBaseTexture9 methods ***/
    IMPL_IDIRECT3DRESOURCE9();

    STDMETHOD_(DWORD, SetLOD)(THIS_ DWORD LODNew)
		{ XXX; return _DOCALL(SetLOD, LODNew); }
    STDMETHOD_(DWORD, GetLOD)(THIS)
        { XXX; return _DOCALL0(GetLOD); }
    STDMETHOD_(DWORD, GetLevelCount)(THIS)
        { XXX; return _DOCALL0(GetLevelCount); }
    STDMETHOD(SetAutoGenFilterType)(THIS_ D3DTEXTUREFILTERTYPE FilterType)
		{ XXX; return _DOCALL(SetAutoGenFilterType, FilterType); }
    STDMETHOD_(D3DTEXTUREFILTERTYPE, GetAutoGenFilterType)(THIS)
        { XXX; return _DOCALL0(GetAutoGenFilterType); }

    STDMETHOD_(void, GenerateMipSubLevels)(THIS)
		{ XXX; return _DOCALL0(GenerateMipSubLevels); }

    STDMETHOD(GetLevelDesc)(THIS_ UINT Level,D3DSURFACE_DESC *pDesc)
        { XXX; return _DOCALL(GetLevelDesc, Level, pDesc); }

    STDMETHOD(GetCubeMapSurface)(THIS_ D3DCUBEMAP_FACES FaceType,UINT Level,IDirect3DSurface9** ppCubeMapSurface)
	{
		XXX;
		HRESULT hr = _DOCALL(GetCubeMapSurface, FaceType, Level, ppCubeMapSurface);
		return AllocOverride(&hr, m_Data.pDevice, ppCubeMapSurface);
	}
    STDMETHOD(LockRect)(THIS_ D3DCUBEMAP_FACES FaceType,UINT Level,D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags)
		{ XXX; return _DOCALL(LockRect, FaceType, Level, pLockedRect, pRect, Flags); }
    STDMETHOD(UnlockRect)(THIS_ D3DCUBEMAP_FACES FaceType,UINT Level)
		{ XXX; return _DOCALL(UnlockRect, FaceType, Level); }

    STDMETHOD(AddDirtyRect)(THIS_ D3DCUBEMAP_FACES FaceType,CONST RECT* pDirtyRect)
        { XXX; return _DOCALL(AddDirtyRect, FaceType, pDirtyRect); }
};

class CDirect3DVolume9Hook : public CDx9HookBase<IDirect3DVolume9>, public IDirect3DVolume9
{
public:
    /*** IUnknown methods ***/
    IMPL_IUNKOWN();

    /*** IDirect3DVolume9 methods ***/
    IMPL_GETDEVICE();

    IMPL_SETPRIVATEDATA();
    IMPL_GETPRIVATEDATA();
    IMPL_FREEPRIVATEDATA()

    STDMETHOD(GetContainer)(THIS_ REFIID riid,void** ppContainer)
	{
		//$ TODO: do the call, check riid, and wrap the returned ppContainer
		__debugbreak();
		XXX; return _DOCALL(GetContainer, riid, ppContainer);
	}

    STDMETHOD(GetDesc)(THIS_ D3DVOLUME_DESC *pDesc)
        { XXX; return _DOCALL(GetDesc, pDesc); }

    STDMETHOD(LockBox)(THIS_ D3DLOCKED_BOX * pLockedVolume,CONST D3DBOX* pBox,DWORD Flags)
		{ XXX; return _DOCALL(LockBox, pLockedVolume, pBox, Flags); }
    STDMETHOD(UnlockBox)(THIS)
		{ XXX; return _DOCALL0(UnlockBox); }
};

class CDirect3DVolumeTexture9Hook : public CDx9HookBase<IDirect3DVolumeTexture9>, public IDirect3DVolumeTexture9
{
public:
    /*** IUnknown methods ***/
    IMPL_IUNKOWN();

    /*** IDirect3DBaseTexture9 methods ***/
    IMPL_IDIRECT3DRESOURCE9();

    STDMETHOD_(DWORD, SetLOD)(THIS_ DWORD LODNew)
		{ XXX; return _DOCALL(SetLOD, LODNew); }
    STDMETHOD_(DWORD, GetLOD)(THIS)
        { XXX; return _DOCALL0(GetLOD); }
    STDMETHOD_(DWORD, GetLevelCount)(THIS)
        { XXX; return _DOCALL0(GetLevelCount); }
    STDMETHOD(SetAutoGenFilterType)(THIS_ D3DTEXTUREFILTERTYPE FilterType)
		{ XXX; return _DOCALL(SetAutoGenFilterType, FilterType); }
    STDMETHOD_(D3DTEXTUREFILTERTYPE, GetAutoGenFilterType)(THIS)
        { XXX; return _DOCALL0(GetAutoGenFilterType); }

    STDMETHOD_(void, GenerateMipSubLevels)(THIS)
		{ XXX; return _DOCALL0(GenerateMipSubLevels); }

    STDMETHOD(GetLevelDesc)(THIS_ UINT Level,D3DVOLUME_DESC *pDesc)
        { XXX; return _DOCALL(GetLevelDesc, Level, pDesc); }

    STDMETHOD(GetVolumeLevel)(THIS_ UINT Level,IDirect3DVolume9** ppVolumeLevel)
	{
		XXX;
		HRESULT hr = _DOCALL(GetVolumeLevel, Level, ppVolumeLevel);
		return AllocOverride( &hr, m_Data.pDevice, ppVolumeLevel );
	}
    STDMETHOD(LockBox)(THIS_ UINT Level,D3DLOCKED_BOX* pLockedVolume,CONST D3DBOX* pBox,DWORD Flags)
		{ XXX; return _DOCALL(LockBox, Level, pLockedVolume, pBox, Flags); }
    STDMETHOD(UnlockBox)(THIS_ UINT Level)
		{ XXX; return _DOCALL(UnlockBox, Level); }

    STDMETHOD(AddDirtyBox)(THIS_ CONST D3DBOX* pDirtyBox)
        { XXX; return _DOCALL(AddDirtyBox, pDirtyBox); }
};

class CDirect3DDevice9Hook : public CDx9HookBase<IDirect3DDevice9>, public IDirect3DDevice9
{
public:
    /*** IUnknown methods ***/
    IMPL_IUNKOWN();

	CDirect3DDevice9Hook() : CDx9HookBase<IDirect3DDevice9>(), IDirect3DDevice9()
	{
		D3D_BATCH_PERF( g_nTotalD3DCycles = 0; g_nTotalD3DCalls = 0; m_batch_state.Clear(); m_nTotalDraws = 0; m_nTotalPrims = 0; m_nTotalD3DCalls = 0; m_flTotalD3DTime = 0; m_nOverallDraws = 0; m_nOverallPrims = 0; m_nOverallD3DCalls = 0; m_flOverallD3DTime = 0; m_nTotalFrames = 0; m_pPrevRenderTarget0 = NULL; )
	}

    // IDirect3DDevice9 methods
    STDMETHOD(TestCooperativeLevel)(THIS)
        { XXX; return _DOCALL0(TestCooperativeLevel); }
    STDMETHOD_(UINT, GetAvailableTextureMem)(THIS)
        {  XXX; return _DOCALL0(GetAvailableTextureMem); }
    STDMETHOD(EvictManagedResources)(THIS)
        { XXX; return _DOCALL0(EvictManagedResources); }
    STDMETHOD(GetDirect3D)(THIS_ IDirect3D9** ppD3D9)
	{
		XXX;
		HRESULT hr = _DOCALL(GetDirect3D, ppD3D9);
		return AllocOverride(&hr, this, ppD3D9);
	}
    STDMETHOD(GetDeviceCaps)(THIS_ D3DCAPS9* pCaps)
		{ XXX; return _DOCALL(GetDeviceCaps, pCaps); }
    STDMETHOD(GetDisplayMode)(THIS_ UINT iSwapChain,D3DDISPLAYMODE* pMode)
        { XXX; return _DOCALL(GetDisplayMode, iSwapChain, pMode); }
    STDMETHOD(GetCreationParameters)(THIS_ D3DDEVICE_CREATION_PARAMETERS *pParameters)
        { XXX; return _DOCALL(GetCreationParameters, pParameters); }
    STDMETHOD(SetCursorProperties)(THIS_ UINT XHotSpot,UINT YHotSpot,IDirect3DSurface9* pCursorBitmap)
		{ XXX; return _DOCALL(SetCursorProperties, XHotSpot, YHotSpot, GetHWPtr(pCursorBitmap)); }
    STDMETHOD_(void, SetCursorPosition)(THIS_ int X,int Y,DWORD Flags)
        { XXX; _DOCALL_NORET(SetCursorPosition, X, Y, Flags); }
    STDMETHOD_(BOOL, ShowCursor)(THIS_ BOOL bShow)
        { XXX; return _DOCALL(ShowCursor, bShow); }
    STDMETHOD(CreateAdditionalSwapChain)(THIS_ D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DSwapChain9** ppSwapChain)
	{
		XXX;
		HRESULT hr = _DOCALL(CreateAdditionalSwapChain, pPresentationParameters, ppSwapChain);
		return AllocOverride(&hr, this, ppSwapChain);
	}
    STDMETHOD(GetSwapChain)(THIS_ UINT iSwapChain,IDirect3DSwapChain9** ppSwapChain)
	{
		XXX;
		HRESULT hr = _DOCALL(GetSwapChain, iSwapChain, ppSwapChain);
		return AllocOverride(&hr, this, ppSwapChain);
	}
    STDMETHOD_(UINT, GetNumberOfSwapChains)(THIS)
        { XXX; return _DOCALL0(GetNumberOfSwapChains); }
    STDMETHOD(Reset)(THIS_ D3DPRESENT_PARAMETERS* pPresentationParameters)
		{ XXX; return _DOCALL( Reset, pPresentationParameters ); }
    STDMETHOD(Present)(THIS_ CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion)
	{ 
		HRESULT hres;

#if D3D_BATCH_PERF_ANALYSIS
		uint64 nStartCycles = g_nTotalD3DCycles;

		CFastTimer tm;
		tm.Start();

		g_nTotalD3DCalls++;
#endif

		{
			XXX; 
			hres = _DOCALL(Present, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion); 
		}
				
#if D3D_BATCH_PERF_ANALYSIS
		const uint nFrameIndex = m_nTotalFrames;
		m_nTotalFrames++;

		if (nFrameIndex >= 5)
		{
			double flPresentTime = tm.GetDurationInProgress().GetMillisecondsF();
			uint64 nEndCycles = g_nTotalD3DCycles;

			double flTotalPresentTime = ( nEndCycles - nStartCycles ) * s_rdtsc_to_ms;

			m_nTotalD3DCalls += g_nTotalD3DCalls;
			m_flTotalD3DTime += g_nTotalD3DCycles * s_rdtsc_to_ms;
				
			static int bPrevBatchVis = -1;

			if ((bPrevBatchVis == 1) && m_batch_vis_bitmap.is_valid())
			{
				m_nOverallDraws += m_nTotalDraws;
				m_nOverallPrims += m_nTotalPrims;
				m_nOverallD3DCalls += m_nTotalD3DCalls;
				m_flOverallD3DTime += m_flTotalD3DTime;

				m_batch_vis_bitmap.fill_box(0, m_nBatchVisY, (uint)(.5f + flPresentTime / d3d_present_vis_abs_scale.GetFloat() * m_batch_vis_bitmap.width()), 10, 255, 16, 128);
				m_batch_vis_bitmap.additive_fill_box(0, m_nBatchVisY, (uint)(.5f + flTotalPresentTime / d3d_present_vis_abs_scale.GetFloat() * m_batch_vis_bitmap.width()), 10, 0, 255, 128);
				m_nBatchVisY += 10;
			
				uint y = MAX( 600, m_nBatchVisY + 20 ), l = 0;
				m_batch_vis_bitmap.draw_formatted_text(0, y+8*(l++), 1, 255, 255, 255, "Frame: %u, Batches: %u, Prims: %u", nFrameIndex, m_nTotalDraws, m_nTotalPrims );
				m_batch_vis_bitmap.draw_formatted_text(0, y+8*(l++), 1, 255, 255, 255, "Frame: D3D Calls: %u, D3D Time: %3.3fms", m_nTotalD3DCalls, m_flTotalD3DTime);
				l++;
				m_batch_vis_bitmap.draw_formatted_text(0, y+8*(l++), 1, 255, 255, 255, "Overall: Batches: %u, Prims: %u", m_nOverallDraws, m_nOverallPrims );
				m_batch_vis_bitmap.draw_formatted_text(0, y+8*(l++), 1, 255, 255, 255, "Overall: D3D Calls: %u D3D Time: %4.3fms", m_nOverallD3DCalls, m_flOverallD3DTime );

				size_t png_size = 0;
				void *pPNG_data = tdefl_write_image_to_png_file_in_memory(m_batch_vis_bitmap.get_ptr(), m_batch_vis_bitmap.width(), m_batch_vis_bitmap.height(), 3, &png_size, true);
				if (pPNG_data)
				{
					char filename[256];
					V_snprintf(filename, sizeof(filename), "left4dead2/batchvis_%u_%u.png", m_nBatchVisFileIdx, m_nBatchVisFrameIndex);
					FILE* pFile = fopen(filename, "wb");
					if (pFile)
					{
						fwrite(pPNG_data, png_size, 1, pFile);
						fclose(pFile);
					}
					free(pPNG_data);
				}
				m_nBatchVisFrameIndex++;
				m_nBatchVisY = 0;
				m_batch_vis_bitmap.cls();
			}

			if (bPrevBatchVis != (int)d3d_batch_vis.GetBool())
			{
				bPrevBatchVis = d3d_batch_vis.GetBool();
				if (!bPrevBatchVis)
				{
					m_batch_vis_bitmap.clear();
				}
				else
				{
					m_batch_vis_bitmap.init(768, 1024);
				}
				m_nBatchVisY = 0;
				m_nBatchVisFrameIndex = 0;
				m_nBatchVisFileIdx = (uint)time(NULL); //rand();

				m_nOverallDraws = 0;
				m_nOverallPrims = 0;
				m_nOverallD3DCalls = 0;
				m_flOverallD3DTime = 0;
			}
		}

		m_nTotalD3DCalls = 0;
		m_nTotalPrims = 0;
		m_flTotalD3DTime = 0;
		g_nTotalD3DCycles = 0;
		g_nTotalD3DCalls = 0;
		m_nTotalDraws = 0;
#else
		if ( d3d_batch_vis.GetBool() )
		{
			d3d_batch_vis.SetValue( false );

			ConMsg( "Must define D3D_BATCH_PERF_ANALYSIS to use this feature" );
		}
#endif

		return hres;
	}

    STDMETHOD(GetBackBuffer)(THIS_ UINT iSwapChain,UINT iBackBuffer,D3DBACKBUFFER_TYPE Type,IDirect3DSurface9** ppBackBuffer)
	{
		XXX;
		HRESULT hr = _DOCALL(GetBackBuffer, iSwapChain, iBackBuffer, Type, ppBackBuffer);
		return AllocOverride(&hr, this, ppBackBuffer);
	}
    STDMETHOD(GetRasterStatus)(THIS_ UINT iSwapChain,D3DRASTER_STATUS* pRasterStatus)
        { XXX; return _DOCALL(GetRasterStatus, iSwapChain, pRasterStatus); }
    STDMETHOD(SetDialogBoxMode)(THIS_ BOOL bEnableDialogs)
        { XXX; return _DOCALL(SetDialogBoxMode, bEnableDialogs); }
    STDMETHOD_(void, SetGammaRamp)(THIS_ UINT iSwapChain,DWORD Flags,CONST D3DGAMMARAMP* pRamp)
        { XXX; _DOCALL_NORET(SetGammaRamp, iSwapChain, Flags, pRamp); }
    STDMETHOD_(void, GetGammaRamp)(THIS_ UINT iSwapChain,D3DGAMMARAMP* pRamp)
        { XXX; _DOCALL_NORET(GetGammaRamp, iSwapChain, pRamp); }
    STDMETHOD(CreateTexture)(THIS_ UINT Width,UINT Height,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DTexture9** ppTexture,HANDLE* pSharedHandle)
	{
		XXX;
		HRESULT hr = _DOCALL(CreateTexture, Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);
		return AllocOverride(&hr, this, ppTexture);
	}
    STDMETHOD(CreateVolumeTexture)(THIS_ UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DVolumeTexture9** ppVolumeTexture,HANDLE* pSharedHandle)
	{
		XXX;
		HRESULT hr = _DOCALL(CreateVolumeTexture, Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle);
		return AllocOverride(&hr, this, ppVolumeTexture);
	}
    STDMETHOD(CreateCubeTexture)(THIS_ UINT EdgeLength,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DCubeTexture9** ppCubeTexture,HANDLE* pSharedHandle)
	{
		XXX;
		HRESULT hr = _DOCALL(CreateCubeTexture, EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle);
		return AllocOverride(&hr, this, ppCubeTexture);
	}
    STDMETHOD(CreateVertexBuffer)(THIS_ UINT Length,DWORD Usage,DWORD FVF,D3DPOOL Pool,IDirect3DVertexBuffer9** ppVertexBuffer,HANDLE* pSharedHandle)
	{
		XXX;
		HRESULT hr = _DOCALL(CreateVertexBuffer, Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle);
		return AllocOverride(&hr, this, ppVertexBuffer);
	}
    STDMETHOD(CreateIndexBuffer)(THIS_ UINT Length,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DIndexBuffer9** ppIndexBuffer,HANDLE* pSharedHandle)
	{
		XXX;
		HRESULT hr = _DOCALL(CreateIndexBuffer, Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle);
		return AllocOverride(&hr, this, ppIndexBuffer);
	}
    STDMETHOD(CreateRenderTarget)(THIS_ UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultiSampleQuality,BOOL Lockable,IDirect3DSurface9** ppSurface,HANDLE* pSharedHandle)
	{
		XXX;
		HRESULT hr = _DOCALL(CreateRenderTarget, Width, Height, Format, MultiSample, MultiSampleQuality, Lockable, ppSurface, pSharedHandle);
		return AllocOverride(&hr, this, ppSurface);
	}
    STDMETHOD(CreateDepthStencilSurface)(THIS_ UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultiSampleQuality,BOOL Discard,IDirect3DSurface9** ppSurface,HANDLE* pSharedHandle)
	{
		XXX;
        HRESULT hr = _DOCALL(CreateDepthStencilSurface, Width, Height, Format, MultiSample, MultiSampleQuality, Discard, ppSurface, pSharedHandle);
		return AllocOverride(&hr, this, ppSurface);
	}
    STDMETHOD(UpdateSurface)(THIS_ IDirect3DSurface9* pSourceSurface,CONST RECT* pSourceRect,IDirect3DSurface9* pDestinationSurface,CONST POINT* pDestPoint)
		{ XXX; return _DOCALL(UpdateSurface, GetHWPtr(pSourceSurface), pSourceRect, GetHWPtr(pDestinationSurface), pDestPoint); }
    STDMETHOD(UpdateTexture)(THIS_ IDirect3DBaseTexture9* pSourceTexture,IDirect3DBaseTexture9* pDestinationTexture)
		{ XXX; return _DOCALL(UpdateTexture, GetHWPtr(pSourceTexture), GetHWPtr(pDestinationTexture)); }
    STDMETHOD(GetRenderTargetData)(THIS_ IDirect3DSurface9* pRenderTarget,IDirect3DSurface9* pDestSurface)
		{ XXX; return _DOCALL(GetRenderTargetData, GetHWPtr(pRenderTarget), GetHWPtr(pDestSurface)); }
    STDMETHOD(GetFrontBufferData)(THIS_ UINT iSwapChain,IDirect3DSurface9* pDestSurface)
		{ XXX; return _DOCALL(GetFrontBufferData, iSwapChain, GetHWPtr(pDestSurface)); }
    STDMETHOD(StretchRect)(THIS_ IDirect3DSurface9* pSourceSurface,CONST RECT* pSourceRect,IDirect3DSurface9* pDestSurface,CONST RECT* pDestRect,D3DTEXTUREFILTERTYPE Filter)
		{ XXX; return _DOCALL(StretchRect, GetHWPtr(pSourceSurface), pSourceRect, GetHWPtr(pDestSurface), pDestRect, Filter); }
    STDMETHOD(ColorFill)(THIS_ IDirect3DSurface9* pSurface,CONST RECT* pRect,D3DCOLOR color)
		{ XXX; return _DOCALL(ColorFill, GetHWPtr(pSurface), pRect, color); }
    STDMETHOD(CreateOffscreenPlainSurface)(THIS_ UINT Width,UINT Height,D3DFORMAT Format,D3DPOOL Pool,IDirect3DSurface9** ppSurface,HANDLE* pSharedHandle)
	{
		XXX;
		HRESULT hr = _DOCALL(CreateOffscreenPlainSurface, Width, Height, Format, Pool, ppSurface, pSharedHandle);
		return AllocOverride(&hr, this, ppSurface);
	}
    
	STDMETHOD(SetRenderTarget)(THIS_ DWORD RenderTargetIndex,IDirect3DSurface9* pRenderTarget)
	{ 
		HRESULT hres;
		{
			XXX; 
			hres = _DOCALL(SetRenderTarget, RenderTargetIndex, GetHWPtr(pRenderTarget)); 
		}
#if D3D_BATCH_PERF_ANALYSIS
		if ( m_batch_vis_bitmap.is_valid() && !RenderTargetIndex )
		{
			if ( pRenderTarget != m_pPrevRenderTarget0 )
			{
				m_batch_vis_bitmap.fill_box(0, m_nBatchVisY, m_batch_vis_bitmap.width(), 1, 30, 20, 20);
				m_nBatchVisY += 1;
			}
		}
		if ( !RenderTargetIndex )
		{
			m_pPrevRenderTarget0 = pRenderTarget;
		}
#endif
		return hres;
	}

    STDMETHOD(GetRenderTarget)(THIS_ DWORD RenderTargetIndex,IDirect3DSurface9** ppRenderTarget)
	{
		XXX;
		HRESULT hr =  _DOCALL(GetRenderTarget, RenderTargetIndex, ppRenderTarget);
		return AllocOverride(&hr, this, ppRenderTarget);
	}
    STDMETHOD(SetDepthStencilSurface)(THIS_ IDirect3DSurface9* pNewZStencil)
		{ XXX; return _DOCALL(SetDepthStencilSurface, GetHWPtr(pNewZStencil)); }
    STDMETHOD(GetDepthStencilSurface)(THIS_ IDirect3DSurface9** ppZStencilSurface)
	{
		XXX;
		HRESULT hr = _DOCALL(GetDepthStencilSurface, ppZStencilSurface);
		return AllocOverride(&hr, this, ppZStencilSurface);
	}
    STDMETHOD(BeginScene)(THIS)
        { XXX; return _DOCALL0(BeginScene); }
    STDMETHOD(EndScene)(THIS)
        { XXX; return _DOCALL0(EndScene); }
    STDMETHOD(Clear)(THIS_ DWORD Count,CONST D3DRECT* pRects,DWORD Flags,D3DCOLOR Color,float Z,DWORD Stencil)
		{ XXX; return _DOCALL(Clear, Count, pRects, Flags, Color, Z, Stencil); }
    STDMETHOD(SetTransform)(THIS_ D3DTRANSFORMSTATETYPE State,CONST D3DMATRIX* pMatrix)
		{ XXX; return _DOCALL(SetTransform, State, pMatrix); }
    STDMETHOD(GetTransform)(THIS_ D3DTRANSFORMSTATETYPE State,D3DMATRIX* pMatrix)
        { XXX; return _DOCALL(GetTransform, State, pMatrix); }
    STDMETHOD(MultiplyTransform)(THIS_ D3DTRANSFORMSTATETYPE State,CONST D3DMATRIX *pMatrix)
        { XXX; return _DOCALL(MultiplyTransform, State, pMatrix); }
    STDMETHOD(SetViewport)(THIS_ CONST D3DVIEWPORT9* pViewport)
        { XXX; return _DOCALL(SetViewport, pViewport); }
    STDMETHOD(GetViewport)(THIS_ D3DVIEWPORT9* pViewport)
        { XXX; return _DOCALL(GetViewport, pViewport); }
    STDMETHOD(SetMaterial)(THIS_ CONST D3DMATERIAL9* pMaterial)
        { XXX; return _DOCALL(SetMaterial, pMaterial); }
    STDMETHOD(GetMaterial)(THIS_ D3DMATERIAL9* pMaterial)
        { XXX; return _DOCALL(GetMaterial, pMaterial); }
    STDMETHOD(SetLight)(THIS_ DWORD Index,CONST D3DLIGHT9* pLight)
		{ XXX; return _DOCALL(SetLight, Index, pLight); }
    STDMETHOD(GetLight)(THIS_ DWORD Index,D3DLIGHT9* pLight)
        { XXX; return _DOCALL(GetLight, Index, pLight); }
    STDMETHOD(LightEnable)(THIS_ DWORD Index,BOOL Enable)
		{ XXX; return _DOCALL(LightEnable, Index, Enable); }
    STDMETHOD(GetLightEnable)(THIS_ DWORD Index,BOOL* pEnable)
        { XXX; return _DOCALL(GetLightEnable, Index, pEnable); }
    STDMETHOD(SetClipPlane)(THIS_ DWORD Index,CONST float* pPlane)
        { XXX; return _DOCALL(SetClipPlane, Index, pPlane);}
    STDMETHOD(GetClipPlane)(THIS_ DWORD Index,float* pPlane)
        { XXX; return _DOCALL(GetClipPlane, Index, pPlane); }
    STDMETHOD(SetRenderState)(THIS_ D3DRENDERSTATETYPE State,DWORD Value)
        { XXX; return _DOCALL(SetRenderState, State, Value); }
    STDMETHOD(GetRenderState)(THIS_ D3DRENDERSTATETYPE State,DWORD* pValue)
        { XXX; return _DOCALL(GetRenderState, State, pValue); }
    STDMETHOD(CreateStateBlock)(THIS_ D3DSTATEBLOCKTYPE Type,IDirect3DStateBlock9** ppSB)
	{
		XXX;
		HRESULT hr = _DOCALL(CreateStateBlock, Type, ppSB);
		return AllocOverride(&hr, this, ppSB);
	}
    STDMETHOD(BeginStateBlock)(THIS)
		{ XXX; return _DOCALL0(BeginStateBlock); }
    STDMETHOD(EndStateBlock)(THIS_ IDirect3DStateBlock9** ppSB)
	{
		XXX;
		HRESULT hr = _DOCALL(EndStateBlock, ppSB);
		return AllocOverride(&hr, this, ppSB);
	}
    STDMETHOD(SetClipStatus)(THIS_ CONST D3DCLIPSTATUS9* pClipStatus)
        { XXX; return _DOCALL(SetClipStatus, pClipStatus);}
    STDMETHOD(GetClipStatus)(THIS_ D3DCLIPSTATUS9* pClipStatus)
        { XXX; return _DOCALL(GetClipStatus, pClipStatus);}
    STDMETHOD(GetTexture)(THIS_ DWORD Stage,IDirect3DBaseTexture9** ppTexture)
	{
		XXX;
		HRESULT hr = _DOCALL(GetTexture, Stage, ppTexture);
		return AllocOverride(&hr, this, ppTexture);
	}
    
	STDMETHOD(SetTexture)(THIS_ DWORD Stage,IDirect3DBaseTexture9* pTexture)
	{ 
		HRESULT hres;
		{
			XXX; 
			hres = _DOCALL(SetTexture, Stage, GetHWPtr(pTexture)); 
		}
		D3D_BATCH_PERF( m_batch_state.m_nNumSamplersChanged++; )
		return hres;
	}

    STDMETHOD(GetTextureStageState)(THIS_ DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD* pValue)
        { XXX; return _DOCALL(GetTextureStageState, Stage, Type, pValue); }
    STDMETHOD(SetTextureStageState)(THIS_ DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD Value)
        { XXX; return _DOCALL(SetTextureStageState, Stage, Type, Value); }
    STDMETHOD(GetSamplerState)(THIS_ DWORD Sampler,D3DSAMPLERSTATETYPE Type,DWORD* pValue)
        { XXX; return _DOCALL(GetSamplerState, Sampler, Type, pValue); }
    STDMETHOD(SetSamplerState)(THIS_ DWORD Sampler,D3DSAMPLERSTATETYPE Type,DWORD Value)
	{ 
		HRESULT hres;
		{
			XXX; 
			hres = _DOCALL(SetSamplerState, Sampler, Type, Value); 
		}
		D3D_BATCH_PERF( m_batch_state.m_nNumSamplerStatesChanged++; )
		return hres;
	}

    STDMETHOD(ValidateDevice)(THIS_ DWORD* pNumPasses)
        { XXX; return _DOCALL(ValidateDevice, pNumPasses); }
    STDMETHOD(SetPaletteEntries)(THIS_ UINT PaletteNumber,CONST PALETTEENTRY* pEntries)
        { XXX; return _DOCALL(SetPaletteEntries, PaletteNumber, pEntries); }
    STDMETHOD(GetPaletteEntries)(THIS_ UINT PaletteNumber,PALETTEENTRY* pEntries)
        { XXX; return _DOCALL(GetPaletteEntries, PaletteNumber, pEntries); }
    STDMETHOD(SetCurrentTexturePalette)(THIS_ UINT PaletteNumber)
        { XXX; return _DOCALL(SetCurrentTexturePalette, PaletteNumber);}
    STDMETHOD(GetCurrentTexturePalette)(THIS_ UINT *PaletteNumber)
        { XXX; return _DOCALL(GetCurrentTexturePalette, PaletteNumber); }
    STDMETHOD(SetScissorRect)(THIS_ CONST RECT* pRect)
        { XXX; return _DOCALL(SetScissorRect, pRect); }
    STDMETHOD(GetScissorRect)(THIS_ RECT* pRect)
        { XXX; return _DOCALL(GetScissorRect, pRect); }
    STDMETHOD(SetSoftwareVertexProcessing)(THIS_ BOOL bSoftware)
        { XXX; return _DOCALL(SetSoftwareVertexProcessing, bSoftware); }
    STDMETHOD_(BOOL, GetSoftwareVertexProcessing)(THIS)
        { XXX; return _DOCALL0(GetSoftwareVertexProcessing); }
    STDMETHOD(SetNPatchMode)(THIS_ float nSegments)
        { XXX; return _DOCALL(SetNPatchMode, nSegments); }
    STDMETHOD_(float, GetNPatchMode)(THIS)
        { XXX; return _DOCALL0(GetNPatchMode);}
    STDMETHOD(DrawPrimitive)(THIS_ D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount)
		{ XXX; m_nTotalDraws++; m_nTotalPrims += PrimitiveCount; return _DOCALL(DrawPrimitive, PrimitiveType, StartVertex, PrimitiveCount); }
    STDMETHOD(DrawIndexedPrimitive)(THIS_ D3DPRIMITIVETYPE PrimitiveType,INT BaseVertexIndex,UINT MinVertexIndex,UINT NumVertices,UINT startIndex,UINT primCount)
	{ 
		m_nTotalDraws++;
		m_nTotalPrims += primCount; 

#if D3D_BATCH_PERF_ANALYSIS
		CFastTimer tm;
		if ( m_batch_vis_bitmap.is_valid() )
		{
			tm.Start();
		}
#endif

		HRESULT hres;
		{
			XXX; 
			hres = _DOCALL(DrawIndexedPrimitive, PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount); 
		}
		
#if D3D_BATCH_PERF_ANALYSIS
		if ( m_batch_vis_bitmap.is_valid() )
		{
			double t = tm.GetDurationInProgress().GetMillisecondsF();

			uint h = 1;
			if ( d3d_batch_vis_y_scale.GetFloat() > 0.0f)
			{
				h = ceil( t / d3d_batch_vis_y_scale.GetFloat() );
				h = MAX(h, 1);
			}

			m_batch_vis_bitmap.fill_box(0, m_nBatchVisY, (uint)(.5f + t / d3d_batch_vis_abs_scale.GetFloat() * m_batch_vis_bitmap.width()), h, 64, 64, 64);

			if ( s_rdtsc_to_ms == 0.0f )
			{
				TmU64 t0 = tmFastTime();
				double d0 = Plat_FloatTime();

				ThreadSleep( 1 );

				TmU64 t1 = tmFastTime();
				double d1 = Plat_FloatTime();

				s_rdtsc_to_ms = ( 1000.0f * ( d1 - d0 ) ) / ( t1 - t0 );
			}

			double flTotalD3DCallMS = g_nTotalD3DCycles * s_rdtsc_to_ms;

			m_batch_vis_bitmap.additive_fill_box(0, m_nBatchVisY, (uint)(.5f + flTotalD3DCallMS / d3d_batch_vis_abs_scale.GetFloat() * m_batch_vis_bitmap.width()), h, 96, 96, 128);

			if (m_batch_state.m_bVertexShaderChanged) m_batch_vis_bitmap.additive_fill_box(0, m_nBatchVisY, 8, h, 0, 0, 64);
			if (m_batch_state.m_bPixelShaderChanged) m_batch_vis_bitmap.additive_fill_box(32, m_nBatchVisY, 8, h, 64, 0, 64);

			int lm = 80;
			m_batch_vis_bitmap.fill_box(lm+0+0, m_nBatchVisY, m_batch_state.m_nNumVSConstants, h, 64, 255, 255);
			m_batch_vis_bitmap.fill_box(lm+64+256+0, m_nBatchVisY, m_batch_state.m_nNumPSConstants, h, 64, 64, 255);

			m_batch_vis_bitmap.fill_box(lm+64+256+32, m_nBatchVisY, m_batch_state.m_nNumSamplersChanged, h, 255, 255, 255);
			m_batch_vis_bitmap.fill_box(lm+64+256+32+16, m_nBatchVisY, m_batch_state.m_nNumSamplerStatesChanged, h, 92, 128, 255);

			if ( m_batch_state.m_bStreamSourceChanged) m_batch_vis_bitmap.fill_box(lm+64+256+32+16+64, m_nBatchVisY, 16, h, 128, 128, 128);
			if ( m_batch_state.m_bIndicesChanged ) m_batch_vis_bitmap.fill_box(lm+64+256+32+16+64+16, m_nBatchVisY, 16, h, 128, 128, 255);

			m_nBatchVisY += h;
			
			m_nTotalD3DCalls += g_nTotalD3DCalls;
			m_flTotalD3DTime += flTotalD3DCallMS;
			
			g_nTotalD3DCycles = 0;
			g_nTotalD3DCalls = 0;
			
			m_batch_state.Clear();
		}
#endif

		return hres;
	}

    STDMETHOD(DrawPrimitiveUP)(THIS_ D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCount,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
		{ XXX; m_nTotalDraws++; m_nTotalPrims += PrimitiveCount; return _DOCALL(DrawPrimitiveUP, PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride); }
    STDMETHOD(DrawIndexedPrimitiveUP)(THIS_ D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertices,UINT PrimitiveCount,CONST void* pIndexData,D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
		{ XXX; m_nTotalDraws++; m_nTotalPrims += PrimitiveCount; return _DOCALL(DrawIndexedPrimitiveUP, PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride); }
    STDMETHOD(ProcessVertices)(THIS_ UINT SrcStartIndex,UINT DestIndex,UINT VertexCount,IDirect3DVertexBuffer9* pDestBuffer,IDirect3DVertexDeclaration9* pVertexDecl,DWORD Flags)
		{ XXX; return _DOCALL(ProcessVertices, SrcStartIndex, DestIndex, VertexCount, GetHWPtr(pDestBuffer), GetHWPtr(pVertexDecl), Flags); }
    STDMETHOD(CreateVertexDeclaration)(THIS_ CONST D3DVERTEXELEMENT9* pVertexElements,IDirect3DVertexDeclaration9** ppDecl)
	{
		XXX;
		HRESULT hr = _DOCALL(CreateVertexDeclaration, pVertexElements, ppDecl);
		return AllocOverride(&hr, this, ppDecl);
	}
    STDMETHOD(SetVertexDeclaration)(THIS_ IDirect3DVertexDeclaration9* pDecl)
		{ XXX; return _DOCALL(SetVertexDeclaration, GetHWPtr(pDecl)); }
    STDMETHOD(GetVertexDeclaration)(THIS_ IDirect3DVertexDeclaration9** ppDecl)
	{
		XXX;
		HRESULT hr = _DOCALL(GetVertexDeclaration, ppDecl);
		return AllocOverride(&hr, this, ppDecl);
	}
    STDMETHOD(SetFVF)(THIS_ DWORD FVF)
		{ XXX; return _DOCALL(SetFVF, FVF); }
    STDMETHOD(GetFVF)(THIS_ DWORD* pFVF)
        { XXX; return _DOCALL(GetFVF, pFVF); }
    STDMETHOD(CreateVertexShader)(THIS_ CONST DWORD* pFunction,IDirect3DVertexShader9** ppShader)
	{
		XXX;
		HRESULT hr = _DOCALL(CreateVertexShader, pFunction, ppShader);
		return AllocOverride(&hr, this, ppShader);
	}
    
	STDMETHOD(SetVertexShader)(THIS_ IDirect3DVertexShader9* pShader)
	{ 
		HRESULT hres;
		{
			XXX; 
			hres = _DOCALL(SetVertexShader, GetHWPtr(pShader)); 
		}
		D3D_BATCH_PERF( m_batch_state.m_bVertexShaderChanged = true; )
		return hres;
	}

    STDMETHOD(GetVertexShader)(THIS_ IDirect3DVertexShader9** ppShader)
	{
		XXX;
		HRESULT hr = _DOCALL(GetVertexShader, ppShader);
		return AllocOverride(&hr, this, ppShader);
	}
    
	STDMETHOD(SetVertexShaderConstantF)(THIS_ UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount)
	{ 
		HRESULT hres;
		{
			XXX; 
			hres = _DOCALL(SetVertexShaderConstantF, StartRegister, pConstantData, Vector4fCount); 
		}
		D3D_BATCH_PERF( m_batch_state.m_nNumVSConstants += Vector4fCount; )
		return hres;
	}

    STDMETHOD(GetVertexShaderConstantF)(THIS_ UINT StartRegister,float* pConstantData,UINT Vector4fCount)
		{ XXX; return _DOCALL(GetVertexShaderConstantF, StartRegister, pConstantData, Vector4fCount);}
    STDMETHOD(SetVertexShaderConstantI)(THIS_ UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount)
		{ XXX; return _DOCALL(SetVertexShaderConstantI, StartRegister, pConstantData, Vector4iCount); }
    STDMETHOD(GetVertexShaderConstantI)(THIS_ UINT StartRegister,int* pConstantData,UINT Vector4iCount)
		{ XXX; return _DOCALL(GetVertexShaderConstantI, StartRegister, pConstantData, Vector4iCount); }
    STDMETHOD(SetVertexShaderConstantB)(THIS_ UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount)
		{ XXX; return _DOCALL(SetVertexShaderConstantB, StartRegister, pConstantData, BoolCount); }
    STDMETHOD(GetVertexShaderConstantB)(THIS_ UINT StartRegister,BOOL* pConstantData,UINT BoolCount)
		{ XXX; return _DOCALL(GetVertexShaderConstantB, StartRegister, pConstantData, BoolCount); }
		    
	STDMETHOD(SetStreamSource)(THIS_ UINT StreamNumber,IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride)
		
	{ 
		HRESULT hres;
		{
			XXX; 
			hres = _DOCALL(SetStreamSource, StreamNumber, GetHWPtr(pStreamData), OffsetInBytes, Stride);
		}

		D3D_BATCH_PERF( m_batch_state.m_bStreamSourceChanged = true; )

		return hres;
	}

    STDMETHOD(GetStreamSource)(THIS_ UINT StreamNumber,IDirect3DVertexBuffer9** ppStreamData,UINT* OffsetInBytes,UINT* pStride)
	{
		XXX;
		HRESULT hr = _DOCALL(GetStreamSource, StreamNumber, ppStreamData, OffsetInBytes, pStride);
		return AllocOverride(&hr, this, ppStreamData);
	}
    STDMETHOD(SetStreamSourceFreq)(THIS_ UINT StreamNumber,UINT Divider)
        { XXX; return _DOCALL(SetStreamSourceFreq, StreamNumber, Divider); }

    STDMETHOD(GetStreamSourceFreq)(THIS_ UINT StreamNumber,UINT* Divider)
        { XXX; return _DOCALL(GetStreamSourceFreq, StreamNumber, Divider); }
    
	STDMETHOD(SetIndices)(THIS_ IDirect3DIndexBuffer9* pIndexData)
	{ 
		HRESULT hres;

		{
			XXX; 
			hres = _DOCALL(SetIndices, GetHWPtr(pIndexData)); 
		}

		D3D_BATCH_PERF( m_batch_state.m_bIndicesChanged = true; )

		return hres;
	}

    STDMETHOD(GetIndices)(THIS_ IDirect3DIndexBuffer9** ppIndexData)
	{
		XXX;
		HRESULT hr = _DOCALL(GetIndices, ppIndexData);
		return AllocOverride(&hr, this, ppIndexData);
	}
    STDMETHOD(CreatePixelShader)(THIS_ CONST DWORD* pFunction,IDirect3DPixelShader9** ppShader)
	{
		XXX;
		HRESULT hr = _DOCALL(CreatePixelShader, pFunction, ppShader);
		return AllocOverride(&hr, this, ppShader);
	}
    
	STDMETHOD(SetPixelShader)(THIS_ IDirect3DPixelShader9* pShader)
	{ 
		HRESULT hres;
		{
			XXX; 
			hres = _DOCALL(SetPixelShader, GetHWPtr(pShader)); 
		}
		D3D_BATCH_PERF( m_batch_state.m_bPixelShaderChanged = true; )
		return hres;
	}

    STDMETHOD(GetPixelShader)(THIS_ IDirect3DPixelShader9** ppShader)
	{
		XXX;
		HRESULT hr = _DOCALL(GetPixelShader, ppShader);
		return AllocOverride(&hr, this, ppShader);
	}
    
	STDMETHOD(SetPixelShaderConstantF)(THIS_ UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount)
	{ 
		HRESULT hres;
		{
			XXX; 
			hres = _DOCALL(SetPixelShaderConstantF, StartRegister, pConstantData, Vector4fCount); 
		}
		D3D_BATCH_PERF( m_batch_state.m_nNumPSConstants += Vector4fCount; )
		return hres;
	}

    STDMETHOD(GetPixelShaderConstantF)(THIS_ UINT StartRegister,float* pConstantData,UINT Vector4fCount)
		{ XXX; return _DOCALL(GetPixelShaderConstantF, StartRegister, pConstantData, Vector4fCount); }
    STDMETHOD(SetPixelShaderConstantI)(THIS_ UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount)
		{ XXX; return _DOCALL(SetPixelShaderConstantI, StartRegister, pConstantData, Vector4iCount); }
    STDMETHOD(GetPixelShaderConstantI)(THIS_ UINT StartRegister,int* pConstantData,UINT Vector4iCount)
		{ XXX; return _DOCALL(GetPixelShaderConstantI, StartRegister, pConstantData, Vector4iCount); }
    STDMETHOD(SetPixelShaderConstantB)(THIS_ UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount)
		{ XXX; return _DOCALL(SetPixelShaderConstantB, StartRegister, pConstantData, BoolCount); }
    STDMETHOD(GetPixelShaderConstantB)(THIS_ UINT StartRegister,BOOL* pConstantData,UINT BoolCount)
		{ XXX; return _DOCALL(GetPixelShaderConstantB, StartRegister, pConstantData, BoolCount); }
    STDMETHOD(DrawRectPatch)(THIS_ UINT Handle,CONST float* pNumSegs,CONST D3DRECTPATCH_INFO* pRectPatchInfo)
        { XXX; return _DOCALL(DrawRectPatch, Handle, pNumSegs, pRectPatchInfo);}
    STDMETHOD(DrawTriPatch)(THIS_ UINT Handle,CONST float* pNumSegs,CONST D3DTRIPATCH_INFO* pTriPatchInfo)
        { XXX; return _DOCALL(DrawTriPatch, Handle, pNumSegs, pTriPatchInfo); }
    STDMETHOD(DeletePatch)(THIS_ UINT Handle)
        { XXX; return _DOCALL(DeletePatch, Handle);}
    STDMETHOD(CreateQuery)(THIS_ D3DQUERYTYPE Type,IDirect3DQuery9** ppQuery)
	{
		XXX;
		HRESULT hr = _DOCALL(CreateQuery, Type, ppQuery);
		return AllocOverride(&hr, this, ppQuery);
	}

private:

#if D3D_BATCH_PERF_ANALYSIS
	IDirect3DSurface9* m_pPrevRenderTarget0;
	simple_bitmap m_batch_vis_bitmap;
	uint m_nBatchVisY;
	uint m_nBatchVisFrameIndex, m_nBatchVisFileIdx;
	
	struct BatchState_t
	{
		void Clear() { memset(this, 0, sizeof(*this)); }

		bool m_bStreamSourceChanged;
		bool m_bIndicesChanged;
		bool m_bPixelShaderChanged;
		bool m_bVertexShaderChanged;
		uint m_nNumPSConstants;
		uint m_nNumVSConstants;
		uint m_nNumSamplersChanged;
		uint m_nNumSamplerStatesChanged;
	};

	BatchState_t m_batch_state;
	
	uint m_nTotalFrames;

	uint m_nTotalDraws;
	uint m_nTotalPrims;
	uint m_nTotalD3DCalls;
	double m_flTotalD3DTime;

	uint m_nOverallDraws;
	uint m_nOverallPrims;
	uint m_nOverallD3DCalls;
	double m_flOverallD3DTime;
#endif
};

class CDirect3D9Hook : public CDx9HookBase<IDirect3D9>, public IDirect3D9
{
public:
    /*** IUnknown methods ***/
    IMPL_IUNKOWN();

    // IDirect3D9 methods
    STDMETHOD(RegisterSoftwareDevice)(THIS_ void* pInitializeFunction)
		{ XXX; return _DOCALL(RegisterSoftwareDevice, pInitializeFunction); }
    STDMETHOD_(UINT, GetAdapterCount)(THIS)
		{ XXX; return _DOCALL0(GetAdapterCount); }
    STDMETHOD(GetAdapterIdentifier)(THIS_ UINT Adapter,DWORD Flags,D3DADAPTER_IDENTIFIER9* pIdentifier)
		{ XXX; return _DOCALL(GetAdapterIdentifier, Adapter, Flags, pIdentifier);}
    STDMETHOD_(UINT, GetAdapterModeCount)(THIS_ UINT Adapter,D3DFORMAT Format)
		{ XXX; return _DOCALL(GetAdapterModeCount, Adapter, Format);}
    STDMETHOD(EnumAdapterModes)(THIS_ UINT Adapter,D3DFORMAT Format,UINT Mode,D3DDISPLAYMODE* pMode)
		{ XXX; return _DOCALL(EnumAdapterModes, Adapter, Format, Mode, pMode); }
    STDMETHOD(GetAdapterDisplayMode)(THIS_ UINT Adapter,D3DDISPLAYMODE* pMode)
		{ XXX; return _DOCALL(GetAdapterDisplayMode, Adapter, pMode); }
    STDMETHOD(CheckDeviceType)(THIS_ UINT iAdapter,D3DDEVTYPE DevType,D3DFORMAT DisplayFormat,D3DFORMAT BackBufferFormat,BOOL bWindowed)
		{ XXX; return _DOCALL(CheckDeviceType, iAdapter, DevType, DisplayFormat, BackBufferFormat, bWindowed); }
    STDMETHOD(CheckDeviceFormat)(THIS_ UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,DWORD Usage,D3DRESOURCETYPE RType,D3DFORMAT CheckFormat)
		{ XXX; return _DOCALL(CheckDeviceFormat, Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat); }
    STDMETHOD(CheckDeviceMultiSampleType)(THIS_ UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT SurfaceFormat,BOOL Windowed,D3DMULTISAMPLE_TYPE MultiSampleType,DWORD* pQualityLevels)
		{ XXX; return _DOCALL(CheckDeviceMultiSampleType, Adapter, DeviceType, SurfaceFormat, Windowed, MultiSampleType, pQualityLevels); }
    STDMETHOD(CheckDepthStencilMatch)(THIS_ UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,D3DFORMAT RenderTargetFormat,D3DFORMAT DepthStencilFormat)
		{ XXX; return _DOCALL(CheckDepthStencilMatch, Adapter, DeviceType, AdapterFormat, RenderTargetFormat, DepthStencilFormat); }
    STDMETHOD(CheckDeviceFormatConversion)(THIS_ UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT SourceFormat,D3DFORMAT TargetFormat)
		{ XXX; return _DOCALL(CheckDeviceFormatConversion, Adapter, DeviceType, SourceFormat, TargetFormat); }
    STDMETHOD(GetDeviceCaps)(THIS_ UINT Adapter,D3DDEVTYPE DeviceType,D3DCAPS9* pCaps)
		{ XXX; return _DOCALL(GetDeviceCaps, Adapter, DeviceType, pCaps); }
    STDMETHOD_(HMONITOR, GetAdapterMonitor)(THIS_ UINT Adapter)
		{ XXX; return _DOCALL(GetAdapterMonitor, Adapter); }
    STDMETHOD(CreateDevice)(THIS_ UINT Adapter,D3DDEVTYPE DeviceType,HWND hFocusWindow,DWORD BehaviorFlags,D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DDevice9** ppReturnedDeviceInterface)
	{
		XXX;
		HRESULT hr = _DOCALL(CreateDevice, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

		if( SUCCEEDED( hr ) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface )
		{
			CDirect3DDevice9Hook *pDevice = new CDirect3DDevice9Hook;
			if(!pDevice)
			{
				( *ppReturnedDeviceInterface )->Release();
				hr = E_OUTOFMEMORY;
				return NULL;
			}
			pDevice->m_Data.pDevice = pDevice;
			pDevice->m_Data.pHWObj = *ppReturnedDeviceInterface;
			*ppReturnedDeviceInterface = pDevice;
		}
		return hr;
	}
};

inline IDirect3D9 *Direct3DCreate9Hook( UINT SDKVersion )
{
	HRESULT hr = S_OK;
	IDirect3D9 *pD3D = Direct3DCreate9( D3D_SDK_VERSION );
	AllocOverride( &hr, NULL, &pD3D );
	return pD3D;
}

#endif // _DX9HOOK_H_
