//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef STUBD3DDEVICE_H
#define STUBD3DDEVICE_H
#ifdef _WIN32
#pragma once
#endif

#ifdef STUBD3D

#include "locald3dtypes.h"
#include "filesystem.h"

#ifdef USE_FOPEN
#include <stdio.h>
#define FPRINTF fprintf
#else
#define FPRINTF s_pFileSystem->FPrintf
#endif

#ifdef USE_FOPEN

static FILE *s_FileHandle;

#else

static IFileSystem *s_pFileSystem;
static FileHandle_t s_FileHandle;

#endif



class CStubD3DTexture : public IDirect3DTexture8
{
private:
	IDirect3DTexture8 *m_pTexture;
	IDirect3DDevice8 *m_pDevice;

public:
	CStubD3DTexture( IDirect3DTexture8 *pTexture, IDirect3DDevice8 *pDevice )
	{
		m_pTexture = pTexture;
		m_pDevice = pDevice;
	}

    /*** IUnknown methods ***/
    HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObj)
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::QueryInterface\n" );
		return m_pTexture->QueryInterface( riid, ppvObj );
	}

    ULONG __stdcall AddRef()
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::AddRef\n" );
		return m_pTexture->AddRef();
	}

    ULONG __stdcall Release()
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::Release\n" );
		return m_pTexture->Release();
	}

    /*** IDirect3DBaseTexture8 methods ***/
    HRESULT __stdcall GetDevice( IDirect3DDevice8** ppDevice )
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::GetDevice\n" );
#if 0
		*ppDevice = m_pDevice;
		return D3D_OK;		
#else
		return m_pTexture->GetDevice( ppDevice );
#endif
	}

    HRESULT __stdcall SetPrivateData( REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags)
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::SetPrivateData\n" );
		return m_pTexture->SetPrivateData( refguid, pData, SizeOfData, Flags );
	}

    HRESULT __stdcall GetPrivateData( REFGUID refguid,void* pData,DWORD* pSizeOfData )
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::GetPrivateData\n" );
		return m_pTexture->GetPrivateData( refguid, pData, pSizeOfData );
	}

    HRESULT __stdcall FreePrivateData( REFGUID refguid )
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::GetPrivateData\n" );
		return m_pTexture->FreePrivateData( refguid );
	}
	
    DWORD __stdcall SetPriority( DWORD PriorityNew )
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::SetPriority\n" );
		return m_pTexture->SetPriority( PriorityNew );
	}
	
    DWORD __stdcall GetPriority()
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::GetPriority\n" );
		return m_pTexture->GetPriority();
	}
	
    void __stdcall PreLoad()
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::PreLoad\n" );
		m_pTexture->PreLoad();
	}
	
    D3DRESOURCETYPE __stdcall GetType()
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::GetType\n" );
		return m_pTexture->GetType();
	}
	
    DWORD __stdcall SetLOD( DWORD LODNew )
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::SetLOD\n" );
		return m_pTexture->SetLOD( LODNew );
	}
	
    DWORD __stdcall GetLOD()
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::GetLOD\n" );
		return m_pTexture->GetLOD();
	}
	
    DWORD __stdcall GetLevelCount()
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::GetLevelCount\n" );
		return m_pTexture->GetLevelCount();
	}
	
    HRESULT __stdcall GetLevelDesc(UINT Level,D3DSURFACE_DESC *pDesc)
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::GetLevelCount\n" );
		return m_pTexture->GetLevelDesc( Level, pDesc );
	}
	
    HRESULT __stdcall GetSurfaceLevel(UINT Level,IDirect3DSurface8** ppSurfaceLevel)
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::GetSurfaceLevel\n" );
		return m_pTexture->GetSurfaceLevel( Level, ppSurfaceLevel );
	}
	
    HRESULT __stdcall LockRect(UINT Level,D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags)
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::LockRect\n" );
		return m_pTexture->LockRect( Level, pLockedRect, pRect, Flags );
	}
	
    HRESULT __stdcall UnlockRect(UINT Level)
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::UnlockRect\n" );
		return m_pTexture->UnlockRect( Level );
	}
	
    HRESULT __stdcall AddDirtyRect( CONST RECT* pDirtyRect )
	{
		FPRINTF( s_FileHandle, "IDirect3DTexture8::AddDirtyRect\n" );
		return m_pTexture->AddDirtyRect( pDirtyRect );
	}
};

