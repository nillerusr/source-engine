//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef SHADERDEVICEDX10_H
#define SHADERDEVICEDX10_H

#ifdef _WIN32
#pragma once
#endif


#include "shaderdevicebase.h"
#include "tier1/utlvector.h"
#include "tier1/utlrbtree.h"
#include "tier1/utllinkedlist.h"


//-----------------------------------------------------------------------------
// Forward declaration
//-----------------------------------------------------------------------------
struct IDXGIFactory;
struct IDXGIAdapter;
struct IDXGIOutput;
struct IDXGISwapChain;
struct ID3D10Device;
struct ID3D10RenderTargetView;
struct ID3D10VertexShader;
struct ID3D10PixelShader;
struct ID3D10GeometryShader;
struct ID3D10InputLayout;
struct ID3D10ShaderReflection;


//-----------------------------------------------------------------------------
// The Base implementation of the shader device
//-----------------------------------------------------------------------------
class CShaderDeviceMgrDx10 : public CShaderDeviceMgrBase
{
	typedef CShaderDeviceMgrBase BaseClass;

public:
	// constructor, destructor
	CShaderDeviceMgrDx10();
	virtual ~CShaderDeviceMgrDx10();

	// Methods of IAppSystem
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// Methods of IShaderDeviceMgr
	virtual int	 GetAdapterCount() const;
	virtual void GetAdapterInfo( int adapter, MaterialAdapterInfo_t& info ) const;
	virtual int	 GetModeCount( int nAdapter ) const;
	virtual void GetModeInfo( ShaderDisplayMode_t* pInfo, int nAdapter, int mode ) const;
	virtual void GetCurrentModeInfo( ShaderDisplayMode_t* pInfo, int nAdapter ) const;
	virtual bool SetAdapter( int nAdapter, int nFlags );
	virtual CreateInterfaceFn SetMode( void *hWnd, int nAdapter, const ShaderDeviceInfo_t& mode );

private:
	// Initialize adapter information
	void InitAdapterInfo();

	// Determines hardware caps from D3D
	bool ComputeCapsFromD3D( HardwareCaps_t *pCaps, IDXGIAdapter *pAdapter, IDXGIOutput *pOutput );

	// Returns the amount of video memory in bytes for a particular adapter
	virtual int GetVidMemBytes( int nAdapter ) const;

	// Returns the appropriate adapter output to use
	IDXGIOutput* GetAdapterOutput( int nAdapter ) const;

	// Returns the adapter interface for a particular adapter
	IDXGIAdapter* GetAdapter( int nAdapter ) const;

	// Used to enumerate adapters, attach to windows
	IDXGIFactory *m_pDXGIFactory;

	bool m_bObeyDxCommandlineOverride: 1;

	friend class CShaderDeviceDx10;
};


//-----------------------------------------------------------------------------
// The Dx10 implementation of the shader device
//-----------------------------------------------------------------------------
class CShaderDeviceDx10 : public CShaderDeviceBase
{
public:
	// constructor, destructor
	CShaderDeviceDx10();
	virtual ~CShaderDeviceDx10();

public:
	// Methods of IShaderDevice
	virtual bool IsUsingGraphics() const;
	virtual int GetCurrentAdapter() const;
	virtual ImageFormat GetBackBufferFormat() const;
	virtual void GetBackBufferDimensions( int& width, int& height ) const;
	virtual void SpewDriverInfo() const;
	virtual void Present();
	virtual IShaderBuffer* CompileShader( const char *pProgram, size_t nBufLen, const char *pShaderVersion );
	virtual VertexShaderHandle_t CreateVertexShader( IShaderBuffer *pShader );
	virtual void DestroyVertexShader( VertexShaderHandle_t hShader );
	virtual GeometryShaderHandle_t CreateGeometryShader( IShaderBuffer* pShaderBuffer );
	virtual void DestroyGeometryShader( GeometryShaderHandle_t hShader );
	virtual PixelShaderHandle_t CreatePixelShader( IShaderBuffer* pShaderBuffer );
	virtual void DestroyPixelShader( PixelShaderHandle_t hShader );
	virtual void ReleaseResources() {}
	virtual void ReacquireResources() {}
	virtual IMesh* CreateStaticMesh( VertexFormat_t format, const char *pTextureBudgetGroup, IMaterial * pMaterial );
	virtual void DestroyStaticMesh( IMesh* mesh );
	virtual IVertexBuffer *CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pTextureBudgetGroup );
	virtual void DestroyVertexBuffer( IVertexBuffer *pVertexBuffer );
	virtual IIndexBuffer *CreateIndexBuffer( ShaderBufferType_t type, MaterialIndexFormat_t fmt, int nIndexCount, const char *pTextureBudgetGroup );
	virtual void DestroyIndexBuffer( IIndexBuffer *pIndexBuffer );
	virtual IVertexBuffer *GetDynamicVertexBuffer( int nStreamID, VertexFormat_t vertexFormat, bool bBuffered = true );
	virtual IIndexBuffer *GetDynamicIndexBuffer( MaterialIndexFormat_t fmt, bool bBuffered = true );
	virtual void SetHardwareGammaRamp( float fGamma, float fGammaTVRangeMin, float fGammaTVRangeMax, float fGammaTVExponent, bool bTVEnabled );

	// A special path used to tick the front buffer while loading on the 360
	virtual void EnableNonInteractiveMode( MaterialNonInteractiveMode_t mode, ShaderNonInteractiveInfo_t *pInfo ) {}
	virtual void RefreshFrontBufferNonInteractive( ) {}
	virtual void HandleThreadEvent( uint32 threadEvent ) {}