class CStubD3DDevice : public IDirect3DDevice8
{
public:
	CStubD3DDevice( IDirect3DDevice8 *pD3DDevice, IFileSystem *pFileSystem )
	{
		Assert( pD3DDevice );
		m_pD3DDevice = pD3DDevice;
#ifdef USE_FOPEN
		s_FileHandle = fopen( "stubd3d.txt", "w" );
#else
		Assert( pFileSystem );
		s_pFileSystem = pFileSystem;
		s_FileHandle = pFileSystem->Open( "stubd3d.txt", "w" );
#endif
	}

	~CStubD3DDevice()
	{
#ifdef USE_FOPEN
		fclose( s_FileHandle );
#else
		s_pFileSystem->Close( s_FileHandle );
#endif
	}

private:
	IDirect3DDevice8 *m_pD3DDevice;

public:
    /*** IUnknown methods ***/
    HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObj)
	{
		FPRINTF( s_FileHandle, "QueryInterface\n" );
		return m_pD3DDevice->QueryInterface( riid, ppvObj );
	}

    ULONG __stdcall AddRef()
	{
		FPRINTF( s_FileHandle, "AddRef\n" );
		return m_pD3DDevice->AddRef();
	}

    ULONG __stdcall Release()
	{
		FPRINTF( s_FileHandle, "Release\n" );
		return m_pD3DDevice->Release();
		delete this;
	}

    /*** IDirect3DDevice8 methods ***/
    HRESULT __stdcall TestCooperativeLevel()
	{
		FPRINTF( s_FileHandle, "TestCooperativeLevel\n" );
		return m_pD3DDevice->TestCooperativeLevel();
	}

    UINT __stdcall GetAvailableTextureMem()
	{
		FPRINTF( s_FileHandle, "GetAvailableTextureMem\n" );
		return m_pD3DDevice->GetAvailableTextureMem();
	}

    HRESULT __stdcall ResourceManagerDiscardBytes(DWORD Bytes)
	{
		FPRINTF( s_FileHandle, "ResourceManagerDiscardBytes\n" );
		return m_pD3DDevice->ResourceManagerDiscardBytes( Bytes );
	}

    HRESULT __stdcall GetDirect3D(IDirect3D8** ppD3D8)
	{
		FPRINTF( s_FileHandle, "GetDirect3D\n" );
		return m_pD3DDevice->GetDirect3D( ppD3D8 );
	}

    HRESULT __stdcall GetDeviceCaps(D3DCAPS8* pCaps)
	{
		FPRINTF( s_FileHandle, "GetDeviceCaps\n" );
		return m_pD3DDevice->GetDeviceCaps( pCaps );
	}

    HRESULT __stdcall GetDisplayMode(D3DDISPLAYMODE* pMode)
	{
		FPRINTF( s_FileHandle, "GetDisplayMode\n" );
		return m_pD3DDevice->GetDisplayMode( pMode );
	}

    HRESULT __stdcall GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters)
	{
		FPRINTF( s_FileHandle, "GetCreationParameters\n" );
		return m_pD3DDevice->GetCreationParameters( pParameters );
	}

    HRESULT __stdcall SetCursorProperties(UINT XHotSpot,UINT YHotSpot,IDirect3DSurface8* pCursorBitmap)
	{
		FPRINTF( s_FileHandle, "SetCursorProperties\n" );
		return m_pD3DDevice->SetCursorProperties( XHotSpot, YHotSpot, pCursorBitmap );
	}

    void __stdcall SetCursorPosition(UINT XScreenSpace,UINT YScreenSpace,DWORD Flags)
	{
		FPRINTF( s_FileHandle, "SetCursorPosition\n" );
		m_pD3DDevice->SetCursorPosition( XScreenSpace, YScreenSpace, Flags );
	}

    BOOL __stdcall ShowCursor(BOOL bShow)
	{
		FPRINTF( s_FileHandle, "ShowCursor\n" );
		return m_pD3DDevice->ShowCursor( bShow );
	}

    HRESULT __stdcall CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DSwapChain8** pSwapChain)
	{
		FPRINTF( s_FileHandle, "CreateAdditionalSwapChain\n" );
		return m_pD3DDevice->CreateAdditionalSwapChain( pPresentationParameters, pSwapChain );
	}

    HRESULT __stdcall Reset(D3DPRESENT_PARAMETERS* pPresentationParameters)
	{
		FPRINTF( s_FileHandle, "Reset\n" );
		return m_pD3DDevice->Reset( pPresentationParameters );
	}

    HRESULT __stdcall Present(CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion)
	{
		FPRINTF( s_FileHandle, "Present\n" );
		return m_pD3DDevice->Present( pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion );
	}

    HRESULT __stdcall GetBackBuffer(UINT BackBuffer,D3DBACKBUFFER_TYPE Type,IDirect3DSurface8** ppBackBuffer)
	{
		FPRINTF( s_FileHandle, "GetBackBuffer\n" );
		return m_pD3DDevice->GetBackBuffer( BackBuffer, Type, ppBackBuffer );
	}

    HRESULT __stdcall GetRasterStatus(D3DRASTER_STATUS* pRasterStatus)
	{
		FPRINTF( s_FileHandle, "GetRasterStatus\n" );
		return m_pD3DDevice->GetRasterStatus( pRasterStatus );
	}

    void __stdcall SetGammaRamp(DWORD Flags,CONST D3DGAMMARAMP* pRamp)
	{
		FPRINTF( s_FileHandle, "SetGammaRamp\n" );
		m_pD3DDevice->SetGammaRamp( Flags, pRamp );
	}

    void __stdcall GetGammaRamp(D3DGAMMARAMP* pRamp)
	{
		FPRINTF( s_FileHandle, "GetGammaRamp\n" );
		m_pD3DDevice->GetGammaRamp( pRamp );
	}

    HRESULT __stdcall CreateTexture(UINT Width,UINT Height,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DTexture8** ppTexture)
	{
		FPRINTF( s_FileHandle, "CreateTexture\n" );
#if 0
		HRESULT ret = m_pD3DDevice->CreateTexture( Width, Height, Levels, Usage, Format, Pool, ppTexture );
		if( ret == D3D_OK )
		{
			*ppTexture = new CStubD3DTexture( *ppTexture, this );
			return ret;
		}
		else
		{
			return ret;
		}
#else
		return m_pD3DDevice->CreateTexture( Width, Height, Levels, Usage, Format, Pool, ppTexture );
#endif
	}

    HRESULT __stdcall CreateVolumeTexture(UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DVolumeTexture8** ppVolumeTexture)
	{
		FPRINTF( s_FileHandle, "CreateVolumeTexture\n" );
		return m_pD3DDevice->CreateVolumeTexture( Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture );
	}

    HRESULT __stdcall CreateCubeTexture(UINT EdgeLength,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DCubeTexture8** ppCubeTexture)
	{
		FPRINTF( s_FileHandle, "CreateCubeTexture\n" );
		return m_pD3DDevice->CreateCubeTexture( EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture );
	}

    HRESULT __stdcall CreateVertexBuffer(UINT Length,DWORD Usage,DWORD FVF,D3DPOOL Pool,IDirect3DVertexBuffer8** ppVertexBuffer)
	{
		FPRINTF( s_FileHandle, "CreateVertexBuffer\n" );
		return m_pD3DDevice->CreateVertexBuffer( Length, Usage, FVF, Pool, ppVertexBuffer );
	}

    HRESULT __stdcall CreateIndexBuffer(UINT Length,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DIndexBuffer8** ppIndexBuffer)
	{
		FPRINTF( s_FileHandle, "CreateIndexBuffer\n" );
		return m_pD3DDevice->CreateIndexBuffer( Length, Usage, Format, Pool, ppIndexBuffer );
	}

    HRESULT __stdcall CreateRenderTarget(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,BOOL Lockable,IDirect3DSurface8** ppSurface)
	{
		FPRINTF( s_FileHandle, "CreateRenderTarget\n" );
		return m_pD3DDevice->CreateRenderTarget( Width, Height, Format, MultiSample, Lockable, ppSurface );
	}

    HRESULT __stdcall CreateDepthStencilSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,IDirect3DSurface8** ppSurface)
	{
		FPRINTF( s_FileHandle, "CreateDepthStencilSurface\n" );
		return m_pD3DDevice->CreateDepthStencilSurface( Width, Height, Format, MultiSample, ppSurface );
	}

    HRESULT __stdcall CreateImageSurface(UINT Width,UINT Height,D3DFORMAT Format,IDirect3DSurface8** ppSurface)
	{
		FPRINTF( s_FileHandle, "CreateImageSurface\n" );
		return m_pD3DDevice->CreateImageSurface( Width, Height, Format, ppSurface );
	}

    HRESULT __stdcall CopyRects(IDirect3DSurface8* pSourceSurface,CONST RECT* pSourceRectsArray,UINT cRects,IDirect3DSurface8* pDestinationSurface,CONST POINT* pDestPointsArray)
	{
		FPRINTF( s_FileHandle, "CopyRects\n" );
		return m_pD3DDevice->CopyRects( pSourceSurface, pSourceRectsArray, cRects, pDestinationSurface, pDestPointsArray );
	}

    HRESULT __stdcall UpdateTexture(IDirect3DBaseTexture8* pSourceTexture,IDirect3DBaseTexture8* pDestinationTexture)
	{
		FPRINTF( s_FileHandle, "UpdateTexture\n" );
		return m_pD3DDevice->UpdateTexture( pSourceTexture, pDestinationTexture );
	}

    HRESULT __stdcall GetFrontBuffer(IDirect3DSurface8* pDestSurface)
	{
		FPRINTF( s_FileHandle, "GetFrontBuffer\n" );
		return m_pD3DDevice->GetFrontBuffer( pDestSurface );
	}

    HRESULT __stdcall SetRenderTarget(IDirect3DSurface8* pRenderTarget,IDirect3DSurface8* pNewZStencil)
	{
		FPRINTF( s_FileHandle, "SetRenderTarget\n" );
		return m_pD3DDevice->SetRenderTarget( pRenderTarget, pNewZStencil );
	}

    HRESULT __stdcall GetRenderTarget(IDirect3DSurface8** ppRenderTarget)
	{
		FPRINTF( s_FileHandle, "GetRenderTarget\n" );
		return m_pD3DDevice->GetRenderTarget( ppRenderTarget );
	}

    HRESULT __stdcall GetDepthStencilSurface(IDirect3DSurface8** ppZStencilSurface)
	{
		FPRINTF( s_FileHandle, "GetDepthStencilSurface\n" );
		return m_pD3DDevice->GetDepthStencilSurface( ppZStencilSurface );
	}

    HRESULT __stdcall BeginScene( void )
	{
		FPRINTF( s_FileHandle, "BeginScene\n" );
		return m_pD3DDevice->BeginScene();
	}

    HRESULT __stdcall EndScene()
	{
		FPRINTF( s_FileHandle, "EndScene\n" );
		return m_pD3DDevice->EndScene();
	}

    HRESULT __stdcall Clear(DWORD Count,CONST D3DRECT* pRects,DWORD Flags,D3DCOLOR Color,float Z,DWORD Stencil)
	{
		FPRINTF( s_FileHandle, "Clear\n" );
		return m_pD3DDevice->Clear( Count, pRects, Flags, Color, Z, Stencil );
	}

    HRESULT __stdcall SetTransform(D3DTRANSFORMSTATETYPE State,CONST D3DMATRIX* pMatrix)
	{
		FPRINTF( s_FileHandle, "SetTransform\n" );
		return m_pD3DDevice->SetTransform( State, pMatrix );
	}

    HRESULT __stdcall GetTransform(D3DTRANSFORMSTATETYPE State,D3DMATRIX* pMatrix)
	{
		FPRINTF( s_FileHandle, "GetTransform\n" );
		return m_pD3DDevice->GetTransform( State, pMatrix );
	}

    HRESULT __stdcall MultiplyTransform(D3DTRANSFORMSTATETYPE transformState,CONST D3DMATRIX* pMatrix)
	{
		FPRINTF( s_FileHandle, "MultiplyTransform\n" );
		return m_pD3DDevice->MultiplyTransform( transformState, pMatrix );
	}

    HRESULT __stdcall SetViewport(CONST D3DVIEWPORT8* pViewport)
	{
		FPRINTF( s_FileHandle, "SetViewport\n" );
		return m_pD3DDevice->SetViewport( pViewport );
	}

    HRESULT __stdcall GetViewport(D3DVIEWPORT8* pViewport)
	{
		FPRINTF( s_FileHandle, "GetViewport\n" );
		return m_pD3DDevice->GetViewport( pViewport );
	}

    HRESULT __stdcall SetMaterial(CONST D3DMATERIAL8* pMaterial)
	{
		FPRINTF( s_FileHandle, "SetMaterial\n" );
		return m_pD3DDevice->SetMaterial( pMaterial );
	}

    HRESULT __stdcall GetMaterial(D3DMATERIAL8* pMaterial)
	{
		FPRINTF( s_FileHandle, "GetMaterial\n" );
		return m_pD3DDevice->GetMaterial( pMaterial );
	}

    HRESULT __stdcall SetLight(DWORD Index,CONST D3DLIGHT8* pLight)
	{
		FPRINTF( s_FileHandle, "SetLight\n" );
		return m_pD3DDevice->SetLight( Index, pLight );
	}

    HRESULT __stdcall GetLight(DWORD Index,D3DLIGHT8* pLight)
	{
		FPRINTF( s_FileHandle, "GetLight\n" );
		return m_pD3DDevice->GetLight( Index, pLight );
	}

    HRESULT __stdcall LightEnable(DWORD Index,BOOL Enable)
	{
		FPRINTF( s_FileHandle, "LightEnable\n" );
		return m_pD3DDevice->LightEnable( Index, Enable );
	}

    HRESULT __stdcall GetLightEnable(DWORD Index,BOOL* pEnable)
	{
		FPRINTF( s_FileHandle, "GetLightEnable\n" );
		return m_pD3DDevice->GetLightEnable( Index, pEnable );
	}

    HRESULT __stdcall SetClipPlane(DWORD Index,CONST float* pPlane)
	{
		FPRINTF( s_FileHandle, "SetClipPlane\n" );
		return m_pD3DDevice->SetClipPlane( Index, pPlane );
	}

    HRESULT __stdcall GetClipPlane(DWORD Index,float* pPlane)
	{
		FPRINTF( s_FileHandle, "GetClipPlane\n" );
		return m_pD3DDevice->GetClipPlane( Index, pPlane );
	}

    HRESULT __stdcall SetRenderState(D3DRENDERSTATETYPE State,DWORD Value)
	{
		FPRINTF( s_FileHandle, "SetRenderState\n" );
		return m_pD3DDevice->SetRenderState( State, Value );
	}

    HRESULT __stdcall GetRenderState(D3DRENDERSTATETYPE State,DWORD* pValue)
	{
		FPRINTF( s_FileHandle, "GetRenderState\n" );
		return m_pD3DDevice->GetRenderState( State, pValue );
	}

    HRESULT __stdcall BeginStateBlock(void)
	{
		FPRINTF( s_FileHandle, "BeginStateBlock\n" );
		return m_pD3DDevice->BeginStateBlock();
	}

    HRESULT __stdcall EndStateBlock(DWORD* pToken)
	{
		FPRINTF( s_FileHandle, "EndStateBlock\n" );
		return m_pD3DDevice->EndStateBlock( pToken );
	}

    HRESULT __stdcall ApplyStateBlock(DWORD Token)
	{
		FPRINTF( s_FileHandle, "ApplyStateBlock\n" );
		return m_pD3DDevice->ApplyStateBlock( Token );
	}

    HRESULT __stdcall CaptureStateBlock(DWORD Token)
	{
		FPRINTF( s_FileHandle, "CaptureStateBlock\n" );
		return m_pD3DDevice->CaptureStateBlock( Token );
	}

    HRESULT __stdcall DeleteStateBlock(DWORD Token)
	{
		FPRINTF( s_FileHandle, "DeleteStateBlock\n" );
		return m_pD3DDevice->DeleteStateBlock( Token );
	}

    HRESULT __stdcall CreateStateBlock(D3DSTATEBLOCKTYPE Type,DWORD* pToken)
	{
		FPRINTF( s_FileHandle, "CreateStateBlock\n" );
		return m_pD3DDevice->CreateStateBlock( Type, pToken );
	}

    HRESULT __stdcall SetClipStatus(CONST D3DCLIPSTATUS8* pClipStatus)
	{
		FPRINTF( s_FileHandle, "SetClipStatus\n" );
		return m_pD3DDevice->SetClipStatus( pClipStatus );
	}

    HRESULT __stdcall GetClipStatus(D3DCLIPSTATUS8* pClipStatus)
	{
		FPRINTF( s_FileHandle, "GetClipStatus\n" );
		return m_pD3DDevice->GetClipStatus( pClipStatus );
	}

    HRESULT __stdcall GetTexture(DWORD Stage,IDirect3DBaseTexture8** ppTexture)
	{
		FPRINTF( s_FileHandle, "GetTexture\n" );
		return m_pD3DDevice->GetTexture( Stage, ppTexture );
	}

    HRESULT __stdcall SetTexture(DWORD Stage,IDirect3DBaseTexture8* pTexture)
	{
		FPRINTF( s_FileHandle, "SetTexture\n" );
		return m_pD3DDevice->SetTexture( Stage, pTexture );
	}

    HRESULT __stdcall GetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD* pValue)
	{
		FPRINTF( s_FileHandle, "GetTextureStageState\n" );
		return m_pD3DDevice->GetTextureStageState( Stage, Type, pValue );
	}

    HRESULT __stdcall SetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD Value)
	{
		FPRINTF( s_FileHandle, "SetTextureStageState\n" );
		return m_pD3DDevice->SetTextureStageState( Stage, Type, Value );
	}

    HRESULT __stdcall ValidateDevice(DWORD* pNumPasses)
	{
		FPRINTF( s_FileHandle, "ValidateDevice\n" );
#if 0
		return m_pD3DDevice->ValidateDevice( pNumPasses );
#else
		return D3D_OK;
#endif
	}

    HRESULT __stdcall GetInfo(DWORD DevInfoID,void* pDevInfoStruct,DWORD DevInfoStructSize)
	{
		FPRINTF( s_FileHandle, "GetInfo\n" );
		return m_pD3DDevice->GetInfo( DevInfoID, pDevInfoStruct, DevInfoStructSize );
	}

    HRESULT __stdcall SetPaletteEntries(UINT PaletteNumber,CONST PALETTEENTRY* pEntries)
	{
		FPRINTF( s_FileHandle, "SetPaletteEntries\n" );
		return m_pD3DDevice->SetPaletteEntries( PaletteNumber, pEntries );
	}

    HRESULT __stdcall GetPaletteEntries(UINT PaletteNumber,PALETTEENTRY* pEntries)
	{
		FPRINTF( s_FileHandle, "GetPaletteEntries\n" );
		return m_pD3DDevice->GetPaletteEntries( PaletteNumber, pEntries );
	}

    HRESULT __stdcall SetCurrentTexturePalette(UINT PaletteNumber)
	{
		FPRINTF( s_FileHandle, "SetCurrentTexturePalette\n" );
		return m_pD3DDevice->SetCurrentTexturePalette( PaletteNumber );
	}

    HRESULT __stdcall GetCurrentTexturePalette(UINT *PaletteNumber)
	{
		FPRINTF( s_FileHandle, "GetCurrentTexturePalette\n" );
		return m_pD3DDevice->GetCurrentTexturePalette( PaletteNumber );
	}

    HRESULT __stdcall DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount)
	{
		FPRINTF( s_FileHandle, "DrawPrimitive\n" );
		return m_pD3DDevice->DrawPrimitive( PrimitiveType, StartVertex, PrimitiveCount );
	}

    HRESULT __stdcall DrawIndexedPrimitive(D3DPRIMITIVETYPE primitiveType,UINT minIndex,UINT NumVertices,UINT startIndex,UINT primCount)
	{
		FPRINTF( s_FileHandle, "DrawIndexedPrimitive\n" );
		return m_pD3DDevice->DrawIndexedPrimitive( primitiveType,minIndex,NumVertices,startIndex,primCount );
	}

    HRESULT __stdcall DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCount,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
	{
		FPRINTF( s_FileHandle, "DrawPrimitiveUP\n" );
		return m_pD3DDevice->DrawPrimitiveUP( PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride );
	}

    HRESULT __stdcall DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertexIndices,UINT PrimitiveCount,CONST void* pIndexData,D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
	{
		FPRINTF( s_FileHandle, "DrawIndexedPrimitiveUP\n" );
		return m_pD3DDevice->DrawIndexedPrimitiveUP( PrimitiveType, MinVertexIndex, NumVertexIndices, PrimitiveCount, pIndexData, IndexDataFormat,pVertexStreamZeroData, VertexStreamZeroStride );
	}

    HRESULT __stdcall ProcessVertices(UINT SrcStartIndex,UINT DestIndex,UINT VertexCount,IDirect3DVertexBuffer8* pDestBuffer,DWORD Flags)
	{
		FPRINTF( s_FileHandle, "ProcessVertices\n" );
		return m_pD3DDevice->ProcessVertices( SrcStartIndex, DestIndex, VertexCount, pDestBuffer, Flags );
	}

    HRESULT __stdcall CreateVertexShader(CONST DWORD* pDeclaration,CONST DWORD* pFunction,DWORD* pHandle,DWORD Usage)
	{
		FPRINTF( s_FileHandle, "CreateVertexShader\n" );
		return m_pD3DDevice->CreateVertexShader( pDeclaration, pFunction, pHandle, Usage );
	}

    HRESULT __stdcall SetVertexShader(DWORD Handle)
	{
		FPRINTF( s_FileHandle, "SetVertexShader\n" );
		return m_pD3DDevice->SetVertexShader( Handle );
	}

    HRESULT __stdcall GetVertexShader(DWORD* pHandle)
	{
		FPRINTF( s_FileHandle, "GetVertexShader\n" );
		return m_pD3DDevice->GetVertexShader( pHandle );
	}

    HRESULT __stdcall DeleteVertexShader(DWORD Handle)
	{
		FPRINTF( s_FileHandle, "DeleteVertexShader\n" );
		return m_pD3DDevice->DeleteVertexShader( Handle );
	}

    HRESULT __stdcall SetVertexShaderConstant(DWORD Register,CONST void* pConstantData,DWORD ConstantCount)
	{
		FPRINTF( s_FileHandle, "SetVertexShaderConstant\n" );
		return m_pD3DDevice->SetVertexShaderConstant( Register, pConstantData, ConstantCount );
	}

    HRESULT __stdcall GetVertexShaderConstant(DWORD Register,void* pConstantData,DWORD ConstantCount)
	{
		FPRINTF( s_FileHandle, "GetVertexShaderConstant\n" );
		return m_pD3DDevice->GetVertexShaderConstant( Register, pConstantData, ConstantCount );
	}

    HRESULT __stdcall GetVertexShaderDeclaration(DWORD Handle,void* pData,DWORD* pSizeOfData)
	{
		FPRINTF( s_FileHandle, "GetVertexShaderDeclaration\n" );
		return m_pD3DDevice->GetVertexShaderDeclaration( Handle, pData, pSizeOfData );
	}

    HRESULT __stdcall GetVertexShaderFunction(DWORD Handle,void* pData,DWORD* pSizeOfData)
	{
		FPRINTF( s_FileHandle, "GetVertexShaderFunction\n" );
		return m_pD3DDevice->GetVertexShaderFunction( Handle, pData, pSizeOfData );
	}

    HRESULT __stdcall SetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer8* pStreamData,UINT Stride)
	{
		FPRINTF( s_FileHandle, "SetStreamSource\n" );
		return m_pD3DDevice->SetStreamSource( StreamNumber, pStreamData, Stride );
	}

    HRESULT __stdcall GetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer8** ppStreamData,UINT* pStride)
	{
		FPRINTF( s_FileHandle, "GetStreamSource\n" );
		return m_pD3DDevice->GetStreamSource( StreamNumber, ppStreamData, pStride );
	}

    HRESULT __stdcall SetIndices(IDirect3DIndexBuffer8* pIndexData,UINT BaseVertexIndex)
	{
		FPRINTF( s_FileHandle, "SetIndices\n" );
		return m_pD3DDevice->SetIndices( pIndexData, BaseVertexIndex );
	}

    HRESULT __stdcall GetIndices(IDirect3DIndexBuffer8** ppIndexData,UINT* pBaseVertexIndex)
	{
		FPRINTF( s_FileHandle, "GetIndices\n" );
		return m_pD3DDevice->GetIndices( ppIndexData, pBaseVertexIndex );
	}

    HRESULT __stdcall CreatePixelShader(CONST DWORD* pFunction,DWORD* pHandle)
	{
		FPRINTF( s_FileHandle, "CreatePixelShader\n" );
		return m_pD3DDevice->CreatePixelShader( pFunction, pHandle );
	}

    HRESULT __stdcall SetPixelShader(DWORD Handle)
	{
		FPRINTF( s_FileHandle, "SetPixelShader\n" );
		return m_pD3DDevice->SetPixelShader( Handle );
	}

    HRESULT __stdcall GetPixelShader(DWORD* pHandle)
	{
		FPRINTF( s_FileHandle, "GetPixelShader\n" );
		return m_pD3DDevice->GetPixelShader( pHandle );
	}

    HRESULT __stdcall DeletePixelShader(DWORD Handle)
	{
		FPRINTF( s_FileHandle, "DeletePixelShader\n" );
		return m_pD3DDevice->DeletePixelShader( Handle );
	}

    HRESULT __stdcall SetPixelShaderConstant(DWORD Register,CONST void* pConstantData,DWORD ConstantCount)
	{
		FPRINTF( s_FileHandle, "SetPixelShaderConstant\n" );
		return m_pD3DDevice->SetPixelShaderConstant( Register, pConstantData, ConstantCount );
	}

    HRESULT __stdcall GetPixelShaderConstant(DWORD Register,void* pConstantData,DWORD ConstantCount)
	{
		FPRINTF( s_FileHandle, "GetPixelShaderConstant\n" );
		return m_pD3DDevice->GetPixelShaderConstant( Register, pConstantData, ConstantCount );
	}

    HRESULT __stdcall GetPixelShaderFunction(DWORD Handle,void* pData,DWORD* pSizeOfData)
	{
		FPRINTF( s_FileHandle, "GetPixelShaderFunction\n" );
		return m_pD3DDevice->GetPixelShaderFunction( Handle, pData, pSizeOfData );
	}

    HRESULT __stdcall DrawRectPatch(UINT Handle,CONST float* pNumSegs,CONST D3DRECTPATCH_INFO* pRectPatchInfo)
	{
		FPRINTF( s_FileHandle, "DrawRectPatch\n" );
		return m_pD3DDevice->DrawRectPatch( Handle, pNumSegs, pRectPatchInfo );
	}

    HRESULT __stdcall DrawTriPatch(UINT Handle,CONST float* pNumSegs,CONST D3DTRIPATCH_INFO* pTriPatchInfo)
	{
		FPRINTF( s_FileHandle, "DrawTriPatch\n" );
		return m_pD3DDevice->DrawTriPatch( Handle, pNumSegs, pTriPatchInfo );
	}

    HRESULT __stdcall DeletePatch(UINT Handle)
	{
		FPRINTF( s_FileHandle, "DeletePatch\n" );
		return m_pD3DDevice->DeletePatch( Handle );
	}
};

#endif // STUBD3D

#endif // STUBD3DDEVICE_H