public:
	// Methods of CShaderDeviceBase
	virtual bool InitDevice( void *hWnd, int nAdapter, const ShaderDeviceInfo_t& mode );
	virtual void ShutdownDevice();
	virtual bool IsDeactivated() const { return false; }

	// Other public methods
	ID3D10VertexShader* GetVertexShader( VertexShaderHandle_t hShader ) const;
	ID3D10GeometryShader* GetGeometryShader( GeometryShaderHandle_t hShader ) const;
	ID3D10PixelShader* GetPixelShader( PixelShaderHandle_t hShader ) const;
	ID3D10InputLayout* GetInputLayout( VertexShaderHandle_t hShader, VertexFormat_t format );

private:
	struct InputLayout_t
	{
		ID3D10InputLayout *m_pInputLayout;
		VertexFormat_t m_VertexFormat;
	};

	typedef CUtlRBTree< InputLayout_t, unsigned short > InputLayoutDict_t;

	static bool InputLayoutLessFunc( const InputLayout_t &lhs, const InputLayout_t &rhs )	
	{ 
		return ( lhs.m_VertexFormat < rhs.m_VertexFormat );	
	}

	struct VertexShader_t
	{
		ID3D10VertexShader *m_pShader;
		ID3D10ShaderReflection *m_pInfo;
		void *m_pByteCode;
		size_t m_nByteCodeLen;
		InputLayoutDict_t m_InputLayouts;

		VertexShader_t() : m_InputLayouts( 0, 0, InputLayoutLessFunc ) {}
	};

	struct GeometryShader_t
	{
		ID3D10GeometryShader *m_pShader;
		ID3D10ShaderReflection *m_pInfo;
	};

	struct PixelShader_t
	{
		ID3D10PixelShader *m_pShader;
		ID3D10ShaderReflection *m_pInfo;
	};

	typedef CUtlFixedLinkedList< VertexShader_t >::IndexType_t VertexShaderIndex_t;
	typedef CUtlFixedLinkedList< GeometryShader_t >::IndexType_t GeometryShaderIndex_t;
	typedef CUtlFixedLinkedList< PixelShader_t >::IndexType_t PixelShaderIndex_t;

	void SetupHardwareCaps();
	void ReleaseInputLayouts( VertexShaderIndex_t nIndex );

	IDXGIOutput *m_pOutput;
	ID3D10Device *m_pDevice;	
	IDXGISwapChain *m_pSwapChain;
	ID3D10RenderTargetView *m_pRenderTargetView;

	CUtlFixedLinkedList< VertexShader_t > m_VertexShaderDict;
	CUtlFixedLinkedList< GeometryShader_t > m_GeometryShaderDict;
	CUtlFixedLinkedList< PixelShader_t > m_PixelShaderDict;

	friend ID3D10Device *D3D10Device();
	friend IDXGISwapChain *D3D10SwapChain();
	friend ID3D10RenderTargetView *D3D10RenderTargetView();
};


//-----------------------------------------------------------------------------
// Inline methods of CShaderDeviceDx10
//-----------------------------------------------------------------------------
inline ID3D10VertexShader* CShaderDeviceDx10::GetVertexShader( VertexShaderHandle_t hShader ) const
{
	if ( hShader != VERTEX_SHADER_HANDLE_INVALID )
		return m_VertexShaderDict[ (VertexShaderIndex_t)hShader ].m_pShader;
	return NULL;
}

inline ID3D10GeometryShader* CShaderDeviceDx10::GetGeometryShader( GeometryShaderHandle_t hShader ) const
{
	if ( hShader != GEOMETRY_SHADER_HANDLE_INVALID )
		return m_GeometryShaderDict[ (GeometryShaderIndex_t)hShader ].m_pShader;
	return NULL;
}

inline ID3D10PixelShader* CShaderDeviceDx10::GetPixelShader( PixelShaderHandle_t hShader ) const
{
	if ( hShader != PIXEL_SHADER_HANDLE_INVALID )
		return m_PixelShaderDict[ (PixelShaderIndex_t)hShader ].m_pShader;
	return NULL;
}


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
extern CShaderDeviceDx10* g_pShaderDeviceDx10;


//-----------------------------------------------------------------------------
// Utility methods
//-----------------------------------------------------------------------------
inline ID3D10Device *D3D10Device()
{
	return g_pShaderDeviceDx10->m_pDevice;	
}

inline IDXGISwapChain *D3D10SwapChain()
{
	return g_pShaderDeviceDx10->m_pSwapChain;	
}

inline ID3D10RenderTargetView *D3D10RenderTargetView()
{
	return g_pShaderDeviceDx10->m_pRenderTargetView;	
}


#endif // SHADERDEVICEDX10_H